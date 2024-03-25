/*
SPDX-FileCopyrightText: 2012 Till Theato <root@ttill.de>
SPDX-FileCopyrightText: 2014 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "projectclip.h"
#include "audio/audioInfo.h"
#include "bin.h"
#include "clipcreator.hpp"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "doc/kthumb.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "jobs/audiolevelstask.h"
#include "jobs/cachetask.h"
#include "jobs/cliploadtask.h"
#include "jobs/proxytask.h"
#include "kdenlivesettings.h"
#include "lib/audio/audioStreamInfo.h"
#include "macros.hpp"
#include "mltcontroller/clippropertiescontroller.h"
#include "model/markerlistmodel.hpp"
#include "model/markersortmodel.h"
#include "profiles/profilemodel.hpp"
#include "project/projectmanager.h"
#include "projectfolder.h"
#include "projectitemmodel.h"
#include "projectsubclip.h"
#include "timeline2/model/snapmodel.hpp"
#include "utils/thumbnailcache.hpp"
#include "utils/timecode.h"
#include "xml/xml.hpp"

#include "kdenlive_debug.h"
#include "utils/KMessageBox_KdenliveCompat.h"
#include <KIO/RenameDialog>
#include <KImageCache>
#include <KLocalizedString>
#include <KMessageBox>
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDomElement>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QPainter>
#include <QProcess>
#include <QtMath>

#ifdef CRASH_AUTO_TEST
#include "logger.hpp"
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
#endif

ProjectClip::ProjectClip(const QString &id, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model, std::shared_ptr<Mlt::Producer> &producer)
    : AbstractProjectItem(AbstractProjectItem::ClipItem, id, model)
    , ClipController(id, producer)
    , isReloading(false)
    , m_resetTimelineOccurences(false)
    , m_uuid(QUuid::createUuid())
{
    m_markerModel = std::make_shared<MarkerListModel>(id, pCore->projectManager()->undoStack());
    m_markerFilterModel.reset(new MarkerSortModel(this));
    m_markerFilterModel->setSourceModel(m_markerModel.get());
    m_markerFilterModel->setSortRole(MarkerListModel::PosRole);
    m_markerFilterModel->sort(0, Qt::AscendingOrder);
    if (m_masterProducer->get_int("_placeholder") == 1) {
        m_clipStatus = FileStatus::StatusMissing;
    } else if (m_masterProducer->get_int("_missingsource") == 1) {
        m_clipStatus = FileStatus::StatusProxyOnly;
    } else if (m_usesProxy) {
        m_clipStatus = FileStatus::StatusProxy;
    } else {
        m_clipStatus = FileStatus::StatusReady;
    }
    m_name = clipName();
    m_duration = getStringDuration();
    m_inPoint = 0;
    m_outPoint = 0;
    m_date = date;
    updateDescription();
    if (m_clipType == ClipType::Audio) {
        m_thumbnail = QIcon::fromTheme(QStringLiteral("audio-x-generic"));
    } else {
        if (m_clipType == ClipType::Timeline) {
            // Initialize path for thumbnails playlist
            m_sequenceUuid = QUuid(m_masterProducer->get("kdenlive:uuid"));
            if (model->hasSequenceId(m_sequenceUuid)) {
                // We already have a sequence with this uuid, this is probably a duplicate, update uuid
                const QUuid prevUuid = m_sequenceUuid;
                m_sequenceUuid = QUuid::createUuid();
                m_masterProducer->set("kdenlive:uuid", m_sequenceUuid.toString().toUtf8().constData());
                m_masterProducer->parent().set("kdenlive:uuid", m_sequenceUuid.toString().toUtf8().constData());
                const QString subValue(m_masterProducer->get("kdenlive:sequenceproperties.subtitlesList"));

                if (!subValue.isEmpty()) {
                    int ix = m_masterProducer->get_int("kdenlive:sequenceproperties.kdenlive:activeSubtitleIndex");
                    pCore->currentDoc()->setSequenceProperty(m_sequenceUuid, QStringLiteral("kdenlive:activeSubtitleIndex"), QString::number(ix));
                    pCore->currentDoc()->duplicateSequenceProperty(m_sequenceUuid, prevUuid, subValue);
                }
            }
            m_sequenceThumbFile.setFileTemplate(QDir::temp().absoluteFilePath(QStringLiteral("thumbs-%1-XXXXXX.mlt").arg(m_binId)));
        }
        m_thumbnail = thumb;
    }
    // Make sure we have a hash for this clip
    hash();
    m_boundaryTimer.setSingleShot(true);
    m_boundaryTimer.setInterval(500);
    if (hasLimitedDuration()) {
        connect(&m_boundaryTimer, &QTimer::timeout, this, &ProjectClip::refreshBounds);
    }
    connect(m_markerModel.get(), &MarkerListModel::modelChanged, this,
            [&]() { setProducerProperty(QStringLiteral("kdenlive:markers"), m_markerModel->toJson()); });
    QString markers = getProducerProperty(QStringLiteral("kdenlive:markers"));
    if (!markers.isEmpty()) {
        QMetaObject::invokeMethod(m_markerModel.get(), "importFromJson", Qt::QueuedConnection, Q_ARG(QString, markers), Q_ARG(bool, true), Q_ARG(bool, false));
    }
    setTags(getProducerProperty(QStringLiteral("kdenlive:tags")));
    AbstractProjectItem::setRating(uint(getProducerIntProperty(QStringLiteral("kdenlive:rating"))));
    connectEffectStack();
    // Timeline clip thumbs will be generated later after the tractor has been updated
    if (m_clipType != ClipType::Timeline &&
        (m_clipStatus == FileStatus::StatusProxy || m_clipStatus == FileStatus::StatusReady || m_clipStatus == FileStatus::StatusProxyOnly)) {
        // Generate clip thumbnail
        ObjectId oid(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid());
        ClipLoadTask::start(oid, QDomElement(), true, -1, -1, this);
        // Generate audio thumbnail
        if (KdenliveSettings::audiothumbnails() &&
            (m_clipType == ClipType::AV || m_clipType == ClipType::Audio || (m_hasAudio && m_clipType != ClipType::Timeline))) {
            AudioLevelsTask::start(oid, this, false);
        }
    }
}

// static
std::shared_ptr<ProjectClip> ProjectClip::construct(const QString &id, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model,
                                                    std::shared_ptr<Mlt::Producer> &producer)
{
    std::shared_ptr<ProjectClip> self(new ProjectClip(id, thumb, model, producer));
    baseFinishConstruct(self);
    QMetaObject::invokeMethod(model.get(), "loadSubClips", Qt::QueuedConnection, Q_ARG(QString, id),
                              Q_ARG(QString, self->getProducerProperty(QStringLiteral("kdenlive:clipzones"))), Q_ARG(bool, false));
    return self;
}

void ProjectClip::importEffects(const std::shared_ptr<Mlt::Producer> &producer, const QString &originalDecimalPoint)
{
    m_effectStack->importEffects(producer, PlaylistState::Disabled, true, originalDecimalPoint);
}

ProjectClip::ProjectClip(const QString &id, const QDomElement &description, const QIcon &thumb, const std::shared_ptr<ProjectItemModel> &model)
    : AbstractProjectItem(AbstractProjectItem::ClipItem, id, model)
    , ClipController(id)
    , isReloading(false)
    , m_resetTimelineOccurences(false)
    , m_uuid(QUuid::createUuid())
{
    m_clipStatus = FileStatus::StatusWaiting;
    m_thumbnail = thumb;
    if (description.hasAttribute(QStringLiteral("type"))) {
        m_clipType = ClipType::ProducerType(description.attribute(QStringLiteral("type")).toInt());
        if (m_clipType == ClipType::Audio) {
            m_thumbnail = QIcon::fromTheme(QStringLiteral("audio-x-generic"));
        }
    }

    m_markerModel = std::make_shared<MarkerListModel>(m_binId, pCore->projectManager()->undoStack());
    m_markerFilterModel.reset(new MarkerSortModel(this));
    m_markerFilterModel->setSourceModel(m_markerModel.get());
    m_markerFilterModel->setSortRole(MarkerListModel::PosRole);
    m_markerFilterModel->sort(0, Qt::AscendingOrder);
    if (m_clipType == ClipType::Timeline) {
        m_sequenceUuid = QUuid(getXmlProperty(description, QStringLiteral("kdenlive:uuid")));
    }

    const QString proxy = getXmlProperty(description, QStringLiteral("kdenlive:proxy"));
    if (proxy.length() > 3) {
        m_temporaryUrl = getXmlProperty(description, QStringLiteral("kdenlive:originalurl"));
    }
    if (m_temporaryUrl.isEmpty()) {
        m_temporaryUrl = getXmlProperty(description, QStringLiteral("resource"));
    }
    if (m_name.isEmpty()) {
        QString clipName = getXmlProperty(description, QStringLiteral("kdenlive:clipname"));
        if (!clipName.isEmpty()) {
            m_name = clipName;
        } else if (!m_temporaryUrl.isEmpty() && m_clipType != ClipType::Timeline) {
            m_name = QFileInfo(m_temporaryUrl).fileName();
        } else {
            m_name = i18n("Unnamed");
        }
    }
    m_date = QFileInfo(m_temporaryUrl).lastModified();
    m_boundaryTimer.setSingleShot(true);
    m_boundaryTimer.setInterval(500);
    connect(m_markerModel.get(), &MarkerListModel::modelChanged, this,
            [&]() { setProducerProperty(QStringLiteral("kdenlive:markers"), m_markerModel->toJson()); });
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
    if (pCore->currentDoc()->closing) {
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
    }
}

std::shared_ptr<MarkerListModel> ProjectClip::markerModel()
{
    return m_markerModel;
}

void ProjectClip::connectEffectStack()
{
    connect(m_effectStack.get(), &EffectStackModel::dataChanged, this, [&]() {
        if (auto ptr = m_model.lock()) {
            std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()),
                                                                           {AbstractProjectItem::IconOverlay});
        }
    });
}

QString ProjectClip::getToolTip() const
{
    if (m_clipType == ClipType::Color && m_path.contains(QLatin1Char('/'))) {
        return m_path.section(QLatin1Char('/'), -1);
    }
    if (m_clipType == ClipType::Timeline) {
        return i18n("Timeline sequence");
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

void ProjectClip::updateAudioThumbnail(bool cachedThumb)
{
    Q_EMIT audioThumbReady();
    if (m_clipType == ClipType::Audio) {
        QImage thumb = ThumbnailCache::get()->getThumbnail(m_binId, 0);
        if (thumb.isNull() && !pCore->taskManager.hasPendingJob(ObjectId(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid()), AbstractTask::AUDIOTHUMBJOB)) {
            int iconHeight = int(QFontInfo(qApp->font()).pixelSize() * 3.5);
            QImage img(QSize(int(iconHeight * pCore->getCurrentDar()), iconHeight), QImage::Format_ARGB32);
            img.fill(Qt::darkGray);
            QMap<int, QString> streams = audioInfo()->streams();
            QMap<int, int> channelsList = audioInfo()->streamChannels();
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
                    double channelHeight = double(streamHeight) / channels;
                    const QVector<uint8_t> audioLevels = audioFrameCache(st.key());
                    qreal indicesPrPixel = qreal(audioLevels.length()) / img.width();
                    int idx;
                    for (int channel = 0; channel < channels; channel++) {
                        double y = (streamHeight * streamCount) + (channel * channelHeight) + channelHeight / 2;
                        for (int i = 0; i <= img.width(); i++) {
                            idx = int(ceil(i * indicesPrPixel));
                            idx += idx % channels;
                            idx += channel;
                            if (idx >= audioLevels.length() || idx < 0) {
                                break;
                            }
                            double level = audioLevels.at(idx) * channelHeight / 510.; // divide height by 510 (2*255) to get height
                            painter.drawLine(i, int(y - level), i, int(y + level));
                        }
                    }
                    streamCount++;
                }
            }
            thumb = img;
            // Cache thumbnail
            ThumbnailCache::get()->storeThumbnail(m_binId, 0, thumb, true);
        }
        if (!thumb.isNull()) {
            setThumbnail(thumb, -1, -1);
        }
    }
    if (!KdenliveSettings::audiothumbnails()) {
        return;
    }
    m_audioThumbCreated = true;
    if (!cachedThumb) {
        // Audio was just created
        updateTimelineClips({TimelineModel::ReloadAudioThumbRole});
    }
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

bool ProjectClip::hasUrl() const
{
    if (m_clipType == ClipType::Color || m_clipType == ClipType::Unknown || m_clipType == ClipType::Timeline) {
        return false;
    }
    return !clipUrl().isEmpty();
}

const QString ProjectClip::url() const
{
    return clipUrl();
}

const QSize ProjectClip::frameSize() const
{
    return getFrameSize();
}

GenTime ProjectClip::duration() const
{
    return getPlaytime();
}

size_t ProjectClip::frameDuration() const
{
    return size_t(getFramePlaytime());
}

void ProjectClip::resetSequenceThumbnails()
{
    QMutexLocker lk(&m_thumbMutex);
    pCore->taskManager.discardJobs(ObjectId(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid()), AbstractTask::LOADJOB, true);
    m_thumbXml.clear();
    ThumbnailCache::get()->invalidateThumbsForClip(m_binId);
    // Force refeshing thumbs producer
    lk.unlock();
    m_uuid = QUuid::createUuid();
    // Clips will be replanted so no need to refresh thumbs
    // updateTimelineClips({TimelineModel::ClipThumbRole});
}

void ProjectClip::reloadProducer(bool refreshOnly, bool isProxy, bool forceAudioReload)
{
    // we find if there are some loading job on that clip
    QMutexLocker lock(&m_thumbMutex);
    ObjectId oid(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid());
    if (refreshOnly) {
        // In that case, we only want a new thumbnail.
        // We thus set up a thumb job. We must make sure that there is no pending LOADJOB
        // Clear cache first
        ThumbnailCache::get()->invalidateThumbsForClip(m_binId);
        pCore->taskManager.discardJobs(oid, AbstractTask::LOADJOB, true);
        pCore->taskManager.discardJobs(oid, AbstractTask::CACHEJOB);
        m_thumbXml.clear();
        // Reset uuid to enforce reloading thumbnails from qml cache
        m_uuid = QUuid::createUuid();
        updateTimelineClips({TimelineModel::ClipThumbRole, TimelineModel::ResourceRole});
        ClipLoadTask::start(oid, QDomElement(), true, -1, -1, this);
    } else {
        // If another load job is running?
        pCore->taskManager.discardJobs(oid, AbstractTask::LOADJOB, true);
        pCore->taskManager.discardJobs(oid, AbstractTask::CACHEJOB);
        if (QFile::exists(m_path) && (!isProxy && !hasProxy()) && m_properties) {
            clearBackupProperties();
        }
        QDomDocument doc;
        QDomElement xml;
        QString resource;
        if (m_properties) {
            resource = m_properties->get("resource");
        }
        if (m_service.isEmpty() && !resource.isEmpty()) {
            xml = ClipCreator::getXmlFromUrl(resource).documentElement();
        } else {
            xml = toXml(doc);
        }
        if (!xml.isNull()) {
            bool hashChanged = false;
            m_thumbXml.clear();
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
            m_audioThumbCreated = false;
            isReloading = true;
            // Reset uuid to enforce reloading thumbnails from qml cache
            m_uuid = QUuid::createUuid();
            if (forceAudioReload || (!isProxy && hashChanged)) {
                discardAudioThumb();
            }
            if (m_clipStatus != FileStatus::StatusMissing) {
                m_clipStatus = FileStatus::StatusWaiting;
            }
            m_thumbXml.clear();
            ClipLoadTask::start(oid, xml, false, -1, -1, this);
        }
    }
}

QDomElement ProjectClip::toXml(QDomDocument &document, bool includeMeta, bool includeProfile)
{
    getProducerXML(document, includeMeta, includeProfile);
    QDomElement prod;
    QString tag = document.documentElement().tagName();
    if (tag == QLatin1String("producer") || tag == QLatin1String("chain")) {
        prod = document.documentElement();
    } else {
        // Check if this is a sequence clip
        if (m_clipType == ClipType::Timeline) {
            prod = document.documentElement();
            prod.setAttribute(QStringLiteral("kdenlive:id"), m_binId);
            prod.setAttribute(QStringLiteral("kdenlive:producer_type"), ClipType::Timeline);
            prod.setAttribute(QStringLiteral("kdenlive:uuid"), m_sequenceUuid.toString());
            prod.setAttribute(QStringLiteral("kdenlive:duration"), QString::number(frameDuration()));
            prod.setAttribute(QStringLiteral("kdenlive:clipname"), clipName());
        } else {
            prod = document.documentElement().firstChildElement(QStringLiteral("chain"));
            if (prod.isNull()) {
                prod = document.documentElement().firstChildElement(QStringLiteral("producer"));
            }
        }
    }
    if (m_clipType != ClipType::Unknown) {
        prod.setAttribute(QStringLiteral("type"), int(m_clipType));
    }
    return prod;
}

void ProjectClip::setThumbnail(const QImage &img, int in, int out, bool inCache)
{
    if (img.isNull()) {
        return;
    }
    if (in > -1) {
        std::shared_ptr<ProjectSubClip> sub = getSubClip(in, out);
        if (sub) {
            sub->setThumbnail(img);
        }
        return;
    }
    QPixmap thumb = roundedPixmap(QPixmap::fromImage(img));
    if (hasProxy() && !thumb.isNull()) {
        // Overlay proxy icon
        QPainter p(&thumb);
        QColor c(220, 220, 10, 200);
        QRect r(0, 0, int(thumb.height() / 2.5), int(thumb.height() / 2.5));
        p.fillRect(r, c);
        QFont font = p.font();
        font.setPixelSize(r.height());
        font.setBold(true);
        p.setFont(font);
        p.setPen(Qt::black);
        p.drawText(r, Qt::AlignCenter, i18nc("@label The first letter of Proxy, used as abbreviation", "P"));
    }
    m_thumbnail = QIcon(thumb);
    if (auto ptr = m_model.lock()) {
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()),
                                                                       {AbstractProjectItem::DataThumbnail});
    }
    if (!inCache && (m_clipType == ClipType::Text || m_clipType == ClipType::TextTemplate)) {
        // Title clips always use the same thumb as bin, refresh
        updateTimelineClips({TimelineModel::ClipThumbRole});
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

bool ProjectClip::setProducer(std::shared_ptr<Mlt::Producer> producer, bool generateThumb, bool clearTrackProducers)
{
    qDebug() << "################### ProjectClip::setproducer #################";
    // Discard running tasks for this producer
    QMutexLocker locker(&m_producerMutex);
    FileStatus::ClipStatus currentStatus = m_clipStatus;
    if (producer->property_exists("_reloadName")) {
        m_name.clear();
    }
    bool buildProxy = producer->property_exists("_replaceproxy") && !pCore->currentDoc()->loading;
    updateProducer(producer);
    producer.reset();
    pCore->taskManager.discardJobs(ObjectId(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid()), AbstractTask::LOADJOB);
    // Abort thumbnail tasks if any
    m_thumbMutex.lock();
    m_thumbXml.clear();
    m_thumbMutex.unlock();

    isReloading = false;
    // Make sure we have a hash for this clip
    getFileHash();
    Q_EMIT producerChanged(m_binId, m_clipType == ClipType::Timeline ? m_masterProducer->parent() : *m_masterProducer.get());
    connectEffectStack();

    // Update info
    if (m_name.isEmpty()) {
        m_name = clipName();
    }
    QVector<int> updateRoles;
    if (m_date != date) {
        m_date = date;
        updateRoles << AbstractProjectItem::DataDate;
    }
    updateDescription();
    m_temporaryUrl.clear();
    if (m_clipType == ClipType::Audio) {
        m_thumbnail = QIcon::fromTheme(QStringLiteral("audio-x-generic"));
    } else if (m_clipType == ClipType::Image) {
        if (m_masterProducer->get_int("meta.media.width") < 8 || m_masterProducer->get_int("meta.media.height") < 8) {
            KMessageBox::information(QApplication::activeWindow(),
                                     i18n("Image dimension smaller than 8 pixels.\nThis is not correctly supported by our video framework."));
        }
    } else if (m_clipType == ClipType::Timeline) {
        if (m_sequenceUuid.isNull()) {
            m_sequenceUuid = QUuid::createUuid();
            setProducerProperty(QStringLiteral("kdenlive:uuid"), m_sequenceUuid.toString());
        }
    }
    m_duration = getStringDuration();
    m_clipStatus = m_usesProxy ? FileStatus::StatusProxy : FileStatus::StatusReady;
    locker.unlock();
    if (m_clipStatus != currentStatus) {
        updateRoles << AbstractProjectItem::ClipStatus << AbstractProjectItem::IconOverlay;
        updateTimelineClips({TimelineModel::StatusRole, TimelineModel::ClipThumbRole});
    }
    setTags(getProducerProperty(QStringLiteral("kdenlive:tags")));
    AbstractProjectItem::setRating(uint(getProducerIntProperty(QStringLiteral("kdenlive:rating"))));
    if (auto ptr = m_model.lock()) {
        updateRoles << AbstractProjectItem::DataDuration;
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()), updateRoles);
        std::static_pointer_cast<ProjectItemModel>(ptr)->updateWatcher(std::static_pointer_cast<ProjectClip>(shared_from_this()));
        if (currentStatus == FileStatus::StatusMissing) {
            std::static_pointer_cast<ProjectItemModel>(ptr)->missingClipTimer.start();
        }
    }
    // set parent again (some info need to be stored in producer)
    updateParent(parentItem().lock());
    if (generateThumb && m_clipType != ClipType::Audio) {
        // Generate video thumb
        ClipLoadTask::start(ObjectId(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid()), QDomElement(), true, -1, -1, this);
    }
    if (KdenliveSettings::audiothumbnails() &&
        (m_clipType == ClipType::AV || m_clipType == ClipType::Audio || (m_hasAudio && m_clipType != ClipType::Timeline))) {
        AudioLevelsTask::start(ObjectId(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid()), this, false);
    }
    if (pCore->bin()) {
        pCore->bin()->reloadMonitorIfActive(clipId());
    }
    if (clearTrackProducers) {
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
        if (m_timewarpProducers.size() > 0) {
            if (m_clipType == ClipType::Timeline) {
                bool ok;
                QDir sequenceFolder = pCore->currentDoc()->getCacheDir(CacheTmpWorkFiles, &ok);
                if (ok) {
                    QString resource = sequenceFolder.absoluteFilePath(QString("sequence-%1.mlt").arg(m_sequenceUuid.toString()));
                    QFile::remove(resource);
                }
            }
        }
        m_timewarpProducers.clear();
    }
    Q_EMIT refreshPropertiesPanel();
    if (hasLimitedDuration()) {
        connect(&m_boundaryTimer, &QTimer::timeout, this, &ProjectClip::refreshBounds);
    } else {
        disconnect(&m_boundaryTimer, &QTimer::timeout, this, &ProjectClip::refreshBounds);
    }
    replaceInTimeline();
    updateTimelineClips({TimelineModel::IsProxyRole});
    bool generateProxy = false;
    std::shared_ptr<ProjectClip> clipToProxy = nullptr;
    if (buildProxy ||
        (!m_usesProxy && pCore->currentDoc()->useProxy() && pCore->currentDoc()->getDocumentProperty(QStringLiteral("generateproxy")).toInt() == 1)) {
        // automatic proxy generation enabled
        if (m_clipType == ClipType::Image && pCore->currentDoc()->getDocumentProperty(QStringLiteral("generateimageproxy")).toInt() == 1) {
            if (getProducerIntProperty(QStringLiteral("meta.media.width")) >= KdenliveSettings::proxyimageminsize() &&
                getProducerProperty(QStringLiteral("kdenlive:proxy")) == QLatin1String()) {
                clipToProxy = std::static_pointer_cast<ProjectClip>(shared_from_this());
            }
        } else if ((buildProxy || pCore->currentDoc()->getDocumentProperty(QStringLiteral("generateproxy")).toInt() == 1) &&
                   (m_clipType == ClipType::AV || m_clipType == ClipType::Video) && getProducerProperty(QStringLiteral("kdenlive:proxy")) == QLatin1String()) {
            if (m_hasVideo && (buildProxy || getProducerIntProperty(QStringLiteral("meta.media.width")) >= KdenliveSettings::proxyminsize())) {
                clipToProxy = std::static_pointer_cast<ProjectClip>(shared_from_this());
            }
        } else if (m_clipType == ClipType::Playlist && pCore->getCurrentFrameDisplaySize().width() >= KdenliveSettings::proxyminsize() &&
                   getProducerProperty(QStringLiteral("kdenlive:proxy")) == QLatin1String()) {
            clipToProxy = std::static_pointer_cast<ProjectClip>(shared_from_this());
        }
        if (clipToProxy != nullptr) {
            generateProxy = true;
        }
    }
    if (!generateProxy && KdenliveSettings::hoverPreview() &&
        (m_clipType == ClipType::AV || m_clipType == ClipType::Video || m_clipType == ClipType::Playlist)) {
        QTimer::singleShot(1000, this, [this]() { CacheTask::start(ObjectId(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid()), 30, 0, 0, this); });
    }
    if (generateProxy) {
        QMetaObject::invokeMethod(pCore->currentDoc(), "slotProxyCurrentItem", Q_ARG(bool, true), Q_ARG(QList<std::shared_ptr<ProjectClip>>, {clipToProxy}),
                                  Q_ARG(bool, false));
    }
    return true;
}

// static
const QString ProjectClip::getOriginalFromProxy(QString proxyPath)
{
    QStringList externalParams = pCore->currentDoc()->getDocumentProperty(QStringLiteral("externalproxyparams")).split(QLatin1Char(';'));
    if (externalParams.count() >= 6) {
        QFileInfo info(proxyPath);
        QDir dir = info.absoluteDir();
        dir.cd(externalParams.at(3));
        QString fileName = info.fileName();
        bool matchFound = false;
        while (externalParams.count() >= 6) {
            if (fileName.startsWith(externalParams.at(1))) {
                matchFound = true;
                break;
            }
            externalParams = externalParams.mid(6);
        }
        if (matchFound) {
            fileName.remove(0, externalParams.at(1).size());
            fileName.prepend(externalParams.at(4));
            if (!externalParams.at(2).isEmpty()) {
                if (!fileName.endsWith(externalParams.at(2))) {
                    // File does not match, abort
                    return QString();
                }
                fileName.chop(externalParams.at(2).size());
            }
            fileName.append(externalParams.at(5));
            if (fileName != proxyPath && dir.exists(fileName)) {
                return dir.absoluteFilePath(fileName);
            }
        }
    }
    return QString();
}

// static
const QString ProjectClip::getProxyFromOriginal(QString originalPath)
{
    QStringList externalParams = pCore->currentDoc()->getDocumentProperty(QStringLiteral("externalproxyparams")).split(QLatin1Char(';'));
    if (externalParams.count() >= 6) {
        QFileInfo info(originalPath);
        QDir dir = info.absoluteDir();
        dir.cd(externalParams.at(0));
        QString fileName = info.fileName();
        bool matchFound = false;
        while (externalParams.count() >= 6) {
            if (fileName.startsWith(externalParams.at(4))) {
                matchFound = true;
                break;
            }
            externalParams = externalParams.mid(6);
        }
        if (matchFound) {
            fileName.remove(0, externalParams.at(4).size());
            fileName.prepend(externalParams.at(1));
            if (!externalParams.at(5).isEmpty()) {
                if (!fileName.endsWith(externalParams.at(5))) {
                    // File does not match, abort
                    return QString();
                }
                fileName.chop(externalParams.at(5).size());
            }
            fileName.append(externalParams.at(2));
            if (fileName != originalPath && dir.exists(fileName)) {
                return dir.absoluteFilePath(fileName);
            }
        }
    }
    return QString();
}

std::unique_ptr<Mlt::Producer> ProjectClip::getThumbProducer()
{
    if (clipType() == ClipType::Unknown || m_masterProducer == nullptr || m_clipStatus == FileStatus::StatusWaiting) {
        return nullptr;
    }
    QMutexLocker lock(&m_thumbMutex);
    std::unique_ptr<Mlt::Producer> thumbProd;
    if (!m_thumbXml.isEmpty()) {
        QMutexLocker lock(&pCore->xmlMutex);
        thumbProd.reset(new Mlt::Producer(pCore->thumbProfile(), "xml-string", m_thumbXml.constData()));
        return thumbProd;
    }
    if (KdenliveSettings::gpu_accel()) {
        // TODO: when the original producer changes, we must reload this thumb producer
        thumbProd = softClone(ClipController::getPassPropertiesList());
    } else if (m_clipType == ClipType::Timeline) {
        if (pCore->currentDoc()->loading) {
            return nullptr;
        }
        if (!m_sequenceThumbFile.isOpen() && !m_sequenceThumbFile.open()) {
            // Something went wrong
            qWarning() << "Cannot write to temporary file: " << m_sequenceThumbFile.fileName();
            return nullptr;
        }
        cloneProducerToFile(m_sequenceThumbFile.fileName(), true);
        QMutexLocker lock(&pCore->xmlMutex);
        thumbProd.reset(new Mlt::Producer(pCore->thumbProfile(), "consumer", m_sequenceThumbFile.fileName().toUtf8().constData()));
    } else {
        QString mltService = m_masterProducer->get("mlt_service");
        const QString mltResource = m_masterProducer->get("resource");
        if (mltService == QLatin1String("avformat")) {
            mltService = QStringLiteral("avformat-novalidate");
        }
        thumbProd.reset(new Mlt::Producer(pCore->thumbProfile(), mltService.toUtf8().constData(), mltResource.toUtf8().constData()));
    }
    if (thumbProd->is_valid()) {
        Mlt::Properties original(m_masterProducer->get_properties());
        Mlt::Properties cloneProps(thumbProd->get_properties());
        cloneProps.pass_list(original, ClipController::getPassPropertiesList());
        thumbProd->set("audio_index", -1);
        thumbProd->set("astream", -1);
        // Required to make get_playtime() return > 1
        thumbProd->set("out", thumbProd->get_length() - 1);
        Mlt::Filter scaler(pCore->thumbProfile(), "swscale");
        Mlt::Filter padder(pCore->thumbProfile(), "resize");
        Mlt::Filter converter(pCore->thumbProfile(), "avcolor_space");
        thumbProd->attach(scaler);
        thumbProd->attach(padder);
        thumbProd->attach(converter);
    }
    m_thumbXml = ClipController::producerXml(*thumbProd.get(), true, false);
    return thumbProd;
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

int ProjectClip::getRecordTime()
{
    if (m_masterProducer && (m_clipType == ClipType::AV || m_clipType == ClipType::Video || m_clipType == ClipType::Audio)) {
        int recTime = m_masterProducer->get_int("kdenlive:record_date");
        if (recTime > 0) {
            return recTime;
        }
        if (recTime < 0) {
            // Cannot read record date on this clip, abort
            return 0;
        }
        QString timecode = m_masterProducer->get("meta.attr.timecode.markup");
        // First try to get timecode from MLT metadata
        if (!timecode.isEmpty()) {
            // Timecode Format HH:MM:SS:FF
            // Check if we have a different fps
            double producerFps = m_masterProducer->get_double("meta.media.frame_rate_num") / m_masterProducer->get_double("meta.media.frame_rate_den");
            if (!qFuzzyCompare(producerFps, pCore->getCurrentFps())) {
                // Producer and project have a different fps
                bool ok;
                int frames = timecode.section(QLatin1Char(':'), -1).toInt(&ok);
                if (ok) {
                    frames *= int(pCore->getCurrentFps() / producerFps);
                    timecode.chop(2);
                    timecode.append(QString::number(frames).rightJustified(1, QChar('0')));
                }
            }
            recTime = int(1000 * pCore->timecode().getFrameCount(timecode) / pCore->getCurrentFps());
            m_masterProducer->set("kdenlive:record_date", recTime);
            return recTime;
        } else {
            if (KdenliveSettings::mediainfopath().isEmpty() || !QFileInfo::exists(KdenliveSettings::mediainfopath())) {
                // Try to find binary
                const QStringList mltpath({QFileInfo(KdenliveSettings::meltpath()).canonicalPath(), qApp->applicationDirPath()});
                QString mediainfopath = QStandardPaths::findExecutable(QStringLiteral("mediainfo"), mltpath);
                if (mediainfopath.isEmpty()) {
                    mediainfopath = QStandardPaths::findExecutable(QStringLiteral("mediainfo"));
                }
                if (!mediainfopath.isEmpty()) {
                    KdenliveSettings::setMediainfopath(mediainfopath);
                }
            }
            if (!KdenliveSettings::mediainfopath().isEmpty()) {
                // Getting the timecode was not successfull yet,
                // but mediainfo is available so try to get it with mediainfo
                QProcess extractInfo;
                extractInfo.start(KdenliveSettings::mediainfopath(), {url(), QStringLiteral("--output=XML")});
                extractInfo.waitForFinished();
                if (extractInfo.exitStatus() != QProcess::NormalExit || extractInfo.exitCode() != 0) {
                    KMessageBox::error(QApplication::activeWindow(),
                                       i18n("Cannot extract metadata from %1\n%2", url(), QString(extractInfo.readAllStandardError())));
                    return 0;
                }
                QDomDocument doc;
                doc.setContent(extractInfo.readAllStandardOutput());
                bool dateFormat = false;
                QDomNodeList nodes = doc.documentElement().elementsByTagName(QStringLiteral("TimeCode_FirstFrame"));
                if (nodes.isEmpty()) {
                    nodes = doc.documentElement().elementsByTagName(QStringLiteral("Recorded_Date"));
                    dateFormat = true;
                }
                if (!nodes.isEmpty()) {
                    // Parse recorded time (HH:MM:SS)
                    QString recInfo = nodes.at(0).toElement().text();
                    if (!recInfo.isEmpty()) {
                        if (dateFormat) {
                            if (recInfo.contains(QLatin1Char('+'))) {
                                recInfo = recInfo.section(QLatin1Char('+'), 0, 0);
                            } else if (recInfo.contains(QLatin1Char('-'))) {
                                recInfo = recInfo.section(QLatin1Char('-'), 0, 0);
                            }
                            QDateTime date = QDateTime::fromString(recInfo, "yyyy-MM-dd hh:mm:ss");
                            recTime = date.time().msecsSinceStartOfDay();
                        } else {
                            // Timecode Format HH:MM:SS:FF
                            // Check if we have a different fps
                            double producerFps =
                                m_masterProducer->get_double("meta.media.frame_rate_num") / m_masterProducer->get_double("meta.media.frame_rate_den");
                            if (!qFuzzyCompare(producerFps, pCore->getCurrentFps())) {
                                // Producer and project have a different fps
                                bool ok;
                                int frames = recInfo.section(QLatin1Char(':'), -1).toInt(&ok);
                                if (ok) {
                                    frames *= int(pCore->getCurrentFps() / producerFps);
                                    recInfo.chop(2);
                                    recInfo.append(QString::number(frames).rightJustified(1, QChar('0')));
                                }
                            }
                            recTime = int(1000 * pCore->timecode().getFrameCount(recInfo) / pCore->getCurrentFps());
                        }
                        m_masterProducer->set("kdenlive:record_date", recTime);
                        return recTime;
                    }
                }
                // set record date to -1 to avoid trying with mediainfo again and again
                m_masterProducer->set("kdenlive:record_date", -1);
                return 0;
            }
        }
    }
    return 0;
}

std::shared_ptr<Mlt::Producer> ProjectClip::getTimelineProducer(int trackId, int clipId, PlaylistState::ClipState state, int audioStream, double speed,
                                                                bool secondPlaylist, const TimeWarpInfo timeremapInfo)
{
    if (!m_masterProducer) {
        return nullptr;
    }
    if (qFuzzyCompare(speed, 1.0) && !timeremapInfo.enableRemap) {
        // we are requesting a normal speed producer
        bool byPassTrackProducer = false;
        if (trackId == -1 && (state != PlaylistState::AudioOnly || audioStream == m_masterProducer->get_int("audio_index"))) {
            byPassTrackProducer = true;
        }
        if (byPassTrackProducer ||
            (state == PlaylistState::VideoOnly && (m_clipType == ClipType::Color || m_clipType == ClipType::Image || m_clipType == ClipType::Text ||
                                                   m_clipType == ClipType::TextTemplate || m_clipType == ClipType::Qml))) {
            // Temporary copy, return clone of master
            int duration = m_masterProducer->time_to_frames(m_masterProducer->get("kdenlive:duration"));
            std::shared_ptr<Mlt::Producer> prod(m_masterProducer->cut(-1, duration > 0 ? duration - 1 : -1));
            if (m_clipType == ClipType::Timeline && m_masterProducer->property_exists("kdenlive:maxduration")) {
                prod->set("kdenlive:maxduration", m_masterProducer->get_int("kdenlive:maxduration"));
            }
            return prod;
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
            // second playlist producers use negative trackId
            if (secondPlaylist) {
                trackId = -trackId;
            }
            if (m_audioProducers.count(trackId) == 0) {
                if (m_clipType == ClipType::Timeline) {
                    std::shared_ptr<Mlt::Producer> prod(m_masterProducer->cut(0, -1));
                    m_audioProducers[trackId] = prod;
                } else {
                    m_audioProducers[trackId] = cloneProducer(true, true);
                }
                m_audioProducers[trackId]->set("set.test_audio", 0);
                m_audioProducers[trackId]->set("set.test_image", 1);
                if (m_streamEffects.contains(audioStream)) {
                    QStringList effects = m_streamEffects.value(audioStream);
                    for (const QString &effect : qAsConst(effects)) {
                        Mlt::Filter filt(m_audioProducers[trackId]->get_profile(), effect.toUtf8().constData());
                        if (filt.is_valid()) {
                            // Add stream effect markup
                            filt.set("kdenlive:stream", 1);
                            m_audioProducers[trackId]->attach(filt);
                        }
                    }
                }
                if (audioStream > -1) {
                    int newAudioStreamIndex = audioStreamIndex(audioStream);
                    if (newAudioStreamIndex > -1) {
                        /** If the audioStreamIndex is not found, for example when replacing a clip with another one using different indexes,
                        default to first audio stream */
                        m_audioProducers[trackId]->set("audio_index", audioStream);
                    } else {
                        newAudioStreamIndex = 0;
                    }
                    if (newAudioStreamIndex > audioStreamsCount() - 1) {
                        newAudioStreamIndex = 0;
                    }
                    m_audioProducers[trackId]->set("astream", newAudioStreamIndex);
                }
                m_effectStack->addService(m_audioProducers[trackId]);
            }
            std::shared_ptr<Mlt::Producer> prod(m_audioProducers[trackId]->cut());
            if (m_clipType == ClipType::Timeline && m_audioProducers[trackId]->parent().property_exists("kdenlive:maxduration")) {
                int max = m_audioProducers[trackId]->parent().get_int("kdenlive:maxduration");
                prod->set("kdenlive:maxduration", max);
                prod->set("length", max);
            }
            return prod;
        }
        if (m_audioProducers.count(trackId) > 0) {
            m_effectStack->removeService(m_audioProducers[trackId]);
            m_audioProducers.erase(trackId);
        }
        if (state == PlaylistState::VideoOnly) {
            // we return the video producer
            // We need to get an video producer, if none exists
            // second playlist producers use negative trackId
            if (secondPlaylist) {
                trackId = -trackId;
            }
            if (m_videoProducers.count(trackId) == 0) {
                if (m_clipType == ClipType::Timeline) {
                    std::shared_ptr<Mlt::Producer> prod(m_masterProducer->cut(0, -1));
                    m_videoProducers[trackId] = prod;
                } else {
                    m_videoProducers[trackId] = cloneProducer(true, true);
                }
                if (m_masterProducer->property_exists("kdenlive:maxduration")) {
                    m_videoProducers[trackId]->set("kdenlive:maxduration", m_masterProducer->get_int("kdenlive:maxduration"));
                }

                // Let audio enabled so that we can use audio visualization filters ?
                m_videoProducers[trackId]->set("set.test_audio", 1);
                m_videoProducers[trackId]->set("set.test_image", 0);
                m_effectStack->addService(m_videoProducers[trackId]);
            }
            int duration = m_masterProducer->time_to_frames(m_masterProducer->get("kdenlive:duration"));
            return std::shared_ptr<Mlt::Producer>(m_videoProducers[trackId]->cut(-1, duration > 0 ? duration - 1 : -1));
        }
        if (m_videoProducers.count(trackId) > 0) {
            m_effectStack->removeService(m_videoProducers[trackId]);
            m_videoProducers.erase(trackId);
        }
        Q_ASSERT(state == PlaylistState::Disabled);
        createDisabledMasterProducer();
        int duration = m_masterProducer->time_to_frames(m_masterProducer->get("kdenlive:duration"));
        std::shared_ptr<Mlt::Producer> prod(m_disabledProducer->cut(-1, duration > 0 ? duration - 1 : -1));
        if (m_clipType == ClipType::Timeline && m_videoProducers[trackId]->parent().property_exists("kdenlive:maxduration")) {
            int max = m_videoProducers[trackId]->parent().get_int("kdenlive:maxduration");
            prod->set("kdenlive:maxduration", max);
            prod->set("length", max);
        }
        return prod;
    }

    // For timewarp clips, we keep one separate producer for each clip.
    std::shared_ptr<Mlt::Producer> warpProducer;
    if (m_timewarpProducers.count(clipId) > 0) {
        // remove in all cases, we add it unconditionally anyways
        m_effectStack->removeService(m_timewarpProducers[clipId]);
        if (qFuzzyCompare(m_timewarpProducers[clipId]->get_double("warp_speed"), speed)) {
            // the producer we have is good, use it !
            warpProducer = m_timewarpProducers[clipId];
            qDebug() << "Reusing timewarp producer!";
        } else if (!timeremapInfo.timeMapData.isEmpty()) {
            // the producer we have is good, use it !
            warpProducer = m_timewarpProducers[clipId];
            qDebug() << "Reusing time remap producer for cid: " << clipId;
        } else {
            m_timewarpProducers.erase(clipId);
        }
    }
    if (!warpProducer) {
        QString resource(originalProducer()->get("resource"));
        if (resource.isEmpty() || resource == QLatin1String("<producer>")) {
            resource = m_service;
        }
        if (m_clipType == ClipType::Timeline) {
            // speed effects of sequence clips have to use an external mlt playslist file
            bool ok;
            QDir sequenceFolder = pCore->currentDoc()->getCacheDir(CacheTmpWorkFiles, &ok);
            if (!ok) {
                qWarning() << "Cannot write to cache folder: " << sequenceFolder.absolutePath();
                return nullptr;
            }
            resource = sequenceFolder.absoluteFilePath(QString("sequence-%1.mlt").arg(m_sequenceUuid.toString()));
            if (!QFileInfo::exists(resource)) {
                cloneProducerToFile(resource);
            }
        }
        if (timeremapInfo.enableRemap) {
            Mlt::Chain *chain = new Mlt::Chain(pCore->getProjectProfile(), resource.toUtf8().constData());
            Mlt::Link link("timeremap");
            if (!timeremapInfo.timeMapData.isEmpty()) {
                link.set("time_map", timeremapInfo.timeMapData.toUtf8().constData());
            }
            link.set("pitch", timeremapInfo.pitchShift);
            link.set("image_mode", timeremapInfo.imageMode.toUtf8().constData());
            chain->attach(link);
            warpProducer.reset(chain);
        } else {
            QString url;
            QString original_resource;
            if (m_clipStatus == FileStatus::StatusMissing) {
                url = QString("timewarp:%1:%2").arg(QString::fromStdString(std::to_string(speed)), QString("qtext"));
                original_resource = originalProducer()->get("resource");

            } else {
                if (resource.endsWith(QLatin1String(":qtext"))) {
                    resource.replace(QLatin1String("qtext"), originalProducer()->get("warp_resource"));
                }
                if (m_clipType == ClipType::Timeline || m_clipType == ClipType::Playlist) {
                    // We must use the special "consumer" producer for mlt playlist files
                    resource.prepend(QStringLiteral("consumer:"));
                }
                url = QString("timewarp:%1:%2").arg(QString::fromStdString(std::to_string(speed)), resource);
            }
            warpProducer.reset(new Mlt::Producer(pCore->getProjectProfile(), url.toUtf8().constData()));
            int original_length = originalProducer()->get_length();
            int updated_length = qRound(original_length / std::abs(speed));
            warpProducer->set("length", updated_length);
            if (!original_resource.isEmpty()) {
                // Don't lose original resource for placeholder clips
                // warpProducer->set("warp_resource", original_resource.toUtf8().constData());
                warpProducer->set("text", i18n("Invalid").toUtf8().constData());
            }
        }
        // this is a workaround to cope with Mlt erroneous rounding
        Mlt::Properties original(m_masterProducer->get_properties());
        Mlt::Properties cloneProps(warpProducer->get_properties());
        cloneProps.pass_list(original, ClipController::getPassPropertiesList(false));

        if (audioStream > -1) {
            int newAudioStreamIndex = audioStreamIndex(audioStream);
            if (newAudioStreamIndex > -1) {
                /** If the audioStreamIndex is not found, for example when replacing a clip with another one using different indexes,
                default to first audio stream */
                warpProducer->set("audio_index", audioStream);
            } else {
                newAudioStreamIndex = 0;
            }
            if (newAudioStreamIndex > audioStreamsCount() - 1) {
                newAudioStreamIndex = 0;
            }
            warpProducer->set("astream", newAudioStreamIndex);
        } else {
            warpProducer->set("audio_index", audioStream);
            warpProducer->set("astream", audioStreamIndex(audioStream));
        }
    }

    // if the producer has a "time-to-live" (frame duration) we need to scale it according to the speed
    int ttl = originalProducer()->get_int("ttl");
    if (ttl > 0) {
        int new_ttl = qRound(ttl / std::abs(speed));
        warpProducer->set("ttl", std::max(new_ttl, 1));
    }

    qDebug() << "warp LENGTH" << warpProducer->get_length();
    warpProducer->set("set.test_audio", 1);
    warpProducer->set("set.test_image", 1);
    warpProducer->set("kdenlive:id", binId().toUtf8().constData());
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

