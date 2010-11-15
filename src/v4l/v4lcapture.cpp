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
#include "dec.h"

static src_t v4lsrc;

QImage add_image_png(src_t *src)
{
    QImage im;
    im.loadFromData((uchar *)src->img, src->length, "PNG");
    return im;
}

QImage add_image_jpeg(src_t *src)
{
    uint32_t hlength;
    uint8_t *himg = NULL;
    QImage im;

    /* MJPEG data may lack the DHT segment required for decoding... */
    verify_jpeg_dht((uint8_t *) src->img, src->length, &himg, &hlength);

    im.loadFromData(himg, hlength, "JPG");
    return im;
}

class MyDisplay : public QLabel
{
public:
    MyDisplay(QWidget *parent = 0);
    void setImage(QImage img);
    virtual void paintEvent(QPaintEvent *);
    virtual void resizeEvent(QResizeEvent *);

private:
    QImage m_img;
    bool m_clear;
};

MyDisplay::MyDisplay(QWidget *parent):
    QLabel(parent)
    , m_clear(false)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void MyDisplay::resizeEvent(QResizeEvent *)
{
    m_clear = true;
}

void MyDisplay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (m_clear) {
        // widget resized, cleanup
        p.fillRect(0, 0, width(), height(), palette().background());
        m_clear = false;
    }
    if (m_img.isNull()) return;
    QImage img = m_img.scaled(width(), height(), Qt::KeepAspectRatio);
    p.drawImage((width() - img.width()) / 2, (height() - img.height()) / 2, img);
    p.end();
}

void MyDisplay::setImage(QImage img)
{
    m_img = img;
    update();
}



V4lCaptureHandler::V4lCaptureHandler(QVBoxLayout *lay, QWidget *parent):
    CaptureHandler(lay, parent)
    , m_update(false)
    , m_device(KdenliveSettings::video4vdevice())
    , m_width(-1)
    , m_height(-1)
{
    if (lay == NULL) return;
    m_display = new MyDisplay;
    lay->addWidget(m_display);
}

QStringList V4lCaptureHandler::getDeviceName(QString input)
{
    fswebcam_config_t *config;
    /* Prepare the configuration structure. */
    config = (fswebcam_config_t *) calloc(sizeof(fswebcam_config_t), 1);
    if (!config) {
        /*WARN("Out of memory.");*/
        fprintf(stderr, "Out of MEM....");
        return QStringList() << input;
    }

    /* Set the defaults. */
    config->loop = 0;
    config->offset = 0;
    config->background = 0;
    config->pidfile = NULL;
    config->logfile = NULL;
    config->gmt = 0;
    config->start = 0;
    config->device = strdup(input.toUtf8().constData());
    config->input = NULL;
    config->tuner = 0;
    config->frequency = 0;
    config->delay = 0;
    config->use_read = 0;
    config->list = 0;
    if (m_width > 0) config->width = m_width;
    else config->width = 384;
    if (m_height > 0) config->height = m_height;
    else config->height = 288;
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
    uint width = 0;
    uint height = 0;
    char *pixelformatdescription;
    QString deviceName(src_query(&v4lsrc, source, &width, &height, &pixelformatdescription));
    free(config);
    QStringList result;
    result << (deviceName.isEmpty() ? input : deviceName) << (width == 0 ? QString() : QString("%1x%2").arg(width).arg(height)) << QString(pixelformatdescription);
    return result;
}

void V4lCaptureHandler::setDevice(const QString input, QString size)
{
    m_device = input;
    if (!size.isEmpty()) {
        m_width = size.section('x', 0, 0).toInt();
        m_height = size.section('x', -1).toInt();

    }
}

