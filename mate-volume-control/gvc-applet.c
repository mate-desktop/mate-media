/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
 * Copyright (C) 2019 Victor Kareh <vkareh@vkareh.net>
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
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>

#include <libmatemixer/matemixer.h>
#include <mate-panel-applet.h>

#include "gvc-applet.h"
#include "gvc-mpris-manager.h"
#include "gvc-player-widget.h"
#include "gvc-stream-applet-icon.h"

const gchar *mate_panel_applet_get_object_path (MatePanelApplet *applet);

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

static void menu_output_mute (GtkAction *action, GvcApplet *applet);
static void menu_activate_open_volume_control (GtkAction *action, GvcApplet *applet);

static const GtkActionEntry applet_menu_actions [] = {
        { "MuteOutput", "audio-volume-muted", N_("Mute Output"), NULL, NULL, G_CALLBACK (menu_output_mute) },
        { "Preferences", APPLET_ICON, N_("_Sound Preferences"), NULL, NULL, G_CALLBACK (menu_activate_open_volume_control) }
};

static char *ui = "<menuitem name='MuteOutput' action='MuteOutput' />"
                  "<menuitem name='Preferences' action='Preferences' />";

struct _GvcAppletPrivate
{
        GvcStreamAppletIcon *icon_input;
        GvcStreamAppletIcon *icon_output;
        gboolean             running;
        MateMixerContext    *context;
        MateMixerStream     *output;
        MateMixerStream     *input;
        GvcMprisManager     *mpris_manager;
        GvcMprisPlayer      *active_player;
        GvcMprisPlayer      *selected_player;
        GtkWidget           *player_widget;
        GtkLabel            *track_label;
        GSettings           *applet_settings;

        MatePanelApplet     *applet;
        GtkBox              *box;
        GtkActionGroup      *action_group;
};

G_DEFINE_TYPE_WITH_PRIVATE (GvcApplet, gvc_applet, G_TYPE_OBJECT)

