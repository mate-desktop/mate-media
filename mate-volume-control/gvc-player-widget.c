/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

#include "config.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#include "gvc-player-widget.h"

#define COVER_SIZE 300
#define PLAYER_WIDTH 316

struct _GvcPlayerWidgetPrivate
{
        GvcMprisPlayer *player;
        GtkImage       *cover;
        GtkLabel       *player_label;
        GtkComboBox    *player_combo;
        GtkLabel       *title_label;
        GtkLabel       *artist_label;
        GtkButton      *prev_button;
        GtkButton      *play_button;
        GtkButton      *stop_button;
        GtkButton      *next_button;
        GtkButton      *raise_button;
        GtkButton      *quit_button;
        GtkToggleButton *shuffle_button;
        GtkButton      *loop_button;
        GtkScale       *seek_scale;
        GtkLabel       *time_label;
        guint           seek_timer_id;
        gboolean        updating_seek;
        gboolean        updating_players;
};

enum
{
        PLAYER_SELECTED,
        N_SIGNALS
};

enum
{
        PLAYER_COLUMN_LABEL,
        PLAYER_COLUMN_PLAYER,
        N_PLAYER_COLUMNS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GvcPlayerWidget, gvc_player_widget, GTK_TYPE_BOX)

static gchar *
format_time (gint64 seconds)
{
        gint64 minutes;

        if (seconds < 0)
                seconds = 0;

        minutes = seconds / 60;
        seconds = seconds % 60;

        return g_strdup_printf ("%" G_GINT64_FORMAT ":%02" G_GINT64_FORMAT, minutes, seconds);
}

static void
set_cover_fallback (GvcPlayerWidget *widget)
{
        gtk_image_set_from_icon_name (widget->priv->cover,
                                      "media-optical",
                                      GTK_ICON_SIZE_DIALOG);
        gtk_image_set_pixel_size (widget->priv->cover, COVER_SIZE);
}

static void
on_cover_pixbuf_ready (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
        GvcPlayerWidget *widget = GVC_PLAYER_WIDGET (user_data);
        GdkPixbuf       *pixbuf;
        GError          *error = NULL;

        pixbuf = gdk_pixbuf_new_from_stream_finish (res, &error);
        if (pixbuf != NULL) {
                gtk_image_set_from_pixbuf (widget->priv->cover, pixbuf);
                g_object_unref (pixbuf);
        } else {
                set_cover_fallback (widget);
                if (error != NULL)
                        g_error_free (error);
        }

        g_object_unref (widget);
}

static void
load_cover_from_stream (GvcPlayerWidget *widget,
                        GInputStream    *stream)
{
        gdk_pixbuf_new_from_stream_at_scale_async (stream,
                                                   COVER_SIZE,
                                                   COVER_SIZE,
                                                   TRUE,
                                                   NULL,
                                                   on_cover_pixbuf_ready,
                                                   g_object_ref (widget));
}

static void
on_cover_file_read_ready (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
        GvcPlayerWidget *widget = GVC_PLAYER_WIDGET (user_data);
        GInputStream    *stream;
        GError          *error = NULL;

        stream = G_INPUT_STREAM (g_file_read_finish (G_FILE (source_object), res, &error));
        if (stream != NULL) {
                load_cover_from_stream (widget, stream);
                g_object_unref (stream);
        } else {
                set_cover_fallback (widget);
                if (error != NULL)
                        g_error_free (error);
        }

        g_object_unref (widget);
}

