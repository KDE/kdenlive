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
#include "jobs/audiothumbjob.hpp"
#include "jobs/jobmanager.h"
#include "jobs/loadjob.hpp"
#include "jobs/thumbjob.hpp"
#include "jobs/cachejob.hpp"
#include "kdenlivesettings.h"
#include "lib/audio/audioStreamInfo.h"
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

#include "utils/thumbnailcache.hpp"
#include "xml/xml.hpp"
#include <QPainter>
#include <jobs/proxyclipjob.h>
#include <kimagecache.h>

#include "kdenlive_debug.h"
#include "logger.hpp"
#include <KLocalizedString>
#include <KMessageBox>
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDomElement>
#include <QFile>
#include <memory>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <rttr/registration>

#pragma GCC diagnostic pop
RTTR_REGISTRATION
{
    using namespace rttr;
    registration::class_<ProjectClip>("ProjectClip");
}

ProjectClip::ProjectClip(const QString &id, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model, std::shared_ptr<Mlt::Producer> producer)
    : AbstractProjectItem(AbstractProjectItem::ClipItem, id, model)
    , ClipController(id, std::move(producer))
{
    m_markerModel = std::make_shared<MarkerListModel>(id, pCore->projectManager()->undoStack());
    if (producer->get_int("_placeholder") == 1) {
        m_clipStatus = StatusMissing;
    } else if (producer->get_int("_missingsource") == 1) {
        m_clipStatus = StatusProxyOnly;
    } else {
        m_clipStatus = StatusReady;
    }
    m_name = clipName();
    m_duration = getStringDuration();
    m_inPoint = 0;
    m_outPoint = 0;
    m_date = date;
    m_description = ClipController::description();
    if (m_clipType == ClipType::Audio) {
        m_thumbnail = QIcon::fromTheme(QStringLiteral("audio-x-generic"));
    } else {
        m_thumbnail = thumb;
    }
    // Make sure we have a hash for this clip
    hash();
    connect(m_markerModel.get(), &MarkerListModel::modelChanged, this, [&]() {
        setProducerProperty(QStringLiteral("kdenlive:markers"), m_markerModel->toJson());
    });
    QString markers = getProducerProperty(QStringLiteral("kdenlive:markers"));
    if (!markers.isEmpty()) {
        QMetaObject::invokeMethod(m_markerModel.get(), "importFromJson", Qt::QueuedConnection, Q_ARG(QString, markers), Q_ARG(bool, true),
                                  Q_ARG(bool, false));
    }
    setTags(getProducerProperty(QStringLiteral("kdenlive:tags")));
    AbstractProjectItem::setRating((uint) getProducerIntProperty(QStringLiteral("kdenlive:rating")));
    connectEffectStack();
}

// static
std::shared_ptr<ProjectClip> ProjectClip::construct(const QString &id, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model,
                                                    const std::shared_ptr<Mlt::Producer> &producer)
{
    std::shared_ptr<ProjectClip> self(new ProjectClip(id, thumb, model, producer));
    baseFinishConstruct(self);
    QMetaObject::invokeMethod(model.get(), "loadSubClips", Qt::QueuedConnection, Q_ARG(QString, id), Q_ARG(QString, self->getProducerProperty(QStringLiteral("kdenlive:clipzones"))));
    return self;
}

void ProjectClip::importEffects(const std::shared_ptr<Mlt::Producer> &producer, QString originalDecimalPoint)
{
    m_effectStack->importEffects(producer, PlaylistState::Disabled, true, originalDecimalPoint);
}

ProjectClip::ProjectClip(const QString &id, const QDomElement &description, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model)
    : AbstractProjectItem(AbstractProjectItem::ClipItem, id, model)
    , ClipController(id)
{
    m_clipStatus = StatusWaiting;
    m_thumbnail = thumb;
    m_markerModel = std::make_shared<MarkerListModel>(m_binId, pCore->projectManager()->undoStack());
    if (description.hasAttribute(QStringLiteral("type"))) {
        m_clipType = (ClipType::ProducerType)description.attribute(QStringLiteral("type")).toInt();
        if (m_clipType == ClipType::Audio) {
            m_thumbnail = QIcon::fromTheme(QStringLiteral("audio-x-generic"));
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
    connect(m_markerModel.get(), &MarkerListModel::modelChanged, this, [&]() { setProducerProperty(QStringLiteral("kdenlive:markers"), m_markerModel->toJson()); });
}

std::shared_ptr<ProjectClip> ProjectClip::construct(const QString &id, const QDomElement &description, const QIcon &thumb,
                                                    std::shared_ptr<ProjectItemModel> model)
{
    std::shared_ptr<ProjectClip> self(new ProjectClip(id, description, thumb, std::move(model)));
    baseFinishConstruct(self);
    return self;
}

ProjectClip::~ProjectClip()
{
    // controller is deleted in bincontroller
    m_thumbMutex.lock();
    m_requestedThumbs.clear();
    m_thumbMutex.unlock();
    m_thumbThread.waitForFinished();
}

void ProjectClip::connectEffectStack()
{
    connect(m_effectStack.get(), &EffectStackModel::dataChanged, this, [&]() {
        if (auto ptr = m_model.lock()) {
            std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()),
                                                                           AbstractProjectItem::IconOverlay);
        }
    });
}

