/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#if GTK_CHECK_VERSION (3, 0, 0)
#include <gdk/gdkkeysyms-compat.h>
#endif

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
        GtkWidget        *applications_scrolled_window;
        GtkWidget        *no_apps_label;
        GtkWidget        *output_treeview;
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
        GtkSizeGroup     *apps_size_group;
        gdouble           last_input_peak;
        guint             num_apps;
};

enum {
        NAME_COLUMN,
        DESCRIPTION_COLUMN,
        DEVICE_COLUMN,
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
        HW_PROFILE_HUMAN_COLUMN,
        HW_SENSITIVE_COLUMN,
        HW_NUM_COLUMNS
};

enum {
        PAGE_EVENTS,
        PAGE_HARDWARE,
        PAGE_INPUT,
        PAGE_OUTPUT,
        PAGE_APPLICATIONS
};

enum
{
        PROP_0,
        PROP_MIXER_CONTROL
};

static void     gvc_mixer_dialog_class_init (GvcMixerDialogClass *klass);
static void     gvc_mixer_dialog_init       (GvcMixerDialog      *mixer_dialog);
static void     gvc_mixer_dialog_finalize   (GObject             *object);

static void     bar_set_stream              (GvcMixerDialog      *dialog,
                                             GtkWidget           *bar,
                                             MateMixerStream     *stream);

static void     on_adjustment_value_changed (GtkAdjustment  *adjustment,
                                             GvcMixerDialog *dialog);

G_DEFINE_TYPE (GvcMixerDialog, gvc_mixer_dialog, GTK_TYPE_DIALOG)

static void
update_default_input (GvcMixerDialog *dialog)
{
        GtkTreeModel *model;
        GtkTreeIter   iter;
        gboolean      ret;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->input_treeview));
        ret = gtk_tree_model_get_iter_first (model, &iter);
        if (ret == FALSE) {
                g_debug ("No default input selected or available");
                return;
        }
        do {
                gboolean        toggled;
                gboolean        is_default = FALSE;
                MateMixerStream *stream;
                gchar *name;

                gtk_tree_model_get (model, &iter,
                                    NAME_COLUMN, &name,
                                    ACTIVE_COLUMN, &toggled,
                                    -1);

                stream = mate_mixer_control_get_stream (dialog->priv->control, name);
                if (stream == NULL) {
                        g_warning ("Unable to find stream for id: %s", name);
                        g_free (name);
                        continue;
                }

                if (stream == mate_mixer_control_get_default_input_stream (dialog->priv->control))
                        is_default = TRUE;

                gtk_list_store_set (GTK_LIST_STORE (model),
                                    &iter,
                                    ACTIVE_COLUMN, is_default,
                                    -1);

                g_free (name);
        } while (gtk_tree_model_iter_next (model, &iter));
}

static void
update_description (GvcMixerDialog *dialog,
                    guint column,
                    const gchar *value,
                    MateMixerStream *stream)
{
        GtkTreeModel *model;
        GtkTreeIter   iter;
        MateMixerStreamFlags flags;
        const gchar *name;

        flags = mate_mixer_stream_get_flags (stream);

        if (flags & MATE_MIXER_STREAM_INPUT)
                model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->input_treeview));
        else if (flags & MATE_MIXER_STREAM_OUTPUT)
                model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->output_treeview));
        else
                g_assert_not_reached ();

        gtk_tree_model_get_iter_first (model, &iter);

        name = mate_mixer_stream_get_name (stream);
        do {
                const gchar *current_name;

                gtk_tree_model_get (model, &iter, NAME_COLUMN, &current_name, -1);
                if (!g_strcmp0 (name, current_name))
                        continue;

                gtk_list_store_set (GTK_LIST_STORE (model),
                                    &iter,
                                    column, value,
                                    -1);
                break;
        } while (gtk_tree_model_iter_next (model, &iter));
}

static void
port_selection_changed (GvcComboBox    *combo,
                        const gchar    *port,
                        GvcMixerDialog *dialog)
{
        MateMixerStream *stream;

        stream = g_object_get_data (G_OBJECT (combo), "stream");
        if (stream == NULL) {
                g_warning ("Could not find stream for port combo box");
                return;
        }

        mate_mixer_stream_set_active_port (stream, port);
}

static void
update_output_settings (GvcMixerDialog *dialog)
{
        MateMixerStream      *stream;
        MateMixerStreamFlags  flags;
        const GList          *ports;

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
                g_warning ("Default sink stream not found");
                return;
        }

        flags = mate_mixer_stream_get_flags (stream);

        gvc_channel_bar_set_base_volume (GVC_CHANNEL_BAR (dialog->priv->output_bar),
                                         mate_mixer_stream_get_base_volume (stream));
        gvc_channel_bar_set_is_amplified (GVC_CHANNEL_BAR (dialog->priv->output_bar),
                                          flags & MATE_MIXER_STREAM_HAS_DECIBEL_VOLUME);

        dialog->priv->output_balance_bar = gvc_balance_bar_new (stream, BALANCE_TYPE_RL);
        if (dialog->priv->size_group != NULL) {
                gvc_balance_bar_set_size_group (GVC_BALANCE_BAR (dialog->priv->output_balance_bar),
                                                dialog->priv->size_group,
                                                TRUE);
        }
        gtk_box_pack_start (GTK_BOX (dialog->priv->output_settings_box),
                            dialog->priv->output_balance_bar,
                            FALSE, FALSE, 6);
        gtk_widget_show (dialog->priv->output_balance_bar);

        if (flags & MATE_MIXER_STREAM_CAN_FADE) {
                dialog->priv->output_fade_bar = gvc_balance_bar_new (stream, BALANCE_TYPE_FR);
                if (dialog->priv->size_group != NULL) {
                        gvc_balance_bar_set_size_group (GVC_BALANCE_BAR (dialog->priv->output_fade_bar),
                                                        dialog->priv->size_group,
                                                        TRUE);
                }
                gtk_box_pack_start (GTK_BOX (dialog->priv->output_settings_box),
                                    dialog->priv->output_fade_bar,
                                    FALSE, FALSE, 6);
                gtk_widget_show (dialog->priv->output_fade_bar);
        }

        if (mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_LFE)) {
                dialog->priv->output_lfe_bar = gvc_balance_bar_new (stream, BALANCE_TYPE_LFE);
                if (dialog->priv->size_group != NULL) {
                        gvc_balance_bar_set_size_group (GVC_BALANCE_BAR (dialog->priv->output_lfe_bar),
                                                        dialog->priv->size_group,
                                                        TRUE);
                }
                gtk_box_pack_start (GTK_BOX (dialog->priv->output_settings_box),
                                    dialog->priv->output_lfe_bar,
                                    FALSE, FALSE, 6);
                gtk_widget_show (dialog->priv->output_lfe_bar);
        }

        ports = mate_mixer_stream_list_ports (stream);
        if (ports != NULL) {
                MateMixerPort *port;

                port = mate_mixer_stream_get_active_port (stream);

                dialog->priv->output_port_combo = gvc_combo_box_new (_("Co_nnector:"));
                gvc_combo_box_set_ports (GVC_COMBO_BOX (dialog->priv->output_port_combo),
                                         ports);

                gvc_combo_box_set_active (GVC_COMBO_BOX (dialog->priv->output_port_combo),
                                          mate_mixer_port_get_name (port));

                g_object_set_data (G_OBJECT (dialog->priv->output_port_combo), "stream", stream);
                g_signal_connect (G_OBJECT (dialog->priv->output_port_combo), "changed",
                                  G_CALLBACK (port_selection_changed), dialog);

                gtk_box_pack_start (GTK_BOX (dialog->priv->output_settings_box),
                                    dialog->priv->output_port_combo,
                                    TRUE, FALSE, 6);

                gvc_combo_box_set_size_group (GVC_COMBO_BOX (dialog->priv->output_port_combo), dialog->priv->size_group, FALSE);

                gtk_widget_show (dialog->priv->output_port_combo);
        }

        /* FIXME: We could make this into a "No settings" label instead */
        gtk_widget_set_sensitive (dialog->priv->output_balance_bar, flags & MATE_MIXER_STREAM_CAN_BALANCE);
}

