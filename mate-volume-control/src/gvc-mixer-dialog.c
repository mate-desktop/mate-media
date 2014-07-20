/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann
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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libmatemixer/matemixer.h>

#include "gvc-channel-bar.h"
#include "gvc-balance-bar.h"
#include "gvc-combo-box.h"
#include "gvc-mixer-dialog.h"
#include "gvc-sound-theme-chooser.h"
#include "gvc-level-bar.h"
#include "gvc-speaker-test.h"
#include "mvc-helpers.h"

#define GVC_MIXER_DIALOG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GVC_TYPE_MIXER_DIALOG, GvcMixerDialogPrivate))

struct _GvcMixerDialogPrivate
{
        MateMixerControl *control;
        GHashTable       *bars;
        GtkWidget        *notebook;
        GtkWidget        *output_bar;
        GtkWidget        *input_bar;
        GtkWidget        *input_level_bar;
        GtkWidget        *effects_bar;
        GtkWidget        *output_stream_box;
        GtkWidget        *sound_effects_box;
        GtkWidget        *hw_box;
        GtkWidget        *hw_treeview;
        GtkWidget        *hw_settings_box;
        GtkWidget        *hw_profile_combo;
        GtkWidget        *input_box;
        GtkWidget        *output_box;
        GtkWidget        *applications_box;
        GtkWidget        *applications_window;
        GtkWidget        *no_apps_label;
        GtkWidget        *output_treeview;
        GtkWidget        *output_settings_frame;
        GtkWidget        *output_settings_box;
        GtkWidget        *output_balance_bar;
        GtkWidget        *output_fade_bar;
        GtkWidget        *output_lfe_bar;
        GtkWidget        *output_port_combo;
        GtkWidget        *input_treeview;
        GtkWidget        *input_port_combo;
        GtkWidget        *input_settings_box;
        GtkWidget        *sound_theme_chooser;
        GtkSizeGroup     *size_group;
        gdouble           last_input_peak;
        guint             num_apps;
};

enum {
        ICON_COLUMN,
        NAME_COLUMN,
        DESCRIPTION_COLUMN,
        ACTIVE_COLUMN,
        SPEAKERS_COLUMN,
        NUM_COLUMNS
};

enum {
        HW_ICON_COLUMN,
        HW_NAME_COLUMN,
        HW_DESCRIPTION_COLUMN,
        HW_STATUS_COLUMN,
        HW_PROFILE_COLUMN,
        HW_NUM_COLUMNS
};

enum {
        PAGE_EFFECTS,
        PAGE_HARDWARE,
        PAGE_INPUT,
        PAGE_OUTPUT,
        PAGE_APPLICATIONS
};

enum {
        PROP_0,
        PROP_CONTROL
};

static const guint tab_accel_keys[] = {
        GDK_KEY_1, GDK_KEY_2, GDK_KEY_3, GDK_KEY_4, GDK_KEY_5
};

static void gvc_mixer_dialog_class_init (GvcMixerDialogClass *klass);
static void gvc_mixer_dialog_init       (GvcMixerDialog      *dialog);
static void gvc_mixer_dialog_finalize   (GObject             *object);

static void bar_set_stream              (GvcMixerDialog      *dialog,
                                         GtkWidget           *bar,
                                         MateMixerStream     *stream);

G_DEFINE_TYPE (GvcMixerDialog, gvc_mixer_dialog, GTK_TYPE_DIALOG)

static gboolean
find_tree_item_by_name (GtkTreeModel *model,
                        const gchar  *name,
                        guint         column,
                        GtkTreeIter  *iter)
{
        gboolean found = FALSE;

        if (!gtk_tree_model_get_iter_first (model, iter))
                return FALSE;

        do {
                gchar *n;

                gtk_tree_model_get (model, iter, column, &n, -1);

                if (!g_strcmp0 (name, n))
                        found = TRUE;

                g_free (n);
        } while (!found && gtk_tree_model_iter_next (model, iter));

        return found;
}

static void
update_default_item (GvcMixerDialog  *dialog,
                     GtkTreeModel    *model,
                     MateMixerStream *stream)
{
        GtkTreeIter  iter;
        const gchar *name = NULL;

        if (gtk_tree_model_get_iter_first (model, &iter) == FALSE)
                return;

        /* The supplied stream is the default, or the selected item. Traverse
         * the item list and mark each item as being selected or not. Also do not
         * presume some known stream is selected and allow NULL here. */
        if (stream != NULL)
                name = mate_mixer_stream_get_name (stream);

        do {
                gchar *n;

                gtk_tree_model_get (model, &iter,
                                    NAME_COLUMN, &n,
                                    -1);
                gtk_list_store_set (GTK_LIST_STORE (model),
                                    &iter,
                                    ACTIVE_COLUMN, !g_strcmp0 (name, n),
                                    -1);
                g_free (n);
        } while (gtk_tree_model_iter_next (model, &iter));
}

static void
on_combo_box_port_changed (GvcComboBox    *combo,
                           const gchar    *name,
                           GvcMixerDialog *dialog)
{
        MateMixerStream *stream;
        MateMixerPort   *port = NULL;
        GList           *ports;

        stream = g_object_get_data (G_OBJECT (combo), "stream");
        if (G_UNLIKELY (stream == NULL)) {
                g_warn_if_reached ();
                return;
        }

        ports = (GList *) mate_mixer_stream_list_ports (stream);
        while (ports) {
                port = MATE_MIXER_PORT (ports->data);

                if (!g_strcmp0 (mate_mixer_port_get_name (port), name))
                        break;

                port  = NULL;
                ports = ports->next;
        }

        if (G_UNLIKELY (port == NULL)) {
                g_warn_if_reached ();
                return;
        }

        g_debug ("Stream port changed to %s for stream %s",
                 name,
                 mate_mixer_stream_get_name (stream));

        mate_mixer_stream_set_active_port (stream, port);
}

static void
on_combo_box_profile_changed (GvcComboBox    *combo,
                              const gchar    *name,
                              GvcMixerDialog *dialog)
{
        MateMixerDevice        *device;
        MateMixerDeviceProfile *profile = NULL;
        GList                  *profiles;

        device = g_object_get_data (G_OBJECT (combo), "device");
        if (G_UNLIKELY (device == NULL)) {
                g_warn_if_reached ();
                return;
        }

        profiles = (GList *) mate_mixer_device_list_profiles (device);
        while (profiles) {
                profile = MATE_MIXER_DEVICE_PROFILE (profiles->data);

                if (!g_strcmp0 (mate_mixer_device_profile_get_name (profile), name))
                        break;

                profile  = NULL;
                profiles = profiles->next;
        }

        if (G_UNLIKELY (profile == NULL)) {
                g_warn_if_reached ();
                return;
        }

        g_debug ("Device profile changed to %s for device %s",
                 name,
                 mate_mixer_device_get_name (device));

        mate_mixer_device_set_active_profile (device, profile);
}

static GtkWidget *
create_port_combo_box (GvcMixerDialog  *dialog,
                       MateMixerStream *stream,
                       const gchar     *name,
                       const GList     *items,
                       const gchar     *active)
{
        GtkWidget *combobox;

        combobox = gvc_combo_box_new (name);

        gvc_combo_box_set_ports (GVC_COMBO_BOX (combobox), items);
        gvc_combo_box_set_active (GVC_COMBO_BOX (combobox), active);

        gvc_combo_box_set_size_group (GVC_COMBO_BOX (combobox),
                                      dialog->priv->size_group,
                                      FALSE);

        g_object_set_data_full (G_OBJECT (combobox),
                                "stream",
                                g_object_ref (stream),
                                g_object_unref);

        g_signal_connect (G_OBJECT (combobox),
                          "changed",
                          G_CALLBACK (on_combo_box_port_changed),
                          dialog);

        return combobox;
}

