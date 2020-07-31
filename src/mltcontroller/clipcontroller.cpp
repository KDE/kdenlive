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
#include "bin/model/markerlistmodel.hpp"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "kdenlivesettings.h"
#include "lib/audio/audioStreamInfo.h"
#include "profiles/profilemodel.hpp"
#include "bin/clipcreator.hpp"
#include "doc/kthumb.h"

#include "core.h"
#include "kdenlive_debug.h"
#include <KLocalizedString>
#include <QFileInfo>
#include <QPixmap>

std::shared_ptr<Mlt::Producer> ClipController::mediaUnavailable;

ClipController::ClipController(const QString &clipId, const std::shared_ptr<Mlt::Producer> &producer)
    : selectedEffectIndex(1)
    , m_audioThumbCreated(false)
    , m_masterProducer(producer)
    , m_properties(producer ? new Mlt::Properties(producer->get_properties()) : nullptr)
    , m_usesProxy(false)
    , m_audioInfo(nullptr)
    , m_videoIndex(0)
    , m_clipType(ClipType::Unknown)
    , m_hasLimitedDuration(true)
    , m_effectStack(producer ? EffectStackModel::construct(producer, {ObjectType::BinClip, clipId.toInt()}, pCore->undoStack()) : nullptr)
    , m_hasAudio(false)
    , m_hasVideo(false)
    , m_thumbsProducer(nullptr)
    , m_producerLock(QReadWriteLock::Recursive)
    , m_controllerBinId(clipId)
{
    if (m_masterProducer && !m_masterProducer->is_valid()) {
        qCDebug(KDENLIVE_LOG) << "// WARNING, USING INVALID PRODUCER";
        return;
    }
    if (m_properties) {
        setProducerProperty(QStringLiteral("kdenlive:id"), m_controllerBinId);
        getInfoForProducer();
        checkAudioVideo();
    } else {
        m_producerLock.lockForWrite();
    }
}

ClipController::~ClipController()
{
    delete m_properties;
    m_masterProducer.reset();
}

const QString ClipController::binId() const
{
    return m_controllerBinId;
}

const std::unique_ptr<AudioStreamInfo> &ClipController::audioInfo() const
{
    return m_audioInfo;
}

void ClipController::addMasterProducer(const std::shared_ptr<Mlt::Producer> &producer)
{
    qDebug() << "################### ClipController::addmasterproducer";
    m_masterProducer = producer;
    m_properties = new Mlt::Properties(m_masterProducer->get_properties());
    m_producerLock.unlock();
    // Pass temporary properties
    QMapIterator<QString, QVariant> i(m_tempProps);
    while (i.hasNext()) {
        i.next();
        switch(i.value().type()) {
            case QVariant::Int:
                setProducerProperty(i.key(), i.value().toInt());
                break;
            case QVariant::Double:
                setProducerProperty(i.key(), i.value().toDouble());
                break;
            default:
                setProducerProperty(i.key(), i.value().toString());
                break;
        }
    }
    m_tempProps.clear();
    int id = m_controllerBinId.toInt();
    m_effectStack = EffectStackModel::construct(producer, {ObjectType::BinClip, id}, pCore->undoStack());
    if (!m_masterProducer->is_valid()) {
        m_masterProducer = ClipController::mediaUnavailable;
        qCDebug(KDENLIVE_LOG) << "// WARNING, USING INVALID PRODUCER";
    } else {
        checkAudioVideo();
        setProducerProperty(QStringLiteral("kdenlive:id"), m_controllerBinId);
        getInfoForProducer();
        emitProducerChanged(m_controllerBinId, producer);
    }
    connectEffectStack();
}

namespace {
QString producerXml(const std::shared_ptr<Mlt::Producer> &producer, bool includeMeta, bool includeProfile)
{
    Mlt::Consumer c(*producer->profile(), "xml", "string");
    Mlt::Service s(producer->get_service());
    if (!s.is_valid()) {
        return QString();
    }
    int ignore = s.get_int("ignore_points");
    if (ignore != 0) {
        s.set("ignore_points", 0);
    }
    c.set("time_format", "frames");
    if (!includeMeta) {
        c.set("no_meta", 1);
    }
    if (!includeProfile) {
        c.set("no_profile", 1);
    }
    c.set("store", "kdenlive");
    c.set("no_root", 1);
    c.set("root", "/");
    c.connect(s);
    c.start();
    if (ignore != 0) {
        s.set("ignore_points", ignore);
    }
    return QString::fromUtf8(c.get("string"));
}
} // namespace

