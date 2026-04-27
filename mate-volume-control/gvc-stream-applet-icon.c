/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
 * Copyright (C) 2019 Victor Kareh <vkareh@vkareh.net>
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
#include <config.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#if defined(ENABLE_WAYLAND)
#include <gdk/gdkwayland.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#endif

#include <libmatemixer/matemixer.h>
#include <mate-panel-applet.h>

#define MATE_DESKTOP_USE_UNSTABLE_API
#include <libmate-desktop/mate-desktop-utils.h>

#include "gvc-channel-bar.h"
#include "gvc-mpris-player.h"
#include "gvc-player-widget.h"
#include "gvc-stream-applet-icon.h"

struct _GvcStreamAppletIconPrivate
{
        GSettings              *sound_settings;
        GSettings              *applet_settings;
        gchar                 **icon_names;
        GtkImage               *image;
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
        MatePanelAppletOrient   orient;
        guint                   size;
};

enum
{
        PROP_0,
        PROP_CONTROL,
        PROP_DISPLAY_NAME,
        PROP_ICON_NAMES,
        N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void gvc_stream_applet_icon_finalize   (GObject *object);
static void update_icon                       (GvcStreamAppletIcon *icon);

G_DEFINE_TYPE_WITH_PRIVATE (GvcStreamAppletIcon, gvc_stream_applet_icon, GTK_TYPE_EVENT_BOX)

static void
configure_volume_box_layout (GvcStreamAppletIcon *icon,
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
configure_dock_layout (GvcStreamAppletIcon *icon,
                       gboolean             volume_first)
{
        gboolean vertical_panel;

        vertical_panel = icon->priv->orient == MATE_PANEL_APPLET_ORIENT_LEFT ||
                         icon->priv->orient == MATE_PANEL_APPLET_ORIENT_RIGHT;

        if (icon->priv->player_widget_visible && vertical_panel) {
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
                        gtk_box_reorder_child (GTK_BOX (icon->priv->dock_box),
                                               icon->priv->player_widget,
                                               2);
                } else {
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
        switch (icon->priv->orient) {
            case MATE_PANEL_APPLET_ORIENT_LEFT:
            case MATE_PANEL_APPLET_ORIENT_RIGHT:
                gvc_channel_bar_set_orientation (GVC_CHANNEL_BAR (icon->priv->bar), GTK_ORIENTATION_HORIZONTAL);
                configure_volume_box_layout (icon, GTK_ORIENTATION_HORIZONTAL);
                break;
            case MATE_PANEL_APPLET_ORIENT_UP:
            case MATE_PANEL_APPLET_ORIENT_DOWN:
            default:
                gvc_channel_bar_set_orientation (GVC_CHANNEL_BAR (icon->priv->bar), GTK_ORIENTATION_VERTICAL);
                configure_volume_box_layout (icon, GTK_ORIENTATION_VERTICAL);
        }
}

static gboolean
popup_dock (GvcStreamAppletIcon *icon, guint time)
{
        GtkAllocation  allocation;
        GdkDisplay    *display;
        GdkScreen     *screen;
        int            x, y;
        GdkMonitor    *monitor_num;
        GdkRectangle   monitor;
        GtkRequisition dock_req;

        screen = gtk_widget_get_screen (GTK_WIDGET (icon));
        gtk_widget_get_allocation (GTK_WIDGET (icon), &allocation);
        gdk_window_get_origin (gtk_widget_get_window (GTK_WIDGET (icon)), &allocation.x, &allocation.y);
        gtk_widget_set_state_flags (GTK_WIDGET (icon), GTK_STATE_FLAG_CHECKED, FALSE);

        /* position roughly */
        gtk_window_set_screen (GTK_WINDOW (icon->priv->dock), screen);
        configure_dock_layout (icon, FALSE);

        display = gdk_screen_get_display (screen);
        monitor_num = gdk_display_get_monitor_at_point (display, allocation.x, allocation.y);
        gdk_monitor_get_geometry (monitor_num, &monitor);

        gtk_container_foreach (GTK_CONTAINER (icon->priv->dock), (GtkCallback) gtk_widget_show_all, NULL);
        gtk_widget_get_preferred_size (icon->priv->dock, &dock_req, NULL);

#if defined(ENABLE_WAYLAND)
        if (GDK_IS_WAYLAND_DISPLAY (display))
        {
            gboolean top, bottom, left, right;
            GtkWidget *toplevel;
            toplevel = gtk_widget_get_toplevel (GTK_WIDGET (icon));

            if (!gtk_layer_is_layer_window (GTK_WINDOW (icon->priv->dock)))
            {
                gtk_layer_init_for_window (GTK_WINDOW (icon->priv->dock));
                gtk_layer_set_layer (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_LAYER_TOP);
                gtk_layer_set_keyboard_mode (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
            }

            top = gtk_layer_get_anchor (GTK_WINDOW (toplevel), GTK_LAYER_SHELL_EDGE_TOP);
            bottom = gtk_layer_get_anchor (GTK_WINDOW (toplevel), GTK_LAYER_SHELL_EDGE_BOTTOM);
            left = gtk_layer_get_anchor (GTK_WINDOW (toplevel), GTK_LAYER_SHELL_EDGE_LEFT);
            right = gtk_layer_get_anchor (GTK_WINDOW (toplevel), GTK_LAYER_SHELL_EDGE_RIGHT);

            /*Set anchors to the edges (will hold to panel edge) and position along the panel
             *Unset margins and anchors from any other position so as to avoid rendering issues
             *when orientation changes as when the panel is moved
             */

            if (top && left && right)
            {
                configure_dock_layout (icon, TRUE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_RIGHT, FALSE);
                gtk_layer_set_margin (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_LEFT, allocation.x);
                gtk_layer_set_margin (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_TOP, 0);
            }
            if (bottom && left && right)
            {
                configure_dock_layout (icon, FALSE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_TOP, FALSE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_RIGHT, FALSE);
                gtk_layer_set_margin (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_LEFT, allocation.x);
                gtk_layer_set_margin (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_TOP, 0);
            }
            if (left && bottom && top && !right)
            {
                configure_dock_layout (icon, TRUE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_RIGHT, FALSE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
                gtk_layer_set_margin (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_TOP, allocation.y);
                gtk_layer_set_margin (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_LEFT, 0);
            }
            if (right && bottom && top && !left)
            {
                configure_dock_layout (icon, FALSE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_LEFT, FALSE);
                gtk_layer_set_anchor (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE);
                gtk_layer_set_margin (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_TOP, allocation.y);
                gtk_layer_set_margin (GTK_WINDOW (icon->priv->dock), GTK_LAYER_SHELL_EDGE_LEFT, 0);
            }
            gtk_widget_show_all (icon->priv->dock);

            /* Grab focus */
            gtk_grab_add (icon->priv->dock);
            gtk_widget_grab_focus (icon->priv->dock);

            return TRUE;
        }
#endif /* wayland support */

        if (icon->priv->orient == MATE_PANEL_APPLET_ORIENT_LEFT || icon->priv->orient == MATE_PANEL_APPLET_ORIENT_RIGHT) {
                if (allocation.x + allocation.width + dock_req.width <= monitor.x + monitor.width)
                        x = allocation.x + allocation.width;
                else
                        x = allocation.x - dock_req.width;

                if (allocation.y + dock_req.height <= monitor.y + monitor.height)
                        y = allocation.y;
                else
                        y = monitor.y + monitor.height - dock_req.height;
        } else {
                if (allocation.y + allocation.height + dock_req.height <= monitor.y + monitor.height)
                        y = allocation.y + allocation.height;
                else
                        y = allocation.y - dock_req.height;

                if (allocation.x + dock_req.width <= monitor.x + monitor.width)
                        x = allocation.x;
                else
                        x = monitor.x + monitor.width - dock_req.width;
        }

        if (icon->priv->orient == MATE_PANEL_APPLET_ORIENT_LEFT ||
            icon->priv->orient == MATE_PANEL_APPLET_ORIENT_RIGHT)
                configure_dock_layout (icon, x > allocation.x);
        else
                configure_dock_layout (icon, y > allocation.y);

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

static gboolean
on_applet_icon_button_press (GtkWidget           *applet_icon,
                             GdkEventButton      *event,
                             GvcStreamAppletIcon *icon)
{
        if (event->button == 1) {
                popup_dock (icon, GDK_CURRENT_TIME);
                return TRUE;
        }

        if (event->button == 8 && icon->priv->player != NULL) {
                gvc_mpris_player_previous (icon->priv->player);
                return TRUE;
        }

        if (event->button == 9 && icon->priv->player != NULL) {
                gvc_mpris_player_next (icon->priv->player);
                return TRUE;
        }

        /* Middle click acts as mute/unmute */
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
                } else if (g_strcmp0 (action, "mute-output") != 0 &&
                           g_strcmp0 (action, "mute-all") != 0 &&
                           action != NULL) {
                        g_free (action);
                        return FALSE;
                }

                g_free (action);

                if (icon->priv->control == NULL)
                        return FALSE;

                gboolean is_muted = mate_mixer_stream_control_get_mute (icon->priv->control);

                mate_mixer_stream_control_set_mute (icon->priv->control, !is_muted);
                return TRUE;
        }
        return FALSE;
}

void
gvc_stream_applet_icon_set_mute (GvcStreamAppletIcon *icon, gboolean mute)
{
        mate_mixer_stream_control_set_mute (icon->priv->control, mute);
}

gboolean
gvc_stream_applet_icon_get_mute (GvcStreamAppletIcon *icon)
{
        return mate_mixer_stream_control_get_mute (icon->priv->control);
}

void
gvc_stream_applet_icon_volume_control (GvcStreamAppletIcon *icon)
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

static gboolean
on_applet_icon_scroll_event (GtkWidget           *event_box,
                             GdkEventScroll      *event,
                             GvcStreamAppletIcon *icon)
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

void
gvc_stream_applet_icon_set_applet_settings (GvcStreamAppletIcon *icon,
                                            GSettings           *settings)
{
        g_return_if_fail (GVC_IS_STREAM_APPLET_ICON (icon));
        g_return_if_fail (settings == NULL || G_IS_SETTINGS (settings));

        if (icon->priv->applet_settings == settings)
                return;

        g_clear_object (&icon->priv->applet_settings);

        if (settings != NULL)
                icon->priv->applet_settings = g_object_ref (settings);

        update_icon (icon);
}

static void
gvc_icon_release_grab (GvcStreamAppletIcon *icon, GdkEventButton *event)
{
        GdkDisplay *display = gtk_widget_get_display (icon->priv->dock);
        GdkSeat *seat = gdk_display_get_default_seat (display);
        gdk_seat_ungrab (seat);
        gtk_grab_remove (icon->priv->dock);

        /* Hide again */
        gtk_widget_unset_state_flags (GTK_WIDGET (icon), GTK_STATE_FLAG_CHECKED);
        gtk_widget_hide (icon->priv->dock);
}

void
gvc_stream_applet_icon_set_player_widget (GvcStreamAppletIcon *icon,
                                          GtkWidget           *player_widget)
{
        g_return_if_fail (GVC_IS_STREAM_APPLET_ICON (icon));
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
}

static void
on_mpris_player_tooltip_changed (GvcMprisPlayer      *player,
                                 GvcStreamAppletIcon *icon)
{
        update_icon (icon);
}

void
gvc_stream_applet_icon_set_mpris_player (GvcStreamAppletIcon *icon,
                                         GvcMprisPlayer      *player)
{
        g_return_if_fail (GVC_IS_STREAM_APPLET_ICON (icon));

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
gvc_stream_applet_icon_set_player_widget_visible (GvcStreamAppletIcon *icon,
                                                 gboolean             visible)
{
        g_return_if_fail (GVC_IS_STREAM_APPLET_ICON (icon));

        icon->priv->player_widget_visible = visible;

        if (icon->priv->player_separator != NULL)
                gtk_widget_set_visible (icon->priv->player_separator, visible);
}

static gboolean
on_dock_button_press (GtkWidget           *widget,
                      GdkEventButton      *event,
                      GvcStreamAppletIcon *icon)
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
popdown_dock (GvcStreamAppletIcon *icon)
{
        GdkDisplay *display;

        display = gtk_widget_get_display (icon->priv->dock);

        GdkSeat *seat = gdk_display_get_default_seat (display);
        gdk_seat_ungrab (seat);
        if (gtk_widget_has_grab (icon->priv->dock))
                gtk_grab_remove (icon->priv->dock);

        /* Hide again */
        gtk_widget_unset_state_flags (GTK_WIDGET (icon), GTK_STATE_FLAG_CHECKED);
        gtk_widget_hide (icon->priv->dock);
}

/* This is called when the grab is broken for either the dock, or the scale */
static void
gvc_icon_grab_notify (GvcStreamAppletIcon *icon, gboolean was_grabbed)
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
                     GvcStreamAppletIcon *icon)
{
        gvc_icon_grab_notify (icon, was_grabbed);
}

static gboolean
on_dock_grab_broken_event (GtkWidget           *widget,
                           gboolean             was_grabbed,
                           GvcStreamAppletIcon *icon)
{
        gvc_icon_grab_notify (icon, FALSE);
        return FALSE;
}

static gboolean
on_dock_key_release (GtkWidget           *widget,
                     GdkEventKey         *event,
                     GvcStreamAppletIcon *icon)
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
                      GvcStreamAppletIcon *icon)
{
        if (icon->priv->player_widget_visible &&
            icon->priv->player_widget != NULL &&
            GVC_IS_PLAYER_WIDGET (icon->priv->player_widget) &&
            gvc_player_widget_scroll_is_seek (GVC_PLAYER_WIDGET (icon->priv->player_widget), event))
                return gvc_player_widget_handle_seek_scroll (GVC_PLAYER_WIDGET (icon->priv->player_widget), event);

        /* Forward event to the applet icon */
        on_applet_icon_scroll_event (NULL, event, icon);
        return TRUE;
}

static void
gvc_stream_applet_icon_set_icon_from_name (GvcStreamAppletIcon *icon,
                                           const gchar *icon_name)
{
        GtkIconTheme    *icon_theme;
        gint             icon_scale;
        guint            size;
        cairo_surface_t *surface;

        if (icon_name == NULL)
                return;

        size = icon->priv->size;
        if (size == 0)
                size = 24;

        icon_theme = gtk_icon_theme_get_default ();
        icon_scale = gtk_widget_get_scale_factor (GTK_WIDGET (icon));

        surface = gtk_icon_theme_load_surface (icon_theme, icon_name,
                                               size,
                                               icon_scale, NULL,
                                               GTK_ICON_LOOKUP_FORCE_SIZE,
                                               NULL);

        if (surface == NULL) {
                gtk_image_set_from_icon_name (GTK_IMAGE (icon->priv->image),
                                              icon_name,
                                              GTK_ICON_SIZE_LARGE_TOOLBAR);
                gtk_image_set_pixel_size (GTK_IMAGE (icon->priv->image), size);
                return;
        }

        gtk_image_set_from_surface (GTK_IMAGE (icon->priv->image), surface);
        cairo_surface_destroy (surface);
}

static void
update_icon (GvcStreamAppletIcon *icon)
{
        guint                       volume = 0;
        gdouble                     decibel = 0;
        guint                       normal = 0;
        gboolean                    muted = FALSE;
        guint                       n = 0;
        gchar                      *markup;
        const gchar                *description;
        MateMixerStreamControlFlags flags;

        if (icon->priv->control == NULL) {
                /* Do not bother creating a tooltip for an unusable icon as it
                 * has no practical use */
                gtk_widget_set_has_tooltip (GTK_WIDGET (icon), FALSE);
                return;
        } else
                gtk_widget_set_has_tooltip (GTK_WIDGET (icon), TRUE);

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

        gvc_stream_applet_icon_set_icon_from_name (icon, icon->priv->icon_names[n]);
        icon->priv->current_icon = n;

        description = mate_mixer_stream_control_get_label (icon->priv->control);

        guint volume_percent = (guint) round (100.0 * volume / normal);
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

        gtk_widget_set_tooltip_markup (GTK_WIDGET (icon), markup);

        g_free (markup);
}

void
gvc_stream_applet_icon_set_size (GvcStreamAppletIcon *icon,
                                 guint                size)
{

        /*Iterate through the icon sizes so they can be kept sharp*/
        if (size < 22)
                size = 16;
        else if (size < 24)
                size = 22;
        else if (size < 32)
                size = 24;
        else if (size < 48)
                size = 32;

        icon->priv->size = size;
        if (icon->priv->icon_names != NULL)
                gvc_stream_applet_icon_set_icon_from_name (icon, icon->priv->icon_names[icon->priv->current_icon]);
}

void
gvc_stream_applet_icon_set_orient (GvcStreamAppletIcon  *icon,
                                   MatePanelAppletOrient orient)
{
        /* Sometimes orient does not get properly defined especially on a bottom panel.
         * Use the applet orientation if it is valid, otherwise set a vertical slider,
         * otherwise bottom panels get a horizontal slider.
         */
        if (orient)
                icon->priv->orient = orient;
        else
                icon->priv->orient = MATE_PANEL_APPLET_ORIENT_DOWN;
}

void
gvc_stream_applet_icon_set_icon_names (GvcStreamAppletIcon  *icon,
                                       const gchar         **names)
{
        g_return_if_fail (GVC_IS_STREAM_APPLET_ICON (icon));
        g_return_if_fail (names != NULL && *names != NULL);

        if (G_UNLIKELY (g_strv_length ((gchar **) names) != 4)) {
                g_warn_if_reached ();
                return;
        }

        g_strfreev (icon->priv->icon_names);

        icon->priv->icon_names = g_strdupv ((gchar **) names);

        /* Set the first icon as the initial one, the icon may be immediately
         * updated or not depending on whether a stream is available */
        gvc_stream_applet_icon_set_icon_from_name (icon, names[0]);
        update_icon (icon);

        g_object_notify_by_pspec (G_OBJECT (icon), properties[PROP_ICON_NAMES]);
}

static void
on_stream_control_volume_notify (MateMixerStreamControl *control,
                                 GParamSpec             *pspec,
                                 GvcStreamAppletIcon    *icon)
{
        update_icon (icon);
}

static void
on_stream_control_mute_notify (MateMixerStreamControl *control,
                               GParamSpec             *pspec,
                               GvcStreamAppletIcon    *icon)
{
        update_icon (icon);
}

void
gvc_stream_applet_icon_set_display_name (GvcStreamAppletIcon *icon,
                                         const gchar         *name)
{
        g_return_if_fail (GVC_IS_STREAM_APPLET_ICON (icon));

        g_free (icon->priv->display_name);

        icon->priv->display_name = g_strdup (name);
        update_icon (icon);

        g_object_notify_by_pspec (G_OBJECT (icon), properties[PROP_DISPLAY_NAME]);
}

void
gvc_stream_applet_icon_set_control (GvcStreamAppletIcon    *icon,
                                    MateMixerStreamControl *control)
{
        g_return_if_fail (GVC_IS_STREAM_APPLET_ICON (icon));

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

static void
gvc_stream_applet_icon_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
        GvcStreamAppletIcon *self = GVC_STREAM_APPLET_ICON (object);

        switch (prop_id) {
        case PROP_CONTROL:
                gvc_stream_applet_icon_set_control (self, g_value_get_object (value));
                break;
        case PROP_DISPLAY_NAME:
                gvc_stream_applet_icon_set_display_name (self, g_value_get_string (value));
                break;
        case PROP_ICON_NAMES:
                gvc_stream_applet_icon_set_icon_names (self, g_value_get_boxed (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_stream_applet_icon_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
        GvcStreamAppletIcon *self = GVC_STREAM_APPLET_ICON (object);

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
gvc_stream_applet_icon_dispose (GObject *object)
{
        GvcStreamAppletIcon *icon = GVC_STREAM_APPLET_ICON (object);

        if (icon->priv->dock != NULL) {
                gtk_widget_destroy (icon->priv->dock);
                icon->priv->dock = NULL;
        }

        g_clear_object (&icon->priv->player);
        g_clear_object (&icon->priv->control);
        g_clear_object (&icon->priv->applet_settings);

        G_OBJECT_CLASS (gvc_stream_applet_icon_parent_class)->dispose (object);
}

static void
gvc_stream_applet_icon_class_init (GvcStreamAppletIconClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

        object_class->finalize     = gvc_stream_applet_icon_finalize;
        object_class->dispose      = gvc_stream_applet_icon_dispose;
        object_class->set_property = gvc_stream_applet_icon_set_property;
        object_class->get_property = gvc_stream_applet_icon_get_property;

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

        gtk_widget_class_set_css_name (widget_class, "volume-applet");

        g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
on_applet_icon_visible_notify (GvcStreamAppletIcon *icon)
{
        if (gtk_widget_get_visible (GTK_WIDGET (icon)) == FALSE)
                gtk_widget_hide (icon->priv->dock);
}

static void
on_icon_theme_change (GtkSettings         *settings,
                      GParamSpec          *pspec,
                      GvcStreamAppletIcon *icon)
{
        gvc_stream_applet_icon_set_icon_from_name (icon, icon->priv->icon_names[icon->priv->current_icon]);
}

static void
gvc_stream_applet_icon_init (GvcStreamAppletIcon *icon)
{
        GtkWidget *frame;
        GtkWidget *box;

        icon->priv = gvc_stream_applet_icon_get_instance_private (icon);

        icon->priv->sound_settings = g_settings_new ("org.mate.sound");

        icon->priv->image = GTK_IMAGE (gtk_image_new ());
        gtk_container_add (GTK_CONTAINER (icon), GTK_WIDGET (icon->priv->image));
        gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (icon)), "menu-button"); // icon = volume-applet

        g_signal_connect (GTK_WIDGET (icon),
                          "button-press-event",
                          G_CALLBACK (on_applet_icon_button_press),
                          icon);
        g_signal_connect (GTK_WIDGET (icon),
                          "scroll-event",
                          G_CALLBACK (on_applet_icon_scroll_event),
                          icon);
        g_signal_connect (GTK_WIDGET (icon),
                          "notify::visible",
                          G_CALLBACK (on_applet_icon_visible_notify),
                          NULL);

        /* Create the dock window */
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

        /* Set volume control frame, slider and toplevel window to follow panel theme */
        GtkWidget *toplevel = gtk_widget_get_toplevel (icon->priv->dock);
        GtkStyleContext *context;
        context = gtk_widget_get_style_context (GTK_WIDGET(toplevel));
        gtk_style_context_add_class(context,"mate-panel-applet-slider");

        /* Make transparency possible in gtk3 theme */
        GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(toplevel));
        GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
        gtk_widget_set_visual(GTK_WIDGET(toplevel), visual);

        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
        icon->priv->dock_box = box;

        gtk_container_set_border_width (GTK_CONTAINER (box), 2);
        gtk_container_add (GTK_CONTAINER (frame), box);

        gtk_box_pack_start (GTK_BOX (icon->priv->volume_box), icon->priv->bar, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (icon->priv->volume_box),
                            GTK_WIDGET (icon->priv->volume_image),
                            FALSE,
                            FALSE,
                            0);
        gtk_box_pack_start (GTK_BOX (box), icon->priv->volume_box, TRUE, FALSE, 0);

        g_signal_connect (gtk_settings_get_default (),
                          "notify::gtk-icon-theme-name",
                          G_CALLBACK (on_icon_theme_change),
                          icon);
}

static void
gvc_stream_applet_icon_finalize (GObject *object)
{
        GvcStreamAppletIcon *icon;

        icon = GVC_STREAM_APPLET_ICON (object);

        g_strfreev (icon->priv->icon_names);
        g_clear_pointer (&icon->priv->display_name, g_free);

        g_signal_handlers_disconnect_by_func (gtk_settings_get_default (),
                                              on_icon_theme_change,
                                              icon);

        g_clear_object (&icon->priv->sound_settings);
        g_clear_object (&icon->priv->applet_settings);

        G_OBJECT_CLASS (gvc_stream_applet_icon_parent_class)->finalize (object);
}

GvcStreamAppletIcon *
gvc_stream_applet_icon_new (MateMixerStreamControl *control,
                            const gchar           **icon_names)
{
        return g_object_new (GVC_TYPE_STREAM_APPLET_ICON,
                             "control", control,
                             "icon-names", icon_names,
                             NULL);
}