static void
update_output_settings (GvcMixerDialog *dialog)
{
        MateMixerStream      *stream;
        MateMixerStreamFlags  flags;
        const GList          *ports;
        gboolean              has_settings = FALSE;

        g_debug ("Updating output settings");

        if (dialog->priv->output_balance_bar != NULL) {
                gtk_container_remove (GTK_CONTAINER (dialog->priv->output_settings_box),
                                      dialog->priv->output_balance_bar);

                dialog->priv->output_balance_bar = NULL;
        }
        if (dialog->priv->output_fade_bar != NULL) {
                gtk_container_remove (GTK_CONTAINER (dialog->priv->output_settings_box),
                                      dialog->priv->output_fade_bar);

                dialog->priv->output_fade_bar = NULL;
        }
        if (dialog->priv->output_lfe_bar != NULL) {
                gtk_container_remove (GTK_CONTAINER (dialog->priv->output_settings_box),
                                      dialog->priv->output_lfe_bar);

                dialog->priv->output_lfe_bar = NULL;
        }
        if (dialog->priv->output_port_combo != NULL) {
                gtk_container_remove (GTK_CONTAINER (dialog->priv->output_settings_box),
                                      dialog->priv->output_port_combo);

                dialog->priv->output_port_combo = NULL;
        }

        stream = mate_mixer_control_get_default_output_stream (dialog->priv->control);
        if (stream == NULL) {
                g_debug ("There is no default output stream - output settings disabled");
                gtk_widget_hide (dialog->priv->output_settings_frame);
                return;
        }

        flags = mate_mixer_stream_get_flags (stream);

        /* Enable balance bar if stream feature is available */
        if (flags & MATE_MIXER_STREAM_CAN_BALANCE) {
                dialog->priv->output_balance_bar =
                        gvc_balance_bar_new (stream, BALANCE_TYPE_RL);

                gvc_balance_bar_set_size_group (GVC_BALANCE_BAR (dialog->priv->output_balance_bar),
                                                dialog->priv->size_group,
                                                TRUE);

                gtk_box_pack_start (GTK_BOX (dialog->priv->output_settings_box),
                                    dialog->priv->output_balance_bar,
                                    FALSE, FALSE, 6);

                gtk_widget_show (dialog->priv->output_balance_bar);
                has_settings = TRUE;
        }

        /* Enable fade bar if stream feature is available */
        if (flags & MATE_MIXER_STREAM_CAN_FADE) {
                dialog->priv->output_fade_bar =
                        gvc_balance_bar_new (stream, BALANCE_TYPE_FR);

                gvc_balance_bar_set_size_group (GVC_BALANCE_BAR (dialog->priv->output_fade_bar),
                                                dialog->priv->size_group,
                                                TRUE);

                gtk_box_pack_start (GTK_BOX (dialog->priv->output_settings_box),
                                    dialog->priv->output_fade_bar,
                                    FALSE, FALSE, 6);

                gtk_widget_show (dialog->priv->output_fade_bar);
                has_settings = TRUE;
        }

        /* Enable subwoofer volume bar if subwoofer is available */
        if (mate_mixer_stream_has_channel_position (stream, MATE_MIXER_CHANNEL_LFE)) {
                dialog->priv->output_lfe_bar =
                        gvc_balance_bar_new (stream, BALANCE_TYPE_LFE);

                gvc_balance_bar_set_size_group (GVC_BALANCE_BAR (dialog->priv->output_lfe_bar),
                                                dialog->priv->size_group,
                                                TRUE);

                gtk_box_pack_start (GTK_BOX (dialog->priv->output_settings_box),
                                    dialog->priv->output_lfe_bar,
                                    FALSE, FALSE, 6);

                gtk_widget_show (dialog->priv->output_lfe_bar);
                has_settings = TRUE;
        }

        ports = mate_mixer_stream_list_ports (stream);
        if (ports != NULL) {
                MateMixerPort *port;

                port = mate_mixer_stream_get_active_port (stream);
                if (G_UNLIKELY (port == NULL)) {
                        /* Select the first port if none is selected at the moment */
                        port = MATE_MIXER_PORT (ports->data);
                        mate_mixer_stream_set_active_port (stream, port);
                }

                dialog->priv->output_port_combo =
                        create_port_combo_box (dialog,
                                               stream,
                                               _("Co_nnector:"),
                                               ports,
                                               mate_mixer_port_get_name (port));

                gtk_box_pack_start (GTK_BOX (dialog->priv->output_settings_box),
                                    dialog->priv->output_port_combo,
                                    TRUE, FALSE, 6);

                gtk_widget_show (dialog->priv->output_port_combo);
                has_settings = TRUE;
        }

        if (has_settings == TRUE)
                gtk_widget_show (dialog->priv->output_settings_frame);
        else
                gtk_widget_hide (dialog->priv->output_settings_frame);
}

static void
on_control_default_output_notify (MateMixerControl *control,
                                  GParamSpec       *pspec,
                                  GvcMixerDialog   *dialog)
{
        GtkTreeModel    *model;
        MateMixerStream *stream;

        stream = mate_mixer_control_get_default_output_stream (control);
        if (stream != NULL) {
                GList *streams;

                streams = (GList *) mate_mixer_control_list_cached_streams (dialog->priv->control);

                /* Move all cached stream to the newly selected default stream */
                while (streams) {
                        MateMixerStream       *parent;
                        MateMixerClientStream *client = MATE_MIXER_CLIENT_STREAM (streams->data);

                        parent = mate_mixer_client_stream_get_parent (client);

                        if (parent != stream) {
                                MateMixerStreamFlags flags;

                                flags = mate_mixer_stream_get_flags (MATE_MIXER_STREAM (client));

                                if (flags & MATE_MIXER_STREAM_OUTPUT)
                                        mate_mixer_client_stream_set_parent (client, stream);
                        }
                        streams = streams->next;
                }
        }

        bar_set_stream (dialog, dialog->priv->output_bar, stream);

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->output_treeview));
        update_default_item (dialog, model, stream);

        update_output_settings (dialog);
}

#define DECAY_STEP .15

static void
on_stream_monitor_value (MateMixerStream *stream,
                         gdouble          value,
                         GvcMixerDialog  *dialog)
{
        GtkAdjustment *adj;

        if (dialog->priv->last_input_peak >= DECAY_STEP) {
                if (value < dialog->priv->last_input_peak - DECAY_STEP) {
                        value = dialog->priv->last_input_peak - DECAY_STEP;
                }
        }

        dialog->priv->last_input_peak = value;

        adj = gvc_level_bar_get_peak_adjustment (GVC_LEVEL_BAR (dialog->priv->input_level_bar));
        if (value >= 0)
                gtk_adjustment_set_value (adj, value);
        else
                gtk_adjustment_set_value (adj, 0.0);
}

static void
update_input_settings (GvcMixerDialog *dialog)
{
        MateMixerStream     *stream;
        MateMixerStreamFlags flags;
        const GList         *ports;

        g_debug ("Updating input settings");

        if (dialog->priv->input_port_combo != NULL) {
                gtk_container_remove (GTK_CONTAINER (dialog->priv->input_settings_box),
                                      dialog->priv->input_port_combo);

                dialog->priv->input_port_combo = NULL;
        }

        stream = mate_mixer_control_get_default_input_stream (dialog->priv->control);
        if (stream == NULL) {
                g_debug ("There is no default input stream");
                return;
        }

        flags = mate_mixer_stream_get_flags (stream);

        /* Enable level bar only if supported by the stream */
        if (flags & MATE_MIXER_STREAM_HAS_MONITOR) {
                mate_mixer_stream_monitor_set_name (stream, _("Peak detect"));

                g_signal_connect (G_OBJECT (stream),
                                  "monitor-value",
                                  G_CALLBACK (on_stream_monitor_value),
                                  dialog);
        }

        ports = mate_mixer_stream_list_ports (stream);
        if (ports != NULL) {
                MateMixerPort *port;

                port = mate_mixer_stream_get_active_port (stream);
                if (G_UNLIKELY (port == NULL)) {
                        /* Select the first port if none is selected at the moment */
                        port = MATE_MIXER_PORT (ports->data);
                        mate_mixer_stream_set_active_port (stream, port);
                }

                dialog->priv->input_port_combo =
                        create_port_combo_box (dialog,
                                               stream,
                                               _("Co_nnector:"),
                                               ports,
                                               mate_mixer_port_get_name (port));

                gtk_box_pack_start (GTK_BOX (dialog->priv->input_settings_box),
                                    dialog->priv->input_port_combo,
                                    TRUE, TRUE, 0);

                gtk_widget_show (dialog->priv->input_port_combo);
        }
}

static void
on_control_default_input_notify (MateMixerControl *control,
                                 GParamSpec       *pspec,
                                 GvcMixerDialog   *dialog)
{
        GtkTreeModel    *model;
        MateMixerStream *stream;
        MateMixerStream *current;

        g_debug ("Default input stream has changed");

        current = gvc_channel_bar_get_stream (GVC_CHANNEL_BAR (dialog->priv->input_bar));
        if (current != NULL) {
                /* Make sure to disable monitoring of the removed stream */
                g_signal_handlers_disconnect_by_func (G_OBJECT (current),
                                                      G_CALLBACK (on_stream_monitor_value),
                                                      dialog);

                mate_mixer_stream_monitor_stop (current);
        }

        stream = mate_mixer_control_get_default_input_stream (control);
        if (stream != NULL) {
                GList *streams;
                guint  page = gtk_notebook_get_current_page (GTK_NOTEBOOK (dialog->priv->notebook));

                streams = (GList *) mate_mixer_control_list_cached_streams (dialog->priv->control);

                /* Move all cached stream to the newly selected default stream */
                while (streams) {
                        MateMixerStream       *parent;
                        MateMixerClientStream *client = MATE_MIXER_CLIENT_STREAM (streams->data);

                        parent = mate_mixer_client_stream_get_parent (client);

                        if (parent != stream) {
                                MateMixerStreamFlags flags;

                                flags = mate_mixer_stream_get_flags (MATE_MIXER_STREAM (client));

                                if (flags & MATE_MIXER_STREAM_INPUT)
                                        mate_mixer_client_stream_set_parent (client, stream);
                        }
                        streams = streams->next;
                }

                if (page == PAGE_INPUT)
                        mate_mixer_stream_monitor_start (stream);
        }

        bar_set_stream (dialog, dialog->priv->input_bar, stream);

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->input_treeview));
        update_default_item (dialog, model, stream);

        update_input_settings (dialog);
}

static GvcComboBox *
find_combo_box_by_stream (GvcMixerDialog *dialog, MateMixerStream *stream)
{
        MateMixerStream *combo_stream;

        if (dialog->priv->output_port_combo != NULL) {
                combo_stream = g_object_get_data (G_OBJECT (dialog->priv->output_port_combo),
                                                  "stream");
                if (combo_stream == stream)
                        return GVC_COMBO_BOX (dialog->priv->output_port_combo);
        }

        if (dialog->priv->input_port_combo != NULL) {
                combo_stream = g_object_get_data (G_OBJECT (dialog->priv->input_port_combo),
                                                  "stream");
                if (combo_stream == stream)
                        return GVC_COMBO_BOX (dialog->priv->input_port_combo);
        }
        return NULL;
}

