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

#include "projectclip.h"
#include "bin.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "doc/kthumb.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "jobs/jobmanager.h"
#include "jobs/thumbjob.hpp"
#include "jobs/loadjob.hpp"
#include <jobs/proxyclipjob.h>
#include "kdenlivesettings.h"
#include "lib/audio/audioStreamInfo.h"
#include "mltcontroller/clip.h"
#include "mltcontroller/clipcontroller.h"
#include "mltcontroller/clippropertiescontroller.h"
#include "model/markerlistmodel.hpp"
#include "profiles/profilemodel.hpp"
#include "project/projectcommands.h"
#include "project/projectmanager.h"
#include "projectfolder.h"
#include "projectitemmodel.h"
#include "projectsubclip.h"
#include "timecode.h"
#include "timeline2/model/snapmodel.hpp"
#include "utils/KoIconUtils.h"
#include "utils/thumbnailcache.hpp"
#include "xml/xml.hpp"
#include <QPainter>
#include <kimagecache.h>

#include "kdenlive_debug.h"
#include <KLocalizedString>
#include <KMessageBox>
#include <QCryptographicHash>
#include <QDir>
#include <QDomElement>
#include <QFile>
#include <QtConcurrent>
#include <utility>

ProjectClip::ProjectClip(const QString &id, const QIcon &thumb, std::shared_ptr<ProjectItemModel> model, std::shared_ptr<Mlt::Producer> producer)
    : AbstractProjectItem(AbstractProjectItem::ClipItem, id, model)
    , ClipController(id, producer)
    , m_thumbsProducer(nullptr)
{
    m_markerModel = std::make_shared<MarkerListModel>(id, pCore->projectManager()->undoStack());
    m_clipStatus = StatusReady;
    m_name = clipName();
    m_duration = getStringDuration();
    m_date = date;
    m_description = ClipController::description();
    if (m_clipType == ClipType::Audio) {
        m_thumbnail = KoIconUtils::themedIcon(QStringLiteral("audio-x-generic"));
    } else {
        m_thumbnail = thumb;
    }
    if (m_clipType == ClipType::AV || m_clipType == ClipType::Audio || m_clipType == ClipType::Image || m_clipType == ClipType::Video || m_clipType == ClipType::Playlist || m_clipType == ClipType::TextTemplate) {
        pCore->bin()->addWatchFile(id, clipUrl());
    }
    // Make sure we have a hash for this clip
    hash();
    connect(m_markerModel.get(), &MarkerListModel::modelChanged, [&]() { setProducerProperty(QStringLiteral("kdenlive:markers"), m_markerModel->toJson()); });
    QString markers = getProducerProperty(QStringLiteral("kdenlive:markers"));
    if (!markers.isEmpty()) {
        QMetaObject::invokeMethod(m_markerModel.get(), "importFromJson", Qt::QueuedConnection, Q_ARG(const QString &, markers), Q_ARG(bool, true),
                                  Q_ARG(bool, false));
    }
    connectEffectStack();
}

// static
std::shared_ptr<ProjectClip> ProjectClip::construct(const QString &id, const QIcon &thumb, std::shared_ptr<ProjectItemModel> model,
                                                    std::shared_ptr<Mlt::Producer> producer)
{
    std::shared_ptr<ProjectClip> self(new ProjectClip(id, thumb, model, producer));
    baseFinishConstruct(self);
    model->loadSubClips(id, self->getPropertiesFromPrefix(QStringLiteral("kdenlive:clipzone.")));
    return self;
}

ProjectClip::ProjectClip(const QString &id, const QDomElement &description, const QIcon &thumb, std::shared_ptr<ProjectItemModel> model)
    : AbstractProjectItem(AbstractProjectItem::ClipItem, id, model)
    , ClipController(id)
    , m_thumbsProducer(nullptr)
{
    m_clipStatus = StatusWaiting;
    m_thumbnail = thumb;
    m_markerModel = std::make_shared<MarkerListModel>(m_binId, pCore->projectManager()->undoStack());
    if (description.hasAttribute(QStringLiteral("type"))) {
        m_clipType = (ClipType)description.attribute(QStringLiteral("type")).toInt();
        if (m_clipType == ClipType::Audio) {
            m_thumbnail = KoIconUtils::themedIcon(QStringLiteral("audio-x-generic"));
        }
    }
    m_temporaryUrl = getXmlProperty(description, QStringLiteral("resource"));
    QString clipName = getXmlProperty(description, QStringLiteral("kdenlive:clipname"));
    if (!clipName.isEmpty()) {
        m_name = clipName;
    } else if (!m_temporaryUrl.isEmpty()) {
        m_name = QFileInfo(m_temporaryUrl).fileName();
    } else {
        m_name = i18n("Untitled");
    }
    connect(m_markerModel.get(), &MarkerListModel::modelChanged, [&]() { setProducerProperty(QStringLiteral("kdenlive:markers"), m_markerModel->toJson()); });
}