void ClipController::getProducerXML(QDomDocument &document, bool includeMeta, bool includeProfile)
{
    // TODO refac this is a probable duplicate with Clip::xml
    if (m_masterProducer) {
        QString xml = producerXml(m_masterProducer, includeMeta, includeProfile);
        document.setContent(xml);
    } else {
        if (!m_temporaryUrl.isEmpty()) {
            document = ClipCreator::getXmlFromUrl(m_temporaryUrl);
        } else if (!m_path.isEmpty()) {
            document = ClipCreator::getXmlFromUrl(m_path);
        }
        qCDebug(KDENLIVE_LOG) << " + + ++ NO MASTER PROD";
    }
}

void ClipController::getInfoForProducer()
{
    QReadLocker lock(&m_producerLock);
    m_service = m_properties->get("mlt_service");
    if (m_service == QLatin1String("qtext")) {
        // Placeholder clip, find real service
        QString originalService = m_properties->get("kdenlive:orig_service");
        if (!originalService.isEmpty()) {
            m_service = originalService;
        }
    }
    QString proxy = m_properties->get("kdenlive:proxy");
    QString path = m_properties->get("resource");
    if (proxy.length() > 2) {
        if (QFileInfo(path).isRelative()) {
            path.prepend(pCore->currentDoc()->documentRoot());
            m_properties->set("resource", path.toUtf8().constData());
        }
        // This is a proxy producer, read original url from kdenlive property
        path = m_properties->get("kdenlive:originalurl");
        if (QFileInfo(path).isRelative()) {
            path.prepend(pCore->currentDoc()->documentRoot());
        }
        m_usesProxy = true;
    } else if (m_service != QLatin1String("color") && m_service != QLatin1String("colour") && !path.isEmpty() && QFileInfo(path).isRelative() &&
                   path != QLatin1String("<producer>")) {
        path.prepend(pCore->currentDoc()->documentRoot());
        m_properties->set("resource", path.toUtf8().constData());
    }
    m_path = path.isEmpty() ? QString() : QFileInfo(path).absoluteFilePath();
    QString origurl = m_properties->get("kdenlive:originalurl");
    if (!origurl.isEmpty()) {
        m_properties->set("kdenlive:originalurl", m_path.toUtf8().constData());
    }
    date = QFileInfo(m_path).lastModified();
    m_videoIndex = -1;
    int audioIndex = -1;
    // special case: playlist with a proxy clip have to be detected separately
    if (m_usesProxy && m_path.endsWith(QStringLiteral(".mlt"))) {
        m_clipType = ClipType::Playlist;
    } else if (m_service == QLatin1String("avformat") || m_service == QLatin1String("avformat-novalidate")) {
        audioIndex = getProducerIntProperty(QStringLiteral("audio_index"));
        m_videoIndex = getProducerIntProperty(QStringLiteral("video_index"));
        if (m_videoIndex == -1) {
            m_clipType = ClipType::Audio;
        } else {
            if (audioIndex == -1) {
                m_clipType = ClipType::Video;
            } else {
                m_clipType = ClipType::AV;
            }
            if (m_service == QLatin1String("avformat")) {
                m_properties->set("mlt_service", "avformat-novalidate");
                m_properties->set("mute_on_pause", 0);
            }
        }
    } else if (m_service == QLatin1String("qimage") || m_service == QLatin1String("pixbuf")) {
        if (m_path.contains(QLatin1Char('%')) || m_path.contains(QStringLiteral("/.all."))) {
            m_clipType = ClipType::SlideShow;
            m_hasLimitedDuration = true;
        } else {
            m_clipType = ClipType::Image;
            m_hasLimitedDuration = false;
        }
    } else if (m_service == QLatin1String("colour") || m_service == QLatin1String("color")) {
        m_clipType = ClipType::Color;
        // Required for faster compositing
        m_masterProducer->set("mlt_image_format", "rgb24");
        m_hasLimitedDuration = false;
    } else if (m_service == QLatin1String("kdenlivetitle")) {
        if (!m_path.isEmpty()) {
            m_clipType = ClipType::TextTemplate;
        } else {
            m_clipType = ClipType::Text;
        }
        m_hasLimitedDuration = false;
    } else if (m_service == QLatin1String("xml") || m_service == QLatin1String("consumer")) {
        m_clipType = ClipType::Playlist;
    } else if (m_service == QLatin1String("webvfx")) {
        m_clipType = ClipType::WebVfx;
    } else if (m_service == QLatin1String("qtext")) {
        m_clipType = ClipType::QText;
    } else if (m_service == QLatin1String("qml")) {
        m_clipType = ClipType::Qml;
        m_hasLimitedDuration = false;
    } else if (m_service == QLatin1String("blipflash")) {
        // Mostly used for testing
        m_clipType = ClipType::AV;
        m_hasLimitedDuration = true;
    } else {
        m_clipType = ClipType::Unknown;
    }
    if (audioIndex > -1 || m_clipType == ClipType::Playlist) {
        m_audioInfo = std::make_unique<AudioStreamInfo>(m_masterProducer, audioIndex);
        // Load stream effects
        for (int stream : m_audioInfo->streams().keys()) {
            QString streamEffect = m_properties->get(QString("kdenlive:stream:%1").arg(stream).toUtf8().constData());
            if (!streamEffect.isEmpty()) {
                m_streamEffects.insert(stream, streamEffect.split(QChar('#')));
            }
        }
    }

    if (!m_hasLimitedDuration) {
        int playtime = m_masterProducer->time_to_frames(m_masterProducer->get("kdenlive:duration"));
        if (playtime <= 0) {
            // Fix clips having missing kdenlive:duration
            m_masterProducer->set("kdenlive:duration", m_masterProducer->frames_to_time(m_masterProducer->get_playtime(), mlt_time_clock));
            m_masterProducer->set("out", m_masterProducer->frames_to_time(m_masterProducer->get_length() - 1, mlt_time_clock));
        }
    }
}

