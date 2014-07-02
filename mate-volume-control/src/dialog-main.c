/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Red Hat, Inc.
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

#include <glib/gi18n.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <libintl.h>
#include <unique/uniqueapp.h>
#include <libmatemixer/matemixer.h>

#include "gvc-mixer-dialog.h"

#define DIALOG_POPUP_TIMEOUT 3

static gboolean show_version = FALSE;
static gchar* page = NULL;

static guint popup_id = 0;
static GtkWidget *dialog = NULL;
static GtkWidget *warning_dialog = NULL;

static void
on_dialog_response (GtkDialog *dialog, guint response_id, gpointer data)
{
        gtk_main_quit ();
}

static void
on_dialog_close (GtkDialog *dialog, gpointer data)
{
        gtk_main_quit ();
}

static UniqueResponse
message_received_cb (UniqueApp         *app,
                     int                command,
                     UniqueMessageData *message_data,
                     guint              time_,
                     gpointer           user_data)
{
        gtk_window_present (GTK_WINDOW (user_data));

        return UNIQUE_RESPONSE_OK;
}

static void
on_control_ready (GvcMixerControl *control, UniqueApp *app)
{
	if (popup_id != 0) {
		g_source_remove (popup_id);
		popup_id = 0;
	}
	if (warning_dialog != NULL) {
		gtk_widget_destroy (warning_dialog);
		warning_dialog = NULL;
	}

        if (dialog)
                return;

        dialog = GTK_WIDGET (gvc_mixer_dialog_new (control));
        g_signal_connect (dialog,
                          "response",
                          G_CALLBACK (on_dialog_response),
                          NULL);
        g_signal_connect (dialog,
                          "close",
                          G_CALLBACK (on_dialog_close),
                          NULL);

        gvc_mixer_dialog_set_page (GVC_MIXER_DIALOG (dialog), page);

        g_signal_connect (app, "message-received",
                          G_CALLBACK (message_received_cb), dialog);

        gtk_widget_show (dialog);
}

static void
warning_dialog_answered (GtkDialog *d,
			 gpointer data)
{
	gtk_widget_destroy (warning_dialog);
	gtk_main_quit ();
}

static gboolean
dialog_popup_timeout (gpointer data)
{
	warning_dialog = gtk_message_dialog_new (GTK_WINDOW(dialog),
	                                         0,
	                                         GTK_MESSAGE_INFO,
	                                         GTK_BUTTONS_CANCEL,
	                                         _("Waiting for sound system to respond"));

	g_signal_connect (warning_dialog, "response",
			  G_CALLBACK (warning_dialog_answered), NULL);
	g_signal_connect (warning_dialog, "close",
			  G_CALLBACK (warning_dialog_answered), NULL);

	gtk_widget_show (warning_dialog);

	return FALSE;
}

static void
on_control_connecting (GvcMixerControl *control,
                       UniqueApp       *app)
{
        if (popup_id != 0)
                return;

        popup_id = g_timeout_add_seconds (DIALOG_POPUP_TIMEOUT,
                                          dialog_popup_timeout,
                                          NULL);
}

int
main (int argc, char **argv)
{
        GError          *error = NULL;
        GvcMixerControl *control;
        UniqueApp       *app;
        GOptionEntry     entries[] = {
                { "page",    'p', 0, G_OPTION_ARG_STRING, &page, N_("Startup page"), "effects|hardware|input|output|applications" },
                { "version", 'v', 0, G_OPTION_ARG_NONE,   &show_version, N_("Version of this application"), NULL },
                { NULL }
        };

        bindtextdomain (GETTEXT_PACKAGE, LOCALE_DIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

        gtk_init_with_args (&argc, &argv,
                            (char *) _(" — MATE Volume Control"),
                            entries, GETTEXT_PACKAGE,
                            &error);
        if (error != NULL) {
                g_warning ("%s", error->message);
                return 1;
        }
        if (show_version) {
                g_print ("%s %s\n", argv[0], VERSION);
                return 0;
        }

        app = unique_app_new (GVC_DIALOG_DBUS_NAME, NULL);

        if (unique_app_is_running (app)) {
                unique_app_send_message (app, UNIQUE_ACTIVATE, NULL);
                return 0;
        }
        if (!mate_mixer_init ()) {
                g_warning ("libmatemixer initialization failed, exiting");
                return 1;
        }

        gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                           ICON_DATA_DIR);

        gtk_window_set_default_icon_name ("multimedia-volume-control");

        control = gvc_mixer_control_new ("MATE Volume Control Dialog");

        g_signal_connect (control,
                          "connecting",
                          G_CALLBACK (on_control_connecting),
                          app);
        g_signal_connect (control,
                          "ready",
                          G_CALLBACK (on_control_ready),
                          app);

        gvc_mixer_control_open (control);

        gtk_main ();

        g_object_unref (control);

        return 0;
}
