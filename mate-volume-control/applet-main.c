/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
 * Copyright (C) 2019 Victor Kareh <vkareh@vkareh.net>
 * Copyright (C) 2014-2021 MATE Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <libintl.h>
#include <gio/gio.h>
#include <libmatemixer/matemixer.h>
#include <mate-panel-applet.h>

#include "gvc-applet.h"

static gboolean
applet_main (MatePanelApplet* applet_widget)
{
        GError       *error = NULL;
        GvcApplet    *applet;
        GApplication *app = NULL;

        app = g_application_new (GVC_APPLET_DBUS_NAME, G_APPLICATION_FLAGS_NONE);

        if (!g_application_register (app, NULL, &error)) {
                g_warning ("%s", error->message);
                g_error_free (error);
                return FALSE;
        }
        if (g_application_get_is_remote (app)) {
                g_warning ("Applet is already running, exiting");
                return TRUE;
        }
        if (mate_mixer_init () == FALSE) {
                g_warning ("libmatemixer initialization failed, exiting");
                return FALSE;
        }

        gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                           ICON_DATA_DIR);

        applet = gvc_applet_new ();

        gvc_applet_fill (applet, applet_widget);
        gvc_applet_start (applet);

        g_object_unref (app);

        return TRUE;
}

/* this function, called by mate-panel, will create the applet */
static gboolean
applet_factory (MatePanelApplet* applet, const char* iid, gpointer data)
{
        gboolean retval = FALSE;

        if (!g_strcmp0 (iid, "GvcApplet"))
                retval = applet_main (applet);

        return retval;
}

#ifdef IN_PROCESS
/* needed by mate-panel applet library */
MATE_PANEL_APPLET_IN_PROCESS_FACTORY("GvcAppletFactory",
                                      PANEL_TYPE_APPLET,
                                      "Volume Control applet",
                                      applet_factory,
                                      NULL)
#else
MATE_PANEL_APPLET_OUT_PROCESS_FACTORY("GvcAppletFactory",
                                      PANEL_TYPE_APPLET,
                                      "Volume Control applet",
                                      applet_factory,
                                      NULL)
#endif