bool ClipController::hasLimitedDuration() const
{
    return m_hasLimitedDuration;
}

void ClipController::forceLimitedDuration()
{
    m_hasLimitedDuration = true;
}

std::shared_ptr<Mlt::Producer> ClipController::originalProducer()
{
    QReadLocker lock(&m_producerLock);
    return m_masterProducer;
}

Mlt::Producer *ClipController::masterProducer()
{
    return new Mlt::Producer(*m_masterProducer);
}

bool ClipController::isValid()
{
    if (m_masterProducer == nullptr) {
        return false;
    }
    return m_masterProducer->is_valid();
}

// static
const char *ClipController::getPassPropertiesList(bool passLength)
{
    if (!passLength) {
        return "kdenlive:proxy,kdenlive:originalurl,force_aspect_num,force_aspect_den,force_aspect_ratio,force_fps,force_progressive,force_tff,threads,force_"
               "colorspace,set.force_full_luma,file_hash,autorotate,disable_exif,xmldata,video_index,audio_index,set.test_image,set.test_audio";
    }
    return "kdenlive:proxy,kdenlive:originalurl,force_aspect_num,force_aspect_den,force_aspect_ratio,force_fps,force_progressive,force_tff,threads,force_"
           "colorspace,set.force_full_luma,templatetext,file_hash,autorotate,disable_exif,xmldata,length,video_index,audio_index,set.test_image,set.test_audio";
}

QMap<QString, QString> ClipController::getPropertiesFromPrefix(const QString &prefix, bool withPrefix)
{
    QReadLocker lock(&m_producerLock);
    Mlt::Properties subProperties;
    subProperties.pass_values(*m_properties, prefix.toUtf8().constData());
    QMap<QString, QString> subclipsData;
    for (int i = 0; i < subProperties.count(); i++) {
        subclipsData.insert(withPrefix ? QString(prefix + subProperties.get_name(i)) : subProperties.get_name(i), subProperties.get(i));
    }
    return subclipsData;
}

void ClipController::updateProducer(const std::shared_ptr<Mlt::Producer> &producer)
{
    qDebug() << "################### ClipController::updateProducer";
    // TODO replace all track producers
    if (!m_properties) {
        // producer has not been initialized
        return addMasterProducer(producer);
    }
    m_producerLock.lockForWrite();
    Mlt::Properties passProperties;
    // Keep track of necessary properties
    QString proxy = producer->get("kdenlive:proxy");
    if (proxy.length() > 2) {
        // This is a proxy producer, read original url from kdenlive property
        m_usesProxy = true;
    } else {
        m_usesProxy = false;
    }
    // When resetting profile, duration can change so we invalidate it to 0 in that case
    int length = m_properties->get_int("length");
    const char *passList = getPassPropertiesList(m_usesProxy && length > 0);
    // This is necessary as some properties like set.test_audio are reset on producer creation
    passProperties.pass_list(*m_properties, passList);
    delete m_properties;
    *m_masterProducer = producer.get();
    m_properties = new Mlt::Properties(m_masterProducer->get_properties());
    m_producerLock.unlock();
    if (!m_masterProducer->is_valid()) {
        qCDebug(KDENLIVE_LOG) << "// WARNING, USING INVALID PRODUCER";
    } else {
        // Pass properties from previous producer
        m_properties->pass_list(passProperties, passList);
        checkAudioVideo();
        setProducerProperty(QStringLiteral("kdenlive:id"), m_controllerBinId);
        m_effectStack->resetService(m_masterProducer);
        emitProducerChanged(m_controllerBinId, producer);
        // URL and name should not be updated otherwise when proxying a clip we cannot find back the original url
        /*m_url = QUrl::fromLocalFile(m_masterProducer->get("resource"));
        if (m_url.isValid()) {
            m_name = m_url.fileName();
        }
        */
    }
    qDebug() << "// replace finished: " << binId() << " : " << m_masterProducer->get("resource");
}

