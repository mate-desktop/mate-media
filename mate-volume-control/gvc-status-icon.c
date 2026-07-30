/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
 * Copyright (C) 2014-2021 MATE Developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>

#include <libmatemixer/matemixer.h>

#include "gvc-status-icon.h"
#include "gvc-stream-status-icon.h"
#include "gvc-mpris-manager.h"
#include "gvc-mpris-player.h"
#include "gvc-player-widget.h"

static const gchar *icon_names_output[] = {
        "audio-volume-muted",
        "audio-volume-low",
        "audio-volume-medium",
        "audio-volume-high",
        NULL
};

static const gchar *icon_names_input[] = {
        "audio-input-microphone-muted",
        "audio-input-microphone-low",
        "audio-input-microphone-medium",
        "audio-input-microphone-high",
        NULL
};

struct _GvcStatusIconPrivate
{
        GvcStreamStatusIcon *icon_input;
        GvcStreamStatusIcon *icon_output;
        gboolean             running;
        MateMixerContext    *context;
        MateMixerStream     *output;
        MateMixerStream     *input;
        GvcMprisManager     *mpris_manager;
        GvcMprisPlayer      *active_player;
        GvcMprisPlayer      *selected_player;
        GtkWidget           *player_widget;
        GSettings           *applet_settings;
};

G_DEFINE_TYPE_WITH_PRIVATE (GvcStatusIcon, gvc_status_icon, G_TYPE_OBJECT)

static void update_active_player (GvcStatusIcon *status_icon);

static GSettings *
new_applet_settings (void)
{
        GSettingsSchemaSource *source;
        GSettingsSchema       *schema;

        source = g_settings_schema_source_get_default ();
        if (source == NULL)
                return NULL;

        schema = g_settings_schema_source_lookup (source,
                                                  "org.mate.volume-control",
                                                  TRUE);
        if (schema == NULL)
                return NULL;

        g_settings_schema_unref (schema);

        return g_settings_new ("org.mate.volume-control");
}

static gboolean
settings_get_boolean (GvcStatusIcon *status_icon,
                      const gchar   *key,
                      gboolean       fallback)
{
        if (status_icon->priv->applet_settings == NULL)
                return fallback;

        return g_settings_get_boolean (status_icon->priv->applet_settings, key);
}

static gboolean
is_recording_application_control (MateMixerStreamControl *control)
{
        MateMixerAppInfo *app_info;
        const gchar      *app_id;

        if (mate_mixer_stream_control_get_role (control) != MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION)
                return FALSE;

        app_info = mate_mixer_stream_control_get_app_info (control);
        if (app_info == NULL)
                return TRUE;

        app_id = mate_mixer_app_info_get_id (app_info);
        if (app_id == NULL)
                return TRUE;

        return strcmp (app_id, "org.mate.VolumeControl") != 0 &&
               strcmp (app_id, "org.gnome.VolumeControl") != 0 &&
               strcmp (app_id, "org.PulseAudio.pavucontrol") != 0;
}

