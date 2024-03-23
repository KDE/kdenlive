/*
SPDX-FileCopyrightText: 2012 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2014 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "clipcontroller.h"
#include "bin/clipcreator.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "bin/model/markersortmodel.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "doc/kthumb.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "kdenlivesettings.h"
#include "lib/audio/audioStreamInfo.h"
#include "profiles/profilemodel.hpp"

#include "core.h"
#include "kdenlive_debug.h"
#include <KLocalizedString>
#include <QApplication>
#include <QFileInfo>
#include <QPixmap>

std::shared_ptr<Mlt::Producer> ClipController::mediaUnavailable;

ClipController::ClipController(const QString &clipId, const std::shared_ptr<Mlt::Producer> &producer)
    : selectedEffectIndex(1)
    , m_audioThumbCreated(false)
    , m_producerLock(QReadWriteLock::Recursive)
    , m_masterProducer(std::move(producer))
    , m_properties(m_masterProducer ? new Mlt::Properties(m_masterProducer->get_properties()) : nullptr)
    , m_usesProxy(false)
    , m_audioInfo(nullptr)
    , m_videoIndex(0)
    , m_clipType(ClipType::Unknown)
    , m_forceLimitedDuration(false)
    , m_hasMultipleVideoStreams(false)
    , m_effectStack(m_masterProducer
                        ? EffectStackModel::construct(m_masterProducer, ObjectId(KdenliveObjectType::BinClip, clipId.toInt(), QUuid()), pCore->undoStack())
                        : nullptr)
    , m_hasAudio(false)
    , m_hasVideo(false)
    , m_controllerBinId(clipId)
{
    if (m_masterProducer && !m_masterProducer->is_valid()) {
        qCDebug(KDENLIVE_LOG) << "// WARNING, USING INVALID PRODUCER";
        return;
    }
    if (m_properties) {
        m_hasMultipleVideoStreams = m_properties->property_exists("kdenlive:multistreams");
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
    Q_ASSERT(m_masterProducer.use_count() <= 1);
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
    qDebug() << "################### ClipController::addmasterproducer FOR: " << m_controllerBinId;
    if (QString(producer->get("mlt_service")).contains(QLatin1String("avformat")) && producer->type() == mlt_service_producer_type) {
        std::shared_ptr<Mlt::Chain> chain(new Mlt::Chain(pCore->getProjectProfile()));
        chain->set_source(*producer.get());
        m_masterProducer = std::move(chain);
    } else {
        m_masterProducer = std::move(producer);
    }
    m_properties = new Mlt::Properties(m_masterProducer->get_properties());
    m_producerLock.unlock();
    // Pass temporary properties
    QMapIterator<QString, QVariant> i(m_tempProps);
    while (i.hasNext()) {
        i.next();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        switch (i.value().type()) {
#else
        switch (i.value().typeId()) {
#endif
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
    m_effectStack = EffectStackModel::construct(m_masterProducer, ObjectId(KdenliveObjectType::BinClip, id, QUuid()), pCore->undoStack());
    if (!m_masterProducer->is_valid()) {
        m_masterProducer = ClipController::mediaUnavailable;
        qCDebug(KDENLIVE_LOG) << "// WARNING, USING INVALID PRODUCER";
    } else {
        setProducerProperty(QStringLiteral("kdenlive:id"), m_controllerBinId);
        getInfoForProducer();
        checkAudioVideo();
        if (!m_hasMultipleVideoStreams && m_service.startsWith(QLatin1String("avformat")) && (m_clipType == ClipType::AV || m_clipType == ClipType::Video)) {
            // Check if clip has multiple video streams
            QList<int> videoStreams;
            QList<int> audioStreams;
            int aStreams = m_properties->get_int("meta.media.nb_streams");
            for (int ix = 0; ix < aStreams; ++ix) {
                char property[200];
                snprintf(property, sizeof(property), "meta.media.%d.stream.type", ix);
                QString type = m_properties->get(property);
                if (type == QLatin1String("video")) {
                    QString key = QString("meta.media.%1.codec.name").arg(ix);
                    QString codec_name = m_properties->get(key.toLatin1().constData());
                    if (codec_name == QLatin1String("png")) {
                        // This is a cover image, skip
                        qDebug() << "=== FOUND PNG COVER ART STREAM: " << ix;
                        continue;
                    }
                    if (codec_name == QLatin1String("mjpeg")) {
                        key = QString("meta.media.%1.stream.frame_rate").arg(ix);
                        QString fps = m_properties->get(key.toLatin1().constData());
                        if (fps.isEmpty()) {
                            key = QString("meta.media.%1.codec.frame_rate").arg(ix);
                            fps = m_properties->get(key.toLatin1().constData());
                        }
                        if (fps == QLatin1String("90000")) {
                            // This is a cover image, skip
                            qDebug() << "=== FOUND MJPEG COVER ART STREAM: " << ix;
                            continue;
                        }
                    }
                    videoStreams << ix;
                } else if (type == QLatin1String("audio")) {
                    audioStreams << ix;
                }
            }
            if (videoStreams.count() > 1) {
                setProducerProperty(QStringLiteral("kdenlive:multistreams"), 1);
                m_hasMultipleVideoStreams = true;
                QMetaObject::invokeMethod(pCore->bin(), "processMultiStream", Qt::QueuedConnection, Q_ARG(const QString &, m_controllerBinId),
                                          Q_ARG(QList<int>, videoStreams), Q_ARG(QList<int>, audioStreams));
            }
        }
    }
    connectEffectStack();
}

const QByteArray ClipController::producerXml(Mlt::Producer producer, bool includeMeta, bool includeProfile)
{
    QMutexLocker lock(&pCore->xmlMutex);
    Mlt::Consumer c(*producer.profile(), "xml", "string");
    if (!producer.is_valid()) {
        return QByteArray();
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
    c.connect(producer);
    c.run();
    return QByteArray(c.get("string"));
}

void ClipController::getProducerXML(QDomDocument &document, bool includeMeta, bool includeProfile)
{
    // TODO refac this is a probable duplicate with Clip::xml
    if (m_masterProducer) {
        const QByteArray xml = producerXml(m_masterProducer->parent(), includeMeta, includeProfile);
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
    if (!m_service.isEmpty() && proxy.length() > 2) {
        if (QFileInfo(path).isRelative() && path != QLatin1String("<tractor>")) {
            path.prepend(pCore->currentDoc()->documentRoot());
            m_properties->set("resource", path.toUtf8().constData());
        }
        if (QFileInfo(proxy).isRelative()) {
            proxy.prepend(pCore->currentDoc()->documentRoot());
            m_properties->set("kdenlive:proxy", proxy.toUtf8().constData());
        }
        if (proxy == path) {
            // This is a proxy producer, read original url from kdenlive property
            path = m_properties->get("kdenlive:originalurl");
            if (QFileInfo(path).isRelative()) {
                path.prepend(pCore->currentDoc()->documentRoot());
            }
            m_usesProxy = true;
        }
    } else if (m_service != QLatin1String("color") && m_service != QLatin1String("colour") && !path.isEmpty() && QFileInfo(path).isRelative() &&
               path != QLatin1String("<producer>") && path != QLatin1String("<tractor>")) {
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
        if (m_clipType != ClipType::Timeline) {
            m_clipType = ClipType::Playlist;
        }
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
        /*} else if (m_service.isEmpty() && path == QLatin1String("<producer>")) {
                m_clipType = ClipType::Timeline;
                m_properties = new Mlt::Properties(m_masterProducer->parent().get_properties());
                return getInfoForProducer();*/
    } else if (m_service == QLatin1String("qimage") || m_service == QLatin1String("pixbuf")) {
        if (m_path.contains(QLatin1Char('%')) || m_path.contains(QStringLiteral("/.all.")) || m_path.contains(QStringLiteral("\\.all."))) {
            m_clipType = ClipType::SlideShow;
        } else {
            m_clipType = ClipType::Image;
        }
    } else if (m_service == QLatin1String("colour") || m_service == QLatin1String("color")) {
        m_clipType = ClipType::Color;
        // Required for faster compositing
        m_masterProducer->set("mlt_image_format", "rgb");
    } else if (m_service == QLatin1String("kdenlivetitle")) {
        if (!m_path.isEmpty()) {
            m_clipType = ClipType::TextTemplate;
        } else {
            m_clipType = ClipType::Text;
        }
    } else if (m_service == QLatin1String("xml") || m_service == QLatin1String("consumer")) {
        if (m_clipType == ClipType::Timeline) {
            // Nothing to do, don't change existing type
        } else if (m_properties->property_exists("kdenlive:producer_type")) {
            m_clipType = (ClipType::ProducerType)m_properties->get_int("kdenlive:producer_type");
        } else {
            m_clipType = ClipType::Playlist;
        }
    } else if (m_service == QLatin1String("tractor") || m_service == QLatin1String("xml-string")) {
        if (m_properties->property_exists("kdenlive:producer_type")) {
            m_clipType = (ClipType::ProducerType)m_properties->get_int("kdenlive:producer_type");
        } else {
            m_clipType = ClipType::Timeline;
        }
    } else if (m_service == QLatin1String("webvfx")) {
        m_clipType = ClipType::WebVfx;
    } else if (m_service == QLatin1String("qtext")) {
        m_clipType = ClipType::QText;
    } else if (m_service == QLatin1String("qml")) {
        m_clipType = ClipType::Qml;
    } else if (m_service == QLatin1String("blipflash")) {
        // Mostly used for testing
        m_clipType = ClipType::AV;
    } else if (m_service == QLatin1String("glaxnimate")) {
        // Mostly used for testing
        m_clipType = ClipType::Animation;
    } else {
        if (m_properties->property_exists("kdenlive:producer_type")) {
            m_clipType = (ClipType::ProducerType)m_properties->get_int("kdenlive:producer_type");
        } else {
            m_clipType = ClipType::Unknown;
        }
    }

    if (audioIndex > -1) {
        buildAudioInfo(audioIndex);
    }

    if (!hasLimitedDuration()) {
        int playtime = m_masterProducer->time_to_frames(m_masterProducer->parent().get("kdenlive:duration"));
        if (playtime <= 0) {
            // Fix clips having missing kdenlive:duration
            m_masterProducer->parent().set("kdenlive:duration", m_masterProducer->frames_to_time(m_masterProducer->get_playtime(), mlt_time_clock));
            m_masterProducer->set("out", m_masterProducer->frames_to_time(m_masterProducer->get_length() - 1, mlt_time_clock));
        }
    }
}