static void
update_cover (GvcPlayerWidget *widget)
{
        const gchar *art_url;

        art_url = gvc_mpris_player_get_art_url (widget->priv->player);
        if (art_url == NULL || art_url[0] == '\0') {
                set_cover_fallback (widget);
                return;
        }

        if (g_str_has_prefix (art_url, "data:image/")) {
                const gchar *comma;

                comma = strchr (art_url, ',');
                if (comma != NULL) {
                        guchar       *decoded;
                        gsize         len;
                        GInputStream *stream;

                        decoded = g_base64_decode (comma + 1, &len);
                        stream = g_memory_input_stream_new_from_data (decoded, len, g_free);
                        load_cover_from_stream (widget, stream);
                        g_object_unref (stream);
                        return;
                }
        } else {
                GFile *file;

                file = g_file_new_for_uri (art_url);
                g_file_read_async (file,
                                   G_PRIORITY_DEFAULT,
                                   NULL,
                                   on_cover_file_read_ready,
                                   g_object_ref (widget));
                g_object_unref (file);
                return;
        }

        set_cover_fallback (widget);
}

static void
on_position_ready (GvcMprisPlayer *player,
                   gint64          position,
                   GError         *error,
                   gpointer        user_data)
{
        GvcPlayerWidget *widget = GVC_PLAYER_WIDGET (user_data);
        gint64           length;
        gchar           *elapsed;
        gchar           *total;
        gchar           *label;

        if (error != NULL || widget->priv->player != player) {
                g_object_unref (widget);
                return;
        }

        length = gvc_mpris_player_get_length (player) / G_USEC_PER_SEC;
        position = position / G_USEC_PER_SEC;

        widget->priv->updating_seek = TRUE;
        gtk_range_set_value (GTK_RANGE (widget->priv->seek_scale), position);
        widget->priv->updating_seek = FALSE;

        elapsed = format_time (position);
        total = format_time (length);
        label = g_strdup_printf ("%s / %s", elapsed, total);
        gtk_label_set_text (widget->priv->time_label, label);
        g_free (elapsed);
        g_free (total);
        g_free (label);
        g_object_unref (widget);
}

static gboolean
seek_timer_cb (gpointer user_data)
{
        GvcPlayerWidget *widget = GVC_PLAYER_WIDGET (user_data);

        if (widget->priv->player == NULL ||
            !gvc_mpris_player_is_playing (widget->priv->player) ||
            !gvc_mpris_player_get_can_seek (widget->priv->player)) {
                widget->priv->seek_timer_id = 0;
                return G_SOURCE_REMOVE;
        }

        gvc_mpris_player_get_position_async (widget->priv->player,
                                             on_position_ready,
                                             g_object_ref (widget));
        return G_SOURCE_CONTINUE;
}

static void
ensure_seek_timer (GvcPlayerWidget *widget)
{
        if (widget->priv->seek_timer_id != 0)
                g_source_remove (widget->priv->seek_timer_id);

        widget->priv->seek_timer_id = 0;

        if (widget->priv->player != NULL &&
            gvc_mpris_player_is_playing (widget->priv->player) &&
            gvc_mpris_player_get_can_seek (widget->priv->player)) {
                widget->priv->seek_timer_id = g_timeout_add_seconds (1, seek_timer_cb, widget);
                seek_timer_cb (widget);
        }
}

static void
update_metadata (GvcPlayerWidget *widget)
{
        const gchar *title;
        const gchar *artist;
        gint64       length;
        gchar       *total;
        gchar       *label;

        if (widget->priv->player == NULL)
                return;

        title = gvc_mpris_player_get_title (widget->priv->player);
        artist = gvc_mpris_player_get_artist (widget->priv->player);

        gtk_label_set_text (widget->priv->title_label,
                            title[0] != '\0' ? title : _("Unknown Title"));
        gtk_label_set_text (widget->priv->artist_label,
                            artist[0] != '\0' ? artist : _("Unknown Artist"));

        length = gvc_mpris_player_get_length (widget->priv->player) / G_USEC_PER_SEC;
        gtk_range_set_range (GTK_RANGE (widget->priv->seek_scale), 0, MAX (length, 1));
        total = format_time (length);
        label = g_strdup_printf ("0:00 / %s", total);
        gtk_label_set_text (widget->priv->time_label, label);
        g_free (total);
        g_free (label);

        update_cover (widget);
        ensure_seek_timer (widget);
}

