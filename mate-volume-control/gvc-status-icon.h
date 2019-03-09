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

#ifndef __GVC_STATUS_ICON_H
#define __GVC_STATUS_ICON_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GVC_STATUS_ICON_DBUS_NAME    "org.mate.VolumeControlStatusIcon"

#define GVC_TYPE_STATUS_ICON         (gvc_status_icon_get_type ())
#define GVC_STATUS_ICON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GVC_TYPE_STATUS_ICON, GvcStatusIcon))
#define GVC_STATUS_ICON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GVC_TYPE_STATUS_ICON, GvcStatusIconClass))
#define GVC_IS_STATUS_ICON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GVC_TYPE_STATUS_ICON))
#define GVC_IS_STATUS_ICON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GVC_TYPE_STATUS_ICON))
#define GVC_STATUS_ICON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GVC_TYPE_STATUS_ICON, GvcStatusIconClass))

typedef struct _GvcStatusIcon         GvcStatusIcon;
typedef struct _GvcStatusIconClass    GvcStatusIconClass;
typedef struct _GvcStatusIconPrivate  GvcStatusIconPrivate;

struct _GvcStatusIcon
{
        GObject               parent;
        GvcStatusIconPrivate *priv;
};

struct _GvcStatusIconClass
{
        GObjectClass          parent_class;
};

GType               gvc_status_icon_get_type            (void) G_GNUC_CONST;

GvcStatusIcon *     gvc_status_icon_new                 (void);
void                gvc_status_icon_start               (GvcStatusIcon *status_icon);

G_END_DECLS

#endif /* __GVC_STATUS_ICON_H */