static void
update_icon_input (GvcStatusIcon *status_icon)
{
        MateMixerStreamControl *control = NULL;
        gboolean                show = FALSE;

        /* Enable the input icon when any application is recording, regardless of
         * which input stream it uses */
        if (status_icon->priv->input != NULL) {
                const gchar *stream_name;
                const GList *inputs = mate_mixer_stream_list_controls (status_icon->priv->input);

                control = mate_mixer_stream_get_default_control (status_icon->priv->input);

                stream_name = mate_mixer_stream_get_name (status_icon->priv->input);
                if (stream_name != NULL && g_str_has_suffix (stream_name, ".monitor"))
                        inputs = NULL;

                while (inputs != NULL) {
                        MateMixerStreamControl *input = MATE_MIXER_STREAM_CONTROL (inputs->data);

                        if (is_recording_application_control (input)) {
                                if (G_UNLIKELY (control == NULL))
                                        control = input;
                                show = TRUE;
                                break;
                        }
                        inputs = inputs->next;
                }
        }

        if (!show && status_icon->priv->context != NULL) {
                const GList *streams;

                streams = mate_mixer_context_list_streams (status_icon->priv->context);
                while (streams != NULL && !show) {
                        MateMixerStream *stream = MATE_MIXER_STREAM (streams->data);
                        const gchar     *name;
                        const GList     *inputs;

                        if (stream == status_icon->priv->input ||
                            mate_mixer_stream_get_direction (stream) != MATE_MIXER_DIRECTION_INPUT) {
                                streams = streams->next;
                                continue;
                        }

                        name = mate_mixer_stream_get_name (stream);
                        if (name != NULL && g_str_has_suffix (name, ".monitor")) {
                                streams = streams->next;
                                continue;
                        }

                        inputs = mate_mixer_stream_list_controls (stream);
                        while (inputs != NULL) {
                                MateMixerStreamControl *input = MATE_MIXER_STREAM_CONTROL (inputs->data);

                                if (is_recording_application_control (input)) {
                                        control = mate_mixer_stream_get_default_control (stream);
                                        if (control == NULL)
                                                control = input;
                                        show = TRUE;
                                        break;
                                }

                                inputs = inputs->next;
                        }

                        streams = streams->next;
                }
        }

        gvc_stream_status_icon_set_control (status_icon->priv->icon_input, control);
        gtk_status_icon_set_visible (GTK_STATUS_ICON (status_icon->priv->icon_input), show);
}

static void
update_icon_output (GvcStatusIcon *status_icon)
{
        MateMixerStream        *stream;
        MateMixerStreamControl *control = NULL;

        stream = mate_mixer_context_get_default_output_stream (status_icon->priv->context);
        if (stream != NULL)
                control = mate_mixer_stream_get_default_control (stream);

        gvc_stream_status_icon_set_control (status_icon->priv->icon_output, control);

        gtk_status_icon_set_visible (GTK_STATUS_ICON (status_icon->priv->icon_output),
                                     control != NULL);
}

static void
on_output_stream_control_added (MateMixerStream *stream,
                                const gchar     *name,
                                GvcStatusIcon   *status_icon)
{
        MateMixerStreamControl *control;

        control = mate_mixer_stream_get_control (stream, name);
        if (G_LIKELY (control != NULL)) {
                MateMixerStreamControlRole role = mate_mixer_stream_control_get_role (control);

                if (role != MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION)
                        return;
        }

        update_icon_output (status_icon);
}

static void
on_output_stream_control_removed (MateMixerStream *stream,
                                  const gchar     *name,
                                  GvcStatusIcon   *status_icon)
{
        update_icon_output (status_icon);
}

static void
update_player_selector (GvcStatusIcon *status_icon)
{
        GList *players;

        players = gvc_mpris_manager_get_players (status_icon->priv->mpris_manager);
        gvc_player_widget_set_players (GVC_PLAYER_WIDGET (status_icon->priv->player_widget),
                                       players,
                                       status_icon->priv->active_player);
        g_list_free (players);
}

static void
on_player_widget_player_selected (GvcPlayerWidget *widget,
                                  GvcMprisPlayer  *player,
                                  GvcStatusIcon   *status_icon)
{
        g_clear_object (&status_icon->priv->selected_player);
        if (player != NULL)
                status_icon->priv->selected_player = g_object_ref (player);

        update_active_player (status_icon);
}

static void
on_status_icon_player_selected (GvcStreamStatusIcon *icon,
                                GvcMprisPlayer      *player,
                                GvcStatusIcon       *status_icon)
{
        g_clear_object (&status_icon->priv->selected_player);
        if (player != NULL)
                status_icon->priv->selected_player = g_object_ref (player);

        update_active_player (status_icon);
}

static void
on_mpris_player_changed (GvcMprisPlayer *player,
                         GvcStatusIcon  *status_icon)
{
        update_active_player (status_icon);
}