static void
update_status (GvcPlayerWidget *widget)
{
        const gchar *status;
        gchar       *label;

        if (widget->priv->player == NULL)
                return;

        status = gvc_mpris_player_get_playback_status (widget->priv->player);
        label = g_strdup_printf ("%s - %s",
                                 gvc_mpris_player_get_identity (widget->priv->player),
                                 status);
        gtk_label_set_text (widget->priv->player_label, label);
        g_free (label);

        gtk_button_set_image (widget->priv->play_button,
                              gtk_image_new_from_icon_name (gvc_mpris_player_is_playing (widget->priv->player) ?
                                                            "media-playback-pause-symbolic" :
                                                            "media-playback-start-symbolic",
                                                            GTK_ICON_SIZE_BUTTON));
        ensure_seek_timer (widget);
}

static void
update_capabilities (GvcPlayerWidget *widget)
{
        const gchar *loop_status;
        const gchar *loop_icon = "media-playlist-repeat-symbolic";

        if (widget->priv->player == NULL)
                return;

        gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->prev_button),
                                  gvc_mpris_player_get_can_go_previous (widget->priv->player));
        gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->next_button),
                                  gvc_mpris_player_get_can_go_next (widget->priv->player));
        gtk_widget_set_sensitive (GTK_WIDGET (widget->priv->seek_scale),
                                  gvc_mpris_player_get_can_seek (widget->priv->player));
        gtk_widget_set_visible (GTK_WIDGET (widget->priv->raise_button),
                                gvc_mpris_player_get_can_raise (widget->priv->player));
        gtk_widget_set_visible (GTK_WIDGET (widget->priv->quit_button),
                                gvc_mpris_player_get_can_quit (widget->priv->player));
        gtk_toggle_button_set_active (widget->priv->shuffle_button,
                                      gvc_mpris_player_get_shuffle (widget->priv->player));

        loop_status = gvc_mpris_player_get_loop_status (widget->priv->player);
        if (g_strcmp0 (loop_status, "Track") == 0)
                loop_icon = "media-playlist-repeat-song-symbolic";
        else if (g_strcmp0 (loop_status, "None") == 0)
                loop_icon = "media-playlist-consecutive-symbolic";

        gtk_button_set_image (widget->priv->loop_button,
                              gtk_image_new_from_icon_name (loop_icon, GTK_ICON_SIZE_BUTTON));
        ensure_seek_timer (widget);
}

static void
on_player_metadata_changed (GvcMprisPlayer  *player,
                            GvcPlayerWidget *widget)
{
        update_metadata (widget);
}

static void
on_player_status_changed (GvcMprisPlayer  *player,
                          GvcPlayerWidget *widget)
{
        update_status (widget);
}

static void
on_player_capabilities_changed (GvcMprisPlayer  *player,
                                GvcPlayerWidget *widget)
{
        update_capabilities (widget);
}

static void
on_player_combo_changed (GtkComboBox     *combo,
                         GvcPlayerWidget *widget)
{
        GtkTreeIter     iter;
        GvcMprisPlayer *player = NULL;

        if (widget->priv->updating_players)
                return;

        if (!gtk_combo_box_get_active_iter (combo, &iter))
                return;

        gtk_tree_model_get (gtk_combo_box_get_model (combo),
                            &iter,
                            PLAYER_COLUMN_PLAYER, &player,
                            -1);

        if (player != NULL) {
                g_signal_emit (widget, signals[PLAYER_SELECTED], 0, player);
                g_object_unref (player);
        }
}

static GtkWidget *
new_icon_button (const gchar *icon_name,
                 const gchar *tooltip)
{
        GtkWidget *button;

        button = gtk_button_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
        gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
        gtk_widget_set_tooltip_text (button, tooltip);

        return button;
}

