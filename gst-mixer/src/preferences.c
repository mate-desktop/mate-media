/* MATE Volume Control
 * Copyright (C) 2003-2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
 * preferences.c: preferences screen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include "element.h"
#include "preferences.h"
#include "schemas.h"
#include "track.h"
#include "misc.h"

enum {
  COL_ACTIVE,
  COL_LABEL,
  COL_TRACK,
  COL_TYPE,
  COL_PAGE,
  NUM_COLS
};

G_DEFINE_TYPE (MateVolumeControlPreferences, mate_volume_control_preferences, GTK_TYPE_DIALOG)

static void	mate_volume_control_preferences_class_init	(MateVolumeControlPreferencesClass *klass);
static void	mate_volume_control_preferences_init	(MateVolumeControlPreferences *prefs);
static void	mate_volume_control_preferences_dispose (GObject *object);
static void	mate_volume_control_preferences_response (GtkDialog *dialog,
							   gint       response_id);

static void	set_gsettings_track_active	(GSettings *settings, GstMixer *mixer, 
					 GstMixerTrack *track, gboolean active, gboolean is_whitelist);


static void	cb_toggle		(GtkCellRendererToggle *cell,
					 gchar                 *path_str,
					 MateVolumeControlPreferences *prefs);
static void	cb_activated		(GtkTreeView *view, GtkTreePath *path,
					 GtkTreeViewColumn *col, gpointer userdata);
static void	cb_gsettings		(GSettings     *settings,
					 gchar           *key,
					 MateVolumeControlPreferences *prefs);


static void
mate_volume_control_preferences_class_init (MateVolumeControlPreferencesClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkDialogClass *gtkdialog_class = (GtkDialogClass *) klass;

  gobject_class->dispose = mate_volume_control_preferences_dispose;
  gtkdialog_class->response = mate_volume_control_preferences_response;
}

/*
 * Mixer tracks are sorted by their types.
 */
static gint
sort_by_page_num (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	gint a_type, b_type;

	gtk_tree_model_get (model, a, COL_PAGE, &a_type, -1);
	gtk_tree_model_get (model, b, COL_PAGE, &b_type, -1);

	return (a_type - b_type);
}

static void
mate_volume_control_preferences_init (MateVolumeControlPreferences *prefs)
{
  GtkWidget *box, *label, *view;
  GtkListStore *store;
  GtkTreeSelection *sel;
  GtkTreeViewColumn *col;
  GtkCellRenderer *render;

  prefs->settings = NULL;
  prefs->mixer = NULL;

  /* make window look cute */
  gtk_window_set_title (GTK_WINDOW (prefs), _("Volume Control Preferences"));
#if !GTK_CHECK_VERSION (3, 0, 0)
  gtk_dialog_set_has_separator (GTK_DIALOG (prefs), FALSE);
#endif
  gtk_container_set_border_width (GTK_CONTAINER (prefs), 5);
  gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (prefs))), 2);
  gtk_dialog_add_buttons (GTK_DIALOG (prefs),
			  GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
			  /* help goes here (future) */
			  NULL);

  /* add a treeview for all the properties */
  box = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (box), 5);

  label = gtk_label_new_with_mnemonic (_("_Select mixers to be visible:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  store = gtk_list_store_new (NUM_COLS, G_TYPE_BOOLEAN,
			      G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_STRING,
			      G_TYPE_INT);
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (store), sort_by_page_num, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
  prefs->treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (prefs->treeview), FALSE);
  gtk_label_set_mnemonic_widget (GTK_LABEL(label), GTK_WIDGET (prefs->treeview));

  /* viewport for lots of tracks */
  view = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (view),
				  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (view),
				       GTK_SHADOW_IN);
  gtk_widget_set_size_request (view, -1, 250);

  gtk_container_add (GTK_CONTAINER (view), prefs->treeview);
  gtk_box_pack_start (GTK_BOX (box), view, TRUE, TRUE, 0);

  gtk_widget_show (prefs->treeview);
  gtk_widget_show (view);

  /* treeview internals */
  sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (prefs->treeview));
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);

  render = gtk_cell_renderer_toggle_new ();
  g_signal_connect (render, "toggled",
		    G_CALLBACK (cb_toggle), prefs);
  g_signal_connect (prefs->treeview, "row-activated",
		    G_CALLBACK (cb_activated), prefs);
  col = gtk_tree_view_column_new_with_attributes ("Active", render,
						  "active", COL_ACTIVE,
						  NULL);
  gtk_tree_view_column_set_clickable (col, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (prefs->treeview), col);

  render = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes ("Track name", render,
						  "text", COL_LABEL,
						  NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (prefs->treeview), col);

  render = gtk_cell_renderer_text_new ();
  col = gtk_tree_view_column_new_with_attributes ("Type", render,
                          "text", COL_TYPE,
						  NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (prefs->treeview), col);

  gtk_tree_view_set_search_column (GTK_TREE_VIEW (prefs->treeview), COL_LABEL);

  /* and show */
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (prefs))), box,
		      TRUE, TRUE, 0);
  gtk_widget_show (box);
}

