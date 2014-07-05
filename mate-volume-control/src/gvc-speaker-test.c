/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Bastien Nocera
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <canberra.h>
#include <libmatemixer/matemixer.h>

#ifdef HAVE_PULSEAUDIO
#include <pulse/pulseaudio.h>
#endif

#include "gvc-speaker-test.h"

#define GVC_SPEAKER_TEST_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GVC_TYPE_SPEAKER_TEST, GvcSpeakerTestPrivate))

struct _GvcSpeakerTestPrivate
{
        // XXX pulse constant
        GtkWidget        *channel_controls[PA_CHANNEL_POSITION_MAX];
        ca_context       *canberra;
        MateMixerControl *control;
        MateMixerDevice  *device;
};

enum {
        PROP_0,
        PROP_CONTROL,
        PROP_DEVICE,
        N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void     gvc_speaker_test_class_init (GvcSpeakerTestClass *klass);
static void     gvc_speaker_test_init       (GvcSpeakerTest      *speaker_test);
static void     gvc_speaker_test_dispose    (GObject *object);
static void     gvc_speaker_test_finalize   (GObject            *object);
static void     update_channel_map          (GvcSpeakerTest *speaker_test);

G_DEFINE_TYPE (GvcSpeakerTest, gvc_speaker_test, GTK_TYPE_TABLE)

static const int position_table[] = {
        /* Position, X, Y */
        MATE_MIXER_CHANNEL_FRONT_LEFT, 0, 0,
        MATE_MIXER_CHANNEL_FRONT_LEFT_CENTER, 1, 0,
        MATE_MIXER_CHANNEL_FRONT_CENTER, 2, 0,
        MATE_MIXER_CHANNEL_MONO, 2, 0,
        MATE_MIXER_CHANNEL_FRONT_RIGHT_CENTER, 3, 0,
        MATE_MIXER_CHANNEL_FRONT_RIGHT, 4, 0,
        MATE_MIXER_CHANNEL_SIDE_LEFT, 0, 1,
        MATE_MIXER_CHANNEL_SIDE_RIGHT, 4, 1,
        MATE_MIXER_CHANNEL_BACK_LEFT, 0, 2,
        MATE_MIXER_CHANNEL_BACK_CENTER, 2, 2,
        MATE_MIXER_CHANNEL_BACK_RIGHT, 4, 2,
        MATE_MIXER_CHANNEL_LFE, 3, 2
};

static void
gvc_speaker_test_set_property (GObject       *object,
                               guint          prop_id,
                               const GValue  *value,
                               GParamSpec    *pspec)
{
        GvcSpeakerTest *self = GVC_SPEAKER_TEST (object);

        switch (prop_id) {
        case PROP_CONTROL:
                /* Construct-only object */
                self->priv->control = g_value_dup_object (value);
                if (self->priv->control != NULL)
                        update_channel_map (self);
                break;

        case PROP_DEVICE:
                if (self->priv->device)
                        g_object_unref (self->priv->device);

                self->priv->device = g_value_dup_object (value);
                if (self->priv->device != NULL)
                        update_channel_map (self);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_speaker_test_get_property (GObject     *object,
                               guint        prop_id,
                               GValue      *value,
                               GParamSpec  *pspec)
{
        GvcSpeakerTest *self = GVC_SPEAKER_TEST (object);

        switch (prop_id) {
        case PROP_CONTROL:
                g_value_set_object (value, self->priv->control);
                break;
        case PROP_DEVICE:
                g_value_set_object (value, self->priv->device);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_speaker_test_class_init (GvcSpeakerTestClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose  = gvc_speaker_test_dispose;
        object_class->finalize = gvc_speaker_test_finalize;
        object_class->set_property = gvc_speaker_test_set_property;
        object_class->get_property = gvc_speaker_test_get_property;

        properties[PROP_CONTROL] =
                g_param_spec_object ("control",
                                     "Control",
                                     "The mixer controller",
                                     MATE_MIXER_TYPE_CONTROL,
                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

        properties[PROP_DEVICE] =
                g_param_spec_object ("device",
                                     "Device",
                                     "The mixer device",
                                     MATE_MIXER_TYPE_DEVICE,
                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

        g_object_class_install_properties (object_class, N_PROPERTIES, properties);

        g_type_class_add_private (klass, sizeof (GvcSpeakerTestPrivate));
}

static const gchar *
sound_name (MateMixerChannelPosition position)
{
        switch (position) {
        case MATE_MIXER_CHANNEL_FRONT_LEFT:
                return "audio-channel-front-left";
        case MATE_MIXER_CHANNEL_FRONT_RIGHT:
                return "audio-channel-front-right";
        case MATE_MIXER_CHANNEL_FRONT_CENTER:
                return "audio-channel-front-center";
        case MATE_MIXER_CHANNEL_BACK_LEFT:
                return "audio-channel-rear-left";
        case MATE_MIXER_CHANNEL_BACK_RIGHT:
                return "audio-channel-rear-right";
        case MATE_MIXER_CHANNEL_BACK_CENTER:
                return "audio-channel-rear-center";
        case MATE_MIXER_CHANNEL_LFE:
                return "audio-channel-lfe";
        case MATE_MIXER_CHANNEL_SIDE_LEFT:
                return "audio-channel-side-left";
        case MATE_MIXER_CHANNEL_SIDE_RIGHT:
                return "audio-channel-side-right";
        default:
                return NULL;
        }
}

static const gchar *
icon_name (MateMixerChannelPosition position, gboolean playing)
{
        switch (position) {
        case MATE_MIXER_CHANNEL_FRONT_LEFT:
                return playing ? "audio-speaker-left-testing" : "audio-speaker-left";
        case MATE_MIXER_CHANNEL_FRONT_RIGHT:
                return playing ? "audio-speaker-right-testing" : "audio-speaker-right";
        case MATE_MIXER_CHANNEL_FRONT_CENTER:
                return playing ? "audio-speaker-center-testing" : "audio-speaker-center";
        case MATE_MIXER_CHANNEL_BACK_LEFT:
                return playing ? "audio-speaker-left-back-testing" : "audio-speaker-left-back";
        case MATE_MIXER_CHANNEL_BACK_RIGHT:
                return playing ? "audio-speaker-right-back-testing" : "audio-speaker-right-back";
        case MATE_MIXER_CHANNEL_BACK_CENTER:
                return playing ? "audio-speaker-center-back-testing" : "audio-speaker-center-back";
        case MATE_MIXER_CHANNEL_LFE:
                return playing ? "audio-subwoofer-testing" : "audio-subwoofer";
        case MATE_MIXER_CHANNEL_SIDE_LEFT:
                return playing ? "audio-speaker-left-side-testing" : "audio-speaker-left-side";
        case MATE_MIXER_CHANNEL_SIDE_RIGHT:
                return playing ? "audio-speaker-right-side-testing" : "audio-speaker-right-side";
        default:
                return NULL;
        }
}

static void
update_button (GtkWidget *control)
{
        GtkWidget *button;
        GtkWidget *image;
        pa_channel_position_t position;
        gboolean playing;

        button = g_object_get_data (G_OBJECT (control), "button");
        image = g_object_get_data (G_OBJECT (control), "image");
        position = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (control), "position"));
        playing = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (control), "playing"));
        gtk_button_set_label (GTK_BUTTON (button), playing ? _("Stop") : _("Test"));
        gtk_image_set_from_icon_name (GTK_IMAGE (image), icon_name (position, playing), GTK_ICON_SIZE_DIALOG);
}

static const char *
channel_position_string (MateMixerChannelPosition position, gboolean pretty)
{
#ifdef HAVE_PULSEAUDIO
        pa_channel_position_t pa_position;

        switch (position) {
        case MATE_MIXER_CHANNEL_MONO:
                pa_position = PA_CHANNEL_POSITION_MONO;
                break;
        case MATE_MIXER_CHANNEL_FRONT_LEFT:
                pa_position = PA_CHANNEL_POSITION_FRONT_LEFT;
                break;
        case MATE_MIXER_CHANNEL_FRONT_RIGHT:
                pa_position = PA_CHANNEL_POSITION_FRONT_RIGHT;
                break;
        case MATE_MIXER_CHANNEL_FRONT_CENTER:
                pa_position = PA_CHANNEL_POSITION_FRONT_CENTER;
                break;
        case MATE_MIXER_CHANNEL_LFE:
                pa_position = PA_CHANNEL_POSITION_LFE;
                break;
        case MATE_MIXER_CHANNEL_BACK_LEFT:
                pa_position = PA_CHANNEL_POSITION_REAR_LEFT;
                break;
        case MATE_MIXER_CHANNEL_BACK_RIGHT:
                pa_position = PA_CHANNEL_POSITION_REAR_RIGHT;
                break;
        case MATE_MIXER_CHANNEL_BACK_CENTER:
                pa_position = PA_CHANNEL_POSITION_REAR_CENTER;
                break;
        case MATE_MIXER_CHANNEL_FRONT_LEFT_CENTER:
                pa_position = PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
                break;
        case MATE_MIXER_CHANNEL_FRONT_RIGHT_CENTER:
                pa_position = PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
                break;
        case MATE_MIXER_CHANNEL_SIDE_LEFT:
                pa_position = PA_CHANNEL_POSITION_SIDE_LEFT;
                break;
        case MATE_MIXER_CHANNEL_SIDE_RIGHT:
                pa_position = PA_CHANNEL_POSITION_SIDE_RIGHT;
                break;
        case MATE_MIXER_CHANNEL_TOP_FRONT_LEFT:
                pa_position = PA_CHANNEL_POSITION_TOP_FRONT_LEFT;
                break;
        case MATE_MIXER_CHANNEL_TOP_FRONT_RIGHT:
                pa_position = PA_CHANNEL_POSITION_TOP_FRONT_RIGHT;
                break;
        case MATE_MIXER_CHANNEL_TOP_FRONT_CENTER:
                pa_position = PA_CHANNEL_POSITION_TOP_FRONT_CENTER;
                break;
        case MATE_MIXER_CHANNEL_TOP_CENTER:
                pa_position = PA_CHANNEL_POSITION_TOP_CENTER;
                break;
        case MATE_MIXER_CHANNEL_TOP_BACK_LEFT:
                pa_position = PA_CHANNEL_POSITION_TOP_REAR_LEFT;
                break;
        case MATE_MIXER_CHANNEL_TOP_BACK_RIGHT:
                pa_position = PA_CHANNEL_POSITION_TOP_REAR_RIGHT;
                break;
        case MATE_MIXER_CHANNEL_TOP_BACK_CENTER:
                pa_position = PA_CHANNEL_POSITION_TOP_REAR_CENTER;
                break;
        default:
                pa_position = PA_CHANNEL_POSITION_INVALID;
                break;
        }

        if (pretty)
                return pa_channel_position_to_pretty_string (pa_position);
        else
                return pa_channel_position_to_string (pa_position);
#endif
        return NULL;
}

static gboolean
idle_cb (GtkWidget *control)
{
        if (control != NULL) {
                /* This is called in the background thread, hence forward to main thread
                 * via idle callback */
                g_object_set_data (G_OBJECT (control), "playing", GINT_TO_POINTER (FALSE));

                update_button (control);
        }
        return FALSE;
}

static void
finish_cb (ca_context *c, uint32_t id, int error_code, void *userdata)
{
        GtkWidget *control = (GtkWidget *) userdata;

        if (error_code == CA_ERROR_DESTROYED || control == NULL)
                return;

        g_idle_add ((GSourceFunc) idle_cb, control);
}

static void
on_test_button_clicked (GtkButton *button, GtkWidget *control)
{
        gboolean    playing;
        ca_context *canberra;

        canberra = g_object_get_data (G_OBJECT (control), "canberra");

        ca_context_cancel (canberra, 1);

        playing = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (control), "playing"));

        if (playing) {
                g_object_set_data (G_OBJECT (control), "playing", GINT_TO_POINTER (FALSE));
        } else {
                MateMixerChannelPosition position;
                const char  *name;
                ca_proplist *proplist;

                position = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (control), "position"));