static void
update_default_output (GvcMixerDialog *dialog)
{
        GtkTreeModel *model;
        GtkTreeIter   iter;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->output_treeview));
        gtk_tree_model_get_iter_first (model, &iter);
        do {
                gboolean        toggled;
                gboolean        is_default = FALSE;
                gchar *name;
                MateMixerStream *stream;

                gtk_tree_model_get (model, &iter,
                                    NAME_COLUMN, &name,
                                    ACTIVE_COLUMN, &toggled,
                                    -1);

                stream = mate_mixer_control_get_stream (dialog->priv->control, name);
                if (stream == NULL) {
                        g_warning ("Unable to find stream for id: %s", name);
                        g_free (name);
                        continue;
                }

                if (stream == mate_mixer_control_get_default_output_stream (dialog->priv->control))
                        is_default = TRUE;

                gtk_list_store_set (GTK_LIST_STORE (model),
                                    &iter,
                                    ACTIVE_COLUMN, is_default,
                                    -1);
                g_free (name);
        } while (gtk_tree_model_iter_next (model, &iter));
}

static void
on_control_default_output_notify (MateMixerControl *control,
                                  GParamSpec       *pspec,
                                  GvcMixerDialog   *dialog)
{
        MateMixerStream *stream;

        g_debug ("Default output stream has changed");

        stream = mate_mixer_control_get_default_output_stream (control);

        bar_set_stream (dialog, dialog->priv->output_bar, stream);

        update_output_settings (dialog);

        update_default_output (dialog);
}


#define DECAY_STEP .15

static void
update_input_peak (GvcMixerDialog *dialog, gdouble v)
{
        GtkAdjustment *adj;

        if (dialog->priv->last_input_peak >= DECAY_STEP) {
                if (v < dialog->priv->last_input_peak - DECAY_STEP) {
                        v = dialog->priv->last_input_peak - DECAY_STEP;
                }
        }

        dialog->priv->last_input_peak = v;

        adj = gvc_level_bar_get_peak_adjustment (GVC_LEVEL_BAR (dialog->priv->input_level_bar));
        if (v >= 0) {
                gtk_adjustment_set_value (adj, v);
        } else {
                gtk_adjustment_set_value (adj, 0.0);
        }
}

static void
on_monitor_suspended_callback (pa_stream *s,
                               void      *userdata)
{
        // XXX
        /*
        GvcMixerDialog *dialog;

        dialog = userdata;

        if (pa_stream_is_suspended (s)) {
                g_debug ("Stream suspended");
                update_input_meter (dialog,
                                    pa_stream_get_device_index (s),
                                    PA_INVALID_INDEX,
                                    -1);
        }
        */
}

static void
on_stream_monitor_value (MateMixerStream *stream,
                         gdouble          value,
                         GvcMixerDialog  *dialog)
{
        g_debug ("Monitor %.2f", value);
        update_input_peak (dialog, value);
}

