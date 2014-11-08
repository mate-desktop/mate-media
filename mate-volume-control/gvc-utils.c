/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libmatemixer/matemixer.h>

#include "gvc-utils.h"

/* libcanberra requires a PulseAudio channel name to be given to its
 * CA_PROP_CANBERRA_FORCE_CHANNEL property.
 *
 * The strings here are copied from PulseAudio source code to avoid depending
 * on libpulse. */
static const gchar *pulse_position[MATE_MIXER_CHANNEL_MAX] = {
        [MATE_MIXER_CHANNEL_MONO] = "mono",
        [MATE_MIXER_CHANNEL_FRONT_LEFT] = "front-left",
        [MATE_MIXER_CHANNEL_FRONT_RIGHT] = "front-right",
        [MATE_MIXER_CHANNEL_FRONT_CENTER] = "front-center",
        [MATE_MIXER_CHANNEL_LFE] = "lfe",
        [MATE_MIXER_CHANNEL_BACK_LEFT] = "rear-left",
        [MATE_MIXER_CHANNEL_BACK_RIGHT] = "rear-right",
        [MATE_MIXER_CHANNEL_BACK_CENTER] = "rear-center",
        [MATE_MIXER_CHANNEL_FRONT_LEFT_CENTER] = "front-left-of-center",
        [MATE_MIXER_CHANNEL_FRONT_RIGHT_CENTER] = "front-right-of-center",
        [MATE_MIXER_CHANNEL_SIDE_LEFT] = "side-left",
        [MATE_MIXER_CHANNEL_SIDE_RIGHT] = "side-right",
        [MATE_MIXER_CHANNEL_TOP_FRONT_LEFT] = "top-front-left",
        [MATE_MIXER_CHANNEL_TOP_FRONT_RIGHT] = "top-front-right",
        [MATE_MIXER_CHANNEL_TOP_FRONT_CENTER] = "top-front-center",
        [MATE_MIXER_CHANNEL_TOP_CENTER] = "top-center",
        [MATE_MIXER_CHANNEL_TOP_BACK_LEFT] = "top-rear-left",
        [MATE_MIXER_CHANNEL_TOP_BACK_RIGHT] = "top-rear-right",
        [MATE_MIXER_CHANNEL_TOP_BACK_CENTER] = "top-rear-center"
};

static const gchar *pretty_position[MATE_MIXER_CHANNEL_MAX] = {
        [MATE_MIXER_CHANNEL_UNKNOWN] = N_("Unknown"),
        /* Speaker channel names */
        [MATE_MIXER_CHANNEL_MONO] = N_("Mono"),
        [MATE_MIXER_CHANNEL_FRONT_LEFT] = N_("Front Left"),
        [MATE_MIXER_CHANNEL_FRONT_RIGHT] = N_("Front Right"),
        [MATE_MIXER_CHANNEL_FRONT_CENTER] = N_("Front Center"),
        [MATE_MIXER_CHANNEL_LFE] = N_("LFE"),
        [MATE_MIXER_CHANNEL_BACK_LEFT] = N_("Rear Left"),
        [MATE_MIXER_CHANNEL_BACK_RIGHT] = N_("Rear Right"),
        [MATE_MIXER_CHANNEL_BACK_CENTER] = N_("Rear Center"),
        [MATE_MIXER_CHANNEL_FRONT_LEFT_CENTER] = N_("Front Left of Center"),
        [MATE_MIXER_CHANNEL_FRONT_RIGHT_CENTER] = N_("Front Right of Center"),
        [MATE_MIXER_CHANNEL_SIDE_LEFT] = N_("Side Left"),
        [MATE_MIXER_CHANNEL_SIDE_RIGHT] = N_("Side Right"),
        [MATE_MIXER_CHANNEL_TOP_FRONT_LEFT] = N_("Top Front Left"),
        [MATE_MIXER_CHANNEL_TOP_FRONT_RIGHT] = N_("Top Front Right"),
        [MATE_MIXER_CHANNEL_TOP_FRONT_CENTER] = N_("Top Front Center"),
        [MATE_MIXER_CHANNEL_TOP_CENTER] = N_("Top Center"),
        [MATE_MIXER_CHANNEL_TOP_BACK_LEFT] = N_("Top Rear Left"),
        [MATE_MIXER_CHANNEL_TOP_BACK_RIGHT] = N_("Top Rear Right"),
        [MATE_MIXER_CHANNEL_TOP_BACK_CENTER] = N_("Top Rear Center")
};

const gchar *
gvc_channel_position_to_pulse_string (MateMixerChannelPosition position)
{
        g_return_val_if_fail (position >= 0 && position < MATE_MIXER_CHANNEL_MAX, NULL);

        return pulse_position[position];
}

const gchar *
gvc_channel_position_to_pretty_string (MateMixerChannelPosition position)
{
        g_return_val_if_fail (position >= 0 && position < MATE_MIXER_CHANNEL_MAX, NULL);

        return pretty_position[position];
}