void ClipController::buildAudioInfo(int audioIndex)
{
    if (m_audioInfo) {
        m_audioInfo.reset();
    }
    m_audioInfo = std::make_unique<AudioStreamInfo>(m_masterProducer, audioIndex, m_clipType == ClipType::Playlist || m_clipType == ClipType::Timeline);
    // Load stream effects
    for (int stream : m_audioInfo->streams().keys()) {
        QString streamEffect = m_properties->get(QString("kdenlive:stream:%1").arg(stream).toUtf8().constData());
        if (!streamEffect.isEmpty()) {
            m_streamEffects.insert(stream, streamEffect.split(QChar('#')));
        }
    }
}

bool ClipController::hasLimitedDuration() const
{
    if (m_forceLimitedDuration) {
        return true;
    }

    switch (m_clipType) {
        case ClipType::SlideShow:
            return getProducerIntProperty(QStringLiteral("loop")) == 1 ? false : true;
        case ClipType::Image:
        case ClipType::Color:
        case ClipType::TextTemplate:
        case ClipType::Text:
        case ClipType::QText:
        case ClipType::Qml:
            return false;
        case ClipType::AV:
        case ClipType::Animation:
        case ClipType::Playlist:
        case ClipType::Timeline:
            return true;
        default:
            return true;
    }
}