std::shared_ptr<ProjectClip> ProjectClip::construct(const QString &id, const QDomElement &description, const QIcon &thumb,
                                                    std::shared_ptr<ProjectItemModel> model)
{
    std::shared_ptr<ProjectClip> self(new ProjectClip(id, description, thumb, model));
    baseFinishConstruct(self);
    return self;
}

ProjectClip::~ProjectClip()
{
    // controller is deleted in bincontroller
    if (m_clipType == ClipType::AV || m_clipType == ClipType::Audio || m_clipType == ClipType::Image || m_clipType == ClipType::Video || m_clipType == ClipType::Playlist || m_clipType == ClipType::TextTemplate) {
        pCore->bin()->removeWatchFile(clipId(), clipUrl());
    }
    m_thumbMutex.lock();
    m_requestedThumbs.clear();
    m_thumbMutex.unlock();
    m_thumbThread.waitForFinished();
    audioFrameCache.clear();
    // delete all timeline producers
    std::map<int, std::shared_ptr<Mlt::Producer>>::iterator itr = m_timelineProducers.begin();
    while (itr != m_timelineProducers.end()) {
        itr = m_timelineProducers.erase(itr);
    }
}

void ProjectClip::connectEffectStack()
{
    connect(m_effectStack.get(), &EffectStackModel::modelChanged, this, &ProjectClip::updateChildProducers);
    connect(m_effectStack.get(), &EffectStackModel::dataChanged, this, &ProjectClip::updateChildProducers);
    /*connect(m_effectStack.get(), &EffectStackModel::modelChanged, [&](){
        qDebug()<<"/ / / STACK CHANGED";
        updateChildProducers();
    });*/
}

QString ProjectClip::getToolTip() const
{
    return url();
}

QString ProjectClip::getXmlProperty(const QDomElement &producer, const QString &propertyName, const QString &defaultValue)
{
    QString value = defaultValue;
    QDomNodeList props = producer.elementsByTagName(QStringLiteral("property"));
    for (int i = 0; i < props.count(); ++i) {
        if (props.at(i).toElement().attribute(QStringLiteral("name")) == propertyName) {
            value = props.at(i).firstChild().nodeValue();
            break;
        }
    }
    return value;
}

void ProjectClip::updateAudioThumbnail(QVariantList audioLevels)
{
    std::swap(audioFrameCache, audioLevels); // avoid second copy
    m_audioThumbCreated = true;
    if (auto ptr = m_model.lock()) {
        emit std::static_pointer_cast<ProjectItemModel>(ptr)->refreshAudioThumbs(m_binId);
    }
    updateTimelineClips({TimelineModel::AudioLevelsRole});
}

bool ProjectClip::audioThumbCreated() const
{
    return (m_audioThumbCreated);
}

ClipType ProjectClip::clipType() const
{
    return m_clipType;
}

bool ProjectClip::hasParent(const QString &id) const
{
    std::shared_ptr<AbstractProjectItem> par = parent();
    while (par) {
        if (par->clipId() == id) {
            return true;
        }
        par = par->parent();
    }
    return false;
}

std::shared_ptr<ProjectClip> ProjectClip::clip(const QString &id)
{
    if (id == m_binId) {
        return std::static_pointer_cast<ProjectClip>(shared_from_this());
    }
    return std::shared_ptr<ProjectClip>();
}

std::shared_ptr<ProjectFolder> ProjectClip::folder(const QString &id)
{
    Q_UNUSED(id)
    return std::shared_ptr<ProjectFolder>();
}

std::shared_ptr<ProjectSubClip> ProjectClip::getSubClip(int in, int out)
{
    for (int i = 0; i < childCount(); ++i) {
        std::shared_ptr<ProjectSubClip> clip = std::static_pointer_cast<ProjectSubClip>(child(i))->subClip(in, out);
        if (clip) {
            return clip;
        }
    }
    return std::shared_ptr<ProjectSubClip>();
}

QStringList ProjectClip::subClipIds() const
{
    QStringList subIds;
    for (int i = 0; i < childCount(); ++i) {
        std::shared_ptr<AbstractProjectItem> clip = std::static_pointer_cast<AbstractProjectItem>(child(i));
        if (clip) {
            subIds << clip->clipId();
        }
    }
    return subIds;
}

std::shared_ptr<ProjectClip> ProjectClip::clipAt(int ix)
{
    if (ix == row()) {
        return std::static_pointer_cast<ProjectClip>(shared_from_this());
    }
    return std::shared_ptr<ProjectClip>();
}

/*bool ProjectClip::isValid() const
{
    return m_controller->isValid();
}*/

