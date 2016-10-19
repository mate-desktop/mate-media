/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
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
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <libintl.h>
#include <gio/gio.h>
#include <libmatemixer/matemixer.h>

#include "gvc-applet.h"

static gboolean show_version = FALSE;
static gboolean debug = FALSE;

int
main (int argc, char **argv)
{
        GError       *error = NULL;
        GvcApplet    *applet;
        GApplication *app = NULL;
        GOptionEntry  entries[] = {
                { "version", 'v', 0, G_OPTION_ARG_NONE, &show_version, N_("Version of this application"), NULL },
                { "debug", 'd', 0, G_OPTION_ARG_NONE, &debug, N_("Enable debug"), NULL },
                { NULL }
        };

        bindtextdomain (GETTEXT_PACKAGE, LOCALE_DIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

        gtk_init_with_args (&argc, &argv,
                            _(" — MATE Volume Control Applet"),
                            entries, GETTEXT_PACKAGE,
                            &error);

        if (error != NULL) {
                g_warning ("%s", error->message);
                g_error_free (error);
                return 1;
        }
        if (show_version == TRUE) {
                g_print ("%s %s\n", argv[0], VERSION);
                return 0;
        }
        if (debug == TRUE) {
                g_setenv ("G_MESSAGES_DEBUG", "all", FALSE);
        }

        app = g_application_new (GVC_APPLET_DBUS_NAME, G_APPLICATION_FLAGS_NONE);

        if (!g_application_register (app, NULL, &error)) {
                g_warning ("%s", error->message);
                g_error_free (error);
                return 1;
        }
        if (g_application_get_is_remote (app)) {
                g_warning ("Applet is already running, exiting");
                return 0;
        }
        if (mate_mixer_init () == FALSE) {
                g_warning ("libmatemixer initialization failed, exiting");
                return 1;
        }

        gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                           ICON_DATA_DIR);

        applet = gvc_applet_new ();

        gvc_applet_start (applet);
        gtk_main ();

        g_object_unref (applet);
        g_object_unref (app);

        return 0;
}
