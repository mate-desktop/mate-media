/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann <william.jon.mccann@gmail.com>
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

#include <math.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#define MATE_DESKTOP_USE_UNSTABLE_API
#include <libmate-desktop/mate-desktop-utils.h>

#include "gvc-level-bar.h"
#include "gvc-utils.h"

#define GVC_LEVEL_BAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GVC_TYPE_LEVEL_BAR, GvcLevelBarPrivate))

#define NUM_BOXES                  15
#define MIN_HORIZONTAL_BAR_WIDTH   150
#define HORIZONTAL_BAR_HEIGHT      6
#define VERTICAL_BAR_WIDTH         6
#define MIN_VERTICAL_BAR_HEIGHT    400

typedef struct {
        int            peak_num;
        int            max_peak_num;
        GdkRectangle   area;
        int            delta;
        int            box_width;
        int            box_height;
        int            box_radius;
        GdkRGBA        color_bg;
        GdkRGBA        color_fg;
        GdkRGBA        color_dark;
} LevelBarLayout;

struct _GvcLevelBarPrivate
{
        GtkOrientation orientation;
        GtkAdjustment *peak_adjustment;
        GtkAdjustment *rms_adjustment;
        GvcLevelScale  scale;
        gdouble        peak_fraction;
        gdouble        rms_fraction;
        gdouble        max_peak;
        guint          max_peak_id;
        LevelBarLayout layout;
};