bool ProjectClip::hasUrl() const
{
    if ((m_clipType != ClipType::Color) && (m_clipType != ClipType::Unknown)) {
        return (!clipUrl().isEmpty());
    }
    return false;
}

const QString ProjectClip::url() const
{
    return clipUrl();
}

GenTime ProjectClip::duration() const
{
    return getPlaytime();
}

int ProjectClip::frameDuration() const
{
    GenTime d = duration();
    return d.frames(pCore->getCurrentFps());
}

void ProjectClip::reloadProducer(bool refreshOnly)
{
    // we find if there are some loading job on that clip
    int loadjobId = -1;
    pCore->jobManager()->hasPendingJob(clipId(), AbstractClipJob::LOADJOB, &loadjobId);
    if (refreshOnly) {
        // In that case, we only want a new thumbnail.
        // We thus set up a thumb job. We must make sure that there is no pending LOADJOB
        // Clear cache first
        m_thumbsProducer.reset();
        ThumbnailCache::get()->invalidateThumbsForClip(clipId());
        pCore->jobManager()->startJob<ThumbJob>({clipId()}, loadjobId, QString(), 150, -1, true, true);

    } else {
        //TODO: check if another load job is running?
        QDomDocument doc;
        QDomElement xml = toXml(doc);
        if (!xml.isNull()) {
            m_thumbsProducer.reset();
            ThumbnailCache::get()->invalidateThumbsForClip(clipId());
            int loadJob = pCore->jobManager()->startJob<LoadJob>({clipId()}, loadjobId, QString(), xml);
            pCore->jobManager()->startJob<ThumbJob>({clipId()}, loadJob, QString(), 150, -1, true, true);
        }
    }
}

QDomElement ProjectClip::toXml(QDomDocument &document, bool includeMeta)
{
    getProducerXML(document, includeMeta);
    QDomElement prod = document.documentElement().firstChildElement(QStringLiteral("producer"));
    if (m_clipType != ClipType::Unknown) {
        prod.setAttribute(QStringLiteral("type"), (int)m_clipType);
    }
    return prod;
}

void ProjectClip::setThumbnail(const QImage &img)
{
    QPixmap thumb = roundedPixmap(QPixmap::fromImage(img));
    if (hasProxy() && !thumb.isNull()) {
        // Overlay proxy icon
        QPainter p(&thumb);
        QColor c(220, 220, 10, 200);
        QRect r(0, 0, thumb.height() / 2.5, thumb.height() / 2.5);
        p.fillRect(r, c);
        QFont font = p.font();
        font.setPixelSize(r.height());
        font.setBold(true);
        p.setFont(font);
        p.setPen(Qt::black);
        p.drawText(r, Qt::AlignCenter, i18nc("The first letter of Proxy, used as abbreviation", "P"));
    }
    m_thumbnail = QIcon(thumb);
    if (auto ptr = m_model.lock()) {
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()), AbstractProjectItem::DataThumbnail);
    }
}

QPixmap ProjectClip::thumbnail(int width, int height)
{
    return m_thumbnail.pixmap(width, height);
}

bool ProjectClip::setProducer(std::shared_ptr<Mlt::Producer> producer, bool replaceProducer)
{
    Q_UNUSED(replaceProducer)
    qDebug() << "################### ProjectClip::setproducer";
    QMutexLocker locker(&m_producerMutex);
    updateProducer(std::move(producer));
    connectEffectStack();

    // Update info
    if (m_name.isEmpty()) {
        m_name = clipName();
    }
    m_date = date;
    m_description = ClipController::description();
    m_temporaryUrl.clear();
    if (m_clipType == ClipType::Audio) {
        m_thumbnail = KoIconUtils::themedIcon(QStringLiteral("audio-x-generic"));
    } else if (m_clipType == ClipType::Image) {
        if (getProducerIntProperty(QStringLiteral("meta.media.width")) < 8 || getProducerIntProperty(QStringLiteral("meta.media.height")) < 8) {
            KMessageBox::information(QApplication::activeWindow(),
                                     i18n("Image dimension smaller than 8 pixels.\nThis is not correctly supported by our video framework."));
        }
    }
    m_duration = getStringDuration();
    m_clipStatus = StatusReady;
    if (!hasProxy()) {
        if (auto ptr = m_model.lock()) emit std::static_pointer_cast<ProjectItemModel>(ptr)->refreshPanel(m_binId);
    }
    if (auto ptr = m_model.lock()) {
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()), AbstractProjectItem::DataDuration);
    }
    // Make sure we have a hash for this clip
    getFileHash();
    if (m_clipType == ClipType::AV || m_clipType == ClipType::Audio || m_clipType == ClipType::Image || m_clipType == ClipType::Video || m_clipType == ClipType::Playlist || m_clipType == ClipType::TextTemplate) {
        pCore->bin()->addWatchFile(clipId(), clipUrl());
    }
    // set parent again (some info need to be stored in producer)
    updateParent(parentItem().lock());
    return true;
}