static void update_active_player (GvcApplet *applet);

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
settings_get_boolean (GvcApplet   *applet,
                      const gchar *key,
                      gboolean     fallback)
{
        if (applet->priv->applet_settings == NULL)
                return fallback;

        return g_settings_get_boolean (applet->priv->applet_settings, key);
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
update_icon_input (GvcApplet *applet)
{
        MateMixerStreamControl *control = NULL;
        gboolean                show = FALSE;

        /* Enable the input icon in case there is an input stream present and there
         * is a non-mixer application using the input */
        if (applet->priv->input != NULL) {
                const gchar *app_id = NULL;
                const gchar *stream_name;
                const GList *inputs = mate_mixer_stream_list_controls (applet->priv->input);

                control = mate_mixer_stream_get_default_control (applet->priv->input);

                stream_name = mate_mixer_stream_get_name (applet->priv->input);
                g_debug ("Got stream name %s", stream_name);
                if (g_str_has_suffix (stream_name, ".monitor")) {
                        inputs = NULL;
                        g_debug ("Stream is a monitor, ignoring");
                }

                while (inputs != NULL) {
                        MateMixerStreamControl *input = MATE_MIXER_STREAM_CONTROL (inputs->data);

                        if (is_recording_application_control (input)) {
                                MateMixerAppInfo *app_info;

                                app_info = mate_mixer_stream_control_get_app_info (input);
                                app_id = app_info != NULL ? mate_mixer_app_info_get_id (app_info) : NULL;
                                if (app_id != NULL)
                                        g_debug ("Found a recording application %s", app_id);
                                else
                                        g_debug ("Found a recording application control %s",
                                                 mate_mixer_stream_control_get_label (input));

                                if (G_UNLIKELY (control == NULL))
                                        control = input;

                                show = TRUE;
                                break;
                        }
                        inputs = inputs->next;
                }
        }

        if (!show && applet->priv->context != NULL) {
                const GList *streams;

                streams = mate_mixer_context_list_streams (applet->priv->context);
                while (streams != NULL && !show) {
                        MateMixerStream *stream = MATE_MIXER_STREAM (streams->data);
                        const gchar     *name;
                        const GList     *inputs;

                        if (stream == applet->priv->input ||
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

        if (show == TRUE)
                g_debug ("Input icon enabled");
        else
                g_debug ("There is no recording application, input icon disabled");

        gvc_stream_applet_icon_set_control (applet->priv->icon_input, control);

        gtk_widget_set_visible (GTK_WIDGET (applet->priv->icon_input), show);
}

static void
update_icon_output (GvcApplet *applet)
{
        MateMixerStream        *stream;
        MateMixerStreamControl *control = NULL;

        stream = mate_mixer_context_get_default_output_stream (applet->priv->context);
        if (stream != NULL)
                control = mate_mixer_stream_get_default_control (stream);

        gvc_stream_applet_icon_set_control (applet->priv->icon_output, control);

        if (control != NULL) {
                g_debug ("Output icon enabled");
                gtk_widget_set_visible (GTK_WIDGET (applet->priv->icon_output), TRUE);
        }
        else {
                g_debug ("There is no output stream/control, output icon disabled");
                gtk_widget_set_visible (GTK_WIDGET (applet->priv->icon_output), FALSE);
        }
}

static void
update_track_label (GvcApplet *applet)
{
        MatePanelAppletOrient orient;
        const gchar          *title;
        const gchar          *artist;
        gchar                *text = NULL;

        if (applet->priv->track_label == NULL)
                return;

        if (!settings_get_boolean (applet, "show-track-in-panel", FALSE) ||
            applet->priv->active_player == NULL ||
            applet->priv->applet == NULL) {
                gtk_widget_hide (GTK_WIDGET (applet->priv->track_label));
                return;
        }

        orient = mate_panel_applet_get_orient (applet->priv->applet);
        if (orient == MATE_PANEL_APPLET_ORIENT_LEFT ||
            orient == MATE_PANEL_APPLET_ORIENT_RIGHT) {
                gtk_widget_hide (GTK_WIDGET (applet->priv->track_label));
                return;
        }

        title = gvc_mpris_player_get_title (applet->priv->active_player);
        artist = gvc_mpris_player_get_artist (applet->priv->active_player);

        if (title != NULL && title[0] != '\0' &&
            artist != NULL && artist[0] != '\0')
                text = g_strdup_printf ("%s - %s", title, artist);
        else if (title != NULL && title[0] != '\0')
                text = g_strdup (title);
        else if (artist != NULL && artist[0] != '\0')
                text = g_strdup (artist);

        if (text == NULL) {
                gtk_widget_hide (GTK_WIDGET (applet->priv->track_label));
                return;
        }

        gtk_label_set_text (applet->priv->track_label, text);
        gtk_widget_set_tooltip_text (GTK_WIDGET (applet->priv->track_label), text);
        gtk_widget_show (GTK_WIDGET (applet->priv->track_label));

        g_free (text);
}

static void
update_player_selector (GvcApplet *applet)
{
        GList *players;

        players = gvc_mpris_manager_get_players (applet->priv->mpris_manager);
        gvc_player_widget_set_players (GVC_PLAYER_WIDGET (applet->priv->player_widget),
                                       players,
                                       applet->priv->active_player);
        g_list_free (players);
}

static void
on_player_widget_player_selected (GvcPlayerWidget *widget,
                                  GvcMprisPlayer  *player,
                                  GvcApplet       *applet)
{
        g_clear_object (&applet->priv->selected_player);
        if (player != NULL)
                applet->priv->selected_player = g_object_ref (player);

        update_active_player (applet);
}

static void
on_mpris_player_changed (GvcMprisPlayer *player,
                         GvcApplet      *applet)
{
        update_active_player (applet);
}

static void
update_active_player (GvcApplet *applet)
{
        GvcMprisPlayer *player;

        player = NULL;
        if (applet->priv->selected_player != NULL &&
            gvc_mpris_player_is_ready (applet->priv->selected_player))
                player = applet->priv->selected_player;
        if (player == NULL)
                player = gvc_mpris_manager_get_best_player (applet->priv->mpris_manager);

        if (player == applet->priv->active_player) {
                if (player != NULL) {
                        if (settings_get_boolean (applet, "show-player-controls", TRUE))
                                gvc_player_widget_set_player (GVC_PLAYER_WIDGET (applet->priv->player_widget), player);
                        else
                                gvc_player_widget_set_player (GVC_PLAYER_WIDGET (applet->priv->player_widget), NULL);
                        gvc_stream_applet_icon_set_player_widget_visible (applet->priv->icon_output,
                                                                          settings_get_boolean (applet, "show-player-controls", TRUE));
                }
                update_player_selector (applet);
                update_track_label (applet);
                return;
        }

        g_clear_object (&applet->priv->active_player);
        if (player != NULL)
                applet->priv->active_player = g_object_ref (player);

        if (settings_get_boolean (applet, "show-player-controls", TRUE))
                gvc_player_widget_set_player (GVC_PLAYER_WIDGET (applet->priv->player_widget),
                                              applet->priv->active_player);
        else
                gvc_player_widget_set_player (GVC_PLAYER_WIDGET (applet->priv->player_widget), NULL);
        gvc_stream_applet_icon_set_mpris_player (applet->priv->icon_output,
                                                 applet->priv->active_player);
        gvc_stream_applet_icon_set_player_widget_visible (applet->priv->icon_output,
                                                          applet->priv->active_player != NULL &&
                                                          settings_get_boolean (applet, "show-player-controls", TRUE));
        update_player_selector (applet);
        update_track_label (applet);
}

static void
on_player_menu_item_toggled (GtkCheckMenuItem *item,
                             GvcApplet        *applet)
{
        GvcMprisPlayer *player;

        if (!gtk_check_menu_item_get_active (item))
                return;

        player = g_object_get_data (G_OBJECT (item), "gvc-player");
        g_clear_object (&applet->priv->selected_player);
        if (player != NULL)
                applet->priv->selected_player = g_object_ref (player);

        update_active_player (applet);
}

static void
on_stream_menu_item_toggled (GtkCheckMenuItem *item,
                             GvcApplet        *applet)
{
        MateMixerStream *stream;
        MateMixerDirection direction;

        if (!gtk_check_menu_item_get_active (item))
                return;

        stream = g_object_get_data (G_OBJECT (item), "gvc-stream");
        if (stream == NULL)
                return;

        direction = mate_mixer_stream_get_direction (stream);
        if (direction == MATE_MIXER_DIRECTION_OUTPUT)
                mate_mixer_context_set_default_output_stream (applet->priv->context, stream);
        else if (direction == MATE_MIXER_DIRECTION_INPUT)
                mate_mixer_context_set_default_input_stream (applet->priv->context, stream);
}

static void
on_launch_player_menu_item_activate (GtkMenuItem *item,
                                     GvcApplet   *applet)
{
        const gchar     *desktop_entry;
        GDesktopAppInfo *app_info;
        GError          *error = NULL;

        desktop_entry = g_object_get_data (G_OBJECT (item), "desktop-entry");
        if (desktop_entry == NULL)
                return;

        app_info = g_desktop_app_info_new (desktop_entry);
        if (app_info == NULL && !g_str_has_suffix (desktop_entry, ".desktop")) {
                gchar *desktop_id;

                desktop_id = g_strconcat (desktop_entry, ".desktop", NULL);
                app_info = g_desktop_app_info_new (desktop_id);
                g_free (desktop_id);
        }

        if (app_info == NULL)
                return;

        g_app_info_launch (G_APP_INFO (app_info), NULL, NULL, &error);
        if (error != NULL)
                g_error_free (error);

        g_object_unref (app_info);
}

static void
on_custom_menu_mute_activate (GtkMenuItem *item,
                              GvcApplet   *applet)
{
        gboolean is_muted;

        is_muted = gvc_stream_applet_icon_get_mute (applet->priv->icon_output);
        gvc_stream_applet_icon_set_mute (applet->priv->icon_output, !is_muted);
}

static void
on_custom_menu_preferences_activate (GtkMenuItem *item,
                                     GvcApplet   *applet)
{
        menu_activate_open_volume_control (NULL, applet);
}

static GtkWidget *
new_image_menu_item (const gchar *label,
                     const gchar *icon_name)
{
        GtkWidget *item;
        GtkWidget *image;

        item = gtk_image_menu_item_new_with_label (label);
        image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
        gtk_image_menu_item_set_always_show_image (GTK_IMAGE_MENU_ITEM (item), TRUE);

        return item;
}

static void
emit_panel_applet_signal (GvcApplet   *applet,
                          const gchar *signal_name)
{
        GDBusConnection *connection = NULL;
        const gchar     *object_path;
        GError          *error = NULL;

        if (applet->priv->applet == NULL)
                return;

        object_path = mate_panel_applet_get_object_path (applet->priv->applet);
        if (object_path == NULL)
                return;

        if (g_object_class_find_property (G_OBJECT_GET_CLASS (applet->priv->applet), "connection") == NULL)
                return;

        g_object_get (applet->priv->applet, "connection", &connection, NULL);
        if (connection == NULL)
                return;

        g_dbus_connection_emit_signal (connection,
                                       NULL,
                                       object_path,
                                       "org.mate.panel.applet.Applet",
                                       signal_name,
                                       NULL,
                                       &error);
        if (error != NULL)
                g_error_free (error);

        g_object_unref (connection);
}

static void
on_panel_menu_remove_activate (GtkMenuItem *item,
                               GvcApplet   *applet)
{
        emit_panel_applet_signal (applet, "RemoveFromPanel");
}

static void
on_panel_menu_move_activate (GtkMenuItem *item,
                             GvcApplet   *applet)
{
        emit_panel_applet_signal (applet, "Move");
}

static void
on_panel_menu_lock_activate (GtkMenuItem *item,
                             GvcApplet   *applet)
{
        if (applet->priv->applet != NULL &&
            g_object_class_find_property (G_OBJECT_GET_CLASS (applet->priv->applet), "locked") != NULL)
                g_object_set (applet->priv->applet,
                              "locked",
                              gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)),
                              NULL);
}

static void
append_stream_menu_items (GvcApplet          *applet,
                          GtkWidget          *submenu,
                          MateMixerDirection  direction)
{
        const GList *streams;
        GSList      *group = NULL;

        streams = mate_mixer_context_list_streams (applet->priv->context);
        while (streams != NULL) {
                MateMixerStream *stream = MATE_MIXER_STREAM (streams->data);

                if (mate_mixer_stream_get_direction (stream) == direction) {
                        GtkWidget       *item;
                        MateMixerStream *active;
                        const gchar     *label;

                        active = direction == MATE_MIXER_DIRECTION_OUTPUT ?
                                mate_mixer_context_get_default_output_stream (applet->priv->context) :
                                mate_mixer_context_get_default_input_stream (applet->priv->context);
                        label = mate_mixer_stream_get_label (stream);
                        if (label == NULL || label[0] == '\0')
                                label = mate_mixer_stream_get_name (stream);

                        item = gtk_radio_menu_item_new_with_label (group, label);
                        group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
                        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), stream == active);
                        g_object_set_data_full (G_OBJECT (item), "gvc-stream", g_object_ref (stream), g_object_unref);
                        g_signal_connect (item, "toggled", G_CALLBACK (on_stream_menu_item_toggled), applet);
                        gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
                }

                streams = streams->next;
        }
}