void ClipController::forceLimitedDuration()
{
    m_forceLimitedDuration = true;
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
        return "kdenlive:proxy,kdenlive:originalurl,kdenlive:multistreams,rotate,force_aspect_num,force_aspect_den,force_aspect_ratio,force_fps,force_"
               "progressive,force_tff,threads,"
               "force_"
               "colorspace,set.force_full_luma,file_hash,autorotate,disable_exif,xmldata,vstream,astream,set.test_image,set.test_audio,ttl";
    }
    return "kdenlive:proxy,kdenlive:originalurl,kdenlive:multistreams,rotate,force_aspect_num,force_aspect_den,force_aspect_ratio,force_fps,force_progressive,"
           "force_tff,threads,"
           "force_"
           "colorspace,set.force_full_luma,templatetext,file_hash,autorotate,disable_exif,xmldata,length,vstream,astream,set.test_image,set.test_audio,"
           "ttl";
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
    if (!m_properties) {
        // producer has not been initialized
        return addMasterProducer(producer);
    }
    m_producerLock.lockForWrite();
    Mlt::Properties passProperties;
    // Keep track of necessary properties
    const QString proxy(producer->get("kdenlive:proxy"));
    if (proxy.length() > 2 && producer->get("resource") == proxy) {
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

    if (QString(producer->get("mlt_service")).contains(QLatin1String("avformat")) && producer->type() == mlt_service_producer_type) {
        std::shared_ptr<Mlt::Chain> chain(new Mlt::Chain(pCore->getProjectProfile()));
        chain->set_source(*producer.get());
        m_masterProducer = std::move(chain);
    } else {
        m_masterProducer = std::move(producer);
    }
    m_properties = new Mlt::Properties(m_masterProducer->get_properties());
    m_producerLock.unlock();
    if (!m_masterProducer->is_valid()) {
        qCDebug(KDENLIVE_LOG) << "// WARNING, USING INVALID PRODUCER";
    } else {
        // Pass properties from previous producer
        m_properties->pass_list(passProperties, passList);
        setProducerProperty(QStringLiteral("kdenlive:id"), m_controllerBinId);
        if (m_clipType != ClipType::Timeline) {
            // Timeline clips effect stack point to the tractor and are handled in ProjectClip::reloadTimeline
            m_effectStack->resetService(m_masterProducer);
        }
        if (m_clipType == ClipType::Unknown) {
            getInfoForProducer();
        }
        checkAudioVideo();
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
        int playtime = m_masterProducer->time_to_frames(m_masterProducer->parent().get("kdenlive:duration"));
        if (playtime > 0) {
            return QString(m_properties->frames_to_time(playtime, mlt_time_smpte_df));
        }
        return m_properties->frames_to_time(m_masterProducer->parent().get_length(), mlt_time_smpte_df);
    }
    return i18n("Unknown");
}