GtkWidget *
mate_volume_control_preferences_new (GstElement  *element)
{
  MateVolumeControlPreferences *prefs;

  g_return_val_if_fail (GST_IS_MIXER (element), NULL);

  /* element */
  prefs = g_object_new (MATE_VOLUME_CONTROL_TYPE_PREFERENCES, NULL);
  prefs->settings = g_settings_new (MATE_VOLUME_CONTROL_SCHEMA);

  mate_volume_control_preferences_change (prefs, element);

  /* gsettings */
  g_signal_connect (prefs->settings, "changed::" MATE_VOLUME_CONTROL_KEY_SHOWN_ELEMENTS,
                    G_CALLBACK (cb_gsettings), prefs);

  return GTK_WIDGET (prefs);
}

static void
mate_volume_control_preferences_dispose (GObject *object)
{
  MateVolumeControlPreferences *prefs;

  prefs = MATE_VOLUME_CONTROL_PREFERENCES (object);

  if (prefs->settings) {
    g_object_unref (G_OBJECT (prefs->settings));
    prefs->settings = NULL;
  }

  if (prefs->mixer) {
    gst_object_unref (GST_OBJECT (prefs->mixer));
    prefs->mixer = NULL;
  }

  G_OBJECT_CLASS (mate_volume_control_preferences_parent_class)->dispose (object);
}

static void
mate_volume_control_preferences_response (GtkDialog *dialog,
					   gint       response_id)
{
  switch (response_id) {
    case GTK_RESPONSE_CLOSE:
      gtk_widget_destroy (GTK_WIDGET (dialog));
      break;

    default:
      break;
  }

  if (((GtkDialogClass *) mate_volume_control_preferences_parent_class)->response)
    ((GtkDialogClass *) mate_volume_control_preferences_parent_class)->response (dialog, response_id);
}

/*
 * Hide non-alphanumeric characters, for saving in gsettings.
 */

gchar *
get_gsettings_name (GstMixer *mixer, GstMixerTrack *track)
{
  const gchar *dev;
  gchar *res;
  gint i, pos;
  gchar *label = NULL;

  g_return_val_if_fail(mixer != NULL, NULL);

  dev = g_object_get_data (G_OBJECT (mixer),
			   "mate-volume-control-name");
  if (track != NULL) {
    label = g_strdup (track->label);
  } else {
    label = g_strdup ("");
  }

  pos = 0;
  res = g_new (gchar, strlen (dev) + 1 + strlen (label) + 1);

  for (i = 0; dev[i] != '\0'; i++) {
    if (g_ascii_isalnum (dev[i]))
      res[pos++] = dev[i];
  }
  res[pos] = '/';
  for (i = 0; label[i] != '\0'; i++) {
    if (g_ascii_isalnum (label[i]))
      res[pos++] = label[i];
  }
  res[pos] = '\0';

  g_free (label);
  return res;
}

/*
 * Change the element. Basically recreates this object internally.
 */

void
mate_volume_control_preferences_change (MateVolumeControlPreferences *prefs,
					 GstElement *element)
{
  GstMixer *mixer;
  GtkTreeIter iter;
  GtkListStore *store;
  const GList *item;
  gint pgnum;

  g_return_if_fail (GST_IS_MIXER (element));
  mixer = GST_MIXER (element);

  store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (prefs->treeview)));

  /* remove old */
  while (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter)) {
    gtk_list_store_remove (store, &iter);
  }

  /* take/put reference */
  gst_object_replace ((GstObject **) &prefs->mixer, GST_OBJECT (element));

  /* add all tracks */
  mate_volume_control_element_whitelist (mixer, NULL);
  for (item = gst_mixer_list_tracks (mixer);
       item != NULL; item = item->next) {
    GstMixerTrack *track = item->data;
    gboolean active = mate_volume_control_element_is_to_show (prefs->settings, mixer, track);

    pgnum = get_page_num (mixer, track);
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
			COL_ACTIVE, active,
			COL_LABEL, track->label,
			COL_TRACK, track,
			COL_TYPE, get_page_description (pgnum),
			COL_PAGE, pgnum,
			-1);
  }
}

