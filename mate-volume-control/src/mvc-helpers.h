/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
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

#ifndef __MVC_HELPERS_H
#define __MVC_HELPERS_H

#include <glib.h>

#include <libmatemixer/matemixer.h>

G_BEGIN_DECLS

const gchar *mvc_channel_position_to_string        (MateMixerChannelPosition position);
const gchar *mvc_channel_position_to_pretty_string (MateMixerChannelPosition position);
const gchar *mvc_channel_map_to_pretty_string      (MateMixerStream         *stream);

#if GTK_CHECK_VERSION (3, 0, 0)
void         mvc_color_shade                       (GdkRGBA                 *a,
                                                    GdkRGBA                 *b,
                                                    gdouble                  k);
#endif

G_END_DECLS

#endif /* __MVC_HELPERS_H */
