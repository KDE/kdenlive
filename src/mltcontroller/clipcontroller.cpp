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
    , m_snapMarkers(QList < CommentedTime >())
    , m_hasLimitedDuration(true)
    , m_properties(new Mlt::Properties(producer.get_properties()))
{
    m_masterProducer = &producer;
    if (!m_masterProducer->is_valid()) qDebug()<<"// WARNING, USING INVALID PRODUCER";
    else {
        QString proxy = m_properties->get("kdenlive:proxy");
        if (proxy.length() > 2) {
            // This is a proxy producer, read original url from kdenlive property
            m_url = QUrl::fromLocalFile(m_properties->get("kdenlive:originalurl"));
        }
        else m_url = QUrl::fromLocalFile(m_properties->get("resource"));
        m_service = m_properties->get("mlt_service");
        getInfoForProducer();
    }
}

ClipController::ClipController(BinController *bincontroller) : QObject()
    , m_binController(bincontroller)
    , m_snapMarkers(QList < CommentedTime >())
    , m_hasLimitedDuration(true)
    , m_clipType(Unknown)
    , m_properties(NULL)
{
    m_masterProducer = NULL;
}

ClipController::~ClipController()
{
}

double ClipController::dar() const
{
    return m_binController->dar();
}

void ClipController::addMasterProducer(Mlt::Producer &producer)
{
    m_properties = new Mlt::Properties(producer.get_properties());
    m_masterProducer = &producer;
    if (!m_masterProducer->is_valid()) qDebug()<<"// WARNING, USING INVALID PRODUCER";
    else {
        QString proxy = m_properties->get("kdenlive:proxy");
        if (proxy.length() > 2) {
            // This is a proxy producer, read original url from kdenlive property
            m_url = QUrl::fromLocalFile(m_properties->get("kdenlive:originalurl"));
        }
        else m_url = QUrl::fromLocalFile(m_properties->get("resource"));
        m_service = m_properties->get("mlt_service");
        getInfoForProducer();
    }
}

void ClipController::getProducerXML(QDomDocument& document)
{
    if (m_masterProducer) {
        QString xml = m_binController->getProducerXML(*m_masterProducer);
        document.setContent(xml);
    }
    else qDebug()<<" + + ++ NO MASTER PROD";
}

void ClipController::getInfoForProducer()
{
    m_audioIndex = int_property("audio_index");
    m_videoIndex = int_property("video_index");
    if (m_service == "avformat" || m_service == "avformat-novalidate") {
        if (m_audioIndex == -1) {
            m_clipType = Video;
        }
        else if (m_videoIndex == -1) {
            m_clipType = Audio;
        }
        else {
            m_clipType = AV;
        }
    }
    else if (m_service == "qimage" || m_service == "pixbuf") {
        m_clipType = Image;
        m_hasLimitedDuration = false;
    }
    else if (m_service == "colour" || m_service == "color") {
        m_clipType = Color;
        m_hasLimitedDuration = false;
    }
    else if (m_service == "kdenlivetitle") {
        m_clipType = Text;
        m_hasLimitedDuration = false;
    }
    else if (m_service == "mlt") {
        m_clipType = Playlist;
    }
    else if (m_service == "webvfx") {
        m_clipType = WebVfx;
    }
    else m_clipType = Unknown;
}

bool ClipController::hasLimitedDuration() const
{
    return m_hasLimitedDuration;
}

Mlt::Producer &ClipController::originalProducer()
{
    return *m_masterProducer;
}

Mlt::Producer *ClipController::masterProducer()
{
    return new Mlt::Producer(*m_masterProducer);
}

Mlt::Producer *ClipController::zoneProducer(int in, int out)
{
    return m_masterProducer->cut(in, out);
}

bool ClipController::isValid()
{
    if (m_masterProducer == NULL) return false;
    return m_masterProducer->is_valid();
}

const QString ClipController::clipId()
{
    if (m_masterProducer == NULL) return QString();
    return property("id");
}

const char *ClipController::getPassPropertiesList() const
{
    return "kdenlive:proxy,kdenlive:originalurl,force_aspect_ratio,force_aspect_num,force_aspect_den,force_aspect_ratio,force_fps,force_progressive,force_tff,threads,force_colorspace,set.force_full_luma,templatetext,file_hash";
}

QMap <QString, QString> ClipController::getSubClips()
{
    Mlt::Properties subProperties;
    subProperties.pass_values(*m_properties, "kdenlive:clipzone.");
    QMap <QString,QString> subclipsData;
    for (int i = 0; i < subProperties.count(); i++) {
        subclipsData.insert(subProperties.get_name(i), subProperties.get(i));
    }
    return subclipsData;
}