static void
on_stream_description_notify (MateMixerStream *stream,
                              GParamSpec      *pspec,
                              GvcMixerDialog  *dialog)
{
        GtkTreeModel        *model;
        GtkTreeIter          iter;
        MateMixerStreamFlags flags;

        flags = mate_mixer_stream_get_flags (stream);

        if (flags & MATE_MIXER_STREAM_INPUT)
                model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->input_treeview));
        else
                model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->output_treeview));

        if (find_tree_item_by_name (model,
                                    mate_mixer_stream_get_name (stream),
                                    NAME_COLUMN,
                                    &iter) == TRUE) {
                const gchar *description;

                description = mate_mixer_stream_get_description (stream);
                gtk_list_store_set (GTK_LIST_STORE (model),
                                    &iter,
                                    DESCRIPTION_COLUMN, description,
                                    -1);
        }
}

static void
on_stream_port_notify (MateMixerStream *stream,
                       GParamSpec      *pspec,
                       GvcMixerDialog  *dialog)
{
        GvcComboBox   *combobox;
        MateMixerPort *port;

        combobox = find_combo_box_by_stream (dialog, stream);
        if (G_UNLIKELY (combobox == NULL)) {
                g_warn_if_reached ();
                return;
        }

        g_debug ("Stream %s port has changed",
                 mate_mixer_stream_get_name (stream));

        g_signal_handlers_block_by_func (G_OBJECT (combobox),
                                         on_combo_box_port_changed,
                                         dialog);

        port = mate_mixer_stream_get_active_port (stream);
        if (G_LIKELY (port != NULL)) {
                const gchar *name = mate_mixer_port_get_name (port);

                g_debug ("The current port is %s", name);

                gvc_combo_box_set_active (GVC_COMBO_BOX (combobox), name);
        }

        g_signal_handlers_unblock_by_func (G_OBJECT (combobox),
                                           on_combo_box_port_changed,
                                           dialog);
}

static void
on_stream_mute_notify (MateMixerStream *stream,
                       GParamSpec      *pspec,
                       GvcMixerDialog  *dialog)
{
        MateMixerStream *input;

        input = mate_mixer_control_get_default_input_stream (dialog->priv->control);

        /* Stop monitoring the input stream when it gets muted */
        if (stream == input) {
                if (mate_mixer_stream_get_mute (stream))
                        mate_mixer_stream_monitor_stop (stream);
                else
                        mate_mixer_stream_monitor_start (stream);
        }
}

static GtkWidget *
create_bar (GvcMixerDialog *dialog, gboolean use_size_group, gboolean symmetric)
{
        GtkWidget *bar;

        bar = gvc_channel_bar_new (NULL);

        if (use_size_group == TRUE)
                gvc_channel_bar_set_size_group (GVC_CHANNEL_BAR (bar),
                                                dialog->priv->size_group,
                                                symmetric);

        g_object_set (G_OBJECT (bar),
                      "orientation", GTK_ORIENTATION_HORIZONTAL,
                      "show-mute",   TRUE,
                      "show-icons",  TRUE,
                      "show-marks",  TRUE,
                      "extended",    TRUE, NULL);
        return bar;
}

static void
bar_set_stream (GvcMixerDialog  *dialog,
                GtkWidget       *bar,
                MateMixerStream *stream)
{
        MateMixerStream *previous;

        previous = gvc_channel_bar_get_stream (GVC_CHANNEL_BAR (bar));
        if (previous != NULL) {
                const gchar *name = mate_mixer_stream_get_name (previous);

                g_debug ("Disconnecting old stream %s", name);

                g_signal_handlers_disconnect_by_func (previous,
                                                      on_stream_mute_notify,
                                                      dialog);
                g_signal_handlers_disconnect_by_func (previous,
                                                      on_stream_port_notify,
                                                      dialog);

                if (mate_mixer_stream_get_flags (previous) & MATE_MIXER_STREAM_INPUT)
                        mate_mixer_stream_monitor_stop (previous);

                g_hash_table_remove (dialog->priv->bars, name);
        }

        gvc_channel_bar_set_stream (GVC_CHANNEL_BAR (bar), stream);

        if (stream != NULL) {
                const gchar *name = mate_mixer_stream_get_name (stream);

                g_debug ("Connecting new stream %s", name);

                g_signal_connect (stream,
                                  "notify::mute",
                                  G_CALLBACK (on_stream_mute_notify),
                                  dialog);
                g_signal_connect (stream,
                                  "notify::active-port",
                                  G_CALLBACK (on_stream_port_notify),
                                  dialog);

                g_hash_table_insert (dialog->priv->bars, (gpointer) name, bar);
        }

        gtk_widget_set_sensitive (GTK_WIDGET (bar), (stream != NULL));
}

static void
add_stream (GvcMixerDialog *dialog, MateMixerStream *stream)
{
        GtkWidget           *bar = NULL;
        MateMixerStreamFlags flags;

        flags = mate_mixer_stream_get_flags (stream);

        if (MATE_MIXER_IS_CLIENT_STREAM (stream)) {
                MateMixerClientStream     *client ;
                MateMixerClientStreamFlags client_flags;
                MateMixerClientStreamRole  client_role;
                const gchar *name;
                const gchar *icon;

                client       = MATE_MIXER_CLIENT_STREAM (stream);
                client_flags = mate_mixer_client_stream_get_flags (client);
                client_role  = mate_mixer_client_stream_get_role (client);

                /* We use a cached event stream for the effects volume slider,
                 * because regular streams are only created when an event sound
                 * is played and then immediately destroyed.
                 * The cached event stream should exist all the time. */
                if (client_flags & MATE_MIXER_CLIENT_STREAM_CACHED &&
                    client_role == MATE_MIXER_CLIENT_STREAM_ROLE_EVENT) {
                        g_debug ("Found cached event role stream %s",
                                 mate_mixer_stream_get_name (stream));

                        bar = dialog->priv->effects_bar;
                }

                /* Add stream to the applications page, but make sure the stream
                 * qualifies for the inclusion */
                if (client_flags & MATE_MIXER_CLIENT_STREAM_APPLICATION) {
                        const gchar *app_id;

                        /* Skip streams with roles we don't care about */
                        if (client_role == MATE_MIXER_CLIENT_STREAM_ROLE_EVENT ||
                            client_role == MATE_MIXER_CLIENT_STREAM_ROLE_TEST ||
                            client_role == MATE_MIXER_CLIENT_STREAM_ROLE_ABSTRACT ||
                            client_role == MATE_MIXER_CLIENT_STREAM_ROLE_FILTER)
                                return;

                        app_id = mate_mixer_client_stream_get_app_id (client);

                        /* These applications may have associated streams because
                         * they do peak level monitoring, skip these too */
                        if (!g_strcmp0 (app_id, "org.mate.VolumeControl") ||
                            !g_strcmp0 (app_id, "org.gnome.VolumeControl") ||
                            !g_strcmp0 (app_id, "org.PulseAudio.pavucontrol"))
                                return;

                        name = mate_mixer_client_stream_get_app_name (client);
                        if (name == NULL)
                                name = mate_mixer_stream_get_description (stream);
                        if (name == NULL)
                                name = mate_mixer_stream_get_name (stream);
                        if (G_UNLIKELY (name == NULL))
                                return;

                        bar = create_bar (dialog, FALSE, FALSE);

                        g_object_set (G_OBJECT (bar),
                                      "show-marks", FALSE,
                                      "extended", FALSE,
                                      NULL);

                        /* By default channel bars use speaker icons, use microphone
                         * icons instead for recording applications */
                        if (flags & MATE_MIXER_STREAM_INPUT)
                                g_object_set (G_OBJECT (bar),
                                              "low-icon-name", "audio-input-microphone-low",
                                              "high-icon-name", "audio-input-microphone-high",
                                              NULL);

                        icon = mate_mixer_client_stream_get_app_icon (client);
                        if (icon == NULL) {
                                if (flags & MATE_MIXER_STREAM_INPUT)
                                        icon = "audio-input-microphone";
                                else
                                        icon = "applications-multimedia";
                        }

                        gvc_channel_bar_set_name (GVC_CHANNEL_BAR (bar), name);
                        gvc_channel_bar_set_icon_name (GVC_CHANNEL_BAR (bar), icon);

                        gtk_box_pack_start (GTK_BOX (dialog->priv->applications_box),
                                            bar,
                                            FALSE, FALSE, 12);

                        gtk_widget_hide (dialog->priv->no_apps_label);
                        dialog->priv->num_apps++;
                }
        } else {
                GtkTreeModel    *model = NULL;
                GtkTreeIter      iter;
                MateMixerStream *input;
                MateMixerStream *output;
                const gchar     *speakers = NULL;
                gboolean         is_default = FALSE;

                input  = mate_mixer_control_get_default_input_stream (dialog->priv->control);
                output = mate_mixer_control_get_default_output_stream (dialog->priv->control);

                if (flags & MATE_MIXER_STREAM_INPUT) {
                        if (stream == input) {
                                bar = dialog->priv->input_bar;

                                update_input_settings (dialog);
                                is_default = TRUE;
                        }

                        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->input_treeview));
                }
                else if (flags & MATE_MIXER_STREAM_OUTPUT) {
                        if (stream == output) {
                                bar = dialog->priv->output_bar;

                                update_output_settings (dialog);
                                is_default = TRUE;
                        }

                        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->output_treeview));
                        speakers = mvc_channel_map_to_pretty_string (stream);
                }

                if (model != NULL) {
                        const gchar *name;
                        const gchar *description;

                        name = mate_mixer_stream_get_name (stream);
                        description = mate_mixer_stream_get_description (stream);

                        gtk_list_store_append (GTK_LIST_STORE (model), &iter);
                        gtk_list_store_set (GTK_LIST_STORE (model),
                                            &iter,
                                            NAME_COLUMN, name,
                                            DESCRIPTION_COLUMN, description,
                                            ACTIVE_COLUMN, is_default,
                                            SPEAKERS_COLUMN, speakers,
                                            -1);

                        g_signal_connect (stream,
                                          "notify::description",
                                          G_CALLBACK (on_stream_description_notify),
                                          dialog);
                }
        }

        if (bar != NULL) {
                bar_set_stream (dialog, bar, stream);
                gtk_widget_show (bar);
        }
}