enum
{
        PROP_0,
        PROP_PEAK_ADJUSTMENT,
        PROP_RMS_ADJUSTMENT,
        PROP_SCALE,
        PROP_ORIENTATION,
        N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void gvc_level_bar_class_init (GvcLevelBarClass *klass);
static void gvc_level_bar_init       (GvcLevelBar      *bar);
static void gvc_level_bar_finalize   (GObject          *object);

G_DEFINE_TYPE (GvcLevelBar, gvc_level_bar, GTK_TYPE_WIDGET)

static gboolean
layout_changed (LevelBarLayout *layout1, LevelBarLayout *layout2)
{
        if (layout1->area.x != layout2->area.x)
                return TRUE;
        if (layout1->area.y != layout2->area.y)
                return TRUE;
        if (layout1->area.width != layout2->area.width)
                return TRUE;
        if (layout1->area.height != layout2->area.height)
                return TRUE;

        if (layout1->delta != layout2->delta)
                return TRUE;
        if (layout1->peak_num != layout2->peak_num)
                return TRUE;
        if (layout1->max_peak_num != layout2->max_peak_num)
                return TRUE;

        if (!gdk_rgba_equal (&layout1->color_fg, &layout2->color_fg))
                return TRUE;
        if (!gdk_rgba_equal (&layout1->color_bg, &layout2->color_bg))
                return TRUE;
        if (!gdk_rgba_equal (&layout1->color_dark, &layout2->color_dark))
                return TRUE;

        return FALSE;
}

static gdouble
fraction_from_adjustment (GvcLevelBar   *bar,
                          GtkAdjustment *adjustment)
{
        gdouble level;
        gdouble fraction = 0.0;
        gdouble min;
        gdouble max;

        level = gtk_adjustment_get_value (adjustment);
        min   = gtk_adjustment_get_lower (adjustment);
        max   = gtk_adjustment_get_upper (adjustment);

        switch (bar->priv->scale) {
        case GVC_LEVEL_SCALE_LINEAR:
                fraction = (level - min) / (max - min);
                break;
        case GVC_LEVEL_SCALE_LOG:
                fraction = log10 ((level - min + 1) / (max - min + 1));
                break;
        }

        return fraction;
}

static gboolean
reset_max_peak (GvcLevelBar *bar)
{
        bar->priv->max_peak = gtk_adjustment_get_lower (bar->priv->peak_adjustment);

        bar->priv->layout.max_peak_num = 0;

        gtk_widget_queue_draw (GTK_WIDGET (bar));

        bar->priv->max_peak_id = 0;
        return FALSE;
}

static void
bar_calc_layout (GvcLevelBar *bar)
{
        int           peak_level;
        int           max_peak_level;
        GtkAllocation allocation;

        GtkStyleContext *context;

        context = gtk_widget_get_style_context (GTK_WIDGET (bar));

        gtk_style_context_save (context);
        gtk_style_context_set_state (context, GTK_STATE_FLAG_NORMAL);
        gtk_style_context_get_background_color (context,
                                                gtk_style_context_get_state (context),
                                                &bar->priv->layout.color_bg);
        mate_desktop_gtk_style_get_dark_color (context,
                                               gtk_style_context_get_state (context),
                                               &bar->priv->layout.color_dark);

        gtk_style_context_set_state (context, GTK_STATE_FLAG_SELECTED);
        gtk_style_context_get_background_color (context,
                                                gtk_style_context_get_state (context),
                                                &bar->priv->layout.color_fg);
        gtk_style_context_restore (context);

        gtk_widget_get_allocation (GTK_WIDGET (bar), &allocation);

        bar->priv->layout.area.width = allocation.width - 2;
        bar->priv->layout.area.height = allocation.height - 2;

        if (bar->priv->orientation == GTK_ORIENTATION_VERTICAL) {
                peak_level = bar->priv->peak_fraction * bar->priv->layout.area.height;
                max_peak_level = bar->priv->max_peak * bar->priv->layout.area.height;

                bar->priv->layout.delta = bar->priv->layout.area.height / NUM_BOXES;
                bar->priv->layout.area.x = 0;
                bar->priv->layout.area.y = 0;
                bar->priv->layout.box_height = bar->priv->layout.delta / 2;
                bar->priv->layout.box_width  = bar->priv->layout.area.width;
                bar->priv->layout.box_radius = bar->priv->layout.box_width / 2;
        } else {
                peak_level = bar->priv->peak_fraction * bar->priv->layout.area.width;
                max_peak_level = bar->priv->max_peak * bar->priv->layout.area.width;

                bar->priv->layout.delta = bar->priv->layout.area.width / NUM_BOXES;
                bar->priv->layout.area.x = 0;
                bar->priv->layout.area.y = 0;
                bar->priv->layout.box_width  = bar->priv->layout.delta / 2;
                bar->priv->layout.box_height = bar->priv->layout.area.height;
                bar->priv->layout.box_radius = bar->priv->layout.box_height / 2;
        }

        bar->priv->layout.peak_num = peak_level / bar->priv->layout.delta;
        bar->priv->layout.max_peak_num = max_peak_level / bar->priv->layout.delta;
}

static void
update_peak_value (GvcLevelBar *bar)
{
        gdouble        value;
        LevelBarLayout layout;

        value = fraction_from_adjustment (bar, bar->priv->peak_adjustment);

        bar->priv->peak_fraction = value;

        if (value > bar->priv->max_peak) {
                if (bar->priv->max_peak_id > 0)
                        g_source_remove (bar->priv->max_peak_id);

                bar->priv->max_peak_id =
                        g_timeout_add_seconds (1, (GSourceFunc) reset_max_peak, bar);
                bar->priv->max_peak = value;
        }

        layout = bar->priv->layout;

        bar_calc_layout (bar);

        if (layout_changed (&bar->priv->layout, &layout))
                gtk_widget_queue_draw (GTK_WIDGET (bar));
}

static void
update_rms_value (GvcLevelBar *bar)
{
        bar->priv->rms_fraction = fraction_from_adjustment (bar, bar->priv->rms_adjustment);
}

GtkOrientation
gvc_level_bar_get_orientation (GvcLevelBar *bar)
{
        g_return_val_if_fail (GVC_IS_LEVEL_BAR (bar), 0);

        return bar->priv->orientation;
}

void
gvc_level_bar_set_orientation (GvcLevelBar   *bar,
                               GtkOrientation orientation)
{
        g_return_if_fail (GVC_IS_LEVEL_BAR (bar));

        if (orientation != bar->priv->orientation) {
                if (G_UNLIKELY (orientation != GTK_ORIENTATION_VERTICAL &&
                                orientation != GTK_ORIENTATION_HORIZONTAL)) {
                        g_warn_if_reached ();
                        return;
                }

                bar->priv->orientation = orientation;

                gtk_widget_queue_draw (GTK_WIDGET (bar));

                g_object_notify_by_pspec (G_OBJECT (bar), properties[PROP_ORIENTATION]);
        }
}

static void
on_peak_adjustment_value_changed (GtkAdjustment *adjustment,
                                  GvcLevelBar   *bar)
{
        update_peak_value (bar);
}

static void
on_rms_adjustment_value_changed (GtkAdjustment *adjustment,
                                 GvcLevelBar   *bar)
{
        update_rms_value (bar);
}

void
gvc_level_bar_set_peak_adjustment (GvcLevelBar   *bar,
                                   GtkAdjustment *adjustment)
{
        g_return_if_fail (GVC_LEVEL_BAR (bar));
        g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

        if (bar->priv->peak_adjustment != NULL) {
                g_signal_handlers_disconnect_by_func (G_OBJECT (bar->priv->peak_adjustment),
                                                      G_CALLBACK (on_peak_adjustment_value_changed),
                                                      bar);
                g_object_unref (bar->priv->peak_adjustment);
        }

        bar->priv->peak_adjustment = g_object_ref_sink (adjustment);

        g_signal_connect (G_OBJECT (bar->priv->peak_adjustment),
                          "value-changed",
                          G_CALLBACK (on_peak_adjustment_value_changed),
                          bar);

        update_peak_value (bar);

        g_object_notify_by_pspec (G_OBJECT (bar), properties[PROP_PEAK_ADJUSTMENT]);
}

void
gvc_level_bar_set_rms_adjustment (GvcLevelBar   *bar,
                                  GtkAdjustment *adjustment)
{
        g_return_if_fail (GVC_LEVEL_BAR (bar));
        g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

        if (bar->priv->rms_adjustment != NULL) {
                g_signal_handlers_disconnect_by_func (G_OBJECT (bar->priv->rms_adjustment),
                                                      G_CALLBACK (on_rms_adjustment_value_changed),
                                                      bar);
                g_object_unref (bar->priv->rms_adjustment);
        }

        bar->priv->rms_adjustment = g_object_ref_sink (adjustment);

        g_signal_connect (G_OBJECT (bar->priv->rms_adjustment),
                          "value-changed",
                          G_CALLBACK (on_rms_adjustment_value_changed),
                          bar);

        update_rms_value (bar);

        g_object_notify_by_pspec (G_OBJECT (bar), properties[PROP_RMS_ADJUSTMENT]);
}

GtkAdjustment *
gvc_level_bar_get_peak_adjustment (GvcLevelBar *bar)
{
        g_return_val_if_fail (GVC_IS_LEVEL_BAR (bar), NULL);

        return bar->priv->peak_adjustment;
}

GtkAdjustment *
gvc_level_bar_get_rms_adjustment (GvcLevelBar *bar)
{
        g_return_val_if_fail (GVC_IS_LEVEL_BAR (bar), NULL);

        return bar->priv->rms_adjustment;
}

void
gvc_level_bar_set_scale (GvcLevelBar *bar, GvcLevelScale scale)
{
        g_return_if_fail (GVC_IS_LEVEL_BAR (bar));

        if (scale != bar->priv->scale) {
                if (G_UNLIKELY (scale != GVC_LEVEL_SCALE_LINEAR &&
                                scale != GVC_LEVEL_SCALE_LOG)) {
                        g_warn_if_reached ();
                        return;
                }
                bar->priv->scale = scale;

                update_peak_value (bar);
                update_rms_value (bar);

                g_object_notify_by_pspec (G_OBJECT (bar), properties[PROP_SCALE]);
        }
}

static void
gvc_level_bar_set_property (GObject       *object,
                            guint          prop_id,
                            const GValue  *value,
                            GParamSpec    *pspec)
{
        GvcLevelBar *self = GVC_LEVEL_BAR (object);

        switch (prop_id) {
        case PROP_SCALE:
                gvc_level_bar_set_scale (self, g_value_get_int (value));
                break;
        case PROP_ORIENTATION:
                gvc_level_bar_set_orientation (self, g_value_get_enum (value));
                break;
        case PROP_PEAK_ADJUSTMENT:
                gvc_level_bar_set_peak_adjustment (self, g_value_get_object (value));
                break;
        case PROP_RMS_ADJUSTMENT:
                gvc_level_bar_set_rms_adjustment (self, g_value_get_object (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_level_bar_get_property (GObject     *object,
                            guint        prop_id,
                            GValue      *value,
                            GParamSpec  *pspec)
{
        GvcLevelBar *self = GVC_LEVEL_BAR (object);

        switch (prop_id) {
        case PROP_SCALE:
                g_value_set_int (value, self->priv->scale);
                break;
        case PROP_ORIENTATION:
                g_value_set_enum (value, self->priv->orientation);
                break;
        case PROP_PEAK_ADJUSTMENT:
                g_value_set_object (value, self->priv->peak_adjustment);
                break;
        case PROP_RMS_ADJUSTMENT:
                g_value_set_object (value, self->priv->rms_adjustment);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_level_bar_size_request (GtkWidget      *widget,
                            GtkRequisition *requisition)
{
        GvcLevelBar *bar;

        g_return_if_fail (GVC_IS_LEVEL_BAR (widget));
        g_return_if_fail (requisition != NULL);

        bar = GVC_LEVEL_BAR (widget);

        switch (bar->priv->orientation) {
        case GTK_ORIENTATION_VERTICAL:
                requisition->width  = VERTICAL_BAR_WIDTH;
                requisition->height = MIN_VERTICAL_BAR_HEIGHT;
                break;
        case GTK_ORIENTATION_HORIZONTAL:
                requisition->width  = MIN_HORIZONTAL_BAR_WIDTH;
                requisition->height = HORIZONTAL_BAR_HEIGHT;
                break;
        }
}

static void
gvc_level_bar_get_preferred_width (GtkWidget *widget,
                                   gint      *minimum,
                                   gint      *natural)
{
        GtkRequisition requisition;

        gvc_level_bar_size_request (widget, &requisition);

        if (minimum != NULL)
                *minimum = requisition.width;
        if (natural != NULL)
                *natural = requisition.width;
}

static void
gvc_level_bar_get_preferred_height (GtkWidget *widget,
                                    gint      *minimum,
                                    gint      *natural)
{
        GtkRequisition requisition;

        gvc_level_bar_size_request (widget, &requisition);

        if (minimum != NULL)
                *minimum = requisition.height;
        if (natural != NULL)
                *natural = requisition.height;
}

static void
gvc_level_bar_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
        GvcLevelBar *bar;

        bar = GVC_LEVEL_BAR (widget);

        /* FIXME: add height property, labels, etc */
        GTK_WIDGET_CLASS (gvc_level_bar_parent_class)->size_allocate (widget, allocation);

        gtk_widget_set_allocation (widget, allocation);
        gtk_widget_get_allocation (widget, allocation);

        if (bar->priv->orientation == GTK_ORIENTATION_VERTICAL) {
                allocation->height = MIN (allocation->height, MIN_VERTICAL_BAR_HEIGHT);
                allocation->width  = MAX (allocation->width, VERTICAL_BAR_WIDTH);
        } else {
                allocation->width  = MIN (allocation->width, MIN_HORIZONTAL_BAR_WIDTH);
                allocation->height = MAX (allocation->height, HORIZONTAL_BAR_HEIGHT);
        }

        bar_calc_layout (bar);
}

static void
curved_rectangle (cairo_t *cr,
                  double   x0,
                  double   y0,
                  double   width,
                  double   height,
                  double   radius)
{
        double x1;
        double y1;

        x1 = x0 + width;
        y1 = y0 + height;

        if (!width || !height)
                return;

        if (width / 2 < radius) {
                if (height / 2 < radius) {
                        cairo_move_to  (cr, x0, (y0 + y1) / 2);
                        cairo_curve_to (cr, x0 ,y0, x0, y0, (x0 + x1) / 2, y0);
                        cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1) / 2);
                        cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0) / 2, y1);
                        cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1) / 2);
                } else {
                        cairo_move_to  (cr, x0, y0 + radius);
                        cairo_curve_to (cr, x0, y0, x0, y0, (x0 + x1) / 2, y0);
                        cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
                        cairo_line_to  (cr, x1, y1 - radius);
                        cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0) / 2, y1);
                        cairo_curve_to (cr, x0, y1, x0, y1, x0, y1 - radius);
                }
        } else {
                if (height / 2 < radius) {
                        cairo_move_to  (cr, x0, (y0 + y1) / 2);
                        cairo_curve_to (cr, x0, y0, x0 , y0, x0 + radius, y0);
                        cairo_line_to  (cr, x1 - radius, y0);
                        cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1) / 2);
                        cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
                        cairo_line_to  (cr, x0 + radius, y1);
                        cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1) / 2);
                } else {
                        cairo_move_to  (cr, x0, y0 + radius);
                        cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
                        cairo_line_to  (cr, x1 - radius, y0);
                        cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
                        cairo_line_to  (cr, x1, y1 - radius);
                        cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
                        cairo_line_to  (cr, x0 + radius, y1);
                        cairo_curve_to (cr, x0, y1, x0, y1, x0, y1 - radius);
                }
        }

        cairo_close_path (cr);
}

