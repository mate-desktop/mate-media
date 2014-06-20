/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
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

// XXX there are some bugs unrelated to the port, let's see if I can fix them:
// - clicking in the slider makes it disappear
// - drag all the way down to mute, then start dragging up and the change won't
//   be reflected until i stop dragging
// - disable the slider if the volume is not changable

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
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
        MateMixerControl    *control;
};

static void gvc_applet_class_init (GvcAppletClass *klass);
static void gvc_applet_init       (GvcApplet      *applet);

G_DEFINE_TYPE (GvcApplet, gvc_applet, G_TYPE_OBJECT)

static void
update_icon_input (GvcApplet *applet)
{
        MateMixerStream *stream;
        gboolean         show = FALSE;

        /* Enable the input icon in case there is an input stream present and there
         * is a non-mixer application using the input */
        stream = mate_mixer_control_get_default_input_stream (applet->priv->control);
        if (stream != NULL) {
                const gchar *app;
                const GList *inputs =
                        mate_mixer_control_list_streams (applet->priv->control);

                while (inputs) {
                        MateMixerStream *input = MATE_MIXER_STREAM (inputs->data);
                        MateMixerStreamFlags flags = mate_mixer_stream_get_flags (input);

                        if (flags & MATE_MIXER_STREAM_INPUT &&
                            flags & MATE_MIXER_STREAM_APPLICATION) {
                                MateMixerClientStream *client =
                                        MATE_MIXER_CLIENT_STREAM (input);

                                app = mate_mixer_client_stream_get_app_id (client);
                                if (app == NULL) {
                                        /* A recording application which has no
                                         * identifier set */
                                        show = TRUE;
                                        break;
                                }

                                if (!g_str_equal (app, "org.mate.VolumeControl") &&
                                    !g_str_equal (app, "org.gnome.VolumeControl") &&
                                    !g_str_equal (app, "org.PulseAudio.pavucontrol")) {
                                        show = TRUE;
                                        break;
                                }
                        }
                        inputs = inputs->next;
                }
        }

        gvc_stream_status_icon_set_mixer_stream (applet->priv->icon_input, stream);

        gtk_status_icon_set_visible (GTK_STATUS_ICON (applet->priv->icon_input), show);
}

static void
update_icon_output (GvcApplet *applet)
{
        MateMixerStream *stream;

        stream = mate_mixer_control_get_default_output_stream (applet->priv->control);

        gvc_stream_status_icon_set_mixer_stream (applet->priv->icon_output, stream);

        /* Enable the output icon in case there is an output stream present */
        gtk_status_icon_set_visible (GTK_STATUS_ICON (applet->priv->icon_output),
                                     (stream != NULL) ? TRUE : FALSE);
}

void
gvc_applet_start (GvcApplet *applet)
{
        g_return_if_fail (GVC_IS_APPLET (applet));

        if (G_UNLIKELY (applet->priv->running))
                return;

        if (mate_mixer_control_open (applet->priv->control) == FALSE) {
                /* Normally this should never happen, in the worst case we
                 * should end up with the Null module */
                g_warning ("Failed to connect to a sound system");

                gtk_status_icon_set_visible (GTK_STATUS_ICON (applet->priv->icon_output),
                                             FALSE);
                gtk_status_icon_set_visible (GTK_STATUS_ICON (applet->priv->icon_input),
                                             FALSE);
        }

        applet->priv->running = TRUE;
}

static void
on_control_state_notify (MateMixerControl *control,
                         GParamSpec       *pspec,
                         GvcApplet        *applet)
{
        MateMixerState state = mate_mixer_control_get_state (control);

        if (state == MATE_MIXER_STATE_FAILED)
                g_warning ("Failed to connect to a sound system");

        /* Each status change may affect the visibility of the icons */
        update_icon_output (applet);
        update_icon_input (applet);
}

static void
on_control_default_input_notify (MateMixerControl *control,
                                 GParamSpec       *pspec,
                                 GvcApplet        *applet)
{
        update_icon_input (applet);
}

static void
on_control_default_output_notify (MateMixerControl *control,
                                  GParamSpec       *pspec,
                                  GvcApplet        *applet)
{
        update_icon_output (applet);
}

static void
on_control_stream_added (MateMixerControl *control,
                         const gchar      *name,
                         GvcApplet        *applet)
{
        MateMixerStream      *stream;
        MateMixerStreamFlags  flags;

        stream = mate_mixer_control_get_stream (control, name);
        if (G_UNLIKELY (stream == NULL))
                return;

        flags = mate_mixer_stream_get_flags (stream);

        /* Newly added input application stream may cause the input status
         * icon to change visibility */
        if (flags & MATE_MIXER_STREAM_INPUT &&
            flags & MATE_MIXER_STREAM_APPLICATION)
                update_icon_input (applet);
}

static void
on_control_stream_removed (MateMixerControl *control,
                           const gchar      *name,
                           GvcApplet        *applet)
{
        /* The removed stream could be an application input, which may cause
         * the input status icon to disappear */
        update_icon_input (applet);
}

static void
gvc_applet_dispose (GObject *object)
{
        GvcApplet *applet = GVC_APPLET (object);

        g_clear_object (&applet->priv->control);
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

        applet->priv->control = mate_mixer_control_new ();

        mate_mixer_control_set_app_name (applet->priv->control,
                                         _("MATE Volume Control Applet"));

        mate_mixer_control_set_app_id (applet->priv->control, GVC_APPLET_DBUS_NAME);
        mate_mixer_control_set_app_version (applet->priv->control, VERSION);
        mate_mixer_control_set_app_icon (applet->priv->control, "multimedia-volume-control");

        g_signal_connect (applet->priv->control,
                          "notify::state",
                          G_CALLBACK (on_control_state_notify),
                          applet);
        g_signal_connect (applet->priv->control,
                          "notify::default-input",
                          G_CALLBACK (on_control_default_input_notify),
                          applet);
        g_signal_connect (applet->priv->control,
                          "notify::default-output",
                          G_CALLBACK (on_control_default_output_notify),
                          applet);
        g_signal_connect (applet->priv->control,
                          "stream-added",
                          G_CALLBACK (on_control_stream_added),
                          applet);
        g_signal_connect (applet->priv->control,
                          "stream-removed",
                          G_CALLBACK (on_control_stream_removed),
                          applet);
}

GvcApplet *
gvc_applet_new (void)
{
        return g_object_new (GVC_TYPE_APPLET, NULL);
}