QString ProjectClip::getToolTip() const
{
    if (m_clipType == ClipType::Color && m_path.contains(QLatin1Char('/'))) {
        return m_path.section(QLatin1Char('/'), -1);
    }
    return m_path;
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

void ProjectClip::updateAudioThumbnail()
{
    emit audioThumbReady();
    if (m_clipType == ClipType::Audio) {
        QImage thumb = ThumbnailCache::get()->getThumbnail(m_binId, 0);
        if (thumb.isNull()) {
            int iconHeight = QFontInfo(qApp->font()).pixelSize() * 3.5;
            QImage img(QSize(iconHeight * pCore->getCurrentDar(), iconHeight), QImage::Format_ARGB32);
            img.fill(Qt::darkGray);
            QMap <int, QString> streams = audioInfo()->streams();
            QMap <int, int> channelsList = audioInfo()->streamChannels();
            QPainter painter(&img);
            QPen pen = painter.pen();
            pen.setColor(Qt::white);
            painter.setPen(pen);
            int streamCount = 0;
            if (streams.count() > 0) {
                double streamHeight = iconHeight / streams.count();
                QMapIterator<int, QString> st(streams);
                while (st.hasNext()) {
                    st.next();
                    int channels = channelsList.value(st.key());
                    double channelHeight = (double) streamHeight / channels;
                    const QVector <uint8_t> audioLevels = audioFrameCache(st.key());
                    qreal indicesPrPixel = qreal(audioLevels.length()) / img.width();
                    int idx;
                    for (int channel = 0; channel < channels; channel++) {
                        double y = (streamHeight * streamCount) + (channel * channelHeight) + channelHeight / 2;
                        for (int i = 0; i <= img.width(); i++) {
                            idx = ceil(i * indicesPrPixel);
                            idx += idx % channels;
                            idx += channel;
                            if (idx >= audioLevels.length() || idx < 0) {
                                break;
                            }
                            double level = audioLevels.at(idx) * channelHeight / 510.; // divide height by 510 (2*255) to get height
                            painter.drawLine(i, y - level, i, y + level);
                        }
                    }
                    streamCount++;
                }
            }
            thumb = img;
            // Cache thumbnail
            ThumbnailCache::get()->storeThumbnail(m_binId, 0, thumb, true);
        }
        setThumbnail(thumb);
    }
    if (!KdenliveSettings::audiothumbnails()) {
        return;
    }
    m_audioThumbCreated = true;
    updateTimelineClips({TimelineModel::ReloadThumbRole});
}

bool ProjectClip::audioThumbCreated() const
{
    return (m_audioThumbCreated);
}

ClipType::ProducerType ProjectClip::clipType() const
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

size_t ProjectClip::frameDuration() const
{
    return (size_t)getFramePlaytime();
}

void ProjectClip::reloadProducer(bool refreshOnly, bool isProxy, bool forceAudioReload)
{
    // we find if there are some loading job on that clip
    int loadjobId = -1;
    pCore->jobManager()->hasPendingJob(clipId(), AbstractClipJob::LOADJOB, &loadjobId);
    QMutexLocker lock(&m_thumbMutex);
    if (refreshOnly) {
        // In that case, we only want a new thumbnail.
        // We thus set up a thumb job. We must make sure that there is no pending LOADJOB
        // Clear cache first
        ThumbnailCache::get()->invalidateThumbsForClip(clipId());
        pCore->jobManager()->discardJobs(clipId(), AbstractClipJob::THUMBJOB);
        m_thumbsProducer.reset();
        emit pCore->jobManager()->startJob<ThumbJob>({clipId()}, loadjobId, QString(), -1, true, true);
    } else {
        // If another load job is running?
        if (loadjobId > -1) {
            pCore->jobManager()->discardJobs(clipId(), AbstractClipJob::LOADJOB);
        }
        if (QFile::exists(m_path) && !hasProxy()) {
            clearBackupProperties();
        }
        QDomDocument doc;
        QDomElement xml = toXml(doc);
        if (!xml.isNull()) {
            bool hashChanged = false;
            pCore->jobManager()->discardJobs(clipId(), AbstractClipJob::THUMBJOB);
            m_thumbsProducer.reset();
            ClipType::ProducerType type = clipType();
            if (type != ClipType::Color && type != ClipType::Image && type != ClipType::SlideShow) {
                xml.removeAttribute("out");
            }
            if (type == ClipType::Audio || type == ClipType::AV) {
                // Check if source file was changed and rebuild audio data if necessary
                QString clipHash = getProducerProperty(QStringLiteral("kdenlive:file_hash"));
                if (!clipHash.isEmpty()) {
                    if (clipHash != getFileHash()) {
                        // Source clip has changed, rebuild data
                        hashChanged = true;
                    }
                }
                
            }
            ThumbnailCache::get()->invalidateThumbsForClip(clipId());
            int loadJob = pCore->jobManager()->startJob<LoadJob>({clipId()}, loadjobId, QString(), xml);
            emit pCore->jobManager()->startJob<ThumbJob>({clipId()}, loadJob, QString(), -1, true, true);
            if (forceAudioReload || (!isProxy && hashChanged)) {
                discardAudioThumb();
            }
            if (KdenliveSettings::audiothumbnails()) {
                emit pCore->jobManager()->startJob<AudioThumbJob>({clipId()}, loadjobId, QString());
            }
        }
    }
}

QDomElement ProjectClip::toXml(QDomDocument &document, bool includeMeta, bool includeProfile)
{
    getProducerXML(document, includeMeta, includeProfile);
    QDomElement prod;
    if (document.documentElement().tagName() == QLatin1String("producer")) {
        prod = document.documentElement();
    } else {
        prod = document.documentElement().firstChildElement(QStringLiteral("producer"));
    }
    if (m_clipType != ClipType::Unknown) {
        prod.setAttribute(QStringLiteral("type"), (int)m_clipType);
    }
    return prod;
}

void ProjectClip::setThumbnail(const QImage &img)
{
    if (img.isNull()) {
        return;
    }
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
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()),
                                                                       AbstractProjectItem::DataThumbnail);
    }
}

bool ProjectClip::hasAudioAndVideo() const
{
    return hasAudio() && hasVideo() && m_masterProducer->get_int("set.test_image") == 0 && m_masterProducer->get_int("set.test_audio") == 0;
}

bool ProjectClip::isCompatible(PlaylistState::ClipState state) const
{
    switch (state) {
    case PlaylistState::AudioOnly:
        return hasAudio() && (m_masterProducer->get_int("set.test_audio") == 0);
    case PlaylistState::VideoOnly:
        return hasVideo() && (m_masterProducer->get_int("set.test_image") == 0);
    default:
        return true;
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
    updateProducer(producer);
    m_thumbsProducer.reset();
    connectEffectStack();

    // Update info
    if (m_name.isEmpty()) {
        m_name = clipName();
    }
    m_date = date;
    m_description = ClipController::description();
    m_temporaryUrl.clear();
    if (m_clipType == ClipType::Audio) {
        m_thumbnail = QIcon::fromTheme(QStringLiteral("audio-x-generic"));
    } else if (m_clipType == ClipType::Image) {
        if (producer->get_int("meta.media.width") < 8 || producer->get_int("meta.media.height") < 8) {
            KMessageBox::information(QApplication::activeWindow(),
                                     i18n("Image dimension smaller than 8 pixels.\nThis is not correctly supported by our video framework."));
        }
    }
    m_duration = getStringDuration();
    m_clipStatus = StatusReady;
    setTags(getProducerProperty(QStringLiteral("kdenlive:tags")));
    AbstractProjectItem::setRating((uint) getProducerIntProperty(QStringLiteral("kdenlive:rating")));
    if (auto ptr = m_model.lock()) {
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()),
                                                                       AbstractProjectItem::DataDuration);
        std::static_pointer_cast<ProjectItemModel>(ptr)->updateWatcher(std::static_pointer_cast<ProjectClip>(shared_from_this()));
    }
    // Make sure we have a hash for this clip
    getFileHash();
    // set parent again (some info need to be stored in producer)
    updateParent(parentItem().lock());

    if (pCore->currentDoc()->getDocumentProperty(QStringLiteral("enableproxy")).toInt() == 1) {
        QList<std::shared_ptr<ProjectClip>> clipList;
        // automatic proxy generation enabled
        if (m_clipType == ClipType::Image && pCore->currentDoc()->getDocumentProperty(QStringLiteral("generateimageproxy")).toInt() == 1) {
            if (getProducerIntProperty(QStringLiteral("meta.media.width")) >= KdenliveSettings::proxyimageminsize() &&
                getProducerProperty(QStringLiteral("kdenlive:proxy")) == QLatin1String()) {
                clipList << std::static_pointer_cast<ProjectClip>(shared_from_this());
            }
        } else if (pCore->currentDoc()->getDocumentProperty(QStringLiteral("generateproxy")).toInt() == 1 &&
                   (m_clipType == ClipType::AV || m_clipType == ClipType::Video) && getProducerProperty(QStringLiteral("kdenlive:proxy")) == QLatin1String()) {
            bool skipProducer = false;
            if (pCore->currentDoc()->getDocumentProperty(QStringLiteral("enableexternalproxy")).toInt() == 1) {
                QStringList externalParams = pCore->currentDoc()->getDocumentProperty(QStringLiteral("externalproxyparams")).split(QLatin1Char(';'));
                // We have a camcorder profile, check if we have opened a proxy clip
                if (externalParams.count() >= 6) {
                    QFileInfo info(m_path);
                    QDir dir = info.absoluteDir();
                    dir.cd(externalParams.at(3));
                    QString fileName = info.fileName();
                    if (!externalParams.at(2).isEmpty()) {
                        fileName.chop(externalParams.at(2).size());
                    }
                    fileName.append(externalParams.at(5));
                    if (dir.exists(fileName)) {
                        setProducerProperty(QStringLiteral("kdenlive:proxy"), m_path);
                        m_path = dir.absoluteFilePath(fileName);
                        setProducerProperty(QStringLiteral("kdenlive:originalurl"), m_path);
                        getFileHash();
                        skipProducer = true;
                    }
                }
            }
            if (!skipProducer && getProducerIntProperty(QStringLiteral("meta.media.width")) >= KdenliveSettings::proxyminsize()) {
                clipList << std::static_pointer_cast<ProjectClip>(shared_from_this());
            }
        }
        if (!clipList.isEmpty()) {
            pCore->currentDoc()->slotProxyCurrentItem(true, clipList, false);
        }
    }
    pCore->bin()->reloadMonitorIfActive(clipId());
    for (auto &p : m_audioProducers) {
        m_effectStack->removeService(p.second);
    }
    for (auto &p : m_videoProducers) {
        m_effectStack->removeService(p.second);
    }
    for (auto &p : m_timewarpProducers) {
        m_effectStack->removeService(p.second);
    }
    // Release audio producers
    m_audioProducers.clear();
    m_videoProducers.clear();
    m_timewarpProducers.clear();
    emit refreshPropertiesPanel();
    if (KdenliveSettings::hoverPreview() && (m_clipType == ClipType::AV || m_clipType == ClipType::Video || m_clipType == ClipType::Playlist)) {
        QTimer::singleShot(1000, this, [this]() {
            int loadjobId;
            if (!pCore->jobManager()->hasPendingJob(m_binId, AbstractClipJob::CACHEJOB, &loadjobId)) {
                emit pCore->jobManager()->startJob<CacheJob>({m_binId}, -1, QString());
            }
        });
    }
    replaceInTimeline();
    updateTimelineClips({TimelineModel::IsProxyRole});
    return true;
}