static int
gvc_level_bar_draw (GtkWidget *widget, cairo_t *cr)
{
        GvcLevelBar *bar;

        bar = GVC_LEVEL_BAR (widget);

        cairo_save (cr);

        if (bar->priv->orientation == GTK_ORIENTATION_VERTICAL) {
                int i;
                int by;

                for (i = 0; i < NUM_BOXES; i++) {
                        by = i * bar->priv->layout.delta;
                        curved_rectangle (cr,
                                          bar->priv->layout.area.x + 0.5,
                                          by + 0.5,
                                          bar->priv->layout.box_width - 1,
                                          bar->priv->layout.box_height - 1,
                                          bar->priv->layout.box_radius);
                        if ((bar->priv->layout.max_peak_num - 1) == i) {
                                /* fill peak foreground */
                                gdk_cairo_set_source_rgba (cr, &bar->priv->layout.color_fg);
                                cairo_fill_preserve (cr);
                        } else if ((bar->priv->layout.peak_num - 1) >= i) {
                                /* fill background */
                                gdk_cairo_set_source_rgba (cr, &bar->priv->layout.color_bg);
                                cairo_fill_preserve (cr);

                                /* fill foreground */
                                cairo_set_source_rgba (cr,
                                                       bar->priv->layout.color_fg.red,
                                                       bar->priv->layout.color_fg.green,
                                                       bar->priv->layout.color_fg.blue,
                                                       0.5);
                                cairo_fill_preserve (cr);
                        } else {
                                /* fill background */
                                gdk_cairo_set_source_rgba (cr, &bar->priv->layout.color_bg);
                                cairo_fill_preserve (cr);
                        }

                        /* stroke border */
                        gdk_cairo_set_source_rgba (cr, &bar->priv->layout.color_dark);
                        cairo_set_line_width (cr, 1);
                        cairo_stroke (cr);
                }
        } else {
                int i;
                int bx;

                if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) {
                        GtkAllocation allocation;

                        gtk_widget_get_allocation (widget, &allocation);

                        cairo_scale (cr, -1, 1);
                        cairo_translate (cr, -allocation.width, 0);
                }

                for (i = 0; i < NUM_BOXES; i++) {
                        bx = i * bar->priv->layout.delta;
                        curved_rectangle (cr,
                                          bx + 0.5,
                                          bar->priv->layout.area.y + 0.5,
                                          bar->priv->layout.box_width - 1,
                                          bar->priv->layout.box_height - 1,
                                          bar->priv->layout.box_radius);

                        if ((bar->priv->layout.max_peak_num - 1) == i) {
                                /* fill peak foreground */
                                gdk_cairo_set_source_rgba (cr, &bar->priv->layout.color_fg);
                                cairo_fill_preserve (cr);
                        } else if ((bar->priv->layout.peak_num - 1) >= i) {
                                /* fill background */
                                gdk_cairo_set_source_rgba (cr, &bar->priv->layout.color_bg);
                                cairo_fill_preserve (cr);

                                /* fill foreground */
                                cairo_set_source_rgba (cr,
                                                       bar->priv->layout.color_fg.red,
                                                       bar->priv->layout.color_fg.green,
                                                       bar->priv->layout.color_fg.blue,
                                                       0.5);
                                cairo_fill_preserve (cr);
                        } else {
                                /* fill background */
                                gdk_cairo_set_source_rgba (cr, &bar->priv->layout.color_bg);
                                cairo_fill_preserve (cr);
                        }

                        /* stroke border */
                        gdk_cairo_set_source_rgba (cr, &bar->priv->layout.color_dark);
                        cairo_set_line_width (cr, 1);
                        cairo_stroke (cr);
                }
        }

        cairo_restore (cr);

        return FALSE;
}

