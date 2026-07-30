/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
 * Copyright (C) 2014-2021 MATE Developers
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <libmatemixer/matemixer.h>

#define MATE_DESKTOP_USE_UNSTABLE_API
#include <libmate-desktop/mate-desktop-utils.h>
#include <libmate-desktop/mate-image-menu-item.h>

#include "gvc-channel-bar.h"
#include "gvc-mpris-manager.h"
#include "gvc-mpris-player.h"
#include "gvc-player-widget.h"
#include "gvc-stream-status-icon.h"

struct _GvcStreamStatusIconPrivate
{
        GSettings              *sound_settings;
        GSettings              *applet_settings;
        gchar                 **icon_names;
        GtkWidget              *dock;
        GtkWidget              *dock_box;
        GtkWidget              *player_widget;
        GtkWidget              *player_separator;
        GtkWidget              *volume_box;
        GtkImage               *volume_image;
        GtkWidget              *bar;
        GvcMprisPlayer         *player;
        gboolean                player_widget_visible;
        guint                   current_icon;
        gchar                  *display_name;
        MateMixerStreamControl *control;
        GvcMprisManager        *mpris_manager;
        MateMixerContext       *mixer_context;
};

enum
{
        PLAYER_SELECTED,
        N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

enum
{
        PROP_0,
        PROP_CONTROL,
        PROP_DISPLAY_NAME,
        PROP_ICON_NAMES,
        N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void gvc_stream_status_icon_finalize (GObject *object);
static void update_icon                     (GvcStreamStatusIcon *icon);

G_DEFINE_TYPE_WITH_PRIVATE (GvcStreamStatusIcon, gvc_stream_status_icon, GTK_TYPE_STATUS_ICON)

static void
configure_volume_box_layout (GvcStreamStatusIcon *icon,
                             GtkOrientation       orientation)
{
        if (icon->priv->volume_box == NULL ||
            icon->priv->volume_image == NULL)
                return;

        gtk_orientable_set_orientation (GTK_ORIENTABLE (icon->priv->volume_box),
                                        orientation);

        if (orientation == GTK_ORIENTATION_HORIZONTAL) {
                gtk_box_reorder_child (GTK_BOX (icon->priv->volume_box),
                                       GTK_WIDGET (icon->priv->volume_image),
                                       0);
                gtk_box_reorder_child (GTK_BOX (icon->priv->volume_box),
                                       icon->priv->bar,
                                       1);
        } else {
                gtk_box_reorder_child (GTK_BOX (icon->priv->volume_box),
                                       icon->priv->bar,
                                       0);
                gtk_box_reorder_child (GTK_BOX (icon->priv->volume_box),
                                       GTK_WIDGET (icon->priv->volume_image),
                                       1);
        }
}

static void
configure_dock_layout (GvcStreamStatusIcon *icon,
                       GtkOrientation       tray_orientation,
                       gboolean             volume_first)
{
        gboolean vertical_tray;

        vertical_tray = tray_orientation == GTK_ORIENTATION_VERTICAL;

        if (icon->priv->player_widget_visible && vertical_tray) {
                gtk_orientable_set_orientation (GTK_ORIENTABLE (icon->priv->dock_box),
                                                GTK_ORIENTATION_HORIZONTAL);
                gvc_channel_bar_set_orientation (GVC_CHANNEL_BAR (icon->priv->bar),
                                                 GTK_ORIENTATION_VERTICAL);
                configure_volume_box_layout (icon, GTK_ORIENTATION_VERTICAL);

                if (icon->priv->player_separator != NULL)
                        gtk_orientable_set_orientation (GTK_ORIENTABLE (icon->priv->player_separator),
                                                        GTK_ORIENTATION_VERTICAL);

                if (volume_first) {
                        gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box), icon->priv->volume_box, 0);
                        if (icon->priv->player_separator != NULL)
                                gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box),
                                                       icon->priv->player_separator,
                                                       1);
                        if (icon->priv->player_widget != NULL)
                                gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box),
                                                       icon->priv->player_widget,
                                                       2);
                } else {
                        if (icon->priv->player_widget != NULL)
                                gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box),
                                                       icon->priv->player_widget,
                                                       0);
                        if (icon->priv->player_separator != NULL)
                                gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box),
                                                       icon->priv->player_separator,
                                                       1);
                        gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box), icon->priv->volume_box, 2);
                }

                gtk_widget_set_size_request (icon->priv->dock, -1, -1);
                return;
        }

        gtk_orientable_set_orientation (GTK_ORIENTABLE (icon->priv->dock_box),
                                        GTK_ORIENTATION_VERTICAL);

        if (icon->priv->player_separator != NULL)
                gtk_orientable_set_orientation (GTK_ORIENTABLE (icon->priv->player_separator),
                                                GTK_ORIENTATION_HORIZONTAL);

        if (icon->priv->player_widget_visible && volume_first) {
                gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box), icon->priv->volume_box, 0);
                if (icon->priv->player_separator != NULL)
                        gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box),
                                               icon->priv->player_separator,
                                               1);
                if (icon->priv->player_widget != NULL)
                        gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box),
                                               icon->priv->player_widget,
                                               2);
        } else {
                if (icon->priv->player_widget != NULL)
                        gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box),
                                               icon->priv->player_widget,
                                               0);
                if (icon->priv->player_separator != NULL)
                        gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box),
                                               icon->priv->player_separator,
                                               1);
                gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box), icon->priv->volume_box, 2);
        }

        if (icon->priv->player_widget_visible) {
                gvc_channel_bar_set_orientation (GVC_CHANNEL_BAR (icon->priv->bar),
                                                 GTK_ORIENTATION_HORIZONTAL);
                configure_volume_box_layout (icon, GTK_ORIENTATION_HORIZONTAL);
                gtk_widget_set_size_request (icon->priv->dock, 336, -1);
                return;
        }

        gtk_widget_set_size_request (icon->priv->dock, -1, -1);
        gvc_channel_bar_set_orientation (GVC_CHANNEL_BAR (icon->priv->bar),
                                         vertical_tray ? GTK_ORIENTATION_HORIZONTAL
                                                       : GTK_ORIENTATION_VERTICAL);
        configure_volume_box_layout (icon,
                                     vertical_tray ? GTK_ORIENTATION_HORIZONTAL
                                                   : GTK_ORIENTATION_VERTICAL);
}