void ClipController::updateProducer(const QString &id, Mlt::Producer* producer)
{
    //TODO replace all track producers
    Mlt::Properties passProperties;
    // Keep track of necessary properties
    passProperties.pass_list(*m_properties, getPassPropertiesList());
    delete m_properties;
    delete m_masterProducer;
    m_masterProducer = producer;
    m_properties = new Mlt::Properties(producer->get_properties());
    // Pass properties from previous producer
    m_properties->pass_list(passProperties, getPassPropertiesList());
    if (!m_masterProducer->is_valid()) qDebug()<<"// WARNING, USING INVALID PRODUCER";
    else {
        // URL and name shoule not be updated otherwise when proxying a clip we cannot find back the original url
        /*m_url = QUrl::fromLocalFile(m_masterProducer->get("resource"));
        if (m_url.isValid()) {
            m_name = m_url.fileName();
        }
        */
    }
}


Mlt::Producer *ClipController::getTrackProducer(const QString trackName, PlaylistState::ClipState clipState, double speed)
{
    if (trackName.isEmpty()) {
        return m_masterProducer;
    }
    if  (m_clipType != AV && m_clipType != Audio && m_clipType != Playlist) {
        // Only producers with audio need a different producer for each track (or we have an audio crackle bug)
        return new Mlt::Producer(m_masterProducer->parent());
    }
    QString clipWithTrackId = clipId();
    clipWithTrackId.append("_" + trackName);
    
    //TODO handle audio / video only producers and framebuffer
    if (clipState == PlaylistState::AudioOnly) clipWithTrackId.append("_audio");
    else if (clipState == PlaylistState::VideoOnly) clipWithTrackId.append("_video");
    
    Mlt::Producer *clone = m_binController->cloneProducer(*m_masterProducer);
    clone->set("id", clipWithTrackId.toUtf8().constData());
    //m_binController->replaceBinPlaylistClip(clipWithTrackId, clone->parent());
    return clone;
}

void ClipController::appendTrackProducer(const QString trackName, Mlt::Producer &producer)
{
    //insert(trackName, &producer);
}

const QString ClipController::getStringDuration()
{
    if (m_masterProducer) return m_masterProducer->get_length_time(mlt_time_smpte);
    return QString(i18n("Unknown"));
}

GenTime ClipController::getPlaytime() const
{
    return GenTime(m_masterProducer->get_playtime(), m_binController->fps());
}

QString ClipController::property(const QString &name) const
{
    if (!m_properties) return QString();
    return QString(m_properties->get(name.toUtf8().constData()));
}

int ClipController::int_property(const QString &name) const
{
    if (!m_properties) return 0;
    return m_properties->get_int(name.toUtf8().constData());
}

double ClipController::double_property(const QString &name) const
{
    if (!m_properties) return 0;
    return m_properties->get_double(name.toUtf8().constData());
}

QColor ClipController::color_property(const QString &name) const
{
    if (!m_properties) return QColor();
    mlt_color color = m_properties->get_color(name.toUtf8().constData());
    return QColor::fromRgb(color.r, color.g, color.b);
}

double ClipController::originalFps() const
{
    if (!m_properties) return 0;
    QString propertyName = QString("meta.media.%1.stream.frame_rate").arg(m_videoIndex);
    return m_properties->get_double(propertyName.toUtf8().constData());
}

const QString ClipController::codec(bool audioCodec) const
{
    if (!m_properties || (m_clipType!= AV && m_clipType != Video && m_clipType != Audio)) return QString();
    QString propertyName = QString("meta.media.%1.codec.name").arg(audioCodec ? m_audioIndex : m_videoIndex);
    return m_properties->get(propertyName.toUtf8().constData());
}

QSize ClipController::originalFrameSize() const
{
    if (!m_properties) return QSize();
    QString propertyName = QString("meta.media.%1.codec.width").arg(m_videoIndex);
    int x = m_properties->get_int(propertyName.toUtf8().constData());
    propertyName = QString("meta.media.%1.codec.height").arg(m_videoIndex);
    int y = m_properties->get_int(propertyName.toUtf8().constData());
    return QSize(x, y);
}

QUrl ClipController::clipUrl() const
{
    return m_url;
}

