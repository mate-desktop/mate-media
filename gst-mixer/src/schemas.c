/* MATE Volume Control
 * Copyright (C) 2012 Stefano Karapetsas <stefano@karapetsas.com>
 *
 * schemas.c: useful functions for gsettings
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gio/gio.h>

gboolean
schemas_is_str_in_strv (GSettings *settings, const gchar *key, const gchar *value)
{
    gboolean retval = FALSE;
    gchar **array;
    gint i;

    array = g_settings_get_strv (settings, key);

    if (array) {
        for (i = 0; array[i] != NULL ; i++) {
            if (g_strcmp0 (value, array[i]) == 0) {
                retval = TRUE;
                break;
            }
        }
        g_strfreev (array);
    }
    return retval;
}

/* from gnome-panel */
gboolean
schemas_gsettings_append_strv (GSettings *settings, const gchar *key, const gchar *value)
{
    gchar    **old;
    gchar    **new;
    gint       size;
    gboolean   retval;

    old = g_settings_get_strv (settings, key);

    for (size = 0; old[size] != NULL; size++);

    size += 1; /* appended value */
    size += 1; /* NULL */

    new = g_realloc_n (old, size, sizeof (gchar *));

    new[size - 2] = g_strdup (value);
    new[size - 1] = NULL;

    retval = g_settings_set_strv (settings, key,
                                  (const gchar **) new);

    g_strfreev (new);

    return retval;
}

/* from gnome-panel */
gboolean
schemas_gsettings_remove_all_from_strv (GSettings *settings, const gchar *key, const gchar *value)
{
    GArray    *array;
    gchar    **old;
    gint       i;
    gboolean   changed = FALSE;
    gboolean   retval;

    old = g_settings_get_strv (settings, key);
    array = g_array_new (TRUE, TRUE, sizeof (gchar *));

    for (i = 0; old[i] != NULL; i++) {
        if (g_strcmp0 (old[i], value) != 0)
            array = g_array_append_val (array, old[i]);
        else
            changed = TRUE;
    }

    if (changed)
        retval = g_settings_set_strv (settings, key,
                                      (const gchar **) array->data);
    else
        retval = TRUE;

    g_strfreev (old);
    g_array_free (array, TRUE);

    return retval;
}