static gboolean
popup_dock (GvcStreamStatusIcon *icon, guint time)
{
        GdkRectangle   area;
        GtkOrientation orientation;
        GdkDisplay    *display;
        GdkScreen     *screen;
        int            x;
        int            y;
        GdkMonitor    *monitor_num;
        GdkRectangle   monitor;
        GtkRequisition dock_req;

        screen = gtk_status_icon_get_screen (GTK_STATUS_ICON (icon));

        if (gtk_status_icon_get_geometry (GTK_STATUS_ICON (icon),
                                          &screen,
                                          &area,
                                          &orientation) == FALSE) {
                /* Some platforms (notably Wayland / StatusNotifierItem hosts)
                 * don't expose tray geometry; position the dock near the
                 * pointer as a best-effort fallback. */
                GdkDevice *pointer;
                GdkSeat   *seat;

                g_warning ("Unable to determine geometry of status icon");

                screen = gdk_screen_get_default ();
                seat = gdk_display_get_default_seat (gdk_screen_get_display (screen));
                pointer = gdk_seat_get_pointer (seat);
                gdk_device_get_position (pointer, NULL, &area.x, &area.y);
                area.width = 1;
                area.height = 1;
                orientation = GTK_ORIENTATION_HORIZONTAL;
        }

        gtk_window_set_screen (GTK_WINDOW (icon->priv->dock), screen);

        monitor_num = gdk_display_get_monitor_at_point (gdk_screen_get_display (screen), area.x, area.y);
        gdk_monitor_get_geometry (monitor_num, &monitor);
        configure_dock_layout (icon, orientation, FALSE);

        gtk_container_foreach (GTK_CONTAINER (icon->priv->dock),
                               (GtkCallback) gtk_widget_show_all, NULL);
        gtk_widget_get_preferred_size (icon->priv->dock, &dock_req, NULL);

        if (orientation == GTK_ORIENTATION_VERTICAL) {
                if (area.x + area.width + dock_req.width <= monitor.x + monitor.width)
                        x = area.x + area.width;
                else
                        x = area.x - dock_req.width;

                if (area.y + dock_req.height <= monitor.y + monitor.height)
                        y = area.y;
                else
                        y = monitor.y + monitor.height - dock_req.height;
        } else {
                if (area.y + area.height + dock_req.height <= monitor.y + monitor.height)
                        y = area.y + area.height;
                else
                        y = area.y - dock_req.height;

                if (area.x + dock_req.width <= monitor.x + monitor.width)
                        x = area.x;
                else
                        x = monitor.x + monitor.width - dock_req.width;
        }

        if (orientation == GTK_ORIENTATION_VERTICAL)
                configure_dock_layout (icon, orientation, x > area.x);
        else
                configure_dock_layout (icon, orientation, y > area.y);

        gtk_window_move (GTK_WINDOW (icon->priv->dock), x, y);

        /* Without this, the popup window appears as a square after changing
         * the orientation */
        gtk_window_resize (GTK_WINDOW (icon->priv->dock), 1, 1);

        gtk_widget_show_all (icon->priv->dock);

        /* Grab focus */
        gtk_grab_add (icon->priv->dock);

        display = gtk_widget_get_display (icon->priv->dock);

        do {
                GdkSeat *seat = gdk_display_get_default_seat (display);
                GdkWindow *window = gtk_widget_get_window (icon->priv->dock);

                if (gdk_seat_grab (seat,
                                   window,
                                   GDK_SEAT_CAPABILITY_ALL,
                                   TRUE,
                                   NULL,
                                   NULL,
                                   NULL,
                                   NULL) != GDK_GRAB_SUCCESS) {
                        gtk_grab_remove (icon->priv->dock);
                        gtk_widget_hide (icon->priv->dock);
                        break;
                }
        } while (0);

        gtk_widget_grab_focus (icon->priv->dock);

        return TRUE;
}

static void
on_status_icon_activate (GtkStatusIcon *status_icon, GvcStreamStatusIcon *icon)
{
        popup_dock (icon, GDK_CURRENT_TIME);
}

static gboolean
on_status_icon_button_press (GtkStatusIcon       *status_icon,
                             GdkEventButton      *event,
                             GvcStreamStatusIcon *icon)
{
        if (event->button == 8 && icon->priv->player != NULL) {
                gvc_mpris_player_previous (icon->priv->player);
                return TRUE;
        }

        if (event->button == 9 && icon->priv->player != NULL) {
                gvc_mpris_player_next (icon->priv->player);
                return TRUE;
        }

        if (event->button == 2) {
                gchar *action = NULL;

                if (icon->priv->applet_settings != NULL)
                        action = g_settings_get_string (icon->priv->applet_settings, "middle-click-action");

                if (g_strcmp0 (action, "player") == 0) {
                        if (icon->priv->player != NULL) {
                                gvc_mpris_player_play_pause (icon->priv->player);
                                g_free (action);
                                return TRUE;
                        }
                } else if (action != NULL &&
                           g_strcmp0 (action, "mute-output") != 0 &&
                           g_strcmp0 (action, "mute-all") != 0) {
                        g_free (action);
                        return FALSE;
                }

                g_free (action);

                if (icon->priv->control == NULL)
                        return FALSE;

                mate_mixer_stream_control_set_mute (icon->priv->control,
                                                    !mate_mixer_stream_control_get_mute (icon->priv->control));
                return TRUE;
        }
        return FALSE;
}

static void
on_menu_mute_toggled (GtkMenuItem *item, GvcStreamStatusIcon *icon)
{
        mate_mixer_stream_control_set_mute (icon->priv->control, !mate_mixer_stream_control_get_mute (icon->priv->control));
}

static void
on_menu_activate_open_volume_control (GtkMenuItem         *item,
                                      GvcStreamStatusIcon *icon)
{
        GError *error = NULL;

        mate_gdk_spawn_command_line_on_screen (gtk_widget_get_screen (icon->priv->dock),
                                               "mate-volume-control",
                                               &error);

        if (error != NULL) {
                GtkWidget *dialog;

                dialog = gtk_message_dialog_new (NULL,
                                                 0,
                                                 GTK_MESSAGE_ERROR,
                                                 GTK_BUTTONS_CLOSE,
                                                 _("Failed to start Sound Preferences: %s"),
                                                 error->message);
                g_signal_connect (G_OBJECT (dialog),
                                  "response",
                                  G_CALLBACK (gtk_widget_destroy),
                                  NULL);
                gtk_widget_show (dialog);
                g_error_free (error);
        }
}

