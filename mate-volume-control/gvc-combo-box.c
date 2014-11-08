/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Bastien Nocera
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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gtk/gtk.h>

#include <libmatemixer/matemixer.h>

#include "gvc-combo-box.h"

#define GVC_COMBO_BOX_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GVC_TYPE_COMBO_BOX, GvcComboBoxPrivate))

struct _GvcComboBoxPrivate
{
        GtkWidget     *drop_box;
        GtkWidget     *start_box;
        GtkWidget     *end_box;
        GtkWidget     *label;
        GtkWidget     *button;
        GtkTreeModel  *model;
        GtkWidget     *combobox;
        gboolean       set_called;
        GtkSizeGroup  *size_group;
        gboolean       symmetric;
};

enum {
        COL_NAME,
        COL_HUMAN_NAME,
        NUM_COLS
};

enum {
        CHANGED,
        BUTTON_CLICKED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

enum {
        PROP_0,
        PROP_LABEL,
        PROP_SHOW_BUTTON,
        PROP_BUTTON_LABEL,
        N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static void gvc_combo_box_class_init (GvcComboBoxClass *klass);
static void gvc_combo_box_init       (GvcComboBox      *combo);
static void gvc_combo_box_dispose    (GObject          *object);

#if GTK_CHECK_VERSION (3, 0, 0)
G_DEFINE_TYPE (GvcComboBox, gvc_combo_box, GTK_TYPE_BOX)
#else
G_DEFINE_TYPE (GvcComboBox, gvc_combo_box, GTK_TYPE_HBOX)
#endif

void
gvc_combo_box_set_size_group (GvcComboBox  *combobox,
                              GtkSizeGroup *group,
                              gboolean      symmetric)
{
        g_return_if_fail (GVC_IS_COMBO_BOX (combobox));
        g_return_if_fail (GTK_IS_SIZE_GROUP (group));

        combobox->priv->size_group = group;
        combobox->priv->symmetric = symmetric;

        if (combobox->priv->size_group != NULL) {
                gtk_size_group_add_widget (combobox->priv->size_group,
                                           combobox->priv->start_box);

                if (combobox->priv->symmetric)
                        gtk_size_group_add_widget (combobox->priv->size_group,
                                                   combobox->priv->end_box);
        }
        gtk_widget_queue_draw (GTK_WIDGET (combobox));
}

static void
gvc_combo_box_set_property (GObject       *object,
                            guint          prop_id,
                            const GValue  *value,
                            GParamSpec    *pspec)
{
        GvcComboBox *self = GVC_COMBO_BOX (object);

        switch (prop_id) {
        case PROP_LABEL:
                gtk_label_set_text_with_mnemonic (GTK_LABEL (self->priv->label), g_value_get_string (value));
                break;
        case PROP_BUTTON_LABEL:
                gtk_button_set_label (GTK_BUTTON (self->priv->button), g_value_get_string (value));
                break;
        case PROP_SHOW_BUTTON:
                gtk_widget_set_visible (self->priv->button, g_value_get_boolean (value));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_combo_box_get_property (GObject     *object,
                            guint        prop_id,
                            GValue      *value,
                            GParamSpec  *pspec)
{
        GvcComboBox *self = GVC_COMBO_BOX (object);

        switch (prop_id) {
        case PROP_LABEL:
                g_value_set_string (value, gtk_label_get_text (GTK_LABEL (self->priv->label)));
                break;
        case PROP_BUTTON_LABEL:
                g_value_set_string (value, gtk_button_get_label (GTK_BUTTON (self->priv->button)));
                break;
        case PROP_SHOW_BUTTON:
                g_value_set_boolean (value, gtk_widget_get_visible (self->priv->button));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gvc_combo_box_class_init (GvcComboBoxClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = gvc_combo_box_dispose;
        object_class->set_property = gvc_combo_box_set_property;
        object_class->get_property = gvc_combo_box_get_property;

        properties[PROP_LABEL] =
                g_param_spec_string ("label",
                                     "label",
                                     "The combo box label",
                                     _("_Profile:"),
                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

        properties[PROP_SHOW_BUTTON] =
                g_param_spec_boolean ("show-button",
                                      "show-button",
                                      "Whether to show the button",
                                      FALSE,
                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

        properties[PROP_BUTTON_LABEL] =
                g_param_spec_string ("button-label",
                                     "button-label",
                                     "The button's label",
                                     "",
                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

        g_object_class_install_properties (object_class, N_PROPERTIES, properties);

        signals[CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GvcComboBoxClass, changed),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE, 1, G_TYPE_STRING);

        signals[BUTTON_CLICKED] =
                g_signal_new ("button-clicked",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (GvcComboBoxClass, button_clicked),
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0, G_TYPE_NONE);

        g_type_class_add_private (klass, sizeof (GvcComboBoxPrivate));
}

void
gvc_combo_box_set_options (GvcComboBox *combobox, const GList *options)
{
        const GList *l;

        g_return_if_fail (GVC_IS_COMBO_BOX (combobox));
        g_return_if_fail (combobox->priv->set_called == FALSE);

        for (l = options; l != NULL; l = l->next) {
                MateMixerSwitchOption *option = MATE_MIXER_SWITCH_OPTION (l->data);

                gtk_list_store_insert_with_values (GTK_LIST_STORE (combobox->priv->model),
                                                   NULL,
                                                   G_MAXINT,
                                                   COL_NAME,
                                                   mate_mixer_switch_option_get_name (option),
                                                   COL_HUMAN_NAME,
                                                   mate_mixer_switch_option_get_label (option),
                                                   -1);
        }
        combobox->priv->set_called = TRUE;
}

void
gvc_combo_box_set_active (GvcComboBox *combobox, const gchar *id)
{
        GtkTreeIter iter;
        gboolean    cont;

        g_return_if_fail (GVC_IS_COMBO_BOX (combobox));
        g_return_if_fail (id != NULL);

        cont = gtk_tree_model_get_iter_first (combobox->priv->model, &iter);
        while (cont != FALSE) {
                gchar *name;

                gtk_tree_model_get (combobox->priv->model, &iter,
                                    COL_NAME, &name,
                                    -1);
                if (g_strcmp0 (name, id) == 0) {
                        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox->priv->combobox), &iter);
                        g_free (name);
                        return;
                }
                g_free (name);

                gtk_tree_model_iter_next (combobox->priv->model, &iter);
        }
        g_warning ("Could not find id '%s' in combo box", id);
}

static void
on_combo_box_changed (GtkComboBox *widget, GvcComboBox *combobox)
{
        GtkTreeIter iter;
        gchar      *profile;

        if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter) == FALSE) {
                g_warning ("Could not find an active profile or port");
                return;
        }

        gtk_tree_model_get (combobox->priv->model, &iter,
                            COL_NAME, &profile,
                            -1);

        g_signal_emit (combobox, signals[CHANGED], 0, profile);
        g_free (profile);
}

static void
on_combo_box_button_clicked (GtkButton *button, GvcComboBox *combobox)
{
        g_signal_emit (combobox, signals[BUTTON_CLICKED], 0);
}

static void
gvc_combo_box_init (GvcComboBox *combobox)
{
        GtkWidget       *frame;
        GtkCellRenderer *renderer;

        combobox->priv = GVC_COMBO_BOX_GET_PRIVATE (combobox);

        combobox->priv->model = GTK_TREE_MODEL (gtk_list_store_new (NUM_COLS,
                                                                    G_TYPE_STRING,
                                                                    G_TYPE_STRING));
        combobox->priv->label = gtk_label_new (NULL);

        gtk_misc_set_alignment (GTK_MISC (combobox->priv->label), 0.0, 0.5);

        /* Frame */
        frame = gtk_frame_new (NULL);
        gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
        gtk_box_pack_start (GTK_BOX (combobox), frame, TRUE, TRUE, 0);

#if GTK_CHECK_VERSION (3, 0, 0)
        combobox->priv->drop_box  = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        combobox->priv->start_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        combobox->priv->end_box   = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
#else
        combobox->priv->drop_box  = gtk_hbox_new (FALSE, 6);
        combobox->priv->start_box = gtk_hbox_new (FALSE, 6);
        combobox->priv->end_box   = gtk_hbox_new (FALSE, 6);
#endif
        combobox->priv->combobox  = gtk_combo_box_new_with_model (combobox->priv->model);

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox->priv->combobox),
                                    renderer,
                                    FALSE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combobox->priv->combobox),
                                       renderer,
                                       "text",
                                       COL_HUMAN_NAME);

#if GTK_CHECK_VERSION (3, 0, 0)
        /* Make sure the combo box does not get too long on long profile names */
        g_object_set (G_OBJECT (renderer),
                      "ellipsize",
                      PANGO_ELLIPSIZE_END,
                      NULL);

