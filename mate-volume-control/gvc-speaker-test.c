/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Bastien Nocera
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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <canberra.h>
#include <libmatemixer/matemixer.h>

#include "gvc-speaker-test.h"
#include "gvc-utils.h"

#define GVC_SPEAKER_TEST_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GVC_TYPE_SPEAKER_TEST, GvcSpeakerTestPrivate))

struct _GvcSpeakerTestPrivate
{
        GArray           *controls;
        ca_context       *canberra;
        MateMixerStream  *stream;
};

enum {
        PROP_0,
        PROP_STREAM,
        N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void gvc_speaker_test_class_init (GvcSpeakerTestClass *klass);
static void gvc_speaker_test_init       (GvcSpeakerTest      *test);
static void gvc_speaker_test_dispose    (GObject             *object);
static void gvc_speaker_test_finalize   (GObject             *object);

G_DEFINE_TYPE (GvcSpeakerTest, gvc_speaker_test, GTK_TYPE_GRID)

typedef struct {
        MateMixerChannelPosition position;
        guint left;
        guint top;
} TablePosition;

static const TablePosition positions[] = {
        /* Position, X, Y */
        { MATE_MIXER_CHANNEL_FRONT_LEFT, 0, 0, },
        { MATE_MIXER_CHANNEL_FRONT_LEFT_CENTER, 1, 0, },
        { MATE_MIXER_CHANNEL_FRONT_CENTER, 2, 0, },
        { MATE_MIXER_CHANNEL_MONO, 2, 0, },
        { MATE_MIXER_CHANNEL_FRONT_RIGHT_CENTER, 3, 0, },
        { MATE_MIXER_CHANNEL_FRONT_RIGHT, 4, 0, },
        { MATE_MIXER_CHANNEL_SIDE_LEFT, 0, 1, },
        { MATE_MIXER_CHANNEL_SIDE_RIGHT, 4, 1, },
        { MATE_MIXER_CHANNEL_BACK_LEFT, 0, 2, },
        { MATE_MIXER_CHANNEL_BACK_CENTER, 2, 2, },
        { MATE_MIXER_CHANNEL_BACK_RIGHT, 4, 2, },
        { MATE_MIXER_CHANNEL_LFE, 3, 2 }
};

MateMixerStream *
gvc_speaker_test_get_stream (GvcSpeakerTest *test)
{
        g_return_val_if_fail (GVC_IS_SPEAKER_TEST (test), NULL);

        return test->priv->stream;
}

static void
gvc_speaker_test_set_stream (GvcSpeakerTest *test, MateMixerStream *stream)
{
        MateMixerStreamControl *control;
        const gchar            *name;
        guint                   i;

        name = mate_mixer_stream_get_name (stream);
        control = mate_mixer_stream_get_default_control (stream);

        ca_context_change_device (test->priv->canberra, name);

        for (i = 0; i < G_N_ELEMENTS (positions); i++) {
                gboolean has_position =
                        mate_mixer_stream_control_has_channel_position (control, positions[i].position);

                gtk_widget_set_visible (g_array_index (test->priv->controls, GtkWidget *, i),
                                        has_position);
        }

        test->priv->stream = g_object_ref (stream);
}

static void
gvc_speaker_test_set_property (GObject       *object,
                               guint          prop_id,
                               const GValue  *value,
                               GParamSpec    *pspec)
{
        GvcSpeakerTest *self = GVC_SPEAKER_TEST (object);

        switch (prop_id) {
        case PROP_STREAM:
                gvc_speaker_test_set_stream (self, g_value_get_object (value));
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
        case PROP_STREAM:
                g_value_set_object (value, self->priv->stream);
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

        properties[PROP_STREAM] =
                g_param_spec_object ("stream",
                                     "Stream",
                                     "MateMixer stream",
                                     MATE_MIXER_TYPE_STREAM,
                                     G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT_ONLY |
                                     G_PARAM_STATIC_STRINGS);

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
                return playing
                        ? "audio-speaker-left-testing"
                        : "audio-speaker-left";
        case MATE_MIXER_CHANNEL_FRONT_RIGHT:
                return playing
                        ? "audio-speaker-right-testing"
                        : "audio-speaker-right";
        case MATE_MIXER_CHANNEL_FRONT_CENTER:
                return playing
                        ? "audio-speaker-center-testing"
                        : "audio-speaker-center";
        case MATE_MIXER_CHANNEL_BACK_LEFT:
                return playing
                        ? "audio-speaker-left-back-testing"
                        : "audio-speaker-left-back";
        case MATE_MIXER_CHANNEL_BACK_RIGHT:
                return playing
                        ? "audio-speaker-right-back-testing"
                        : "audio-speaker-right-back";
        case MATE_MIXER_CHANNEL_BACK_CENTER:
                return playing
                        ? "audio-speaker-center-back-testing"
                        : "audio-speaker-center-back";
        case MATE_MIXER_CHANNEL_LFE:
                return playing
                        ? "audio-subwoofer-testing"
                        : "audio-subwoofer";
        case MATE_MIXER_CHANNEL_SIDE_LEFT:
                return playing
                        ? "audio-speaker-left-side-testing"
                        : "audio-speaker-left-side";
        case MATE_MIXER_CHANNEL_SIDE_RIGHT:
                return playing
                        ? "audio-speaker-right-side-testing"
                        : "audio-speaker-right-side";
        default:
                return NULL;
        }
}

static void
update_button (GtkWidget *control)
{
        GtkWidget *button;
        GtkWidget *image;
        gboolean   playing;
        MateMixerChannelPosition position;

        button = g_object_get_data (G_OBJECT (control), "button");
        image  = g_object_get_data (G_OBJECT (control), "image");

        position = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (control), "position"));
        playing  = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (control), "playing"));

        gtk_button_set_label (GTK_BUTTON (button), playing ? _("Stop") : _("Test"));

        gtk_image_set_from_icon_name (GTK_IMAGE (image),
                                      icon_name (position, playing),
                                      GTK_ICON_SIZE_DIALOG);
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
                const gchar *name;
                ca_proplist *proplist;

                position = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (control), "position"));

                ca_proplist_create (&proplist);
                ca_proplist_sets (proplist,
                                  CA_PROP_MEDIA_ROLE, "test");
                ca_proplist_sets (proplist,
                                  CA_PROP_MEDIA_NAME,
                                  gvc_channel_position_to_pretty_string (position));
                ca_proplist_sets (proplist,
                                  CA_PROP_CANBERRA_FORCE_CHANNEL,
                                  gvc_channel_position_to_pulse_string (position));

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

                g_object_set_data (G_OBJECT (control), "playing", GINT_TO_POINTER (playing));
        }

        update_button (control);
}