static void
update_input_settings (GvcMixerDialog *dialog)
{
        MateMixerStream *stream;
        MateMixerStreamFlags flags;
        const GList    *ports;

        g_debug ("Updating input settings");

        if (dialog->priv->input_port_combo != NULL) {
                gtk_container_remove (GTK_CONTAINER (dialog->priv->input_settings_box),
                                      dialog->priv->input_port_combo);
                dialog->priv->input_port_combo = NULL;
        }

        stream = mate_mixer_control_get_default_input_stream (dialog->priv->control);
        if (stream == NULL) {
                g_debug ("Default source stream not found");
                return;
        }

        mate_mixer_stream_monitor_set_name (stream, _("Peak detect"));

        g_signal_connect (G_OBJECT (stream),
                          "monitor-value",
                          G_CALLBACK (on_stream_monitor_value),
                          dialog);

        flags = mate_mixer_stream_get_flags (stream);

        gvc_channel_bar_set_base_volume (GVC_CHANNEL_BAR (dialog->priv->input_bar),
                                         mate_mixer_stream_get_base_volume (stream));

        // XXX probably wrong
        gvc_channel_bar_set_is_amplified (GVC_CHANNEL_BAR (dialog->priv->input_bar),
                                          flags & MATE_MIXER_STREAM_HAS_DECIBEL_VOLUME);

        ports = mate_mixer_stream_list_ports (stream);
        if (ports != NULL) {
                MateMixerPort *port;

                port = mate_mixer_stream_get_active_port (stream);

                dialog->priv->input_port_combo = gvc_combo_box_new (_("Co_nnector:"));
                gvc_combo_box_set_ports (GVC_COMBO_BOX (dialog->priv->input_port_combo),
                                         ports);
                gvc_combo_box_set_active (GVC_COMBO_BOX (dialog->priv->input_port_combo),
                                          mate_mixer_port_get_name (port));

                g_object_set_data (G_OBJECT (dialog->priv->input_port_combo),
                                   "stream",
                                   stream);

                g_signal_connect (G_OBJECT (dialog->priv->input_port_combo),
                                  "changed",
                                  G_CALLBACK (port_selection_changed),
                                  dialog);

                gvc_combo_box_set_size_group (GVC_COMBO_BOX (dialog->priv->input_port_combo),
                                              dialog->priv->size_group,
                                              FALSE);

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
        MateMixerStream *stream;
        MateMixerStream *current;
        GtkAdjustment *adj;

        stream = mate_mixer_control_get_default_input_stream (control);

        current = g_object_get_data (G_OBJECT (dialog->priv->input_bar), "gvc-mixer-dialog-stream");
        if (current != NULL)
                g_signal_handlers_disconnect_by_func (G_OBJECT (current),
                                                      on_stream_monitor_value,
                                                      dialog);

        if (gtk_notebook_get_current_page (GTK_NOTEBOOK (dialog->priv->notebook)) == PAGE_INPUT) {
                if (current != NULL)
                        mate_mixer_stream_monitor_stop (current);
                if (stream != NULL)
                        mate_mixer_stream_monitor_start (stream);
        }

        // g_debug ("GvcMixerDialog: default source changed: %u", id);

        // XXX is the default input reffed/unreffed anywhere?

        /* Disconnect the adj, otherwise it might change if is_amplified changes */
        adj = GTK_ADJUSTMENT (gvc_channel_bar_get_adjustment (GVC_CHANNEL_BAR (dialog->priv->input_bar)));

        g_signal_handlers_disconnect_by_func (adj,
                                              on_adjustment_value_changed,
                                              dialog);

        bar_set_stream (dialog, dialog->priv->input_bar, stream);
        update_input_settings (dialog);

        g_signal_connect (adj,
                          "value-changed",
                          G_CALLBACK (on_adjustment_value_changed),
                          dialog);

        update_default_input (dialog);
}

static void
on_adjustment_value_changed (GtkAdjustment  *adjustment,
                             GvcMixerDialog *dialog)
{
        MateMixerStream *stream;

        stream = g_object_get_data (G_OBJECT (adjustment), "gvc-mixer-dialog-stream");
        if (stream != NULL) {
                GObject *bar;
                gdouble volume, rounded;
                char *name;

                volume = gtk_adjustment_get_value (adjustment);
                rounded = round (volume);

                bar = g_object_get_data (G_OBJECT (adjustment), "gvc-mixer-dialog-bar");
                g_object_get (bar, "name", &name, NULL);
                g_debug ("Setting stream volume %lf (rounded: %lf) for bar '%s'", volume, rounded, name);
                g_free (name);

                /* FIXME would need to do that in the balance bar really... */
                /* Make sure we do not unmute muted streams, there's a button for that */
                if (volume == 0.0)
                        mate_mixer_stream_set_mute (stream, TRUE);

                mate_mixer_stream_set_volume (stream, rounded);
        }
}

static void
on_bar_is_muted_notify (GObject        *object,
                        GParamSpec     *pspec,
                        GvcMixerDialog *dialog)
{
        gboolean        is_muted;
        MateMixerStream *stream;

        is_muted = gvc_channel_bar_get_is_muted (GVC_CHANNEL_BAR (object));

        stream = g_object_get_data (object, "gvc-mixer-dialog-stream");
        if (stream != NULL) {
                mate_mixer_stream_set_mute (stream, is_muted);
        } else {
                char *name;
                g_object_get (object, "name", &name, NULL);
                g_warning ("Unable to find stream for bar '%s'", name);
                g_free (name);
        }
}

static GtkWidget *
lookup_bar_for_stream (GvcMixerDialog *dialog, MateMixerStream *stream)
{
        return g_hash_table_lookup (dialog->priv->bars, mate_mixer_stream_get_name (stream));
}

static GtkWidget *
lookup_combo_box_for_stream (GvcMixerDialog *dialog,
                             MateMixerStream *stream)
{
        MateMixerStream *combo_stream;
        const gchar *name;

        name = mate_mixer_stream_get_name (stream);

        if (dialog->priv->output_port_combo != NULL) {
                combo_stream = g_object_get_data (G_OBJECT (dialog->priv->output_port_combo),
                                                  "stream");
                if (combo_stream != NULL) {
                        if (!g_strcmp0 (name, mate_mixer_stream_get_name (combo_stream)))
                                return dialog->priv->output_port_combo;
                }
        }

        if (dialog->priv->input_port_combo != NULL) {
                combo_stream = g_object_get_data (G_OBJECT (dialog->priv->input_port_combo),
                                                  "stream");
                if (combo_stream != NULL) {
                        if (!g_strcmp0 (name, mate_mixer_stream_get_name (combo_stream)))
                                return dialog->priv->input_port_combo;
                }
        }

        return NULL;
}

static void
on_stream_description_notify (MateMixerStream *stream,
                              GParamSpec     *pspec,
                              GvcMixerDialog *dialog)
{
        update_description (dialog, NAME_COLUMN,
                            mate_mixer_stream_get_description (stream),
                            stream);
}

static void
on_stream_port_notify (GObject        *object,
                       GParamSpec     *pspec,
                       GvcMixerDialog *dialog)
{
        GvcComboBox   *combo;
        MateMixerPort *port;

        combo = GVC_COMBO_BOX (lookup_combo_box_for_stream (dialog, MATE_MIXER_STREAM (object)));
        if (combo == NULL)
                return;

        g_signal_handlers_block_by_func (G_OBJECT (combo),
                                         port_selection_changed,
                                         dialog);

        g_object_get (object, "active-port", &port, NULL);
        // XXX is this correct?
        if (port) {
                gvc_combo_box_set_active (GVC_COMBO_BOX (combo),
                                          mate_mixer_port_get_name (port));
        }

        g_signal_handlers_unblock_by_func (G_OBJECT (combo),
                                           port_selection_changed,
                                           dialog);
}

static void
on_stream_volume_notify (GObject        *object,
                         GParamSpec     *pspec,
                         GvcMixerDialog *dialog)
{
        MateMixerStream *stream;
        GtkWidget      *bar;
        GtkAdjustment  *adj;

        stream = MATE_MIXER_STREAM (object);

        bar = lookup_bar_for_stream (dialog, stream);

        if (bar == NULL) {
                g_warning ("Unable to find bar for stream %s in on_stream_volume_notify()",
                           mate_mixer_stream_get_name (stream));
                return;
        }

        adj = GTK_ADJUSTMENT (gvc_channel_bar_get_adjustment (GVC_CHANNEL_BAR (bar)));

        g_signal_handlers_block_by_func (adj,
                                         on_adjustment_value_changed,
                                         dialog);

        gtk_adjustment_set_value (adj,
                                  mate_mixer_stream_get_volume (stream));

        g_signal_handlers_unblock_by_func (adj,
                                           on_adjustment_value_changed,
                                           dialog);
}

static void
on_stream_mute_notify (GObject        *object,
                       GParamSpec     *pspec,
                       GvcMixerDialog *dialog)
{
        MateMixerStream *stream;
        GtkWidget      *bar;
        gboolean        is_muted;

        stream = MATE_MIXER_STREAM (object);
        bar = lookup_bar_for_stream (dialog, stream);

        if (bar == NULL) {
                g_warning ("Unable to find bar for stream %s in on_stream_is_muted_notify()",
                           mate_mixer_stream_get_name (stream));
                return;
        }

        is_muted = mate_mixer_stream_get_mute (stream);
        gvc_channel_bar_set_is_muted (GVC_CHANNEL_BAR (bar), is_muted);

        if (stream == mate_mixer_control_get_default_output_stream (dialog->priv->control)) {
                gtk_widget_set_sensitive (dialog->priv->applications_box,
                                          !is_muted);
        }

}

static void
save_bar_for_stream (GvcMixerDialog  *dialog,
                     MateMixerStream *stream,
                     GtkWidget       *bar)
{
        g_hash_table_insert (dialog->priv->bars,
                             (gpointer) mate_mixer_stream_get_name (stream),
                             bar);
}

static GtkWidget *
create_bar (GvcMixerDialog *dialog,
            GtkSizeGroup   *size_group,
            gboolean        symmetric)
{
        GtkWidget *bar;

        bar = gvc_channel_bar_new ();
        gtk_widget_set_sensitive (bar, FALSE);
        if (size_group != NULL) {
                gvc_channel_bar_set_size_group (GVC_CHANNEL_BAR (bar),
                                                size_group,
                                                symmetric);
        }
        gvc_channel_bar_set_orientation (GVC_CHANNEL_BAR (bar),
                                         GTK_ORIENTATION_HORIZONTAL);
        gvc_channel_bar_set_show_mute (GVC_CHANNEL_BAR (bar),
                                       TRUE);
        g_signal_connect (bar,
                          "notify::is-muted",
                          G_CALLBACK (on_bar_is_muted_notify),
                          dialog);
        return bar;
}

static void
bar_set_stream (GvcMixerDialog *dialog, GtkWidget *bar, MateMixerStream *stream)
{
        GtkAdjustment   *adj;
        MateMixerStream *old_stream;

        old_stream = g_object_get_data (G_OBJECT (bar), "gvc-mixer-dialog-stream");
        if (old_stream != NULL) {
                char *name;

                g_object_get (bar, "name", &name, NULL);
                g_debug ("Disconnecting old stream '%s' from bar '%s'",
                         mate_mixer_stream_get_name (old_stream), name);
                g_free (name);

                g_signal_handlers_disconnect_by_func (old_stream, on_stream_mute_notify, dialog);
                g_signal_handlers_disconnect_by_func (old_stream, on_stream_volume_notify, dialog);
                g_signal_handlers_disconnect_by_func (old_stream, on_stream_port_notify, dialog);

                g_hash_table_remove (dialog->priv->bars, name);
        }

        gtk_widget_set_sensitive (bar, (stream != NULL));

        adj = GTK_ADJUSTMENT (gvc_channel_bar_get_adjustment (GVC_CHANNEL_BAR (bar)));

        // XXX already done in notify
        g_signal_handlers_disconnect_by_func (adj, on_adjustment_value_changed, dialog);

        g_object_set_data (G_OBJECT (bar), "gvc-mixer-dialog-stream", stream);
        g_object_set_data (G_OBJECT (adj), "gvc-mixer-dialog-stream", stream);
        g_object_set_data (G_OBJECT (adj), "gvc-mixer-dialog-bar", bar);

        if (stream != NULL) {
                gboolean is_muted;

                is_muted = mate_mixer_stream_get_mute (stream);
                gvc_channel_bar_set_is_muted (GVC_CHANNEL_BAR (bar), is_muted);

                save_bar_for_stream (dialog, stream, bar);

                gtk_adjustment_set_value (adj, mate_mixer_stream_get_volume (stream));

                g_signal_connect (stream,
                                  "notify::mute",
                                  G_CALLBACK (on_stream_mute_notify),
                                  dialog);
                g_signal_connect (stream,
                                  "notify::volume",
                                  G_CALLBACK (on_stream_volume_notify),
                                  dialog);
                g_signal_connect (stream,
                                  "notify::active-port",
                                  G_CALLBACK (on_stream_port_notify),
                                  dialog);
                g_signal_connect (adj,
                                  "value-changed",
                                  G_CALLBACK (on_adjustment_value_changed),
                                  dialog);
        }
}

static void
add_stream (GvcMixerDialog *dialog, MateMixerStream *stream)
{
        GtkWidget             *bar = NULL;
        gboolean               is_default = FALSE;
        MateMixerClientStream *client = NULL;
        MateMixerStreamFlags   flags;

        flags = mate_mixer_stream_get_flags (stream);

        if (flags & MATE_MIXER_STREAM_EVENT)
                return;
        if (flags & MATE_MIXER_STREAM_APPLICATION) {
                const gchar *app_id;

                client = MATE_MIXER_CLIENT_STREAM (stream);
                app_id = mate_mixer_client_stream_get_app_id (client);

                /* These applications are not displayed on the Applications tab */
                if (!g_strcmp0 (app_id, "org.mate.VolumeControl") ||
                    !g_strcmp0 (app_id, "org.gnome.VolumeControl") ||
                    !g_strcmp0 (app_id, "org.PulseAudio.pavucontrol"))
                        return;
        }

        if (client == NULL) {
                GtkTreeModel    *model;
                GtkTreeIter      iter;
                MateMixerStream *input;
                MateMixerStream *output;
                GtkAdjustment   *adj;

                input  = mate_mixer_control_get_default_input_stream (dialog->priv->control);
                output = mate_mixer_control_get_default_output_stream (dialog->priv->control);

                if (flags & MATE_MIXER_STREAM_INPUT) {
                        if (stream == input) {
                                bar = dialog->priv->input_bar;
                                adj = GTK_ADJUSTMENT (gvc_channel_bar_get_adjustment (GVC_CHANNEL_BAR (bar)));

                                g_signal_handlers_disconnect_by_func (G_OBJECT (adj),
                                                                      on_adjustment_value_changed,
                                                                      dialog);
                                update_input_settings (dialog);
                                is_default = TRUE;
                        }

                        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->input_treeview));

                        gtk_list_store_append (GTK_LIST_STORE (model), &iter);
                        gtk_list_store_set (GTK_LIST_STORE (model),
                                            &iter,
                                            NAME_COLUMN, mate_mixer_stream_get_name (stream),
                                            DESCRIPTION_COLUMN, mate_mixer_stream_get_description (stream),
                                            DEVICE_COLUMN, "",
                                            ACTIVE_COLUMN, is_default,
                                            -1);
                        g_signal_connect (stream,
                                          "notify::description",
                                          G_CALLBACK (on_stream_description_notify),
                                          dialog);
                } else if (flags & MATE_MIXER_STREAM_OUTPUT) {
                        if (stream == output) {
                                bar = dialog->priv->output_bar;
                                adj = GTK_ADJUSTMENT (gvc_channel_bar_get_adjustment (GVC_CHANNEL_BAR (bar)));

                                gtk_widget_set_sensitive (dialog->priv->applications_box,
                                                          mate_mixer_stream_get_mute (stream) == FALSE);

                                g_signal_handlers_disconnect_by_func (G_OBJECT (adj),
                                                                      on_adjustment_value_changed,
                                                                      dialog);
                                update_output_settings (dialog);
                                is_default = TRUE;
                        }

                        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->output_treeview));

                        gtk_list_store_append (GTK_LIST_STORE (model), &iter);
                        gtk_list_store_set (GTK_LIST_STORE (model),
                                            &iter,
                                            NAME_COLUMN, mate_mixer_stream_get_name (stream),
                                            DESCRIPTION_COLUMN, mate_mixer_stream_get_description (stream),
                                            DEVICE_COLUMN, "",
                                            ACTIVE_COLUMN, is_default,
                                            SPEAKERS_COLUMN, mvc_channel_map_to_pretty_string (stream),
                                            -1);
                        g_signal_connect (stream,
                                          "notify::description",
                                          G_CALLBACK (on_stream_description_notify),
                                          dialog);
                }
        } else {
                const gchar *name;
                const gchar *icon;

                bar = create_bar (dialog, dialog->priv->apps_size_group, FALSE);

                // FIXME:
                // 1) We should ideally provide application name on the first line and
                //    description on a second line in italics
                // 2) We should watch for name changes as it may include for example
                //    the name of the current song
                name = mate_mixer_client_stream_get_app_name (client);
                if (name == NULL)
                        name = mate_mixer_stream_get_description (stream);
                if (name == NULL)
                        name = mate_mixer_stream_get_name (stream);

                if (name == NULL || strchr (name, '_') == NULL) {
                        gvc_channel_bar_set_name (GVC_CHANNEL_BAR (bar), name);
                } else {
                        char **tokens, *escaped;

                        // XXX why is this here?
                        tokens = g_strsplit (name, "_", -1);
                        escaped = g_strjoinv ("__", tokens);
                        g_strfreev (tokens);
                        gvc_channel_bar_set_name (GVC_CHANNEL_BAR (bar), escaped);
                        g_free (escaped);
                }

                icon = mate_mixer_client_stream_get_app_icon (client);
                if (icon == NULL) {
                        /* If there is no name of the application icon, set a default */
                        icon = "applications-multimedia";
                }

                gvc_channel_bar_set_icon_name (GVC_CHANNEL_BAR (bar), icon);

                gtk_box_pack_start (GTK_BOX (dialog->priv->applications_box), bar, FALSE, FALSE, 12);
                gtk_widget_hide (dialog->priv->no_apps_label);

                dialog->priv->num_apps++;
        }

        if (bar != NULL) {
                bar_set_stream (dialog, bar, stream);
                gtk_widget_show (bar);
        }
}

