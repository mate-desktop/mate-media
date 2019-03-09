/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
 * Copyright (C) 2019 Victor Kareh <vkareh@vkareh.net>
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

#ifndef __GVC_STREAM_APPLET_ICON_H
#define __GVC_STREAM_APPLET_ICON_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

G_BEGIN_DECLS

#define GVC_TYPE_STREAM_APPLET_ICON         (gvc_stream_applet_icon_get_type ())
#define GVC_STREAM_APPLET_ICON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GVC_TYPE_STREAM_APPLET_ICON, GvcStreamAppletIcon))
#define GVC_STREAM_APPLET_ICON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GVC_TYPE_STREAM_APPLET_ICON, GvcStreamAppletIconClass))
#define GVC_IS_STREAM_APPLET_ICON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GVC_TYPE_STREAM_APPLET_ICON))
#define GVC_IS_STREAM_APPLET_ICON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GVC_TYPE_STREAM_APPLET_ICON))
#define GVC_STREAM_APPLET_ICON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GVC_TYPE_STREAM_APPLET_ICON, GvcStreamAppletIconClass))

typedef struct _GvcStreamAppletIcon         GvcStreamAppletIcon;
typedef struct _GvcStreamAppletIconClass    GvcStreamAppletIconClass;
typedef struct _GvcStreamAppletIconPrivate  GvcStreamAppletIconPrivate;

struct _GvcStreamAppletIcon
{
        GtkEventBox                 parent;
        GvcStreamAppletIconPrivate *priv;
};

struct _GvcStreamAppletIconClass
{
        GtkEventBoxClass            parent_class;
};

GType                 gvc_stream_applet_icon_get_type         (void) G_GNUC_CONST;

GvcStreamAppletIcon * gvc_stream_applet_icon_new              (MateMixerStreamControl *control,
                                                               const gchar           **icon_names);

void                  gvc_stream_applet_icon_set_icon_names   (GvcStreamAppletIcon    *icon,
                                                               const gchar           **icon_names);
void                  gvc_stream_applet_icon_set_display_name (GvcStreamAppletIcon    *icon,
                                                                  const gchar         *display_name);

void                  gvc_stream_applet_icon_set_control      (GvcStreamAppletIcon    *icon,
                                                               MateMixerStreamControl *control);

void                  gvc_stream_applet_icon_set_size         (GvcStreamAppletIcon *icon,
                                                               guint                size);

void                  gvc_stream_applet_icon_set_orient       (GvcStreamAppletIcon  *icon,
                                                               MatePanelAppletOrient orient);

gboolean              gvc_stream_applet_icon_get_mute         (GvcStreamAppletIcon *icon);

void                  gvc_stream_applet_icon_set_mute         (GvcStreamAppletIcon *icon,
                                                               gboolean mute);

void                  gvc_stream_applet_icon_volume_control   (GvcStreamAppletIcon *icon);

G_END_DECLS

#endif /* __GVC_STREAM_APPLET_ICON_H */
