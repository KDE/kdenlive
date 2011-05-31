/***************************************************************************
 *   Copyright (C) 2010 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>

#include <QDebug>
#include <QImage>
#include <QTimer>
#include <QPainter>

#include <KDebug>
#include <KLocale>

#include "v4lcapture.h"
#include "kdenlivesettings.h"


V4lCaptureHandler::V4lCaptureHandler()
{
}

//static

QStringList V4lCaptureHandler::getDeviceName(QString input)
{
    src_t v4lsrc;

    /* Set source options... */
    memset(&v4lsrc, 0, sizeof(v4lsrc));
    v4lsrc.input      = NULL;
    v4lsrc.tuner      = 0;
    v4lsrc.frequency  = 0;
    v4lsrc.delay      = 0;
    v4lsrc.timeout    = 10; /* seconds */
    v4lsrc.use_read   = 0;
    v4lsrc.list       = 0;
    v4lsrc.palette    = SRC_PAL_ANY;
    v4lsrc.width      = 384;
    v4lsrc.height     = 288;
    v4lsrc.fps        = 0;
    v4lsrc.option     = NULL;
    v4lsrc.source     = strdup(input.toUtf8().constData());
    char *pixelformatdescription;
    pixelformatdescription = (char *) calloc(2048, sizeof(char));
    QStringList result;
    const char *devName = query_v4ldevice(&v4lsrc, &pixelformatdescription);
    if (devName == NULL) return result; 
    QString deviceName(devName);
    QString info(pixelformatdescription);
    free (pixelformatdescription);
    result << (deviceName.isEmpty() ? input : deviceName) << info;
    return result;
}



