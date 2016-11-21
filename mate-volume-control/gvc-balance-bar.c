/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann
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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <canberra-gtk.h>
#include <libmatemixer/matemixer.h>

#include "gvc-balance-bar.h"

#define BALANCE_BAR_STYLE                                       \
        "style \"balance-bar-scale-style\" {\n"                 \
        " GtkScale::trough-side-details = 0\n"                  \
        "}\n"                                                   \
        "widget \"*.balance-bar-scale\" style : rc \"balance-bar-scale-style\"\n"

#define SCALE_SIZE 128
#define GVC_BALANCE_BAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GVC_TYPE_BALANCE_BAR, GvcBalanceBarPrivate))

struct _GvcBalanceBarPrivate
{
        GvcBalanceType   btype;
        GtkWidget       *scale_box;
        GtkWidget       *start_box;
        GtkWidget       *end_box;
        GtkWidget       *label;
        GtkWidget       *scale;
        GtkAdjustment   *adjustment;
        GtkSizeGroup    *size_group;
        gboolean         symmetric;
        MateMixerStreamControl *control;
        gint             lfe_channel;
};

enum
{
        PROP_0,
        PROP_CONTROL,
        PROP_BALANCE_TYPE,
        N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void     gvc_balance_bar_class_init  (GvcBalanceBarClass *klass);
static void     gvc_balance_bar_init        (GvcBalanceBar      *balance_bar);
static void     gvc_balance_bar_dispose     (GObject            *object);

static gboolean on_scale_scroll_event       (GtkWidget          *widget,
                                             GdkEventScroll     *event,
                                             GvcBalanceBar      *bar);

static void     on_adjustment_value_changed (GtkAdjustment      *adjustment,
                                             GvcBalanceBar      *bar);

G_DEFINE_TYPE (GvcBalanceBar, gvc_balance_bar, GTK_TYPE_BOX)

static void
create_scale_box (GvcBalanceBar *bar)
{
        bar->priv->scale_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        bar->priv->start_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        bar->priv->end_box   = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        bar->priv->scale     = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL,
                                              bar->priv->adjustment);

        /* Balance and fade scales do not have an origin */
        if (bar->priv->btype != BALANCE_TYPE_LFE)
                gtk_scale_set_has_origin (GTK_SCALE (bar->priv->scale), FALSE);

        gtk_widget_set_size_request (bar->priv->scale, SCALE_SIZE, -1);

        gtk_box_pack_start (GTK_BOX (bar->priv->scale_box),
                            bar->priv->start_box,
                            FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (bar->priv->start_box),
                            bar->priv->label,
                            FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (bar->priv->scale_box),
                            bar->priv->scale,
                            TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (bar->priv->scale_box),
                            bar->priv->end_box,
                            FALSE, FALSE, 0);

        ca_gtk_widget_disable_sounds (bar->priv->scale, FALSE);

        gtk_widget_add_events (bar->priv->scale, GDK_SCROLL_MASK);

        g_signal_connect (G_OBJECT (bar->priv->scale),
                          "scroll-event",
                          G_CALLBACK (on_scale_scroll_event),
                          bar);

        if (bar->priv->size_group != NULL) {
                gtk_size_group_add_widget (bar->priv->size_group,
                                           bar->priv->start_box);

                if (bar->priv->symmetric)
                        gtk_size_group_add_widget (bar->priv->size_group,
                                                   bar->priv->end_box);
        }

        gtk_scale_set_draw_value (GTK_SCALE (bar->priv->scale), FALSE);
}