void V4lCaptureHandler::startPreview(int /*deviceId*/, int /*captureMode*/, bool)
{
    m_display->setHidden(false);
    fswebcam_config_t *config;
    /* Prepare the configuration structure. */
    config = (fswebcam_config_t *) calloc(sizeof(fswebcam_config_t), 1);
    if (!config) {
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
    config->device = strdup(m_device.toUtf8().constData());
    config->input = NULL;
    config->tuner = 0;
    config->frequency = 0;
    config->delay = 0;
    config->use_read = 0;
    config->list = 0;
    if (m_width > 0) config->width = m_width;
    else config->width = KdenliveSettings::video4size().section("x", 0, 0).toInt();/*384;*/
    if (m_height > 0) config->height = m_height;
    else config->height = KdenliveSettings::video4size().section("x", -1).toInt();/*288;*/
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

    if (src_open(&v4lsrc, source) != 0) return;
    m_update = true;
    free(config);
    QTimer::singleShot(200, this, SLOT(slotUpdate()));
}

V4lCaptureHandler::~V4lCaptureHandler()
{
    stopCapture();
}

void V4lCaptureHandler::slotUpdate()
{
    if (!m_update) return;
    src_grab(&v4lsrc);
    QImage qimg(v4lsrc.width, v4lsrc.height, QImage::Format_RGB888);
    switch (v4lsrc.palette) {
    case SRC_PAL_PNG:
        qimg = add_image_png(&v4lsrc);
        break;
    case SRC_PAL_JPEG:
    case SRC_PAL_MJPEG:
        qimg = add_image_jpeg(&v4lsrc);
        break;
    case SRC_PAL_S561:
        fswc_add_image_s561(qimg.bits(), (uchar *)v4lsrc.img, v4lsrc.length, v4lsrc.width, v4lsrc.height, v4lsrc.palette);
        break;
    case SRC_PAL_RGB32:
        fswc_add_image_rgb32(&v4lsrc, qimg.bits());
        break;
    case SRC_PAL_BGR32:
        fswc_add_image_bgr32(&v4lsrc, qimg.bits());
        break;
    case SRC_PAL_RGB24:
        fswc_add_image_rgb24(&v4lsrc, qimg.bits());
        break;
    case SRC_PAL_BGR24:
        fswc_add_image_bgr24(&v4lsrc, qimg.bits());
        break;
    case SRC_PAL_BAYER:
    case SRC_PAL_SGBRG8:
    case SRC_PAL_SGRBG8:
        fswc_add_image_bayer(qimg.bits(), (uchar *)v4lsrc.img, v4lsrc.length, v4lsrc.width, v4lsrc.height, v4lsrc.palette);
        break;
    case SRC_PAL_YUYV:
    case SRC_PAL_UYVY:
        fswc_add_image_yuyv(&v4lsrc, (avgbmp_t *)qimg.bits());
        break;
    case SRC_PAL_YUV420P:
        fswc_add_image_yuv420p(&v4lsrc, qimg.bits());
        break;
    case SRC_PAL_NV12MB:
        fswc_add_image_nv12mb(&v4lsrc, qimg.bits());
        break;
    case SRC_PAL_RGB565:
        fswc_add_image_rgb565(&v4lsrc, qimg.bits());
        break;
    case SRC_PAL_RGB555:
        fswc_add_image_rgb555(&v4lsrc, qimg.bits());
        break;
    case SRC_PAL_Y16:
        fswc_add_image_y16(&v4lsrc, qimg.bits());
        break;
    case SRC_PAL_GREY:
        fswc_add_image_grey(&v4lsrc, qimg.bits());
        break;
    }

    if (!m_captureFramePath.isEmpty()) {
        qimg.save(m_captureFramePath);
        emit frameSaved(m_captureFramePath);
        m_captureFramePath.clear();
    }
    if (!m_overlayImage.isNull()) {
        // overlay image
        QPainter p(&qimg);
        p.setOpacity(0.5);
        p.drawImage(0, 0, m_overlayImage);
        p.end();
    }
    m_display->setImage(qimg);
    if (m_update) QTimer::singleShot(200, this, SLOT(slotUpdate()));
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

void V4lCaptureHandler::showOverlay(QImage img, bool /*transparent*/)
{
    m_overlayImage = img;
}

void V4lCaptureHandler::hideOverlay()
{
    m_overlayImage = QImage();
}

void V4lCaptureHandler::hidePreview(bool hide)
{
    m_display->setHidden(hide);
}

void V4lCaptureHandler::stopPreview()
{
    m_display->setHidden(true);
    if (!m_update) return;
    m_update = false;
    src_close(&v4lsrc);
}