std::pair<std::shared_ptr<Mlt::Producer>, bool> ProjectClip::giveMasterAndGetTimelineProducer(int clipId, std::shared_ptr<Mlt::Producer> master,
                                                                                              PlaylistState::ClipState state, int tid, bool secondPlaylist)
{
    int in = master->get_in();
    int out = master->get_out();
    if (master->parent().is_valid()) {
        // in that case, we have a cut
        // check whether it's a timewarp
        double speed = 1.0;
        bool timeWarp = false;
        ProjectClip::TimeWarpInfo remapInfo;
        remapInfo.enableRemap = false;
        if (master->parent().property_exists("warp_speed")) {
            speed = master->parent().get_double("warp_speed");
            timeWarp = true;
        } else if (master->parent().type() == mlt_service_chain_type) {
            // Check if we have a timeremap link
            Mlt::Chain parentChain(master->parent());
            if (parentChain.link_count() > 0) {
                for (int i = 0; i < parentChain.link_count(); i++) {
                    std::unique_ptr<Mlt::Link> link(parentChain.link(i));
                    if (strcmp(link->get("mlt_service"), "timeremap") == 0) {
                        if (!link->property_exists("time_map")) {
                            link->set("time_map", link->get("map"));
                        }
                        remapInfo.enableRemap = true;
                        remapInfo.timeMapData = link->get("time_map");
                        remapInfo.pitchShift = link->get_int("pitch");
                        remapInfo.imageMode = link->get("image_mode");
                        break;
                    }
                }
            }
        }
        if (master->parent().get_int("_loaded") == 1) {
            // we already have a clip that shares the same master
            if (state != PlaylistState::Disabled || timeWarp || !remapInfo.timeMapData.isEmpty()) {
                // In that case, we must create copies
                std::shared_ptr<Mlt::Producer> prod(
                    getTimelineProducer(tid, clipId, state, master->parent().get_int("audio_index"), speed, secondPlaylist, remapInfo)->cut(in, out));
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
            if (timeWarp || !remapInfo.timeMapData.isEmpty()) {
                QString resource = master->parent().get("resource");
                if (master->parent().property_exists("_rebuild") || resource.endsWith(QLatin1String("qtext"))) {
                    // This was a placeholder or missing clip, reset producer
                    std::shared_ptr<Mlt::Producer> prod(
                        getTimelineProducer(tid, clipId, state, master->parent().get_int("audio_index"), speed, secondPlaylist, remapInfo));
                    m_timewarpProducers[clipId] = prod;
                } else {
                    m_timewarpProducers[clipId] = std::make_shared<Mlt::Producer>(&master->parent());
                }
                m_effectStack->loadService(m_timewarpProducers[clipId]);
                return {master, true};
            }
            if (state == PlaylistState::AudioOnly) {
                int audioStream = master->parent().get_int("audio_index");
                if (audioStream > -1) {
                    tid += 100 * audioStream;
                }
                if (secondPlaylist) {
                    tid = -tid;
                }
                if (m_audioProducers.find(tid) != m_audioProducers.end()) {
                    // Buggy project, all clips in a track should use the same track producer, fix
                    qDebug() << "/// FOUND INCORRECT PRODUCER ON AUDIO TRACK; FIXING";
                    std::shared_ptr<Mlt::Producer> prod(getTimelineProducer(tid, clipId, state, master->parent().get_int("audio_index"), speed)->cut(in, out));
                    return {prod, false};
                }
                m_audioProducers[tid] = std::make_shared<Mlt::Producer>(&master->parent());
                m_effectStack->loadService(m_audioProducers[tid]);
                return {master, true};
            }
            if (state == PlaylistState::VideoOnly) {
                // good, we found a master video producer, and we didn't have any
                if (m_clipType != ClipType::Color && m_clipType != ClipType::Image && m_clipType != ClipType::Text) {
                    // Color, image and text clips always use master producer in timeline
                    if (secondPlaylist) {
                        tid = -tid;
                    }
                    if (m_videoProducers.find(tid) != m_videoProducers.end()) {
                        qDebug() << "/// FOUND INCORRECT PRODUCER ON VIDEO TRACK; FIXING";
                        // Buggy project, all clips in a track should use the same track producer, fix
                        std::shared_ptr<Mlt::Producer> prod(
                            getTimelineProducer(tid, clipId, state, master->parent().get_int("audio_index"), speed)->cut(in, out));
                        return {prod, false};
                    }
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

void ProjectClip::cloneProducerToFile(const QString &path, bool thumbsProducer)
{
    QMutexLocker lk(&m_producerMutex);
    QMutexLocker lock(&pCore->xmlMutex);
    Mlt::Consumer c(m_masterProducer->get_profile(), "xml", path.toUtf8().constData());
    c.set("time_format", "frames");
    c.set("no_meta", 1);
    c.set("no_root", 1);
    if (m_clipType != ClipType::Timeline && m_clipType != ClipType::Playlist && m_clipType != ClipType::Text && m_clipType != ClipType::TextTemplate) {
        // Playlist and text clips need to keep their profile info
        c.set("no_profile", 1);
    }
    c.set("root", "/");
    if (!thumbsProducer) {
        c.set("store", "kdenlive");
    }
    Mlt::Service s(m_masterProducer->get_service());
    c.connect(s);
    c.run();
    /*if (ignore) {
        s.set("ignore_points", ignore);
    }*/
    if (!thumbsProducer && m_usesProxy) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QTextStream in(&file);
            QString content = in.readAll();
            file.close();
            content.replace(getProducerProperty(QStringLiteral("resource")), getProducerProperty(QStringLiteral("kdenlive:originalurl")));
            if (file.open(QIODevice::WriteOnly)) {
                QTextStream out(&file);
                out << content;
                file.close();
            }
        }
    }
}

void ProjectClip::saveZone(QPoint zone, const QDir &dir)
{
    QString path = QString(clipName() + QLatin1Char('_') + QString::number(zone.x()) + QStringLiteral(".mlt"));
    QString fullPath = dir.absoluteFilePath(path);
    if (dir.exists(path)) {
        QUrl url = QUrl::fromLocalFile(fullPath);
        KIO::RenameDialog renameDialog(QApplication::activeWindow(), i18n("File already exists"), url, url, KIO::RenameDialog_Option::RenameDialog_Overwrite);
        if (renameDialog.exec() != QDialog::Rejected) {
            url = renameDialog.newDestUrl();
            if (url.isValid()) {
                fullPath = url.toLocalFile();
            }
        } else {
            return;
        }
    }
    QReadLocker lock(&m_producerLock);
    Mlt::Consumer xmlConsumer(pCore->getProjectProfile(), "xml", fullPath.toUtf8().constData());
    xmlConsumer.set("terminate_on_pause", 1);
    xmlConsumer.set("store", "kdenlive");
    xmlConsumer.set("no_meta", 1);
    if (m_clipType != ClipType::Timeline) {
        Mlt::Producer prod(m_masterProducer->parent());
        std::unique_ptr<Mlt::Producer> prod2(prod.cut(zone.x(), zone.y()));
        Mlt::Playlist list(pCore->getProjectProfile());
        list.insert_at(0, *prod2.get(), 0);
        // list.set("title", desc.toUtf8().constData());
        xmlConsumer.connect(list);
    } else {
        xmlConsumer.connect(m_masterProducer->parent());
    }
    QMutexLocker xmlLock(&pCore->xmlMutex);
    xmlConsumer.run();
}

std::shared_ptr<Mlt::Producer> ProjectClip::cloneProducer(bool removeEffects, bool timelineProducer)
{
    Q_UNUSED(timelineProducer);
    QMutexLocker lk(&m_producerMutex);
    QMutexLocker lock(&pCore->xmlMutex);
    Mlt::Consumer c(pCore->getProjectProfile(), "xml", "string");
    Mlt::Service s(m_masterProducer->get_service());
    m_masterProducer->lock();
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
    lock.unlock();
    m_masterProducer->unlock();
    const QByteArray clipXml = c.get("string");
    std::shared_ptr<Mlt::Producer> prod(new Mlt::Producer(pCore->getProjectProfile(), "xml-string", clipXml.constData()));
    if (strcmp(prod->get("mlt_service"), "avformat") == 0) {
        prod->set("mlt_service", "avformat-novalidate");
        prod->set("mute_on_pause", 0);
    }
    // TODO: needs more testing, removes clutter from project files
    /*if (timelineProducer) {
        // Strip the kdenlive: properties, not useful in timeline
        const char *prefix = "kdenlive:";
        const size_t prefix_len = strlen(prefix);
        QStringList propertiesToRemove;
        for (int i = prod->count() - 1; i >= 0; --i) {
            char *current = prod->get_name(i);
            if (strlen(current) >= prefix_len && strncmp(current, prefix, prefix_len) == 0) {
                propertiesToRemove << qstrdup(current);
            }
        }
        propertiesToRemove.removeAll(QLatin1String("kdenlive:id"));
        qDebug()<<"::: CLEARING PROPERTIES: "<<propertiesToRemove;
        Mlt::Properties props(*prod.get());
        for (auto &p : propertiesToRemove) {
            props.clear(p.toUtf8().constData());
        }
    } else {*/
    // we pass some properties that wouldn't be passed because of the novalidate
    const char *prefix = "meta.";
    const size_t prefix_len = strlen(prefix);
    for (int i = 0; i < m_masterProducer->count(); ++i) {
        char *current = m_masterProducer->get_name(i);
        if (strlen(current) >= prefix_len && strncmp(current, prefix, prefix_len) == 0) {
            prod->set(current, m_masterProducer->get(i));
        }
    }
    //}

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
    prod->set("id", nullptr);
    return prod;
}

std::shared_ptr<Mlt::Producer> ProjectClip::cloneProducer(const std::shared_ptr<Mlt::Producer> &producer)
{
    QMutexLocker lock(&pCore->xmlMutex);
    Mlt::Consumer c(pCore->getProjectProfile(), "xml", "string");
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
    c.run();
    if (ignore) {
        s.set("ignore_points", ignore);
    }
    const QByteArray clipXml = c.get("string");
    std::shared_ptr<Mlt::Producer> prod(new Mlt::Producer(pCore->getProjectProfile(), "xml-string", clipXml.constData()));
    if (strcmp(prod->get("mlt_service"), "avformat") == 0) {
        prod->set("mlt_service", "avformat-novalidate");
        prod->set("mute_on_pause", 0);
    }
    return prod;
}

std::unique_ptr<Mlt::Producer> ProjectClip::softClone(const char *list)
{
    QString service = QString::fromLatin1(m_masterProducer->get("mlt_service"));
    QString resource = QString::fromUtf8(m_masterProducer->get("resource"));
    std::unique_ptr<Mlt::Producer> clone(new Mlt::Producer(pCore->thumbProfile(), service.toUtf8().constData(), resource.toUtf8().constData()));
    Mlt::Filter scaler(pCore->thumbProfile(), "swscale");
    Mlt::Filter converter(pCore->getProjectProfile(), "avcolor_space");
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
    std::unique_ptr<Mlt::Producer> clone(new Mlt::Producer(m_masterProducer->get_profile(), service.toUtf8().constData(), resource.toUtf8().constData()));
    Mlt::Properties original(m_masterProducer->get_properties());
    Mlt::Properties cloneProps(clone->get_properties());
    cloneProps.pass_list(original, list);
    return clone;
}

QPoint ProjectClip::zone() const
{
    int in = getProducerIntProperty(QStringLiteral("kdenlive:zone_in"));
    int max = getFramePlaytime();
    int out = qMin(getProducerIntProperty(QStringLiteral("kdenlive:zone_out")), max);
    if (out <= in) {
        out = max;
    }
    return QPoint(in, out);
}

const QString ProjectClip::hash(bool createIfEmpty)
{
    if (m_clipStatus == FileStatus::StatusWaiting) {
        // Clip is not ready
        return QString();
    }
    QString clipHash = getProducerProperty(QStringLiteral("kdenlive:file_hash"));
    if (!clipHash.isEmpty() || !createIfEmpty) {
        return clipHash;
    }
    return getFileHash();
}

const QString ProjectClip::hashForThumbs()
{
    if (m_clipStatus == FileStatus::StatusWaiting) {
        // Clip is not ready
        return QString();
    }
    if (m_clipType == ClipType::Timeline) {
        return m_sequenceUuid.toString();
    }
    QString clipHash = getProducerProperty(QStringLiteral("kdenlive:file_hash"));
    if (!clipHash.isEmpty() && m_hasMultipleVideoStreams) {
        clipHash.append(m_properties->get("video_index"));
    }
    return clipHash;
}

const QByteArray ProjectClip::getFolderHash(const QDir &dir, QString fileName)
{
    QStringList files = dir.entryList(QDir::Files);
    fileName.append(files.join(QLatin1Char(',')));
    // Include file hash info in case we have several folders with same file names (can happen for image sequences)
    if (!files.isEmpty()) {
        QPair<QByteArray, qint64> hashData = calculateHash(dir.absoluteFilePath(files.first()));
        fileName.append(hashData.first);
        fileName.append(QString::number(hashData.second));
        if (files.size() > 1) {
            hashData = calculateHash(dir.absoluteFilePath(files.at(files.size() / 2)));
            fileName.append(hashData.first);
            fileName.append(QString::number(hashData.second));
        }
    }
    QByteArray fileData = fileName.toUtf8();
    return QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
}

const QString ProjectClip::getFileHash()
{
    QByteArray fileData;
    QByteArray fileHash;

    switch (m_clipType) {
    case ClipType::SlideShow:
        fileHash = getFolderHash(QFileInfo(clipUrl()).absoluteDir(), QFileInfo(clipUrl()).fileName());
        break;
    case ClipType::Text: {
        fileData = getProducerProperty(QStringLiteral("xmldata")).toUtf8();
        // If 2 clips share the same content (for example duplicated clips), they must not have the same hash
        QByteArray uniqueId = getProducerProperty(QStringLiteral("kdenlive:uniqueId")).toUtf8();
        if (uniqueId.isEmpty()) {
            const QUuid uuid = QUuid::createUuid();
            setProducerProperty(QStringLiteral("kdenlive:uniqueId"), uuid.toString());
            uniqueId = uuid.toString().toUtf8();
        }
        fileData.prepend(uniqueId);
        fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
        break;
    }
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
    case ClipType::Timeline:
        fileData = m_sequenceUuid.toString().toUtf8();
        fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
        break;
    default:
        QPair<QByteArray, qint64> hashData = calculateHash(clipUrl());
        fileHash = hashData.first;
        ClipController::setProducerProperty(QStringLiteral("kdenlive:file_size"), QString::number(hashData.second));
        break;
    }
    if (fileHash.isEmpty()) {
        if (m_service == QLatin1String("blipflash")) {
            // Used in tests
            fileData = getProducerProperty(QStringLiteral("resource")).toUtf8();
            fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
        } else {
            qDebug() << "// WARNING EMPTY CLIP HASH: ";
            return QString();
        }
    }
    QString result = fileHash.toHex();
    ClipController::setProducerProperty(QStringLiteral("kdenlive:file_hash"), result);
    return result;
}

const QPair<QByteArray, qint64> ProjectClip::calculateHash(const QString &path)
{
    QFile file(path);
    QByteArray fileHash;
    qint64 fSize = 0;
    if (file.open(QIODevice::ReadOnly)) { // write size and hash only if resource points to a file
        /*
         * 1 MB = 1 second per 450 files (or faster)
         * 10 MB = 9 seconds per 450 files (or faster)
         */
        QByteArray fileData;
        fSize = file.size();
        if (fSize > 2000000) {
            fileData = file.read(1000000);
            if (file.seek(file.size() - 1000000)) {
                fileData.append(file.readAll());
            }
        } else {
            fileData = file.readAll();
        }
        file.close();
        fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
    }
    return {fileHash, fSize};
}

double ProjectClip::getOriginalFps() const
{
    return originalFps();
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
                                                                           {AbstractProjectItem::DataDescription});
        refreshPanel = true;
    }
    // Some properties also need to be passed to track producers
    QStringList timelineProperties{
        QStringLiteral("force_aspect_ratio"), QStringLiteral("set.force_full_luma"), QStringLiteral("full_luma"),         QStringLiteral("threads"),
        QStringLiteral("force_colorspace"),   QStringLiteral("force_tff"),           QStringLiteral("force_progressive"), QStringLiteral("video_delay")};
    QStringList forceReloadProperties{QStringLiteral("rotate"),      QStringLiteral("autorotate"),     QStringLiteral("resource"),
                                      QStringLiteral("force_fps"),   QStringLiteral("set.test_image"), QStringLiteral("video_index"),
                                      QStringLiteral("disable_exif")};
    QStringList keys{QStringLiteral("luma_duration"), QStringLiteral("luma_file"), QStringLiteral("fade"),      QStringLiteral("ttl"),
                     QStringLiteral("softness"),      QStringLiteral("crop"),      QStringLiteral("animation"), QStringLiteral("low-pass")};
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
    if (m_clipType == ClipType::QText && properties.contains(QStringLiteral("text"))) {
        reload = true;
        refreshOnly = false;
    }
    if (m_clipType == ClipType::TextTemplate && properties.contains(QStringLiteral("templatetext"))) {
        m_masterProducer->lock();
        m_masterProducer->set("force_reload", 1);
        m_masterProducer->unlock();
        ThumbnailCache::get()->invalidateThumbsForClip(m_binId);
        reload = true;
        refreshOnly = true;
        updateRoles << TimelineModel::ResourceRole;
    }
    if (properties.contains(QStringLiteral("resource"))) {
        // Clip source was changed, update important stuff
        refreshPanel = true;
        reload = true;
        ThumbnailCache::get()->invalidateThumbsForClip(m_binId);
        resetProducerProperty(QStringLiteral("kdenlive:file_hash"));
        if (m_clipType == ClipType::Color) {
            refreshOnly = true;
            updateRoles << TimelineModel::ResourceRole;
        } else if (properties.contains("_fullreload")) {
            // Clip resource changed, update thumbnail, name, clear hash
            refreshOnly = false;
            // Enforce reloading clip type in case of clip replacement
            if (m_clipType == ClipType::Image) {
                // If replacing an image with another one, don't clear type so duration is preserved
                QMimeDatabase db;
                QMimeType type = db.mimeTypeForUrl(QUrl::fromLocalFile(properties.value(QStringLiteral("resource"))));
                if (!type.name().startsWith(QLatin1String("image/"))) {
                    m_service.clear();
                    m_clipType = ClipType::Unknown;
                }
            } else {
                m_service.clear();
                m_clipType = ClipType::Unknown;
            }
            clearBackupProperties();
            updateRoles << TimelineModel::ResourceRole << TimelineModel::MaxDurationRole << TimelineModel::NameRole;
        }
    }
    if (properties.contains(QStringLiteral("kdenlive:proxy")) && !properties.contains("_fullreload")) {
        QString value = properties.value(QStringLiteral("kdenlive:proxy"));
        // If value is "-", that means user manually disabled proxy on this clip
        ObjectId oid(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid());
        if (value.isEmpty() || value == QLatin1String("-")) {
            // reset proxy
            if (pCore->taskManager.hasPendingJob(oid, AbstractTask::PROXYJOB)) {
                // The proxy clip is being created, abort
                pCore->taskManager.discardJobs(oid, AbstractTask::PROXYJOB);
            } else {
                reload = true;
                refreshOnly = false;
                // Restore original url
                QString resource = getProducerProperty(QStringLiteral("kdenlive:originalurl"));
                if (!resource.isEmpty()) {
                    setProducerProperty(QStringLiteral("resource"), resource);
                }
            }
        } else {
            // A proxy was requested, make sure to keep original url
            setProducerProperty(QStringLiteral("kdenlive:originalurl"), url());
            backupOriginalProperties();
            ProxyTask::start(oid, this);
        }
    } else if (!reload) {
        const QList<QString> propKeys = properties.keys();
        for (const QString &k : propKeys) {
            if (forceReloadProperties.contains(k)) {
                refreshPanel = true;
                refreshOnly = false;
                reload = true;
                ThumbnailCache::get()->invalidateThumbsForClip(m_binId);
                break;
            }
        }
    }
    if (!reload && (properties.contains(QStringLiteral("xmldata")) || !passProperties.isEmpty())) {
        reload = true;
        updateRoles << TimelineModel::ResourceRole;
    }
    if (refreshAnalysis) {
        Q_EMIT refreshAnalysisPanel();
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
                                                                           {AbstractProjectItem::DataDuration});
        refreshOnly = false;
        reload = m_clipType != ClipType::Timeline;
    }
    QVector<int> refreshRoles;
    if (properties.contains(QStringLiteral("kdenlive:tags"))) {
        setTags(properties.value(QStringLiteral("kdenlive:tags")));
        if (auto ptr = m_model.lock()) {
            std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()),
                                                                           {AbstractProjectItem::DataTag});
        }
        refreshRoles << TimelineModel::TagRole;
    }
    if (properties.contains(QStringLiteral("kdenlive:clipname"))) {
        const QString updatedName = properties.value(QStringLiteral("kdenlive:clipname"));
        if (updatedName.isEmpty()) {
            if (m_clipType != ClipType::Timeline && m_clipType != ClipType::Text && m_clipType != ClipType::TextTemplate) {
                m_name = QFileInfo(m_path).fileName();
            }
        } else {
            m_name = updatedName;
        }
        refreshPanel = true;
        if (auto ptr = m_model.lock()) {
            std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()),
                                                                           {AbstractProjectItem::DataName});
        }
        refreshRoles << TimelineModel::NameRole;
        if (m_clipType == ClipType::Timeline && !m_sequenceUuid.isNull()) {
            // This is a timeline clip, update tab name
            Q_EMIT pCore->bin()->updateTabName(m_sequenceUuid, m_name);
        }
    }
    if (properties.contains(QStringLiteral("kdenlive:description"))) {
        m_description = properties.value(QStringLiteral("kdenlive:description"));
        refreshPanel = true;
        if (auto ptr = m_model.lock()) {
            std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()),
                                                                           {AbstractProjectItem::DataDescription});
        }
    }
    // update timeline clips
    if (!reload) {
        updateTimelineClips(refreshRoles);
    }
    bool audioStreamChanged = properties.contains(QStringLiteral("audio_index")) || properties.contains(QStringLiteral("astream"));
    if (reload) {
        // producer has changed, refresh monitor and thumbnail
        if (hasProxy()) {
            ObjectId oid(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid());
            pCore->taskManager.discardJobs(oid, AbstractTask::PROXYJOB);
            setProducerProperty(QStringLiteral("_overwriteproxy"), 1);
            ProxyTask::start(oid, this);
        } else {
            reloadProducer(refreshOnly, properties.contains(QStringLiteral("kdenlive:proxy")));
        }
        if (refreshOnly) {
            if (auto ptr = m_model.lock()) {
                Q_EMIT std::static_pointer_cast<ProjectItemModel>(ptr)->refreshClip(m_binId);
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
            Q_EMIT audioThumbReady();
            pCore->bin()->reloadMonitorStreamIfActive(clipId());
            refreshPanel = true;
        }
    }
    if (refreshPanel && m_properties) {
        // Some of the clip properties have changed through a command, update properties panel
        Q_EMIT refreshPropertiesPanel();
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

void ProjectClip::refreshTracksState(int tracksCount)
{
    if (tracksCount > -1) {
        setProducerProperty(QStringLiteral("kdenlive:sequenceproperties.tracksCount"), tracksCount);
    }
    if (m_clipStatus == FileStatus::StatusReady) {
        checkAudioVideo();
        Q_EMIT refreshPropertiesPanel();
    }
}

ClipPropertiesController *ProjectClip::buildProperties(QWidget *parent)
{
    auto ptr = m_model.lock();
    Q_ASSERT(ptr);
    auto *panel = new ClipPropertiesController(clipName(), static_cast<ClipController *>(this), parent);
    connect(this, &ProjectClip::refreshPropertiesPanel, panel, &ClipPropertiesController::slotReloadProperties);
    connect(this, &ProjectClip::refreshAnalysisPanel, panel, &ClipPropertiesController::slotFillAnalysisData);
    connect(this, &ProjectClip::updateStreamInfo, panel, &ClipPropertiesController::updateStreamInfo);
    connect(panel, &ClipPropertiesController::requestProxy, this, [this](bool doProxy) {
        QList<std::shared_ptr<ProjectClip>> clipList{std::static_pointer_cast<ProjectClip>(shared_from_this())};
        pCore->currentDoc()->slotProxyCurrentItem(doProxy, clipList);
    });
    connect(panel, &ClipPropertiesController::deleteProxy, [this]() { deleteProxy(); });
    return panel;
}

void ProjectClip::deleteProxy(bool reloadClip)
{
    // Disable proxy file
    QString proxy = getProducerProperty(QStringLiteral("kdenlive:proxy"));
    QList<std::shared_ptr<ProjectClip>> clipList{std::static_pointer_cast<ProjectClip>(shared_from_this())};
    if (reloadClip) {
        pCore->currentDoc()->slotProxyCurrentItem(false, clipList);
    }
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

QString ProjectClip::clipName()
{
    if (m_name.isEmpty()) {
        m_name = getProducerProperty(QStringLiteral("kdenlive:clipname"));
        if (m_name.isEmpty()) {
            m_name = m_path.isEmpty() || m_clipType == ClipType::Timeline ? i18n("Unnamed") : QFileInfo(m_path).fileName();
        }
    }
    return m_name;
}

bool ProjectClip::rename(const QString &name, int column)
{
    QMap<QString, QString> newProperties;
    QMap<QString, QString> oldProperties;
    bool edited = false;
    switch (column) {
    case 0:
        if (m_name == name || ((m_clipType == ClipType::Timeline || m_clipType == ClipType::Text) && name.isEmpty())) {
            return false;
        }
        // Rename clip
        oldProperties.insert(QStringLiteral("kdenlive:clipname"), m_name);
        newProperties.insert(QStringLiteral("kdenlive:clipname"), name);
        edited = true;
        break;
    case 2:
        if (m_description == name) {
            return false;
        }
        // Rename clip
        if (m_clipType == ClipType::TextTemplate) {
            oldProperties.insert(QStringLiteral("templatetext"), m_description);
            newProperties.insert(QStringLiteral("templatetext"), name);
        } else {
            oldProperties.insert(QStringLiteral("kdenlive:description"), m_description);
            newProperties.insert(QStringLiteral("kdenlive:description"), name);
        }
        edited = true;
        break;
    }
    if (edited) {
        pCore->bin()->slotEditClipCommand(m_binId, oldProperties, newProperties);
    }
    return edited;
}

const QVariant ProjectClip::getData(DataType type) const
{
    switch (type) {
    case AbstractProjectItem::IconOverlay:
        if (m_clipStatus == FileStatus::StatusMissing) {
            return QVariant("window-close");
        }
        if (m_clipStatus == FileStatus::StatusWaiting) {
            return QVariant("view-refresh");
        }
        if (m_properties && m_properties->get_int("meta.media.variable_frame_rate")) {
            return QVariant("emblem-warning");
        }
        return m_effectStack && m_effectStack->rowCount() > 0 ? QVariant("kdenlive-track_has_effect") : QVariant();
    default:
        return AbstractProjectItem::getData(type);
    }
}

bool ProjectClip::hasVariableFps()
{
    if (m_properties && m_properties->get_int("meta.media.variable_frame_rate")) {
        return true;
    }
    return false;
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
    pCore->taskManager.discardJobs(ObjectId(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid()), AbstractTask::AUDIOTHUMBJOB);
    QString audioThumbPath;
    QList<int> streams = m_audioInfo->streams().keys();
    // Delete audio thumbnail data
    for (int &st : streams) {
        audioThumbPath = getAudioThumbPath(st);
        if (!audioThumbPath.isEmpty()) {
            QFile::remove(audioThumbPath);
        }
        // Clear audio cache
        QString key = QString("%1:%2").arg(m_binId).arg(st);
        pCore->audioThumbCache.insert(key, QByteArray("-"));
    }
    // Delete thumbnail
    for (int &st : streams) {
        audioThumbPath = getAudioThumbPath(st);
        if (!audioThumbPath.isEmpty()) {
            QFile::remove(audioThumbPath);
        }
    }

    resetProducerProperty(QStringLiteral("kdenlive:audio_max"));
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
    bool ok;
    QDir thumbFolder = pCore->projectManager()->cacheDir(true, &ok);
    if (!ok) {
        qWarning() << "Cannot write to cache folder: " << thumbFolder.absolutePath();
        return QString();
    }
    const QString clipHash = hash(false);
    if (clipHash.isEmpty()) {
        return QString();
    }
    QString audioPath = thumbFolder.absoluteFilePath(clipHash);
    audioPath.append(QLatin1Char('_') + QString::number(stream));
    int roundedFps = int(pCore->getCurrentFps());
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
        // TODO
        /*if (KMessageBox::questionTwoActions(QApplication::activeWindow(), i18n("Clip already contains analysis data %1", name), QString(),
                                            KGuiItem(i18n("Merge")), KStandardGuiItem::add()) == KMessageBox::PrimaryAction) {
            // Merge data
            // TODO MLT7: convert to Mlt::Animation
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
        }*/
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
    // TODO MLT7: port to Mlt::Animation
    /*auto &profile = pCore->getCurrentProfile();
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
    */
    return QString();
}

bool ProjectClip::isSplittable() const
{
    return (m_clipType == ClipType::AV || m_clipType == ClipType::Playlist || m_clipType == ClipType::Timeline);
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
    Q_ASSERT(!timeline.expired());
    uint currentCount = 0;
    if (auto ptr = timeline.lock()) {
        if (m_hasAudio) {
            if (ptr->getClipState(clipId) == PlaylistState::AudioOnly) {
                m_AudioUsage++;
            }
        }
        const QUuid uuid = ptr->uuid();
        if (m_registeredClipsByUuid.contains(uuid)) {
            QList<int> values = m_registeredClipsByUuid.value(uuid);
            Q_ASSERT(values.contains(clipId) == false);
            values << clipId;
            currentCount = values.size();
            m_registeredClipsByUuid[uuid] = values;
        } else {
            m_registeredClipsByUuid.insert(uuid, {clipId});
            currentCount = 1;
        }
    }
    uint totalCount = 0;
    QMapIterator<QUuid, QList<int>> i(m_registeredClipsByUuid);
    while (i.hasNext()) {
        i.next();
        totalCount += i.value().size();
    }
    setRefCount(currentCount, totalCount);
    Q_EMIT registeredClipChanged();
}

void ProjectClip::checkClipBounds()
{
    m_boundaryTimer.start();
}

void ProjectClip::refreshBounds()
{
    QVector<QPoint> boundaries;
    uint currentCount = 0;
    const QUuid uuid = pCore->currentTimelineId();
    if (m_registeredClipsByUuid.contains(uuid)) {
        const QList<int> clips = m_registeredClipsByUuid.value(uuid);
        currentCount = clips.size();
        auto timeline = pCore->currentDoc()->getTimeline(uuid);
        for (auto &c : clips) {
            QPoint point = timeline->getClipInDuration(c);
            if (!boundaries.contains(point)) {
                boundaries << point;
            }
        }
    }
    uint totalCount = 0;
    QMapIterator<QUuid, QList<int>> i(m_registeredClipsByUuid);
    while (i.hasNext()) {
        i.next();
        totalCount += i.value().size();
    }
    setRefCount(currentCount, totalCount);
    Q_EMIT boundsChanged(boundaries);
}

void ProjectClip::deregisterTimelineClip(int clipId, bool audioClip, const QUuid &uuid)
{
    if (m_hasAudio && audioClip) {
        m_AudioUsage--;
    }
    if (m_videoProducers.count(clipId) > 0) {
        m_effectStack->removeService(m_videoProducers[clipId]);
        m_videoProducers.erase(clipId);
    }
    if (m_audioProducers.count(clipId) > 0) {
        m_effectStack->removeService(m_audioProducers[clipId]);
        m_audioProducers.erase(clipId);
    }
    // Clip might already have been deregistered
    if (m_registeredClipsByUuid.contains(uuid)) {
        QList<int> clips = m_registeredClipsByUuid.value(uuid);
        Q_ASSERT(clips.contains(clipId));
        clips.removeAll(clipId);
        if (clips.isEmpty()) {
            m_registeredClipsByUuid.remove(uuid);
        } else {
            m_registeredClipsByUuid[uuid] = clips;
        }
        uint currentCount = 0;
        uint totalCount = 0;
        QMapIterator<QUuid, QList<int>> i(m_registeredClipsByUuid);
        while (i.hasNext()) {
            i.next();
            totalCount += i.value().size();
            if (i.key() == pCore->currentTimelineId()) {
                currentCount = uint(i.value().size());
            }
        }
        setRefCount(currentCount, totalCount);
        Q_EMIT registeredClipChanged();
    }
}

QList<int> ProjectClip::timelineInstances(QUuid activeUuid) const
{
    if (activeUuid.isNull()) {
        activeUuid = pCore->currentTimelineId();
    }
    if (!m_registeredClipsByUuid.contains(activeUuid)) {
        return {};
    }
    return m_registeredClipsByUuid.value(activeUuid);
}

QMap<QUuid, QList<int>> ProjectClip::getAllTimelineInstances() const
{
    return m_registeredClipsByUuid;
}

QStringList ProjectClip::timelineSequenceExtraResources() const
{
    QStringList urls;
    if (m_clipType != ClipType::Timeline) {
        return urls;
    }
    for (auto &warp : m_timewarpProducers) {
        urls << warp.second->get("warp_resource");
    }
    urls.removeDuplicates();
    return urls;
}

const QString ProjectClip::isReferenced(const QUuid &activeUuid) const
{
    if (m_registeredClipsByUuid.contains(activeUuid) && !m_registeredClipsByUuid.value(activeUuid).isEmpty()) {
        return m_binId;
    }
    return QString();
}

void ProjectClip::purgeReferences(const QUuid &activeUuid, bool deleteClip)
{
    if (!m_registeredClipsByUuid.contains(activeUuid)) {
        return;
    }
    if (deleteClip) {
        QList<int> toDelete = m_registeredClipsByUuid.value(activeUuid);
        auto timeline = pCore->currentDoc()->getTimeline(activeUuid);
        while (!toDelete.isEmpty()) {
            int id = toDelete.takeFirst();
            if (m_hasAudio) {
                if (timeline->getClipState(id) == PlaylistState::AudioOnly) {
                    m_AudioUsage--;
                }
            }
        }
    }
    m_registeredClipsByUuid.remove(activeUuid);
    uint currentCount = 0;
    uint totalCount = 0;
    QMapIterator<QUuid, QList<int>> i(m_registeredClipsByUuid);
    while (i.hasNext()) {
        i.next();
        totalCount += i.value().size();
        if (i.key() == pCore->currentTimelineId()) {
            currentCount = uint(i.value().size());
        }
    }
    setRefCount(currentCount, totalCount);
    Q_EMIT registeredClipChanged();
}

bool ProjectClip::selfSoftDelete(Fun &undo, Fun &redo)
{
    Fun operation = [this]() {
        // Free audio thumb data and timeline producers
        pCore->taskManager.discardJobs(ObjectId(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid()));
        m_audioLevels.clear();
        m_disabledProducer.reset();
        m_audioProducers.clear();
        m_videoProducers.clear();
        if (m_timewarpProducers.size() > 0 && pCore->window() && pCore->bin()->isEnabled()) {
            // If the clip is deleted, remove timewarp producers. Don't delete if Bin is disabled because this is when we are closing a project
            if (m_clipType == ClipType::Timeline) {
                bool ok;
                QDir sequenceFolder = pCore->currentDoc()->getCacheDir(CacheTmpWorkFiles, &ok);
                if (ok) {
                    QString resource = sequenceFolder.absoluteFilePath(QString("sequence-%1.mlt").arg(m_sequenceUuid.toString()));
                    QFile::remove(resource);
                }
            }
        }
        m_timewarpProducers.clear();
        return true;
    };
    operation();
    QMapIterator<QUuid, QList<int>> i(m_registeredClipsByUuid);
    while (i.hasNext()) {
        i.next();
        const QUuid uuid = i.key();
        QList<int> instances = i.value();
        if (!instances.isEmpty()) {
            auto timeline = pCore->currentDoc()->getTimeline(uuid, pCore->projectItemModel()->closing);
            if (!timeline) {
                if (pCore->projectItemModel()->closing) {
                    break;
                }
                qDebug() << "Error while deleting clip: timeline unavailable";
                Q_ASSERT(false);
                return false;
            }
            for (int cid : instances) {
                if (!timeline->isClip(cid)) {
                    // clip already deleted, was probably grouped with another one
                    continue;
                }
                timeline->requestClipUngroup(cid, undo, redo);
                if (!timeline->requestItemDeletion(cid, undo, redo, true)) {
                    return false;
                }
            }
            if (timeline->isClosed) {
                // Refresh timeline occurences
                pCore->currentDoc()->setModified(true);
                pCore->currentDoc()->setSequenceThumbRequiresUpdate(uuid);
                pCore->projectManager()->doSyncTimeline(timeline, false);
            }
        }
    }
    m_registeredClipsByUuid.clear();
    PUSH_LAMBDA(operation, redo);
    return AbstractProjectItem::selfSoftDelete(undo, redo);
}

void ProjectClip::copyTimeWarpProducers(const QDir sequenceFolder, bool copy)
{
    if (m_clipType == ClipType::Timeline) {
        for (auto &warp : m_timewarpProducers) {
            const QString service(warp.second->get("mlt_service"));
            QString path;
            bool isTimeWarp = false;
            const QString resource(warp.second->get("resource"));
            if (service == QLatin1String("timewarp")) {
                path = warp.second->get("warp_resource");
                isTimeWarp = true;
            } else {
                path = resource;
            }
            bool consumerProducer = false;
            if (resource.contains(QLatin1String("consumer:"))) {
                consumerProducer = true;
            }
            if (path.startsWith(QLatin1String("consumer:"))) {
                path = path.section(QLatin1Char(':'), 1);
            }
            if (QFileInfo(path).isRelative()) {
                path.prepend(pCore->currentDoc()->documentRoot());
            }
            QString destFile = sequenceFolder.absoluteFilePath(QFileInfo(path).fileName());
            if (copy) {
                if (!destFile.endsWith(QLatin1String(".mlt")) || destFile == path) {
                    continue;
                }
                QFile::remove(destFile);
                QFile::copy(path, destFile);
            }
            if (isTimeWarp) {
                warp.second->set("warp_resource", destFile.toUtf8().constData());
                QString speed(warp.second->get("warp_speed"));
                speed.append(QStringLiteral(":"));
                if (consumerProducer) {
                    destFile.prepend(QStringLiteral("consumer:"));
                }
                destFile.prepend(speed);
                warp.second->set("resource", destFile.toUtf8().constData());

            } else {
                if (consumerProducer) {
                    destFile.prepend(QStringLiteral("consumer:"));
                }
                warp.second->set("resource", destFile.toUtf8().constData());
            }
        }
    }
}

void ProjectClip::reloadTimeline(std::shared_ptr<EffectStackModel> stack)
{
    if (pCore->bin()) {
        pCore->bin()->reloadMonitorIfActive(m_binId);
    }
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
    if (m_timewarpProducers.size() > 0) {
        if (m_clipType == ClipType::Timeline) {
            bool ok;
            QDir sequenceFolder = pCore->currentDoc()->getCacheDir(CacheTmpWorkFiles, &ok);
            if (ok) {
                QString resource = sequenceFolder.absoluteFilePath(QString("sequence-%1.mlt").arg(m_sequenceUuid.toString()));
                QFile::remove(resource);
            }
        }
    }
    m_timewarpProducers.clear();
    Q_EMIT refreshPropertiesPanel();
    replaceInTimeline();
    updateTimelineClips({TimelineModel::IsProxyRole});
    if (stack) {
        m_effectStack = stack;
    }
}

Fun ProjectClip::getAudio_lambda()
{
    return [this]() {
        if (KdenliveSettings::audiothumbnails() &&
            (m_clipType == ClipType::AV || m_clipType == ClipType::Audio || (m_clipType == ClipType::Playlist && m_hasAudio)) && m_audioLevels.isEmpty()) {
            // Generate audio levels
            AudioLevelsTask::start(ObjectId(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid()), this, false);
        }
        return true;
    };
}

bool ProjectClip::isIncludedInTimeline()
{
    return !m_registeredClipsByUuid.isEmpty();
}

void ProjectClip::replaceInTimeline()
{
    int updatedDuration = m_resetTimelineOccurences ? getFramePlaytime() : -1;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool pushUndo = false;
    QMapIterator<QUuid, QList<int>> i(m_registeredClipsByUuid);
    QMap<QUuid, int> sequencesToUpdate;
    while (i.hasNext()) {
        i.next();
        QList<int> instances = i.value();
        if (!instances.isEmpty()) {
            auto timeline = pCore->currentDoc()->getTimeline(i.key());
            if (!timeline) {
                if (pCore->projectItemModel()->closing) {
                    break;
                }
                qDebug() << "Error while reloading clip: timeline unavailable";
                Q_ASSERT(false);
            }
            for (auto &cid : instances) {
                if (timeline->requestClipReload(cid, updatedDuration, undo, redo)) {
                    pushUndo = true;
                }
            }
            // If this sequence is used in another sequence, update it too
            if (auto ptr = m_model.lock()) {
                std::shared_ptr<ProjectClip> sClip = std::static_pointer_cast<ProjectItemModel>(ptr)->getSequenceClip(i.key());
                if (sClip && sClip->refCount() > 0) {
                    sequencesToUpdate.insert(i.key(), timeline->duration());
                }
            }
        }
    }
    if (pushUndo && !m_resetTimelineOccurences) {
        pCore->pushUndo(undo, redo, i18n("Adjust timeline clips"));
    }
    m_resetTimelineOccurences = false;
    // Update each sequence clips that embedded this clip
    if (!sequencesToUpdate.isEmpty()) {
        Q_EMIT pCore->bin()->requestUpdateSequences(sequencesToUpdate);
    }
}

void ProjectClip::updateTimelineClips(const QVector<int> &roles)
{
    const QUuid uuid = pCore->currentTimelineId();
    if (m_registeredClipsByUuid.contains(uuid)) {
        QList<int> instances = m_registeredClipsByUuid.value(uuid);
        if (!instances.isEmpty()) {
            auto timeline = pCore->currentDoc()->getTimeline(uuid);
            if (!timeline) {
                if (pCore->projectItemModel()->closing) {
                    return;
                }
                qDebug() << "Error while reloading clip: timeline unavailable";
                Q_ASSERT(false);
            }
            for (auto &cid : instances) {
                timeline->requestClipUpdate(cid, roles);
            }
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
                currentZone.insert(QLatin1String("rating"), QJsonValue(int(clip->rating())));
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

int ProjectClip::getThumbFrame() const
{
    if (m_clipType == ClipType::Timeline) {
        return qMax(0, pCore->currentDoc()->getSequenceProperty(m_sequenceUuid, QStringLiteral("thumbnailFrame")).toInt());
    }
    return qMax(0, getProducerIntProperty(QStringLiteral("kdenlive:thumbnailFrame")));
}

void ProjectClip::getThumbFromPercent(int percent, bool storeFrame)
{
    // extract a maximum of 30 frames for bin preview
    if (percent < 0) {
        int framePos = getThumbFrame();
        if (framePos > 0) {
            QImage thumb = ThumbnailCache::get()->getThumbnail(hashForThumbs(), m_binId, framePos);
            if (!thumb.isNull()) {
                setThumbnail(thumb, -1, -1);
            }
        }
        return;
    }
    int duration = getFramePlaytime();
    int steps = qCeil(qMax(pCore->getCurrentFps(), double(duration) / 30));
    int framePos = duration * percent / 100;
    framePos -= framePos % steps;
    QImage thumb = ThumbnailCache::get()->getThumbnail(hashForThumbs(), m_binId, framePos);
    if (!thumb.isNull()) {
        setThumbnail(thumb, -1, -1);
    } else {
        // Generate percent thumbs
        ObjectId oid(KdenliveObjectType::BinClip, m_binId.toInt(), QUuid());
        if (!pCore->taskManager.hasPendingJob(oid, AbstractTask::CACHEJOB)) {
            CacheTask::start(oid, 30, 0, 0, this);
        }
    }
    if (storeFrame) {
        if (m_clipType == ClipType::Timeline) {
            pCore->currentDoc()->setSequenceProperty(m_sequenceUuid, QStringLiteral("thumbnailFrame"), framePos);
        } else {
            setProducerProperty(QStringLiteral("kdenlive:thumbnailFrame"), framePos);
        }
    }
}

void ProjectClip::setRating(uint rating)
{
    AbstractProjectItem::setRating(rating);
    setProducerProperty(QStringLiteral("kdenlive:rating"), int(rating));
    pCore->currentDoc()->setModified(true);
}

int ProjectClip::getAudioMax(int stream)
{
    const QString key = QString("kdenlive:audio_max%1").arg(stream);
    if (m_masterProducer->property_exists(key.toUtf8().constData())) {
        return m_masterProducer->get_int(key.toUtf8().constData());
    }
    // Process audio max for the stream
    const QString key2 = QString("_kdenlive:audio%1").arg(stream);
    if (!m_masterProducer->property_exists(key2.toUtf8().constData())) {
        return 0;
    }
    const QVector<uint8_t> audioData = *static_cast<QVector<uint8_t> *>(m_masterProducer->get_data(key2.toUtf8().constData()));
    if (audioData.isEmpty()) {
        return 0;
    }
    uint max = *std::max_element(audioData.constBegin(), audioData.constEnd());
    m_masterProducer->set(key.toUtf8().constData(), int(max));
    return int(max);
}

const QVector<uint8_t> ProjectClip::audioFrameCache(int stream)
{
    QVector<uint8_t> audioLevels;
    if (stream == -1) {
        if (m_audioInfo) {
            stream = m_audioInfo->ffmpeg_audio_index();
        } else {
            return audioLevels;
        }
    }
    const QString key = QString("_kdenlive:audio%1").arg(stream);
    if (m_masterProducer->get_data(key.toUtf8().constData())) {
        const QVector<uint8_t> audioData = *static_cast<QVector<uint8_t> *>(m_masterProducer->get_data(key.toUtf8().constData()));
        return audioData;
    } else {
        qDebug() << "=== AUDIO NOT FOUND ";
    }
    return QVector<uint8_t>();

    // TODO
    /*QString key = QString("%1:%2").arg(m_binId).arg(stream);
    QByteArray audioData;
    if (pCore->audioThumbCache.find(key, &audioData)) {
        if (audioData != QByteArray("-")) {
            QDataStream in(audioData);
            in >> audioLevels;
            return audioLevels;
        }
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
            audioLevels << uint8_t(qRed(p));
            audioLevels << uint8_t(qGreen(p));
            audioLevels << uint8_t(qBlue(p));
            audioLevels << uint8_t(qAlpha(p));
        }
        // populate vector
        QDataStream st(&audioData, QIODevice::WriteOnly);
        st << audioLevels;
        pCore->audioThumbCache.insert(key, audioData);
    }
    return audioLevels;*/
}

void ProjectClip::setClipStatus(FileStatus::ClipStatus status)
{
    FileStatus::ClipStatus previousStatus = m_clipStatus;
    AbstractProjectItem::setClipStatus(status);
    updateTimelineClips({TimelineModel::StatusRole});
    if (auto ptr = m_model.lock()) {
        std::shared_ptr<ProjectItemModel> model = std::static_pointer_cast<ProjectItemModel>(ptr);
        model->onItemUpdated(std::static_pointer_cast<ProjectClip>(shared_from_this()), {AbstractProjectItem::IconOverlay});
        if (status == FileStatus::StatusMissing || previousStatus == FileStatus::StatusMissing) {
            model->missingClipTimer.start();
        }
    }
}

void ProjectClip::renameAudioStream(int id, const QString &name)
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
        Q_EMIT updateStreamInfo(streamIndex);
        return true;
    };
    Fun undo = [this, streamIndex, effectName, oldEffect]() {
        if (!oldEffect.isEmpty()) {
            // restore previous parameter value
            addAudioStreamEffect(streamIndex, oldEffect);
        } else {
            removeAudioStreamEffect(streamIndex, effectName);
        }
        Q_EMIT updateStreamInfo(streamIndex);
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
        Q_EMIT updateStreamInfo(streamIndex);
        return true;
    };
    Fun redo = [this, streamIndex, effectName]() {
        removeAudioStreamEffect(streamIndex, effectName);
        Q_EMIT updateStreamInfo(streamIndex);
        return true;
    };
    removeAudioStreamEffect(streamIndex, effectName);
    pCore->pushUndo(undo, redo, i18n("Remove stream effect"));
}

void ProjectClip::addAudioStreamEffect(int streamIndex, const QString effectName)
{
    QString addedEffectName;
    QMap<QString, QString> effectParams;
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
            // Remove existing effects with same name
            int max = p.second->filter_count();
            for (int i = 0; i < max; i++) {
                QScopedPointer<Mlt::Filter> f(p.second->filter(i));
                if (f->get("mlt_service") == addedEffectName) {
                    p.second->detach(*f.get());
                    break;
                }
            }
            Mlt::Filter filt(p.second->get_profile(), addedEffectName.toUtf8().constData());
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
                if (fl->get_int("kdenlive:stream") != 1) {
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

void ProjectClip::updateTimelineOnReload()
{
    const QUuid uuid = pCore->currentTimelineId();
    if (m_registeredClipsByUuid.contains(uuid)) {
        QList<int> instances = m_registeredClipsByUuid.value(uuid);
        if (!instances.isEmpty() && instances.size() < 3) {
            auto timeline = pCore->currentDoc()->getTimeline(uuid);
            if (timeline) {
                for (auto &cid : instances) {
                    if (timeline->getClipPlaytime(cid) > static_cast<int>(frameDuration())) {
                        // reload producer
                        m_resetTimelineOccurences = true;
                        break;
                    }
                }
            }
        }
    }
}

void ProjectClip::updateJobProgress()
{
    if (auto ptr = m_model.lock()) {
        std::static_pointer_cast<ProjectItemModel>(ptr)->onItemUpdated(m_binId, AbstractProjectItem::JobProgress);
    }
}

void ProjectClip::setInvalid()
{
    m_isInvalid = true;
    m_producerLock.unlock();
}

void ProjectClip::updateProxyProducer(const QString &path)
{
    resetProducerProperty(QStringLiteral("_overwriteproxy"));
    setProducerProperty(QStringLiteral("resource"), path);
    reloadProducer(false, true);
}

void ProjectClip::importJsonMarkers(const QString &json)
{
    getMarkerModel()->importFromJson(json, true);
}

const QStringList ProjectClip::enforcedParams() const
{
    QStringList params;
    QStringList paramNames = {QStringLiteral("rotate"), QStringLiteral("autorotate")};
    for (auto &name : paramNames) {
        if (hasProducerProperty(name)) {
            params << QString("%1=%2").arg(name, getProducerProperty(name));
        }
    }
    return params;
}

const QString ProjectClip::baseThumbPath()
{
    return QString("%1/%2/#").arg(m_binId).arg(m_uuid.toString());
}

bool ProjectClip::canBeDropped(const QUuid &uuid) const
{
    if (m_sequenceUuid == uuid) {
        return false;
    }
    if (auto ptr = m_model.lock()) {
        return std::static_pointer_cast<ProjectItemModel>(ptr)->canBeEmbeded(uuid, m_sequenceUuid);
    } else {
        qDebug() << "..... ERROR CANNOT LOCK MODEL";
    }
    return true;
}

const QList<QUuid> ProjectClip::registeredUuids() const
{
    return m_registeredClipsByUuid.keys();
}

const QUuid &ProjectClip::getSequenceUuid() const
{
    return m_sequenceUuid;
}

void ProjectClip::updateDescription()
{
    if (m_clipType == ClipType::TextTemplate) {
        m_description = getProducerProperty(QStringLiteral("templatetext"));
    } else {
        m_description = getProducerProperty(QStringLiteral("kdenlive:description"));
        if (m_description.isEmpty()) {
            m_description = getProducerProperty(QStringLiteral("meta.attr.comment.markup"));
        }
    }
}

QPixmap ProjectClip::pixmap(int framePosition, int width, int height)
{
    // TODO refac this should use the new thumb infrastructure
    QReadLocker lock(&m_producerLock);
    std::unique_ptr<Mlt::Producer> thumbProducer = getThumbProducer();
    if (thumbProducer == nullptr) {
        return QPixmap();
    }
    thumbProducer->seek(framePosition);
    QScopedPointer<Mlt::Frame> frame(thumbProducer->get_frame());
    if (frame == nullptr || !frame->is_valid()) {
        QPixmap p(width, height);
        p.fill(QColor(Qt::red).rgb());
        return p;
    }
    frame->set("consumer.deinterlacer", "onefield");
    frame->set("consumer.top_field_first", -1);
    frame->set("consumer.rescale", "nearest");
    QImage img = KThumb::getFrame(frame.data());
    return QPixmap::fromImage(img /*.scaled(height, width, Qt::KeepAspectRatio)*/);
}