static gboolean
on_applet_icon_context_menu (GtkWidget      *widget,
                             GdkEventButton *event,
                             GvcApplet      *applet)
{
        GtkWidget *menu;
        GtkWidget *item;
        GtkWidget *submenu;
        GList     *players;
        GList     *l;
        GSList    *player_group = NULL;
        gboolean   locked = FALSE;

        if (event->type != GDK_BUTTON_PRESS || event->button != 3)
                return FALSE;

        menu = gtk_menu_new ();

        item = new_image_menu_item (_("Media Players"), "applications-multimedia");
        submenu = gtk_menu_new ();
        players = gvc_mpris_manager_get_players (applet->priv->mpris_manager);
        for (l = players; l != NULL; l = l->next) {
                GvcMprisPlayer *player = GVC_MPRIS_PLAYER (l->data);
                GtkWidget      *player_item;
                const gchar    *label;

                if (!gvc_mpris_player_is_ready (player))
                        continue;

                label = gvc_mpris_player_get_identity (player);
                player_item = gtk_radio_menu_item_new_with_label (player_group,
                                                                  label != NULL && label[0] != '\0' ?
                                                                  label :
                                                                  gvc_mpris_player_get_bus_name (player));
                player_group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (player_item));
                gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (player_item), player == applet->priv->active_player);
                g_object_set_data_full (G_OBJECT (player_item), "gvc-player", g_object_ref (player), g_object_unref);
                g_signal_connect (player_item, "toggled", G_CALLBACK (on_player_menu_item_toggled), applet);
                gtk_menu_shell_append (GTK_MENU_SHELL (submenu), player_item);
        }
        g_list_free (players);
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        if (applet->priv->applet_settings != NULL) {
                gchar **known;
                gint    i;

                item = new_image_menu_item (_("Launch Player"), "applications-multimedia");
                submenu = gtk_menu_new ();
                known = g_settings_get_strv (applet->priv->applet_settings, "known-players");
                for (i = 0; known != NULL && known[i] != NULL; i++) {
                        GtkWidget *launcher_item;

                        launcher_item = gtk_menu_item_new_with_label (known[i]);
                        g_object_set_data_full (G_OBJECT (launcher_item), "desktop-entry", g_strdup (known[i]), g_free);
                        g_signal_connect (launcher_item, "activate", G_CALLBACK (on_launch_player_menu_item_activate), applet);
                        gtk_menu_shell_append (GTK_MENU_SHELL (submenu), launcher_item);
                }
                g_strfreev (known);
                gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        }

        item = new_image_menu_item (_("Output Device"), "audio-speakers");
        submenu = gtk_menu_new ();
        append_stream_menu_items (applet, submenu, MATE_MIXER_DIRECTION_OUTPUT);
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        item = new_image_menu_item (_("Input Device"), "audio-input-microphone");
        submenu = gtk_menu_new ();
        append_stream_menu_items (applet, submenu, MATE_MIXER_DIRECTION_INPUT);
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

        item = new_image_menu_item (gvc_stream_applet_icon_get_mute (applet->priv->icon_output) ?
                                    _("Unmute Output") :
                                    _("Mute Output"),
                                    gvc_stream_applet_icon_get_mute (applet->priv->icon_output) ?
                                    "audio-volume-medium" :
                                    "audio-volume-muted");
        g_signal_connect (item, "activate", G_CALLBACK (on_custom_menu_mute_activate), applet);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        item = new_image_menu_item (_("Sound Preferences"), APPLET_ICON);
        g_signal_connect (item, "activate", G_CALLBACK (on_custom_menu_preferences_activate), applet);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

        item = new_image_menu_item (_("_Remove From Panel"), "list-remove");
        gtk_menu_item_set_use_underline (GTK_MENU_ITEM (item), TRUE);
        g_signal_connect (item, "activate", G_CALLBACK (on_panel_menu_remove_activate), applet);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        item = gtk_menu_item_new_with_mnemonic (_("_Move"));
        gtk_menu_item_set_use_underline (GTK_MENU_ITEM (item), TRUE);
        g_signal_connect (item, "activate", G_CALLBACK (on_panel_menu_move_activate), applet);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

        if (applet->priv->applet != NULL &&
            g_object_class_find_property (G_OBJECT_GET_CLASS (applet->priv->applet), "locked") != NULL)
                g_object_get (applet->priv->applet, "locked", &locked, NULL);

        item = gtk_check_menu_item_new_with_mnemonic (_("Loc_k To Panel"));
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), locked);
        g_signal_connect (item, "activate", G_CALLBACK (on_panel_menu_lock_activate), applet);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        g_signal_connect (menu, "selection-done", G_CALLBACK (gtk_widget_destroy), NULL);

        gtk_widget_show_all (menu);
        gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *) event);

        return TRUE;
}