static void
update_scale_marks (GvcBalanceBar *bar)
{
        gchar    *str_lower = NULL,
                 *str_upper = NULL;
        gdouble   lower,
                  upper;

        gtk_scale_clear_marks (GTK_SCALE (bar->priv->scale));

        switch (bar->priv->btype) {
        case BALANCE_TYPE_RL:
                str_lower = g_strdup_printf ("<small>%s</small>", C_("balance", "Left"));
                str_upper = g_strdup_printf ("<small>%s</small>", C_("balance", "Right"));
                break;
        case BALANCE_TYPE_FR:
                str_lower = g_strdup_printf ("<small>%s</small>", C_("balance", "Rear"));
                str_upper = g_strdup_printf ("<small>%s</small>", C_("balance", "Front"));
                break;
        case BALANCE_TYPE_LFE:
                str_lower = g_strdup_printf ("<small>%s</small>", C_("balance", "Minimum"));
                str_upper = g_strdup_printf ("<small>%s</small>", C_("balance", "Maximum"));
                break;
        }

        lower = gtk_adjustment_get_lower (bar->priv->adjustment);
        gtk_scale_add_mark (GTK_SCALE (bar->priv->scale),
                            lower,
                            GTK_POS_BOTTOM,
                            str_lower);
        upper = gtk_adjustment_get_upper (bar->priv->adjustment);
        gtk_scale_add_mark (GTK_SCALE (bar->priv->scale),
                            upper,
                            GTK_POS_BOTTOM,
                            str_upper);
        g_free (str_lower);
        g_free (str_upper);

        if (bar->priv->btype != BALANCE_TYPE_LFE)
                gtk_scale_add_mark (GTK_SCALE (bar->priv->scale),
                                    (upper - lower) / 2 + lower,
                                    GTK_POS_BOTTOM,
                                    NULL);
}

void
gvc_balance_bar_set_size_group (GvcBalanceBar *bar,
                                GtkSizeGroup  *group,
                                gboolean       symmetric)
{
        g_return_if_fail (GVC_IS_BALANCE_BAR (bar));
        g_return_if_fail (GTK_IS_SIZE_GROUP (group));

        bar->priv->size_group = group;
        bar->priv->symmetric = symmetric;

        if (bar->priv->size_group != NULL) {
                gtk_size_group_add_widget (bar->priv->size_group,
                                           bar->priv->start_box);

                if (bar->priv->symmetric)
                        gtk_size_group_add_widget (bar->priv->size_group,
                                                   bar->priv->end_box);
        }
        gtk_widget_queue_draw (GTK_WIDGET (bar));
}

static void
update_balance_value (GvcBalanceBar *bar)
{
        gdouble value = 0;

        switch (bar->priv->btype) {
        case BALANCE_TYPE_RL:
                value = mate_mixer_stream_control_get_balance (bar->priv->control);
                g_debug ("Balance value changed to %.2f", value);
                break;
        case BALANCE_TYPE_FR:
                value = mate_mixer_stream_control_get_fade (bar->priv->control);
                g_debug ("Fade value changed to %.2f", value);
                break;
        case BALANCE_TYPE_LFE:
                value = mate_mixer_stream_control_get_channel_volume (bar->priv->control,
                                                                      bar->priv->lfe_channel);

                g_debug ("Subwoofer volume changed to %.0f", value);
                break;
        }

        gtk_adjustment_set_value (bar->priv->adjustment, value);
}

static void
on_balance_value_changed (MateMixerStream *stream,
                          GParamSpec      *pspec,
                          GvcBalanceBar   *bar)
{
        update_balance_value (bar);
}

static gint
find_stream_lfe_channel (MateMixerStreamControl *control)
{
        guint i;

        for (i = 0; i < mate_mixer_stream_control_get_num_channels (control); i++) {
                MateMixerChannelPosition position;

                position = mate_mixer_stream_control_get_channel_position (control, i);
                if (position == MATE_MIXER_CHANNEL_LFE)
                        return i;
        }

        return -1;
}

