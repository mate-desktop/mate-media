/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

#ifndef __GVC_MPRIS_PLAYER_H
#define __GVC_MPRIS_PLAYER_H

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GVC_TYPE_MPRIS_PLAYER         (gvc_mpris_player_get_type ())
#define GVC_MPRIS_PLAYER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GVC_TYPE_MPRIS_PLAYER, GvcMprisPlayer))
#define GVC_MPRIS_PLAYER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GVC_TYPE_MPRIS_PLAYER, GvcMprisPlayerClass))
#define GVC_IS_MPRIS_PLAYER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GVC_TYPE_MPRIS_PLAYER))
#define GVC_IS_MPRIS_PLAYER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GVC_TYPE_MPRIS_PLAYER))
#define GVC_MPRIS_PLAYER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GVC_TYPE_MPRIS_PLAYER, GvcMprisPlayerClass))

typedef struct _GvcMprisPlayer        GvcMprisPlayer;
typedef struct _GvcMprisPlayerClass   GvcMprisPlayerClass;
typedef struct _GvcMprisPlayerPrivate GvcMprisPlayerPrivate;

struct _GvcMprisPlayer
{
        GObject                 parent;
        GvcMprisPlayerPrivate  *priv;
};

struct _GvcMprisPlayerClass
{
        GObjectClass            parent_class;
};

typedef void (*GvcMprisPositionCallback) (GvcMprisPlayer *player,
                                          gint64          position,
                                          GError         *error,
                                          gpointer        user_data);

GType            gvc_mpris_player_get_type              (void) G_GNUC_CONST;
GvcMprisPlayer * gvc_mpris_player_new                   (const gchar *bus_name,
                                                         const gchar *owner);

const gchar *    gvc_mpris_player_get_bus_name          (GvcMprisPlayer *player);
const gchar *    gvc_mpris_player_get_owner             (GvcMprisPlayer *player);
gboolean         gvc_mpris_player_is_ready              (GvcMprisPlayer *player);
gboolean         gvc_mpris_player_is_playing            (GvcMprisPlayer *player);

const gchar *    gvc_mpris_player_get_identity          (GvcMprisPlayer *player);
const gchar *    gvc_mpris_player_get_desktop_entry     (GvcMprisPlayer *player);
const gchar *    gvc_mpris_player_get_playback_status   (GvcMprisPlayer *player);
const gchar *    gvc_mpris_player_get_title             (GvcMprisPlayer *player);
const gchar *    gvc_mpris_player_get_artist            (GvcMprisPlayer *player);
const gchar *    gvc_mpris_player_get_album             (GvcMprisPlayer *player);
const gchar *    gvc_mpris_player_get_art_url           (GvcMprisPlayer *player);
const gchar *    gvc_mpris_player_get_track_id          (GvcMprisPlayer *player);
gint64           gvc_mpris_player_get_length            (GvcMprisPlayer *player);

gboolean         gvc_mpris_player_get_can_go_next       (GvcMprisPlayer *player);
gboolean         gvc_mpris_player_get_can_go_previous   (GvcMprisPlayer *player);
gboolean         gvc_mpris_player_get_can_play          (GvcMprisPlayer *player);
gboolean         gvc_mpris_player_get_can_pause         (GvcMprisPlayer *player);
gboolean         gvc_mpris_player_get_can_seek          (GvcMprisPlayer *player);
gboolean         gvc_mpris_player_get_can_raise         (GvcMprisPlayer *player);
gboolean         gvc_mpris_player_get_can_quit          (GvcMprisPlayer *player);
gboolean         gvc_mpris_player_get_shuffle           (GvcMprisPlayer *player);
const gchar *    gvc_mpris_player_get_loop_status       (GvcMprisPlayer *player);

void             gvc_mpris_player_play_pause            (GvcMprisPlayer *player);
void             gvc_mpris_player_stop                  (GvcMprisPlayer *player);
void             gvc_mpris_player_next                  (GvcMprisPlayer *player);
void             gvc_mpris_player_previous              (GvcMprisPlayer *player);
void             gvc_mpris_player_set_position          (GvcMprisPlayer *player,
                                                         const gchar    *track_id,
                                                         gint64          position);
void             gvc_mpris_player_raise                 (GvcMprisPlayer *player);
void             gvc_mpris_player_quit                  (GvcMprisPlayer *player);
void             gvc_mpris_player_set_shuffle           (GvcMprisPlayer *player,
                                                         gboolean        shuffle);
void             gvc_mpris_player_set_loop_status       (GvcMprisPlayer *player,
                                                         const gchar    *loop_status);
void             gvc_mpris_player_get_position_async    (GvcMprisPlayer          *player,
                                                         GvcMprisPositionCallback callback,
                                                         gpointer                 user_data);

G_END_DECLS

#endif /* __GVC_MPRIS_PLAYER_H */