int ClipController::getProducerDuration() const
{
    QReadLocker lock(&m_producerLock);
    if (m_masterProducer) {
        int playtime = m_masterProducer->time_to_frames(m_masterProducer->parent().get("kdenlive:duration"));
        if (playtime <= 0) {
            return m_masterProducer->get_length();
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
    if (!hasLimitedDuration()) {
        int playtime = m_masterProducer->time_to_frames(m_masterProducer->parent().get("kdenlive:duration"));
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
    if (!hasLimitedDuration() || m_clipType == ClipType::Playlist || m_clipType == ClipType::Timeline) {
        if (!m_masterProducer->parent().property_exists("kdenlive:duration")) {
            return m_masterProducer->get_length();
        }
        int playtime = m_masterProducer->time_to_frames(m_masterProducer->parent().get("kdenlive:duration"));
        return playtime == 0 ? m_masterProducer->get_length() : playtime;
    }
    return m_masterProducer->get_length();
}

bool ClipController::hasProducerProperty(const QString &name) const
{
    QReadLocker lock(&m_producerLock);
    if (m_properties == nullptr) {
        return false;
    }
    return m_properties->property_exists(name.toUtf8().constData());
}

QString ClipController::getProducerProperty(const QString &name) const
{
    QReadLocker lock(&m_producerLock);
    if (m_properties == nullptr) {
        return m_tempProps.value(name).toString();
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
    if (m_clipType == ClipType::Color || m_clipType == ClipType::Text || m_clipType == ClipType::Timeline) {
        return true;
    }
    if (m_clipType == ClipType::SlideShow) {
        // TODO
        return true;
    }
    return QFile::exists(m_path);
}

QString ClipController::serviceName() const
{
    return m_service;
}

void ClipController::setProducerProperty(const QString &name, int value)
{
    if (!m_properties) {
        m_tempProps.insert(name, value);
        return;
    }
    QWriteLocker lock(&m_producerLock);
    m_masterProducer->parent().set(name.toUtf8().constData(), value);
}

void ClipController::setProducerProperty(const QString &name, double value)
{
    if (!m_properties) {
        m_tempProps.insert(name, value);
        return;
    }
    QWriteLocker lock(&m_producerLock);
    m_masterProducer->parent().set(name.toUtf8().constData(), value);
}

void ClipController::setProducerProperty(const QString &name, const QString &value)
{
    if (!m_properties) {
        m_tempProps.insert(name, value);
        return;
    }

    QWriteLocker lock(&m_producerLock);
    if (value.isEmpty()) {
        m_masterProducer->parent().set(name.toUtf8().constData(), nullptr);
    } else {
        m_masterProducer->parent().set(name.toUtf8().constData(), value.toUtf8().constData());
    }
}

void ClipController::resetProducerProperty(const QString &name)
{
    if (!m_properties) {
        m_tempProps.insert(name, QString());
        return;
    }

    QWriteLocker lock(&m_producerLock);
    m_masterProducer->parent().set(name.toUtf8().constData(), nullptr);
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
    if (m_usesProxy) {
        int width = m_properties->get_int("kdenlive:original.meta.media.width");
        int height = m_properties->get_int("kdenlive:original.meta.media.height");
        if (width == 0) {
            width = m_properties->get_int("kdenlive:original.width");
        }
        if (height == 0) {
            height = m_properties->get_int("kdenlive:original.height");
        }
        if (width > 0 && height > 0) {
            return QSize(width, height);
        }
    }
    int width = m_properties->get_int("meta.media.width");
    if (width == 0) {
        width = m_properties->get_int("width");
    }
    int height = m_properties->get_int("meta.media.height");
    if (height == 0) {
        height = m_properties->get_int("height");
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
    if (m_masterProducer->get_int("_placeholder") == 1 || m_masterProducer->get_int("_missingsource") == 1) {
        // This is a placeholder file, try to guess from its properties
        QString orig_service = m_masterProducer->get("kdenlive:orig_service");
        if (orig_service.startsWith(QStringLiteral("avformat")) || (m_masterProducer->get_int("audio_index") + m_masterProducer->get_int("video_index") > 0)) {
            m_hasAudio = m_masterProducer->get_int("audio_index") >= 0;
            m_hasVideo = m_masterProducer->get_int("video_index") >= 0;
        } else if (orig_service == QStringLiteral("xml")) {
            // Playlist, assume we have audio and video
            m_hasAudio = true;
            m_hasVideo = true;
        } else {
            // Assume image or text producer
            m_hasAudio = false;
            m_hasVideo = true;
        }
        return;
    }
    if (m_masterProducer->property_exists("kdenlive:clip_type")) {
        int clipType = m_masterProducer->get_int("kdenlive:clip_type");
        qDebug() << "------------\nFOUND PRESET CTYPE: " << clipType << "\n------------------------";
        switch (clipType) {
        case 1:
            m_hasAudio = true;
            m_hasVideo = false;
            break;
        case 2:
            m_hasAudio = false;
            m_hasVideo = true;
            break;
        default:
            m_hasAudio = true;
            m_hasVideo = true;
            break;
        }
        if (m_clipType == ClipType::Timeline) {
            if (m_audioInfo == nullptr) {
                if (m_hasAudio) {
                    buildAudioInfo(-1);
                }
            } else if (!m_hasAudio) {
                m_audioInfo.reset();
            }
        }
    } else {
        QString service = m_masterProducer->get("kdenlive:orig_service");
        if (service.isEmpty()) {
            service = m_masterProducer->get("mlt_service");
        }
        QList<ClipType::ProducerType> avTypes = {ClipType::Playlist, ClipType::AV, ClipType::Audio, ClipType::Unknown};
        if (m_clipType == ClipType::Timeline) {
            // use sequenceproperties to decide if clip has audio, video or both
            if (m_masterProducer->parent().get_int("kdenlive:sequenceproperties.hasAudio") == 1) {
                m_hasAudio = true;
            }
            if (m_masterProducer->parent().get_int("kdenlive:sequenceproperties.hasVideo") == 1) {
                m_hasVideo = true;
            }
            if (m_hasAudio) {
                if (m_hasVideo) {
                    m_masterProducer->parent().set("kdenlive:clip_type", 0);
                } else {
                    m_masterProducer->parent().set("kdenlive:clip_type", 1);
                }
            } else if (!m_hasVideo) {
                // sequence is incorrectly configured, enable both audio and video
                m_hasAudio = true;
                m_hasVideo = true;
                m_masterProducer->parent().set("kdenlive:clip_type", 0);
            } else {
                m_masterProducer->parent().set("kdenlive:clip_type", 2);
            }
            if (m_hasAudio) {
                buildAudioInfo(-1);
            }
            return;
        }
        if (avTypes.contains(m_clipType)) {
            m_masterProducer->seek(0);
            QScopedPointer<Mlt::Frame> frame(m_masterProducer->get_frame());
            if (frame->is_valid()) {
                // test_audio returns 1 if there is NO audio (strange but true at the time this code is written)
                m_hasAudio = frame->get_int("test_audio") == 0;
                m_hasVideo = frame->get_int("test_image") == 0;
                if (m_hasAudio) {
                    if (m_hasVideo) {
                        m_masterProducer->set("kdenlive:clip_type", 0);
                    } else {
                        m_masterProducer->set("kdenlive:clip_type", 1);
                    }
                } else if (m_hasVideo) {
                    m_masterProducer->set("kdenlive:clip_type", 2);
                }
                qDebug() << "------------\nFRAME HAS AUDIO: " << m_hasAudio << " / " << m_hasVideo << "\n------------------------";
                m_masterProducer->seek(0);
            } else {
                qDebug() << "* * * *ERROR INVALID FRAME On test";
            }
            if (m_clipType == ClipType::Playlist) {
                if (m_audioInfo == nullptr) {
                    if (m_hasAudio) {
                        buildAudioInfo(-1);
                    }
                } else if (!m_hasAudio) {
                    m_audioInfo.reset();
                }
            }
        } else {
            // Assume video only producer
            m_hasAudio = false;
            m_hasVideo = true;
            m_masterProducer->set("kdenlive:clip_type", 2);
        }
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

void ClipController::setZone(const QPoint &zone)
{
    setProducerProperty(QStringLiteral("kdenlive:zone_in"), zone.x());
    setProducerProperty(QStringLiteral("kdenlive:zone_out"), zone.y());
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
    QStringList doNotPass{QStringLiteral("kdenlive:proxy"), QStringLiteral("kdenlive:originalurl"), QStringLiteral("kdenlive:clipname")};
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
        m_properties->clear(p.toUtf8().constData());
    }
    m_properties->clear("kdenlive:original.backup");
}

void ClipController::mirrorOriginalProperties(std::shared_ptr<Mlt::Properties> props)
{
    QReadLocker lock(&m_producerLock);
    if (m_usesProxy && QFileInfo(m_properties->get("resource")).fileName() == QFileInfo(m_properties->get("kdenlive:proxy")).fileName()) {
        // This is a proxy, we need to use the real source properties
        if (m_properties->get_int("kdenlive:original.backup") == 0) {
            // We have a proxy clip, load original source producer
            std::shared_ptr<Mlt::Producer> prod = std::make_shared<Mlt::Producer>(pCore->getProjectProfile(), nullptr, m_path.toUtf8().constData());
            // Probe to retrieve all original props
            prod->probe();
            Mlt::Properties sourceProps(prod->get_properties());
            props->inherit(sourceProps);
            int propsCount = sourceProps.count();
            // store original props
            QStringList doNotPass{QStringLiteral("kdenlive:proxy"), QStringLiteral("kdenlive:originalurl"), QStringLiteral("kdenlive:clipname")};
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
        props->inherit(sourceProps);
    } else {
        if (m_clipType == ClipType::AV || m_clipType == ClipType::Video || m_clipType == ClipType::Audio) {
            // Make sure that a frame / image was fetched to initialize all meta properties
            m_masterProducer->probe();
        }
        props->inherit(*m_properties);
    }
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

QStringList ClipController::filesUsedByEffects()
{
    return m_effectStack->externalFiles();
}

bool ClipController::hasEffects() const
{
    return m_effectStack->rowCount() > 0;
}

void ClipController::setBinEffectsEnabled(bool enabled)
{
    m_effectStack->setEffectStackEnabled(enabled);
}

std::shared_ptr<EffectStackModel> ClipController::getEffectStack() const
{
    return m_effectStack;
}

bool ClipController::addEffect(const QString &effectId, stringMap params)
{
    return m_effectStack->appendEffect(effectId, true, params);
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

QMap<int, QString> ClipController::audioStreams() const
{
    if (m_audioInfo) {
        return m_audioInfo->streams();
    }
    return {};
}

int ClipController::audioStreamIndex(int stream) const
{
    QList<int> streams = audioStreams().keys();
    return streams.indexOf(stream);
}

QList<int> ClipController::activeStreamChannels() const
{
    if (!audioInfo()) {
        return QList<int>();
    }
    return audioInfo()->activeStreamChannels();
}

QMap<int, QString> ClipController::activeStreams() const
{
    if (m_audioInfo) {
        return m_audioInfo->activeStreams();
    }
    return {};
}

QVector<int> ClipController::activeFfmpegStreams() const
{
    if (m_audioInfo) {
        QList<int> activeStreams = m_audioInfo->activeStreams().keys();
        QList<int> allStreams = m_audioInfo->streams().keys();
        QVector<int> ffmpegIndexes;
        for (auto &a : activeStreams) {
            ffmpegIndexes << allStreams.indexOf(a);
        }
        return ffmpegIndexes;
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

const QString ClipController::getOriginalUrl()
{
    QString path = m_properties->get("kdenlive:originalurl");
    if (path.isEmpty()) {
        path = m_path;
    }
    if (!path.isEmpty() && QFileInfo(path).isRelative()) {
        path.prepend(pCore->currentDoc()->documentRoot());
    }
    return path;
}

bool ClipController::hasProxy() const
{
    QString proxy = getProducerProperty(QStringLiteral("kdenlive:proxy"));
    // qDebug()<<"::: PROXY: "<<proxy<<" = "<<getProducerProperty(QStringLiteral("resource"));
    return proxy.size() > 2 && proxy == getProducerProperty(QStringLiteral("resource"));
}

std::shared_ptr<MarkerSortModel> ClipController::getFilteredMarkerModel() const
{
    return m_markerFilterModel;
}

bool ClipController::isFullRange() const
{
    bool full = !qstrcmp(m_masterProducer->get("meta.media.color_range"), "full");
    for (int i = 0; !full && i < m_masterProducer->get_int("meta.media.nb_streams"); i++) {
        QString key = QString("meta.media.%1.stream.type").arg(i);
        QString streamType(m_masterProducer->get(key.toLatin1().constData()));
        if (streamType == "video") {
            if (i == m_masterProducer->get_int("video_index")) {
                key = QString("meta.media.%1.codec.pix_fmt").arg(i);
                QString pix_fmt = QString::fromLatin1(m_masterProducer->get(key.toLatin1().constData()));
                if (pix_fmt.startsWith("yuvj")) {
                    full = true;
                } else if (pix_fmt.contains("gbr") || pix_fmt.contains("rgb")) {
                    full = true;
                }
            }
        }
    }
    return full;
}

bool ClipController::removeMarkerCategories(QList<int> toRemove, const QMap<int, int> remapCategories, Fun &undo, Fun &redo)
{
    bool found = false;
    if (m_markerModel->rowCount() == 0) {
        return false;
    }
    for (int i : toRemove) {
        QList<CommentedTime> toDelete = m_markerModel->getAllMarkers(i);
        if (!found && toDelete.count() > 0) {
            found = true;
        }
        if (remapCategories.contains(i)) {
            // Move markers to another category
            int newType = remapCategories.value(i);
            for (CommentedTime c : toDelete) {
                m_markerModel->addMarker(c.time(), c.comment(), newType, undo, redo);
            }
        } else {
            // Delete markers
            for (CommentedTime c : toDelete) {
                m_markerModel->removeMarker(c.time(), undo, redo);
            }
        }
    }
    return found;
}