std::shared_ptr<Mlt::Producer> ProjectClip::thumbProducer()
{
    if (m_thumbsProducer) {
        return m_thumbsProducer;
    }
    if (clipType() == ClipType::Unknown) {
        return nullptr;
    }
    QMutexLocker lock(&m_thumbMutex);
    std::shared_ptr<Mlt::Producer> prod = originalProducer();
    if (!prod->is_valid()) {
        return nullptr;
    }
    if (KdenliveSettings::gpu_accel()) {
        // TODO: when the original producer changes, we must reload this thumb producer
        m_thumbsProducer = softClone(ClipController::getPassPropertiesList());
    } else {
        QString mltService = m_masterProducer->get("mlt_service");
        const QString mltResource = m_masterProducer->get("resource");
        if (mltService == QLatin1String("avformat")) {
            mltService = QStringLiteral("avformat-novalidate");
        }
        m_thumbsProducer.reset(new Mlt::Producer(*pCore->thumbProfile(), mltService.toUtf8().constData(), mltResource.toUtf8().constData()));
        if (m_thumbsProducer->is_valid()) {
            Mlt::Properties original(m_masterProducer->get_properties());
            Mlt::Properties cloneProps(m_thumbsProducer->get_properties());
            cloneProps.pass_list(original, ClipController::getPassPropertiesList());
            Mlt::Filter scaler(*pCore->thumbProfile(), "swscale");
            Mlt::Filter padder(*pCore->thumbProfile(), "resize");
            Mlt::Filter converter(*pCore->thumbProfile(), "avcolor_space");
            m_thumbsProducer->set("audio_index", -1);
            // Required to make get_playtime() return > 1
            m_thumbsProducer->set("out", m_thumbsProducer->get_length() -1);
            m_thumbsProducer->attach(scaler);
            m_thumbsProducer->attach(padder);
            m_thumbsProducer->attach(converter);
        }
    }
    return m_thumbsProducer;
}

void ProjectClip::createDisabledMasterProducer()
{
    if (!m_disabledProducer) {
        m_disabledProducer = cloneProducer();
        m_disabledProducer->set("set.test_audio", 1);
        m_disabledProducer->set("set.test_image", 1);
        m_effectStack->addService(m_disabledProducer);
    }
}