static void
gvc_balance_bar_set_control (GvcBalanceBar *bar, MateMixerStreamControl *control)
{
        g_return_if_fail (GVC_BALANCE_BAR (bar));
        g_return_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control));

        if (bar->priv->control != NULL) {
                g_signal_handlers_disconnect_by_func (G_OBJECT (bar->priv->control),
                                                      on_balance_value_changed,
                                                      bar);
                g_object_unref (bar->priv->control);
        }

        bar->priv->control = g_object_ref (control);

        if (bar->priv->btype == BALANCE_TYPE_LFE) {
                gdouble minimum;
                gdouble maximum;

                minimum = mate_mixer_stream_control_get_min_volume (bar->priv->control);
                maximum = mate_mixer_stream_control_get_normal_volume (bar->priv->control);

                /* Configure the adjustment for the volume limits of the current
                 * stream.
                 * Only subwoofer scale uses volume, balance and fade use fixed
                 * limits which do not need to be updated as balance type is
                 * only set during construction. */
                gtk_adjustment_configure (GTK_ADJUSTMENT (bar->priv->adjustment),
                                          gtk_adjustment_get_value (bar->priv->adjustment),
                                          minimum,
                                          maximum,
                                          (maximum - minimum) / 100.0,
                                          (maximum - minimum) / 10.0,
                                          0.0);

                bar->priv->lfe_channel = find_stream_lfe_channel (bar->priv->control);

                if (G_LIKELY (bar->priv->lfe_channel > -1))
                        g_debug ("Found LFE channel at position %d", bar->priv->lfe_channel);
                else
                        g_warn_if_reached ();
        } else
                bar->priv->lfe_channel = -1;

        switch (bar->priv->btype) {
        case BALANCE_TYPE_RL:
                g_signal_connect (G_OBJECT (bar->priv->control),
                                  "notify::balance",
                                  G_CALLBACK (on_balance_value_changed),
                                  bar);
                break;
        case BALANCE_TYPE_FR:
                g_signal_connect (G_OBJECT (bar->priv->control),
                                  "notify::fade",
                                  G_CALLBACK (on_balance_value_changed),
                                  bar);
                break;
        case BALANCE_TYPE_LFE:
                g_signal_connect (G_OBJECT (bar->priv->control),
                                  "notify::volume",
                                  G_CALLBACK (on_balance_value_changed),
                                  bar);
                break;
        }

        update_balance_value (bar);
        update_scale_marks (bar);

        g_object_notify_by_pspec (G_OBJECT (bar), properties[PROP_CONTROL]);
}

static void
gvc_balance_bar_set_balance_type (GvcBalanceBar *bar, GvcBalanceType btype)
{
        GtkWidget     *frame;
        GtkAdjustment *adjustment;

        /* Create adjustment with limits for balance and fade types because
         * some limits must be provided.
         * If subwoofer type is used instead, the limits will be changed when
         * stream is set. */
        adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, -1.0, 1.0, 0.05, 0.5, 0.0));

        bar->priv->btype = btype;
        bar->priv->adjustment = GTK_ADJUSTMENT (g_object_ref_sink (adjustment));

        g_signal_connect (G_OBJECT (adjustment),
                          "value-changed",
                          G_CALLBACK (on_adjustment_value_changed),
                          bar);

        switch (btype) {
        case BALANCE_TYPE_RL:
                bar->priv->label = gtk_label_new_with_mnemonic (_("_Balance:"));
                break;
        case BALANCE_TYPE_FR:
                bar->priv->label = gtk_label_new_with_mnemonic (_("_Fade:"));
                break;
        case BALANCE_TYPE_LFE:
                bar->priv->label = gtk_label_new_with_mnemonic (_("_Subwoofer:"));
                break;
        }

#if GTK_CHECK_VERSION (3, 16, 0)
        gtk_label_set_xalign (GTK_LABEL (bar->priv->label), 0.0);
        gtk_label_set_yalign (GTK_LABEL (bar->priv->label), 0.0);
#else
        gtk_misc_set_alignment (GTK_MISC (bar->priv->label), 0.0, 0.0);
#endif

        /* Frame */
        frame = gtk_frame_new (NULL);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (bar), frame, TRUE, TRUE, 0);

        /* Box with scale */
        create_scale_box (bar);
        gtk_container_add (GTK_CONTAINER (frame), bar->priv->scale_box);

        gtk_label_set_mnemonic_widget (GTK_LABEL (bar->priv->label),
                                       bar->priv->scale);

        gtk_widget_set_direction (bar->priv->scale, GTK_TEXT_DIR_LTR);
        gtk_widget_show_all (frame);

        g_object_notify_by_pspec (G_OBJECT (bar), properties[PROP_BALANCE_TYPE]);
}

