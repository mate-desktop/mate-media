/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
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
#include <gtk/gtk.h>

#include <libmatemixer/matemixer.h>

#include "gvc-applet.h"
#include "gvc-stream-status-icon.h"

#define GVC_APPLET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GVC_TYPE_APPLET, GvcAppletPrivate))

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

struct _GvcAppletPrivate
{
        GvcStreamStatusIcon *icon_input;
        GvcStreamStatusIcon *icon_output;
        gboolean             running;
        MateMixerContext    *context;
        MateMixerStream     *input;
};

static void gvc_applet_class_init (GvcAppletClass *klass);
static void gvc_applet_init       (GvcApplet      *applet);

G_DEFINE_TYPE (GvcApplet, gvc_applet, G_TYPE_OBJECT)

static void
update_icon_input (GvcApplet *applet)
{
        MateMixerStreamControl *control = NULL;
        gboolean                show = FALSE;

        /* Enable the input icon in case there is an input stream present and there
         * is a non-mixer application using the input */
        if (applet->priv->input != NULL) {
                const gchar *app_id;
                const GList *inputs =
                        mate_mixer_stream_list_controls (applet->priv->input);

                control = mate_mixer_stream_get_default_control (applet->priv->input);

                while (inputs != NULL) {
                        MateMixerStreamControl    *input =
                                MATE_MIXER_STREAM_CONTROL (inputs->data);
                        MateMixerStreamControlRole role =
                                mate_mixer_stream_control_get_role (input);

                        if (role == MATE_MIXER_STREAM_CONTROL_ROLE_APPLICATION) {
                                MateMixerAppInfo *app_info =
                                        mate_mixer_stream_control_get_app_info (input);

                                app_id = mate_mixer_app_info_get_id (app_info);
                                if (app_id == NULL) {
                                        /* A recording application which has no
                                         * identifier set */
                                        g_debug ("Found a recording application control %s",
                                                 mate_mixer_stream_control_get_label (input));

                                        if G_UNLIKELY (control == NULL) {
                                                /* In the unlikely case when there is no
                                                 * default input control, use the application
                                                 * control for the icon */
                                                control = input;
                                        }
                                        show = TRUE;
                                        break;
                                }

                                if (strcmp (app_id, "org.mate.VolumeControl") != 0 &&
                                    strcmp (app_id, "org.gnome.VolumeControl") != 0 &&
                                    strcmp (app_id, "org.PulseAudio.pavucontrol") != 0) {
                                        g_debug ("Found a recording application %s", app_id);

                                        if G_UNLIKELY (control == NULL)
                                                control = input;

                                        show = TRUE;
                                        break;
                                }
                        }
                        inputs = inputs->next;
                }

                if (show == TRUE)
                        g_debug ("Input icon enabled");
                else
                        g_debug ("There is no recording application, input icon disabled");
        }

        gvc_stream_status_icon_set_control (applet->priv->icon_input, control);

        gtk_status_icon_set_visible (GTK_STATUS_ICON (applet->priv->icon_input), show);
}

static void
update_icon_output (GvcApplet *applet)
{
        MateMixerStream        *stream;
        MateMixerStreamControl *control = NULL;

        stream = mate_mixer_context_get_default_output_stream (applet->priv->context);
        if (stream != NULL)
                control = mate_mixer_stream_get_default_control (stream);

        gvc_stream_status_icon_set_control (applet->priv->icon_output, control);

        if (control != NULL) {
                g_debug ("Output icon enabled");
                gtk_status_icon_set_visible (GTK_STATUS_ICON (applet->priv->icon_output),
                                             TRUE);
        }
        else {
                g_debug ("There is no output stream/control, output icon disabled");
                gtk_status_icon_set_visible (GTK_STATUS_ICON (applet->priv->icon_output),
                                             FALSE);
        }
}

static void
on_input_stream_control_added (MateMixerStream *stream,
                               const gchar     *name,
                               GvcApplet       *applet)
{
        MateMixerStreamControl *control;

        control = mate_mixer_stream_get_control (stream, name);
        if G_LIKELY (control != NULL) {
                MateMixerStreamControlRole role =
                        mate_mixer_stream_control_get_role (control);

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
on_input_stream_control_removed (MateMixerStream *stream,
                                 const gchar     *name,
                                 GvcApplet       *applet)
{
        /* The removed stream could be an application input, which may cause
         * the input status icon to disappear */
        update_icon_input (applet);
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
                g_signal_handlers_disconnect_by_data (G_OBJECT (applet->priv->input),
                                                      applet);
                g_object_unref (applet->priv->input);
        }

        applet->priv->input = g_object_ref (stream);
        if (applet->priv->input != NULL) {
                g_signal_connect (G_OBJECT (applet->priv->input),
                                  "control-added",
                                  G_CALLBACK (on_input_stream_control_added),
                                  applet);
                g_signal_connect (G_OBJECT (applet->priv->input),
                                  "control-removed",
                                  G_CALLBACK (on_input_stream_control_removed),
                                  applet);
        }

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
                update_default_input_stream (applet);

                /* Each status change may affect the visibility of the icons */
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

        update_icon_input (applet);
}

static void
on_context_default_output_stream_notify (MateMixerContext *control,
                                         GParamSpec       *pspec,
                                         GvcApplet        *applet)
{
        update_icon_output (applet);
}

void
gvc_applet_start (GvcApplet *applet)
{
        g_return_if_fail (GVC_IS_APPLET (applet));

        if G_UNLIKELY (applet->priv->running == TRUE)
                return;

        if G_UNLIKELY (mate_mixer_context_open (applet->priv->context) == FALSE) {
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

        if (applet->priv->input != NULL) {
                g_signal_handlers_disconnect_by_data (G_OBJECT (applet->priv->input),
                                                      applet);
                g_clear_object (&applet->priv->input);
        }

        g_clear_object (&applet->priv->context);
        g_clear_object (&applet->priv->icon_input);
        g_clear_object (&applet->priv->icon_output);

        G_OBJECT_CLASS (gvc_applet_parent_class)->dispose (object);
}

static void
gvc_applet_class_init (GvcAppletClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = gvc_applet_dispose;

        g_type_class_add_private (klass, sizeof (GvcAppletPrivate));
}

static void
gvc_applet_init (GvcApplet *applet)
{
        applet->priv = GVC_APPLET_GET_PRIVATE (applet);

        applet->priv->icon_input  = gvc_stream_status_icon_new (NULL, icon_names_input);
        applet->priv->icon_output = gvc_stream_status_icon_new (NULL, icon_names_output);

        gvc_stream_status_icon_set_display_name (applet->priv->icon_input,  _("Input"));
        gvc_stream_status_icon_set_display_name (applet->priv->icon_output, _("Output"));

        gtk_status_icon_set_title (GTK_STATUS_ICON (applet->priv->icon_input),
                                   _("Microphone Volume"));
        gtk_status_icon_set_title (GTK_STATUS_ICON (applet->priv->icon_output),
                                   _("Sound Output Volume"));

        applet->priv->context = mate_mixer_context_new ();

        mate_mixer_context_set_app_name (applet->priv->context,
                                         _("MATE Volume Control Applet"));

        mate_mixer_context_set_app_id (applet->priv->context, GVC_APPLET_DBUS_NAME);
        mate_mixer_context_set_app_version (applet->priv->context, VERSION);
        mate_mixer_context_set_app_icon (applet->priv->context, "multimedia-volume-control");

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
}

GvcApplet *
gvc_applet_new (void)
{
        return g_object_new (GVC_TYPE_APPLET, NULL);
}