static MateMixerStream *
find_device_test_stream (GvcMixerDialog *dialog, MateMixerDevice *device)
{
        GList *streams;

        streams = (GList *) mate_mixer_control_list_streams (dialog->priv->control);
        while (streams) {
                MateMixerStream *stream;

                stream = MATE_MIXER_STREAM (streams->data);

                if (mate_mixer_stream_get_device (stream) == device &&
                    mate_mixer_stream_get_num_channels (stream) >= 1) {
                        MateMixerStreamFlags flags;

                        flags = mate_mixer_stream_get_flags (stream);

                        if ((flags & (MATE_MIXER_STREAM_OUTPUT |
                                      MATE_MIXER_STREAM_CLIENT)) == MATE_MIXER_STREAM_OUTPUT)
                                return stream;
                }
                streams = streams->next;
        }
        return FALSE;
}

static void
update_device_test_visibility (GvcMixerDialog *dialog)
{
        MateMixerDevice *device;
        MateMixerStream *stream;

        device = g_object_get_data (G_OBJECT (dialog->priv->hw_profile_combo), "device");
        if (G_UNLIKELY (device == NULL)) {
                g_warn_if_reached ();
                return;
        }

        stream = find_device_test_stream (dialog, device);

        g_object_set (G_OBJECT (dialog->priv->hw_profile_combo),
                      "show-button", (stream != NULL),
                      NULL);
}

static void
on_control_stream_added (MateMixerControl *control,
                         const gchar      *name,
                         GvcMixerDialog   *dialog)
{
        MateMixerStream     *stream;
        MateMixerStreamFlags flags;
        GtkWidget           *bar;

        stream = mate_mixer_control_get_stream (control, name);
        if (G_UNLIKELY (stream == NULL))
                return;

        flags = mate_mixer_stream_get_flags (stream);

        /* If the newly added stream belongs to the currently selected device and
         * the test button is hidden, this stream may be the one to allow the
         * sound test and therefore we may need to enable the button */
        if (dialog->priv->hw_profile_combo != NULL &&
            ((flags & (MATE_MIXER_STREAM_OUTPUT |
                       MATE_MIXER_STREAM_CLIENT)) == MATE_MIXER_STREAM_OUTPUT)) {
                MateMixerDevice *device1;
                MateMixerDevice *device2;

                device1 = mate_mixer_stream_get_device (stream);
                device2 = g_object_get_data (G_OBJECT (dialog->priv->hw_profile_combo),
                                             "device");

                if (device1 == device2) {
                        gboolean show_button;

                        g_object_get (G_OBJECT (dialog->priv->hw_profile_combo),
                                      "show-button", &show_button,
                                      NULL);

                        if (show_button == FALSE)
                                update_device_test_visibility (dialog);
                }
        }

        bar = g_hash_table_lookup (dialog->priv->bars, name);
        if (G_UNLIKELY (bar != NULL))
                return;

        add_stream (dialog, stream);
}

static void
remove_stream (GvcMixerDialog *dialog, const gchar *name)
{
        GtkWidget    *bar;
        GtkTreeIter   iter;
        GtkTreeModel *model;

        /* Remove bars for applications and reset fixed bars */
        bar = g_hash_table_lookup (dialog->priv->bars, name);

        if (bar == dialog->priv->input_bar || bar == dialog->priv->output_bar) {
                g_debug ("Removing stream %s from bar %s",
                         name,
                         gvc_channel_bar_get_name (GVC_CHANNEL_BAR (bar)));

                bar_set_stream (dialog, bar, NULL);
        } else if (bar != NULL) {
                g_debug ("Removing application stream %s", name);

                g_hash_table_remove (dialog->priv->bars, name);
                gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (bar)), bar);

                if (G_UNLIKELY (dialog->priv->num_apps <= 0)) {
                        g_warn_if_reached ();
                        dialog->priv->num_apps = 1;
                }

                if (--dialog->priv->num_apps == 0)
                        gtk_widget_show (dialog->priv->no_apps_label);
        }

        /* Remove from any models */
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->output_treeview));
        if (find_tree_item_by_name (GTK_TREE_MODEL (model),
                                    name,
                                    NAME_COLUMN,
                                    &iter) == TRUE)
                gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->input_treeview));
        if (find_tree_item_by_name (GTK_TREE_MODEL (model),
                                    name,
                                    NAME_COLUMN,
                                    &iter) == TRUE)
                gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
on_control_stream_removed (MateMixerControl *control,
                           const gchar      *name,
                           GvcMixerDialog   *dialog)
{

        if (dialog->priv->hw_profile_combo != NULL) {
                gboolean show_button;

                g_object_get (G_OBJECT (dialog->priv->hw_profile_combo),
                              "show-button", &show_button,
                              NULL);

                if (show_button == TRUE)
                        update_device_test_visibility (dialog);
        }

        remove_stream (dialog, name);
}

static void
on_control_cached_stream_removed (MateMixerControl *control,
                                  const gchar      *name,
                                  GvcMixerDialog   *dialog)
{
        GtkWidget *bar;

        bar = g_hash_table_lookup (dialog->priv->bars, name);

        if (bar != NULL) {
                /* We only use a cached stream in the effects bar */
                if (G_UNLIKELY (bar != dialog->priv->effects_bar)) {
                        g_warn_if_reached ();
                        return;
                }

                bar_set_stream (dialog, bar, NULL);
        }
}

static gchar *
device_profile_status (MateMixerDeviceProfile *profile)
{
        guint  inputs;
        guint  outputs;
        gchar *inputs_str = NULL;
        gchar *outputs_str = NULL;

        inputs  = mate_mixer_device_profile_get_num_input_streams (profile);
        outputs = mate_mixer_device_profile_get_num_output_streams (profile);

        if (inputs == 0 && outputs == 0) {
                /* translators:
                 * The device has been disabled */
                return g_strdup (_("Disabled"));
        }

        if (outputs > 0) {
                /* translators:
                 * The number of sound outputs on a particular device */
                outputs_str = g_strdup_printf (ngettext ("%u Output",
                                                         "%u Outputs",
                                                          outputs),
                                               outputs);
        }

        if (inputs > 0) {
                /* translators:
                 * The number of sound inputs on a particular device */
                inputs_str = g_strdup_printf (ngettext ("%u Input",
                                                        "%u Inputs",
                                                        inputs),
                                              inputs);
        }

        if (inputs_str != NULL && outputs_str != NULL) {
                gchar *ret = g_strdup_printf ("%s / %s",
                                              outputs_str,
                                              inputs_str);
                g_free (outputs_str);
                g_free (inputs_str);
                return ret;
        }

        if (inputs_str != NULL)
                return inputs_str;

        return outputs_str;
}

static void
update_device_info (GvcMixerDialog *dialog, MateMixerDevice *device)
{
        GtkTreeModel           *model = NULL;
        GtkTreeIter             iter;
        MateMixerDeviceProfile *profile;
        const gchar            *description;
        const gchar            *profile_description = NULL;
        gchar                  *profile_status = NULL;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->hw_treeview));

        if (find_tree_item_by_name (model,
                                    mate_mixer_device_get_name (device),
                                    HW_NAME_COLUMN,
                                    &iter) == FALSE)
                return;

        description = mate_mixer_device_get_description (device);

        profile = mate_mixer_device_get_active_profile (device);
        if (profile != NULL) {
                profile_description = mate_mixer_device_profile_get_description (profile);
                profile_status = device_profile_status (profile);
        }

        gtk_list_store_set (GTK_LIST_STORE (model),
                            &iter,
                            HW_DESCRIPTION_COLUMN, description,
                            HW_PROFILE_COLUMN, profile_description,
                            HW_STATUS_COLUMN, profile_status,
                            -1);

        g_free (profile_status);
}

static void
on_device_profile_notify (MateMixerDevice *device,
                          GParamSpec      *pspec,
                          GvcMixerDialog  *dialog)
{
        MateMixerDeviceProfile *profile;

        g_debug ("Device profile has changed");

        g_signal_handlers_block_by_func (G_OBJECT (dialog->priv->hw_profile_combo),
                                         G_CALLBACK (on_combo_box_profile_changed),
                                         dialog);

        profile = mate_mixer_device_get_active_profile (device);
        if (G_LIKELY (profile != NULL)) {
                const gchar *name = mate_mixer_device_profile_get_name (profile);

                g_debug ("The current profile is %s", name);

                gvc_combo_box_set_active (GVC_COMBO_BOX (dialog->priv->hw_profile_combo),
                                          name);
        }

        g_signal_handlers_unblock_by_func (G_OBJECT (dialog->priv->hw_profile_combo),
                                           G_CALLBACK (on_combo_box_profile_changed),
                                           dialog);

        update_device_info (dialog, device);
}

