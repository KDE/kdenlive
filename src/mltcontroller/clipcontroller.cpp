/*
Copyright (C) 2012  Till Theato <root@ttill.de>
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "clipcontroller.h"
#include "bincontroller.h"

#include <QUrl>
#include <QDebug>
#include <QPixmap>
#include <KLocalizedString>


ClipController::ClipController(BinController *bincontroller, Mlt::Producer& producer) : QObject()
    , m_binController(bincontroller)
{
    m_masterProducer = &producer;
    if (!m_masterProducer->is_valid()) qDebug()<<"// WARNING, USING INVALID PRODUCER";
    else {
        append(m_masterProducer);
        m_url = QUrl::fromLocalFile(m_masterProducer->get("resource"));
        if (m_url.isValid()) {
            m_name = m_url.fileName();
        }
        m_service = m_masterProducer->get("mlt_service");
    }
}

ClipController::ClipController(BinController *bincontroller) : QObject()
    , m_binController(bincontroller)
{
    m_masterProducer = NULL;
}

ClipController::~ClipController()
{
}

void ClipController::addMasterProducer(Mlt::Producer &producer)
{
    m_masterProducer = &producer;
    if (!m_masterProducer->is_valid()) qDebug()<<"// WARNING, USING INVALID PRODUCER";
    else {
        insert(0, m_masterProducer);
        m_url = QUrl::fromLocalFile(m_masterProducer->get("resource"));
        if (m_url.isValid()) {
            m_name = m_url.fileName();
        }
        m_service = m_masterProducer->get("mlt_service");
    }
}

Mlt::Producer &ClipController::originalProducer()
{
    return *m_masterProducer;
}

Mlt::Producer *ClipController::masterProducer()
{
    return new Mlt::Producer(*m_masterProducer);
}

bool ClipController::isValid()
{
    if (m_masterProducer == NULL) return false;
    return m_masterProducer->is_valid();
}

const QString &ClipController::clipId()
{
    if (m_masterProducer == NULL) return QString();
    return property("id");
}

void ClipController::updateProducer(Mlt::Producer* producer)
{
    //TODO replace all track producers
    removeAll(m_masterProducer);
    delete m_masterProducer;
    m_masterProducer = producer;
    if (!m_masterProducer->is_valid()) qDebug()<<"// WARNING, USING INVALID PRODUCER";
    else {
        append(m_masterProducer);
        // URL and name shoule not be updated otherwise when proxying a clip we cannot find back the original url
        /*m_url = QUrl::fromLocalFile(m_masterProducer->get("resource"));
        if (m_url.isValid()) {
            m_name = m_url.fileName();
        }
        */
    }
}

Mlt::Producer *ClipController::getTrackProducer(const QString &id, int track, int clipState, double speed)
{
    QString clipWithTrackId = id;
    if (track == -1) {
        return m_masterProducer;
    }
    clipWithTrackId.append("_" + QString::number(track));
    
    //TODO handle audio / video only producers and framebuffer
    if (clipState == PlaylistState::AudioOnly) clipWithTrackId.append("_audio");
    else if (clipState == PlaylistState::VideoOnly) clipWithTrackId.append("_video");

    if (size() > track) {
        Mlt::Producer *trackProducer = at(track);
        if (trackProducer) return trackProducer;
    } else {
        while (size() < track) {
            append(NULL);
        }
    }

    Mlt::Producer *clone = m_binController->cloneProducer(*m_masterProducer);
    clone->set("id", clipWithTrackId.toUtf8().constData());
    insert(track, clone);
    m_binController->replaceBinPlaylistClip(clipWithTrackId, clone->parent());
    return clone;
}

void ClipController::appendTrackProducer(int track, Mlt::Producer &producer)
{
    while (size() < track) {
            append(NULL);
    }
    insert(track, &producer);
}

const QString ClipController::getStringDuration()
{
    if (m_masterProducer) return m_masterProducer->get_length_time(mlt_time_smpte_df);
    return QString(i18n("Unknown"));
}

int ClipController::getPlaytime() const
{
    return m_masterProducer->get_playtime();
}

QString ClipController::property(const QString &name)
{
    return QString(m_masterProducer->parent().get(name.toUtf8().constData()));
}

QUrl ClipController::clipUrl() const
{
    return m_url;
}

QString ClipController::clipName() const
{
    return m_name;
}

QString ClipController::serviceName() const
{
    return m_service;
}

void ClipController::setProperty(const QString& name, int value)
{
    //TODO: also set property on all track producers
    m_masterProducer->parent().set(name.toUtf8().constData(), value);
}

void ClipController::setProperty(const QString& name, double value)
{
    //TODO: also set property on all track producers
    m_masterProducer->parent().set(name.toUtf8().constData(), value);
}

void ClipController::setProperty(const QString& name, const QString& value)
{
    //TODO: also set property on all track producers
    m_masterProducer->parent().set(name.toUtf8().constData(), value.toUtf8().constData());
}

ClipType ClipController::clipType() const
{
    if (!m_masterProducer || !m_masterProducer->is_valid()) return Unknown;
    if (m_service == "avformat" || m_service == "avformat-novalidate") {
        return AV;
    }
    if (m_service == "qimage" || m_service == "pixbuf") {
        return Image;
    }
    if (m_service == "color") {
        return Color;
    }
    return Unknown;
}


QPixmap ClipController::pixmap(int framePosition, int width, int height)
{
    //int currentPosition = position();
    return QPixmap();
    m_masterProducer->seek(framePosition);
    Mlt::Frame *frame = m_masterProducer->get_frame();
    if (frame == NULL || !frame->is_valid()) {
        QPixmap p(width, height);
        p.fill(QColor(Qt::red).rgb());
        return p;
    }

    frame->set("rescale.interp", "bilinear");
    frame->set("deinterlace_method", "onefield");
    frame->set("top_field_first", -1);

    if (width == 0) {
        width = m_masterProducer->get_int("meta.media.width");
        if (width == 0) {
            width = m_masterProducer->get_int("width");
        }
    }
    if (height == 0) {
        height = m_masterProducer->get_int("meta.media.height");
        if (height == 0) {
            height = m_masterProducer->get_int("height");
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
