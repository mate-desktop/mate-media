/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

#include "config.h"

#include <string.h>
#include <gio/gdesktopappinfo.h>
#include <gdk/gdk.h>

#include "gvc-mpris-player.h"

#define MPRIS_OBJECT_PATH "/org/mpris/MediaPlayer2"
#define MPRIS_SERVER_IFACE "org.mpris.MediaPlayer2"
#define MPRIS_PLAYER_IFACE "org.mpris.MediaPlayer2.Player"
#define DBUS_PROPERTIES_IFACE "org.freedesktop.DBus.Properties"

struct _GvcMprisPlayerPrivate
{
        gchar      *bus_name;
        gchar      *owner;
        GDBusProxy *server_proxy;
        GDBusProxy *player_proxy;
        guint       pending_proxies;
        gboolean    ready;

        gchar      *identity;
        gchar      *desktop_entry;
        gchar      *playback_status;
        gchar      *title;
        gchar      *artist;
        gchar      *album;
        gchar      *art_url;
        gchar      *track_id;
        gchar      *loop_status;
        gint64      length;

        gboolean    can_raise;
        gboolean    can_quit;
        gboolean    can_control;
        gboolean    can_play;
        gboolean    can_pause;
        gboolean    can_go_next;
        gboolean    can_go_previous;
        gboolean    can_seek;
        gboolean    shuffle;
};

enum
{
        READY,
        METADATA_CHANGED,
        STATUS_CHANGED,
        CAPABILITIES_CHANGED,
        CLOSED,
        N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GvcMprisPlayer, gvc_mpris_player, G_TYPE_OBJECT)

static gchar *
variant_dup_string (GVariant *variant)
{
        if (variant == NULL)
                return g_strdup ("");

        if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING) ||
            g_variant_is_of_type (variant, G_VARIANT_TYPE_OBJECT_PATH))
                return g_variant_dup_string (variant, NULL);

        if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING_ARRAY)) {
                const gchar **strv;
                gchar        *joined;

                strv = g_variant_get_strv (variant, NULL);
                joined = g_strjoinv (", ", (gchar **) strv);
                g_free (strv);
                return joined;
        }

        return g_strdup ("");
}

static gboolean
get_cached_boolean (GDBusProxy  *proxy,
                    const gchar *name)
{
        GVariant *value;
        gboolean  result = FALSE;

        value = g_dbus_proxy_get_cached_property (proxy, name);
        if (value != NULL) {
                result = g_variant_get_boolean (value);
                g_variant_unref (value);
        }

        return result;
}

static gchar *
get_cached_string (GDBusProxy  *proxy,
                   const gchar *name)
{
        GVariant *value;
        gchar    *result = NULL;

        value = g_dbus_proxy_get_cached_property (proxy, name);
        if (value != NULL) {
                result = variant_dup_string (value);
                g_variant_unref (value);
        }

        return result;
}

static void
gvc_mpris_player_update_metadata (GvcMprisPlayer *player,
                                  GVariant       *metadata)
{
        GVariantIter iter;
        const gchar *key;
        GVariant    *value;

        if (metadata == NULL)
                return;

        g_clear_pointer (&player->priv->title, g_free);
        g_clear_pointer (&player->priv->artist, g_free);
        g_clear_pointer (&player->priv->album, g_free);
        g_clear_pointer (&player->priv->art_url, g_free);
        g_clear_pointer (&player->priv->track_id, g_free);
        player->priv->length = 0;

        player->priv->title = g_strdup ("");
        player->priv->artist = g_strdup ("");
        player->priv->album = g_strdup ("");
        player->priv->art_url = g_strdup ("");
        player->priv->track_id = g_strdup ("");

        g_variant_iter_init (&iter, metadata);
        while (g_variant_iter_next (&iter, "{&sv}", &key, &value)) {
                if (g_str_equal (key, "xesam:title")) {
                        g_free (player->priv->title);
                        player->priv->title = variant_dup_string (value);
                } else if (g_str_equal (key, "xesam:artist")) {
                        g_free (player->priv->artist);
                        player->priv->artist = variant_dup_string (value);
                } else if (g_str_equal (key, "xesam:album")) {
                        g_free (player->priv->album);
                        player->priv->album = variant_dup_string (value);
                } else if (g_str_equal (key, "mpris:artUrl")) {
                        g_free (player->priv->art_url);
                        player->priv->art_url = variant_dup_string (value);
                } else if (g_str_equal (key, "mpris:trackid")) {
                        g_free (player->priv->track_id);
                        player->priv->track_id = variant_dup_string (value);
                } else if (g_str_equal (key, "mpris:length")) {
                        player->priv->length = g_variant_get_int64 (value);
                }

                g_variant_unref (value);
        }
}