std::shared_ptr<Mlt::Producer> ProjectClip::getTimelineProducer(int trackId, int clipId, PlaylistState::ClipState state, int audioStream, double speed)
{
    if (!m_masterProducer) {
        return nullptr;
    }
    if (qFuzzyCompare(speed, 1.0)) {
        // we are requesting a normal speed producer
        bool byPassTrackProducer = false;
        if (trackId == -1 && (state != PlaylistState::AudioOnly || audioStream == m_masterProducer->get_int("audio_index"))) {
            byPassTrackProducer = true;
        }
        if (byPassTrackProducer ||
            (state == PlaylistState::VideoOnly && (m_clipType == ClipType::Color || m_clipType == ClipType::Image || m_clipType == ClipType::Text|| m_clipType == ClipType::TextTemplate || m_clipType == ClipType::Qml))) {
            // Temporary copy, return clone of master
            int duration = m_masterProducer->time_to_frames(m_masterProducer->get("kdenlive:duration"));
            return std::shared_ptr<Mlt::Producer>(m_masterProducer->cut(-1, duration > 0 ? duration - 1 : -1));
        }
        if (m_timewarpProducers.count(clipId) > 0) {
            m_effectStack->removeService(m_timewarpProducers[clipId]);
            m_timewarpProducers.erase(clipId);
        }
        if (state == PlaylistState::AudioOnly) {
            // We need to get an audio producer, if none exists
            if (audioStream > -1) {
                if (trackId >= 0) {
                    trackId += 100 * audioStream;
                } else {
                    trackId -= 100 * audioStream;
                }
            }
            if (m_audioProducers.count(trackId) == 0) {
                m_audioProducers[trackId] = cloneProducer(true);
                m_audioProducers[trackId]->set("set.test_audio", 0);
                m_audioProducers[trackId]->set("set.test_image", 1);
                if (m_streamEffects.contains(audioStream)) {
                    QStringList effects = m_streamEffects.value(audioStream);
                    for (const QString &effect : qAsConst(effects)) {
                        Mlt::Filter filt(*m_audioProducers[trackId]->profile(), effect.toUtf8().constData());
                        if (filt.is_valid()) {
                            // Add stream effect markup
                            filt.set("kdenlive:stream", 1);
                            m_audioProducers[trackId]->attach(filt);
                        }
                    }
                }
                if (audioStream > -1) {
                    m_audioProducers[trackId]->set("audio_index", audioStream);
                }
                m_effectStack->addService(m_audioProducers[trackId]);
            }
            return std::shared_ptr<Mlt::Producer>(m_audioProducers[trackId]->cut());
        }
        if (m_audioProducers.count(trackId) > 0) {
            m_effectStack->removeService(m_audioProducers[trackId]);
            m_audioProducers.erase(trackId);
        }
        if (state == PlaylistState::VideoOnly) {
            // we return the video producer
            // We need to get an video producer, if none exists
            if (m_videoProducers.count(trackId) == 0) {
                m_videoProducers[trackId] = cloneProducer(true);
                m_videoProducers[trackId]->set("set.test_audio", 1);
                m_videoProducers[trackId]->set("set.test_image", 0);
                m_effectStack->addService(m_videoProducers[trackId]);
            }
            int duration = m_masterProducer->time_to_frames(m_masterProducer->get("kdenlive:duration"));
            return std::shared_ptr<Mlt::Producer>(m_videoProducers[trackId]->cut(-1, duration > 0 ? duration - 1: -1));
        }
        if (m_videoProducers.count(trackId) > 0) {
            m_effectStack->removeService(m_videoProducers[trackId]);
            m_videoProducers.erase(trackId);
        }
        Q_ASSERT(state == PlaylistState::Disabled);
        createDisabledMasterProducer();
        int duration = m_masterProducer->time_to_frames(m_masterProducer->get("kdenlive:duration"));
        return std::shared_ptr<Mlt::Producer>(m_disabledProducer->cut(-1, duration > 0 ? duration - 1: -1));
    }

    // For timewarp clips, we keep one separate producer for each clip.
    std::shared_ptr<Mlt::Producer> warpProducer;
    if (m_timewarpProducers.count(clipId) > 0) {
        // remove in all cases, we add it unconditionally anyways
        m_effectStack->removeService(m_timewarpProducers[clipId]);
        if (qFuzzyCompare(m_timewarpProducers[clipId]->get_double("warp_speed"), speed)) {
            // the producer we have is good, use it !
            warpProducer = m_timewarpProducers[clipId];
            qDebug() << "Reusing producer!";
        } else {
            m_timewarpProducers.erase(clipId);
        }
    }
    if (!warpProducer) {
        QString resource(originalProducer()->get("resource"));
        if (resource.isEmpty() || resource == QLatin1String("<producer>")) {
            resource = m_service;
        }
        QString url = QString("timewarp:%1:%2").arg(QString::fromStdString(std::to_string(speed)), resource);
        warpProducer.reset(new Mlt::Producer(*originalProducer()->profile(), url.toUtf8().constData()));
        qDebug() << "new producer: " << url;
        qDebug() << "warp LENGTH before" << warpProducer->get_length();
        int original_length = originalProducer()->get_length();
        // this is a workaround to cope with Mlt erroneous rounding
        Mlt::Properties original(m_masterProducer->get_properties());
        Mlt::Properties cloneProps(warpProducer->get_properties());
        cloneProps.pass_list(original, ClipController::getPassPropertiesList(false));
        warpProducer->set("length", (int) (original_length / std::abs(speed) + 0.5));
    }

    qDebug() << "warp LENGTH" << warpProducer->get_length();
    warpProducer->set("set.test_audio", 1);
    warpProducer->set("set.test_image", 1);
    if (state == PlaylistState::AudioOnly) {
        warpProducer->set("set.test_audio", 0);
    }
    if (state == PlaylistState::VideoOnly) {
        warpProducer->set("set.test_image", 0);
    }
    m_timewarpProducers[clipId] = warpProducer;
    m_effectStack->addService(m_timewarpProducers[clipId]);
    return std::shared_ptr<Mlt::Producer>(warpProducer->cut());
}

std::pair<std::shared_ptr<Mlt::Producer>, bool> ProjectClip::giveMasterAndGetTimelineProducer(int clipId, std::shared_ptr<Mlt::Producer> master, PlaylistState::ClipState state, int tid)
{
    int in = master->get_in();
    int out = master->get_out();
    if (master->parent().is_valid()) {
        // in that case, we have a cut
        // check whether it's a timewarp
        double speed = 1.0;
        bool timeWarp = false;
        if (QString::fromUtf8(master->parent().get("mlt_service")) == QLatin1String("timewarp")) {
            speed = master->parent().get_double("warp_speed");
            timeWarp = true;
        }
        if (master->parent().get_int("_loaded") == 1) {
            // we already have a clip that shares the same master
            if (state != PlaylistState::Disabled || timeWarp) {
                // In that case, we must create copies
                std::shared_ptr<Mlt::Producer> prod(getTimelineProducer(tid, clipId, state, master->parent().get_int("audio_index"), speed)->cut(in, out));
                return {prod, false};
            }
            if (state == PlaylistState::Disabled) {
                if (!m_disabledProducer) {
                    qDebug() << "Warning: weird, we found a disabled clip whose master is already loaded but we don't have any yet";
                    createDisabledMasterProducer();
                }
                return {std::shared_ptr<Mlt::Producer>(m_disabledProducer->cut(in, out)), false};
            }
            // We have a good id, this clip can be used
            return {master, true};
        } else {
            master->parent().set("_loaded", 1);
            if (timeWarp) {
                m_timewarpProducers[clipId] = std::make_shared<Mlt::Producer>(&master->parent());
                m_effectStack->loadService(m_timewarpProducers[clipId]);
                return {master, true};
            }
            if (state == PlaylistState::AudioOnly) {
                int producerId = tid;
                int audioStream = master->parent().get_int("audio_index");
                if (audioStream > -1) {
                    producerId += 100 * audioStream;
                }
                m_audioProducers[tid] = std::make_shared<Mlt::Producer>(&master->parent());
                m_effectStack->loadService(m_audioProducers[tid]);
                return {master, true};
            }
            if (state == PlaylistState::VideoOnly) {
                // good, we found a master video producer, and we didn't have any
                if (m_clipType != ClipType::Color && m_clipType != ClipType::Image && m_clipType != ClipType::Text) {
                    // Color, image and text clips always use master producer in timeline
                    m_videoProducers[tid] = std::make_shared<Mlt::Producer>(&master->parent());
                    m_effectStack->loadService(m_videoProducers[tid]);
                } else {
                    // Ensure clip out = length - 1 so that effects work correctly
                    if (out != master->parent().get_length() - 1) {
                        master->parent().set("out", master->parent().get_length() - 1);
                    }
                }
                return {master, true};
            }
            if (state == PlaylistState::Disabled) {
                if (!m_disabledProducer) {
                    createDisabledMasterProducer();
                }
                return {std::make_shared<Mlt::Producer>(m_disabledProducer->cut(master->get_in(), master->get_out())), true};
            }
            qDebug() << "Warning: weird, we found a clip whose master is not loaded but we already have a master";
            Q_ASSERT(false);
        }
    } else if (master->is_valid()) {
        // in that case, we have a master
        qDebug() << "Warning: weird, we received a master clip in lieue of a cut";
        double speed = 1.0;
        if (QString::fromUtf8(master->parent().get("mlt_service")) == QLatin1String("timewarp")) {
            speed = master->get_double("warp_speed");
        }
        return {getTimelineProducer(-1, clipId, state, master->get_int("audio_index"), speed), false};
    }
    // we have a problem
    return {std::shared_ptr<Mlt::Producer>(ClipController::mediaUnavailable->cut()), false};
}