static void
on_menu_show_player_toggled (GtkCheckMenuItem    *item,
                             GvcStreamStatusIcon *icon)
{
        if (icon->priv->applet_settings == NULL)
                return;

        g_settings_set_boolean (icon->priv->applet_settings,
                                "show-player-controls",
                                gtk_check_menu_item_get_active (item));
}

static void
on_player_menu_item_toggled (GtkCheckMenuItem    *item,
                             GvcStreamStatusIcon *icon)
{
        GvcMprisPlayer *player;

        if (!gtk_check_menu_item_get_active (item))
                return;

        player = g_object_get_data (G_OBJECT (item), "gvc-player");
        g_signal_emit (icon, signals[PLAYER_SELECTED], 0, player);
}

static void
on_stream_menu_item_toggled (GtkCheckMenuItem    *item,
                             GvcStreamStatusIcon *icon)
{
        MateMixerStream    *stream;
        MateMixerDirection  direction;

        if (!gtk_check_menu_item_get_active (item))
                return;

        stream = g_object_get_data (G_OBJECT (item), "gvc-stream");
        if (stream == NULL || icon->priv->mixer_context == NULL)
                return;

        direction = mate_mixer_stream_get_direction (stream);
        if (direction == MATE_MIXER_DIRECTION_OUTPUT)
                mate_mixer_context_set_default_output_stream (icon->priv->mixer_context, stream);
        else if (direction == MATE_MIXER_DIRECTION_INPUT)
                mate_mixer_context_set_default_input_stream (icon->priv->mixer_context, stream);
}

static void
on_launch_player_menu_item_activate (GtkMenuItem         *item,
                                     GvcStreamStatusIcon *icon)
{
        const gchar     *desktop_entry;
        GDesktopAppInfo *app_info;
        GError          *error = NULL;

        desktop_entry = g_object_get_data (G_OBJECT (item), "desktop-entry");
        if (desktop_entry == NULL)
                return;

        app_info = g_desktop_app_info_new (desktop_entry);
        if (app_info == NULL && !g_str_has_suffix (desktop_entry, ".desktop")) {
                gchar *desktop_id;

                desktop_id = g_strconcat (desktop_entry, ".desktop", NULL);
                app_info = g_desktop_app_info_new (desktop_id);
                g_free (desktop_id);
        }

        if (app_info == NULL)
                return;

        g_app_info_launch (G_APP_INFO (app_info), NULL, NULL, &error);
        if (error != NULL)
                g_error_free (error);

        g_object_unref (app_info);
}

static GtkWidget *
new_image_menu_item (const gchar *label,
                     const gchar *icon_name)
{
        GtkWidget *item;
        GtkWidget *image;

        item = mate_image_menu_item_new_with_label (label);
        image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
        mate_image_menu_item_set_image (MATE_IMAGE_MENU_ITEM (item), image);

        return item;
}

static void
append_stream_menu_items (GvcStreamStatusIcon *icon,
                          GtkWidget           *submenu,
                          MateMixerDirection   direction)
{
        const GList *streams;
        GSList      *group = NULL;

        if (icon->priv->mixer_context == NULL)
                return;

        streams = mate_mixer_context_list_streams (icon->priv->mixer_context);
        while (streams != NULL) {
                MateMixerStream *stream = MATE_MIXER_STREAM (streams->data);

                if (mate_mixer_stream_get_direction (stream) == direction) {
                        GtkWidget       *item;
                        MateMixerStream *active;
                        const gchar     *label;

                        active = direction == MATE_MIXER_DIRECTION_OUTPUT ?
                                mate_mixer_context_get_default_output_stream (icon->priv->mixer_context) :
                                mate_mixer_context_get_default_input_stream (icon->priv->mixer_context);
                        label = mate_mixer_stream_get_label (stream);
                        if (label == NULL || label[0] == '\0')
                                label = mate_mixer_stream_get_name (stream);

                        item = gtk_radio_menu_item_new_with_label (group, label);
                        group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
                        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), stream == active);
                        g_object_set_data_full (G_OBJECT (item), "gvc-stream", g_object_ref (stream), g_object_unref);
                        g_signal_connect (item, "toggled", G_CALLBACK (on_stream_menu_item_toggled), icon);
                        gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
                }

                streams = streams->next;
        }
}

