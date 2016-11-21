/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Red Hat, Inc.
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

#ifndef __GVC_SPEAKER_TEST_H
#define __GVC_SPEAKER_TEST_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <libmatemixer/matemixer.h>

G_BEGIN_DECLS

#define GVC_TYPE_SPEAKER_TEST         (gvc_speaker_test_get_type ())
#define GVC_SPEAKER_TEST(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GVC_TYPE_SPEAKER_TEST, GvcSpeakerTest))
#define GVC_SPEAKER_TEST_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GVC_TYPE_SPEAKER_TEST, GvcSpeakerTestClass))
#define GVC_IS_SPEAKER_TEST(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GVC_TYPE_SPEAKER_TEST))
#define GVC_IS_SPEAKER_TEST_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GVC_TYPE_SPEAKER_TEST))
#define GVC_SPEAKER_TEST_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GVC_TYPE_SPEAKER_TEST, GvcSpeakerTestClass))

typedef struct _GvcSpeakerTest         GvcSpeakerTest;
typedef struct _GvcSpeakerTestClass    GvcSpeakerTestClass;
typedef struct _GvcSpeakerTestPrivate  GvcSpeakerTestPrivate;

struct _GvcSpeakerTest
{
        GtkGrid                   parent;
        GvcSpeakerTestPrivate    *priv;
};

struct _GvcSpeakerTestClass
{
        GtkGridClass              parent_class;
};

GType               gvc_speaker_test_get_type            (void) G_GNUC_CONST;

GtkWidget *         gvc_speaker_test_new                 (MateMixerStream *stream);

MateMixerStream *   gvc_speaker_test_get_stream          (GvcSpeakerTest  *test);

G_END_DECLS

#endif /* __GVC_SPEAKER_TEST_H */