static GtkWidget *
create_control (ca_context *canberra, MateMixerChannelPosition position)
{
        GtkWidget   *control;
        GtkWidget   *box;
        GtkWidget   *label;
        GtkWidget   *image;
        GtkWidget   *test_button;
        const gchar *name;

        control = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        box     = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

        g_object_set_data (G_OBJECT (control), "playing", GINT_TO_POINTER (FALSE));
        g_object_set_data (G_OBJECT (control), "position", GINT_TO_POINTER (position));
        g_object_set_data (G_OBJECT (control), "canberra", canberra);

        name = icon_name (position, FALSE);
        if (name == NULL)
                name = "audio-volume-medium";

        image = gtk_image_new_from_icon_name (name, GTK_ICON_SIZE_DIALOG);
        g_object_set_data (G_OBJECT (control), "image", image);
        gtk_box_pack_start (GTK_BOX (control), image, FALSE, FALSE, 0);

        label = gtk_label_new (gvc_channel_position_to_pretty_string (position));
        gtk_box_pack_start (GTK_BOX (control), label, FALSE, FALSE, 0);

        test_button = gtk_button_new_with_label (_("Test"));
        g_signal_connect (G_OBJECT (test_button),
                          "clicked",
                          G_CALLBACK (on_test_button_clicked),
                          control);

        g_object_set_data (G_OBJECT (control), "button", test_button);

        gtk_box_pack_start (GTK_BOX (box), test_button, TRUE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (control), box, FALSE, FALSE, 0);

        gtk_widget_show_all (control);

        return control;
}

static void
create_controls (GvcSpeakerTest *test)
{
        guint i;

        for (i = 0; i < G_N_ELEMENTS (positions); i++) {
                GtkWidget *control = create_control (test->priv->canberra, positions[i].position);

                gtk_grid_attach (GTK_GRID (test),
                                 control,
                                 positions[i].left,
                                 positions[i].top,
                                 1, 1);
                g_array_insert_val (test->priv->controls, i, control);
        }
}

static void
gvc_speaker_test_init (GvcSpeakerTest *test)
{
        GtkWidget *face;

        test->priv = GVC_SPEAKER_TEST_GET_PRIVATE (test);

        gtk_container_set_border_width (GTK_CONTAINER (test), 12);

        face = gtk_image_new_from_icon_name ("face-smile", GTK_ICON_SIZE_DIALOG);

        gtk_grid_attach (GTK_GRID (test),
                         face,
                         1, 1,
                         3, 1);


        gtk_grid_set_baseline_row (GTK_GRID (test), 1);
        gtk_widget_show (face);

        ca_context_create (&test->priv->canberra);

        /* The test sounds are played for a single channel, set up using the
         * FORCE_CHANNEL property of libcanberra; this property is only supported
         * in the PulseAudio backend, so avoid other backends completely */
        ca_context_set_driver (test->priv->canberra, "pulse");

        ca_context_change_props (test->priv->canberra,
                                 CA_PROP_APPLICATION_ID, "org.mate.VolumeControl",
                                 CA_PROP_APPLICATION_NAME, _("Volume Control"),
                                 CA_PROP_APPLICATION_VERSION, VERSION,
                                 CA_PROP_APPLICATION_ICON_NAME, "multimedia-volume-control",
                                 NULL);

        test->priv->controls = g_array_new (FALSE, FALSE, sizeof (GtkWidget *));

        create_controls (test);
}

static void
gvc_speaker_test_dispose (GObject *object)
{
        GvcSpeakerTest *test;

        test = GVC_SPEAKER_TEST (object);

        g_clear_object (&test->priv->stream);

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
gvc_speaker_test_new (MateMixerStream *stream)
{
        GObject *test;

        g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

        test = g_object_new (GVC_TYPE_SPEAKER_TEST,
                             "row-spacing", 6,
                             "column-spacing", 6,
                             "row-homogeneous", TRUE,
                             "column-homogeneous", TRUE,
                             "stream", stream,
                             NULL);

        return GTK_WIDGET (test);
}