                ca_proplist_create (&proplist);
                ca_proplist_sets (proplist, CA_PROP_MEDIA_ROLE, "test");
                ca_proplist_sets (proplist,
                                  CA_PROP_MEDIA_NAME,
                                  channel_position_string (position, TRUE));
                ca_proplist_sets (proplist,
                                  CA_PROP_CANBERRA_FORCE_CHANNEL,
                                  channel_position_string (position, FALSE));

                ca_proplist_sets (proplist, CA_PROP_CANBERRA_ENABLE, "1");

                name = sound_name (position);
                if (name != NULL) {
                        ca_proplist_sets (proplist, CA_PROP_EVENT_ID, name);
                        playing = ca_context_play_full (canberra, 1, proplist, finish_cb, control) >= 0;
                }

                if (!playing) {
                        ca_proplist_sets (proplist, CA_PROP_EVENT_ID, "audio-test-signal");
                        playing = ca_context_play_full (canberra, 1, proplist, finish_cb, control) >= 0;
                }

                if (!playing) {
                        ca_proplist_sets(proplist, CA_PROP_EVENT_ID, "bell-window-system");
                        playing = ca_context_play_full (canberra, 1, proplist, finish_cb, control) >= 0;
                }
                g_object_set_data (G_OBJECT (control), "playing", GINT_TO_POINTER(playing));
        }

        update_button (control);
}