static GtkWidget *
new_icon_toggle_button (const gchar *icon_name,
                        const gchar *tooltip)
{
        GtkWidget *button;
        GtkWidget *image;

        button = gtk_toggle_button_new ();
        image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image (GTK_BUTTON (button), image);
        gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
        gtk_widget_set_tooltip_text (button, tooltip);

        return button;
}

static void
make_overlay_label (GtkLabel *label)
{
        GdkRGBA white = { 1.0, 1.0, 1.0, 1.0 };

        gtk_widget_override_color (GTK_WIDGET (label),
                                   GTK_STATE_FLAG_NORMAL,
                                   &white);
}

static void on_prev_clicked (GtkButton *button, GvcPlayerWidget *widget) { gvc_mpris_player_previous (widget->priv->player); }
static void on_play_clicked (GtkButton *button, GvcPlayerWidget *widget) { gvc_mpris_player_play_pause (widget->priv->player); }
static void on_stop_clicked (GtkButton *button, GvcPlayerWidget *widget) { gvc_mpris_player_stop (widget->priv->player); }
static void on_next_clicked (GtkButton *button, GvcPlayerWidget *widget) { gvc_mpris_player_next (widget->priv->player); }
static void on_raise_clicked (GtkButton *button, GvcPlayerWidget *widget) { gvc_mpris_player_raise (widget->priv->player); }
static void on_quit_clicked (GtkButton *button, GvcPlayerWidget *widget) { gvc_mpris_player_quit (widget->priv->player); }

static void
on_shuffle_toggled (GtkToggleButton *button,
                    GvcPlayerWidget *widget)
{
        if (widget->priv->player == NULL)
                return;

        gvc_mpris_player_set_shuffle (widget->priv->player,
                                      gtk_toggle_button_get_active (button));
}

static void
on_loop_clicked (GtkButton       *button,
                 GvcPlayerWidget *widget)
{
        const gchar *status;
        const gchar *next = "Playlist";

        if (widget->priv->player == NULL)
                return;

        status = gvc_mpris_player_get_loop_status (widget->priv->player);
        if (g_strcmp0 (status, "Playlist") == 0)
                next = "Track";
        else if (g_strcmp0 (status, "Track") == 0)
                next = "None";

        gvc_mpris_player_set_loop_status (widget->priv->player, next);
}

static gboolean
on_seek_button_release (GtkWidget      *scale,
                        GdkEventButton *event,
                        GvcPlayerWidget *widget)
{
        gint64 value;

        if (widget->priv->player == NULL ||
            widget->priv->updating_seek ||
            !gvc_mpris_player_get_can_seek (widget->priv->player))
                return FALSE;

        value = gtk_range_get_value (GTK_RANGE (scale)) * G_USEC_PER_SEC;
        gvc_mpris_player_set_position (widget->priv->player,
                                       gvc_mpris_player_get_track_id (widget->priv->player),
                                       value);
        return FALSE;
}

static gboolean
seek_by_scroll_event (GvcPlayerWidget *widget,
                      GdkEventScroll  *event)
{
        gdouble current;
        gdouble lower;
        gdouble upper;
        gdouble step;
        gdouble delta;
        gint64  value;

        if (widget->priv->player == NULL ||
            widget->priv->updating_seek ||
            !gvc_mpris_player_get_can_seek (widget->priv->player))
                return TRUE;

        lower = gtk_adjustment_get_lower (gtk_range_get_adjustment (GTK_RANGE (widget->priv->seek_scale)));
        upper = gtk_adjustment_get_upper (gtk_range_get_adjustment (GTK_RANGE (widget->priv->seek_scale)));
        current = gtk_range_get_value (GTK_RANGE (widget->priv->seek_scale));
        step = 5.0;
        delta = 0.0;

        switch (event->direction) {
        case GDK_SCROLL_UP:
        case GDK_SCROLL_RIGHT:
                delta = step;
                break;
        case GDK_SCROLL_DOWN:
        case GDK_SCROLL_LEFT:
                delta = -step;
                break;
        case GDK_SCROLL_SMOOTH:
                delta = -event->delta_y * step;
                if (delta == 0.0)
                        delta = event->delta_x * step;
                break;
        default:
                break;
        }

        if (delta == 0.0)
                return TRUE;

        current = CLAMP (current + delta, lower, upper);
        gtk_range_set_value (GTK_RANGE (widget->priv->seek_scale), current);
        value = current * G_USEC_PER_SEC;
        gvc_mpris_player_set_position (widget->priv->player,
                                       gvc_mpris_player_get_track_id (widget->priv->player),
                                       value);
        return TRUE;
}

