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

#ifndef __GVC_CHANNEL_BAR_H
#define __GVC_CHANNEL_BAR_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <libmatemixer/matemixer.h>

G_BEGIN_DECLS

#define GVC_TYPE_CHANNEL_BAR         (gvc_channel_bar_get_type ())
#define GVC_CHANNEL_BAR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GVC_TYPE_CHANNEL_BAR, GvcChannelBar))
#define GVC_CHANNEL_BAR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GVC_TYPE_CHANNEL_BAR, GvcChannelBarClass))
#define GVC_IS_CHANNEL_BAR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GVC_TYPE_CHANNEL_BAR))
#define GVC_IS_CHANNEL_BAR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GVC_TYPE_CHANNEL_BAR))
#define GVC_CHANNEL_BAR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GVC_TYPE_CHANNEL_BAR, GvcChannelBarClass))

typedef struct _GvcChannelBar         GvcChannelBar;
typedef struct _GvcChannelBarClass    GvcChannelBarClass;
typedef struct _GvcChannelBarPrivate  GvcChannelBarPrivate;

struct _GvcChannelBar
{
        GtkBox                parent;
        GvcChannelBarPrivate *priv;
};

struct _GvcChannelBarClass
{
        GtkBoxClass           parent_class;

        void (* changed) (GvcChannelBar *bar);
};

GType               gvc_channel_bar_get_type            (void);

GtkWidget *         gvc_channel_bar_new                 (MateMixerStreamControl    *control);

MateMixerStreamControl *gvc_channel_bar_get_control          (GvcChannelBar      *bar);
void                gvc_channel_bar_set_control          (GvcChannelBar      *bar,
                                                         MateMixerStreamControl    *control);

const gchar *       gvc_channel_bar_get_name            (GvcChannelBar      *bar);
void                gvc_channel_bar_set_name            (GvcChannelBar      *bar,
                                                         const gchar        *name);

const gchar *       gvc_channel_bar_get_icon_name       (GvcChannelBar      *bar);
void                gvc_channel_bar_set_icon_name       (GvcChannelBar      *bar,
                                                         const gchar        *icon_name);

const gchar *       gvc_channel_bar_get_low_icon_name   (GvcChannelBar      *bar);
void                gvc_channel_bar_set_low_icon_name   (GvcChannelBar      *bar,
                                                         const gchar        *icon_name);

const gchar *       gvc_channel_bar_get_high_icon_name  (GvcChannelBar      *bar);
void                gvc_channel_bar_set_high_icon_name  (GvcChannelBar      *bar,
                                                         const gchar        *icon_name);

GtkOrientation      gvc_channel_bar_get_orientation     (GvcChannelBar      *bar);
void                gvc_channel_bar_set_orientation     (GvcChannelBar      *bar,
                                                         GtkOrientation      orientation);

gboolean            gvc_channel_bar_get_show_icons      (GvcChannelBar      *bar);
void                gvc_channel_bar_set_show_icons      (GvcChannelBar      *bar,
                                                         gboolean            show_mute);

gboolean            gvc_channel_bar_get_show_mute       (GvcChannelBar      *bar);
void                gvc_channel_bar_set_show_mute       (GvcChannelBar      *bar,
                                                         gboolean            show_mute);

gboolean            gvc_channel_bar_get_show_marks      (GvcChannelBar      *bar);
void                gvc_channel_bar_set_show_marks      (GvcChannelBar      *bar,
                                                         gboolean            show_marks);

gboolean            gvc_channel_bar_get_extended        (GvcChannelBar      *bar);
void                gvc_channel_bar_set_extended        (GvcChannelBar      *bar,
                                                         gboolean            extended);

void                gvc_channel_bar_set_size_group      (GvcChannelBar      *bar,
                                                         GtkSizeGroup       *group,
                                                         gboolean            symmetric);

gboolean            gvc_channel_bar_scroll              (GvcChannelBar      *bar,
                                                         GdkScrollDirection  direction);

G_END_DECLS

#endif /* __GVC_CHANNEL_BAR_H */