static void
on_status_icon_popup_menu (GtkStatusIcon       *status_icon,
                           guint                button,
                           guint                activate_time,
                           GvcStreamStatusIcon *icon)
{
        GtkWidget       *menu;
        GtkWidget       *item;
        GtkWidget       *image;
        GtkWidget       *submenu;
        GtkWidget       *toplevel;
        GtkStyleContext *context;
        GdkScreen       *screen;
        GdkVisual       *visual;
        GSList          *player_group = NULL;
        g_autofree char *label = NULL;

        menu = gtk_menu_new ();

        toplevel = gtk_widget_get_toplevel (menu);
        screen = gtk_widget_get_screen (GTK_WIDGET (toplevel));
        visual = gdk_screen_get_rgba_visual (screen);
        gtk_widget_set_visual (GTK_WIDGET (toplevel), visual);
        context = gtk_widget_get_style_context (GTK_WIDGET (toplevel));
        gtk_style_context_add_class (context, "gnome-panel-menu-bar");
        gtk_style_context_add_class (context, "mate-panel-menu-bar");

        if (icon->priv->mpris_manager != NULL) {
                GList *players;
                GList *l;

                item = new_image_menu_item (_("Media Players"), "applications-multimedia");
                submenu = gtk_menu_new ();
                players = gvc_mpris_manager_get_players (icon->priv->mpris_manager);
                for (l = players; l != NULL; l = l->next) {
                        GvcMprisPlayer *player = GVC_MPRIS_PLAYER (l->data);
                        GtkWidget      *player_item;
                        const gchar    *plabel;

                        if (!gvc_mpris_player_is_ready (player))
                                continue;

                        plabel = gvc_mpris_player_get_identity (player);
                        player_item = gtk_radio_menu_item_new_with_label (player_group,
                                                                          plabel != NULL && plabel[0] != '\0' ?
                                                                          plabel :
                                                                          gvc_mpris_player_get_bus_name (player));
                        player_group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (player_item));
                        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (player_item), player == icon->priv->player);
                        g_object_set_data_full (G_OBJECT (player_item), "gvc-player", g_object_ref (player), g_object_unref);
                        g_signal_connect (player_item, "toggled", G_CALLBACK (on_player_menu_item_toggled), icon);
                        gtk_menu_shell_append (GTK_MENU_SHELL (submenu), player_item);
                }
                g_list_free (players);
                gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        }

        if (icon->priv->applet_settings != NULL) {
                gchar **known;
                gint    i;

                item = new_image_menu_item (_("Launch Player"), "applications-multimedia");
                submenu = gtk_menu_new ();
                known = g_settings_get_strv (icon->priv->applet_settings, "known-players");
                for (i = 0; known != NULL && known[i] != NULL; i++) {
                        GtkWidget *launcher_item;

                        launcher_item = gtk_menu_item_new_with_label (known[i]);
                        g_object_set_data_full (G_OBJECT (launcher_item), "desktop-entry", g_strdup (known[i]), g_free);
                        g_signal_connect (launcher_item, "activate", G_CALLBACK (on_launch_player_menu_item_activate), icon);
                        gtk_menu_shell_append (GTK_MENU_SHELL (submenu), launcher_item);
                }
                g_strfreev (known);
                gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        }

        if (icon->priv->mixer_context != NULL) {
                item = new_image_menu_item (_("Output Device"), "audio-speakers");
                submenu = gtk_menu_new ();
                append_stream_menu_items (icon, submenu, MATE_MIXER_DIRECTION_OUTPUT);
                gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

                item = new_image_menu_item (_("Input Device"), "audio-input-microphone");
                submenu = gtk_menu_new ();
                append_stream_menu_items (icon, submenu, MATE_MIXER_DIRECTION_INPUT);
                gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        }

        if (icon->priv->mpris_manager != NULL || icon->priv->mixer_context != NULL) {
                item = gtk_separator_menu_item_new ();
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        }

        if (mate_mixer_stream_control_get_mute (icon->priv->control)) {
                label = g_strdup_printf ("%s %s", _("Unmute"), icon->priv->display_name);
                image = gtk_image_new_from_icon_name (icon->priv->icon_names[2], GTK_ICON_SIZE_MENU);
        } else {
                label = g_strdup_printf ("%s %s", _("Mute"), icon->priv->display_name);
                image = gtk_image_new_from_icon_name (icon->priv->icon_names[0], GTK_ICON_SIZE_MENU);
        }
        item = mate_image_menu_item_new_with_mnemonic (label);
        mate_image_menu_item_set_image (MATE_IMAGE_MENU_ITEM (item), image);
        g_signal_connect (G_OBJECT (item),
                          "activate",
                          G_CALLBACK (on_menu_mute_toggled),
                          icon);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        if (icon->priv->applet_settings != NULL) {
                item = gtk_separator_menu_item_new ();
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

                item = gtk_check_menu_item_new_with_mnemonic (_("Show _media player controls"));
                gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item),
                                                g_settings_get_boolean (icon->priv->applet_settings,
                                                                        "show-player-controls"));
                g_signal_connect (G_OBJECT (item),
                                  "toggled",
                                  G_CALLBACK (on_menu_show_player_toggled),
                                  icon);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

                item = gtk_separator_menu_item_new ();
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        }

        item = mate_image_menu_item_new_with_mnemonic (_("_Sound Preferences"));
        image = gtk_image_new_from_icon_name ("multimedia-volume-control", GTK_ICON_SIZE_MENU);
        mate_image_menu_item_set_image (MATE_IMAGE_MENU_ITEM (item), image);
        g_signal_connect (G_OBJECT (item),
                          "activate",
                          G_CALLBACK (on_menu_activate_open_volume_control),
                          icon);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        g_signal_connect (menu, "selection-done", G_CALLBACK (gtk_widget_destroy), NULL);

        gtk_widget_show_all (menu);
        gtk_menu_popup (GTK_MENU (menu),
                        NULL,
                        NULL,
                        gtk_status_icon_position_menu,
                        status_icon,
                        button,
                        activate_time);
}

static gboolean
on_status_icon_scroll_event (GtkStatusIcon       *status_icon,
                             GdkEventScroll      *event,
                             GvcStreamStatusIcon *icon)
{
        if (icon->priv->player != NULL) {
                if (icon->priv->applet_settings != NULL &&
                    !g_settings_get_boolean (icon->priv->applet_settings, "horizontal-scroll-controls"))
                        return gvc_channel_bar_scroll (GVC_CHANNEL_BAR (icon->priv->bar), event->direction);

                if (event->direction == GDK_SCROLL_LEFT) {
                        gvc_mpris_player_previous (icon->priv->player);
                        return TRUE;
                }
                if (event->direction == GDK_SCROLL_RIGHT) {
                        gvc_mpris_player_next (icon->priv->player);
                        return TRUE;
                }
        }

        return gvc_channel_bar_scroll (GVC_CHANNEL_BAR (icon->priv->bar), event->direction);
}

static void
gvc_icon_release_grab (GvcStreamStatusIcon *icon, GdkEventButton *event)
{
        GdkDisplay *display = gtk_widget_get_display (icon->priv->dock);
        GdkSeat    *seat = gdk_display_get_default_seat (display);
        gdk_seat_ungrab (seat);
        gtk_grab_remove (icon->priv->dock);

        gtk_widget_hide (icon->priv->dock);
}

static gboolean
on_dock_button_press (GtkWidget           *widget,
                      GdkEventButton      *event,
                      GvcStreamStatusIcon *icon)
{
        GtkAllocation allocation;
        GtkWidget    *current_grab;
        gint          dock_x;
        gint          dock_y;
        gboolean      inside_dock;

        if (event->type == GDK_BUTTON_PRESS) {
                current_grab = gtk_grab_get_current ();
                if (current_grab != NULL &&
                    current_grab != icon->priv->dock &&
                    !gtk_widget_is_ancestor (current_grab, icon->priv->dock))
                        return FALSE;

                gtk_widget_get_allocation (icon->priv->dock, &allocation);
                gdk_window_get_origin (gtk_widget_get_window (icon->priv->dock),
                                       &dock_x,
                                       &dock_y);
                inside_dock = event->x_root >= dock_x &&
                              event->x_root < dock_x + allocation.width &&
                              event->y_root >= dock_y &&
                              event->y_root < dock_y + allocation.height;

                if (inside_dock)
                        return FALSE;

                gvc_icon_release_grab (icon, event);
                return TRUE;
        }

        return FALSE;
}

