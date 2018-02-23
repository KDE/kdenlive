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
#include "lib/audio/audioStreamInfo.h"
#include "mltcontroller/effectscontroller.h"
#include "profiles/profilemodel.hpp"

#include "core.h"
#include "kdenlive_debug.h"
#include <KLocalizedString>
#include <QFileInfo>
#include <QPixmap>
#include <utility>

std::shared_ptr<Mlt::Producer> ClipController::mediaUnavailable;

ClipController::ClipController(const QString clipId, std::shared_ptr<Mlt::Producer> producer)
    : selectedEffectIndex(1)
    , m_audioThumbCreated(false)
    , m_masterProducer(producer)
    , m_properties(producer ? new Mlt::Properties(producer->get_properties()) : nullptr)
    , m_usesProxy(false)
    , m_audioInfo(nullptr)
    , m_audioIndex(0)
    , m_videoIndex(0)
    , m_clipType(ClipType::Unknown)
    , m_hasLimitedDuration(true)
    , m_effectStack(producer ? EffectStackModel::construct(producer, {ObjectType::BinClip, clipId.toInt()}, pCore->undoStack()) : nullptr)
    , m_controllerBinId(clipId)
{
    if (m_masterProducer && !m_masterProducer->is_valid()) {
        qCDebug(KDENLIVE_LOG) << "// WARNING, USING INVALID PRODUCER";
        return;
    }
    if (m_properties) {
        setProducerProperty(QStringLiteral("kdenlive:id"), m_controllerBinId);
        m_service = m_properties->get("mlt_service");
        QString proxy = m_properties->get("kdenlive:proxy");
        QString path = m_properties->get("resource");
        if (proxy.length() > 2) {
            // This is a proxy producer, read original url from kdenlive property
            path = m_properties->get("kdenlive:originalurl");
            if (QFileInfo(path).isRelative()) {
                path.prepend(pCore->currentDoc()->documentRoot());
            }
            m_usesProxy = true;
        } else if (m_service != QLatin1String("color") && m_service != QLatin1String("colour") && !path.isEmpty() && QFileInfo(path).isRelative()) {
            path.prepend(pCore->currentDoc()->documentRoot());
        }
        m_path = path.isEmpty() ? QString() : QFileInfo(path).absoluteFilePath();
        getInfoForProducer();
    } else {
        m_producerLock.lock();
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
    QString documentRoot = pCore->currentDoc()->documentRoot();
    m_masterProducer = producer;
    m_properties = new Mlt::Properties(m_masterProducer->get_properties());
    int id = m_controllerBinId.toInt();
    m_effectStack = EffectStackModel::construct(producer, {ObjectType::BinClip, id}, pCore->undoStack());
    if (!m_masterProducer->is_valid()) {
        m_masterProducer = ClipController::mediaUnavailable;
        m_producerLock.unlock();
        qCDebug(KDENLIVE_LOG) << "// WARNING, USING INVALID PRODUCER";
    } else {
        m_producerLock.unlock();
        QString proxy = m_properties->get("kdenlive:proxy");
        m_service = m_properties->get("mlt_service");
        QString path = m_properties->get("resource");
        m_usesProxy = false;
        if (proxy.length() > 2) {
            // This is a proxy producer, read original url from kdenlive property
            path = m_properties->get("kdenlive:originalurl");
            if (QFileInfo(path).isRelative()) {
                path.prepend(documentRoot);
            }
            m_usesProxy = true;
        } else if (m_service != QLatin1String("color") && m_service != QLatin1String("colour") && !path.isEmpty() && QFileInfo(path).isRelative()) {
            path.prepend(documentRoot);
        }
        m_path = path.isEmpty() ? QString() : QFileInfo(path).absoluteFilePath();
        getInfoForProducer();
        emitProducerChanged(m_controllerBinId, producer);
        setProducerProperty(QStringLiteral("kdenlive:id"), m_controllerBinId);
    }
    connectEffectStack();
}

namespace {
QString producerXml(const std::shared_ptr<Mlt::Producer> &producer, bool includeMeta)
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

void ClipController::getProducerXML(QDomDocument &document, bool includeMeta)
{
    // TODO refac this is a probable duplicate with Clip::xml
    if (m_masterProducer) {
        QString xml = producerXml(m_masterProducer, includeMeta);
        document.setContent(xml);
    } else {
        qCDebug(KDENLIVE_LOG) << " + + ++ NO MASTER PROD";
    }
}

void ClipController::getInfoForProducer()
{
    date = QFileInfo(m_path).lastModified();
    m_audioIndex = -1;
    m_videoIndex = -1;
    // special case: playlist with a proxy clip have to be detected separately
    if (m_usesProxy && m_path.endsWith(QStringLiteral(".mlt"))) {
        m_clipType = ClipType::Playlist;
    } else if (m_service == QLatin1String("avformat") || m_service == QLatin1String("avformat-novalidate")) {
        m_audioIndex = getProducerIntProperty(QStringLiteral("audio_index"));
        m_videoIndex = getProducerIntProperty(QStringLiteral("video_index"));
        if (m_audioIndex == -1) {
            m_clipType = ClipType::Video;
        } else if (m_videoIndex == -1) {
            m_clipType = ClipType::Audio;
        } else {
            m_clipType = ClipType::AV;
        }
    } else if (m_service == QLatin1String("qimage") || m_service == QLatin1String("pixbuf")) {
        if (m_path.contains(QLatin1Char('%')) || m_path.contains(QStringLiteral("/.all."))) {
            m_clipType = ClipType::SlideShow;
        } else {
            m_clipType = ClipType::Image;
        }
        m_hasLimitedDuration = false;
    } else if (m_service == QLatin1String("colour") || m_service == QLatin1String("color")) {
        m_clipType = ClipType::Color;
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
    } else {
        m_clipType = ClipType::Unknown;
    }
    if (m_audioIndex > -1 || m_clipType == ClipType::Playlist) {
        m_audioInfo.reset(new AudioStreamInfo(m_masterProducer.get(), m_audioIndex));
    }

    if (!m_hasLimitedDuration) {
        int playtime = m_masterProducer->get_int("kdenlive:duration");
        if (playtime <= 0) {
            // Fix clips having missing kdenlive:duration
            m_masterProducer->set("kdenlive:duration", m_masterProducer->get_playtime());
            m_masterProducer->set("out", m_masterProducer->get_length() - 1);
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
    QMutexLocker lock(&m_producerLock);
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
               "colorspace,set.force_full_luma,file_hash,autorotate";
    }
    return "kdenlive:proxy,kdenlive:originalurl,force_aspect_num,force_aspect_den,force_aspect_ratio,force_fps,force_progressive,force_tff,threads,force_"
           "colorspace,set.force_full_luma,templatetext,file_hash,autorotate,xmldata,length";
}

QMap<QString, QString> ClipController::getPropertiesFromPrefix(const QString &prefix, bool withPrefix)
{
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
    Mlt::Properties passProperties;
    // Keep track of necessary properties
    QString proxy = producer->get("kdenlive:proxy");
    if (proxy.length() > 2) {
        // This is a proxy producer, read original url from kdenlive property
        m_usesProxy = true;
    } else {
        m_usesProxy = false;
    }
    passProperties.pass_list(*m_properties, getPassPropertiesList(m_usesProxy));
    delete m_properties;
    *m_masterProducer = producer.get();
    m_properties = new Mlt::Properties(m_masterProducer->get_properties());
    // Pass properties from previous producer
    m_properties->pass_list(passProperties, getPassPropertiesList(m_usesProxy));
    m_producerLock.unlock();
    if (!m_masterProducer->is_valid()) {
        qCDebug(KDENLIVE_LOG) << "// WARNING, USING INVALID PRODUCER";
    } else {
        m_effectStack->resetService(m_masterProducer);
        // URL and name shoule not be updated otherwise when proxying a clip we cannot find back the original url
        /*m_url = QUrl::fromLocalFile(m_masterProducer->get("resource"));
        if (m_url.isValid()) {
            m_name = m_url.fileName();
        }
        */
    }
    qDebug() << "// replace finished: " << binId() << " : " << m_masterProducer->get("resource");
}

Mlt::Producer *ClipController::getTrackProducer(const QString &trackName, PlaylistState::ClipState clipState, double speed)
{
    // TODO Delete this
    Q_UNUSED(speed);
    Q_UNUSED(trackName);
    Q_UNUSED(clipState);
    return m_masterProducer.get();
    /*
    //TODO
    Q_UNUSED(speed)

    if (trackName.isEmpty()) {
        return m_masterProducer;
    }
    if (m_clipType != AV && m_clipType != Audio && m_clipType != Playlist) {
        // Only producers with audio need a different producer for each track (or we have an audio crackle bug)
        return new Mlt::Producer(m_masterProducer->parent());
    }
    QString clipWithTrackId = clipId();
    clipWithTrackId.append(QLatin1Char('_') + trackName);

    //TODO handle audio / video only producers and framebuffer
    if (clipState == PlaylistState::AudioOnly) {
        clipWithTrackId.append(QStringLiteral("_audio"));
    } else if (clipState == PlaylistState::VideoOnly) {
        clipWithTrackId.append(QStringLiteral("_video"));
    }

    Mlt::Producer *clone = m_binController->cloneProducer(*m_masterProducer);
    clone->set("id", clipWithTrackId.toUtf8().constData());
    //m_binController->replaceBinPlaylistClip(clipWithTrackId, clone->parent());
    return clone;
    */
}

const QString ClipController::getStringDuration()
{
    if (m_masterProducer) {
        int playtime = m_masterProducer->get_int("kdenlive:duration");
        if (playtime > 0) {
            return QString(m_properties->frames_to_time(playtime, mlt_time_smpte_df));
        }
        return m_masterProducer->get_length_time(mlt_time_smpte_df);
    }
    return i18n("Unknown");
}

GenTime ClipController::getPlaytime() const
{
    if (!m_masterProducer || !m_masterProducer->is_valid()) {
        return GenTime();
    }
    double fps = pCore->getCurrentFps();
    if (!m_hasLimitedDuration) {
        int playtime = m_masterProducer->get_int("kdenlive:duration");
        return GenTime(playtime == 0 ? m_masterProducer->get_playtime() : playtime, fps);
    }
    return GenTime(m_masterProducer->get_playtime(), fps);
}

int ClipController::getFramePlaytime() const
{
    if (!m_masterProducer || !m_masterProducer->is_valid()) {
        return 0;
    }
    if (!m_hasLimitedDuration) {
        int playtime = m_masterProducer->get_int("kdenlive:duration");
        return playtime == 0 ? m_masterProducer->get_playtime() : playtime;
    }
    return m_masterProducer->get_playtime();
}

QString ClipController::getProducerProperty(const QString &name) const
{
    if (!m_properties) {
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
    if (!m_properties) {
        return 0;
    }
    return m_properties->get_int64(name.toUtf8().constData());
}

double ClipController::getProducerDoubleProperty(const QString &name) const
{
    if (!m_properties) {
        return 0;
    }
    return m_properties->get_double(name.toUtf8().constData());
}

QColor ClipController::getProducerColorProperty(const QString &name) const
{
    if (!m_properties) {
        return QColor();
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
    if (!m_properties) {
        return 0;
    }
    QString propertyName = QStringLiteral("meta.media.%1.stream.frame_rate").arg(m_videoIndex);
    return m_properties->get_double(propertyName.toUtf8().constData());
}

QString ClipController::videoCodecProperty(const QString &property) const
{
    if (!m_properties) {
        return QString();
    }
    QString propertyName = QStringLiteral("meta.media.%1.codec.%2").arg(m_videoIndex).arg(property);
    return m_properties->get(propertyName.toUtf8().constData());
}

const QString ClipController::codec(bool audioCodec) const
{
    if ((m_properties == nullptr) || (m_clipType != ClipType::AV && m_clipType != ClipType::Video && m_clipType != ClipType::Audio)) {
        return QString();
    }
    QString propertyName = QStringLiteral("meta.media.%1.codec.name").arg(audioCodec ? m_audioIndex : m_videoIndex);
    return m_properties->get(propertyName.toUtf8().constData());
}

const QString ClipController::clipUrl() const
{
    return m_path;
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
    if (!m_masterProducer) return;
    // TODO: also set property on all track producers
    m_masterProducer->parent().set(name.toUtf8().constData(), value);
}

void ClipController::setProducerProperty(const QString &name, double value)
{
    if (!m_masterProducer) return;
    // TODO: also set property on all track producers
    m_masterProducer->parent().set(name.toUtf8().constData(), value);
}

void ClipController::setProducerProperty(const QString &name, const QString &value)
{
    if (!m_masterProducer) return;
    // TODO: also set property on all track producers
    if (value.isEmpty()) {
        m_masterProducer->parent().set(name.toUtf8().constData(), (char *)nullptr);
    } else {
        m_masterProducer->parent().set(name.toUtf8().constData(), value.toUtf8().constData());
    }
}

void ClipController::resetProducerProperty(const QString &name)
{
    // TODO: also set property on all track producers
    m_masterProducer->parent().set(name.toUtf8().constData(), (char *)nullptr);
}

ClipType ClipController::clipType() const
{
    return m_clipType;
}

const QSize ClipController::getFrameSize() const
{
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

QPixmap ClipController::pixmap(int framePosition, int width, int height)
{
    // TODO refac this should use the new thumb infrastructure
    m_masterProducer->seek(framePosition);
    Mlt::Frame *frame = m_masterProducer->get_frame();
    if (frame == nullptr || !frame->is_valid()) {
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
    width += width % 2;
    height += height % 2;
    const uchar *imagedata = frame->get_image(format, width, height);
    QImage image(imagedata, width, height, QImage::Format_RGBA8888);
    QPixmap pixmap;
    pixmap.convertFromImage(image);
    delete frame;
    return pixmap;
}

void ClipController::setZone(const QPoint &zone)
{
    setProducerProperty(QStringLiteral("kdenlive:zone_in"), zone.x());
    setProducerProperty(QStringLiteral("kdenlive:zone_out"), zone.y());
}

QPoint ClipController::zone() const
{
    int in = getProducerIntProperty(QStringLiteral("kdenlive:zone_in"));
    int max = getFramePlaytime() - 1;
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
    return *m_properties;
}

// TODO REFAC: this now should be managed from effectstack
void ClipController::initEffect(QDomElement &xml)
{
    QMutexLocker lock(&m_effectMutex);
    Mlt::Service service = m_masterProducer->parent();
    ItemInfo info;
    info.cropStart = GenTime();
    info.cropDuration = getPlaytime();
    EffectsList eff = effectList();
    EffectsController::initEffect(info, eff, getProducerProperty(QStringLiteral("kdenlive:proxy")), xml);
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

void ClipController::removeEffect(int effectIndex, bool delayRefresh){
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

EffectsList ClipController::effectList()
{
    return xmlEffectList(m_masterProducer->profile(), m_masterProducer->parent());
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

// static
EffectsList ClipController::xmlEffectList(Mlt::Profile *profile, Mlt::Service &service)
{
    Q_UNUSED(profile)
    Q_UNUSED(service)
    EffectsList effList(true);
    // TODO refac : rewrite this
    /*
    for (int ix = 0; ix < service.filter_count(); ++ix) {
        QScopedPointer<Mlt::Filter> effect(service.filter(ix));
        QDomElement clipeffect = Timeline::getEffectByTag(effect->get("tag"), effect->get("kdenlive_id"));
        QDomElement currenteffect = clipeffect.cloneNode().toElement();
        // recover effect parameters
        QDomNodeList params = currenteffect.elementsByTagName(QStringLiteral("parameter"));
        if (effect->get_int("disable") == 1) {
            currenteffect.setAttribute(QStringLiteral("disable"), 1);
        }
        for (int i = 0; i < params.count(); ++i) {
            QDomElement param = params.item(i).toElement();
            Timeline::setParam(param, effect->get(param.attribute(QStringLiteral("name")).toUtf8().constData()));
        }
        effList.append(currenteffect);
    }
    */
    return effList;
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
void ClipController::addEffect(const QString &effectId)
{
    m_effectStack->appendEffect(effectId, true);
}

bool ClipController::copyEffect(std::shared_ptr<EffectStackModel> stackModel, int rowId)
{
    m_effectStack->copyEffect(stackModel->getEffectStackRow(rowId));
    return true;
}

std::shared_ptr<MarkerListModel> ClipController::getMarkerModel() const
{
    return m_markerModel;
}
