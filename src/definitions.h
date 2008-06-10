/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include "gentime.h"

#define FRAME_SIZE 90
#define MAXCLIPDURATION 15000

enum OPERATIONTYPE { NONE = 0, MOVE = 1, RESIZESTART = 2, RESIZEEND = 3, FADEIN = 4, FADEOUT = 5, TRANSITIONSTART = 6, TRANSITIONEND = 7, MOVEGUIDE = 8};
enum CLIPTYPE { UNKNOWN = 0, AUDIO = 1, VIDEO = 2, AV = 3, COLOR = 4, IMAGE = 5, TEXT = 6, SLIDESHOW = 7, VIRTUAL = 8, PLAYLIST = 9, FOLDER = 10};
enum GRAPHICSRECTITEM { AVWIDGET = 70000 , LABELWIDGET , TRANSITIONWIDGET };

enum PROJECTTOOL { SELECTTOOL = 0 , RAZORTOOL = 1 };

enum TRANSITIONTYPE {
    /** TRANSITIONTYPE: between 0-99: video trans, 100-199: video+audio trans, 200-299: audio trans */
    LUMA_TRANSITION = 0,
    COMPOSITE_TRANSITION = 1,
    PIP_TRANSITION = 2,
    LUMAFILE_TRANSITION = 3,
    MIX_TRANSITION = 200
};

enum TRACKTYPE { AUDIOTRACK = 0, VIDEOTRACK = 1 };

struct TrackInfo {
    TRACKTYPE type;
    bool isMute;
    bool isBlind;
};

struct ItemInfo {
    GenTime startPos;
    GenTime endPos;
    int track;
};

struct MltVideoProfile {
    QString path;
    QString description;
    int frame_rate_num;
    int frame_rate_den;
    int width;
    int height;
    bool progressive;
    int sample_aspect_num;
    int sample_aspect_den;
    int display_aspect_num;
    int display_aspect_den;
};

#endif