static gboolean
on_seek_scroll_event (GtkWidget       *scale,
                      GdkEventScroll  *event,
                      GvcPlayerWidget *widget)
{
        return seek_by_scroll_event (widget, event);
}

static void
gvc_player_widget_dispose (GObject *object)
{
        GvcPlayerWidget *widget = GVC_PLAYER_WIDGET (object);

        if (widget->priv->seek_timer_id != 0) {
                g_source_remove (widget->priv->seek_timer_id);
                widget->priv->seek_timer_id = 0;
        }

        gvc_player_widget_set_player (widget, NULL);

        G_OBJECT_CLASS (gvc_player_widget_parent_class)->dispose (object);
}

static void
gvc_player_widget_class_init (GvcPlayerWidgetClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = gvc_player_widget_dispose;

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
gvc_player_widget_init (GvcPlayerWidget *widget)
{
        GtkWidget *header;
        GtkListStore *combo_store;
        GtkCellRenderer *renderer;
        GtkWidget *overlay;
        GtkWidget *cover_frame;
        GtkWidget *overlay_event_box;
        GtkWidget *info_box;
        GtkWidget *controls;
        GtkWidget *seek_box;
        GdkRGBA    overlay_color = { 0.0, 0.0, 0.0, 0.62 };

        widget->priv = gvc_player_widget_get_instance_private (widget);

        gtk_widget_set_no_show_all (GTK_WIDGET (widget), TRUE);
        gtk_orientable_set_orientation (GTK_ORIENTABLE (widget), GTK_ORIENTATION_VERTICAL);
        gtk_box_set_spacing (GTK_BOX (widget), 6);
        gtk_container_set_border_width (GTK_CONTAINER (widget), 8);
        gtk_widget_set_size_request (GTK_WIDGET (widget), PLAYER_WIDTH, -1);

        header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
        widget->priv->player_label = GTK_LABEL (gtk_label_new (""));
        combo_store = gtk_list_store_new (N_PLAYER_COLUMNS,
                                          G_TYPE_STRING,
                                          GVC_TYPE_MPRIS_PLAYER);
        widget->priv->player_combo = GTK_COMBO_BOX (gtk_combo_box_new_with_model (GTK_TREE_MODEL (combo_store)));
        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget->priv->player_combo), renderer, TRUE);
        gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (widget->priv->player_combo),
                                       renderer,
                                       "text",
                                       PLAYER_COLUMN_LABEL);
        gtk_widget_set_no_show_all (GTK_WIDGET (widget->priv->player_combo), TRUE);
        gtk_widget_set_tooltip_text (GTK_WIDGET (widget->priv->player_combo), _("Choose Player"));
        g_object_unref (combo_store);
        widget->priv->raise_button = GTK_BUTTON (new_icon_button ("go-up-symbolic", _("Open Player")));
        widget->priv->quit_button = GTK_BUTTON (new_icon_button ("window-close-symbolic", _("Quit Player")));
        gtk_label_set_ellipsize (widget->priv->player_label, PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars (widget->priv->player_label, 34);
        gtk_label_set_xalign (widget->priv->player_label, 0.0);
        gtk_box_pack_start (GTK_BOX (header), GTK_WIDGET (widget->priv->player_label), TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (header), GTK_WIDGET (widget->priv->player_combo), TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (header), GTK_WIDGET (widget->priv->raise_button), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (header), GTK_WIDGET (widget->priv->quit_button), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (widget), header, FALSE, FALSE, 0);

        overlay = gtk_overlay_new ();
        gtk_widget_set_halign (overlay, GTK_ALIGN_CENTER);
        gtk_widget_set_size_request (overlay, COVER_SIZE, COVER_SIZE);

        cover_frame = gtk_aspect_frame_new (NULL, 0.5, 0.5, 1.0, FALSE);
        gtk_frame_set_shadow_type (GTK_FRAME (cover_frame), GTK_SHADOW_NONE);
        gtk_widget_set_size_request (cover_frame, COVER_SIZE, COVER_SIZE);
        widget->priv->cover = GTK_IMAGE (gtk_image_new ());
        gtk_widget_set_size_request (GTK_WIDGET (widget->priv->cover), COVER_SIZE, COVER_SIZE);
        set_cover_fallback (widget);
        gtk_container_add (GTK_CONTAINER (cover_frame), GTK_WIDGET (widget->priv->cover));
        gtk_container_add (GTK_CONTAINER (overlay), cover_frame);

        overlay_event_box = gtk_event_box_new ();
        gtk_widget_set_halign (overlay_event_box, GTK_ALIGN_FILL);
        gtk_widget_set_valign (overlay_event_box, GTK_ALIGN_END);
        gtk_widget_override_background_color (overlay_event_box,
                                             GTK_STATE_FLAG_NORMAL,
                                             &overlay_color);

        info_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
        gtk_container_set_border_width (GTK_CONTAINER (info_box), 8);
        gtk_container_add (GTK_CONTAINER (overlay_event_box), info_box);

        widget->priv->title_label = GTK_LABEL (gtk_label_new (_("Unknown Title")));
        widget->priv->artist_label = GTK_LABEL (gtk_label_new (_("Unknown Artist")));
        gtk_label_set_ellipsize (widget->priv->title_label, PANGO_ELLIPSIZE_NONE);
        gtk_label_set_ellipsize (widget->priv->artist_label, PANGO_ELLIPSIZE_NONE);
        gtk_label_set_line_wrap (widget->priv->title_label, TRUE);
        gtk_label_set_line_wrap (widget->priv->artist_label, TRUE);
        gtk_label_set_line_wrap_mode (widget->priv->title_label, PANGO_WRAP_WORD_CHAR);
        gtk_label_set_line_wrap_mode (widget->priv->artist_label, PANGO_WRAP_WORD_CHAR);
        gtk_label_set_max_width_chars (widget->priv->title_label, 30);
        gtk_label_set_max_width_chars (widget->priv->artist_label, 30);
        gtk_label_set_xalign (widget->priv->title_label, 0.0);
        gtk_label_set_xalign (widget->priv->artist_label, 0.0);
        make_overlay_label (widget->priv->title_label);
        make_overlay_label (widget->priv->artist_label);
        gtk_box_pack_start (GTK_BOX (info_box), GTK_WIDGET (widget->priv->artist_label), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (info_box), GTK_WIDGET (widget->priv->title_label), FALSE, FALSE, 0);

        controls = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
        gtk_widget_set_halign (controls, GTK_ALIGN_CENTER);
        widget->priv->prev_button = GTK_BUTTON (new_icon_button ("media-skip-backward-symbolic", _("Previous")));
        widget->priv->play_button = GTK_BUTTON (new_icon_button ("media-playback-start-symbolic", _("Play/Pause")));
        widget->priv->stop_button = GTK_BUTTON (new_icon_button ("media-playback-stop-symbolic", _("Stop")));
        widget->priv->next_button = GTK_BUTTON (new_icon_button ("media-skip-forward-symbolic", _("Next")));
        widget->priv->loop_button = GTK_BUTTON (new_icon_button ("media-playlist-repeat-symbolic", _("Repeat")));
        widget->priv->shuffle_button = GTK_TOGGLE_BUTTON (new_icon_toggle_button ("media-playlist-shuffle-symbolic", _("Shuffle")));
        gtk_box_pack_start (GTK_BOX (controls), GTK_WIDGET (widget->priv->prev_button), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (controls), GTK_WIDGET (widget->priv->play_button), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (controls), GTK_WIDGET (widget->priv->stop_button), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (controls), GTK_WIDGET (widget->priv->next_button), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (controls), GTK_WIDGET (widget->priv->loop_button), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (controls), GTK_WIDGET (widget->priv->shuffle_button), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (info_box), controls, FALSE, FALSE, 0);

        gtk_overlay_add_overlay (GTK_OVERLAY (overlay), overlay_event_box);
        gtk_overlay_set_overlay_pass_through (GTK_OVERLAY (overlay), overlay_event_box, FALSE);
        gtk_box_pack_start (GTK_BOX (widget), overlay, FALSE, FALSE, 0);

        seek_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        widget->priv->seek_scale = GTK_SCALE (gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 1, 1));
        gtk_scale_set_draw_value (widget->priv->seek_scale, FALSE);
        gtk_widget_set_hexpand (GTK_WIDGET (widget->priv->seek_scale), TRUE);
        gtk_widget_set_size_request (GTK_WIDGET (widget->priv->seek_scale), 210, -1);
        widget->priv->time_label = GTK_LABEL (gtk_label_new ("0:00 / 0:00"));
        gtk_label_set_width_chars (widget->priv->time_label, 11);
        gtk_label_set_xalign (widget->priv->time_label, 1.0);
        gtk_box_pack_start (GTK_BOX (seek_box), GTK_WIDGET (widget->priv->seek_scale), TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (seek_box), GTK_WIDGET (widget->priv->time_label), FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (widget), seek_box, FALSE, FALSE, 0);

        g_signal_connect (widget->priv->prev_button, "clicked", G_CALLBACK (on_prev_clicked), widget);
        g_signal_connect (widget->priv->play_button, "clicked", G_CALLBACK (on_play_clicked), widget);
        g_signal_connect (widget->priv->stop_button, "clicked", G_CALLBACK (on_stop_clicked), widget);
        g_signal_connect (widget->priv->next_button, "clicked", G_CALLBACK (on_next_clicked), widget);
        g_signal_connect (widget->priv->raise_button, "clicked", G_CALLBACK (on_raise_clicked), widget);
        g_signal_connect (widget->priv->quit_button, "clicked", G_CALLBACK (on_quit_clicked), widget);
        g_signal_connect (widget->priv->loop_button, "clicked", G_CALLBACK (on_loop_clicked), widget);
        g_signal_connect (widget->priv->shuffle_button, "toggled", G_CALLBACK (on_shuffle_toggled), widget);
        g_signal_connect (widget->priv->seek_scale, "button-release-event", G_CALLBACK (on_seek_button_release), widget);
        g_signal_connect (widget->priv->seek_scale, "scroll-event", G_CALLBACK (on_seek_scroll_event), widget);
        g_signal_connect (widget->priv->player_combo, "changed", G_CALLBACK (on_player_combo_changed), widget);
}