static void
update_active_player (GvcStatusIcon *status_icon)
{
        GvcMprisPlayer *player;
        gboolean        show_player;

        show_player = settings_get_boolean (status_icon, "show-player-controls", TRUE);

        player = NULL;
        if (status_icon->priv->selected_player != NULL &&
            gvc_mpris_player_is_ready (status_icon->priv->selected_player))
                player = status_icon->priv->selected_player;
        if (player == NULL)
                player = gvc_mpris_manager_get_best_player (status_icon->priv->mpris_manager);

        if (player == status_icon->priv->active_player) {
                if (player != NULL) {
                        gvc_player_widget_set_player (GVC_PLAYER_WIDGET (status_icon->priv->player_widget),
                                                      show_player ? player : NULL);
                        gvc_stream_status_icon_set_player_widget_visible (status_icon->priv->icon_output,
                                                                          show_player);
                }
                update_player_selector (status_icon);
                return;
        }

        g_clear_object (&status_icon->priv->active_player);
        if (player != NULL)
                status_icon->priv->active_player = g_object_ref (player);

        gvc_player_widget_set_player (GVC_PLAYER_WIDGET (status_icon->priv->player_widget),
                                      show_player ? status_icon->priv->active_player : NULL);
        gvc_stream_status_icon_set_mpris_player (status_icon->priv->icon_output,
                                                 status_icon->priv->active_player);
        gvc_stream_status_icon_set_player_widget_visible (status_icon->priv->icon_output,
                                                          status_icon->priv->active_player != NULL &&
                                                          show_player);
        update_player_selector (status_icon);
}

static void
remember_player (GvcStatusIcon  *status_icon,
                 GvcMprisPlayer *player)
{
        const gchar *desktop_entry;
        gchar      **known;
        gboolean     found = FALSE;
        gint         i;

        if (status_icon->priv->applet_settings == NULL)
                return;

        desktop_entry = gvc_mpris_player_get_desktop_entry (player);
        if (desktop_entry == NULL || desktop_entry[0] == '\0')
                return;

        known = g_settings_get_strv (status_icon->priv->applet_settings, "known-players");
        for (i = 0; known != NULL && known[i] != NULL; i++) {
                if (g_strcmp0 (known[i], desktop_entry) == 0) {
                        found = TRUE;
                        break;
                }
        }

        if (!found) {
                GPtrArray *array;

                array = g_ptr_array_new_with_free_func (g_free);
                for (i = 0; known != NULL && known[i] != NULL; i++)
                        g_ptr_array_add (array, g_strdup (known[i]));
                g_ptr_array_add (array, g_strdup (desktop_entry));
                g_ptr_array_add (array, NULL);
                g_settings_set_strv (status_icon->priv->applet_settings,
                                     "known-players",
                                     (const gchar * const *) array->pdata);
                g_ptr_array_free (array, TRUE);
        }

        g_strfreev (known);
}

static void
on_mpris_player_added (GvcMprisManager *manager,
                       GvcMprisPlayer  *player,
                       GvcStatusIcon   *status_icon)
{
        remember_player (status_icon, player);

        g_signal_connect (player, "status-changed", G_CALLBACK (on_mpris_player_changed), status_icon);
        g_signal_connect (player, "metadata-changed", G_CALLBACK (on_mpris_player_changed), status_icon);
        g_signal_connect (player, "capabilities-changed", G_CALLBACK (on_mpris_player_changed), status_icon);

        update_active_player (status_icon);
}

static void
on_mpris_player_removed (GvcMprisManager *manager,
                         const gchar     *bus_name,
                         GvcStatusIcon   *status_icon)
{
        if (status_icon->priv->selected_player != NULL &&
            g_strcmp0 (gvc_mpris_player_get_bus_name (status_icon->priv->selected_player), bus_name) == 0)
                g_clear_object (&status_icon->priv->selected_player);

        update_active_player (status_icon);
}

static void
on_applet_settings_changed (GSettings     *settings,
                            gchar         *key,
                            GvcStatusIcon *status_icon)
{
        update_active_player (status_icon);
}