static void
remember_player (GvcApplet      *applet,
                 GvcMprisPlayer *player)
{
        const gchar *desktop_entry;
        gchar      **known;
        gboolean     found = FALSE;
        gint         i;

        if (applet->priv->applet_settings == NULL)
                return;

        desktop_entry = gvc_mpris_player_get_desktop_entry (player);
        if (desktop_entry == NULL || desktop_entry[0] == '\0')
                return;

        known = g_settings_get_strv (applet->priv->applet_settings, "known-players");
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
                g_settings_set_strv (applet->priv->applet_settings,
                                     "known-players",
                                     (const gchar * const *) array->pdata);
                g_ptr_array_free (array, TRUE);
        }

        g_strfreev (known);
}

static void
on_mpris_player_added (GvcMprisManager *manager,
                       GvcMprisPlayer  *player,
                       GvcApplet       *applet)
{
        remember_player (applet, player);

        g_signal_connect (player, "status-changed", G_CALLBACK (on_mpris_player_changed), applet);
        g_signal_connect (player, "metadata-changed", G_CALLBACK (on_mpris_player_changed), applet);
        g_signal_connect (player, "capabilities-changed", G_CALLBACK (on_mpris_player_changed), applet);

        update_active_player (applet);
}

static void
on_applet_settings_changed (GSettings *settings,
                            gchar     *key,
                            GvcApplet *applet)
{
        update_active_player (applet);
}