GtkWidget *
gvc_player_widget_new (void)
{
        return g_object_new (GVC_TYPE_PLAYER_WIDGET, NULL);
}

void
gvc_player_widget_set_player (GvcPlayerWidget *widget,
                              GvcMprisPlayer  *player)
{
        g_return_if_fail (GVC_IS_PLAYER_WIDGET (widget));

        if (widget->priv->player == player)
                return;

        if (widget->priv->player != NULL) {
                g_signal_handlers_disconnect_by_data (widget->priv->player, widget);
                g_clear_object (&widget->priv->player);
        }

        if (player != NULL)
                widget->priv->player = g_object_ref (player);

        if (widget->priv->player != NULL) {
                g_signal_connect (widget->priv->player, "metadata-changed", G_CALLBACK (on_player_metadata_changed), widget);
                g_signal_connect (widget->priv->player, "status-changed", G_CALLBACK (on_player_status_changed), widget);
                g_signal_connect (widget->priv->player, "capabilities-changed", G_CALLBACK (on_player_capabilities_changed), widget);
                update_metadata (widget);
                update_status (widget);
                update_capabilities (widget);
                gtk_widget_set_no_show_all (GTK_WIDGET (widget), FALSE);
                gtk_widget_show_all (GTK_WIDGET (widget));
                gtk_widget_set_no_show_all (GTK_WIDGET (widget), TRUE);
                gtk_widget_show (GTK_WIDGET (widget));
        } else {
                gtk_widget_hide (GTK_WIDGET (widget));
        }
}