std::shared_ptr<Mlt::Producer> ProjectClip::cloneProducer(bool removeEffects)
{
    Mlt::Consumer c(pCore->getCurrentProfile()->profile(), "xml", "string");
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
    c.run();
    if (ignore) {
        s.set("ignore_points", ignore);
    }
    const QByteArray clipXml = c.get("string");
    std::shared_ptr<Mlt::Producer> prod;
    prod.reset(new Mlt::Producer(pCore->getCurrentProfile()->profile(), "xml-string", clipXml.constData()));

    if (strcmp(prod->get("mlt_service"), "avformat") == 0) {
        prod->set("mlt_service", "avformat-novalidate");
        prod->set("mute_on_pause", 0);
    }

    // we pass some properties that wouldn't be passed because of the novalidate
    const char *prefix = "meta.";
    const size_t prefix_len = strlen(prefix);
    for (int i = 0; i < m_masterProducer->count(); ++i) {
        char *current = m_masterProducer->get_name(i);
        if (strlen(current) >= prefix_len && strncmp(current, prefix, prefix_len) == 0) {
            prod->set(current, m_masterProducer->get(i));
        }
    }

    if (removeEffects) {
        int ct = 0;
        Mlt::Filter *filter = prod->filter(ct);
        while (filter) {
            qDebug() << "// EFFECT " << ct << " : " << filter->get("mlt_service");
            QString ix = QString::fromLatin1(filter->get("kdenlive_id"));
            if (!ix.isEmpty()) {
                qDebug() << "/ + + DELETING";
                if (prod->detach(*filter) == 0) {
                } else {
                    ct++;
                }
            } else {
                ct++;
            }
            delete filter;
            filter = prod->filter(ct);
        }
    }
    prod->set("id", (char *)nullptr);
    return prod;
}

std::shared_ptr<Mlt::Producer> ProjectClip::cloneProducer(const std::shared_ptr<Mlt::Producer> &producer)
{
    Mlt::Consumer c(*producer->profile(), "xml", "string");
    Mlt::Service s(producer->get_service());
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
    std::shared_ptr<Mlt::Producer> prod(new Mlt::Producer(*producer->profile(), "xml-string", clipXml.constData()));
    if (strcmp(prod->get("mlt_service"), "avformat") == 0) {
        prod->set("mlt_service", "avformat-novalidate");
        prod->set("mute_on_pause", 0);
    }
    return prod;
}

std::shared_ptr<Mlt::Producer> ProjectClip::softClone(const char *list)
{
    QString service = QString::fromLatin1(m_masterProducer->get("mlt_service"));
    QString resource = QString::fromUtf8(m_masterProducer->get("resource"));
    std::shared_ptr<Mlt::Producer> clone(new Mlt::Producer(*pCore->thumbProfile(), service.toUtf8().constData(), resource.toUtf8().constData()));
    Mlt::Filter scaler(*pCore->thumbProfile(), "swscale");
    Mlt::Filter converter(pCore->getCurrentProfile()->profile(), "avcolor_space");
    clone->attach(scaler);
    clone->attach(converter);
    Mlt::Properties original(m_masterProducer->get_properties());
    Mlt::Properties cloneProps(clone->get_properties());
    cloneProps.pass_list(original, list);
    return clone;
}

std::unique_ptr<Mlt::Producer> ProjectClip::getClone()
{
    const char *list = ClipController::getPassPropertiesList();
    QString service = QString::fromLatin1(m_masterProducer->get("mlt_service"));
    QString resource = QString::fromUtf8(m_masterProducer->get("resource"));
    std::unique_ptr<Mlt::Producer> clone(new Mlt::Producer(*m_masterProducer->profile(), service.toUtf8().constData(), resource.toUtf8().constData()));
    Mlt::Properties original(m_masterProducer->get_properties());
    Mlt::Properties cloneProps(clone->get_properties());
    cloneProps.pass_list(original, list);
    return clone;
}

bool ProjectClip::isReady() const
{
    return m_clipStatus == StatusReady || m_clipStatus == StatusProxyOnly;
}