const QString ClipController::getStringDuration()
{
    QReadLocker lock(&m_producerLock);
    if (m_masterProducer) {
        int playtime = m_masterProducer->time_to_frames(m_masterProducer->get("kdenlive:duration"));
        if (playtime > 0) {
            return QString(m_properties->frames_to_time(playtime, mlt_time_smpte_df));
        }
        return m_properties->frames_to_time(m_masterProducer->get_length(), mlt_time_smpte_df);
    }
    return i18n("Unknown");
}

int ClipController::getProducerDuration() const
{
    QReadLocker lock(&m_producerLock);
    if (m_masterProducer) {
        int playtime = m_masterProducer->time_to_frames(m_masterProducer->get("kdenlive:duration"));
        if (playtime <= 0) {
            return playtime = m_masterProducer->get_length();
        }
        return playtime;
    }
    return -1;
}

char *ClipController::framesToTime(int frames) const
{
    QReadLocker lock(&m_producerLock);
    if (m_masterProducer) {
        return m_masterProducer->frames_to_time(frames, mlt_time_clock);
    }
    return nullptr;
}

GenTime ClipController::getPlaytime() const
{
    QReadLocker lock(&m_producerLock);
    if (!m_masterProducer || !m_masterProducer->is_valid()) {
        return GenTime();
    }
    double fps = pCore->getCurrentFps();
    if (!m_hasLimitedDuration) {
        int playtime = m_masterProducer->time_to_frames(m_masterProducer->get("kdenlive:duration"));
        return GenTime(playtime == 0 ? m_masterProducer->get_playtime() : playtime, fps);
    }
    return {m_masterProducer->get_playtime(), fps};
}

int ClipController::getFramePlaytime() const
{
    QReadLocker lock(&m_producerLock);
    if (!m_masterProducer || !m_masterProducer->is_valid()) {
        return 0;
    }
    if (!m_hasLimitedDuration) {
        int playtime = m_masterProducer->time_to_frames(m_masterProducer->get("kdenlive:duration"));
        return playtime == 0 ? m_masterProducer->get_playtime() : playtime;
    }
    return m_masterProducer->get_playtime();
}

QString ClipController::getProducerProperty(const QString &name) const
{
    QReadLocker lock(&m_producerLock);
    if (m_properties == nullptr) {
        return QString();
    }
    if (m_usesProxy && name.startsWith(QLatin1String("meta."))) {
        QString correctedName = QStringLiteral("kdenlive:") + name;
        return m_properties->get(correctedName.toUtf8().constData());
    }
    return QString(m_properties->get(name.toUtf8().constData()));
}

int ClipController::getProducerIntProperty(const QString &name) const
{
    QReadLocker lock(&m_producerLock);
    if (!m_properties) {
        return 0;
    }
    if (m_usesProxy && name.startsWith(QLatin1String("meta."))) {
        QString correctedName = QStringLiteral("kdenlive:") + name;
        return m_properties->get_int(correctedName.toUtf8().constData());
    }
    return m_properties->get_int(name.toUtf8().constData());
}

qint64 ClipController::getProducerInt64Property(const QString &name) const
{
    QReadLocker lock(&m_producerLock);
    if (!m_properties) {
        return 0;
    }
    return m_properties->get_int64(name.toUtf8().constData());
}

double ClipController::getProducerDoubleProperty(const QString &name) const
{
    QReadLocker lock(&m_producerLock);
    if (!m_properties) {
        return 0;
    }
    return m_properties->get_double(name.toUtf8().constData());
}

QColor ClipController::getProducerColorProperty(const QString &name) const
{
    QReadLocker lock(&m_producerLock);
    if (!m_properties) {
        return {};
    }
    mlt_color color = m_properties->get_color(name.toUtf8().constData());
    return QColor::fromRgb(color.r, color.g, color.b);
}

QMap<QString, QString> ClipController::currentProperties(const QMap<QString, QString> &props)
{
    QMap<QString, QString> currentProps;
    QMap<QString, QString>::const_iterator i = props.constBegin();
    while (i != props.constEnd()) {
        currentProps.insert(i.key(), getProducerProperty(i.key()));
        ++i;
    }
    return currentProps;
}

double ClipController::originalFps() const
{
    QReadLocker lock(&m_producerLock);
    if (!m_properties) {
        return 0;
    }
    QString propertyName = QStringLiteral("meta.media.%1.stream.frame_rate").arg(m_videoIndex);
    return m_properties->get_double(propertyName.toUtf8().constData());
}

QString ClipController::videoCodecProperty(const QString &property) const
{
    QReadLocker lock(&m_producerLock);
    if (!m_properties) {
        return QString();
    }
    QString propertyName = QStringLiteral("meta.media.%1.codec.%2").arg(m_videoIndex).arg(property);
    return m_properties->get(propertyName.toUtf8().constData());
}