static void
gvc_mpris_player_update_capabilities (GvcMprisPlayer *player)
{
        if (player->priv->server_proxy != NULL) {
                player->priv->can_raise = get_cached_boolean (player->priv->server_proxy, "CanRaise");
                player->priv->can_quit = get_cached_boolean (player->priv->server_proxy, "CanQuit");
        }

        if (player->priv->player_proxy != NULL) {
                player->priv->can_control = get_cached_boolean (player->priv->player_proxy, "CanControl");
                player->priv->can_play = get_cached_boolean (player->priv->player_proxy, "CanPlay");
                player->priv->can_pause = get_cached_boolean (player->priv->player_proxy, "CanPause");
                player->priv->can_go_next = get_cached_boolean (player->priv->player_proxy, "CanGoNext");
                player->priv->can_go_previous = get_cached_boolean (player->priv->player_proxy, "CanGoPrevious");
                player->priv->can_seek = get_cached_boolean (player->priv->player_proxy, "CanSeek");
        }
}

static void
gvc_mpris_player_read_initial_state (GvcMprisPlayer *player)
{
        GVariant *metadata;
        gchar    *value;

        value = get_cached_string (player->priv->server_proxy, "Identity");
        if (value != NULL && value[0] != '\0') {
                g_free (player->priv->identity);
                player->priv->identity = value;
        } else {
                gchar *name = g_strdup (player->priv->bus_name + strlen ("org.mpris.MediaPlayer2."));
                g_free (value);
                if (name[0] != '\0')
                        name[0] = g_ascii_toupper (name[0]);
                g_free (player->priv->identity);
                player->priv->identity = name;
        }

        g_free (player->priv->desktop_entry);
        player->priv->desktop_entry = get_cached_string (player->priv->server_proxy, "DesktopEntry");

        g_free (player->priv->playback_status);
        player->priv->playback_status = get_cached_string (player->priv->player_proxy, "PlaybackStatus");
        if (player->priv->playback_status == NULL || player->priv->playback_status[0] == '\0') {
                g_free (player->priv->playback_status);
                player->priv->playback_status = g_strdup ("Unknown");
        }

        g_free (player->priv->loop_status);
        player->priv->loop_status = get_cached_string (player->priv->player_proxy, "LoopStatus");
        if (player->priv->loop_status == NULL)
                player->priv->loop_status = g_strdup ("None");

        player->priv->shuffle = get_cached_boolean (player->priv->player_proxy, "Shuffle");

        metadata = g_dbus_proxy_get_cached_property (player->priv->player_proxy, "Metadata");
        gvc_mpris_player_update_metadata (player, metadata);
        if (metadata != NULL)
                g_variant_unref (metadata);

        gvc_mpris_player_update_capabilities (player);
}