const gchar *
gvc_channel_map_to_pretty_string (MateMixerStreamControl *control)
{
        g_return_val_if_fail (MATE_MIXER_IS_STREAM_CONTROL (control), NULL);

#define HAS_POSITION(p) (mate_mixer_stream_control_has_channel_position (control, (p)))

        /* Modeled after PulseAudio 5.0, probably could be extended with other combinations */
        switch (mate_mixer_stream_control_get_num_channels (control)) {
        case 1:
                if (HAS_POSITION (MATE_MIXER_CHANNEL_MONO))
                        return _("Mono");
                break;
        case 2:
                if (HAS_POSITION (MATE_MIXER_CHANNEL_FRONT_LEFT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_FRONT_RIGHT))
                        return _("Stereo");
                break;
        case 4:
                if (HAS_POSITION (MATE_MIXER_CHANNEL_FRONT_LEFT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_FRONT_RIGHT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_BACK_LEFT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_BACK_RIGHT))
                        return _("Surround 4.0");
                break;
        case 5:
                if (HAS_POSITION (MATE_MIXER_CHANNEL_FRONT_LEFT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_FRONT_RIGHT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_BACK_LEFT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_BACK_RIGHT))
                        if (HAS_POSITION (MATE_MIXER_CHANNEL_LFE))
                                return _("Surround 4.1");
                        if (HAS_POSITION (MATE_MIXER_CHANNEL_FRONT_CENTER))
                                return _("Surround 5.0");
                break;
        case 6:
                if (HAS_POSITION (MATE_MIXER_CHANNEL_FRONT_LEFT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_FRONT_RIGHT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_FRONT_CENTER) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_BACK_LEFT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_BACK_RIGHT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_LFE))
                        return _("Surround 5.1");
                break;
        case 8:
                if (HAS_POSITION (MATE_MIXER_CHANNEL_FRONT_LEFT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_FRONT_RIGHT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_FRONT_CENTER) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_BACK_LEFT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_BACK_RIGHT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_SIDE_LEFT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_SIDE_RIGHT) &&
                    HAS_POSITION (MATE_MIXER_CHANNEL_LFE))
                        return _("Surround 7.1");
                break;
        }

#undef HAS_POSITION

        return NULL;
}

#if GTK_CHECK_VERSION (3, 0, 0)
/* Taken from gtkstyle.c */
static void rgb_to_hls (gdouble *r, gdouble *g, gdouble *b);
static void hls_to_rgb (gdouble *h, gdouble *l, gdouble *s);

void
gvc_color_shade (GdkRGBA *a, GdkRGBA *b, gdouble k)
{
        gdouble red;
        gdouble green;
        gdouble blue;

        red = (gdouble) a->red / 65535.0;
        green = (gdouble) a->green / 65535.0;
        blue = (gdouble) a->blue / 65535.0;

        rgb_to_hls (&red, &green, &blue);

        green *= k;
        if (green > 1.0)
                green = 1.0;
        else if (green < 0.0)
                green = 0.0;

        blue *= k;
        if (blue > 1.0)
                blue = 1.0;
        else if (blue < 0.0)
                blue = 0.0;

        hls_to_rgb (&red, &green, &blue);

        b->red = red * 65535.0;
        b->green = green * 65535.0;
        b->blue = blue * 65535.0;
}

static void
rgb_to_hls (gdouble *r, gdouble *g, gdouble *b)
{
        gdouble min;
        gdouble max;
        gdouble red;
        gdouble green;
        gdouble blue;
        gdouble h, l, s;
        gdouble delta;

        red = *r;
        green = *g;
        blue = *b;

        if (red > green) {
                if (red > blue)
                        max = red;
                else
                        max = blue;

                if (green < blue)
                        min = green;
                else
                        min = blue;
        } else {
                if (green > blue)
                        max = green;
                else
                        max = blue;

                if (red < blue)
                        min = red;
                else
                        min = blue;
        }

        l = (max + min) / 2;
        s = 0;
        h = 0;

        if (max != min) {
                if (l <= 0.5)
                        s = (max - min) / (max + min);
                else
                        s = (max - min) / (2 - max - min);

                delta = max - min;
                if (red == max)
                        h = (green - blue) / delta;
                else if (green == max)
                        h = 2 + (blue - red) / delta;
                else if (blue == max)
                        h = 4 + (red - green) / delta;

                h *= 60;
                if (h < 0.0)
                        h += 360;
        }

        *r = h;
        *g = l;
        *b = s;
}

static void
hls_to_rgb (gdouble *h, gdouble *l, gdouble *s)
{
        gdouble hue;
        gdouble lightness;
        gdouble saturation;
        gdouble m1, m2;
        gdouble r, g, b;

        lightness = *l;
        saturation = *s;

        if (lightness <= 0.5)
                m2 = lightness * (1 + saturation);
        else
                m2 = lightness + saturation - lightness * saturation;
        m1 = 2 * lightness - m2;

        if (saturation == 0) {
                *h = lightness;
                *l = lightness;
                *s = lightness;
        } else {
                hue = *h + 120;
                while (hue > 360)
                        hue -= 360;
                while (hue < 0)
                        hue += 360;

                if (hue < 60)
                        r = m1 + (m2 - m1) * hue / 60;
                else if (hue < 180)
                        r = m2;
                else if (hue < 240)
                        r = m1 + (m2 - m1) * (240 - hue) / 60;
                else
                        r = m1;

                hue = *h;
                while (hue > 360)
                        hue -= 360;
                while (hue < 0)
                        hue += 360;

                if (hue < 60)
                        g = m1 + (m2 - m1) * hue / 60;
                else if (hue < 180)
                        g = m2;
                else if (hue < 240)
                        g = m1 + (m2 - m1) * (240 - hue) / 60;
                else
                        g = m1;

                hue = *h - 120;
                while (hue > 360)
                        hue -= 360;
                while (hue < 0)
                        hue += 360;

                if (hue < 60)
                        b = m1 + (m2 - m1) * hue / 60;
                else if (hue < 180)
                        b = m2;
                else if (hue < 240)
                        b = m1 + (m2 - m1) * (240 - hue) / 60;
                else
                        b = m1;

                *h = r;
                *l = g;
                *s = b;
        }
}
#endif