static void
on_mpris_player_removed (GvcMprisManager *manager,
                         const gchar     *bus_name,
                         GvcApplet       *applet)
{
        if (applet->priv->selected_player != NULL &&
            g_strcmp0 (gvc_mpris_player_get_bus_name (applet->priv->selected_player), bus_name) == 0)
                g_clear_object (&applet->priv->selected_player);

        update_active_player (applet);
}

static void
on_output_stream_control_added (MateMixerStream *stream,
                                const gchar     *name,
                                GvcApplet       *applet)
{
        MateMixerStreamControl *control;

        control = mate_mixer_stream_get_control (stream, name);
        if (G_LIKELY (control != NULL)) {
                MateMixerStreamControlRole role = mate_mixer_stream_control_get_role (control);

                /* Non-application output control doesn't affect the icon */
                if (role != MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION)
                        return;
        }

        /* Either an application control has been added or we couldn't
         * read the control, this shouldn't happen but let's revalidate the
         * icon to be sure if it does */
        update_icon_output (applet);
}

static void
on_input_stream_control_added (MateMixerStream *stream,
                               const gchar     *name,
                               GvcApplet       *applet)
{
        MateMixerStreamControl *control;

        control = mate_mixer_stream_get_control (stream, name);
        if (G_LIKELY (control != NULL)) {
                MateMixerStreamControlRole role = mate_mixer_stream_control_get_role (control);

                /* Non-application input control doesn't affect the icon */
                if (role != MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION)
                        return;
        }

        /* Either an application control has been added or we couldn't
         * read the control, this shouldn't happen but let's revalidate the
         * icon to be sure if it does */
        update_icon_input (applet);
}

static void
on_output_stream_control_removed (MateMixerStream *stream,
                                  const gchar     *name,
                                  GvcApplet       *applet)
{
        /* The removed stream could be an application output, which may cause
         * the output applet icon to disappear */
        update_icon_output (applet);
}

static void
on_input_stream_control_removed (MateMixerStream *stream,
                                 const gchar     *name,
                                 GvcApplet       *applet)
{
        /* The removed stream could be an application input, which may cause
         * the input applet icon to disappear */
        update_icon_input (applet);
}

static void
disconnect_input_stream_control_signals (GvcApplet *applet)
{
        const GList *streams;

        streams = mate_mixer_context_list_streams (applet->priv->context);
        while (streams != NULL) {
                MateMixerStream *stream = MATE_MIXER_STREAM (streams->data);

                if (mate_mixer_stream_get_direction (stream) == MATE_MIXER_DIRECTION_INPUT) {
                        g_signal_handlers_disconnect_by_func (G_OBJECT (stream),
                                                              G_CALLBACK (on_input_stream_control_added),
                                                              applet);
                        g_signal_handlers_disconnect_by_func (G_OBJECT (stream),
                                                              G_CALLBACK (on_input_stream_control_removed),
                                                              applet);
                }

                streams = streams->next;
        }
}

static void
connect_input_stream_control_signals (GvcApplet *applet)
{
        const GList *streams;

        disconnect_input_stream_control_signals (applet);

        streams = mate_mixer_context_list_streams (applet->priv->context);
        while (streams != NULL) {
                MateMixerStream *stream = MATE_MIXER_STREAM (streams->data);

                if (mate_mixer_stream_get_direction (stream) == MATE_MIXER_DIRECTION_INPUT) {
                        g_signal_connect (G_OBJECT (stream),
                                          "control-added",
                                          G_CALLBACK (on_input_stream_control_added),
                                          applet);
                        g_signal_connect (G_OBJECT (stream),
                                          "control-removed",
                                          G_CALLBACK (on_input_stream_control_removed),
                                          applet);
                }

                streams = streams->next;
        }
}