static void
add_device (GvcMixerDialog *dialog, MateMixerDevice *device)
{
        GtkTreeModel           *model;
        GtkTreeIter             iter;
        MateMixerDeviceProfile *profile;
        GIcon                  *icon;
        const gchar            *name;
        const gchar            *description;
        const gchar            *profile_description = NULL;
        gchar                  *profile_status = NULL;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->hw_treeview));

        name = mate_mixer_device_get_name (device);
        description = mate_mixer_device_get_description (device);

        if (find_tree_item_by_name (GTK_TREE_MODEL (model),
                                    name,
                                    HW_NAME_COLUMN,
                                    &iter) == FALSE)
                gtk_list_store_append (GTK_LIST_STORE (model), &iter);

        icon = g_themed_icon_new_with_default_fallbacks (mate_mixer_device_get_icon (device));

        profile = mate_mixer_device_get_active_profile (device);
        if (profile != NULL) {
                profile_description = mate_mixer_device_profile_get_description (profile);
                profile_status = device_profile_status (profile);
        }

        gtk_list_store_set (GTK_LIST_STORE (model),
                            &iter,
                            HW_NAME_COLUMN, name,
                            HW_DESCRIPTION_COLUMN, description,
                            HW_ICON_COLUMN, icon,
                            HW_PROFILE_COLUMN, profile_description,
                            HW_STATUS_COLUMN, profile_status,
                            -1);

        g_free (profile_status);

        g_signal_connect (device,
                          "notify::active-profile",
                          G_CALLBACK (on_device_profile_notify),
                          dialog);
}

static void
on_control_device_added (MateMixerControl *control, const gchar *name, GvcMixerDialog *dialog)
{
        MateMixerDevice *device;

        device = mate_mixer_control_get_device (control, name);
        if (G_UNLIKELY (device == NULL))
                return;

        add_device (dialog, device);
}

static void
on_control_device_removed (MateMixerControl *control,
                           const gchar      *name,
                           GvcMixerDialog   *dialog)
{
        GtkTreeIter   iter;
        GtkTreeModel *model;

        /* Remove from any models */
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->hw_treeview));

        if (find_tree_item_by_name (GTK_TREE_MODEL (model),
                                    name,
                                    HW_NAME_COLUMN,
                                    &iter) == TRUE)
                gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
make_label_bold (GtkLabel *label)
{
        PangoFontDescription *font_desc;

        font_desc = pango_font_description_new ();

        pango_font_description_set_weight (font_desc, PANGO_WEIGHT_BOLD);

        /* This will only affect the weight of the font, the rest is
         * from the current state of the widget, which comes from the
         * theme or user prefs, since the font desc only has the
         * weight flag turned on. */
#if GTK_CHECK_VERSION (3, 0, 0)
        gtk_widget_override_font (GTK_WIDGET (label), font_desc);
#else
        gtk_widget_modify_font (GTK_WIDGET (label), font_desc);
#endif
        pango_font_description_free (font_desc);
}

static void
on_input_radio_toggled (GtkCellRendererToggle *renderer,
                        gchar                 *path_str,
                        GvcMixerDialog        *dialog)
{
        GtkTreeModel *model;
        GtkTreeIter   iter;
        GtkTreePath  *path;
        gboolean      toggled = FALSE;
        gchar        *name = NULL;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->input_treeview));
        path  = gtk_tree_path_new_from_string (path_str);

        gtk_tree_model_get_iter (model, &iter, path);
        gtk_tree_path_free (path);

        gtk_tree_model_get (model, &iter,
                            NAME_COLUMN, &name,
                            ACTIVE_COLUMN, &toggled,
                            -1);
        if (toggled ^ 1) {
                MateMixerStream *stream;

                stream = mate_mixer_control_get_stream (dialog->priv->control, name);
                if (G_UNLIKELY (stream == NULL)) {
                        g_warn_if_reached ();
                        g_free (name);
                        return;
                }

                g_debug ("Default input stream selection changed to %s", name);
                mate_mixer_control_set_default_input_stream (dialog->priv->control, stream);
        }
        g_free (name);
}

static void
on_output_radio_toggled (GtkCellRendererToggle *renderer,
                         gchar                 *path_str,
                         GvcMixerDialog        *dialog)
{
        GtkTreeModel *model;
        GtkTreeIter   iter;
        GtkTreePath  *path;
        gboolean      toggled = FALSE;
        gchar        *name = NULL;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->output_treeview));
        path  = gtk_tree_path_new_from_string (path_str);

        gtk_tree_model_get_iter (model, &iter, path);
        gtk_tree_path_free (path);

        gtk_tree_model_get (model, &iter,
                            NAME_COLUMN, &name,
                            ACTIVE_COLUMN, &toggled,
                            -1);
        if (toggled ^ 1) {
                MateMixerStream *stream;

                stream = mate_mixer_control_get_stream (dialog->priv->control, name);
                if (G_UNLIKELY (stream == NULL)) {
                        g_warn_if_reached ();
                        g_free (name);
                        return;
                }

                g_debug ("Default output stream selection changed to %s", name);
                mate_mixer_control_set_default_output_stream (dialog->priv->control, stream);
        }
        g_free (name);
}

static void
stream_name_to_text (GtkTreeViewColumn *column,
                     GtkCellRenderer   *cell,
                     GtkTreeModel      *model,
                     GtkTreeIter       *iter,
                     gpointer           user_data)
{
        gchar *description;
        gchar *speakers;

        gtk_tree_model_get (model, iter,
                            DESCRIPTION_COLUMN, &description,
                            SPEAKERS_COLUMN, &speakers,
                            -1);

        if (speakers != NULL) {
                gchar *str = g_strdup_printf ("%s\n<i>%s</i>",
                                              description,
                                              speakers);

                g_object_set (cell, "markup", str, NULL);
                g_free (str);
                g_free (speakers);
        } else {
                g_object_set (cell, "text", description, NULL);
        }

        g_free (description);
}

static gint
compare_stream_treeview_items (GtkTreeModel *model,
                               GtkTreeIter  *a,
                               GtkTreeIter  *b,
                               gpointer      user_data)
{
        gchar *desc_a = NULL;
        gchar *desc_b = NULL;
        gint   result;

        gtk_tree_model_get (model, a,
                            DESCRIPTION_COLUMN, &desc_a,
                            -1);
        gtk_tree_model_get (model, b,
                            DESCRIPTION_COLUMN, &desc_b,
                            -1);

        result = g_ascii_strcasecmp (desc_a, desc_b);

        g_free (desc_a);
        g_free (desc_b);
        return result;
}

static GtkWidget *
create_stream_treeview (GvcMixerDialog *dialog, GCallback on_toggled)
{
        GtkWidget         *treeview;
        GtkListStore      *store;
        GtkCellRenderer   *renderer;
        GtkTreeViewColumn *column;

        treeview = gtk_tree_view_new ();
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);

        store = gtk_list_store_new (NUM_COLUMNS,
                                    G_TYPE_ICON,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_BOOLEAN,
                                    G_TYPE_STRING);

        gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
                                 GTK_TREE_MODEL (store));

        renderer = gtk_cell_renderer_toggle_new ();
        gtk_cell_renderer_toggle_set_radio (GTK_CELL_RENDERER_TOGGLE (renderer), TRUE);

        column = gtk_tree_view_column_new_with_attributes (NULL,
                                                           renderer,
                                                           "active", ACTIVE_COLUMN,
                                                           NULL);

        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

        g_signal_connect (G_OBJECT (renderer),
                          "toggled",
                          G_CALLBACK (on_toggled),
                          dialog);

        gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (treeview), -1,
                                                    _("Name"),
                                                    gtk_cell_renderer_text_new (),
                                                    stream_name_to_text,
                                                    NULL, NULL);

        /* Keep the list of streams sorted by the name */
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                              DESCRIPTION_COLUMN,
                                              GTK_SORT_ASCENDING);

        gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store),
                                         DESCRIPTION_COLUMN,
                                         compare_stream_treeview_items,
                                         NULL, NULL);
        return treeview;
}

static void
on_test_speakers_clicked (GvcComboBox *widget, GvcMixerDialog *dialog)
{
        GtkWidget       *d,
                        *test,
                        *container;
        gchar           *title;
        MateMixerDevice *device;
        MateMixerStream *stream;

        device = g_object_get_data (G_OBJECT (widget), "device");
        if (G_UNLIKELY (device == NULL)) {
                g_warn_if_reached ();
                return;
        }

        stream = find_device_test_stream (dialog, device);
        if (G_UNLIKELY (stream == NULL)) {
                g_warn_if_reached ();
                return;
        }

        title = g_strdup_printf (_("Speaker Testing for %s"),
                                 mate_mixer_device_get_description (device));

        d = gtk_dialog_new_with_buttons (title,
                                         GTK_WINDOW (dialog),
                                         GTK_DIALOG_MODAL |
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         "gtk-close",
                                         GTK_RESPONSE_CLOSE,
                                         NULL);
        g_free (title);

        gtk_window_set_resizable (GTK_WINDOW (d), FALSE);

        test = gvc_speaker_test_new (stream);
        gtk_widget_show (test);

        container = gtk_dialog_get_content_area (GTK_DIALOG (d));
        gtk_container_add (GTK_CONTAINER (container), test);

        gtk_dialog_run (GTK_DIALOG (d));
        gtk_widget_destroy (d);
}