static GtkWidget *
channel_control_new (ca_context *canberra, MateMixerChannelPosition position)
{
        GtkWidget *control;
        GtkWidget *box;
        GtkWidget *label;
        GtkWidget *image;
        GtkWidget *test_button;
        const char *name;

        control = gtk_vbox_new (FALSE, 6);
        g_object_set_data (G_OBJECT (control), "playing", GINT_TO_POINTER (FALSE));
        g_object_set_data (G_OBJECT (control), "position", GINT_TO_POINTER (position));
        g_object_set_data (G_OBJECT (control), "canberra", canberra);

        name = icon_name (position, FALSE);
        if (name == NULL)
                name = "audio-volume-medium";

        image = gtk_image_new_from_icon_name (name, GTK_ICON_SIZE_DIALOG);
        g_object_set_data (G_OBJECT (control), "image", image);
        gtk_box_pack_start (GTK_BOX (control), image, FALSE, FALSE, 0);

        label = gtk_label_new (channel_position_string (position, TRUE));
        gtk_box_pack_start (GTK_BOX (control), label, FALSE, FALSE, 0);

        test_button = gtk_button_new_with_label (_("Test"));
        g_signal_connect (G_OBJECT (test_button), "clicked",
                          G_CALLBACK (on_test_button_clicked), control);

        g_object_set_data (G_OBJECT (control), "button", test_button);

        box = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX (box), test_button, TRUE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (control), box, FALSE, FALSE, 0);

        gtk_widget_show_all (control);

        return control;
}

