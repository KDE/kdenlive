/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "producerwrapper.h"
#include <KUrl>
#include <KDebug>
#include <QPixmap>


ProducerWrapper::ProducerWrapper(Mlt::Producer* producer) :
    Mlt::Producer(producer)
{
    if (!is_valid()) kDebug()<<"// WARNING, USING INVALID PRODUCER";
}

ProducerWrapper::ProducerWrapper(Mlt::Profile& profile, const QString&input, const QString &service) :
    Mlt::Producer(profile, service == QString() ? NULL : service.toUtf8().constData(), input.toUtf8().constData())
  , m_resource(input)
  , m_service(service)
{
    if (!is_valid()) kDebug()<<"// WARNING, USING INVALID PRODUCER 2";
}

ProducerWrapper::~ProducerWrapper()
{
}
QString ProducerWrapper::property(const QString &name)
{
    return QString(get(name.toUtf8().constData()));
}

QString ProducerWrapper::resourceName() const
{
    return m_resource;
}

QString ProducerWrapper::serviceName() const
{
    return m_service;
}

void ProducerWrapper::setProperty(const QString& name, const QString& value)
{
    set(name.toUtf8().constData(), value.toUtf8().constData());
}

QPixmap ProducerWrapper::pixmap(int framePosition, int width, int height)
{
    //int currentPosition = position();
    return QPixmap();
    seek(framePosition);
    Mlt::Frame *frame = get_frame();
    if (frame == NULL || !frame->is_valid()) {
        QPixmap p(width, height);
        p.fill(QColor(Qt::red).rgb());
        return p;
    }

    frame->set("rescale.interp", "bilinear");
    frame->set("deinterlace_method", "onefield");
    frame->set("top_field_first", -1);

    if (width == 0) {
        width = get_int("meta.media.width");
        if (width == 0) {
            width = get_int("width");
        }
    }
    if (height == 0) {
        height = get_int("meta.media.height");
        if (height == 0) {
            height = get_int("height");
        }
    }
    
    //     int ow = frameWidth;
    //     int oh = height;
    mlt_image_format format = mlt_image_rgb24a;

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    const uchar* imagedata = frame->get_image(format, width, height);
    if (imagedata) {
        QImage temp(width, height, QImage::Format_ARGB32_Premultiplied);
        memcpy(temp.bits(), imagedata, width * height * 4);
        image = temp.rgbSwapped();
    }
    else image.fill(QColor(Qt::red).rgb());
    delete frame;


    QPixmap pixmap;;
    pixmap.convertFromImage(image);

    return pixmap;

    //const uchar* imagedata = frame->get_image(format, ow, oh);
    //QImage image(imagedata, ow, oh, QImage::Format_ARGB32_Premultiplied);
    
    /*if (!image.isNull()) {
        if (ow > (2 * displayWidth)) {
            // there was a scaling problem, do it manually
            image = image.scaled(displayWidth, height).rgbSwapped();
        } else {
            image = image.scaled(displayWidth, height, Qt::IgnoreAspectRatio).rgbSwapped();
        }
        p.fill(QColor(Qt::black).rgb());
        QPainter painter(&p);
        painter.drawImage(p.rect(), image);
        painter.end();
    } else
        p.fill(QColor(Qt::red).rgb());
    return p;*/
}