static void
on_control_stream_added (MateMixerControl *control,
                         const gchar      *name,
                         GvcMixerDialog   *dialog)
{
        MateMixerStream *stream;
        GtkWidget       *bar;

        bar = g_hash_table_lookup (dialog->priv->bars, name);
        if (bar != NULL) {
                g_debug ("Ignoring already known stream %s", name);
                return;
        }

        stream = mate_mixer_control_get_stream (control, name);
        if (G_LIKELY (stream != NULL))
                add_stream (dialog, stream);
}

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
remove_stream (GvcMixerDialog *dialog, const gchar *name)
{
        GtkWidget    *bar;
        gboolean      found;
        GtkTreeIter   iter;
        GtkTreeModel *model;

        /* remove bars for applications and reset fixed bars */
        bar = g_hash_table_lookup (dialog->priv->bars, name);
        if (bar == dialog->priv->output_bar
            || bar == dialog->priv->input_bar
            || bar == dialog->priv->effects_bar) { // XXX effects bad? verify this!
                char *bar_name;
                g_object_get (bar, "name", &bar_name, NULL);
                g_debug ("Removing stream for bar '%s'", bar_name);
                g_free (bar_name);
                bar_set_stream (dialog, bar, NULL);
        } else if (bar != NULL) {
                g_hash_table_remove (dialog->priv->bars, name);
                gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (bar)),
                                      bar);
                dialog->priv->num_apps--;
                if (dialog->priv->num_apps == 0) {
                        gtk_widget_show (dialog->priv->no_apps_label);
                }
        }

        /* remove from any models */
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->output_treeview));
        found = find_tree_item_by_name (GTK_TREE_MODEL (model), name, NAME_COLUMN, &iter);
        if (found) {
                gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        }
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->input_treeview));
        found = find_tree_item_by_name (GTK_TREE_MODEL (model), name, NAME_COLUMN, &iter);
        if (found) {
                gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        }
}