static void
on_input_stream_control_added (MateMixerStream *stream,
                               const gchar     *name,
                               GvcStatusIcon   *status_icon)
{
        MateMixerStreamControl *control;

        control = mate_mixer_stream_get_control (stream, name);
        if (G_LIKELY (control != NULL)) {
                MateMixerStreamControlRole role = mate_mixer_stream_control_get_role (control);
                if (role != MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION)
                        return;
        }

        update_icon_input (status_icon);
}

static void
on_input_stream_control_removed (MateMixerStream *stream,
                                 const gchar     *name,
                                 GvcStatusIcon   *status_icon)
{
        update_icon_input (status_icon);
}

static void
disconnect_input_stream_control_signals (GvcStatusIcon *status_icon)
{
        const GList *streams;

        streams = mate_mixer_context_list_streams (status_icon->priv->context);
        while (streams != NULL) {
                MateMixerStream *stream = MATE_MIXER_STREAM (streams->data);

                if (mate_mixer_stream_get_direction (stream) == MATE_MIXER_DIRECTION_INPUT) {
                        g_signal_handlers_disconnect_by_func (G_OBJECT (stream),
                                                              G_CALLBACK (on_input_stream_control_added),
                                                              status_icon);
                        g_signal_handlers_disconnect_by_func (G_OBJECT (stream),
                                                              G_CALLBACK (on_input_stream_control_removed),
                                                              status_icon);
                }

                streams = streams->next;
        }
}

static void
connect_input_stream_control_signals (GvcStatusIcon *status_icon)
{
        const GList *streams;

        disconnect_input_stream_control_signals (status_icon);

        streams = mate_mixer_context_list_streams (status_icon->priv->context);
        while (streams != NULL) {
                MateMixerStream *stream = MATE_MIXER_STREAM (streams->data);

                if (mate_mixer_stream_get_direction (stream) == MATE_MIXER_DIRECTION_INPUT) {
                        g_signal_connect (G_OBJECT (stream),
                                          "control-added",
                                          G_CALLBACK (on_input_stream_control_added),
                                          status_icon);
                        g_signal_connect (G_OBJECT (stream),
                                          "control-removed",
                                          G_CALLBACK (on_input_stream_control_removed),
                                          status_icon);
                }

                streams = streams->next;
        }
}

static gboolean
update_default_output_stream (GvcStatusIcon *status_icon)
{
        MateMixerStream *stream;

        stream = mate_mixer_context_get_default_output_stream (status_icon->priv->context);
        if (stream == status_icon->priv->output)
                return FALSE;

        if (status_icon->priv->output != NULL) {
                g_signal_handlers_disconnect_by_data (G_OBJECT (status_icon->priv->output), status_icon);
                g_object_unref (status_icon->priv->output);
        }

        status_icon->priv->output = (stream == NULL) ? NULL : g_object_ref (stream);
        if (status_icon->priv->output != NULL) {
                g_signal_connect (G_OBJECT (status_icon->priv->output),
                                  "control-added",
                                  G_CALLBACK (on_output_stream_control_added),
                                  status_icon);
                g_signal_connect (G_OBJECT (status_icon->priv->output),
                                  "control-removed",
                                  G_CALLBACK (on_output_stream_control_removed),
                                  status_icon);
        }

        return TRUE;
}

static gboolean
update_default_input_stream (GvcStatusIcon *status_icon)
{
        MateMixerStream *stream;

        stream = mate_mixer_context_get_default_input_stream (status_icon->priv->context);
        if (stream == status_icon->priv->input)
                return FALSE;

        if (status_icon->priv->input != NULL) {
                g_signal_handlers_disconnect_by_data (G_OBJECT (status_icon->priv->input), status_icon);
                g_object_unref (status_icon->priv->input);
        }

        status_icon->priv->input = (stream == NULL) ? NULL : g_object_ref (stream);
        return TRUE;
}

