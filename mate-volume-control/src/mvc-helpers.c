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

#include <glib.h>
#include <glib/gi18n.h>

#include <libmatemixer/matemixer.h>

#ifdef HAVE_PULSEAUDIO
#include <pulse/pulseaudio.h>
#endif

#include "mvc-helpers.h"

#ifdef HAVE_PULSEAUDIO
static pa_position_t
position_to_pulse (MateMixerChannelPosition position)
{
        switch (position) {
        case MATE_MIXER_CHANNEL_MONO:
                return PA_CHANNEL_POSITION_MONO;
        case MATE_MIXER_CHANNEL_FRONT_LEFT:
                return PA_CHANNEL_POSITION_FRONT_LEFT;
        case MATE_MIXER_CHANNEL_FRONT_RIGHT:
                return PA_CHANNEL_POSITION_FRONT_RIGHT;
        case MATE_MIXER_CHANNEL_FRONT_CENTER:
                return PA_CHANNEL_POSITION_FRONT_CENTER;
        case MATE_MIXER_CHANNEL_LFE:
                return PA_CHANNEL_POSITION_LFE;
        case MATE_MIXER_CHANNEL_BACK_LEFT:
                return PA_CHANNEL_POSITION_REAR_LEFT;
        case MATE_MIXER_CHANNEL_BACK_RIGHT:
                return PA_CHANNEL_POSITION_REAR_RIGHT;
        case MATE_MIXER_CHANNEL_BACK_CENTER:
                return PA_CHANNEL_POSITION_REAR_CENTER;
        case MATE_MIXER_CHANNEL_FRONT_LEFT_CENTER:
                return PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
        case MATE_MIXER_CHANNEL_FRONT_RIGHT_CENTER:
                return PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
        case MATE_MIXER_CHANNEL_SIDE_LEFT:
                return PA_CHANNEL_POSITION_SIDE_LEFT;
        case MATE_MIXER_CHANNEL_SIDE_RIGHT:
                return PA_CHANNEL_POSITION_SIDE_RIGHT;
        case MATE_MIXER_CHANNEL_TOP_FRONT_LEFT:
                return PA_CHANNEL_POSITION_TOP_FRONT_LEFT;
        case MATE_MIXER_CHANNEL_TOP_FRONT_RIGHT:
                return PA_CHANNEL_POSITION_TOP_FRONT_RIGHT;
        case MATE_MIXER_CHANNEL_TOP_FRONT_CENTER:
                return PA_CHANNEL_POSITION_TOP_FRONT_CENTER;
        case MATE_MIXER_CHANNEL_TOP_CENTER:
                return PA_CHANNEL_POSITION_TOP_CENTER;
        case MATE_MIXER_CHANNEL_TOP_BACK_LEFT:
                return PA_CHANNEL_POSITION_TOP_REAR_LEFT;
        case MATE_MIXER_CHANNEL_TOP_BACK_RIGHT:
                return PA_CHANNEL_POSITION_TOP_REAR_RIGHT;
        case MATE_MIXER_CHANNEL_TOP_BACK_CENTER:
                return PA_CHANNEL_POSITION_TOP_REAR_CENTER;
        default:
                return PA_CHANNEL_POSITION_INVALID;
        }
}
#endif

const gchar *
mvc_channel_position_to_string (MateMixerChannelPosition position)
{
#ifdef HAVE_PULSEAUDIO
        return pa_channel_position_to_string (position_to_pulse (position));
#endif
        return NULL;
}

const gchar *
mvc_channel_position_to_pretty_string (MateMixerChannelPosition position)
{
#ifdef HAVE_PULSEAUDIO
        return pa_channel_position_to_pretty_string (position_to_pulse (position));
#endif
        return NULL;
}

const gchar *
mvc_channel_map_to_pretty_string (MateMixerStream *stream)
{
        g_return_val_if_fail (MATE_MIXER_IS_STREAM (stream), NULL);

        /* Modeled after PulseAudio 5.0, probably could be extended with other combinations */
        switch (mate_mixer_stream_get_num_channels (stream)) {
        case 1:
                if (mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_MONO))
                        return _("Mono");
                break;
        case 2:
                if (mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_FRONT_LEFT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_FRONT_RIGHT))
                        return _("Stereo");
                break;
        case 4:
                if (mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_FRONT_LEFT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_FRONT_RIGHT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_BACK_LEFT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_BACK_RIGHT))
                        return _("Surround 4.0");
                break;
        case 5:
                if (mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_FRONT_LEFT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_FRONT_RIGHT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_BACK_LEFT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_BACK_RIGHT))
                        if (mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_LFE))
                                return _("Surround 4.1");
                        if (mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_FRONT_CENTER))
                                return _("Surround 5.0");
                break;
        case 6:
                if (mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_FRONT_LEFT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_FRONT_RIGHT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_FRONT_CENTER) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_BACK_LEFT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_BACK_RIGHT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_LFE))
                        return _("Surround 5.1");
                break;
        case 8:
                if (mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_FRONT_LEFT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_FRONT_RIGHT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_FRONT_CENTER) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_BACK_LEFT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_BACK_RIGHT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_SIDE_LEFT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_SIDE_RIGHT) &&
                    mate_mixer_stream_has_position (stream, MATE_MIXER_CHANNEL_LFE))
                        return _("Surround 7.1");
                break;
        }

        return NULL;
}
