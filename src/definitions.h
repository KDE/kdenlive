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

#define FRAME_SIZE 90
#define MAXCLIPDURATION 15000

enum OPERATIONTYPE { NONE = 0, MOVE = 1, RESIZESTART = 2, RESIZEEND = 3, FADEIN = 4, FADEOUT = 5};
enum CLIPTYPE { UNKNOWN = 0, AUDIO = 1, VIDEO = 2, AV = 3, COLOR = 4, IMAGE = 5, TEXT = 6, SLIDESHOW = 7, VIRTUAL = 8, PLAYLIST = 9};

struct TrackViewClip {
    int startTime;
    int duration;
    int cropTime;
    QString producer;
};

#endif