static void
on_proxy_properties_changed (GDBusProxy     *proxy,
                             GVariant       *changed_properties,
                             const gchar   **invalidated_properties,
                             GvcMprisPlayer *player)
{
        gboolean metadata_changed = FALSE;
        gboolean status_changed = FALSE;
        gboolean capabilities_changed = FALSE;
        gboolean player_iface;
        GVariantIter iter;
        const gchar *key;
        GVariant    *value;

        player_iface = proxy == player->priv->player_proxy;

        g_variant_iter_init (&iter, changed_properties);
        while (g_variant_iter_next (&iter, "{&sv}", &key, &value)) {
                if (player_iface && g_str_equal (key, "Metadata")) {
                        gvc_mpris_player_update_metadata (player, value);
                        metadata_changed = TRUE;
                } else if (player_iface && g_str_equal (key, "PlaybackStatus")) {
                        g_free (player->priv->playback_status);
                        player->priv->playback_status = variant_dup_string (value);
                        status_changed = TRUE;
                } else if (player_iface && g_str_equal (key, "Shuffle")) {
                        player->priv->shuffle = g_variant_get_boolean (value);
                        capabilities_changed = TRUE;
                } else if (player_iface && g_str_equal (key, "LoopStatus")) {
                        g_free (player->priv->loop_status);
                        player->priv->loop_status = variant_dup_string (value);
                        capabilities_changed = TRUE;
                } else if (!player_iface && g_str_equal (key, "Identity")) {
                        g_free (player->priv->identity);
                        player->priv->identity = variant_dup_string (value);
                        metadata_changed = TRUE;
                } else if (!player_iface && g_str_equal (key, "DesktopEntry")) {
                        g_free (player->priv->desktop_entry);
                        player->priv->desktop_entry = variant_dup_string (value);
                } else if (g_str_has_prefix (key, "Can")) {
                        capabilities_changed = TRUE;
                }

                g_variant_unref (value);
        }

        if (capabilities_changed) {
                gvc_mpris_player_update_capabilities (player);
                g_signal_emit (player, signals[CAPABILITIES_CHANGED], 0);
        }
        if (metadata_changed)
                g_signal_emit (player, signals[METADATA_CHANGED], 0);
        if (status_changed)
                g_signal_emit (player, signals[STATUS_CHANGED], 0);
}

static void
on_proxy_ready (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
        GvcMprisPlayer *player = GVC_MPRIS_PLAYER (user_data);
        GDBusProxy     *proxy;
        GError         *error = NULL;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (error != NULL) {
                g_debug ("Unable to create MPRIS proxy for %s: %s",
                         player->priv->bus_name,
                         error->message);
                g_error_free (error);
        } else if (g_str_equal (g_dbus_proxy_get_interface_name (proxy), MPRIS_SERVER_IFACE)) {
                player->priv->server_proxy = proxy;
                g_signal_connect (proxy,
                                  "g-properties-changed",
                                  G_CALLBACK (on_proxy_properties_changed),
                                  player);
        } else {
                player->priv->player_proxy = proxy;
                g_signal_connect (proxy,
                                  "g-properties-changed",
                                  G_CALLBACK (on_proxy_properties_changed),
                                  player);
        }

        player->priv->pending_proxies--;

        if (player->priv->pending_proxies == 0 &&
            player->priv->server_proxy != NULL &&
            player->priv->player_proxy != NULL) {
                gvc_mpris_player_read_initial_state (player);
                player->priv->ready = TRUE;
                g_signal_emit (player, signals[READY], 0);
        }

        g_object_unref (player);
}

static void
gvc_mpris_player_call (GvcMprisPlayer *player,
                       GDBusProxy     *proxy,
                       const gchar    *method,
                       GVariant       *parameters)
{
        if (proxy == NULL)
                return;

        g_dbus_proxy_call (proxy,
                           method,
                           parameters,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           NULL,
                           NULL);
}

static gboolean
gvc_mpris_player_launch_desktop_entry (GvcMprisPlayer *player)
{
        GDesktopAppInfo *app_info;
        GdkDisplay      *display;
        GdkAppLaunchContext *context = NULL;
        GError          *error = NULL;
        gboolean         launched = FALSE;

        if (player->priv->desktop_entry == NULL ||
            player->priv->desktop_entry[0] == '\0')
                return FALSE;

        app_info = g_desktop_app_info_new (player->priv->desktop_entry);
        if (app_info == NULL && !g_str_has_suffix (player->priv->desktop_entry, ".desktop")) {
                gchar *desktop_id;

                desktop_id = g_strconcat (player->priv->desktop_entry, ".desktop", NULL);
                app_info = g_desktop_app_info_new (desktop_id);
                g_free (desktop_id);
        }

        if (app_info == NULL)
                return FALSE;

        display = gdk_display_get_default ();
        if (display != NULL) {
                context = gdk_display_get_app_launch_context (display);
                gdk_app_launch_context_set_timestamp (context, GDK_CURRENT_TIME);
        }

        launched = g_app_info_launch (G_APP_INFO (app_info),
                                      NULL,
                                      context != NULL ? G_APP_LAUNCH_CONTEXT (context) : NULL,
                                      &error);
        if (error != NULL)
                g_error_free (error);

        if (context != NULL)
                g_object_unref (context);
        g_object_unref (app_info);

        return launched;
}

