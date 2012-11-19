/* MATE Volume Control
 * Copyright (C) 2012 Stefano Karapetsas <stefano@karapetsas.com>
 *
 * schemas.h: GSettings schemas and keys
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GVC_SCHEMAS_H__
#define __GVC_SCHEMAS_H__

G_BEGIN_DECLS

#define MATE_VOLUME_CONTROL_SCHEMA "org.mate.volume-control"
#define MATE_VOLUME_CONTROL_KEY_ACTIVE_ELEMENT "active-element"
#define MATE_VOLUME_CONTROL_KEY_SHOWN_ELEMENTS "shown-elements"
#define MATE_VOLUME_CONTROL_KEY_HIDDEN_ELEMENTS "hidden-elements"
#define MATE_VOLUME_CONTROL_KEY_WINDOW_WIDTH "window-width"
#define MATE_VOLUME_CONTROL_KEY_WINDOW_HEIGHT "window-height"

gboolean schemas_is_str_in_strv (GSettings *settings, const gchar *key, const gchar *value);
gboolean schemas_gsettings_append_strv (GSettings *settings, const gchar *key, const gchar *value);
gboolean schemas_gsettings_remove_all_from_strv (GSettings *settings, const gchar *key, const gchar *value);

G_END_DECLS

#endif /* __GVC_SCHEMAS_H__ */
