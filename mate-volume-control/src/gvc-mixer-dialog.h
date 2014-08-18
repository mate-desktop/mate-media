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

#ifndef __GVC_MIXER_DIALOG_H
#define __GVC_MIXER_DIALOG_H

#include <glib.h>
#include <glib-object.h>

#include <libmatemixer/matemixer.h>

G_BEGIN_DECLS

#define GVC_DIALOG_DBUS_NAME          "org.mate.VolumeControl"

#define GVC_TYPE_MIXER_DIALOG         (gvc_mixer_dialog_get_type ())
#define GVC_MIXER_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GVC_TYPE_MIXER_DIALOG, GvcMixerDialog))
#define GVC_MIXER_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GVC_TYPE_MIXER_DIALOG, GvcMixerDialogClass))
#define GVC_IS_MIXER_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GVC_TYPE_MIXER_DIALOG))
#define GVC_IS_MIXER_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GVC_TYPE_MIXER_DIALOG))
#define GVC_MIXER_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GVC_TYPE_MIXER_DIALOG, GvcMixerDialogClass))

typedef struct _GvcMixerDialog         GvcMixerDialog;
typedef struct _GvcMixerDialogClass    GvcMixerDialogClass;
typedef struct _GvcMixerDialogPrivate  GvcMixerDialogPrivate;

struct _GvcMixerDialog
{
        GtkDialog              parent;
        GvcMixerDialogPrivate *priv;
};

struct _GvcMixerDialogClass
{
        GtkDialogClass         parent_class;
};

GType               gvc_mixer_dialog_get_type            (void) G_GNUC_CONST;

GvcMixerDialog *    gvc_mixer_dialog_new                 (MateMixerContext *context);

gboolean            gvc_mixer_dialog_set_page            (GvcMixerDialog   *dialog,
                                                          const gchar      *page);

G_END_DECLS

#endif /* __GVC_MIXER_DIALOG_H */