void
gvc_player_widget_set_players (GvcPlayerWidget *widget,
                               GList           *players,
                               GvcMprisPlayer  *active_player)
{
        GtkListStore *store;
        GtkTreeIter   iter;
        GList        *l;
        gint          count = 0;
        gint          active_index = -1;

        g_return_if_fail (GVC_IS_PLAYER_WIDGET (widget));

        store = GTK_LIST_STORE (gtk_combo_box_get_model (widget->priv->player_combo));

        widget->priv->updating_players = TRUE;
        gtk_list_store_clear (store);

        for (l = players; l != NULL; l = l->next) {
                GvcMprisPlayer *player = GVC_MPRIS_PLAYER (l->data);
                const gchar    *label;

                if (!gvc_mpris_player_is_ready (player))
                        continue;

                label = gvc_mpris_player_get_identity (player);
                if (label == NULL || label[0] == '\0')
                        label = gvc_mpris_player_get_bus_name (player);

                gtk_list_store_append (store, &iter);
                gtk_list_store_set (store,
                                    &iter,
                                    PLAYER_COLUMN_LABEL, label,
                                    PLAYER_COLUMN_PLAYER, player,
                                    -1);

                if (player == active_player)
                        active_index = count;

                count++;
        }

        gtk_combo_box_set_active (widget->priv->player_combo, active_index);
        widget->priv->updating_players = FALSE;

        if (count > 1) {
                gtk_widget_hide (GTK_WIDGET (widget->priv->player_label));
                gtk_widget_show (GTK_WIDGET (widget->priv->player_combo));
        } else {
                gtk_widget_hide (GTK_WIDGET (widget->priv->player_combo));
                gtk_widget_show (GTK_WIDGET (widget->priv->player_label));
        }
}

