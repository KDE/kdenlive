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

#include <KDebug>
#include <KLocale>

#include "v4lcapture.h"
#include "kdenlivesettings.h"


static src_t v4lsrc;

void yuv2rgb_int3(unsigned char *yuv_buffer, unsigned char *rgb_buffer, int width, int height)
{
    int len;
    int r, g, b;
    int Y, U, V, Y2;
    int rgb_ptr, y_ptr, t;

    len = width * height / 2;

    rgb_ptr = 0;
    y_ptr = 0;

    for(t = 0; t < len; t++) { /* process 2 pixels at a time */
        /* Compute parts of the UV components */

        U = yuv_buffer[y_ptr];
        Y = yuv_buffer[y_ptr+1];
        V = yuv_buffer[y_ptr+2];
        Y2 = yuv_buffer[y_ptr+3];
        y_ptr += 4;


        /*r = 1.164*(Y-16) + 1.596*(V-128);
        g = 1.164*(Y-16) - 0.813*(V-128) - 0.391*(U-128);
        b = 1.164*(Y-16) + 2.018*(U-128);*/


        r = ((298 * (Y - 16)               + 409 * (V - 128) + 128) >> 8);

        g = ((298 * (Y - 16) - 100 * (U - 128) - 208 * (V - 128) + 128) >> 8);

        b = ((298 * (Y - 16) + 516 * (U - 128)               + 128) >> 8);

        if(r > 255) r = 255;
        if(g > 255) g = 255;
        if(b > 255) b = 255;

        if(r < 0) r = 0;
        if(g < 0) g = 0;
        if(b < 0) b = 0;

        rgb_buffer[rgb_ptr] = b;
        rgb_buffer[rgb_ptr+1] = g;
        rgb_buffer[rgb_ptr+2] = r;
        rgb_buffer[rgb_ptr+3] = 255;

        rgb_ptr += 4;
        /*r = 1.164*(Y2-16) + 1.596*(V-128);
        g = 1.164*(Y2-16) - 0.813*(V-128) - 0.391*(U-128);
        b = 1.164*(Y2-16) + 2.018*(U-128);*/


        r = ((298 * (Y2 - 16)               + 409 * (V - 128) + 128) >> 8);

        g = ((298 * (Y2 - 16) - 100 * (U - 128) - 208 * (V - 128) + 128) >> 8);

        b = ((298 * (Y2 - 16) + 516 * (U - 128)               + 128) >> 8);

        if(r > 255) r = 255;
        if(g > 255) g = 255;
        if(b > 255) b = 255;

        if(r < 0) r = 0;
        if(g < 0) g = 0;
        if(b < 0) b = 0;

        rgb_buffer[rgb_ptr] = b;
        rgb_buffer[rgb_ptr+1] = g;
        rgb_buffer[rgb_ptr+2] = r;
        rgb_buffer[rgb_ptr+3] = 255;
        rgb_ptr += 4;
    }
}

V4lCaptureHandler::V4lCaptureHandler(QVBoxLayout *lay, QWidget *parent):
    CaptureHandler(lay, parent)
    , m_update(false)
{
    m_display = new QLabel;
    lay->addWidget(m_display);
}

void V4lCaptureHandler::startPreview(int /*deviceId*/, int /*captureMode*/)
{
    m_display->setHidden(false);
    fswebcam_config_t *config;
    /* Prepare the configuration structure. */
    config = (fswebcam_config_t *) calloc(sizeof(fswebcam_config_t), 1);
    if(!config) {
        /*WARN("Out of memory.");*/
        fprintf(stderr, "Out of MEM....");
        return;
    }

    /* Set the defaults. */
    config->loop = 0;
    config->offset = 0;
    config->background = 0;
    config->pidfile = NULL;
    config->logfile = NULL;
    config->gmt = 0;
    config->start = 0;
    config->device = strdup(KdenliveSettings::video4vdevice().toUtf8().constData());
    config->input = NULL;
    config->tuner = 0;
    config->frequency = 0;
    config->delay = 0;
    config->use_read = 0;
    config->list = 0;
    config->width = KdenliveSettings::video4size().section("x", 0, 0).toInt();/*384;*/
    config->height = KdenliveSettings::video4size().section("x", -1).toInt();/*288;*/
    config->fps = 0;
    config->frames = 1;
    config->skipframes = 0;
    config->palette = SRC_PAL_ANY;
    config->option = NULL;
    config->dumpframe = NULL;
    config->jobs = 0;
    config->job = NULL;

    /* Set defaults and parse the command line. */
    /*if(fswc_getopts(config, argc, argv)) return(-1);*/


    /* Record the start time. */
    config->start = time(NULL);
    /* Set source options... */
    memset(&v4lsrc, 0, sizeof(v4lsrc));
    v4lsrc.input      = config->input;
    v4lsrc.tuner      = config->tuner;
    v4lsrc.frequency  = config->frequency;
    v4lsrc.delay      = config->delay;
    v4lsrc.timeout    = 10; /* seconds */
    v4lsrc.use_read   = config->use_read;
    v4lsrc.list       = config->list;
    v4lsrc.palette    = config->palette;
    v4lsrc.width      = config->width;
    v4lsrc.height     = config->height;
    v4lsrc.fps        = config->fps;
    v4lsrc.option     = config->option;
    char *source = config->device;

    if(src_open(&v4lsrc, source) != 0) return;
    m_update = true;
    QTimer::singleShot(200, this, SLOT(slotUpdate()));
}

V4lCaptureHandler::~V4lCaptureHandler()
{
    stopCapture();
}

void V4lCaptureHandler::slotUpdate()
{
    if(!m_update) return;
    src_grab(&v4lsrc);
    uint8_t *img = (uint8_t *) v4lsrc.img;
    uint32_t i = v4lsrc.width * v4lsrc.height;

    if(v4lsrc.length << 2 < i) return;

    QImage qimg(v4lsrc.width, v4lsrc.height, QImage::Format_RGB32);
    //Format_ARGB32_Premultiplied
    //convert from uyvy422 to rgba
    yuv2rgb_int3((uchar *)img, (uchar *)qimg.bits(), v4lsrc.width, v4lsrc.height);
    if(!m_captureFramePath.isEmpty()) {
        qimg.save(m_captureFramePath);
        m_captureFramePath.clear();
    }
    m_display->setPixmap(QPixmap::fromImage(qimg));
    if(m_update) QTimer::singleShot(200, this, SLOT(slotUpdate()));
}

void V4lCaptureHandler::startCapture(const QString &/*path*/)
{
}

void V4lCaptureHandler::stopCapture()
{
}

void V4lCaptureHandler::captureFrame(const QString &fname)
{
    m_captureFramePath = fname;
}

void V4lCaptureHandler::showOverlay(QImage /*img*/, bool /*transparent*/)
{
}

void V4lCaptureHandler::hideOverlay()
{
}

void V4lCaptureHandler::hidePreview(bool hide)
{
    m_display->setHidden(hide);
}

void V4lCaptureHandler::stopPreview()
{
    if(!m_update) return;
    m_update = false;
    src_close(&v4lsrc);
}