/*
 * Callback if something is toggled.
 */

static void
set_gsettings_track_active(GSettings *settings, GstMixer *mixer, 
		       GstMixerTrack *track, gboolean active, gboolean is_whitelist)
{
  gchar *name;

  name = get_gsettings_name (mixer, track);

  if (active == TRUE)
  {
    if (is_whitelist == TRUE)
    {
        schemas_gsettings_remove_all_from_strv (settings, MATE_VOLUME_CONTROL_KEY_HIDDEN_ELEMENTS, name);
    }
    else
    {
      if (schemas_is_str_in_strv (settings, MATE_VOLUME_CONTROL_KEY_SHOWN_ELEMENTS, name) == FALSE)
      {
        schemas_gsettings_append_strv (settings, MATE_VOLUME_CONTROL_KEY_SHOWN_ELEMENTS, name);
      }
    }
  }
  else
  {
    if (is_whitelist == TRUE)
    {
      if (schemas_is_str_in_strv (settings, MATE_VOLUME_CONTROL_KEY_HIDDEN_ELEMENTS, name) == FALSE)
      {
        schemas_gsettings_append_strv (settings, MATE_VOLUME_CONTROL_KEY_HIDDEN_ELEMENTS, name);
      }
    }
    else
    {
      schemas_gsettings_remove_all_from_strv (settings, MATE_VOLUME_CONTROL_KEY_SHOWN_ELEMENTS, name);
    }
  }

  g_free (name);

}

static void	
cb_gsettings(GSettings *settings, gchar *key, MateVolumeControlPreferences *prefs)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gboolean active, valid;
  GstMixerTrack *track;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW(prefs->treeview));

  valid = gtk_tree_model_get_iter_first(model, &iter);

  while (valid == TRUE) {
    gtk_tree_model_get (model, &iter,
                        COL_TRACK, &track,
                        -1);
    active = mate_volume_control_element_is_to_show (settings, prefs->mixer, track);
    gtk_list_store_set( GTK_LIST_STORE(model), &iter, COL_ACTIVE, active, -1);
    valid = gtk_tree_model_iter_next(model, &iter);
  }
}

static void
cb_activated(GtkTreeView *view, GtkTreePath *path,
	     GtkTreeViewColumn *col, gpointer userdata)

{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean active;
  gboolean is_whitelist;
  GstMixerTrack *track;
  MateVolumeControlPreferences *prefs;

  prefs = MATE_VOLUME_CONTROL_PREFERENCES (userdata);
  model = gtk_tree_view_get_model(view);

  mate_volume_control_element_whitelist (prefs->mixer, NULL);
  if (gtk_tree_model_get_iter(model, &iter, path)) {
    gtk_tree_model_get(model, &iter, 
		       COL_ACTIVE, &active, 
		       COL_TRACK, &track,
		       -1);

    is_whitelist = mate_volume_control_element_whitelist (prefs->mixer, track);
  
    active = !active;

    gtk_list_store_set( GTK_LIST_STORE(model), &iter, COL_ACTIVE, active, -1);
    set_gsettings_track_active(prefs->settings, prefs->mixer, track, active, is_whitelist);
  }
}

static void
cb_toggle (GtkCellRendererToggle *cell,
	   gchar                 *path_str,
	   MateVolumeControlPreferences *prefs)
{
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (prefs->treeview));
  GtkTreePath *path = gtk_tree_path_new_from_string (path_str);
  GtkTreeIter iter;
  gboolean active;
  gboolean is_whitelist;
  GstMixerTrack *track;

  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter,
		      COL_ACTIVE, &active,
		      COL_TRACK, &track,
		      -1);

  mate_volume_control_element_whitelist (prefs->mixer, NULL);
  is_whitelist = mate_volume_control_element_whitelist (prefs->mixer, track);
  
  active = !active;

  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                        COL_ACTIVE, active,
                        -1);
  set_gsettings_track_active(prefs->settings, prefs->mixer, track, active, is_whitelist);

  gtk_tree_path_free (path);
}