QPoint ProjectClip::zone() const
{
    return ClipController::zone();
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
        fileData = getProducerProperty(QStringLiteral("xmldata")).toUtf8();
        fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
        break;
    case ClipType::TextTemplate:
        fileData = getProducerProperty(QStringLiteral("resource")).toUtf8();
        fileData.append(getProducerProperty(QStringLiteral("templatetext")).toUtf8());
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
        qDebug() << "// WARNING EMPTY CLIP HASH: ";
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
    qDebug() << "// SETTING CLIP PROPERTIES: " << properties;
    QMapIterator<QString, QString> i(properties);
    QMap<QString, QString> passProperties;
    bool refreshAnalysis = false;
    bool reload = false;
    bool refreshOnly = true;
    if (properties.contains(QStringLiteral("templatetext"))) {
        m_description = properties.value(QStringLiteral("templatetext"));
        if (auto ptr = m_model.lock())
            std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()),
                                                                           AbstractProjectItem::ClipStatus);
        refreshPanel = true;
    }
    // Some properties also need to be passed to track producers
    QStringList timelineProperties{
        QStringLiteral("force_aspect_ratio"), QStringLiteral("set.force_full_luma"), QStringLiteral("full_luma"),         QStringLiteral("threads"),
        QStringLiteral("force_colorspace"), QStringLiteral("force_tff"),           QStringLiteral("force_progressive"), QStringLiteral("video_delay")
    };
    QStringList forceReloadProperties{QStringLiteral("autorotate"), QStringLiteral("templatetext"),   QStringLiteral("resource"), QStringLiteral("force_fps"),   QStringLiteral("set.test_image"), QStringLiteral("video_index"), QStringLiteral("disable_exif")};
    QStringList keys{QStringLiteral("luma_duration"), QStringLiteral("luma_file"), QStringLiteral("fade"),     QStringLiteral("ttl"), QStringLiteral("softness"), QStringLiteral("crop"), QStringLiteral("animation")};
    QVector<int> updateRoles;
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
    if (properties.contains(QStringLiteral("resource"))) {
        // Clip source was changed, update important stuff
        refreshPanel = true;
        reload = true;
        if (m_clipType == ClipType::Color) {
            refreshOnly = true;
            updateRoles << TimelineModel::ResourceRole;
        } else {
            // Clip resource changed, update thumbnail, name, clear hash
            refreshOnly = false;
            resetProducerProperty(QStringLiteral("kdenlive:file_hash"));
            getInfoForProducer();
            updateRoles << TimelineModel::ResourceRole << TimelineModel::MaxDurationRole << TimelineModel::NameRole;
        }
    }
    if (properties.contains(QStringLiteral("kdenlive:proxy"))) {
        QString value = properties.value(QStringLiteral("kdenlive:proxy"));
        // If value is "-", that means user manually disabled proxy on this clip
        if (value.isEmpty() || value == QLatin1String("-")) {
            // reset proxy
            int id;
            if (pCore->jobManager()->hasPendingJob(clipId(), AbstractClipJob::PROXYJOB, &id)) {
                // The proxy clip is being created, abort
                pCore->jobManager()->discardJobs(clipId(), AbstractClipJob::PROXYJOB);
            } else {
                reload = true;
                refreshOnly = false;
            }
        } else {
            // A proxy was requested, make sure to keep original url
            setProducerProperty(QStringLiteral("kdenlive:originalurl"), url());
            backupOriginalProperties();
            emit pCore->jobManager()->startJob<ProxyJob>({clipId()}, -1, QString());
        }
    } else if (!reload) {
        const QList<QString> propKeys = properties.keys();
        for (const QString &k : propKeys) {
            if (forceReloadProperties.contains(k)) {
                refreshPanel = true;
                refreshOnly = false;
                reload = true;
                break;
            }
        }
    }
    if (!reload && (properties.contains(QStringLiteral("xmldata")) || !passProperties.isEmpty())) {
        reload = true;
    }
    if (refreshAnalysis) {
        emit refreshAnalysisPanel();
    }
    if (properties.contains(QStringLiteral("length")) || properties.contains(QStringLiteral("kdenlive:duration"))) {
        // Make sure length is >= kdenlive:duration
        int producerLength = getProducerIntProperty(QStringLiteral("length"));
        int kdenliveLength = getFramePlaytime();
        if (producerLength < kdenliveLength) {
            setProducerProperty(QStringLiteral("length"), kdenliveLength);
        }
        m_duration = getStringDuration();
        if (auto ptr = m_model.lock())
            std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()),
                                                                           AbstractProjectItem::DataDuration);
        refreshOnly = false;
        reload = true;
    }

    if (properties.contains(QStringLiteral("kdenlive:tags"))) {
        setTags(properties.value(QStringLiteral("kdenlive:tags")));
        if (auto ptr = m_model.lock()) {
            std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()),
                                                                           AbstractProjectItem::DataTag);
        }
    }
    if (properties.contains(QStringLiteral("kdenlive:clipname"))) {
        m_name = properties.value(QStringLiteral("kdenlive:clipname"));
        refreshPanel = true;
        if (auto ptr = m_model.lock()) {
            std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()),
                                                                           AbstractProjectItem::DataName);
        }
        // update timeline clips
        if (!reload) {
            updateTimelineClips({TimelineModel::NameRole});
        }
    }
    bool audioStreamChanged = properties.contains(QStringLiteral("audio_index"));
    if (reload) {
        // producer has changed, refresh monitor and thumbnail
        if (hasProxy()) {
            pCore->jobManager()->discardJobs(clipId(), AbstractClipJob::PROXYJOB);
            setProducerProperty(QStringLiteral("_overwriteproxy"), 1);
            emit pCore->jobManager()->startJob<ProxyJob>({clipId()}, -1, QString());
        } else {
            reloadProducer(refreshOnly, properties.contains(QStringLiteral("kdenlive:proxy")));
        }
        if (refreshOnly) {
            if (auto ptr = m_model.lock()) {
                emit std::static_pointer_cast<ProjectItemModel>(ptr)->refreshClip(m_binId);
            }
        }
        if (!updateRoles.isEmpty()) {
            updateTimelineClips(updateRoles);
        }
    } else {
        if (properties.contains(QStringLiteral("kdenlive:active_streams")) && m_audioInfo) {
            // Clip is a multi audio stream and currently in clip monitor, update target tracks
            m_audioInfo->updateActiveStreams(properties.value(QStringLiteral("kdenlive:active_streams")));
            pCore->bin()->updateTargets(clipId());
            if (!audioStreamChanged) {
                pCore->bin()->reloadMonitorStreamIfActive(clipId());
                pCore->bin()->checkProjectAudioTracks(clipId(), m_audioInfo->activeStreams().count());
                refreshPanel = true;
            }
        }
        if (audioStreamChanged) {
            refreshAudioInfo();
            emit audioThumbReady();
            pCore->bin()->reloadMonitorStreamIfActive(clipId());
            refreshPanel = true;
        }
    }
    if (refreshPanel) {
        // Some of the clip properties have changed through a command, update properties panel
        emit refreshPropertiesPanel();
    }
    if (!passProperties.isEmpty() && (!reload || refreshOnly)) {
        for (auto &p : m_audioProducers) {
            QMapIterator<QString, QString> pr(passProperties);
            while (pr.hasNext()) {
                pr.next();
                p.second->set(pr.key().toUtf8().constData(), pr.value().toUtf8().constData());
            }
        }
        for (auto &p : m_videoProducers) {
            QMapIterator<QString, QString> pr(passProperties);
            while (pr.hasNext()) {
                pr.next();
                p.second->set(pr.key().toUtf8().constData(), pr.value().toUtf8().constData());
            }
        }
        for (auto &p : m_timewarpProducers) {
            QMapIterator<QString, QString> pr(passProperties);
            while (pr.hasNext()) {
                pr.next();
                p.second->set(pr.key().toUtf8().constData(), pr.value().toUtf8().constData());
            }
        }
    }
}

ClipPropertiesController *ProjectClip::buildProperties(QWidget *parent)
{
    auto ptr = m_model.lock();
    Q_ASSERT(ptr);
    auto *panel = new ClipPropertiesController(static_cast<ClipController *>(this), parent);
    connect(this, &ProjectClip::refreshPropertiesPanel, panel, &ClipPropertiesController::slotReloadProperties);
    connect(this, &ProjectClip::refreshAnalysisPanel, panel, &ClipPropertiesController::slotFillAnalysisData);
    connect(this, &ProjectClip::updateStreamInfo, panel, &ClipPropertiesController::updateStreamInfo);
    connect(panel, &ClipPropertiesController::requestProxy, this, [this](bool doProxy) {
        QList<std::shared_ptr<ProjectClip>> clipList{std::static_pointer_cast<ProjectClip>(shared_from_this())};
        pCore->currentDoc()->slotProxyCurrentItem(doProxy, clipList);
    });
    connect(panel, &ClipPropertiesController::deleteProxy, this, &ProjectClip::deleteProxy);
    return panel;
}

void ProjectClip::deleteProxy()
{
    // Disable proxy file
    QString proxy = getProducerProperty(QStringLiteral("kdenlive:proxy"));
    QList<std::shared_ptr<ProjectClip>> clipList{std::static_pointer_cast<ProjectClip>(shared_from_this())};
    pCore->currentDoc()->slotProxyCurrentItem(false, clipList);
    // Delete
    bool ok;
    QDir dir = pCore->currentDoc()->getCacheDir(CacheProxy, &ok);
    if (ok && proxy.length() > 2) {
        proxy = QFileInfo(proxy).fileName();
        if (dir.exists(proxy)) {
            dir.remove(proxy);
        }
    }
}