static void
on_context_state_notify (MateMixerContext *context,
                         GParamSpec       *pspec,
                         GvcStatusIcon    *status_icon)
{
        MateMixerState state = mate_mixer_context_get_state (context);

        switch (state) {
        case MATE_MIXER_STATE_FAILED:
                g_warning ("Failed to connect to a sound system");
                break;

        case MATE_MIXER_STATE_READY:
                update_default_output_stream (status_icon);
                update_default_input_stream (status_icon);
                connect_input_stream_control_signals (status_icon);

                update_icon_output (status_icon);
                update_icon_input (status_icon);
                break;
        default:
                break;
        }
}

static void
on_context_default_input_stream_notify (MateMixerContext *context,
                                        GParamSpec       *pspec,
                                        GvcStatusIcon    *status_icon)
{
        if (update_default_input_stream (status_icon) == FALSE)
                return;

        connect_input_stream_control_signals (status_icon);
        update_icon_input (status_icon);
}

static void
on_context_default_output_stream_notify (MateMixerContext *control,
                                         GParamSpec       *pspec,
                                         GvcStatusIcon    *status_icon)
{
        update_default_output_stream (status_icon);
        update_icon_output (status_icon);
}

static void
on_context_stream_added (MateMixerContext *context,
                         const gchar      *name,
                         GvcStatusIcon    *status_icon)
{
        update_default_output_stream (status_icon);
        update_default_input_stream (status_icon);
        connect_input_stream_control_signals (status_icon);
        update_icon_output (status_icon);
        update_icon_input (status_icon);
}

static void
on_context_stream_removed (MateMixerContext *context,
                           const gchar      *name,
                           GvcStatusIcon    *status_icon)
{
        update_default_output_stream (status_icon);
        update_default_input_stream (status_icon);
        connect_input_stream_control_signals (status_icon);
        update_icon_output (status_icon);
        update_icon_input (status_icon);
}

void
gvc_status_icon_start (GvcStatusIcon *status_icon)
{
        g_return_if_fail (GVC_IS_STATUS_ICON (status_icon));

        if (G_UNLIKELY (status_icon->priv->running == TRUE))
                return;

        if (G_UNLIKELY (mate_mixer_context_open (status_icon->priv->context) == FALSE))
                g_warning ("Failed to connect to a sound system");

        g_debug ("StatusIcon has been started");

        status_icon->priv->running = TRUE;
}

static void
gvc_status_icon_dispose (GObject *object)
{
        GvcStatusIcon *status_icon = GVC_STATUS_ICON (object);

        if (status_icon->priv->input != NULL) {
                g_signal_handlers_disconnect_by_data (G_OBJECT (status_icon->priv->input),
                                                      status_icon);
                g_clear_object (&status_icon->priv->input);
        }
        if (status_icon->priv->output != NULL) {
                g_signal_handlers_disconnect_by_data (G_OBJECT (status_icon->priv->output),
                                                      status_icon);
                g_clear_object (&status_icon->priv->output);
        }
        if (status_icon->priv->context != NULL)
                disconnect_input_stream_control_signals (status_icon);

        g_clear_object (&status_icon->priv->context);
        g_clear_object (&status_icon->priv->active_player);
        g_clear_object (&status_icon->priv->selected_player);
        g_clear_object (&status_icon->priv->mpris_manager);
        g_clear_object (&status_icon->priv->applet_settings);
        g_clear_object (&status_icon->priv->icon_input);
        g_clear_object (&status_icon->priv->icon_output);

        G_OBJECT_CLASS (gvc_status_icon_parent_class)->dispose (object);
}

static void
gvc_status_icon_class_init (GvcStatusIconClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = gvc_status_icon_dispose;
}