const QString ClipController::codec(bool audioCodec) const
{
    QReadLocker lock(&m_producerLock);
    if ((m_properties == nullptr) || (m_clipType != ClipType::AV && m_clipType != ClipType::Video && m_clipType != ClipType::Audio)) {
        return QString();
    }
    QString propertyName = QStringLiteral("meta.media.%1.codec.name").arg(audioCodec ? m_properties->get_int("audio_index") : m_videoIndex);
    return m_properties->get(propertyName.toUtf8().constData());
}

const QString ClipController::clipUrl() const
{
    return m_path;
}

bool ClipController::sourceExists() const
{
    if (m_clipType == ClipType::Color || m_clipType == ClipType::Text) {
        return true;
    }
    if (m_clipType == ClipType::SlideShow) {
        //TODO
        return true;
    }
    return QFile::exists(m_path);
}

QString ClipController::clipName() const
{
    QString name = getProducerProperty(QStringLiteral("kdenlive:clipname"));
    if (!name.isEmpty()) {
        return name;
    }
    return QFileInfo(m_path).fileName();
}

QString ClipController::description() const
{
    if (m_clipType == ClipType::TextTemplate) {
        QString name = getProducerProperty(QStringLiteral("templatetext"));
        return name;
    }
    QString name = getProducerProperty(QStringLiteral("kdenlive:description"));
    if (!name.isEmpty()) {
        return name;
    }
    return getProducerProperty(QStringLiteral("meta.attr.comment.markup"));
}

QString ClipController::serviceName() const
{
    return m_service;
}

void ClipController::setProducerProperty(const QString &name, int value)
{
    if (!m_masterProducer) {
        m_tempProps.insert(name, value);
        return;
    }
    QWriteLocker lock(&m_producerLock);
    m_masterProducer->parent().set(name.toUtf8().constData(), value);
}

void ClipController::setProducerProperty(const QString &name, double value)
{
    if (!m_masterProducer) {
        m_tempProps.insert(name, value);
        return;
    }
    QWriteLocker lock(&m_producerLock);
    m_masterProducer->parent().set(name.toUtf8().constData(), value);
}

void ClipController::setProducerProperty(const QString &name, const QString &value)
{
    if (!m_masterProducer) {
        m_tempProps.insert(name, value);
        return;
    }

    QWriteLocker lock(&m_producerLock);
    if (value.isEmpty()) {
        m_masterProducer->parent().set(name.toUtf8().constData(), (char *)nullptr);
    } else {
        m_masterProducer->parent().set(name.toUtf8().constData(), value.toUtf8().constData());
    }
}

void ClipController::resetProducerProperty(const QString &name)
{
    if (!m_masterProducer) {
        m_tempProps.insert(name, QString());
        return;
    }

    QWriteLocker lock(&m_producerLock);
    m_masterProducer->parent().set(name.toUtf8().constData(), (char *)nullptr);
}

ClipType::ProducerType ClipController::clipType() const
{
    return m_clipType;
}

const QSize ClipController::getFrameSize() const
{
    QReadLocker lock(&m_producerLock);
    if (m_masterProducer == nullptr) {
        return QSize();
    }
    int width = m_masterProducer->get_int("meta.media.width");
    if (width == 0) {
        width = m_masterProducer->get_int("width");
    }
    int height = m_masterProducer->get_int("meta.media.height");
    if (height == 0) {
        height = m_masterProducer->get_int("height");
    }
    return QSize(width, height);
}

bool ClipController::hasAudio() const
{
    return m_hasAudio;
}
void ClipController::checkAudioVideo()
{
    QReadLocker lock(&m_producerLock);
    m_masterProducer->seek(0);
    if (m_masterProducer->get_int("_placeholder") == 1 || m_masterProducer->get_int("_missingsource") == 1) {
        // This is a placeholder file, try to guess from its properties
        QString orig_service = m_masterProducer->get("kdenlive:orig_service");
        if (orig_service.startsWith(QStringLiteral("avformat")) || (m_masterProducer->get_int("audio_index") + m_masterProducer->get_int("video_index") > 0)) {
            m_hasAudio = m_masterProducer->get_int("audio_index") >= 0;
            m_hasVideo = m_masterProducer->get_int("video_index") >= 0;
        } else {
            // Assume image or text producer
            m_hasAudio = false;
            m_hasVideo = true;
        }
        return;
    }
    QScopedPointer<Mlt::Frame> frame(m_masterProducer->get_frame());
    if (frame->is_valid()) {
        // test_audio returns 1 if there is NO audio (strange but true at the time this code is written)
        m_hasAudio = frame->get_int("test_audio") == 0;
        m_hasVideo = frame->get_int("test_image") == 0;
        m_masterProducer->seek(0);
    } else {
        qDebug()<<"* * * *ERROR INVALID FRAME On test";
    }
}
bool ClipController::hasVideo() const
{
    return m_hasVideo;
}
PlaylistState::ClipState ClipController::defaultState() const
{
    if (hasVideo()) {
        return PlaylistState::VideoOnly;
    }
    if (hasAudio()) {
        return PlaylistState::AudioOnly;
    }
    return PlaylistState::Disabled;
}