std::shared_ptr<Mlt::Producer> ProjectClip::thumbProducer()
{
    QMutexLocker locker(&m_producerMutex);
    if (m_thumbsProducer) {
        return m_thumbsProducer;
    }
    if (clipType() == ClipType::Unknown) {
        return nullptr;
    }
    std::shared_ptr<Mlt::Producer> prod = originalProducer();
    if (!prod->is_valid()) {
        return nullptr;
    }
    if (KdenliveSettings::gpu_accel()) {
        //TODO: when the original producer changes, we must reload this thumb producer
        Clip clip(*prod.get());
        m_thumbsProducer = std::make_shared<Mlt::Producer>(clip.softClone(ClipController::getPassPropertiesList()));
        Mlt::Filter scaler(*prod->profile(), "swscale");
        Mlt::Filter converter(*prod->profile(), "avcolor_space");
        m_thumbsProducer->attach(scaler);
        m_thumbsProducer->attach(converter);
    } else {
        m_thumbsProducer = cloneProducer(pCore->thumbProfile());
    }
    return m_thumbsProducer;
}

std::shared_ptr<Mlt::Producer> ProjectClip::timelineProducer(PlaylistState::ClipState state, int track)
{
    if (!m_service.startsWith(QLatin1String("avformat"))) {
        std::shared_ptr<Mlt::Producer> prod(originalProducer()->cut());
        int length = getProducerIntProperty(QStringLiteral("kdenlive:duration"));
        if (length > 0) {
            prod->set_in_and_out(0, length);
        }
        return prod;
    }
    if (state == PlaylistState::VideoOnly) {
        if (m_timelineProducers.count(0) > 0) {
            return std::shared_ptr<Mlt::Producer>(m_timelineProducers.find(0)->second->cut());
        }
        std::shared_ptr<Mlt::Producer> videoProd = cloneProducer();
        videoProd->set("audio_index", -1);
        m_timelineProducers[0] = videoProd;
        return std::shared_ptr<Mlt::Producer>(videoProd->cut());
    }
    if (state == PlaylistState::AudioOnly) {
        if (m_timelineProducers.count(-track) > 0) {
            return std::shared_ptr<Mlt::Producer>(m_timelineProducers.find(-track)->second->cut());
        }
        std::shared_ptr<Mlt::Producer> audioProd = cloneProducer();
        audioProd->set("video_index", -1);
        m_timelineProducers[-track] = audioProd;
        return std::shared_ptr<Mlt::Producer>(audioProd->cut());
    }
    if (m_timelineProducers.count(track) > 0) {
        return std::shared_ptr<Mlt::Producer>(m_timelineProducers.find(track)->second->cut());
    }
    std::shared_ptr<Mlt::Producer> normalProd = cloneProducer();
    m_timelineProducers[track] = normalProd;
    return std::shared_ptr<Mlt::Producer>(normalProd->cut());
}

std::shared_ptr<Mlt::Producer> ProjectClip::cloneProducer(Mlt::Profile *destProfile)
{
    Mlt::Consumer c(*m_masterProducer->profile(), "xml", "string");
    Mlt::Service s(m_masterProducer->get_service());
    int ignore = s.get_int("ignore_points");
    if (ignore) {
        s.set("ignore_points", 0);
    }
    c.connect(s);
    c.set("time_format", "frames");
    c.set("no_meta", 1);
    c.set("no_root", 1);
    c.set("no_profile", 1);
    c.set("root", "/");
    c.set("store", "kdenlive");
    c.start();
    if (ignore) {
        s.set("ignore_points", ignore);
    }
    const QByteArray clipXml = c.get("string");
    std::shared_ptr<Mlt::Producer> prod(new Mlt::Producer(destProfile ? *destProfile : *m_masterProducer->profile(), "xml-string", clipXml.constData()));
    if (strcmp(prod->get("mlt_service"), "avformat") == 0) {
        prod->set("mlt_service", "avformat-novalidate");
    }
    return prod;
}

bool ProjectClip::isReady() const
{
    return m_clipStatus == StatusReady;
}

/*void ProjectClip::setZone(const QPoint &zone)
{
    m_zone = zone;
}*/

QPoint ProjectClip::zone() const
{
    int x = getProducerIntProperty(QStringLiteral("kdenlive:zone_in"));
    int y = getProducerIntProperty(QStringLiteral("kdenlive:zone_out"));
    if (y <= x) {
        y = getFramePlaytime() - 1;
    }
    return QPoint(x, y);
}

const QString ProjectClip::hash()
{
    QString clipHash = getProducerProperty(QStringLiteral("kdenlive:file_hash"));
    if (!clipHash.isEmpty()) {
        return clipHash;
    }
    return getFileHash();
}