gboolean
gvc_player_widget_scroll_is_seek (GvcPlayerWidget *widget,
                                  GdkEventScroll  *event)
{
        GtkAllocation allocation;
        GdkWindow    *window;
        gint          x;
        gint          y;

        g_return_val_if_fail (GVC_IS_PLAYER_WIDGET (widget), FALSE);

        if (!gtk_widget_get_visible (GTK_WIDGET (widget->priv->seek_scale)) ||
            gtk_widget_get_window (GTK_WIDGET (widget->priv->seek_scale)) == NULL)
                return FALSE;

        window = gtk_widget_get_window (GTK_WIDGET (widget->priv->seek_scale));
        gdk_window_get_origin (window, &x, &y);
        gtk_widget_get_allocation (GTK_WIDGET (widget->priv->seek_scale), &allocation);

        return event->x_root >= x + allocation.x &&
               event->x_root < x + allocation.x + allocation.width &&
               event->y_root >= y + allocation.y &&
               event->y_root < y + allocation.y + allocation.height;
}

gboolean
gvc_player_widget_handle_seek_scroll (GvcPlayerWidget *widget,
                                      GdkEventScroll  *event)
{
        g_return_val_if_fail (GVC_IS_PLAYER_WIDGET (widget), TRUE);

        return seek_by_scroll_event (widget, event);
}
