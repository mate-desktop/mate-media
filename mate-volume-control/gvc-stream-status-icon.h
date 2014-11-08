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

#ifndef __GVC_STREAM_STATUS_ICON_H
#define __GVC_STREAM_STATUS_ICON_H

#include <glib.h>
#include <glib-object.h>
#include <libmatemixer/matemixer.h>

G_BEGIN_DECLS

#define GVC_TYPE_STREAM_STATUS_ICON         (gvc_stream_status_icon_get_type ())
#define GVC_STREAM_STATUS_ICON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GVC_TYPE_STREAM_STATUS_ICON, GvcStreamStatusIcon))
#define GVC_STREAM_STATUS_ICON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GVC_TYPE_STREAM_STATUS_ICON, GvcStreamStatusIconClass))
#define GVC_IS_STREAM_STATUS_ICON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GVC_TYPE_STREAM_STATUS_ICON))
#define GVC_IS_STREAM_STATUS_ICON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GVC_TYPE_STREAM_STATUS_ICON))
#define GVC_STREAM_STATUS_ICON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GVC_TYPE_STREAM_STATUS_ICON, GvcStreamStatusIconClass))

typedef struct _GvcStreamStatusIcon         GvcStreamStatusIcon;
typedef struct _GvcStreamStatusIconClass    GvcStreamStatusIconClass;
typedef struct _GvcStreamStatusIconPrivate  GvcStreamStatusIconPrivate;

struct _GvcStreamStatusIcon
{
        GtkStatusIcon               parent;
        GvcStreamStatusIconPrivate *priv;
};

struct _GvcStreamStatusIconClass
{
        GtkStatusIconClass          parent_class;
};

GType                 gvc_stream_status_icon_get_type         (void) G_GNUC_CONST;

GvcStreamStatusIcon * gvc_stream_status_icon_new              (MateMixerStreamControl *control,
                                                               const gchar           **icon_names);

void                  gvc_stream_status_icon_set_icon_names   (GvcStreamStatusIcon    *icon,
                                                               const gchar           **icon_names);
void                  gvc_stream_status_icon_set_display_name (GvcStreamStatusIcon    *icon,
                                                                  const gchar         *display_name);

void                  gvc_stream_status_icon_set_control      (GvcStreamStatusIcon    *icon,
                                                               MateMixerStreamControl *control);

G_END_DECLS

#endif /* __GVC_STREAM_STATUS_ICON_H */