const QString ProjectClip::getFileHash()
{
    QByteArray fileData;
    QByteArray fileHash;
    switch (m_clipType) {
    case ClipType::SlideShow:
        fileData = clipUrl().toUtf8();
        fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
        break;
    case ClipType::Text:
    case ClipType::TextTemplate:
        fileData = getProducerProperty(QStringLiteral("xmldata")).toUtf8();
        fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
        break;
    case ClipType::QText:
        fileData = getProducerProperty(QStringLiteral("text")).toUtf8();
        fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
        break;
    case ClipType::Color:
        fileData = getProducerProperty(QStringLiteral("resource")).toUtf8();
        fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
        break;
    default:
        QFile file(clipUrl());
        if (file.open(QIODevice::ReadOnly)) { // write size and hash only if resource points to a file
            /*
             * 1 MB = 1 second per 450 files (or faster)
             * 10 MB = 9 seconds per 450 files (or faster)
             */
            if (file.size() > 2000000) {
                fileData = file.read(1000000);
                if (file.seek(file.size() - 1000000)) {
                    fileData.append(file.readAll());
                }
            } else {
                fileData = file.readAll();
            }
            file.close();
            ClipController::setProducerProperty(QStringLiteral("kdenlive:file_size"), QString::number(file.size()));
            fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
        }
        break;
    }
    if (fileHash.isEmpty()) {
        qDebug()<<"// WARNING EMPTY CLIP HASH: ";
        return QString();
    }
    QString result = fileHash.toHex();
    ClipController::setProducerProperty(QStringLiteral("kdenlive:file_hash"), result);
    return result;
}

double ProjectClip::getOriginalFps() const
{
    return originalFps();
}

bool ProjectClip::hasProxy() const
{
    QString proxy = getProducerProperty(QStringLiteral("kdenlive:proxy"));
    return proxy.size() > 2;
}

void ProjectClip::setProperties(const QMap<QString, QString> &properties, bool refreshPanel)
{
    QMapIterator<QString, QString> i(properties);
    QMap<QString, QString> passProperties;
    bool refreshAnalysis = false;
    bool reload = false;
    bool refreshOnly = true;
    // Some properties also need to be passed to track producers
    QStringList timelineProperties;
    if (properties.contains(QStringLiteral("templatetext"))) {
        m_description = properties.value(QStringLiteral("templatetext"));
        if (auto ptr = m_model.lock())
            std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()), AbstractProjectItem::ClipStatus);
        refreshPanel = true;
    }
    timelineProperties << QStringLiteral("force_aspect_ratio") << QStringLiteral("video_index") << QStringLiteral("audio_index")
                       << QStringLiteral("set.force_full_luma") << QStringLiteral("full_luma") << QStringLiteral("threads")
                       << QStringLiteral("force_colorspace") << QStringLiteral("force_tff") << QStringLiteral("force_progressive")
                       << QStringLiteral("force_fps");
    QStringList keys;
    keys << QStringLiteral("luma_duration") << QStringLiteral("luma_file") << QStringLiteral("fade") << QStringLiteral("ttl") << QStringLiteral("softness")
         << QStringLiteral("crop") << QStringLiteral("animation");
    while (i.hasNext()) {
        i.next();
        setProducerProperty(i.key(), i.value());
        if (m_clipType == ClipType::SlideShow && keys.contains(i.key())) {
            reload = true;
            refreshOnly = false;
        }
        if (i.key().startsWith(QLatin1String("kdenlive:clipanalysis"))) {
            refreshAnalysis = true;
        }
        if (timelineProperties.contains(i.key())) {
            passProperties.insert(i.key(), i.value());
        }
    }
    if (properties.contains(QStringLiteral("kdenlive:proxy"))) {
        QString value = properties.value(QStringLiteral("kdenlive:proxy"));
        // If value is "-", that means user manually disabled proxy on this clip
        if (value.isEmpty() || value == QLatin1String("-")) {
            // reset proxy
            pCore->jobManager()->discardJobs(clipId(), AbstractClipJob::PROXYJOB);
            reloadProducer();
        } else {
            // A proxy was requested, make sure to keep original url
            setProducerProperty(QStringLiteral("kdenlive:originalurl"), url());
            pCore->jobManager()->startJob<ProxyJob>({clipId()}, -1, QString());
        }
    } else if (properties.contains(QStringLiteral("resource")) || properties.contains(QStringLiteral("templatetext")) ||
               properties.contains(QStringLiteral("autorotate"))) {
        // Clip resource changed, update thumbnail
        if (m_clipType != ClipType::Color) {
            reloadProducer();
        } else {
            reload = true;
        }
    }
    if (properties.contains(QStringLiteral("xmldata")) || !passProperties.isEmpty()) {
        reload = true;
    }
    if (refreshAnalysis) {
        emit refreshAnalysisPanel();
    }
    if (properties.contains(QStringLiteral("length")) || properties.contains(QStringLiteral("kdenlive:duration"))) {
        m_duration = getStringDuration();
        if (auto ptr = m_model.lock())
            std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()), AbstractProjectItem::DataDuration);
        refreshOnly = false;
        reload = true;
    }

    if (properties.contains(QStringLiteral("kdenlive:clipname"))) {
        m_name = properties.value(QStringLiteral("kdenlive:clipname"));
        refreshPanel = true;
        if (auto ptr = m_model.lock()) {
            std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()), AbstractProjectItem::DataName);
        }
        // update timeline clips
        updateTimelineClips(QVector<int>() << TimelineModel::NameRole);
    }
    if (refreshPanel) {
        // Some of the clip properties have changed through a command, update properties panel
        emit refreshPropertiesPanel();
    }
    if (reload) {
        // producer has changed, refresh monitor and thumbnail
        reloadProducer(refreshOnly);
        if (auto ptr = m_model.lock()) emit std::static_pointer_cast<ProjectItemModel>(ptr)->refreshClip(m_binId);
    }
    if (!passProperties.isEmpty()) {
        if (auto ptr = m_model.lock()) emit std::static_pointer_cast<ProjectItemModel>(ptr)->updateTimelineProducers(m_binId, passProperties);
    }
}