static void
on_control_stream_removed (GvcMixerControl *control,
                           const gchar     *name,
                           GvcMixerDialog  *dialog)
{
        remove_stream (dialog, name);
}

static gchar *
card_profile_status (MateMixerDeviceProfile *profile)
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

        if (inputs_str == NULL && outputs_str == NULL) {
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
add_device (GvcMixerDialog *dialog, MateMixerDevice *device)
{
        GtkTreeModel        *model;
        GtkTreeIter          iter;
        GtkTreeSelection    *selection;
        MateMixerDeviceProfile *profile;
        GIcon               *icon;
        const gchar *name;
        const gchar *profile_name;
        gchar *profile_status;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->hw_treeview));
        name = mate_mixer_device_get_name (device);

        if (find_tree_item_by_name (GTK_TREE_MODEL (model), name, HW_NAME_COLUMN, &iter) == FALSE)
                gtk_list_store_append (GTK_LIST_STORE (model), &iter);

        profile = mate_mixer_device_get_active_profile (device);
        g_assert (profile != NULL);
        icon = g_themed_icon_new_with_default_fallbacks (mate_mixer_device_get_icon (device));

        //FIXME we need the status (default for a profile?) here

        profile_name = mate_mixer_device_profile_get_name (profile);
        profile_status = card_profile_status (profile);

        gtk_list_store_set (GTK_LIST_STORE (model),
                            &iter,
                            HW_NAME_COLUMN, mate_mixer_device_get_name (device),
                            HW_DESCRIPTION_COLUMN, mate_mixer_device_get_description (device),
                            HW_ICON_COLUMN, icon,
                            HW_PROFILE_COLUMN, profile_name,
                            HW_PROFILE_HUMAN_COLUMN, mate_mixer_device_profile_get_description (profile),
                            HW_STATUS_COLUMN, profile_status,
                            HW_SENSITIVE_COLUMN, g_strcmp0 (profile_name, "off") != 0,
                            -1);

        g_free (profile_status);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->priv->hw_treeview));

        if (gtk_tree_selection_get_selected (selection, NULL, NULL) == FALSE) {
                /* Select the currently added item if nothing is selected now */
                gtk_tree_selection_select_iter (selection, &iter);
        } else if (dialog->priv->hw_profile_combo != NULL) {
                MateMixerDevice *selected;

                /* Set the current profile if it changed for the selected card */
                selected = g_object_get_data (G_OBJECT (dialog->priv->hw_profile_combo), "device");

                if (!g_strcmp0 (mate_mixer_device_get_name (device),
                                mate_mixer_device_get_name (selected))) {
                        gvc_combo_box_set_active (GVC_COMBO_BOX (dialog->priv->hw_profile_combo),
                                                  profile_name);

                        g_object_set (G_OBJECT (dialog->priv->hw_profile_combo),
                                      "show-button",
                                      mate_mixer_device_profile_get_num_output_streams (profile) >= 1,
                                      NULL);
                }
        }
}

