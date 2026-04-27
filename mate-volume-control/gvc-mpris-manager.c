/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

#include "config.h"

#include <string.h>

#include "gvc-mpris-manager.h"

#define DBUS_NAME "org.freedesktop.DBus"
#define DBUS_PATH "/org/freedesktop/DBus"
#define DBUS_IFACE "org.freedesktop.DBus"
#define MPRIS_PREFIX "org.mpris.MediaPlayer2."

struct _GvcMprisManagerPrivate
{
        GDBusConnection *connection;
        GHashTable      *players;
        guint            name_owner_changed_id;
};

enum
{
        PLAYER_ADDED,
        PLAYER_REMOVED,
        N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GvcMprisManager, gvc_mpris_manager, G_TYPE_OBJECT)

static gboolean
is_mpris_name (const gchar *name)
{
        return name != NULL && g_str_has_prefix (name, MPRIS_PREFIX);
}

static void
on_player_ready (GvcMprisPlayer  *player,
                 GvcMprisManager *manager)
{
        g_signal_emit (manager, signals[PLAYER_ADDED], 0, player);
}

static void
manager_add_player (GvcMprisManager *manager,
                    const gchar     *bus_name,
                    const gchar     *owner)
{
        GvcMprisPlayer *player;

        if (g_hash_table_contains (manager->priv->players, bus_name))
                return;

        player = gvc_mpris_player_new (bus_name, owner);
        g_signal_connect (player, "ready", G_CALLBACK (on_player_ready), manager);
        g_hash_table_insert (manager->priv->players, g_strdup (bus_name), player);
}

static void
manager_remove_player (GvcMprisManager *manager,
                       const gchar     *bus_name)
{
        if (!g_hash_table_contains (manager->priv->players, bus_name))
                return;

        g_hash_table_remove (manager->priv->players, bus_name);
        g_signal_emit (manager, signals[PLAYER_REMOVED], 0, bus_name);
}

static void
on_get_name_owner_ready (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
        GvcMprisManager *manager;
        GTask           *task = G_TASK (user_data);
        GVariant        *result;
        GError          *error = NULL;
        gchar           *owner = NULL;
        gchar           *bus_name;

        manager = g_task_get_source_object (task);
        bus_name = g_object_get_data (G_OBJECT (task), "gvc-bus-name");
        result = g_dbus_connection_call_finish (manager->priv->connection, res, &error);

        if (result != NULL) {
                g_variant_get (result, "(s)", &owner);
                if (is_mpris_name (bus_name))
                        manager_add_player (manager, bus_name, owner);
                g_free (owner);
                g_variant_unref (result);
        }

        if (error != NULL)
                g_error_free (error);

        g_object_unref (task);
}

static void
manager_request_name_owner (GvcMprisManager *manager,
                            const gchar     *bus_name)
{
        GTask *task;

        task = g_task_new (manager, NULL, NULL, NULL);
        g_object_set_data_full (G_OBJECT (task), "gvc-bus-name", g_strdup (bus_name), g_free);
        g_dbus_connection_call (manager->priv->connection,
                                DBUS_NAME,
                                DBUS_PATH,
                                DBUS_IFACE,
                                "GetNameOwner",
                                g_variant_new ("(s)", bus_name),
                                G_VARIANT_TYPE ("(s)"),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL,
                                on_get_name_owner_ready,
                                task);
}

static void
on_list_names_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
        GvcMprisManager *manager = GVC_MPRIS_MANAGER (user_data);
        GVariant        *result;
        GError          *error = NULL;
        gchar          **names = NULL;
        gint             i;

        result = g_dbus_connection_call_finish (manager->priv->connection, res, &error);
        if (result != NULL) {
                g_variant_get (result, "(^as)", &names);
                for (i = 0; names != NULL && names[i] != NULL; i++) {
                        if (is_mpris_name (names[i]))
                                manager_request_name_owner (manager, names[i]);
                }
                g_strfreev (names);
                g_variant_unref (result);
        }

        if (error != NULL)
                g_error_free (error);

        g_object_unref (manager);
}

static void
on_name_owner_changed (GDBusConnection *connection,
                       const gchar     *sender_name,
                       const gchar     *object_path,
                       const gchar     *interface_name,
                       const gchar     *signal_name,
                       GVariant        *parameters,
                       gpointer         user_data)
{
        GvcMprisManager *manager = GVC_MPRIS_MANAGER (user_data);
        const gchar     *name;
        const gchar     *old_owner;
        const gchar     *new_owner;

        g_variant_get (parameters, "(&s&s&s)", &name, &old_owner, &new_owner);

        if (!is_mpris_name (name))
                return;

        if (new_owner[0] != '\0') {
                if (old_owner[0] != '\0')
                        manager_remove_player (manager, name);
                manager_add_player (manager, name, new_owner);
        } else if (old_owner[0] != '\0') {
                manager_remove_player (manager, name);
        }
}