ClipPropertiesController *ProjectClip::buildProperties(QWidget *parent)
{
    auto ptr = m_model.lock();
    Q_ASSERT(ptr);
    ClipPropertiesController *panel = new ClipPropertiesController(static_cast<ClipController *>(this), parent);
    connect(this, &ProjectClip::refreshPropertiesPanel, panel, &ClipPropertiesController::slotReloadProperties);
    connect(this, &ProjectClip::refreshAnalysisPanel, panel, &ClipPropertiesController::slotFillAnalysisData);
    return panel;
}

void ProjectClip::updateParent(std::shared_ptr<TreeItem> parent)
{
    if (parent) {
        auto item = std::static_pointer_cast<AbstractProjectItem>(parent);
        ClipController::setProducerProperty(QStringLiteral("kdenlive:folderid"), item->clipId());
        qDebug() << "Setting parent to " << item->clipId();
    }
    AbstractProjectItem::updateParent(parent);
}

bool ProjectClip::matches(const QString &condition)
{
    // TODO
    Q_UNUSED(condition)
    return true;
}

bool ProjectClip::rename(const QString &name, int column)
{
    QMap<QString, QString> newProperites;
    QMap<QString, QString> oldProperites;
    bool edited = false;
    switch (column) {
    case 0:
        if (m_name == name) {
            return false;
        }
        // Rename clip
        oldProperites.insert(QStringLiteral("kdenlive:clipname"), m_name);
        newProperites.insert(QStringLiteral("kdenlive:clipname"), name);
        m_name = name;
        edited = true;
        break;
    case 2:
        if (m_description == name) {
            return false;
        }
        // Rename clip
        if (m_clipType == ClipType::TextTemplate) {
            oldProperites.insert(QStringLiteral("templatetext"), m_description);
            newProperites.insert(QStringLiteral("templatetext"), name);
        } else {
            oldProperites.insert(QStringLiteral("kdenlive:description"), m_description);
            newProperites.insert(QStringLiteral("kdenlive:description"), name);
        }
        m_description = name;
        edited = true;
        break;
    }
    if (edited) {
        pCore->bin()->slotEditClipCommand(m_binId, oldProperites, newProperites);
    }
    return edited;
}

/*QVariant ProjectClip::getData(DataType type) const
{
    switch (type) {
    case AbstractProjectItem::IconOverlay:
        return hasEffects() ? QVariant("kdenlive-track_has_effect") : QVariant();
        break;
    default:
        break;
    }
    return AbstractProjectItem::getData(type);
    }*/

void ProjectClip::slotExtractImage(const QList<int> &frames)
{
    QMutexLocker lock(&m_thumbMutex);
    for (int i = 0; i < frames.count(); i++) {
        if (!m_requestedThumbs.contains(frames.at(i))) {
            m_requestedThumbs << frames.at(i);
        }
    }
    qSort(m_requestedThumbs);
    if (!m_thumbThread.isRunning()) {
        m_thumbThread = QtConcurrent::run(this, &ProjectClip::doExtractImage);
    }
}