static gboolean
update_default_output_stream (GvcApplet *applet)
{
        MateMixerStream *stream;

        stream = mate_mixer_context_get_default_output_stream (applet->priv->context);
        if (stream == applet->priv->output)
                return FALSE;

        /* The output stream has changed */
        if (applet->priv->output != NULL) {
                g_signal_handlers_disconnect_by_data (G_OBJECT (applet->priv->output), applet);
                g_object_unref (applet->priv->output);
        }

        applet->priv->output = (stream == NULL) ? NULL : g_object_ref (stream);
        if (applet->priv->output != NULL) {
                g_signal_connect (G_OBJECT (applet->priv->output),
                                  "control-added",
                                  G_CALLBACK (on_output_stream_control_added),
                                  applet);
                g_signal_connect (G_OBJECT (applet->priv->output),
                                  "control-removed",
                                  G_CALLBACK (on_output_stream_control_removed),
                                  applet);
        }

        /* Return TRUE if the default output stream has changed */
        return TRUE;
}

static gboolean
update_default_input_stream (GvcApplet *applet)
{
        MateMixerStream *stream;

        stream = mate_mixer_context_get_default_input_stream (applet->priv->context);
        if (stream == applet->priv->input)
                return FALSE;

        /* The input stream has changed */
        if (applet->priv->input != NULL) {
                g_signal_handlers_disconnect_by_data (G_OBJECT (applet->priv->input), applet);
                g_object_unref (applet->priv->input);
        }

        applet->priv->input = (stream == NULL) ? NULL : g_object_ref (stream);
        /* Return TRUE if the default input stream has changed */
        return TRUE;
}

static void
on_context_state_notify (MateMixerContext *context,
                         GParamSpec       *pspec,
                         GvcApplet        *applet)
{
        MateMixerState state = mate_mixer_context_get_state (context);

        switch (state) {
        case MATE_MIXER_STATE_FAILED:
                g_warning ("Failed to connect to a sound system");
                break;

        case MATE_MIXER_STATE_READY:
                update_default_output_stream (applet);
                update_default_input_stream (applet);
                connect_input_stream_control_signals (applet);

                /* Each applet change may affect the visibility of the icons */
                update_icon_output (applet);
                update_icon_input (applet);
                break;
        default:
                break;
        }
}

static void
on_context_default_input_stream_notify (MateMixerContext *context,
                                        GParamSpec       *pspec,
                                        GvcApplet        *applet)
{
        if (update_default_input_stream (applet) == FALSE)
                return;

        connect_input_stream_control_signals (applet);
        update_icon_input (applet);
}

static void
on_context_default_output_stream_notify (MateMixerContext *control,
                                         GParamSpec       *pspec,
                                         GvcApplet        *applet)
{
        if (update_default_output_stream (applet) == FALSE)
                return;

        update_icon_output (applet);
}

static void
on_context_stream_added (MateMixerContext *context,
                         const gchar      *name,
                         GvcApplet        *applet)
{
        update_default_output_stream (applet);
        update_default_input_stream (applet);
        connect_input_stream_control_signals (applet);
        update_icon_output (applet);
        update_icon_input (applet);
}

static void
on_context_stream_removed (MateMixerContext *context,
                           const gchar      *name,
                           GvcApplet        *applet)
{
        update_default_output_stream (applet);
        update_default_input_stream (applet);
        connect_input_stream_control_signals (applet);
        update_icon_output (applet);
        update_icon_input (applet);
}

void
gvc_applet_start (GvcApplet *applet)
{
        g_return_if_fail (GVC_IS_APPLET (applet));

        if (G_UNLIKELY (applet->priv->running == TRUE))
                return;

        if (G_UNLIKELY (mate_mixer_context_open (applet->priv->context) == FALSE)) {
                /* Normally this should never happen, in the worst case we
                 * should end up with the Null module */
                g_warning ("Failed to connect to a sound system");
        }

        g_debug ("Applet has been started");

        applet->priv->running = TRUE;
}

static void
gvc_applet_dispose (GObject *object)
{
        GvcApplet *applet = GVC_APPLET (object);

        if (applet->priv->output != NULL) {
                g_signal_handlers_disconnect_by_data (G_OBJECT (applet->priv->output), applet);
                g_clear_object (&applet->priv->output);
        }
        if (applet->priv->input != NULL) {
                g_signal_handlers_disconnect_by_data (G_OBJECT (applet->priv->input), applet);
                g_clear_object (&applet->priv->input);
        }
        if (applet->priv->context != NULL)
                disconnect_input_stream_control_signals (applet);

        g_clear_object (&applet->priv->context);
        g_clear_object (&applet->priv->active_player);
        g_clear_object (&applet->priv->selected_player);
        g_clear_object (&applet->priv->mpris_manager);
        g_clear_object (&applet->priv->applet_settings);
        g_clear_object (&applet->priv->icon_input);
        g_clear_object (&applet->priv->icon_output);

        G_OBJECT_CLASS (gvc_applet_parent_class)->dispose (object);
}

