/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

#ifndef __GVC_MPRIS_MANAGER_H
#define __GVC_MPRIS_MANAGER_H

#include <gio/gio.h>
#include <glib-object.h>

#include "gvc-mpris-player.h"

G_BEGIN_DECLS

#define GVC_TYPE_MPRIS_MANAGER         (gvc_mpris_manager_get_type ())
#define GVC_MPRIS_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GVC_TYPE_MPRIS_MANAGER, GvcMprisManager))
#define GVC_IS_MPRIS_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GVC_TYPE_MPRIS_MANAGER))

typedef struct _GvcMprisManager        GvcMprisManager;
typedef struct _GvcMprisManagerClass   GvcMprisManagerClass;
typedef struct _GvcMprisManagerPrivate GvcMprisManagerPrivate;

struct _GvcMprisManager
{
        GObject                  parent;
        GvcMprisManagerPrivate  *priv;
};

struct _GvcMprisManagerClass
{
        GObjectClass             parent_class;
};

GType            gvc_mpris_manager_get_type        (void) G_GNUC_CONST;
GvcMprisManager *gvc_mpris_manager_new             (void);
GList *          gvc_mpris_manager_get_players     (GvcMprisManager *manager);
GvcMprisPlayer * gvc_mpris_manager_get_best_player (GvcMprisManager *manager);

G_END_DECLS

#endif /* __GVC_MPRIS_MANAGER_H */