static void
gvc_mpris_player_set_property_variant (GvcMprisPlayer *player,
                                       const gchar    *property,
                                       GVariant       *value)
{
        GDBusConnection *connection;

        if (player->priv->player_proxy == NULL)
                return;

        connection = g_dbus_proxy_get_connection (player->priv->player_proxy);
        g_dbus_connection_call (connection,
                                player->priv->bus_name,
                                MPRIS_OBJECT_PATH,
                                DBUS_PROPERTIES_IFACE,
                                "Set",
                                g_variant_new ("(ssv)", MPRIS_PLAYER_IFACE, property, value),
                                NULL,
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL,
                                NULL,
                                NULL);
}

static void
gvc_mpris_player_dispose (GObject *object)
{
        GvcMprisPlayer *player = GVC_MPRIS_PLAYER (object);

        g_clear_object (&player->priv->server_proxy);
        g_clear_object (&player->priv->player_proxy);

        G_OBJECT_CLASS (gvc_mpris_player_parent_class)->dispose (object);
}

static void
gvc_mpris_player_finalize (GObject *object)
{
        GvcMprisPlayer *player = GVC_MPRIS_PLAYER (object);

        g_clear_pointer (&player->priv->bus_name, g_free);
        g_clear_pointer (&player->priv->owner, g_free);
        g_clear_pointer (&player->priv->identity, g_free);
        g_clear_pointer (&player->priv->desktop_entry, g_free);
        g_clear_pointer (&player->priv->playback_status, g_free);
        g_clear_pointer (&player->priv->title, g_free);
        g_clear_pointer (&player->priv->artist, g_free);
        g_clear_pointer (&player->priv->album, g_free);
        g_clear_pointer (&player->priv->art_url, g_free);
        g_clear_pointer (&player->priv->track_id, g_free);
        g_clear_pointer (&player->priv->loop_status, g_free);

        G_OBJECT_CLASS (gvc_mpris_player_parent_class)->finalize (object);
}

static void
gvc_mpris_player_class_init (GvcMprisPlayerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = gvc_mpris_player_dispose;
        object_class->finalize = gvc_mpris_player_finalize;

        signals[READY] =
                g_signal_new ("ready",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL, NULL,
                              G_TYPE_NONE,
                              0);
        signals[METADATA_CHANGED] =
                g_signal_new ("metadata-changed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL, NULL,
                              G_TYPE_NONE,
                              0);
        signals[STATUS_CHANGED] =
                g_signal_new ("status-changed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL, NULL,
                              G_TYPE_NONE,
                              0);
        signals[CAPABILITIES_CHANGED] =
                g_signal_new ("capabilities-changed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL, NULL,
                              G_TYPE_NONE,
                              0);
        signals[CLOSED] =
                g_signal_new ("closed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL, NULL,
                              G_TYPE_NONE,
                              0);
}

static void
gvc_mpris_player_init (GvcMprisPlayer *player)
{
        player->priv = gvc_mpris_player_get_instance_private (player);
        player->priv->pending_proxies = 2;
        player->priv->playback_status = g_strdup ("Unknown");
        player->priv->title = g_strdup ("");
        player->priv->artist = g_strdup ("");
        player->priv->album = g_strdup ("");
        player->priv->art_url = g_strdup ("");
        player->priv->track_id = g_strdup ("");
        player->priv->loop_status = g_strdup ("None");
}