static GtkWidget *
create_profile_combo_box (GvcMixerDialog  *dialog,
                          MateMixerDevice *device,
                          const gchar     *name,
                          const GList     *items,
                          const gchar     *active)
{
        GtkWidget *combobox;

        combobox = gvc_combo_box_new (name);

        gvc_combo_box_set_profiles (GVC_COMBO_BOX (combobox), items);
        gvc_combo_box_set_active (GVC_COMBO_BOX (combobox), active);

        gvc_combo_box_set_size_group (GVC_COMBO_BOX (combobox),
                                      dialog->priv->size_group,
                                      FALSE);

        g_object_set (G_OBJECT (combobox),
                      "button-label", _("Test Speakers"),
                      NULL);

        g_object_set_data_full (G_OBJECT (combobox),
                                "device",
                                g_object_ref (device),
                                g_object_unref);

        g_signal_connect (G_OBJECT (combobox),
                          "changed",
                          G_CALLBACK (on_combo_box_profile_changed),
                          dialog);

        g_signal_connect (G_OBJECT (combobox),
                          "button-clicked",
                          G_CALLBACK (on_test_speakers_clicked),
                          dialog);

        return combobox;
}

static void
on_device_selection_changed (GtkTreeSelection *selection, GvcMixerDialog *dialog)
{
        GtkTreeIter          iter;
        gchar               *name;
        MateMixerDevice     *device;
        MateMixerBackendType backend;

        g_debug ("Device selection changed");

        if (dialog->priv->hw_profile_combo != NULL) {
                gtk_container_remove (GTK_CONTAINER (dialog->priv->hw_settings_box),
                                      dialog->priv->hw_profile_combo);

                dialog->priv->hw_profile_combo = NULL;
        }

        if (gtk_tree_selection_get_selected (selection, NULL, &iter) == FALSE)
                return;

        gtk_tree_model_get (gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->hw_treeview)),
                            &iter,
                            HW_NAME_COLUMN, &name,
                            -1);

        device = mate_mixer_control_get_device (dialog->priv->control, name);
        if (G_UNLIKELY (device == NULL)) {
                g_warn_if_reached ();
                g_free (name);
                return;
        }
        g_free (name);

        backend = mate_mixer_control_get_backend_type (dialog->priv->control);

        /* Speaker test is only possible on PulseAudio and profile changing is
         * not supported on other backends by libmatemixer, so avoid displaying
         * the combo box on other backends */
        if (backend == MATE_MIXER_BACKEND_PULSEAUDIO) {
                const GList *profiles;
                const gchar *profile_name = NULL;

                profiles = mate_mixer_device_list_profiles (device);
                if (profiles != NULL) {
                        MateMixerDeviceProfile *profile;

                        profile = mate_mixer_device_get_active_profile (device);
                        if (G_UNLIKELY (profile == NULL)) {
                                /* Select the first profile if none is selected */
                                profile = MATE_MIXER_DEVICE_PROFILE (profiles->data);
                                mate_mixer_device_set_active_profile (device, profile);
                        }

                        profile_name = mate_mixer_device_profile_get_name (profile);
                }

                dialog->priv->hw_profile_combo =
                        create_profile_combo_box (dialog,
                                                  device,
                                                  _("_Profile:"),
                                                  profiles,
                                                  profile_name);

                gtk_box_pack_start (GTK_BOX (dialog->priv->hw_settings_box),
                                    dialog->priv->hw_profile_combo,
                                    TRUE, TRUE, 6);

                /* Enable the speaker test button if the selected device
                 * is capable of sound output */
                update_device_test_visibility (dialog);

                gtk_widget_show (dialog->priv->hw_profile_combo);
        }
}

static void
on_notebook_switch_page (GtkNotebook    *notebook,
                         GtkWidget      *page,
                         guint           page_num,
                         GvcMixerDialog *dialog)
{
        MateMixerStream *stream;

        stream = mate_mixer_control_get_default_input_stream (dialog->priv->control);
        if (stream == NULL)
                return;

        if (page_num == PAGE_INPUT)
                mate_mixer_stream_monitor_start (stream);
        else
                mate_mixer_stream_monitor_stop (stream);
}

static void
device_name_to_text (GtkTreeViewColumn *column,
                     GtkCellRenderer   *cell,
                     GtkTreeModel      *model,
                     GtkTreeIter       *iter,
                     gpointer           user_data)
{
        gchar *description = NULL;
        gchar *profile = NULL;
        gchar *profile_status = NULL;

        gtk_tree_model_get (model, iter,
                            HW_DESCRIPTION_COLUMN, &description,
                            HW_PROFILE_COLUMN, &profile,
                            HW_STATUS_COLUMN, &profile_status,
                            -1);

        if (profile != NULL) {
                gchar *str;

                if (G_LIKELY (profile_status != NULL))
                        str = g_strdup_printf ("%s\n<i>%s</i>\n<i>%s</i>",
                                               description,
                                               profile_status,
                                               profile);
                else
                        str = g_strdup_printf ("%s\n<i>%s</i>",
                                               description,
                                               profile);

                g_object_set (cell, "markup", str, NULL);
                g_free (str);
                g_free (profile);
                g_free (profile_status);
        } else
                g_object_set (cell, "text", description, NULL);

        g_free (description);
}

static gint
compare_device_treeview_items (GtkTreeModel *model,
                               GtkTreeIter  *a,
                               GtkTreeIter  *b,
                               gpointer      user_data)
{
        gchar *desc_a = NULL;
        gchar *desc_b = NULL;
        gint   result;

        gtk_tree_model_get (model, a,
                            HW_DESCRIPTION_COLUMN, &desc_a,
                            -1);
        gtk_tree_model_get (model, b,
                            HW_DESCRIPTION_COLUMN, &desc_b,
                            -1);

        result = g_ascii_strcasecmp (desc_a, desc_b);

        g_free (desc_a);
        g_free (desc_b);
        return result;
}

static GtkWidget *
create_device_treeview (GvcMixerDialog *dialog, GCallback on_changed)
{
        GtkWidget         *treeview;
        GtkListStore      *store;
        GtkCellRenderer   *renderer;
        GtkTreeViewColumn *column;
        GtkTreeSelection  *selection;

        treeview = gtk_tree_view_new ();
        gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
        g_signal_connect (G_OBJECT (selection),
                          "changed",
                          on_changed,
                          dialog);

        store = gtk_list_store_new (HW_NUM_COLUMNS,
                                    G_TYPE_ICON,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_STRING);

        gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
                                 GTK_TREE_MODEL (store));

        renderer = gtk_cell_renderer_pixbuf_new ();
        g_object_set (G_OBJECT (renderer),
                      "stock-size",
                      GTK_ICON_SIZE_DIALOG,
                      NULL);

        column = gtk_tree_view_column_new_with_attributes (NULL,
                                                           renderer,
                                                           "gicon", HW_ICON_COLUMN,
                                                           NULL);

        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
        gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (treeview), -1,
                                                    _("Name"),
                                                    gtk_cell_renderer_text_new (),
                                                    device_name_to_text,
                                                    NULL, NULL);

        /* Keep the list of streams sorted by the name */
        gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
                                              HW_DESCRIPTION_COLUMN,
                                              GTK_SORT_ASCENDING);

        gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store),
                                         HW_DESCRIPTION_COLUMN,
                                         compare_device_treeview_items,
                                         NULL, NULL);
        return treeview;
}

static void
dialog_accel_cb (GtkAccelGroup    *accelgroup,
                 GObject          *object,
                 guint             key,
                 GdkModifierType   mod,
                 GvcMixerDialog   *self)
{
        gint num = -1;
        gint i;

        for (i = 0; i < G_N_ELEMENTS (tab_accel_keys); i++) {
                if (tab_accel_keys[i] == key) {
                        num = i;
                        break;
                }
        }

        if (num != -1)
                gtk_notebook_set_current_page (GTK_NOTEBOOK (self->priv->notebook), num);
}

static GObject *
gvc_mixer_dialog_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_params)
{
        GObject          *object;
        GvcMixerDialog   *self;
        GtkWidget        *main_vbox;
        GtkWidget        *label;
        GtkWidget        *alignment;
        GtkWidget        *box;
        GtkWidget        *sbox;
        GtkWidget        *ebox;
        GtkTreeSelection *selection;
        GtkAccelGroup    *accel_group;
        GClosure         *closure;
        GtkTreeIter       iter;
        gint              i;
        GList            *list;

        object = G_OBJECT_CLASS (gvc_mixer_dialog_parent_class)->constructor (type,
                                                                              n_construct_properties,
                                                                              construct_params);

        self = GVC_MIXER_DIALOG (object);

        gtk_dialog_add_button (GTK_DIALOG (self), "gtk-close", GTK_RESPONSE_OK);

        main_vbox = gtk_dialog_get_content_area (GTK_DIALOG (self));
        gtk_box_set_spacing (GTK_BOX (main_vbox), 2);

        gtk_container_set_border_width (GTK_CONTAINER (self), 6);

#if GTK_CHECK_VERSION (3, 0, 0)
        self->priv->output_stream_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
#else
        self->priv->output_stream_box = gtk_hbox_new (FALSE, 12);
#endif

        alignment = gtk_alignment_new (0, 0, 1, 1);
        gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 12, 0, 0, 0);
        gtk_container_add (GTK_CONTAINER (alignment), self->priv->output_stream_box);
        gtk_box_pack_start (GTK_BOX (main_vbox),
                            alignment,
                            FALSE, FALSE, 0);

        self->priv->output_bar = create_bar (self, TRUE, TRUE);
        gvc_channel_bar_set_name (GVC_CHANNEL_BAR (self->priv->output_bar),
                                  _("_Output volume: "));

        gtk_widget_set_sensitive (self->priv->output_bar, FALSE);

        gtk_box_pack_start (GTK_BOX (self->priv->output_stream_box),
                            self->priv->output_bar, TRUE, TRUE, 12);

        self->priv->notebook = gtk_notebook_new ();
        gtk_box_pack_start (GTK_BOX (main_vbox),
                            self->priv->notebook,
                            TRUE, TRUE, 0);

        g_signal_connect (G_OBJECT (self->priv->notebook),
                          "switch-page",
                          G_CALLBACK (on_notebook_switch_page),
                          self);

        gtk_container_set_border_width (GTK_CONTAINER (self->priv->notebook), 5);

        /* Set up accels (borrowed from Empathy) */
        accel_group = gtk_accel_group_new ();
        gtk_window_add_accel_group (GTK_WINDOW (self), accel_group);

        for (i = 0; i < G_N_ELEMENTS (tab_accel_keys); i++) {
                closure = g_cclosure_new (G_CALLBACK (dialog_accel_cb),
                                          self,
                                          NULL);
                gtk_accel_group_connect (accel_group,
                                         tab_accel_keys[i],
                                         GDK_MOD1_MASK,
                                         0,
                                         closure);
        }

        g_object_unref (accel_group);

        /* Create notebook pages */
