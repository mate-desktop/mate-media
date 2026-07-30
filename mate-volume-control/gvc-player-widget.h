/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

#ifndef __GVC_PLAYER_WIDGET_H
#define __GVC_PLAYER_WIDGET_H

#include <gtk/gtk.h>

#include "gvc-mpris-player.h"

G_BEGIN_DECLS

#define GVC_TYPE_PLAYER_WIDGET         (gvc_player_widget_get_type ())
#define GVC_PLAYER_WIDGET(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GVC_TYPE_PLAYER_WIDGET, GvcPlayerWidget))
#define GVC_IS_PLAYER_WIDGET(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GVC_TYPE_PLAYER_WIDGET))

typedef struct _GvcPlayerWidget        GvcPlayerWidget;
typedef struct _GvcPlayerWidgetClass   GvcPlayerWidgetClass;
typedef struct _GvcPlayerWidgetPrivate GvcPlayerWidgetPrivate;

struct _GvcPlayerWidget
{
        GtkBox                  parent;
        GvcPlayerWidgetPrivate *priv;
};

struct _GvcPlayerWidgetClass
{
        GtkBoxClass             parent_class;
};

GType       gvc_player_widget_get_type   (void) G_GNUC_CONST;
GtkWidget * gvc_player_widget_new        (void);
void        gvc_player_widget_set_player (GvcPlayerWidget *widget,
                                           GvcMprisPlayer  *player);
void        gvc_player_widget_set_players (GvcPlayerWidget *widget,
                                            GList           *players,
                                            GvcMprisPlayer  *active_player);
gboolean    gvc_player_widget_scroll_is_seek (GvcPlayerWidget *widget,
                                               GdkEventScroll  *event);
gboolean    gvc_player_widget_handle_seek_scroll (GvcPlayerWidget *widget,
                                                   GdkEventScroll  *event);

G_END_DECLS

#endif /* __GVC_PLAYER_WIDGET_H */