static void
on_control_device_added (MateMixerControl *control, const gchar *name, GvcMixerDialog *dialog)
{
        MateMixerDevice *device;

        device = mate_mixer_control_get_device (control, name);
        if (device != NULL)
                add_device (dialog, device);
}

static void
remove_card (GvcMixerDialog  *dialog, const gchar *name)
{
        gboolean      found;
        GtkTreeIter   iter;
        GtkTreeModel *model;

        /* remove from any models */
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->hw_treeview));
        found = find_tree_item_by_name (GTK_TREE_MODEL (model), name, HW_NAME_COLUMN, &iter);
        if (found) {
                gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
        }
}

static void
on_control_device_removed (GvcMixerControl *control,
                           const gchar     *name,
                           GvcMixerDialog  *dialog)
{
        remove_card (dialog, name);
}

static void
_gtk_label_make_bold (GtkLabel *label)
{
        PangoFontDescription *font_desc;

        font_desc = pango_font_description_new ();

        pango_font_description_set_weight (font_desc,
                                           PANGO_WEIGHT_BOLD);

        /* This will only affect the weight of the font, the rest is
         * from the current state of the widget, which comes from the
         * theme or user prefs, since the font desc only has the
         * weight flag turned on.
         */
        gtk_widget_modify_font (GTK_WIDGET (label), font_desc);

        pango_font_description_free (font_desc);
}

static void
on_input_radio_toggled (GtkCellRendererToggle *renderer,
                        char                  *path_str,
                        GvcMixerDialog        *dialog)
{
        GtkTreeModel *model;
        GtkTreeIter   iter;
        GtkTreePath  *path;
        gboolean      toggled;
        gchar        *name;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->input_treeview));

        path = gtk_tree_path_new_from_string (path_str);
        gtk_tree_model_get_iter (model, &iter, path);
        gtk_tree_path_free (path);

        gtk_tree_model_get (model, &iter,
                            NAME_COLUMN, &name,
                            ACTIVE_COLUMN, &toggled,
                            -1);

        if (toggled ^ 1) {
                MateMixerStream *stream;

                g_debug ("Default input selected: %s", name);
                stream = mate_mixer_control_get_stream (dialog->priv->control, name);

                if (stream != NULL)
                        mate_mixer_control_set_default_input_stream (dialog->priv->control,
                                                                     stream);
        }
        g_free (name);
}

static void
on_output_radio_toggled (GtkCellRendererToggle *renderer,
                         char                  *path_str,
                         GvcMixerDialog        *dialog)
{
        GtkTreeModel *model;
        GtkTreeIter   iter;
        GtkTreePath  *path;
        gboolean      toggled;
        gchar        *name;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->output_treeview));

        path = gtk_tree_path_new_from_string (path_str);
        gtk_tree_model_get_iter (model, &iter, path);
        gtk_tree_path_free (path);

        gtk_tree_model_get (model, &iter,
                            NAME_COLUMN, &name,
                            ACTIVE_COLUMN, &toggled,
                            -1);

        if (toggled ^ 1) {
                MateMixerStream *stream;

                g_debug ("Default output selected: %s", name);
                stream = mate_mixer_control_get_stream (dialog->priv->control, name);
                if (stream != NULL)
                        mate_mixer_control_set_default_output_stream (dialog->priv->control,
                                                                      stream);
        }
        g_free (name);
}

static void
name_to_text (GtkTreeViewColumn *column,
              GtkCellRenderer *cell,
              GtkTreeModel *model,
              GtkTreeIter *iter,
              gpointer user_data)
{
        char *description, *mapping;

        gtk_tree_model_get (model, iter,
                            DESCRIPTION_COLUMN, &description,
                            SPEAKERS_COLUMN, &mapping,
                            -1);

        if (mapping == NULL) {
                g_object_set (cell, "text", description, NULL);
        } else {
                gchar *str = g_strdup_printf ("%s\n<i>%s</i>", description, mapping);

                g_object_set (cell, "markup", str, NULL);
                g_free (str);
        }

        g_free (description);
        g_free (mapping);
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
                                    G_TYPE_STRING,
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

        g_signal_connect (renderer,
                          "toggled",
                          G_CALLBACK (on_toggled),
                          dialog);

        gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (treeview), -1,
                                                    _("Name"),
                                                    gtk_cell_renderer_text_new (),
                                                    name_to_text, NULL, NULL);

#if 0
        renderer = gtk_cell_renderer_text_new ();
        column = gtk_tree_view_column_new_with_attributes (_("Device"),
                                                           renderer,
                                                           "text", DEVICE_COLUMN,
                                                           NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
#endif
        return treeview;
}

static void
on_profile_changed (GvcComboBox *widget, const gchar *profile, gpointer user_data)
{
        MateMixerDevice *device;

        device = g_object_get_data (G_OBJECT (widget), "device");
        if (device == NULL) {
                g_warning ("Could not find card for combobox");
                return;
        }

        g_debug ("Profile changed to %s for device %s",
                 profile,
                 mate_mixer_device_get_name (device));

        mate_mixer_device_set_active_profile (device, profile);
}

static void
on_test_speakers_clicked (GvcComboBox *widget, gpointer user_data)
{
        GvcMixerDialog      *dialog = GVC_MIXER_DIALOG (user_data);
        MateMixerDevice     *device;
        GtkWidget           *d, *speaker_test, *container;
        char                *title;

        device = g_object_get_data (G_OBJECT (widget), "device");
        if (device == NULL) {
                g_warning ("Could not find card for combobox");
                return;
        }

        title = g_strdup_printf (_("Speaker Testing for %s"), mate_mixer_device_get_name (device));
        d = gtk_dialog_new_with_buttons (title,
                                         GTK_WINDOW (dialog),
                                         GTK_DIALOG_MODAL |
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         NULL);
        g_free (title);
        speaker_test = gvc_speaker_test_new (dialog->priv->control, device);

        gtk_widget_show (speaker_test);
        container = gtk_dialog_get_content_area (GTK_DIALOG (d));
        gtk_container_add (GTK_CONTAINER (container), speaker_test);

        gtk_dialog_run (GTK_DIALOG (d));
        gtk_widget_destroy (d);
}