static void
popdown_dock (GvcStreamStatusIcon *icon)
{
        GdkDisplay *display;
        GdkSeat    *seat;

        display = gtk_widget_get_display (icon->priv->dock);

        seat = gdk_display_get_default_seat (display);
        gdk_seat_ungrab (seat);
        if (gtk_widget_has_grab (icon->priv->dock))
                gtk_grab_remove (icon->priv->dock);

        gtk_widget_hide (icon->priv->dock);
}

/* This is called when the grab is broken for either the dock, or the scale */
static void
gvc_icon_grab_notify (GvcStreamStatusIcon *icon, gboolean was_grabbed)
{
        GtkWidget *current_grab;

        if (was_grabbed != FALSE)
                return;

        if (gtk_widget_has_grab (icon->priv->dock) == FALSE)
                return;

        current_grab = gtk_grab_get_current ();
        if (current_grab != NULL)
                return;

        popdown_dock (icon);
}

static void
on_dock_grab_notify (GtkWidget           *widget,
                     gboolean             was_grabbed,
                     GvcStreamStatusIcon *icon)
{
        gvc_icon_grab_notify (icon, was_grabbed);
}

static gboolean
on_dock_grab_broken_event (GtkWidget           *widget,
                           gboolean             was_grabbed,
                           GvcStreamStatusIcon *icon)
{
        gvc_icon_grab_notify (icon, FALSE);
        return FALSE;
}

static gboolean
on_dock_key_release (GtkWidget           *widget,
                     GdkEventKey         *event,
                     GvcStreamStatusIcon *icon)
{
        if (event->keyval == GDK_KEY_Escape) {
                popdown_dock (icon);
                return TRUE;
        }
        return TRUE;
}

static gboolean
on_dock_scroll_event (GtkWidget           *widget,
                      GdkEventScroll      *event,
                      GvcStreamStatusIcon *icon)
{
        if (icon->priv->player_widget_visible &&
            icon->priv->player_widget != NULL &&
            GVC_IS_PLAYER_WIDGET (icon->priv->player_widget) &&
            gvc_player_widget_scroll_is_seek (GVC_PLAYER_WIDGET (icon->priv->player_widget), event))
                return gvc_player_widget_handle_seek_scroll (GVC_PLAYER_WIDGET (icon->priv->player_widget), event);

        on_status_icon_scroll_event (NULL, event, icon);
        return TRUE;
}

static void
update_icon (GvcStreamStatusIcon *icon)
{
        guint                       volume = 0;
        guint                       volume_percent = 0;
        gdouble                     decibel = 0;
        guint                       normal = 0;
        gboolean                    muted = FALSE;
        guint                       n = 0;
        gchar                      *markup;
        const gchar                *description;
        MateMixerStreamControlFlags flags;

        if (icon->priv->control == NULL) {
                gtk_status_icon_set_has_tooltip (GTK_STATUS_ICON (icon), FALSE);
                return;
        }
        gtk_status_icon_set_has_tooltip (GTK_STATUS_ICON (icon), TRUE);

        flags = mate_mixer_stream_control_get_flags (icon->priv->control);

        if (flags & MATE_MIXER_STREAM_CONTROL_MUTE_READABLE)
                muted = mate_mixer_stream_control_get_mute (icon->priv->control);

        if (flags & MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE) {
                volume = mate_mixer_stream_control_get_volume (icon->priv->control);
                normal = mate_mixer_stream_control_get_normal_volume (icon->priv->control);

                /* Select an icon, they are expected to be sorted, the lowest index being
                 * the mute icon and the rest being volume increments */
                if (volume <= 0 || muted)
                        n = 0;
                else
                        n = CLAMP (3 * volume / normal + 1, 1, 3);
        }
        if (flags & MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL)
                decibel = mate_mixer_stream_control_get_decibel (icon->priv->control);

        gtk_status_icon_set_from_icon_name (GTK_STATUS_ICON (icon),
                                            icon->priv->icon_names[n]);
        icon->priv->current_icon = n;

        description = mate_mixer_stream_control_get_label (icon->priv->control);

        if (normal != 0)
                volume_percent = (guint) (100.0 * ((double) volume) / ((double) normal));

        if (icon->priv->applet_settings != NULL &&
            !g_settings_get_boolean (icon->priv->applet_settings, "tooltip-show-volume")) {
                markup = g_strdup_printf ("<b>%s</b>", icon->priv->display_name);
        } else if (muted) {
                markup = g_strdup_printf ("<b>%s: %s %u%%</b>\n<small>%s</small>",
                                          icon->priv->display_name,
                                          _("Muted at"),
                                          volume_percent,
                                          description);
        } else if (flags & MATE_MIXER_STREAM_CONTROL_VOLUME_READABLE) {
                if (flags & MATE_MIXER_STREAM_CONTROL_HAS_DECIBEL) {
                        if (decibel > -MATE_MIXER_INFINITY) {
                                markup = g_strdup_printf ("<b>%s: %u%%</b>\n"
                                                          "<small>%0.2f dB\n%s</small>",
                                                          icon->priv->display_name,
                                                          volume_percent,
                                                          decibel,
                                                          description);
                        } else {
                                markup = g_strdup_printf ("<b>%s: %u%%</b>\n"
                                                          "<small>-&#8734; dB\n%s</small>",
                                                          icon->priv->display_name,
                                                          volume_percent,
                                                          description);
                        }
                } else {
                        markup = g_strdup_printf ("<b>%s: %u%%</b>\n<small>%s</small>",
                                                  icon->priv->display_name,
                                                  volume_percent,
                                                  description);
                }
        } else {
                markup = g_strdup_printf ("<b>%s</b>\n<small>%s</small>",
                                          icon->priv->display_name,
                                          description);
        }

        if (icon->priv->player != NULL &&
            icon->priv->applet_settings != NULL &&
            g_settings_get_boolean (icon->priv->applet_settings, "tooltip-show-player")) {
                const gchar *title;
                const gchar *artist;
                const gchar *identity;
                gchar       *identity_escaped;
                gchar       *title_escaped;
                gchar       *artist_escaped;
                gchar       *player_markup;
                gchar       *combined;

                identity = gvc_mpris_player_get_identity (icon->priv->player);
                title = gvc_mpris_player_get_title (icon->priv->player);
                artist = gvc_mpris_player_get_artist (icon->priv->player);

                identity_escaped = g_markup_escape_text (identity != NULL ? identity : "", -1);
                title_escaped = g_markup_escape_text (title != NULL ? title : "", -1);
                artist_escaped = g_markup_escape_text (artist != NULL ? artist : "", -1);

                if (title_escaped[0] != '\0' && artist_escaped[0] != '\0')
                        player_markup = g_strdup_printf ("\n<small>%s\n%s - %s</small>",
                                                         identity_escaped,
                                                         artist_escaped,
                                                         title_escaped);
                else if (title_escaped[0] != '\0')
                        player_markup = g_strdup_printf ("\n<small>%s\n%s</small>",
                                                         identity_escaped,
                                                         title_escaped);
                else
                        player_markup = g_strdup_printf ("\n<small>%s</small>",
                                                         identity_escaped);

                combined = g_strconcat (markup, player_markup, NULL);
                g_free (markup);
                g_free (player_markup);
                g_free (identity_escaped);
                g_free (title_escaped);
                g_free (artist_escaped);
                markup = combined;
        }

        gtk_status_icon_set_tooltip_markup (GTK_STATUS_ICON (icon), markup);

        g_free (markup);
}