void ProjectClip::updateParent(std::shared_ptr<TreeItem> parent)
{
    if (parent) {
        auto item = std::static_pointer_cast<AbstractProjectItem>(parent);
        ClipController::setProducerProperty(QStringLiteral("kdenlive:folderid"), item->clipId());
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

QVariant ProjectClip::getData(DataType type) const
{
    switch (type) {
        case AbstractProjectItem::IconOverlay:
            if (m_clipStatus == AbstractProjectItem::StatusMissing) {
                return QVariant("window-close");
            }
            if (m_clipStatus == AbstractProjectItem::StatusWaiting) {
                return QVariant("view-refresh");
            }
            return m_effectStack && m_effectStack->rowCount() > 0 ? QVariant("kdenlive-track_has_effect") : QVariant();
        default:
            return AbstractProjectItem::getData(type);
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
    if (!m_audioInfo) {
        return;
    }
    pCore->jobManager()->discardJobs(clipId(), AbstractClipJob::AUDIOTHUMBJOB);
    QString audioThumbPath;
    QList <int> streams = m_audioInfo->streams().keys();
    // Delete audio thumbnail data
    for (int &st : streams) {
        audioThumbPath = getAudioThumbPath(st);
        if (!audioThumbPath.isEmpty()) {
            QFile::remove(audioThumbPath);
        }
    }
    // Delete thumbnail
    for (int &st : streams) {
        audioThumbPath = getAudioThumbPath(st);
        if (!audioThumbPath.isEmpty()) {
            QFile::remove(audioThumbPath);
        }
    }
    m_audioThumbCreated = false;
    refreshAudioInfo();
}

int ProjectClip::getAudioStreamFfmpegIndex(int mltStream)
{
    if (!m_masterProducer || !audioInfo()) {
        return -1;
    }
    QList<int> audioStreams = audioInfo()->streams().keys();
    if (audioStreams.contains(mltStream)) {
        return audioStreams.indexOf(mltStream);
    }
    return -1;
}

const QString ProjectClip::getAudioThumbPath(int stream)
{
    if (audioInfo() == nullptr) {
        return QString();
    }
    bool ok = false;
    QDir thumbFolder = pCore->currentDoc()->getCacheDir(CacheAudio, &ok);
    if (!ok) {
        return QString();
    }
    const QString clipHash = hash();
    if (clipHash.isEmpty()) {
        return QString();
    }
    QString audioPath = thumbFolder.absoluteFilePath(clipHash);
    audioPath.append(QLatin1Char('_') + QString::number(stream));
    int roundedFps = (int)pCore->getCurrentFps();
    audioPath.append(QStringLiteral("_%1_audio.png").arg(roundedFps));
    return audioPath;
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

void ProjectClip::registerService(std::weak_ptr<TimelineModel> timeline, int clipId, const std::shared_ptr<Mlt::Producer> &service, bool forceRegister)
{
    if (!service->is_cut() || forceRegister) {
        int hasAudio = service->get_int("set.test_audio") == 0;
        int hasVideo = service->get_int("set.test_image") == 0;
        if (hasVideo && m_videoProducers.count(clipId) == 0) {
            // This is an undo producer, register it!
            m_videoProducers[clipId] = service;
            m_effectStack->addService(m_videoProducers[clipId]);
        } else if (hasAudio && m_audioProducers.count(clipId) == 0) {
            // This is an undo producer, register it!
            m_audioProducers[clipId] = service;
            m_effectStack->addService(m_audioProducers[clipId]);
        }
    }
    registerTimelineClip(std::move(timeline), clipId);
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
    qDebug() << " ** * DEREGISTERING TIMELINE CLIP: " << clipId;
    Q_ASSERT(m_registeredClips.count(clipId) > 0);
    m_registeredClips.erase(clipId);
    if (m_videoProducers.count(clipId) > 0) {
        m_effectStack->removeService(m_videoProducers[clipId]);
        m_videoProducers.erase(clipId);
    }
    if (m_audioProducers.count(clipId) > 0) {
        m_effectStack->removeService(m_audioProducers[clipId]);
        m_audioProducers.erase(clipId);
    }
    setRefCount((uint)m_registeredClips.size());
}

QList<int> ProjectClip::timelineInstances() const
{
    QList<int> ids;
    for (const auto &m_registeredClip : m_registeredClips) {
        ids.push_back(m_registeredClip.first);
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
            timeline->requestClipUngroup(clip.first, undo, redo);
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

void ProjectClip::updateTimelineClips(const QVector<int> &roles)
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

void ProjectClip::updateZones()
{
    int zonesCount = childCount();
    if (zonesCount == 0) {
        resetProducerProperty(QStringLiteral("kdenlive:clipzones"));
        return;
    }
    QJsonArray list;
    for (int i = 0; i < zonesCount; ++i) {
        std::shared_ptr<AbstractProjectItem> clip = std::static_pointer_cast<AbstractProjectItem>(child(i));
        if (clip) {
            QJsonObject currentZone;
            currentZone.insert(QLatin1String("name"), QJsonValue(clip->name()));
            QPoint zone = clip->zone();
            currentZone.insert(QLatin1String("in"), QJsonValue(zone.x()));
            currentZone.insert(QLatin1String("out"), QJsonValue(zone.y()));
            if (clip->rating() > 0) {
                currentZone.insert(QLatin1String("rating"), QJsonValue((int)clip->rating()));
            }
            if (!clip->tags().isEmpty()) {
                currentZone.insert(QLatin1String("tags"), QJsonValue(clip->tags()));
            }
            list.push_back(currentZone);
        }
    }
    QJsonDocument json(list);
    setProducerProperty(QStringLiteral("kdenlive:clipzones"), QString(json.toJson()));
}


void ProjectClip::getThumbFromPercent(int percent)
{
    // extract a maximum of 30 frames for bin preview
    int duration = getFramePlaytime();
    int steps = qCeil(qMax(pCore->getCurrentFps(), (double)duration / 30));
    int framePos = duration * percent / 100;
    framePos -= framePos%steps;
    if (ThumbnailCache::get()->hasThumbnail(m_binId, framePos)) {
        setThumbnail(ThumbnailCache::get()->getThumbnail(m_binId, framePos));
    } else {
        // Generate percent thumbs
        int id;
        if (!pCore->jobManager()->hasPendingJob(m_binId, AbstractClipJob::CACHEJOB, &id)) {
            emit pCore->jobManager()->startJob<CacheJob>({m_binId}, -1, QString());
        }
    }
}

void ProjectClip::setRating(uint rating)
{
    AbstractProjectItem::setRating(rating);
    setProducerProperty(QStringLiteral("kdenlive:rating"), (int) rating);
    pCore->currentDoc()->setModified(true);
}

const QVector <uint8_t> ProjectClip::audioFrameCache(int stream)
{
    QVector <uint8_t> audioLevels;
    if (stream == -1) {
        if (m_audioInfo) {
            stream = m_audioInfo->ffmpeg_audio_index();
        } else {
            return audioLevels;
        }
    }
    QString key = QString("%1:%2").arg(m_binId).arg(stream);
    QByteArray audioData;
    if (pCore->audioThumbCache.find(key, &audioData)) {
        QDataStream in(audioData);
        in >> audioLevels;
        return audioLevels;
    }
    // convert cached image
    const QString cachePath = getAudioThumbPath(stream);
    // checking for cached thumbs
    QImage image(cachePath);
    if (!image.isNull()) {
        int channels = m_audioInfo->channelsForStream(stream);
        int n = image.width() * image.height();
        for (int i = 0; i < n; i++) {
            QRgb p = image.pixel(i / channels, i % channels);
            audioLevels << (uint8_t)qRed(p);
            audioLevels << (uint8_t)qGreen(p);
            audioLevels << (uint8_t)qBlue(p);
            audioLevels << (uint8_t)qAlpha(p);
        }
        // populate vector
        QDataStream st(&audioData, QIODevice::WriteOnly);
        st << audioLevels;
        pCore->audioThumbCache.insert(key, audioData);
    }
    return audioLevels;
}

void ProjectClip::setClipStatus(AbstractProjectItem::CLIPSTATUS status)
{
    AbstractProjectItem::setClipStatus(status);
    if (auto ptr = m_model.lock()) {
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()),
                                                                       AbstractProjectItem::IconOverlay);
    }
}

void ProjectClip::renameAudioStream(int id, QString name)
{
    if (m_audioInfo) {
        m_audioInfo->renameStream(id, name);
        QString prop = QString("kdenlive:streamname.%1").arg(id);
        m_masterProducer->set(prop.toUtf8().constData(), name.toUtf8().constData());
        if (m_audioInfo->activeStreams().keys().contains(id)) {
            pCore->bin()->updateTargets(clipId());
        }
        pCore->bin()->reloadMonitorStreamIfActive(clipId());
    }
}

void ProjectClip::requestAddStreamEffect(int streamIndex, const QString effectName)
{
    QStringList readEffects = m_streamEffects.value(streamIndex);
    QString oldEffect;
    // Remove effect if present (parameters might have changed
    for (const QString &effect : qAsConst(readEffects)) {
        if (effect == effectName || effect.startsWith(effectName + QStringLiteral(" "))) {
            oldEffect = effect;
            break;
        }
    }
    Fun redo = [this, streamIndex, effectName]() {
        addAudioStreamEffect(streamIndex, effectName);
        emit updateStreamInfo(streamIndex);
        return true; };
    Fun undo = [this, streamIndex, effectName, oldEffect]() {
        if (!oldEffect.isEmpty()) {
            // restore previous parameter value
            addAudioStreamEffect(streamIndex, oldEffect);
        } else {
            removeAudioStreamEffect(streamIndex, effectName);
        }
        emit updateStreamInfo(streamIndex);
        return true;
    };
    addAudioStreamEffect(streamIndex, effectName);
    pCore->pushUndo(undo, redo, i18n("Add stream effect"));
}

void ProjectClip::requestRemoveStreamEffect(int streamIndex, const QString effectName)
{
    QStringList readEffects = m_streamEffects.value(streamIndex);
    QString oldEffect = effectName;
    // Remove effect if present (parameters might have changed
    for (const QString &effect : qAsConst(readEffects)) {
        if (effect == effectName || effect.startsWith(effectName + QStringLiteral(" "))) {
            oldEffect = effect;
            break;
        }
    }
    Fun undo = [this, streamIndex, effectName, oldEffect]() {
        addAudioStreamEffect(streamIndex, oldEffect);
        emit updateStreamInfo(streamIndex);
        return true; };
    Fun redo = [this, streamIndex, effectName]() {
        removeAudioStreamEffect(streamIndex, effectName);
        emit updateStreamInfo(streamIndex);
        return true; };
    removeAudioStreamEffect(streamIndex, effectName);
    pCore->pushUndo(undo, redo, i18n("Remove stream effect"));
}

void ProjectClip::addAudioStreamEffect(int streamIndex, const QString effectName)
{
    QString addedEffectName;
    QMap <QString, QString> effectParams;
    if (effectName.contains(QLatin1Char(' '))) {
        // effect has parameters
        QStringList params = effectName.split(QLatin1Char(' '));
        addedEffectName = params.takeFirst();
        for (const QString &p : qAsConst(params)) {
            QStringList paramValue = p.split(QLatin1Char('='));
            if (paramValue.size() == 2) {
                effectParams.insert(paramValue.at(0), paramValue.at(1));
            }
        }
    } else {
        addedEffectName = effectName;
    }
    QStringList effects;
    if (m_streamEffects.contains(streamIndex)) {
        QStringList readEffects = m_streamEffects.value(streamIndex);
        // Remove effect if present (parameters might have changed
        for (const QString &effect : qAsConst(readEffects)) {
            if (effect == addedEffectName || effect.startsWith(addedEffectName + QStringLiteral(" "))) {
                continue;
            }
            effects << effect;
        }
        effects << effectName;
    } else {
        effects = QStringList({effectName});
    }
    m_streamEffects.insert(streamIndex, effects);
    setProducerProperty(QString("kdenlive:stream:%1").arg(streamIndex), effects.join(QLatin1Char('#')));
    for (auto &p : m_audioProducers) {
        int stream = p.first / 100;
        if (stream == streamIndex) {
            Mlt::Filter filt(*p.second->profile(), addedEffectName.toUtf8().constData());
            if (filt.is_valid()) {
                // Add stream effect markup
                filt.set("kdenlive:stream", 1);
                // Set parameters
                QMapIterator<QString, QString> i(effectParams);
                while (i.hasNext()) {
                    i.next();
                    filt.set(i.key().toUtf8().constData(), i.value().toUtf8().constData());
                }
                p.second->attach(filt);
            }
        }
    }
}

void ProjectClip::removeAudioStreamEffect(int streamIndex, QString effectName)
{
    QStringList effects;
    if (effectName.contains(QLatin1Char(' '))) {
        effectName = effectName.section(QLatin1Char(' '), 0, 0);
    }
    if (m_streamEffects.contains(streamIndex)) {
        QStringList readEffects = m_streamEffects.value(streamIndex);
        // Remove effect if present (parameters might have changed
        for (const QString &effect : qAsConst(readEffects)) {
            if (effect == effectName || effect.startsWith(effectName + QStringLiteral(" "))) {
                continue;
            }
            effects << effect;
        }
        if (effects.isEmpty()) {
            m_streamEffects.remove(streamIndex);
            resetProducerProperty(QString("kdenlive:stream:%1").arg(streamIndex));
        } else {
            m_streamEffects.insert(streamIndex, effects);
            setProducerProperty(QString("kdenlive:stream:%1").arg(streamIndex), effects.join(QLatin1Char('#')));
        }
    } else {
        // No effects for this stream, this is not expected, abort
        return;
    }
    for (auto &p : m_audioProducers) {
        int stream = p.first / 100;
        if (stream == streamIndex) {
            int max = p.second->filter_count();
            for (int i = 0; i < max; i++) {
                std::shared_ptr<Mlt::Filter> fl(p.second->filter(i));
                if (!fl->is_valid()) {
                    continue;
                }
                if (fl->get_int("kdenlive:stream") != 1)  {
                    // This is not an audio stream effect
                    continue;
                }
                if (fl->get("mlt_service") == effectName) {
                    p.second->detach(*fl.get());
                    break;
                }
            }
        }
    }
}

QStringList ProjectClip::getAudioStreamEffect(int streamIndex) const
{
    QStringList effects;
    if (m_streamEffects.contains(streamIndex)) {
        effects = m_streamEffects.value(streamIndex);
    }
    return effects;
}