static void
gvc_level_bar_class_init (GvcLevelBarClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = gvc_level_bar_finalize;
        object_class->set_property = gvc_level_bar_set_property;
        object_class->get_property = gvc_level_bar_get_property;

        widget_class->draw = gvc_level_bar_draw;
        widget_class->get_preferred_width = gvc_level_bar_get_preferred_width;
        widget_class->get_preferred_height = gvc_level_bar_get_preferred_height;
        widget_class->size_allocate = gvc_level_bar_size_allocate;
#if GTK_CHECK_VERSION (3, 20, 0)
        gtk_widget_class_set_css_name (widget_class, "gvc-level-bar");
#endif

        properties[PROP_ORIENTATION] =
                g_param_spec_enum ("orientation",
                                   "Orientation",
                                   "The orientation of the bar",
                                   GTK_TYPE_ORIENTATION,
                                   GTK_ORIENTATION_HORIZONTAL,
                                   G_PARAM_READWRITE |
                                   G_PARAM_STATIC_STRINGS);

        properties[PROP_PEAK_ADJUSTMENT] =
                g_param_spec_object ("peak-adjustment",
                                     "Peak Adjustment",
                                     "The GtkAdjustment that contains the current peak value",
                                     GTK_TYPE_ADJUSTMENT,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);

        properties[PROP_RMS_ADJUSTMENT] =
                g_param_spec_object ("rms-adjustment",
                                     "RMS Adjustment",
                                     "The GtkAdjustment that contains the current rms value",
                                     GTK_TYPE_ADJUSTMENT,
                                     G_PARAM_READWRITE |
                                     G_PARAM_STATIC_STRINGS);

        properties[PROP_SCALE] =
                g_param_spec_int ("scale",
                                  "Scale",
                                  "Scale",
                                  0,
                                  G_MAXINT,
                                  GVC_LEVEL_SCALE_LINEAR,
                                  G_PARAM_READWRITE |
                                  G_PARAM_CONSTRUCT |
                                  G_PARAM_STATIC_STRINGS);

        g_object_class_install_properties (object_class, N_PROPERTIES, properties);

        g_type_class_add_private (klass, sizeof (GvcLevelBarPrivate));
}

