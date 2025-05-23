/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "kthumb.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "profiles/profilemodel.hpp"

#include <mlt++/Mlt.h>

#include <QPixmap>
// static
QPixmap KThumb::getImage(const QUrl &url, int width, int height)
{
    if (!url.isValid()) {
        return QPixmap();
    }
    return getImage(url, 0, width, height);
}

QPixmap KThumb::getImageWithParams(const QUrl &url, QMap<QString, QString> params, int width, int height)
{
    if (!url.isValid()) {
        return QPixmap();
    }
    return getImage(url, 0, width, height, params);
}

// static
QPixmap KThumb::getImage(const QUrl &url, int frame, int width, int height, QMap<QString, QString> params)
{
    QScopedPointer<Mlt::Profile> profile(new Mlt::Profile(pCore->getCurrentProfilePath().toUtf8().constData()));
    if (height == -1) {
        height = int(width * double(profile->height()) / profile->width());
    }
    QPixmap pix(width, height);
    if (!url.isValid()) {
        return pix;
    }
    Mlt::Producer *producer = new Mlt::Producer(*(profile.data()), url.toLocalFile().toUtf8().constData());
    if (producer->is_valid()) {
        for (auto i = params.cbegin(), end = params.cend(); i != end; ++i) {
            producer->set(i.key().toUtf8().constData(), i.value().toUtf8().constData());
        }
    }
    if (KdenliveSettings::gpu_accel()) {
        QString service = producer->get("mlt_service");
        QString res = producer->get("resource");
        delete producer;
        producer = new Mlt::Producer(*(profile.data()), service.toUtf8().constData(), res.toUtf8().constData());
        Mlt::Filter scaler(*(profile.data()), "swscale");
        Mlt::Filter converter(*(profile.data()), "avcolor_space");
        producer->attach(scaler);
        producer->attach(converter);
    }
    pix = QPixmap::fromImage(getFrame(producer, frame, width, height));
    delete producer;
    return pix;
}

// static
QImage KThumb::getFrame(Mlt::Producer *producer, int framepos, int width, int height, int displayWidth)
{
    if (producer == nullptr || !producer->is_valid()) {
        QImage p(displayWidth == 0 ? width : displayWidth, height, QImage::Format_ARGB32_Premultiplied);
        p.fill(QColor(Qt::red).rgb());
        return p;
    }
    if (producer->is_blank()) {
        QImage p(displayWidth == 0 ? width : displayWidth, height, QImage::Format_ARGB32_Premultiplied);
        p.fill(QColor(Qt::black).rgb());
        return p;
    }

    producer->seek(framepos);
    Mlt::Frame *frame = producer->get_frame();
    const QImage p = getFrame(frame, width, height, displayWidth);
    delete frame;
    return p;
}

// static
QImage KThumb::getFrame(Mlt::Producer &producer, int framepos, int width, int height, int displayWidth)
{
    if (!producer.is_valid()) {
        QImage p(displayWidth, height, QImage::Format_ARGB32_Premultiplied);
        p.fill(QColor(Qt::red).rgb());
        return p;
    }
    producer.seek(framepos);
    Mlt::Frame *frame = producer.get_frame();
    const QImage p = getFrame(frame, width, height, displayWidth);
    delete frame;
    return p;
}

// static
QImage KThumb::getFrame(Mlt::Frame *frame, int width, int height, int scaledWidth)
{
    if (frame == nullptr || !frame->is_valid()) {
        qDebug() << "* * * *INVALID FRAME";
        return QImage();
    }
    int ow = width;
    int oh = height;
    mlt_image_format format = mlt_image_rgba;
    const uchar *imagedata = frame->get_image(format, ow, oh);
    if (imagedata) {
        QImage temp(ow, oh, QImage::Format_ARGB32);
        memcpy(temp.scanLine(0), imagedata, unsigned(ow * oh * 4));
        if (scaledWidth == 0 || scaledWidth == width) {
            return temp.rgbSwapped();
        }
        return temp.rgbSwapped().scaled(scaledWidth, height == 0 ? oh : height);
    }
    return QImage();
}

// static
int KThumb::imageVariance(const QImage &image)
{
    int delta = 0;
    int avg = 0;
    int bytes = int(image.sizeInBytes());
    int STEPS = bytes / 2;
    QVarLengthArray<uchar> pivot(STEPS);
    const uchar *bits = image.bits();
    // First pass: get pivots and taking average
    for (int i = 0; i < STEPS; ++i) {
        pivot[i] = bits[2 * i];
        avg += pivot.at(i);
    }
    if (STEPS != 0) {
        avg = avg / STEPS;
    }
    // Second Step: calculate delta (average?)
    for (int i = 0; i < STEPS; ++i) {
        int curdelta = abs(int(avg - pivot.at(i)));
        delta += curdelta;
    }
    if (STEPS != 0) {
        return delta / STEPS;
    }
    return 0;
}