        gtk_combo_box_set_popup_fixed_width (GTK_COMBO_BOX (combobox->priv->combobox), FALSE);
#endif

        gtk_box_pack_start (GTK_BOX (combobox->priv->drop_box),
                            combobox->priv->start_box,
                            FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (combobox->priv->start_box),
                            combobox->priv->label,
                            FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (combobox->priv->drop_box),
                            combobox->priv->combobox,
                            TRUE, TRUE, 0);

        combobox->priv->button = gtk_button_new_with_label ("");

        gtk_box_pack_start (GTK_BOX (combobox->priv->drop_box),
                            combobox->priv->button,
                            FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (combobox->priv->drop_box),
                            combobox->priv->end_box,
                            FALSE, FALSE, 0);

        gtk_widget_set_no_show_all (combobox->priv->button, TRUE);

        if (combobox->priv->size_group != NULL) {
                gtk_size_group_add_widget (combobox->priv->size_group,
                                           combobox->priv->start_box);

                if (combobox->priv->symmetric)
                        gtk_size_group_add_widget (combobox->priv->size_group,
                                                   combobox->priv->end_box);
        }

        gtk_label_set_mnemonic_widget (GTK_LABEL (combobox->priv->label),
                                       combobox->priv->combobox);

        gtk_container_add (GTK_CONTAINER (frame), combobox->priv->drop_box);
        gtk_widget_show_all (frame);

        g_signal_connect (G_OBJECT (combobox->priv->combobox),
                          "changed",
                          G_CALLBACK (on_combo_box_changed),
                          combobox);
        g_signal_connect (G_OBJECT (combobox->priv->button),
                          "clicked",
                          G_CALLBACK (on_combo_box_button_clicked),
                          combobox);
}

static void
gvc_combo_box_dispose (GObject *object)
{
        GvcComboBox *combobox;

        combobox = GVC_COMBO_BOX (object);

        g_clear_object (&combobox->priv->model);

        G_OBJECT_CLASS (gvc_combo_box_parent_class)->dispose (object);
}

GtkWidget *
gvc_combo_box_new (const gchar *label)
{
        return g_object_new (GVC_TYPE_COMBO_BOX,
                             "label", label,
#if GTK_CHECK_VERSION (3, 0, 0)
                             "orientation", GTK_ORIENTATION_HORIZONTAL,
#endif
                             NULL);
}