static void
gvc_status_icon_init (GvcStatusIcon *status_icon)
{
        status_icon->priv = gvc_status_icon_get_instance_private (status_icon);

        status_icon->priv->icon_input  = gvc_stream_status_icon_new (NULL, icon_names_input);
        status_icon->priv->icon_output = gvc_stream_status_icon_new (NULL, icon_names_output);

        status_icon->priv->applet_settings = new_applet_settings ();
        gvc_stream_status_icon_set_applet_settings (status_icon->priv->icon_input,
                                                    status_icon->priv->applet_settings);
        gvc_stream_status_icon_set_applet_settings (status_icon->priv->icon_output,
                                                    status_icon->priv->applet_settings);

        status_icon->priv->player_widget = gvc_player_widget_new ();
        gvc_stream_status_icon_set_player_widget (status_icon->priv->icon_output,
                                                  status_icon->priv->player_widget);
        g_signal_connect (status_icon->priv->player_widget,
                          "player-selected",
                          G_CALLBACK (on_player_widget_player_selected),
                          status_icon);
        g_signal_connect (status_icon->priv->icon_output,
                          "player-selected",
                          G_CALLBACK (on_status_icon_player_selected),
                          status_icon);
        g_signal_connect (status_icon->priv->icon_input,
                          "player-selected",
                          G_CALLBACK (on_status_icon_player_selected),
                          status_icon);

        gvc_stream_status_icon_set_display_name (status_icon->priv->icon_input,  _("Input"));
        gvc_stream_status_icon_set_display_name (status_icon->priv->icon_output, _("Output"));

        gtk_status_icon_set_title (GTK_STATUS_ICON (status_icon->priv->icon_input),
                                   _("Microphone Volume"));
        gtk_status_icon_set_title (GTK_STATUS_ICON (status_icon->priv->icon_output),
                                   _("Sound Output Volume"));

        status_icon->priv->context = mate_mixer_context_new ();
        status_icon->priv->mpris_manager = gvc_mpris_manager_new ();

        gvc_stream_status_icon_set_mpris_manager (status_icon->priv->icon_output,
                                                  status_icon->priv->mpris_manager);
        gvc_stream_status_icon_set_mpris_manager (status_icon->priv->icon_input,
                                                  status_icon->priv->mpris_manager);
        gvc_stream_status_icon_set_mate_mixer_context (status_icon->priv->icon_output,
                                                       status_icon->priv->context);
        gvc_stream_status_icon_set_mate_mixer_context (status_icon->priv->icon_input,
                                                       status_icon->priv->context);

        mate_mixer_context_set_app_name (status_icon->priv->context,
                                         _("MATE Volume Control StatusIcon"));

        mate_mixer_context_set_app_id (status_icon->priv->context, GVC_STATUS_ICON_DBUS_NAME);
        mate_mixer_context_set_app_version (status_icon->priv->context, VERSION);
        mate_mixer_context_set_app_icon (status_icon->priv->context, "multimedia-volume-control");

        g_signal_connect (G_OBJECT (status_icon->priv->context),
                          "notify::state",
                          G_CALLBACK (on_context_state_notify),
                          status_icon);
        g_signal_connect (G_OBJECT (status_icon->priv->context),
                          "notify::default-input-stream",
                          G_CALLBACK (on_context_default_input_stream_notify),
                          status_icon);
        g_signal_connect (G_OBJECT (status_icon->priv->context),
                          "notify::default-output-stream",
                          G_CALLBACK (on_context_default_output_stream_notify),
                          status_icon);
        g_signal_connect (G_OBJECT (status_icon->priv->context),
                          "stream-added",
                          G_CALLBACK (on_context_stream_added),
                          status_icon);
        g_signal_connect (G_OBJECT (status_icon->priv->context),
                          "stream-removed",
                          G_CALLBACK (on_context_stream_removed),
                          status_icon);
        g_signal_connect (G_OBJECT (status_icon->priv->mpris_manager),
                          "player-added",
                          G_CALLBACK (on_mpris_player_added),
                          status_icon);
        g_signal_connect (G_OBJECT (status_icon->priv->mpris_manager),
                          "player-removed",
                          G_CALLBACK (on_mpris_player_removed),
                          status_icon);
        if (status_icon->priv->applet_settings != NULL) {
                g_signal_connect (G_OBJECT (status_icon->priv->applet_settings),
                                  "changed",
                                  G_CALLBACK (on_applet_settings_changed),
                                  status_icon);
        }
}

GvcStatusIcon *
gvc_status_icon_new (void)
{
        return g_object_new (GVC_TYPE_STATUS_ICON, NULL);
}