static void
gvc_balance_bar_set_property (GObject       *object,
                              guint          prop_id,
                              const GValue  *value,
                              GParamSpec    *pspec)
{
        GvcBalanceBar *self = GVC_BALANCE_BAR (object);

        switch (prop_id) {
        case PROP_CONTROL:
                gvc_balance_bar_set_control (self, g_value_get_object (value));
                break;
        case PROP_BALANCE_TYPE:
                gvc_balance_bar_set_balance_type (self, g_value_get_int (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_balance_bar_get_property (GObject     *object,
                              guint        prop_id,
                              GValue      *value,
                              GParamSpec  *pspec)
{
        GvcBalanceBar *self = GVC_BALANCE_BAR (object);

        switch (prop_id) {
        case PROP_CONTROL:
                g_value_set_object (value, self->priv->control);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_balance_bar_class_init (GvcBalanceBarClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = gvc_balance_bar_dispose;
        object_class->set_property = gvc_balance_bar_set_property;
        object_class->get_property = gvc_balance_bar_get_property;

        properties[PROP_CONTROL] =
                g_param_spec_object ("control",
                                     "Control",
                                     "MateMixer stream control",
                                     MATE_MIXER_TYPE_STREAM_CONTROL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);

        properties[PROP_BALANCE_TYPE] =
                g_param_spec_int ("balance-type",
                                  "balance type",
                                  "Whether the balance is right-left or front-rear",
                                  BALANCE_TYPE_RL,
                                  NUM_BALANCE_TYPES - 1,
                                  BALANCE_TYPE_RL,
                                  G_PARAM_READWRITE |
                                  G_PARAM_CONSTRUCT_ONLY |
                                  G_PARAM_STATIC_STRINGS);

        g_object_class_install_properties (object_class, N_PROPERTIES, properties);

        g_type_class_add_private (klass, sizeof (GvcBalanceBarPrivate));
}

static gboolean
on_scale_scroll_event (GtkWidget      *widget,
                       GdkEventScroll *event,
                       GvcBalanceBar  *bar)
{
        gdouble value;
        gdouble minimum;
        gdouble maximum;
        gdouble step;

        value   = gtk_adjustment_get_value (bar->priv->adjustment);
        minimum = gtk_adjustment_get_lower (bar->priv->adjustment);
        maximum = gtk_adjustment_get_upper (bar->priv->adjustment);

        // XXX fix this for GTK3

        if (bar->priv->btype == BALANCE_TYPE_LFE)
                step = (maximum - minimum) / 100.0;
        else
                step = 0.05;

        if (event->direction == GDK_SCROLL_UP) {
                if (value + step > maximum)
                        value = maximum;
                else
                        value = value + step;
        } else if (event->direction == GDK_SCROLL_DOWN) {
                if (value - step < minimum)
                        value = minimum;
                else
                        value = value - step;
        }

        gtk_adjustment_set_value (bar->priv->adjustment, value);
        return TRUE;
}

static void
on_adjustment_value_changed (GtkAdjustment *adjustment, GvcBalanceBar *bar)
{
        gdouble value;

        if (bar->priv->control == NULL)
                return;

        value = gtk_adjustment_get_value (adjustment);

        switch (bar->priv->btype) {
        case BALANCE_TYPE_RL:
                mate_mixer_stream_control_set_balance (bar->priv->control, value);
                break;
        case BALANCE_TYPE_FR:
                mate_mixer_stream_control_set_fade (bar->priv->control, value);
                break;
        case BALANCE_TYPE_LFE:
                mate_mixer_stream_control_set_channel_volume (bar->priv->control,
                                                      bar->priv->lfe_channel,
                                                      value);
                break;
        }
}

static void
gvc_balance_bar_init (GvcBalanceBar *bar)
{
        bar->priv = GVC_BALANCE_BAR_GET_PRIVATE (bar);
}

static void
gvc_balance_bar_dispose (GObject *object)
{
        GvcBalanceBar *bar;

        bar = GVC_BALANCE_BAR (object);

        if (bar->priv->control != NULL) {
                g_signal_handlers_disconnect_by_func (G_OBJECT (bar->priv->control),
                                                      on_balance_value_changed,
                                                      bar);
                g_clear_object (&bar->priv->control);
        }

        G_OBJECT_CLASS (gvc_balance_bar_parent_class)->dispose (object);
}

GtkWidget *
gvc_balance_bar_new (MateMixerStreamControl *control, GvcBalanceType btype)
{
        return g_object_new (GVC_TYPE_BALANCE_BAR,
                            "balance-type", btype,
                            "control", control,
                            "orientation", GTK_ORIENTATION_HORIZONTAL,
                            NULL);
}