void ProjectClip::doExtractImage()
{
    // TODO refac: we can probably move that into a ThumbJob
    std::shared_ptr<Mlt::Producer> prod = thumbProducer();
    if (prod == nullptr || !prod->is_valid()) {
        return;
    }
    int frameWidth = 150 * prod->profile()->dar() + 0.5;
    bool ok = false;
    auto ptr = m_model.lock();
    Q_ASSERT(ptr);
    QDir thumbFolder = pCore->currentDoc()->getCacheDir(CacheThumbs, &ok);
    int max = prod->get_length();
    while (!m_requestedThumbs.isEmpty()) {
        m_thumbMutex.lock();
        int pos = m_requestedThumbs.takeFirst();
        m_thumbMutex.unlock();
        if (ok && thumbFolder.exists(hash() + QLatin1Char('#') + QString::number(pos) + QStringLiteral(".png"))) {
            emit thumbReady(pos, QImage(thumbFolder.absoluteFilePath(hash() + QLatin1Char('#') + QString::number(pos) + QStringLiteral(".png"))));
            continue;
        }
        if (pos >= max) {
            pos = max - 1;
        }
        const QString path = url() + QLatin1Char('_') + QString::number(pos);
        QImage img;
        if (ThumbnailCache::get()->hasThumbnail(clipId(), pos, true)) {
            img = ThumbnailCache::get()->getThumbnail(clipId(), pos, true);
        }
        if (!img.isNull()) {
            emit thumbReady(pos, img);
            continue;
        }
        prod->seek(pos);
        Mlt::Frame *frame = prod->get_frame();
        frame->set("deinterlace_method", "onefield");
        frame->set("top_field_first", -1);
        if (frame->is_valid()) {
            img = KThumb::getFrame(frame, frameWidth, 150, prod->profile()->sar() != 1);
            ThumbnailCache::get()->storeThumbnail(clipId(), pos, img, false);
            emit thumbReady(pos, img);
        }
        delete frame;
    }
}

int ProjectClip::audioChannels() const
{
    if (!audioInfo()) {
        return 0;
    }
    return audioInfo()->channels();
}

void ProjectClip::discardAudioThumb()
{
    QString audioThumbPath = getAudioThumbPath();
    if (!audioThumbPath.isEmpty()) {
        QFile::remove(audioThumbPath);
    }
    audioFrameCache.clear();
    qCDebug(KDENLIVE_LOG) << "////////////////////  DISCARD AUIIO THUMBNS";
    m_audioThumbCreated = false;
    pCore->jobManager()->discardJobs(clipId(), AbstractClipJob::AUDIOTHUMBJOB);
}

const QString ProjectClip::getAudioThumbPath()
{
    if (audioInfo() == nullptr) {
        return QString();
    }
    int audioStream = audioInfo()->ffmpeg_audio_index();
    QString clipHash = hash();
    if (clipHash.isEmpty()) {
        return QString();
    }
    bool ok = false;
    QDir thumbFolder = pCore->currentDoc()->getCacheDir(CacheAudio, &ok);
    if (!ok) {
        return QString();
    }
    QString audioPath = thumbFolder.absoluteFilePath(clipHash);
    if (audioStream > 0) {
        audioPath.append(QLatin1Char('_') + QString::number(audioInfo()->audio_index()));
    }
    int roundedFps = (int)pCore->getCurrentFps();
    audioPath.append(QStringLiteral("_%1_audio.png").arg(roundedFps));
    return audioPath;
}

bool ProjectClip::isTransparent() const
{
    if (m_clipType == ClipType::Text) {
        return true;
    }
    return m_clipType == ClipType::Image && getProducerIntProperty(QStringLiteral("kdenlive:transparency")) == 1;
}

QStringList ProjectClip::updatedAnalysisData(const QString &name, const QString &data, int offset)
{
    if (data.isEmpty()) {
        // Remove data
        return QStringList() << QString("kdenlive:clipanalysis." + name) << QString();
        // m_controller->resetProperty("kdenlive:clipanalysis." + name);
    }
    QString current = getProducerProperty("kdenlive:clipanalysis." + name);
    if (!current.isEmpty()) {
        if (KMessageBox::questionYesNo(QApplication::activeWindow(), i18n("Clip already contains analysis data %1", name), QString(), KGuiItem(i18n("Merge")),
                                       KGuiItem(i18n("Add"))) == KMessageBox::Yes) {
            // Merge data
            auto &profile = pCore->getCurrentProfile();
            Mlt::Geometry geometry(current.toUtf8().data(), duration().frames(profile->fps()), profile->width(), profile->height());
            Mlt::Geometry newGeometry(data.toUtf8().data(), duration().frames(profile->fps()), profile->width(), profile->height());
            Mlt::GeometryItem item;
            int pos = 0;
            while (newGeometry.next_key(&item, pos) == 0) {
                pos = item.frame();
                item.frame(pos + offset);
                pos++;
                geometry.insert(item);
            }
            return QStringList() << QString("kdenlive:clipanalysis." + name) << geometry.serialise();
            // m_controller->setProperty("kdenlive:clipanalysis." + name, geometry.serialise());
        }
        // Add data with another name
        int i = 1;
        QString previous = getProducerProperty("kdenlive:clipanalysis." + name + QString::number(i));
        while (!previous.isEmpty()) {
            ++i;
            previous = getProducerProperty("kdenlive:clipanalysis." + name + QString::number(i));
        }
        return QStringList() << QString("kdenlive:clipanalysis." + name + QString::number(i)) << geometryWithOffset(data, offset);
        // m_controller->setProperty("kdenlive:clipanalysis." + name + QLatin1Char(' ') + QString::number(i), geometryWithOffset(data, offset));
    }
    return QStringList() << QString("kdenlive:clipanalysis." + name) << geometryWithOffset(data, offset);
    // m_controller->setProperty("kdenlive:clipanalysis." + name, geometryWithOffset(data, offset));
}