QPixmap ClipController::pixmap(int framePosition, int width, int height)
{
    // TODO refac this should use the new thumb infrastructure
    QReadLocker lock(&m_producerLock);
    if (thumbProducer() == nullptr) {
        return QPixmap();
    }
    m_thumbsProducer->seek(framePosition);
    QScopedPointer<Mlt::Frame> frame(m_thumbsProducer->get_frame());
    if (frame == nullptr || !frame->is_valid()) {
        QPixmap p(width, height);
        p.fill(QColor(Qt::red).rgb());
        return p;
    }
    frame->set("deinterlace_method", "onefield");
    frame->set("top_field_first", -1);
    frame->set("rescale.interp", "nearest");
    QImage img = KThumb::getFrame(frame.data());
    return QPixmap::fromImage(img/*.scaled(height, width, Qt::KeepAspectRatio)*/);
}

void ClipController::setZone(const QPoint &zone)
{
    setProducerProperty(QStringLiteral("kdenlive:zone_in"), zone.x());
    setProducerProperty(QStringLiteral("kdenlive:zone_out"), zone.y());
}

QPoint ClipController::zone() const
{
    int in = getProducerIntProperty(QStringLiteral("kdenlive:zone_in"));
    int max = getFramePlaytime();
    int out = qMin(getProducerIntProperty(QStringLiteral("kdenlive:zone_out")), max);
    if (out <= in) {
        out = max;
    }
    QPoint zone(in, out);
    return zone;
}

const QString ClipController::getClipHash() const
{
    return getProducerProperty(QStringLiteral("kdenlive:file_hash"));
}

Mlt::Properties &ClipController::properties()
{
    QReadLocker lock(&m_producerLock);
    return *m_properties;
}


void ClipController::backupOriginalProperties()
{
    QReadLocker lock(&m_producerLock);
    if (m_properties->get_int("kdenlive:original.backup") == 1) {
        return;
    }
    int propsCount = m_properties->count();
    // store original props
    QStringList doNotPass {QStringLiteral("kdenlive:proxy"),QStringLiteral("kdenlive:originalurl"),QStringLiteral("kdenlive:clipname")};
    for (int j = 0; j < propsCount; j++) {
        QString propName = m_properties->get_name(j);
        if (doNotPass.contains(propName)) {
            continue;
        } 
        if (!propName.startsWith(QLatin1Char('_'))) {
            propName.prepend(QStringLiteral("kdenlive:original."));
            m_properties->set(propName.toUtf8().constData(), m_properties->get(j));
        }
    }
    m_properties->set("kdenlive:original.backup", 1);
}

void ClipController::clearBackupProperties()
{
    QReadLocker lock(&m_producerLock);
    if (m_properties->get_int("kdenlive:original.backup") == 0) {
        return;
    }
    int propsCount = m_properties->count();
    // clear original props
    QStringList passProps;
    for (int j = 0; j < propsCount; j++) {
        QString propName = m_properties->get_name(j);
        if (propName.startsWith(QLatin1String("kdenlive:original."))) {
            passProps << propName;
        }
    }
    for (const QString &p : qAsConst(passProps)) {
        m_properties->set(p.toUtf8().constData(), (char *)nullptr);
    }
    m_properties->set("kdenlive:original.backup", (char *)nullptr);
}