GvcMprisPlayer *
gvc_mpris_player_new (const gchar *bus_name,
                      const gchar *owner)
{
        GvcMprisPlayer *player;

        g_return_val_if_fail (bus_name != NULL, NULL);
        g_return_val_if_fail (owner != NULL, NULL);

        player = g_object_new (GVC_TYPE_MPRIS_PLAYER, NULL);
        player->priv->bus_name = g_strdup (bus_name);
        player->priv->owner = g_strdup (owner);

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  bus_name,
                                  MPRIS_OBJECT_PATH,
                                  MPRIS_SERVER_IFACE,
                                  NULL,
                                  on_proxy_ready,
                                  g_object_ref (player));
        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  bus_name,
                                  MPRIS_OBJECT_PATH,
                                  MPRIS_PLAYER_IFACE,
                                  NULL,
                                  on_proxy_ready,
                                  g_object_ref (player));

        return player;
}

const gchar *
gvc_mpris_player_get_bus_name (GvcMprisPlayer *player)
{
        g_return_val_if_fail (GVC_IS_MPRIS_PLAYER (player), NULL);
        return player->priv->bus_name;
}

const gchar *
gvc_mpris_player_get_owner (GvcMprisPlayer *player)
{
        g_return_val_if_fail (GVC_IS_MPRIS_PLAYER (player), NULL);
        return player->priv->owner;
}

gboolean
gvc_mpris_player_is_ready (GvcMprisPlayer *player)
{
        g_return_val_if_fail (GVC_IS_MPRIS_PLAYER (player), FALSE);
        return player->priv->ready;
}

gboolean
gvc_mpris_player_is_playing (GvcMprisPlayer *player)
{
        g_return_val_if_fail (GVC_IS_MPRIS_PLAYER (player), FALSE);
        return g_strcmp0 (player->priv->playback_status, "Playing") == 0;
}

const gchar * gvc_mpris_player_get_identity (GvcMprisPlayer *player) { return player->priv->identity ? player->priv->identity : ""; }
const gchar * gvc_mpris_player_get_desktop_entry (GvcMprisPlayer *player) { return player->priv->desktop_entry ? player->priv->desktop_entry : ""; }
const gchar * gvc_mpris_player_get_playback_status (GvcMprisPlayer *player) { return player->priv->playback_status ? player->priv->playback_status : "Unknown"; }
const gchar * gvc_mpris_player_get_title (GvcMprisPlayer *player) { return player->priv->title ? player->priv->title : ""; }
const gchar * gvc_mpris_player_get_artist (GvcMprisPlayer *player) { return player->priv->artist ? player->priv->artist : ""; }
const gchar * gvc_mpris_player_get_album (GvcMprisPlayer *player) { return player->priv->album ? player->priv->album : ""; }
const gchar * gvc_mpris_player_get_art_url (GvcMprisPlayer *player) { return player->priv->art_url ? player->priv->art_url : ""; }
const gchar * gvc_mpris_player_get_track_id (GvcMprisPlayer *player) { return player->priv->track_id ? player->priv->track_id : ""; }
gint64 gvc_mpris_player_get_length (GvcMprisPlayer *player) { return player->priv->length; }
gboolean gvc_mpris_player_get_can_go_next (GvcMprisPlayer *player) { return player->priv->can_go_next; }
gboolean gvc_mpris_player_get_can_go_previous (GvcMprisPlayer *player) { return player->priv->can_go_previous; }
gboolean gvc_mpris_player_get_can_play (GvcMprisPlayer *player) { return player->priv->can_play; }
gboolean gvc_mpris_player_get_can_pause (GvcMprisPlayer *player) { return player->priv->can_pause; }
gboolean gvc_mpris_player_get_can_seek (GvcMprisPlayer *player) { return player->priv->can_seek; }
gboolean gvc_mpris_player_get_can_raise (GvcMprisPlayer *player) { return player->priv->can_raise || (player->priv->desktop_entry != NULL && player->priv->desktop_entry[0] != '\0') || (player->priv->identity != NULL && g_ascii_strcasecmp (player->priv->identity, "spotify") == 0); }
gboolean gvc_mpris_player_get_can_quit (GvcMprisPlayer *player) { return player->priv->can_quit; }
gboolean gvc_mpris_player_get_shuffle (GvcMprisPlayer *player) { return player->priv->shuffle; }
const gchar * gvc_mpris_player_get_loop_status (GvcMprisPlayer *player) { return player->priv->loop_status ? player->priv->loop_status : "None"; }