static void
create_channel_controls (GvcSpeakerTest *test)
{
        guint i;

        for (i = 0; i < G_N_ELEMENTS (position_table); i += 3) {
                test->priv->channel_controls[position_table[i]] =
                        channel_control_new (test->priv->canberra,
                                             (MateMixerChannelPosition) position_table[i]);

                gtk_table_attach (GTK_TABLE (test),
                                  test->priv->channel_controls[position_table[i]],
                                  position_table[i+1],
                                  position_table[i+1]+1,
                                  position_table[i+2],
                                  position_table[i+2]+1,
                                  GTK_EXPAND, GTK_EXPAND, 0, 0);
        }
}

static MateMixerStream *
get_device_output_stream (MateMixerControl *control, MateMixerDevice *device)
{
        MateMixerStream *stream;
        const GList     *streams;

        streams = mate_mixer_control_list_streams (control);
        while (streams) {
                stream = MATE_MIXER_STREAM (streams->data);

                if (mate_mixer_stream_get_device (stream) == device) {
                        if ((mate_mixer_stream_get_flags (stream) &
                            (MATE_MIXER_STREAM_OUTPUT |
                             MATE_MIXER_STREAM_CLIENT)) == MATE_MIXER_STREAM_OUTPUT)
                                break;
                }
                stream  = NULL;
                streams = streams->next;
        }

        return stream;
}