void
gvc_stream_status_icon_set_icon_names (GvcStreamStatusIcon  *icon,
                                       const gchar         **names)
{
        g_return_if_fail (GVC_IS_STREAM_STATUS_ICON (icon));
        g_return_if_fail (names != NULL && *names != NULL);

        if (G_UNLIKELY (g_strv_length ((gchar **) names) != 4)) {
                g_warn_if_reached ();
                return;
        }

        g_strfreev (icon->priv->icon_names);

        icon->priv->icon_names = g_strdupv ((gchar **) names);

        gtk_status_icon_set_from_icon_name (GTK_STATUS_ICON (icon), names[0]);
        update_icon (icon);

        g_object_notify_by_pspec (G_OBJECT (icon), properties[PROP_ICON_NAMES]);
}

static void
on_stream_control_volume_notify (MateMixerStreamControl *control,
                                 GParamSpec             *pspec,
                                 GvcStreamStatusIcon    *icon)
{
        update_icon (icon);
}

static void
on_stream_control_mute_notify (MateMixerStreamControl *control,
                               GParamSpec             *pspec,
                               GvcStreamStatusIcon    *icon)
{
        update_icon (icon);
}

void
gvc_stream_status_icon_set_display_name (GvcStreamStatusIcon *icon,
                                         const gchar         *name)
{
        g_return_if_fail (GVC_IS_STREAM_STATUS_ICON (icon));

        g_free (icon->priv->display_name);

        icon->priv->display_name = g_strdup (name);
        update_icon (icon);

        g_object_notify_by_pspec (G_OBJECT (icon), properties[PROP_DISPLAY_NAME]);
}

void
gvc_stream_status_icon_set_control (GvcStreamStatusIcon    *icon,
                                    MateMixerStreamControl *control)
{
        g_return_if_fail (GVC_IS_STREAM_STATUS_ICON (icon));

        if (icon->priv->control == control) {
                if (control != NULL)
                        update_icon (icon);
                return;
        }

        if (control != NULL)
                g_object_ref (control);

        if (icon->priv->control != NULL) {
                g_signal_handlers_disconnect_by_func (G_OBJECT (icon->priv->control),
                                                      G_CALLBACK (on_stream_control_volume_notify),
                                                      icon);
                g_signal_handlers_disconnect_by_func (G_OBJECT (icon->priv->control),
                                                      G_CALLBACK (on_stream_control_mute_notify),
                                                      icon);

                g_object_unref (icon->priv->control);
        }

        icon->priv->control = control;

        if (icon->priv->control != NULL) {
                g_signal_connect (G_OBJECT (icon->priv->control),
                                  "notify::volume",
                                  G_CALLBACK (on_stream_control_volume_notify),
                                  icon);
                g_signal_connect (G_OBJECT (icon->priv->control),
                                  "notify::mute",
                                  G_CALLBACK (on_stream_control_mute_notify),
                                  icon);

                update_icon (icon);
        }

        gvc_channel_bar_set_control (GVC_CHANNEL_BAR (icon->priv->bar), icon->priv->control);

        g_object_notify_by_pspec (G_OBJECT (icon), properties[PROP_CONTROL]);
}

void
gvc_stream_status_icon_set_applet_settings (GvcStreamStatusIcon *icon,
                                            GSettings           *settings)
{
        g_return_if_fail (GVC_IS_STREAM_STATUS_ICON (icon));
        g_return_if_fail (settings == NULL || G_IS_SETTINGS (settings));

        if (icon->priv->applet_settings == settings)
                return;

        g_clear_object (&icon->priv->applet_settings);

        if (settings != NULL)
                icon->priv->applet_settings = g_object_ref (settings);

        update_icon (icon);
}

void
gvc_stream_status_icon_set_player_widget (GvcStreamStatusIcon *icon,
                                          GtkWidget           *player_widget)
{
        g_return_if_fail (GVC_IS_STREAM_STATUS_ICON (icon));
        g_return_if_fail (player_widget == NULL || GTK_IS_WIDGET (player_widget));

        if (icon->priv->player_widget == player_widget)
                return;

        if (icon->priv->player_widget != NULL) {
                gtk_container_remove (GTK_CONTAINER (icon->priv->dock_box),
                                      icon->priv->player_widget);
                gtk_container_remove (GTK_CONTAINER (icon->priv->dock_box),
                                      icon->priv->player_separator);
                icon->priv->player_widget = NULL;
                icon->priv->player_separator = NULL;
        }

        if (player_widget != NULL) {
                GtkWidget *separator;

                icon->priv->player_widget = player_widget;
                separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
                gtk_widget_set_no_show_all (separator, TRUE);
                icon->priv->player_separator = separator;
                gtk_box_pack_start (GTK_BOX (icon->priv->dock_box),
                                    player_widget,
                                    FALSE,
                                    FALSE,
                                    0);
                gtk_box_pack_start (GTK_BOX (icon->priv->dock_box),
                                    separator,
                                    FALSE,
                                    FALSE,
                                    0);
                gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box),
                                       player_widget,
                                       0);
                gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box),
                                       separator,
                                       1);
        }

        configure_dock_layout (icon, GTK_ORIENTATION_HORIZONTAL, FALSE);
}