#if GTK_CHECK_VERSION (3, 0, 0)
        self->priv->sound_effects_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
#else
        self->priv->sound_effects_box = gtk_vbox_new (FALSE, 6);
#endif
        gtk_container_set_border_width (GTK_CONTAINER (self->priv->sound_effects_box), 12);

        label = gtk_label_new (_("Sound Effects"));
        gtk_notebook_append_page (GTK_NOTEBOOK (self->priv->notebook),
                                  self->priv->sound_effects_box,
                                  label);

        self->priv->effects_bar = create_bar (self, TRUE, TRUE);
        gtk_box_pack_start (GTK_BOX (self->priv->sound_effects_box),
                            self->priv->effects_bar, FALSE, FALSE, 0);

        gvc_channel_bar_set_show_marks (GVC_CHANNEL_BAR (self->priv->effects_bar), FALSE);
        gvc_channel_bar_set_extended (GVC_CHANNEL_BAR (self->priv->effects_bar), FALSE);

        gvc_channel_bar_set_name (GVC_CHANNEL_BAR (self->priv->effects_bar),
                                  _("_Alert volume: "));

        gtk_widget_set_sensitive (self->priv->effects_bar, FALSE);

        self->priv->sound_theme_chooser = gvc_sound_theme_chooser_new ();

        gtk_box_pack_start (GTK_BOX (self->priv->sound_effects_box),
                            self->priv->sound_theme_chooser,
                            TRUE, TRUE, 6);

#if GTK_CHECK_VERSION (3, 0, 0)
        self->priv->hw_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
#else
        self->priv->hw_box = gtk_vbox_new (FALSE, 12);
#endif
        gtk_container_set_border_width (GTK_CONTAINER (self->priv->hw_box), 12);

        label = gtk_label_new (_("Hardware"));
        gtk_notebook_append_page (GTK_NOTEBOOK (self->priv->notebook),
                                  self->priv->hw_box,
                                  label);

        box = gtk_frame_new (_("C_hoose a device to configure:"));
        label = gtk_frame_get_label_widget (GTK_FRAME (box));
        make_label_bold (GTK_LABEL (label));
        gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
        gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (self->priv->hw_box), box, TRUE, TRUE, 0);

        alignment = gtk_alignment_new (0, 0, 1, 1);
        gtk_container_add (GTK_CONTAINER (box), alignment);
        gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 0, 0);

        self->priv->hw_treeview = create_device_treeview (self,
                                                         G_CALLBACK (on_device_selection_changed));
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->priv->hw_treeview);

        box = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (box),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (box),
                                             GTK_SHADOW_IN);
        gtk_container_add (GTK_CONTAINER (box), self->priv->hw_treeview);
        gtk_container_add (GTK_CONTAINER (alignment), box);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->hw_treeview));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

        box = gtk_frame_new (_("Settings for the selected device:"));
        label = gtk_frame_get_label_widget (GTK_FRAME (box));
        make_label_bold (GTK_LABEL (label));
        gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (self->priv->hw_box), box, FALSE, TRUE, 12);

#if GTK_CHECK_VERSION (3, 0, 0)
        self->priv->hw_settings_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
#else
        self->priv->hw_settings_box = gtk_vbox_new (FALSE, 12);
#endif

        gtk_container_add (GTK_CONTAINER (box), self->priv->hw_settings_box);

#if GTK_CHECK_VERSION (3, 0, 0)
        self->priv->input_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
#else
        self->priv->input_box = gtk_vbox_new (FALSE, 12);
#endif

        gtk_container_set_border_width (GTK_CONTAINER (self->priv->input_box), 12);

        label = gtk_label_new (_("Input"));
        gtk_notebook_append_page (GTK_NOTEBOOK (self->priv->notebook),
                                  self->priv->input_box,
                                  label);

        self->priv->input_bar = create_bar (self, TRUE, TRUE);
        gvc_channel_bar_set_name (GVC_CHANNEL_BAR (self->priv->input_bar),
                                  _("_Input volume: "));
        gvc_channel_bar_set_low_icon_name (GVC_CHANNEL_BAR (self->priv->input_bar),
                                           "audio-input-microphone-low");
        gvc_channel_bar_set_high_icon_name (GVC_CHANNEL_BAR (self->priv->input_bar),
                                            "audio-input-microphone-high");
        gtk_widget_set_sensitive (self->priv->input_bar, FALSE);

        alignment = gtk_alignment_new (0, 0, 1, 1);
        gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 0, 0);
        gtk_container_add (GTK_CONTAINER (alignment), self->priv->input_bar);
        gtk_box_pack_start (GTK_BOX (self->priv->input_box),
                            alignment,
                            FALSE, FALSE, 0);

#if GTK_CHECK_VERSION (3, 0, 0)
        box  = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        sbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        ebox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
#else
        box  = gtk_hbox_new (FALSE, 6);
        sbox = gtk_hbox_new (FALSE, 6);
        ebox = gtk_hbox_new (FALSE, 6);
#endif

        gtk_box_pack_start (GTK_BOX (self->priv->input_box),
                            box,
                            FALSE, FALSE, 6);
        gtk_box_pack_start (GTK_BOX (box),
                            sbox,
                            FALSE, FALSE, 0);

        label = gtk_label_new (_("Input level:"));
        gtk_box_pack_start (GTK_BOX (sbox),
                            label,
                            FALSE, FALSE, 0);
        gtk_size_group_add_widget (self->priv->size_group, sbox);

        self->priv->input_level_bar = gvc_level_bar_new ();
        gvc_level_bar_set_orientation (GVC_LEVEL_BAR (self->priv->input_level_bar),
                                       GTK_ORIENTATION_HORIZONTAL);
        gvc_level_bar_set_scale (GVC_LEVEL_BAR (self->priv->input_level_bar),
                                 GVC_LEVEL_SCALE_LINEAR);
        gtk_box_pack_start (GTK_BOX (box),
                            self->priv->input_level_bar,
                            TRUE, TRUE, 6);

        gtk_box_pack_start (GTK_BOX (box),
                            ebox,
                            FALSE, FALSE, 0);
        gtk_size_group_add_widget (self->priv->size_group, ebox);

#if GTK_CHECK_VERSION (3, 0, 0)
        self->priv->input_settings_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
#else
        self->priv->input_settings_box = gtk_hbox_new (FALSE, 6);
#endif
        gtk_box_pack_start (GTK_BOX (self->priv->input_box),
                            self->priv->input_settings_box,
                            FALSE, FALSE, 0);

        box = gtk_frame_new (_("C_hoose a device for sound input:"));
        label = gtk_frame_get_label_widget (GTK_FRAME (box));
        make_label_bold (GTK_LABEL (label));
        gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
        gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (self->priv->input_box), box, TRUE, TRUE, 0);

        alignment = gtk_alignment_new (0, 0, 1, 1);
        gtk_container_add (GTK_CONTAINER (box), alignment);
        gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 0, 0);

        self->priv->input_treeview =
                create_stream_treeview (self, G_CALLBACK (on_input_radio_toggled));

        gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->priv->input_treeview);

        box = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (box),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (box),
                                             GTK_SHADOW_IN);
        gtk_container_add (GTK_CONTAINER (box), self->priv->input_treeview);
        gtk_container_add (GTK_CONTAINER (alignment), box);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->input_treeview));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

        /* Output page */
        self->priv->output_box = gtk_vbox_new (FALSE, 12);
        gtk_container_set_border_width (GTK_CONTAINER (self->priv->output_box), 12);
        label = gtk_label_new (_("Output"));
        gtk_notebook_append_page (GTK_NOTEBOOK (self->priv->notebook),
                                  self->priv->output_box,
                                  label);

        box = gtk_frame_new (_("C_hoose a device for sound output:"));
        label = gtk_frame_get_label_widget (GTK_FRAME (box));
        make_label_bold (GTK_LABEL (label));
        gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
        gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (self->priv->output_box), box, TRUE, TRUE, 0);

        alignment = gtk_alignment_new (0, 0, 1, 1);
        gtk_container_add (GTK_CONTAINER (box), alignment);
        gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 0, 0);

        self->priv->output_treeview = create_stream_treeview (self,
                                                              G_CALLBACK (on_output_radio_toggled));
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), self->priv->output_treeview);

        box = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (box),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (box),
                                             GTK_SHADOW_IN);
        gtk_container_add (GTK_CONTAINER (box), self->priv->output_treeview);
        gtk_container_add (GTK_CONTAINER (alignment), box);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->output_treeview));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

        box = gtk_frame_new (_("Settings for the selected device:"));
        label = gtk_frame_get_label_widget (GTK_FRAME (box));
        make_label_bold (GTK_LABEL (label));
        gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (self->priv->output_box), box, FALSE, FALSE, 12);
        self->priv->output_settings_box = gtk_vbox_new (FALSE, 0);
        gtk_container_add (GTK_CONTAINER (box), self->priv->output_settings_box);

        self->priv->output_settings_frame = box;

        /* Applications */
        self->priv->applications_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->priv->applications_window),
                                        GTK_POLICY_NEVER,
                                        GTK_POLICY_AUTOMATIC);

        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (self->priv->applications_window),
                                             GTK_SHADOW_IN);