static void
on_card_selection_changed (GtkTreeSelection *selection,
                           gpointer          user_data)
{
        GvcMixerDialog      *dialog = GVC_MIXER_DIALOG (user_data);
        GtkTreeModel        *model;
        GtkTreeIter          iter;
        const GList         *profiles;
        gchar              *name;
        MateMixerDevice *device;
        MateMixerDeviceProfile *profile;

        g_debug ("Card selection changed");

        if (dialog->priv->hw_profile_combo != NULL) {
                gtk_container_remove (GTK_CONTAINER (dialog->priv->hw_settings_box),
                                      dialog->priv->hw_profile_combo);
                dialog->priv->hw_profile_combo = NULL;
        }

        if (gtk_tree_selection_get_selected (selection, NULL, &iter) == FALSE)
                return;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->hw_treeview));
        gtk_tree_model_get (model, &iter,
                            HW_NAME_COLUMN, &name,
                            -1);

        device = mate_mixer_control_get_device (dialog->priv->control, name);
        if (device == NULL) {
                g_warning ("Unable to find card for id: %s", name);
                g_free (name);
                return;
        }
        g_free (name);

        profile  = mate_mixer_device_get_active_profile (device);
        profiles = mate_mixer_device_list_profiles (device);

        dialog->priv->hw_profile_combo = gvc_combo_box_new (_("_Profile:"));

        g_object_set (G_OBJECT (dialog->priv->hw_profile_combo),
                      "button-label",
                      _("Test Speakers"),
                      NULL);

        gvc_combo_box_set_profiles (GVC_COMBO_BOX (dialog->priv->hw_profile_combo),
                                    profiles);
        gvc_combo_box_set_active (GVC_COMBO_BOX (dialog->priv->hw_profile_combo),
                                  mate_mixer_device_profile_get_name (profile));

        gtk_box_pack_start (GTK_BOX (dialog->priv->hw_settings_box),
                            dialog->priv->hw_profile_combo,
                            TRUE, TRUE, 6);

        g_object_set (G_OBJECT (dialog->priv->hw_profile_combo),
                      "show-button",
                      mate_mixer_device_profile_get_num_output_streams (profile) >= 1,
                      NULL);

        gtk_widget_show (dialog->priv->hw_profile_combo);

        g_object_set_data (G_OBJECT (dialog->priv->hw_profile_combo),
                           "device",
                           g_object_ref (device));

        g_signal_connect (G_OBJECT (dialog->priv->hw_profile_combo),
                          "changed",
                          G_CALLBACK (on_profile_changed),
                          dialog);
        g_signal_connect (G_OBJECT (dialog->priv->hw_profile_combo),
                          "button-clicked",
                          G_CALLBACK (on_test_speakers_clicked),
                          dialog);
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
card_to_text (GtkTreeViewColumn *column,
              GtkCellRenderer *cell,
              GtkTreeModel *model,
              GtkTreeIter *iter,
              gpointer user_data)
{
        gchar *description, *status, *profile, *str;
        gboolean sensitive;

        gtk_tree_model_get (model, iter,
                            HW_DESCRIPTION_COLUMN, &description,
                            HW_STATUS_COLUMN, &status,
                            HW_PROFILE_HUMAN_COLUMN, &profile,
                            HW_SENSITIVE_COLUMN, &sensitive,
                            -1);

        str = g_strdup_printf ("%s\n<i>%s</i>\n<i>%s</i>", description, status, profile);
        g_object_set (cell,
                      "markup", str,
                      "sensitive", sensitive,
                      NULL);
        g_free (str);

        g_free (description);
        g_free (status);
        g_free (profile);
}

static GtkWidget *
create_cards_treeview (GvcMixerDialog *dialog, GCallback on_changed)
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
                                    G_TYPE_STRING,
                                    G_TYPE_STRING,
                                    G_TYPE_BOOLEAN);

        gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
                                 GTK_TREE_MODEL (store));

        renderer = gtk_cell_renderer_pixbuf_new ();
        g_object_set (G_OBJECT (renderer), "stock-size", GTK_ICON_SIZE_DIALOG, NULL);

        column = gtk_tree_view_column_new_with_attributes (NULL,
                                                           renderer,
                                                           "gicon", HW_ICON_COLUMN,
                                                           "sensitive", HW_SENSITIVE_COLUMN,
                                                           NULL);

        gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

        gtk_tree_view_insert_column_with_data_func (GTK_TREE_VIEW (treeview), -1,
                                                    _("Name"),
                                                    gtk_cell_renderer_text_new (),
                                                    card_to_text,
                                                    NULL, NULL);

        return treeview;
}