static void
on_bus_ready (GObject      *source_object,
              GAsyncResult *res,
              gpointer      user_data)
{
        GvcMprisManager *manager = GVC_MPRIS_MANAGER (user_data);
        GError          *error = NULL;

        manager->priv->connection = g_bus_get_finish (res, &error);
        if (error != NULL) {
                g_warning ("Unable to connect to the session bus for MPRIS: %s", error->message);
                g_error_free (error);
                g_object_unref (manager);
                return;
        }

        manager->priv->name_owner_changed_id =
                g_dbus_connection_signal_subscribe (manager->priv->connection,
                                                    DBUS_NAME,
                                                    DBUS_IFACE,
                                                    "NameOwnerChanged",
                                                    DBUS_PATH,
                                                    NULL,
                                                    G_DBUS_SIGNAL_FLAGS_NONE,
                                                    on_name_owner_changed,
                                                    manager,
                                                    NULL);

        g_dbus_connection_call (manager->priv->connection,
                                DBUS_NAME,
                                DBUS_PATH,
                                DBUS_IFACE,
                                "ListNames",
                                NULL,
                                G_VARIANT_TYPE ("(as)"),
                                G_DBUS_CALL_FLAGS_NONE,
                                -1,
                                NULL,
                                on_list_names_ready,
                                g_object_ref (manager));

        g_object_unref (manager);
}

static void
gvc_mpris_manager_dispose (GObject *object)
{
        GvcMprisManager *manager = GVC_MPRIS_MANAGER (object);

        if (manager->priv->connection != NULL &&
            manager->priv->name_owner_changed_id != 0) {
                g_dbus_connection_signal_unsubscribe (manager->priv->connection,
                                                      manager->priv->name_owner_changed_id);
                manager->priv->name_owner_changed_id = 0;
        }

        g_clear_object (&manager->priv->connection);
        g_clear_pointer (&manager->priv->players, g_hash_table_unref);

        G_OBJECT_CLASS (gvc_mpris_manager_parent_class)->dispose (object);
}

static void
gvc_mpris_manager_class_init (GvcMprisManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = gvc_mpris_manager_dispose;

        signals[PLAYER_ADDED] =
                g_signal_new ("player-added",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL, NULL,
                              G_TYPE_NONE,
                              1,
                              GVC_TYPE_MPRIS_PLAYER);
        signals[PLAYER_REMOVED] =
                g_signal_new ("player-removed",
                              G_TYPE_FROM_CLASS (klass),
                              G_SIGNAL_RUN_LAST,
                              0, NULL, NULL, NULL,
                              G_TYPE_NONE,
                              1,
                              G_TYPE_STRING);
}

static void
gvc_mpris_manager_init (GvcMprisManager *manager)
{
        manager->priv = gvc_mpris_manager_get_instance_private (manager);
        manager->priv->players = g_hash_table_new_full (g_str_hash,
                                                        g_str_equal,
                                                        g_free,
                                                        g_object_unref);
}

GvcMprisManager *
gvc_mpris_manager_new (void)
{
        GvcMprisManager *manager;

        manager = g_object_new (GVC_TYPE_MPRIS_MANAGER, NULL);
        g_bus_get (G_BUS_TYPE_SESSION, NULL, on_bus_ready, g_object_ref (manager));

        return manager;
}

GList *
gvc_mpris_manager_get_players (GvcMprisManager *manager)
{
        g_return_val_if_fail (GVC_IS_MPRIS_MANAGER (manager), NULL);
        return g_hash_table_get_values (manager->priv->players);
}

GvcMprisPlayer *
gvc_mpris_manager_get_best_player (GvcMprisManager *manager)
{
        GHashTableIter iter;
        gpointer       value;
        GvcMprisPlayer *fallback = NULL;

        g_return_val_if_fail (GVC_IS_MPRIS_MANAGER (manager), NULL);

        g_hash_table_iter_init (&iter, manager->priv->players);
        while (g_hash_table_iter_next (&iter, NULL, &value)) {
                GvcMprisPlayer *player = value;

                if (!gvc_mpris_player_is_ready (player))
                        continue;

                if (gvc_mpris_player_is_playing (player))
                        return player;

                if (fallback == NULL)
                        fallback = player;
        }

        return fallback;
}