static void
on_mpris_player_tooltip_changed (GvcMprisPlayer      *player,
                                 GvcStreamStatusIcon *icon)
{
        update_icon (icon);
}

void
gvc_stream_status_icon_set_mpris_player (GvcStreamStatusIcon *icon,
                                         GvcMprisPlayer      *player)
{
        g_return_if_fail (GVC_IS_STREAM_STATUS_ICON (icon));

        if (icon->priv->player == player)
                return;

        if (icon->priv->player != NULL)
                g_signal_handlers_disconnect_by_data (icon->priv->player, icon);

        g_clear_object (&icon->priv->player);

        if (player != NULL) {
                icon->priv->player = g_object_ref (player);
                g_signal_connect (player, "metadata-changed", G_CALLBACK (on_mpris_player_tooltip_changed), icon);
                g_signal_connect (player, "status-changed", G_CALLBACK (on_mpris_player_tooltip_changed), icon);
        }

        update_icon (icon);
}

void
gvc_stream_status_icon_set_player_widget_visible (GvcStreamStatusIcon *icon,
                                                  gboolean             visible)
{
        g_return_if_fail (GVC_IS_STREAM_STATUS_ICON (icon));

        icon->priv->player_widget_visible = visible;

        if (icon->priv->player_separator != NULL)
                gtk_widget_set_visible (icon->priv->player_separator, visible);

        configure_dock_layout (icon, GTK_ORIENTATION_HORIZONTAL, FALSE);
}

void
gvc_stream_status_icon_set_mpris_manager (GvcStreamStatusIcon *icon,
                                          GvcMprisManager     *manager)
{
        g_return_if_fail (GVC_IS_STREAM_STATUS_ICON (icon));

        if (icon->priv->mpris_manager == manager)
                return;

        g_clear_object (&icon->priv->mpris_manager);

        if (manager != NULL)
                icon->priv->mpris_manager = g_object_ref (manager);
}

void
gvc_stream_status_icon_set_mate_mixer_context (GvcStreamStatusIcon *icon,
                                               MateMixerContext    *context)
{
        g_return_if_fail (GVC_IS_STREAM_STATUS_ICON (icon));

        if (icon->priv->mixer_context == context)
                return;

        g_clear_object (&icon->priv->mixer_context);

        if (context != NULL)
                icon->priv->mixer_context = g_object_ref (context);
}