static void
gvc_applet_class_init (GvcAppletClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = gvc_applet_dispose;
}

static void
gvc_applet_init (GvcApplet *applet)
{
        applet->priv = gvc_applet_get_instance_private (applet);

        applet->priv->icon_input  = gvc_stream_applet_icon_new (NULL, icon_names_input);
        applet->priv->icon_output = gvc_stream_applet_icon_new (NULL, icon_names_output);
        applet->priv->applet_settings = new_applet_settings ();
        gvc_stream_applet_icon_set_applet_settings (applet->priv->icon_input,
                                                    applet->priv->applet_settings);
        gvc_stream_applet_icon_set_applet_settings (applet->priv->icon_output,
                                                    applet->priv->applet_settings);
        applet->priv->player_widget = gvc_player_widget_new ();
        gvc_stream_applet_icon_set_player_widget (applet->priv->icon_output,
                                                  applet->priv->player_widget);
        g_signal_connect (applet->priv->player_widget,
                          "player-selected",
                          G_CALLBACK (on_player_widget_player_selected),
                          applet);
        applet->priv->track_label = GTK_LABEL (gtk_label_new (NULL));
        gtk_widget_set_no_show_all (GTK_WIDGET (applet->priv->track_label), TRUE);
        gtk_label_set_ellipsize (applet->priv->track_label, PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars (applet->priv->track_label, 30);
        gtk_label_set_single_line_mode (applet->priv->track_label, TRUE);
        gtk_widget_set_margin_start (GTK_WIDGET (applet->priv->track_label), 4);

        gvc_stream_applet_icon_set_display_name (applet->priv->icon_input,  _("Input"));
        gvc_stream_applet_icon_set_display_name (applet->priv->icon_output, _("Output"));

        applet->priv->context = mate_mixer_context_new ();
        applet->priv->mpris_manager = gvc_mpris_manager_new ();

        mate_mixer_context_set_app_name (applet->priv->context, _("MATE Volume Control Applet"));

        mate_mixer_context_set_app_id (applet->priv->context, GVC_APPLET_DBUS_NAME);
        mate_mixer_context_set_app_version (applet->priv->context, VERSION);
        mate_mixer_context_set_app_icon (applet->priv->context, APPLET_ICON);

        g_signal_connect (G_OBJECT (applet->priv->context),
                          "notify::state",
                          G_CALLBACK (on_context_state_notify),
                          applet);
        g_signal_connect (G_OBJECT (applet->priv->context),
                          "notify::default-input-stream",
                          G_CALLBACK (on_context_default_input_stream_notify),
                          applet);
        g_signal_connect (G_OBJECT (applet->priv->context),
                          "notify::default-output-stream",
                          G_CALLBACK (on_context_default_output_stream_notify),
                          applet);
        g_signal_connect (G_OBJECT (applet->priv->context),
                          "stream-added",
                          G_CALLBACK (on_context_stream_added),
                          applet);
        g_signal_connect (G_OBJECT (applet->priv->context),
                          "stream-removed",
                          G_CALLBACK (on_context_stream_removed),
                          applet);
        g_signal_connect (G_OBJECT (applet->priv->mpris_manager),
                          "player-added",
                          G_CALLBACK (on_mpris_player_added),
                          applet);
        g_signal_connect (G_OBJECT (applet->priv->mpris_manager),
                          "player-removed",
                          G_CALLBACK (on_mpris_player_removed),
                          applet);
        if (applet->priv->applet_settings != NULL) {
                g_signal_connect (G_OBJECT (applet->priv->applet_settings),
                                  "changed",
                                  G_CALLBACK (on_applet_settings_changed),
                                  applet);
        }
}

GvcApplet *
gvc_applet_new (void)
{
        return g_object_new (GVC_TYPE_APPLET, NULL);
}

static void
gvc_applet_set_size(GtkWidget* widget, int size, gpointer user_data)
{
        GvcApplet *applet = user_data;

        /*Iterate through the icon sizes so they can be kept sharp*/
        if (size < 22)
                size = 16;
        else if (size < 24)
                size = 22;
        else if (size < 32)
                size = 24;
        else if (size < 48)
                size = 32;

        gvc_stream_applet_icon_set_size (applet->priv->icon_input, size);
        gvc_stream_applet_icon_set_size (applet->priv->icon_output, size);
}

static void
gvc_applet_set_mute (GtkWidget* widget, int size, gpointer user_data)
{
        GvcApplet *applet = user_data;
        gboolean is_muted;
        GtkAction *action;

        is_muted = gvc_stream_applet_icon_get_mute (applet->priv->icon_output);

        action = gtk_action_group_get_action (applet->priv->action_group, "MuteOutput");

        if (is_muted) {
                gtk_action_set_label (action, _("Unmute Output"));
                gtk_action_set_icon_name (action, "audio-volume-medium");
        }
        else
        {
                gtk_action_set_label (action, _("Mute Output"));
                gtk_action_set_icon_name (action, "audio-volume-muted");
        }
}

static void
gvc_applet_set_orient(GtkWidget *widget, MatePanelAppletOrient orient, gpointer user_data)
{
        GvcApplet *applet = user_data;

        gvc_stream_applet_icon_set_orient (applet->priv->icon_input, orient);
        gvc_stream_applet_icon_set_orient (applet->priv->icon_output, orient);
        update_track_label (applet);
}

static void
menu_output_mute (GtkAction *action, GvcApplet *applet)
{
        gboolean               is_muted;

        is_muted = gvc_stream_applet_icon_get_mute(applet->priv->icon_output);
        if (!is_muted) {
                gvc_stream_applet_icon_set_mute (applet->priv->icon_output, TRUE);
                gtk_action_set_label (action, "Unmute Output");
                gtk_action_set_icon_name( action, "audio-volume-medium");

        }
        else {
                gvc_stream_applet_icon_set_mute (applet->priv->icon_output, FALSE);
                gtk_action_set_label (action, "Mute Output");
                gtk_action_set_icon_name (action, "audio-volume-muted");
       }
}

static void
menu_activate_open_volume_control (GtkAction *action, GvcApplet *applet)
{
        gvc_stream_applet_icon_volume_control (applet->priv->icon_output);
}

gboolean
gvc_applet_fill (GvcApplet *applet, MatePanelApplet* applet_widget)
{
#ifndef IN_PROCESS
        g_set_application_name (_("Volume Control Applet"));
        gtk_window_set_default_icon_name (APPLET_ICON);
        mate_panel_applet_set_flags (applet_widget, MATE_PANEL_APPLET_EXPAND_MINOR);
#endif
        applet->priv->applet = applet_widget;
        switch (mate_panel_applet_get_orient (applet->priv->applet)) {
        case MATE_PANEL_APPLET_ORIENT_UP:
                applet->priv->box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
		break;
        case MATE_PANEL_APPLET_ORIENT_DOWN:
                applet->priv->box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
		break;
        case MATE_PANEL_APPLET_ORIENT_LEFT:
                applet->priv->box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
		break;
        case MATE_PANEL_APPLET_ORIENT_RIGHT:
                applet->priv->box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
        break;
        }

        gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (applet->priv->applet)), "mate-volume-applet");

        /* Define an initial size and orientation */
        gvc_stream_applet_icon_set_size (applet->priv->icon_input, mate_panel_applet_get_size (applet->priv->applet));
        gvc_stream_applet_icon_set_size (applet->priv->icon_output, mate_panel_applet_get_size (applet->priv->applet));
        gvc_stream_applet_icon_set_orient (applet->priv->icon_input, mate_panel_applet_get_orient (applet->priv->applet));
        gvc_stream_applet_icon_set_orient (applet->priv->icon_output, mate_panel_applet_get_orient (applet->priv->applet));

        /* we add the Gtk buttons into the applet */
        gtk_box_pack_start (applet->priv->box, GTK_WIDGET (applet->priv->icon_input), TRUE, TRUE, 2);
        gtk_box_pack_start (applet->priv->box, GTK_WIDGET (applet->priv->icon_output), TRUE, TRUE, 2);
        gtk_box_pack_start (applet->priv->box, GTK_WIDGET (applet->priv->track_label), FALSE, FALSE, 2);
        gtk_container_add (GTK_CONTAINER (applet->priv->applet), GTK_WIDGET (applet->priv->box));
        gtk_widget_show_all (GTK_WIDGET (applet->priv->applet));
        update_track_label (applet);

        /* Enable 'scroll-event' signal to be received  */
        gtk_widget_add_events (GTK_WIDGET(applet->priv->icon_input), GDK_SCROLL_MASK);
        gtk_widget_add_events (GTK_WIDGET(applet->priv->icon_output), GDK_SCROLL_MASK);
        gtk_widget_add_events (GTK_WIDGET(applet->priv->icon_input), GDK_BUTTON_PRESS_MASK);
        gtk_widget_add_events (GTK_WIDGET(applet->priv->icon_output), GDK_BUTTON_PRESS_MASK);
        g_signal_connect (GTK_WIDGET (applet->priv->icon_input),
                          "button-press-event",
                          G_CALLBACK (on_applet_icon_context_menu),
                          applet);
        g_signal_connect (GTK_WIDGET (applet->priv->icon_output),
                          "button-press-event",
                          G_CALLBACK (on_applet_icon_context_menu),
                          applet);

        /* Update icons on size/orientation changes*/
        g_object_connect (applet->priv->applet,
                         "signal::change_size", gvc_applet_set_size, applet,
                         "signal::change_orient", gvc_applet_set_orient, applet,
                         "signal::event-after", gvc_applet_set_mute, applet,
                         NULL);

        /* set up context menu */
        applet->priv->action_group = gtk_action_group_new ("Volume Control Applet Actions");
        gtk_action_group_set_translation_domain (applet->priv->action_group, GETTEXT_PACKAGE);
        gtk_action_group_add_actions (applet->priv->action_group, applet_menu_actions,
                                      G_N_ELEMENTS (applet_menu_actions), applet);

        mate_panel_applet_setup_menu (applet->priv->applet, ui, applet->priv->action_group);

        return TRUE;
}