void ClipController::mirrorOriginalProperties(Mlt::Properties &props)
{
    QReadLocker lock(&m_producerLock);
    if (m_usesProxy && QFileInfo(m_properties->get("resource")).fileName() == QFileInfo(m_properties->get("kdenlive:proxy")).fileName()) {
        // This is a proxy, we need to use the real source properties
        if (m_properties->get_int("kdenlive:original.backup") == 0) {
            // We have a proxy clip, load original source producer
            std::shared_ptr<Mlt::Producer> prod = std::make_shared<Mlt::Producer>(pCore->getCurrentProfile()->profile(), nullptr, m_path.toUtf8().constData());
            // Get frame to make sure we retrieve all original props
            std::shared_ptr<Mlt::Frame> fr(prod->get_frame());
            if (!prod->is_valid()) {
                return;
            }
            int width = 0;
            int height = 0;
            mlt_image_format format = mlt_image_none;
            fr->get_image(format, width, height);
            Mlt::Properties sourceProps(prod->get_properties());
            props.inherit(sourceProps);
            int propsCount = sourceProps.count();
            // store original props
            QStringList doNotPass {QStringLiteral("kdenlive:proxy"),QStringLiteral("kdenlive:originalurl"),QStringLiteral("kdenlive:clipname")};
            for (int i = 0; i < propsCount; i++) {
                QString propName = sourceProps.get_name(i);
                if (doNotPass.contains(propName)) {
                    continue;
                }
                if (!propName.startsWith(QLatin1Char('_'))) {
                    propName.prepend(QStringLiteral("kdenlive:original."));
                    m_properties->set(propName.toUtf8().constData(), sourceProps.get(i));
                }
            }
            m_properties->set("kdenlive:original.backup", 1);
        }
        // Properties were fetched in the past, reuse
        Mlt::Properties sourceProps;
        sourceProps.pass_values(*m_properties, "kdenlive:original.");
        props.inherit(sourceProps);
    } else {
        if (m_clipType == ClipType::AV || m_clipType == ClipType::Video || m_clipType == ClipType::Audio) {
            // Make sure that a frame / image was fetched to initialize all meta properties
            QString progressive = m_properties->get("meta.media.progressive");
            if (progressive.isEmpty()) {
                // Fetch a frame to initialize required properties
                QScopedPointer<Mlt::Producer> tmpProd(nullptr);
                if (KdenliveSettings::gpu_accel()) {
                    QString service = m_masterProducer->get("mlt_service");
                    tmpProd.reset(new Mlt::Producer(pCore->getCurrentProfile()->profile(), service.toUtf8().constData(), m_masterProducer->get("resource")));
                }
                std::shared_ptr<Mlt::Frame> fr(tmpProd ? tmpProd->get_frame() : m_masterProducer->get_frame());
                mlt_image_format format = mlt_image_none;
                int width = 0;
                int height = 0;
                fr->get_image(format, width, height);
            }
        }
        props.inherit(*m_properties);
    }
}

void ClipController::addEffect(QDomElement &xml)
{
    Q_UNUSED(xml)
    // TODO refac: this must be rewritten
    /*
    QMutexLocker lock(&m_effectMutex);
    Mlt::Service service = m_masterProducer->parent();
    ItemInfo info;
    info.cropStart = GenTime();
    info.cropDuration = getPlaytime();
    EffectsList eff = effectList();
    EffectsController::initEffect(info, eff, getProducerProperty(QStringLiteral("kdenlive:proxy")), xml);
    // Add effect to list and setup a kdenlive_ix value
    int kdenlive_ix = 0;
    for (int i = 0; i < service.filter_count(); ++i) {
        QScopedPointer<Mlt::Filter> effect(service.filter(i));
        int ix = effect->get_int("kdenlive_ix");
        if (ix > kdenlive_ix) {
            kdenlive_ix = ix;
        }
    }
    kdenlive_ix++;
    xml.setAttribute(QStringLiteral("kdenlive_ix"), kdenlive_ix);
    EffectsParameterList params = EffectsController::getEffectArgs(xml);
    EffectManager effect(service);
    effect.addEffect(params, getPlaytime().frames(pCore->getCurrentFps()));
    if (auto ptr = m_binController.lock()) ptr->updateTrackProducer(m_controllerBinId);
    */
}

void ClipController::removeEffect(int effectIndex, bool delayRefresh)
{
    Q_UNUSED(effectIndex) Q_UNUSED(delayRefresh)
    // TODO refac: this must be rewritten
    /*
    QMutexLocker lock(&m_effectMutex);
    Mlt::Service service(m_masterProducer->parent());
    EffectManager effect(service);
    effect.removeEffect(effectIndex, true);
    if (!delayRefresh) {
        if (auto ptr = m_binController.lock()) ptr->updateTrackProducer(m_controllerBinId);
    }
    */
}

void ClipController::moveEffect(int oldPos, int newPos)
{
    Q_UNUSED(oldPos)
    Q_UNUSED(newPos)
    // TODO refac: this must be rewritten
    /*
    QMutexLocker lock(&m_effectMutex);
    Mlt::Service service(m_masterProducer->parent());
    EffectManager effect(service);
    effect.moveEffect(oldPos, newPos);
    */
}

int ClipController::effectsCount()
{
    int count = 0;
    QReadLocker lock(&m_producerLock);
    Mlt::Service service(m_masterProducer->parent());
    for (int ix = 0; ix < service.filter_count(); ++ix) {
        QScopedPointer<Mlt::Filter> effect(service.filter(ix));
        QString id = effect->get("kdenlive_id");
        if (!id.isEmpty()) {
            count++;
        }
    }
    return count;
}