static const guint tab_accel_keys[] = {
        GDK_1, GDK_2, GDK_3, GDK_4, GDK_5
};

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
        GList            *list;
        GtkTreeSelection *selection;
        GtkAccelGroup    *accel_group;
        GClosure         *closure;
        gint             i;

        object = G_OBJECT_CLASS (gvc_mixer_dialog_parent_class)->constructor (type,
                                                                              n_construct_properties,
                                                                              construct_params);

        self = GVC_MIXER_DIALOG (object);
        gtk_dialog_add_button (GTK_DIALOG (self), "gtk-close", GTK_RESPONSE_OK);

        main_vbox = gtk_dialog_get_content_area (GTK_DIALOG (self));
        gtk_box_set_spacing (GTK_BOX (main_vbox), 2);

        gtk_container_set_border_width (GTK_CONTAINER (self), 6);

        self->priv->output_stream_box = gtk_hbox_new (FALSE, 12);
        alignment = gtk_alignment_new (0, 0, 1, 1);
        gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 12, 0, 0, 0);
        gtk_container_add (GTK_CONTAINER (alignment), self->priv->output_stream_box);
        gtk_box_pack_start (GTK_BOX (main_vbox),
                            alignment,
                            FALSE, FALSE, 0);
        self->priv->output_bar = create_bar (self, self->priv->size_group, TRUE);
        gvc_channel_bar_set_name (GVC_CHANNEL_BAR (self->priv->output_bar),
                                  _("_Output volume: "));
        gtk_widget_set_sensitive (self->priv->output_bar, FALSE);
        gtk_box_pack_start (GTK_BOX (self->priv->output_stream_box),
                            self->priv->output_bar, TRUE, TRUE, 12);

        self->priv->notebook = gtk_notebook_new ();
        gtk_box_pack_start (GTK_BOX (main_vbox),
                            self->priv->notebook,
                            TRUE, TRUE, 0);

        gtk_container_set_border_width (GTK_CONTAINER (self->priv->notebook), 5);

        /* Set up accels (borrowed from Empathy) */
        accel_group = gtk_accel_group_new ();
        gtk_window_add_accel_group (GTK_WINDOW (self), accel_group);

        for (i = 0; i < G_N_ELEMENTS (tab_accel_keys); i++) {
                closure =  g_cclosure_new (G_CALLBACK (dialog_accel_cb),
                                           self,
                                           NULL);
                gtk_accel_group_connect (accel_group,
                                         tab_accel_keys[i],
                                         GDK_MOD1_MASK,
                                         0,
                                         closure);
        }

        g_object_unref (accel_group);

        /* Effects page */
        self->priv->sound_effects_box = gtk_vbox_new (FALSE, 6);
        gtk_container_set_border_width (GTK_CONTAINER (self->priv->sound_effects_box), 12);
        label = gtk_label_new (_("Sound Effects"));
        gtk_notebook_append_page (GTK_NOTEBOOK (self->priv->notebook),
                                  self->priv->sound_effects_box,
                                  label);

        self->priv->effects_bar = create_bar (self, self->priv->size_group, TRUE);
        gvc_channel_bar_set_name (GVC_CHANNEL_BAR (self->priv->effects_bar),
                                  _("_Alert volume: "));
        gtk_widget_set_sensitive (self->priv->effects_bar, FALSE);
        gtk_box_pack_start (GTK_BOX (self->priv->sound_effects_box),
                            self->priv->effects_bar, FALSE, FALSE, 0);

        self->priv->sound_theme_chooser = gvc_sound_theme_chooser_new ();
        gtk_box_pack_start (GTK_BOX (self->priv->sound_effects_box),
                            self->priv->sound_theme_chooser,
                            TRUE, TRUE, 6);

        /* Hardware page */
        self->priv->hw_box = gtk_vbox_new (FALSE, 12);
        gtk_container_set_border_width (GTK_CONTAINER (self->priv->hw_box), 12);
        label = gtk_label_new (_("Hardware"));
        gtk_notebook_append_page (GTK_NOTEBOOK (self->priv->notebook),
                                  self->priv->hw_box,
                                  label);

        box = gtk_frame_new (_("C_hoose a device to configure:"));
        label = gtk_frame_get_label_widget (GTK_FRAME (box));
        _gtk_label_make_bold (GTK_LABEL (label));
        gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
        gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (self->priv->hw_box), box, TRUE, TRUE, 0);

        alignment = gtk_alignment_new (0, 0, 1, 1);
        gtk_container_add (GTK_CONTAINER (box), alignment);
        gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 0, 0);

        self->priv->hw_treeview = create_cards_treeview (self,
                                                         G_CALLBACK (on_card_selection_changed));
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
        _gtk_label_make_bold (GTK_LABEL (label));
        gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (self->priv->hw_box), box, FALSE, TRUE, 12);
        self->priv->hw_settings_box = gtk_vbox_new (FALSE, 12);
        gtk_container_add (GTK_CONTAINER (box), self->priv->hw_settings_box);

        /* Input page */
        self->priv->input_box = gtk_vbox_new (FALSE, 12);
        gtk_container_set_border_width (GTK_CONTAINER (self->priv->input_box), 12);
        label = gtk_label_new (_("Input"));
        gtk_notebook_append_page (GTK_NOTEBOOK (self->priv->notebook),
                                  self->priv->input_box,
                                  label);

        self->priv->input_bar = create_bar (self, self->priv->size_group, TRUE);
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

        box = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start (GTK_BOX (self->priv->input_box),
                            box,
                            FALSE, FALSE, 6);

        sbox = gtk_hbox_new (FALSE, 6);
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

        ebox = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start (GTK_BOX (box),
                            ebox,
                            FALSE, FALSE, 0);
        gtk_size_group_add_widget (self->priv->size_group, ebox);

        self->priv->input_settings_box = gtk_hbox_new (FALSE, 6);
        gtk_box_pack_start (GTK_BOX (self->priv->input_box),
                            self->priv->input_settings_box,
                            FALSE, FALSE, 0);

        box = gtk_frame_new (_("C_hoose a device for sound input:"));
        label = gtk_frame_get_label_widget (GTK_FRAME (box));
        _gtk_label_make_bold (GTK_LABEL (label));
        gtk_label_set_use_underline (GTK_LABEL (label), TRUE);
        gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (self->priv->input_box), box, TRUE, TRUE, 0);

        alignment = gtk_alignment_new (0, 0, 1, 1);
        gtk_container_add (GTK_CONTAINER (box), alignment);
        gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 0, 0);

        self->priv->input_treeview = create_stream_treeview (self,
                                                             G_CALLBACK (on_input_radio_toggled));
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
        _gtk_label_make_bold (GTK_LABEL (label));
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
        _gtk_label_make_bold (GTK_LABEL (label));
        gtk_frame_set_shadow_type (GTK_FRAME (box), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (self->priv->output_box), box, FALSE, FALSE, 12);
        self->priv->output_settings_box = gtk_vbox_new (FALSE, 0);
        gtk_container_add (GTK_CONTAINER (box), self->priv->output_settings_box);

        /* Applications */
        self->priv->applications_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->priv->applications_scrolled_window),
                                        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (self->priv->applications_scrolled_window),
                                             GTK_SHADOW_IN);
        self->priv->applications_box = gtk_vbox_new (FALSE, 12);
        gtk_container_set_border_width (GTK_CONTAINER (self->priv->applications_box), 12);
        gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (self->priv->applications_scrolled_window),
                                               self->priv->applications_box);
        label = gtk_label_new (_("Applications"));
        gtk_notebook_append_page (GTK_NOTEBOOK (self->priv->notebook),
                                  self->priv->applications_scrolled_window,
                                  label);
        self->priv->no_apps_label = gtk_label_new (_("No application is currently playing or recording audio."));
        gtk_box_pack_start (GTK_BOX (self->priv->applications_box),
                            self->priv->no_apps_label,
                            TRUE, TRUE, 0);

        gtk_widget_show_all (main_vbox);

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

        g_signal_connect (G_OBJECT (self->priv->notebook),
                          "switch-page",
                          G_CALLBACK (on_notebook_switch_page),
                          self);

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

        g_signal_connect (dialog->priv->control,
                          "stream-added",
                          G_CALLBACK (on_control_stream_added),
                          dialog);
        g_signal_connect (dialog->priv->control,
                          "stream-removed",
                          G_CALLBACK (on_control_stream_removed),
                          dialog);
        g_signal_connect (dialog->priv->control,
                          "device-added",
                          G_CALLBACK (on_control_device_added),
                          dialog);
        g_signal_connect (dialog->priv->control,
                          "device-removed",
                          G_CALLBACK (on_control_device_removed),
                          dialog);

        g_signal_connect (dialog->priv->control,
                          "notify::default-output",
                          G_CALLBACK (on_control_default_output_notify),
                          dialog);
        g_signal_connect (dialog->priv->control,
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
        case PROP_MIXER_CONTROL:
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
        case PROP_MIXER_CONTROL:
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
                                         PROP_MIXER_CONTROL,
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
        dialog->priv->apps_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
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
                        num = PAGE_EVENTS;
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