static void
gvc_level_bar_init (GvcLevelBar *bar)
{
        GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (bar));

        gtk_style_context_add_class (context, GTK_STYLE_CLASS_LIST_ROW);

        bar->priv = GVC_LEVEL_BAR_GET_PRIVATE (bar);

        bar->priv->peak_adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0,
                                                                         0.0,
                                                                         1.0,
                                                                         0.05,
                                                                         0.1,
                                                                         0.1));
        g_object_ref_sink (bar->priv->peak_adjustment);

        g_signal_connect (bar->priv->peak_adjustment,
                          "value-changed",
                          G_CALLBACK (on_peak_adjustment_value_changed),
                          bar);

        bar->priv->rms_adjustment = GTK_ADJUSTMENT (gtk_adjustment_new (0.0,
                                                                        0.0,
                                                                        1.0,
                                                                        0.05,
                                                                        0.1,
                                                                        0.1));
        g_object_ref_sink (bar->priv->rms_adjustment);

        g_signal_connect (bar->priv->rms_adjustment,
                          "value-changed",
                          G_CALLBACK (on_rms_adjustment_value_changed),
                          bar);

        gtk_widget_set_has_window (GTK_WIDGET (bar), FALSE);
}

static void
gvc_level_bar_finalize (GObject *object)
{
        GvcLevelBar *bar;

        bar = GVC_LEVEL_BAR (object);

        if (bar->priv->max_peak_id > 0)
                g_source_remove (bar->priv->max_peak_id);

        G_OBJECT_CLASS (gvc_level_bar_parent_class)->finalize (object);
}

GtkWidget *
gvc_level_bar_new (void)
{
        return g_object_new (GVC_TYPE_LEVEL_BAR, NULL);
}