static void
update_channel_map (GvcSpeakerTest *test)
{
        MateMixerStream *stream;
        guint i;

        if (test->priv->control == NULL ||
            test->priv->device == NULL)
                return;

        stream = get_device_output_stream (test->priv->control, test->priv->device);
        if (G_UNLIKELY (stream == NULL)) {
                g_debug ("Failed to find an output stream for sound device %s",
                         mate_mixer_device_get_name (test->priv->device));
                return;
        }

        ca_context_change_device (test->priv->canberra,
                                  mate_mixer_stream_get_name (stream));

        for (i = 0; i < G_N_ELEMENTS (position_table); i += 3)
                gtk_widget_set_visible (test->priv->channel_controls[position_table[i]],
                                        mate_mixer_stream_has_position (stream,
                                                                        position_table[i]));
}

static void
gvc_speaker_test_init (GvcSpeakerTest *test)
{
        GtkWidget *icon;

        test->priv = GVC_SPEAKER_TEST_GET_PRIVATE (test);

        ca_context_create (&test->priv->canberra);

        /* The test sounds are played for a single channel, set up using the
         * FORCE_CHANNEL property of libcanberra; this property is only supported
         * in the PulseAudio backend, so avoid other backends completely */
        ca_context_set_driver (test->priv->canberra, "pulse");

        ca_context_change_props (test->priv->canberra,
                                 CA_PROP_APPLICATION_NAME, _("MATE Volume Control Applet"),
                                 CA_PROP_APPLICATION_ID, "org.mate.VolumeControl",
                                 CA_PROP_APPLICATION_VERSION, VERSION,
                                 CA_PROP_APPLICATION_ICON_NAME, "multimedia-volume-control",
                                 NULL);

        gtk_table_resize (GTK_TABLE (test), 3, 5);

        gtk_container_set_border_width (GTK_CONTAINER (test), 12);

        gtk_table_set_homogeneous (GTK_TABLE (test), TRUE);
        gtk_table_set_row_spacings (GTK_TABLE (test), 12);
        gtk_table_set_col_spacings (GTK_TABLE (test), 12);

        create_channel_controls (test);

        icon = gtk_image_new_from_icon_name ("computer", GTK_ICON_SIZE_DIALOG);

        gtk_table_attach (GTK_TABLE (test), icon,
                          2, 3, 1, 2,
                          GTK_EXPAND,
                          GTK_EXPAND,
                          0, 0);

        gtk_widget_show (icon);
}

static void
gvc_speaker_test_dispose (GObject *object)
{
        GvcSpeakerTest *test;

        test = GVC_SPEAKER_TEST (object);

        g_clear_object (&test->priv->control);
        g_clear_object (&test->priv->device);

        G_OBJECT_CLASS (gvc_speaker_test_parent_class)->dispose (object);
}

static void
gvc_speaker_test_finalize (GObject *object)
{
        GvcSpeakerTest *test;

        test = GVC_SPEAKER_TEST (object);

        ca_context_destroy (test->priv->canberra);

        G_OBJECT_CLASS (gvc_speaker_test_parent_class)->finalize (object);
}

GtkWidget *
gvc_speaker_test_new (MateMixerControl *control, MateMixerDevice *device)
{
        GObject *test;

        g_return_val_if_fail (MATE_MIXER_IS_CONTROL (control), NULL);
        g_return_val_if_fail (MATE_MIXER_IS_DEVICE (device), NULL);

        test = g_object_new (GVC_TYPE_SPEAKER_TEST,
                             "control", control,
                             "device", device,
                             NULL);

        return GTK_WIDGET (test);
}