QString ClipController::clipName() const
{
    QString name = property("kdenlive:clipname");
    if (!name.isEmpty()) return name;
    return m_url.fileName();
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

void ClipController::resetProperty(const QString& name)
{
    //TODO: also set property on all track producers
    m_masterProducer->parent().set(name.toUtf8().constData(), (char *)NULL);
}

ClipType ClipController::clipType() const
{
    return m_clipType;
}


QPixmap ClipController::pixmap(int framePosition, int width, int height)
{
    //int currentPosition = position();
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
}

QList < GenTime > ClipController::snapMarkers() const
{
    QList < GenTime > markers;
    for (int count = 0; count < m_snapMarkers.count(); ++count) {
        markers.append(m_snapMarkers.at(count).time());
    }

    return markers;
}

QList < CommentedTime > ClipController::commentedSnapMarkers() const
{
    return m_snapMarkers;
}

void ClipController::loadSnapMarker(const QString &seconds, const QString &hash)
{
    QLocale locale;
    GenTime markerTime(locale.toDouble(seconds));
    CommentedTime marker(hash, markerTime);
    QList < CommentedTime >::Iterator it = m_snapMarkers.begin();
    for (it = m_snapMarkers.begin(); it != m_snapMarkers.end(); ++it) {
        if ((*it).time() >= marker.time())
            break;
    }
    if ((it != m_snapMarkers.end()) && ((*it).time() == marker.time())) {
        (*it).setComment(marker.comment());
        (*it).setMarkerType(marker.markerType());
        //qCritical() << "trying to add Snap Marker that already exists, this will cause inconsistancies with undo/redo";
    } else {
        m_snapMarkers.insert(it, marker);
    }
}

void ClipController::addSnapMarker(const CommentedTime &marker)
{
    QList < CommentedTime >::Iterator it = m_snapMarkers.begin();
    for (it = m_snapMarkers.begin(); it != m_snapMarkers.end(); ++it) {
        if ((*it).time() >= marker.time())
            break;
    }
    QLocale locale;
    QString markerId = clipId() + ":" + locale.toString(marker.time().seconds());
    if ((it != m_snapMarkers.end()) && ((*it).time() == marker.time())) {
        (*it).setComment(marker.comment());
        (*it).setMarkerType(marker.markerType());
        m_binController->storeMarker(markerId, marker.hash());
        //qCritical() << "trying to add Snap Marker that already exists, this will cause inconsistancies with undo/redo";
    } else {
        m_snapMarkers.insert(it, marker);
        m_binController->storeMarker(markerId, marker.hash());
    }
}

void ClipController::editSnapMarker(const GenTime & time, const QString &comment)
{
    QList < CommentedTime >::Iterator it;
    for (it = m_snapMarkers.begin(); it != m_snapMarkers.end(); ++it) {
        if ((*it).time() == time)
            break;
    }
    QLocale locale;
    QString markerId = clipId() + ":" + locale.toString(time.seconds());
    if (it != m_snapMarkers.end()) {
        (*it).setComment(comment);
        m_binController->storeMarker(markerId, (*it).hash());
    } else {
        qCritical() << "trying to edit Snap Marker that does not already exists";
    }
}

QString ClipController::deleteSnapMarker(const GenTime & time)
{
    QString result = i18n("Marker");
    QList < CommentedTime >::Iterator itt = m_snapMarkers.begin();

    while (itt != m_snapMarkers.end()) {
        if ((*itt).time() == time)
            break;
        ++itt;
    }
    QLocale locale;
    QString markerId = clipId() + ":" + locale.toString(time.seconds());
    if ((itt != m_snapMarkers.end()) && ((*itt).time() == time)) {
        result = (*itt).comment();
        m_snapMarkers.erase(itt);
        m_binController->storeMarker(markerId, QString());
    }
    return result;
}


GenTime ClipController::findPreviousSnapMarker(const GenTime & currTime)
{
    int it;
    for (it = 0; it < m_snapMarkers.count(); ++it) {
        if (m_snapMarkers.at(it).time() >= currTime)
            break;
    }
    if (it == 0) return GenTime();
    else if (it == m_snapMarkers.count() - 1 && m_snapMarkers.at(it).time() < currTime)
        return m_snapMarkers.at(it).time();
    else return m_snapMarkers.at(it - 1).time();
}

GenTime ClipController::findNextSnapMarker(const GenTime & currTime)
{
    int it;
    for (it = 0; it < m_snapMarkers.count(); ++it) {
        if (m_snapMarkers.at(it).time() > currTime)
            break;
    }
    if (it < m_snapMarkers.count() && m_snapMarkers.at(it).time() > currTime) return m_snapMarkers.at(it).time();
    return getPlaytime();
}

QString ClipController::markerComment(const GenTime &t) const
{
    QList < CommentedTime >::ConstIterator itt = m_snapMarkers.begin();
    while (itt != m_snapMarkers.end()) {
        if ((*itt).time() == t)
            return (*itt).comment();
        ++itt;
    }
    return QString();
}

CommentedTime ClipController::markerAt(const GenTime &t) const
{
    QList < CommentedTime >::ConstIterator itt = m_snapMarkers.begin();
    while (itt != m_snapMarkers.end()) {
        if ((*itt).time() == t)
            return (*itt);
        ++itt;
    }
    return CommentedTime();
}

void ClipController::setZone(const QPoint &zone)
{
    setProperty("kdenlive:zone_in", zone.x());
    setProperty("kdenlive:zone_out", zone.y());
}

QPoint ClipController::zone() const
{
    int in = int_property("kdenlive:zone_in");
    int out = int_property("kdenlive:zone_out");
    if (out <= in ) out = in + 50;
    QPoint zone(in, out);
    return zone;
}

const QString ClipController::getClipHash() const
{
    return property("kdenlive:file_hash");
}

Mlt::Properties &ClipController::properties()
{
    return *m_properties;
}