QMap<QString, QString> ProjectClip::analysisData(bool withPrefix)
{
    return getPropertiesFromPrefix(QStringLiteral("kdenlive:clipanalysis."), withPrefix);
}

const QString ProjectClip::geometryWithOffset(const QString &data, int offset)
{
    if (offset == 0) {
        return data;
    }
    auto &profile = pCore->getCurrentProfile();
    Mlt::Geometry geometry(data.toUtf8().data(), duration().frames(profile->fps()), profile->width(), profile->height());
    Mlt::Geometry newgeometry(nullptr, duration().frames(profile->fps()), profile->width(), profile->height());
    Mlt::GeometryItem item;
    int pos = 0;
    while (geometry.next_key(&item, pos) == 0) {
        pos = item.frame();
        item.frame(pos + offset);
        pos++;
        newgeometry.insert(item);
    }
    return newgeometry.serialise();
}

bool ProjectClip::isSplittable() const
{
    return (m_clipType == ClipType::AV || m_clipType == ClipType::Playlist);
}

void ProjectClip::setBinEffectsEnabled(bool enabled)
{
    ClipController::setBinEffectsEnabled(enabled);
}

void ProjectClip::registerTimelineClip(std::weak_ptr<TimelineModel> timeline, int clipId)
{
    Q_ASSERT(m_registeredClips.count(clipId) == 0);
    Q_ASSERT(!timeline.expired());
    m_registeredClips[clipId] = std::move(timeline);
    setRefCount((uint)m_registeredClips.size());
}

void ProjectClip::deregisterTimelineClip(int clipId)
{
    Q_ASSERT(m_registeredClips.count(clipId) > 0);
    m_registeredClips.erase(clipId);
    setRefCount((uint)m_registeredClips.size());
}

QList<int> ProjectClip::timelineInstances() const
{
    QList<int> ids;
    for (std::map<int, std::weak_ptr<TimelineModel>>::const_iterator it = m_registeredClips.begin(); it != m_registeredClips.end(); ++it) {
        ids.push_back(it->first);
    }
    return ids;
}

bool ProjectClip::selfSoftDelete(Fun &undo, Fun &redo)
{
    auto toDelete = m_registeredClips; // we cannot use m_registeredClips directly, because it will be modified during loop
    for (const auto &clip : toDelete) {
        if (m_registeredClips.count(clip.first) == 0) {
            // clip already deleted, was probably grouped with another one
            continue;
        }
        if (auto timeline = clip.second.lock()) {
            timeline->requestItemDeletion(clip.first, undo, redo);
        } else {
            qDebug() << "Error while deleting clip: timeline unavailable";
            Q_ASSERT(false);
            return false;
        }
    }
    return AbstractProjectItem::selfSoftDelete(undo, redo);
}

bool ProjectClip::isIncludedInTimeline()
{
    return m_registeredClips.size() > 0;
}

void ProjectClip::updateChildProducers()
{
    // pass effect stack on all child producers
    QMutexLocker locker(&m_producerMutex);
    for (const auto &clip : m_timelineProducers) {
        if (auto producer = clip.second) {
            Clip clp(producer->parent());
            clp.deleteEffects();
            clp.replaceEffects(*m_masterProducer);
        }
    }
}

void ProjectClip::replaceInTimeline()
{
    for (const auto &clip : m_registeredClips) {
        if (auto timeline = clip.second.lock()) {
            timeline->requestClipReload(clip.first);
        } else {
            qDebug() << "Error while reloading clip: timeline unavailable";
            Q_ASSERT(false);
        }
    }
}

void ProjectClip::updateTimelineClips(QVector<int> roles)
{
    for (const auto &clip : m_registeredClips) {
        if (auto timeline = clip.second.lock()) {
            timeline->requestClipUpdate(clip.first, roles);
        } else {
            qDebug() << "Error while reloading clip thumb: timeline unavailable";
            Q_ASSERT(false);
            return;
        }
    }
}