void gvc_mpris_player_play_pause (GvcMprisPlayer *player) { gvc_mpris_player_call (player, player->priv->player_proxy, "PlayPause", NULL); }
void gvc_mpris_player_stop (GvcMprisPlayer *player) { gvc_mpris_player_call (player, player->priv->player_proxy, "Stop", NULL); }
void gvc_mpris_player_next (GvcMprisPlayer *player) { if (player->priv->can_go_next) gvc_mpris_player_call (player, player->priv->player_proxy, "Next", NULL); }
void gvc_mpris_player_previous (GvcMprisPlayer *player) { if (player->priv->can_go_previous) gvc_mpris_player_call (player, player->priv->player_proxy, "Previous", NULL); }
void
gvc_mpris_player_raise (GvcMprisPlayer *player)
{
        if (player->priv->identity != NULL &&
            g_ascii_strcasecmp (player->priv->identity, "spotify") == 0) {
                gchar *argv[] = { "spotify", NULL };
                g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
                return;
        }

        if (player->priv->can_raise)
                gvc_mpris_player_call (player, player->priv->server_proxy, "Raise", NULL);

        gvc_mpris_player_launch_desktop_entry (player);
}
void gvc_mpris_player_quit (GvcMprisPlayer *player) { if (player->priv->can_quit) gvc_mpris_player_call (player, player->priv->server_proxy, "Quit", NULL); }
void gvc_mpris_player_set_shuffle (GvcMprisPlayer *player, gboolean shuffle) { gvc_mpris_player_set_property_variant (player, "Shuffle", g_variant_new_boolean (shuffle)); }
void gvc_mpris_player_set_loop_status (GvcMprisPlayer *player, const gchar *loop_status) { gvc_mpris_player_set_property_variant (player, "LoopStatus", g_variant_new_string (loop_status)); }

void
gvc_mpris_player_set_position (GvcMprisPlayer *player,
                               const gchar    *track_id,
                               gint64          position)
{
        if (!player->priv->can_seek || track_id == NULL || track_id[0] == '\0')
                return;

        gvc_mpris_player_call (player,
                               player->priv->player_proxy,
                               "SetPosition",
                               g_variant_new ("(ox)", track_id, position));
}

typedef struct
{
        GvcMprisPlayer          *player;
        GvcMprisPositionCallback callback;
        gpointer                 user_data;
} PositionData;

static void
on_get_position_ready (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
        PositionData   *data = user_data;
        GVariant       *result;
        GVariant       *value = NULL;
        GError         *error = NULL;
        gint64          position = 0;

        result = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object), res, &error);
        if (result != NULL) {
                g_variant_get (result, "(v)", &value);
                position = g_variant_get_int64 (value);
                g_variant_unref (value);
                g_variant_unref (result);
        }

        if (data->callback != NULL)
                data->callback (data->player, position, error, data->user_data);

        if (error != NULL)
                g_error_free (error);
        g_object_unref (data->player);
        g_free (data);
}

void
gvc_mpris_player_get_position_async (GvcMprisPlayer          *player,
                                     GvcMprisPositionCallback callback,
                                     gpointer                 user_data)
{
        PositionData *data;

        g_return_if_fail (GVC_IS_MPRIS_PLAYER (player));

        if (player->priv->player_proxy == NULL || !player->priv->can_seek)
                return;

        data = g_new0 (PositionData, 1);
        data->player = g_object_ref (player);
        data->callback = callback;
        data->user_data = user_data;

        g_dbus_connection_call (g_dbus_proxy_get_connection (player->priv->player_proxy),
                                player->priv->bus_name,
                                MPRIS_OBJECT_PATH,
                                DBUS_PROPERTIES_IFACE,
                                "Get",
                                g_variant_new ("(ss)", MPRIS_PLAYER_IFACE, "Position"),
                                G_VARIANT_TYPE ("(v)"),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL,
                                on_get_position_ready,
                                data);
}