#if GTK_CHECK_VERSION (3, 0, 0)
        self->priv->applications_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
#else
        self->priv->applications_box = gtk_vbox_new (FALSE, 12);
#endif
        gtk_container_set_border_width (GTK_CONTAINER (self->priv->applications_box), 12);

#if GTK_CHECK_VERSION (3, 8, 0)
        gtk_container_add (GTK_CONTAINER (self->priv->applications_window),
                           self->priv->applications_box);
#else
        gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (self->priv->applications_window),
                                               self->priv->applications_box);
#endif

        label = gtk_label_new (_("Applications"));
        gtk_notebook_append_page (GTK_NOTEBOOK (self->priv->notebook),
                                  self->priv->applications_window,
                                  label);

        self->priv->no_apps_label = gtk_label_new (_("No application is currently playing or recording audio."));
        gtk_box_pack_start (GTK_BOX (self->priv->applications_box),
                            self->priv->no_apps_label,
                            TRUE, TRUE, 0);

        gtk_widget_show_all (main_vbox);

        // XXX subscribe to cached stream stuff, in case event stream is not found

        list = (GList *) mate_mixer_control_list_streams (self->priv->control);
        while (list) {
                add_stream (self, MATE_MIXER_STREAM (list->data));
                list = list->next;
        }

        list = (GList *) mate_mixer_control_list_devices (self->priv->control);
        while (list) {
                add_device (self, MATE_MIXER_DEVICE (list->data));
                list = list->next;
        }

        /* Find an event role stream */
        list = (GList *) mate_mixer_control_list_cached_streams (self->priv->control);
        while (list) {
                MateMixerClientStreamRole role;

                role = mate_mixer_client_stream_get_role (MATE_MIXER_CLIENT_STREAM (list->data));

                if (role == MATE_MIXER_CLIENT_STREAM_ROLE_EVENT)
                        add_stream (self, MATE_MIXER_STREAM (list->data));

                list = list->next;
        }

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->priv->hw_treeview));

        /* Select the first device in the list */
        // XXX handle no devices
        if (gtk_tree_selection_get_selected (selection, NULL, NULL) == FALSE) {
                GtkTreeModel *model =
                        gtk_tree_view_get_model (GTK_TREE_VIEW (self->priv->hw_treeview));

                if (gtk_tree_model_get_iter_first (model, &iter))
                        gtk_tree_selection_select_iter (selection, &iter);
        }

        return object;
}

static MateMixerControl *
gvc_mixer_dialog_get_control (GvcMixerDialog *dialog)
{
        return dialog->priv->control;
}

static void
gvc_mixer_dialog_set_control (GvcMixerDialog *dialog, MateMixerControl *control)
{
        MateMixerState state;

        state = mate_mixer_control_get_state (control);

        if (G_UNLIKELY (state != MATE_MIXER_STATE_READY))
                g_warn_if_reached ();

        dialog->priv->control = g_object_ref (control);

        g_signal_connect (G_OBJECT (dialog->priv->control),
                          "stream-added",
                          G_CALLBACK (on_control_stream_added),
                          dialog);
        g_signal_connect (G_OBJECT (dialog->priv->control),
                          "cached-stream-added",
                          G_CALLBACK (on_control_stream_added),
                          dialog);
        g_signal_connect (G_OBJECT (dialog->priv->control),
                          "stream-removed",
                          G_CALLBACK (on_control_stream_removed),
                          dialog);
        g_signal_connect (G_OBJECT (dialog->priv->control),
                          "cached-stream-removed",
                          G_CALLBACK (on_control_cached_stream_removed),
                          dialog);
        g_signal_connect (G_OBJECT (dialog->priv->control),
                          "device-added",
                          G_CALLBACK (on_control_device_added),
                          dialog);
        g_signal_connect (G_OBJECT (dialog->priv->control),
                          "device-removed",
                          G_CALLBACK (on_control_device_removed),
                          dialog);

        g_signal_connect (G_OBJECT (dialog->priv->control),
                          "notify::default-output",
                          G_CALLBACK (on_control_default_output_notify),
                          dialog);
        g_signal_connect (G_OBJECT (dialog->priv->control),
                          "notify::default-input",
                          G_CALLBACK (on_control_default_input_notify),
                          dialog);

        g_object_notify (G_OBJECT (dialog), "control");
}

static void
gvc_mixer_dialog_set_property (GObject       *object,
                               guint          prop_id,
                               const GValue  *value,
                               GParamSpec    *pspec)
{
        GvcMixerDialog *self = GVC_MIXER_DIALOG (object);

        switch (prop_id) {
        case PROP_CONTROL:
                gvc_mixer_dialog_set_control (self, g_value_get_object (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_mixer_dialog_get_property (GObject     *object,
                               guint        prop_id,
                               GValue      *value,
                               GParamSpec  *pspec)
{
        GvcMixerDialog *self = GVC_MIXER_DIALOG (object);

        switch (prop_id) {
        case PROP_CONTROL:
                g_value_set_object (value, gvc_mixer_dialog_get_control (self));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_mixer_dialog_dispose (GObject *object)
{
        GvcMixerDialog *dialog = GVC_MIXER_DIALOG (object);

        if (dialog->priv->control != NULL) {
                g_signal_handlers_disconnect_by_func (dialog->priv->control,
                                                      on_control_stream_added,
                                                      dialog);
                g_signal_handlers_disconnect_by_func (dialog->priv->control,
                                                      on_control_stream_removed,
                                                      dialog);
                g_signal_handlers_disconnect_by_func (dialog->priv->control,
                                                      on_control_device_added,
                                                      dialog);
                g_signal_handlers_disconnect_by_func (dialog->priv->control,
                                                      on_control_device_removed,
                                                      dialog);

                g_clear_object (&dialog->priv->control);
        }

        G_OBJECT_CLASS (gvc_mixer_dialog_parent_class)->dispose (object);
}

static void
gvc_mixer_dialog_class_init (GvcMixerDialogClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = gvc_mixer_dialog_constructor;
        object_class->dispose = gvc_mixer_dialog_dispose;
        object_class->finalize = gvc_mixer_dialog_finalize;
        object_class->set_property = gvc_mixer_dialog_set_property;
        object_class->get_property = gvc_mixer_dialog_get_property;

        g_object_class_install_property (object_class,
                                         PROP_CONTROL,
                                         g_param_spec_object ("control",
                                                              "Control",
                                                              "MateMixer control",
                                                              MATE_MIXER_TYPE_CONTROL,
                                                              G_PARAM_READWRITE |
                                                              G_PARAM_CONSTRUCT_ONLY));

        g_type_class_add_private (klass, sizeof (GvcMixerDialogPrivate));
}

static void
gvc_mixer_dialog_init (GvcMixerDialog *dialog)
{
        dialog->priv = GVC_MIXER_DIALOG_GET_PRIVATE (dialog);

        dialog->priv->bars = g_hash_table_new (g_str_hash, g_str_equal);
        dialog->priv->size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
}

static void
gvc_mixer_dialog_finalize (GObject *object)
{
        GvcMixerDialog *dialog;

        dialog = GVC_MIXER_DIALOG (object);

        g_hash_table_destroy (dialog->priv->bars);

        G_OBJECT_CLASS (gvc_mixer_dialog_parent_class)->finalize (object);
}

GvcMixerDialog *
gvc_mixer_dialog_new (MateMixerControl *control)
{
        return g_object_new (GVC_TYPE_MIXER_DIALOG,
                             "icon-name", "multimedia-volume-control",
                             "title", _("Sound Preferences"),
                             "control", control,
                             NULL);
}

gboolean
gvc_mixer_dialog_set_page (GvcMixerDialog *self, const gchar *page)
{
        guint num = 0;

        g_return_val_if_fail (GVC_IS_MIXER_DIALOG (self), FALSE);

        if (page != NULL) {
                if (g_str_equal (page, "effects"))
                        num = PAGE_EFFECTS;
                else if (g_str_equal (page, "hardware"))
                        num = PAGE_HARDWARE;
                else if (g_str_equal (page, "input"))
                        num = PAGE_INPUT;
                else if (g_str_equal (page, "output"))
                        num = PAGE_OUTPUT;
                else if (g_str_equal (page, "applications"))
                        num = PAGE_APPLICATIONS;
        }

        gtk_notebook_set_current_page (GTK_NOTEBOOK (self->priv->notebook), num);

        return TRUE;
}