static void
gvc_stream_status_icon_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
        GvcStreamStatusIcon *self = GVC_STREAM_STATUS_ICON (object);

        switch (prop_id) {
        case PROP_CONTROL:
                gvc_stream_status_icon_set_control (self, g_value_get_object (value));
                break;
        case PROP_DISPLAY_NAME:
                gvc_stream_status_icon_set_display_name (self, g_value_get_string (value));
                break;
        case PROP_ICON_NAMES:
                gvc_stream_status_icon_set_icon_names (self, g_value_get_boxed (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_stream_status_icon_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
        GvcStreamStatusIcon *self = GVC_STREAM_STATUS_ICON (object);

        switch (prop_id) {
        case PROP_CONTROL:
                g_value_set_object (value, self->priv->control);
                break;
        case PROP_DISPLAY_NAME:
                g_value_set_string (value, self->priv->display_name);
                break;
        case PROP_ICON_NAMES:
                g_value_set_boxed (value, self->priv->icon_names);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_stream_status_icon_dispose (GObject *object)
{
        GvcStreamStatusIcon *icon = GVC_STREAM_STATUS_ICON (object);

        if (icon->priv->dock != NULL) {
                gtk_widget_destroy (icon->priv->dock);
                icon->priv->dock = NULL;
        }

        if (icon->priv->player != NULL) {
                g_signal_handlers_disconnect_by_data (icon->priv->player, icon);
                g_clear_object (&icon->priv->player);
        }

        g_clear_object (&icon->priv->control);
        g_clear_object (&icon->priv->mpris_manager);
        g_clear_object (&icon->priv->mixer_context);

        G_OBJECT_CLASS (gvc_stream_status_icon_parent_class)->dispose (object);
}

static void
gvc_stream_status_icon_class_init (GvcStreamStatusIconClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gvc_stream_status_icon_finalize;
        object_class->dispose  = gvc_stream_status_icon_dispose;
        object_class->set_property = gvc_stream_status_icon_set_property;
        object_class->get_property = gvc_stream_status_icon_get_property;

        properties[PROP_CONTROL] =
                g_param_spec_object ("control",
                                     "Control",
                                     "MateMixer stream control",
                                     MATE_MIXER_TYPE_STREAM_CONTROL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT |
                                     G_PARAM_STATIC_STRINGS);

        properties[PROP_DISPLAY_NAME] =
                g_param_spec_string ("display-name",
                                     "Display name",
                                     "Name to display for this stream",
                                     NULL,
                                     G_PARAM_READWRITE |
                                     G_PARAM_CONSTRUCT |
                                     G_PARAM_STATIC_STRINGS);

        properties[PROP_ICON_NAMES] =
                g_param_spec_boxed ("icon-names",
                                    "Icon names",
                                    "Name of icon to display for this stream",
                                    G_TYPE_STRV,
                                    G_PARAM_READWRITE |
                                    G_PARAM_CONSTRUCT |
                                    G_PARAM_STATIC_STRINGS);

        g_object_class_install_properties (object_class, N_PROPERTIES, properties);

        signals[PLAYER_SELECTED] =
                g_signal_new ("player-selected",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0,
                              NULL,
                              NULL,
                              NULL,
                              G_TYPE_NONE,
                              1,
                              GVC_TYPE_MPRIS_PLAYER);
}

static void
on_status_icon_visible_notify (GvcStreamStatusIcon *icon)
{
        if (gtk_status_icon_get_visible (GTK_STATUS_ICON (icon)) == FALSE)
                gtk_widget_hide (icon->priv->dock);
}

static void
on_icon_theme_change (GtkSettings         *settings,
                      GParamSpec          *pspec,
                      GvcStreamStatusIcon *icon)
{
        gtk_status_icon_set_from_icon_name (GTK_STATUS_ICON (icon),
                                            icon->priv->icon_names[icon->priv->current_icon]);
}

static void
gvc_stream_status_icon_init (GvcStreamStatusIcon *icon)
{
        GtkWidget       *frame;
        GtkWidget       *toplevel;
        GtkStyleContext *context;
        GdkScreen       *screen;
        GdkVisual       *visual;

        icon->priv = gvc_stream_status_icon_get_instance_private (icon);

        icon->priv->sound_settings = g_settings_new ("org.mate.sound");

        g_signal_connect (G_OBJECT (icon),
                          "activate",
                          G_CALLBACK (on_status_icon_activate),
                          icon);
        g_signal_connect (G_OBJECT (icon),
                          "button-press-event",
                          G_CALLBACK (on_status_icon_button_press),
                          icon);
        g_signal_connect (G_OBJECT (icon),
                          "popup-menu",
                          G_CALLBACK (on_status_icon_popup_menu),
                          icon);
        g_signal_connect (G_OBJECT (icon),
                          "scroll-event",
                          G_CALLBACK (on_status_icon_scroll_event),
                          icon);
        g_signal_connect (G_OBJECT (icon),
                          "notify::visible",
                          G_CALLBACK (on_status_icon_visible_notify),
                          NULL);

        icon->priv->dock = gtk_window_new (GTK_WINDOW_POPUP);

        gtk_window_set_decorated (GTK_WINDOW (icon->priv->dock), FALSE);

        g_signal_connect (G_OBJECT (icon->priv->dock),
                          "button-press-event",
                          G_CALLBACK (on_dock_button_press),
                          icon);
        g_signal_connect (G_OBJECT (icon->priv->dock),
                          "key-release-event",
                          G_CALLBACK (on_dock_key_release),
                          icon);
        g_signal_connect (G_OBJECT (icon->priv->dock),
                          "scroll-event",
                          G_CALLBACK (on_dock_scroll_event),
                          icon);
        g_signal_connect (G_OBJECT (icon->priv->dock),
                          "grab-notify",
                          G_CALLBACK (on_dock_grab_notify),
                          icon);
        g_signal_connect (G_OBJECT (icon->priv->dock),
                          "grab-broken-event",
                          G_CALLBACK (on_dock_grab_broken_event),
                          icon);

        frame = gtk_frame_new (NULL);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
        gtk_container_add (GTK_CONTAINER (icon->priv->dock), frame);

        icon->priv->bar = gvc_channel_bar_new (NULL);
        icon->priv->volume_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
        icon->priv->volume_image = GTK_IMAGE (gtk_image_new_from_icon_name ("multimedia-volume-control",
                                                                            GTK_ICON_SIZE_MENU));
        gtk_widget_set_halign (GTK_WIDGET (icon->priv->volume_image), GTK_ALIGN_CENTER);
        gtk_widget_set_valign (GTK_WIDGET (icon->priv->volume_image), GTK_ALIGN_CENTER);

        gvc_channel_bar_set_orientation (GVC_CHANNEL_BAR (icon->priv->bar),
                                         GTK_ORIENTATION_VERTICAL);

        gvc_channel_bar_set_show_mark_text (GVC_CHANNEL_BAR (icon->priv->bar),
                                            FALSE);

        g_settings_bind (icon->priv->sound_settings, "volume-overamplifiable",
                         icon->priv->bar,            "show-marks",
                         G_SETTINGS_BIND_GET);

        g_settings_bind (icon->priv->sound_settings, "volume-overamplifiable",
                         icon->priv->bar,            "extended",
                         G_SETTINGS_BIND_GET);

        toplevel = gtk_widget_get_toplevel (icon->priv->dock);
        context = gtk_widget_get_style_context (GTK_WIDGET (toplevel));
        gtk_style_context_add_class (context, "mate-panel-applet-slider");
        screen = gtk_widget_get_screen (GTK_WIDGET (toplevel));
        visual = gdk_screen_get_rgba_visual (screen);
        gtk_widget_set_visual (GTK_WIDGET (toplevel), visual);

        icon->priv->dock_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

        gtk_container_set_border_width (GTK_CONTAINER (icon->priv->dock_box), 2);
        gtk_container_add (GTK_CONTAINER (frame), icon->priv->dock_box);

        gtk_box_pack_start (GTK_BOX (icon->priv->volume_box), icon->priv->bar, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (icon->priv->volume_box),
                            GTK_WIDGET (icon->priv->volume_image),
                            FALSE,
                            FALSE,
                            0);
        gtk_box_pack_start (GTK_BOX (icon->priv->dock_box), icon->priv->volume_box, TRUE, FALSE, 0);

        configure_dock_layout (icon, GTK_ORIENTATION_HORIZONTAL, FALSE);

        g_signal_connect (gtk_settings_get_default (),
                          "notify::gtk-icon-theme-name",
                          G_CALLBACK (on_icon_theme_change),
                          icon);
}

static void
gvc_stream_status_icon_finalize (GObject *object)
{
        GvcStreamStatusIcon *icon;

        icon = GVC_STREAM_STATUS_ICON (object);

        g_strfreev (icon->priv->icon_names);
        g_clear_pointer (&icon->priv->display_name, g_free);

        g_signal_handlers_disconnect_by_func (gtk_settings_get_default (),
                                              on_icon_theme_change,
                                              icon);

        g_clear_object (&icon->priv->sound_settings);
        g_clear_object (&icon->priv->applet_settings);

        G_OBJECT_CLASS (gvc_stream_status_icon_parent_class)->finalize (object);
}

GvcStreamStatusIcon *
gvc_stream_status_icon_new (MateMixerStreamControl *control,
                            const gchar           **icon_names)
{
        return g_object_new (GVC_TYPE_STREAM_STATUS_ICON,
                             "control", control,
                             "icon-names", icon_names,
                             NULL);
}