void ClipController::changeEffectState(const QList<int> &indexes, bool disable)
{
    Q_UNUSED(indexes)
    Q_UNUSED(disable)
    // TODO refac : this must be rewritten
    /*
    Mlt::Service service = m_masterProducer->parent();
    for (int i = 0; i < service.filter_count(); ++i) {
        QScopedPointer<Mlt::Filter> effect(service.filter(i));
        if ((effect != nullptr) && effect->is_valid() && indexes.contains(effect->get_int("kdenlive_ix"))) {
            effect->set("disable", (int)disable);
        }
    }
    if (auto ptr = m_binController.lock()) ptr->updateTrackProducer(m_controllerBinId);
    */
}

void ClipController::updateEffect(const QDomElement &e, int ix)
{
    Q_UNUSED(e)
    Q_UNUSED(ix)
    // TODO refac : this must be rewritten
    /*
    QString tag = e.attribute(QStringLiteral("id"));
    if (tag == QLatin1String("autotrack_rectangle") || tag.startsWith(QLatin1String("ladspa")) || tag == QLatin1String("sox")) {
        // this filters cannot be edited, remove and re-add it
        removeEffect(ix, true);
        QDomElement clone = e.cloneNode().toElement();
        addEffect(clone);
        return;
    }
    EffectsParameterList params = EffectsController::getEffectArgs(e);
    Mlt::Service service = m_masterProducer->parent();
    for (int i = 0; i < service.filter_count(); ++i) {
        QScopedPointer<Mlt::Filter> effect(service.filter(i));
        if (!effect || !effect->is_valid() || effect->get_int("kdenlive_ix") != ix) {
            continue;
        }
        service.lock();
        QString prefix;
        QString ser = effect->get("mlt_service");
        if (ser == QLatin1String("region")) {
            prefix = QStringLiteral("filter0.");
        }
        for (int j = 0; j < params.count(); ++j) {
            effect->set((prefix + params.at(j).name()).toUtf8().constData(), params.at(j).value().toUtf8().constData());
            // qCDebug(KDENLIVE_LOG)<<params.at(j).name()<<" = "<<params.at(j).value();
        }
        service.unlock();
    }
    if (auto ptr = m_binController.lock()) ptr->updateTrackProducer(m_controllerBinId);
    // slotRefreshTracks();
    */
}

bool ClipController::hasEffects() const
{
    return m_effectStack->rowCount() > 0;
}

void ClipController::setBinEffectsEnabled(bool enabled)
{
    m_effectStack->setEffectStackEnabled(enabled);
}

void ClipController::saveZone(QPoint zone, const QDir &dir)
{
    QString path = QString(clipName() + QLatin1Char('_') + QString::number(zone.x()) + QStringLiteral(".mlt"));
    if (dir.exists(path)) {
        // TODO ask for overwrite
    }
    Mlt::Consumer xmlConsumer(pCore->getCurrentProfile()->profile(), ("xml:" + dir.absoluteFilePath(path)).toUtf8().constData());
    xmlConsumer.set("terminate_on_pause", 1);
    QReadLocker lock(&m_producerLock);
    Mlt::Producer prod(m_masterProducer->get_producer());
    Mlt::Producer *prod2 = prod.cut(zone.x(), zone.y());
    Mlt::Playlist list(pCore->getCurrentProfile()->profile());
    list.insert_at(0, *prod2, 0);
    // list.set("title", desc.toUtf8().constData());
    xmlConsumer.connect(list);
    xmlConsumer.run();
    delete prod2;
}

std::shared_ptr<EffectStackModel> ClipController::getEffectStack() const
{
    return m_effectStack;
}

bool ClipController::addEffect(const QString &effectId)
{
    return m_effectStack->appendEffect(effectId, true);
}

bool ClipController::copyEffect(const std::shared_ptr<EffectStackModel> &stackModel, int rowId)
{
    m_effectStack->copyEffect(stackModel->getEffectStackRow(rowId),
                              !m_hasAudio ? PlaylistState::VideoOnly : !m_hasVideo ? PlaylistState::AudioOnly : PlaylistState::Disabled);
    return true;
}

std::shared_ptr<MarkerListModel> ClipController::getMarkerModel() const
{
    return m_markerModel;
}

void ClipController::refreshAudioInfo()
{
    if (m_audioInfo && m_masterProducer) {
        QReadLocker lock(&m_producerLock);
        m_audioInfo->setAudioIndex(m_masterProducer, m_properties->get_int("audio_index"));
    }
}

QMap <int, QString> ClipController::audioStreams() const
{
    if (m_audioInfo) {
        return m_audioInfo->streams();
    }
    return {};
}

QList <int> ClipController::activeStreamChannels() const
{
    if (!audioInfo()) {
        return QList <int>();
    }
    return audioInfo()->activeStreamChannels();
}

QMap <int, QString> ClipController::activeStreams() const
{
    if (m_audioInfo) {
        return m_audioInfo->activeStreams();
    }
    return {};
}

int ClipController::audioStreamsCount() const
{
    if (m_audioInfo) {
        return m_audioInfo->streams().count();
    }
    return 0;
}

