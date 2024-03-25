/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "cliploadtask.h"
#include "audio/audioStreamInfo.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "mltcontroller/clipcontroller.h"
#include "project/dialogs/slideshowclip.h"
#include "utils/thumbnailcache.hpp"

#include "xml/xml.hpp"
#include <KLocalizedString>
#include <KMessageWidget>
#include <QAction>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QMimeDatabase>
#include <QPainter>
#include <QString>
#include <QTime>
#include <QUuid>
#include <QVariantList>
#include <audio/audioInfo.h>
#include <monitor/monitor.h>
#include <profiles/profilemodel.hpp>

ClipLoadTask::ClipLoadTask(const ObjectId &owner, const QDomElement &xml, bool thumbOnly, int in, int out, QObject *object)
    : AbstractTask(owner, AbstractTask::LOADJOB, object)
    , m_xml(xml)
    , m_in(in)
    , m_out(out)
    , m_thumbOnly(thumbOnly)
{
    m_description = m_thumbOnly ? i18n("Video thumbs") : i18n("Loading clip");
}

ClipLoadTask::~ClipLoadTask() {}

void ClipLoadTask::start(const ObjectId &owner, const QDomElement &xml, bool thumbOnly, int in, int out, QObject *object, bool force,
                         const std::function<void()> &readyCallBack)
{
    ClipLoadTask *task = new ClipLoadTask(owner, xml, thumbOnly, in, out, object);
    if (!thumbOnly && pCore->taskManager.hasPendingJob(owner, AbstractTask::LOADJOB)) {
        delete task;
        task = nullptr;
    }
    if (task) {
        // Otherwise, start a new load task thread.
        task->m_isForce = force;
        connect(task, &ClipLoadTask::taskDone, [readyCallBack]() { QMetaObject::invokeMethod(qApp, [readyCallBack] { readyCallBack(); }); });
        pCore->taskManager.startTask(owner.itemId, task);
    }
}

ClipType::ProducerType ClipLoadTask::getTypeForService(const QString &id, const QString &path)
{
    if (id.isEmpty()) {
        QString ext = path.section(QLatin1Char('.'), -1);
        if (ext == QLatin1String("mlt") || ext == QLatin1String("kdenlive")) {
            return ClipType::Playlist;
        }
        return ClipType::Unknown;
    }
    if (id == QLatin1String("color") || id == QLatin1String("colour")) {
        return ClipType::Color;
    }
    if (id == QLatin1String("kdenlivetitle")) {
        return ClipType::Text;
    }
    if (id == QLatin1String("qtext")) {
        return ClipType::QText;
    }
    if (id == QLatin1String("xml") || id == QLatin1String("consumer")) {
        return ClipType::Playlist;
    }
    if (id == QLatin1String("tractor")) {
        return ClipType::Timeline;
    }
    if (id == QLatin1String("webvfx")) {
        return ClipType::WebVfx;
    }
    if (id == QLatin1String("qml")) {
        return ClipType::Qml;
    }
    return ClipType::Unknown;
}

// static
std::shared_ptr<Mlt::Producer> ClipLoadTask::loadResource(QString resource, const QString &type)
{
    if (!resource.startsWith(type)) {
        resource.prepend(type);
    }
    return std::make_shared<Mlt::Producer>(pCore->getProjectProfile(), nullptr, resource.toUtf8().constData());
}

std::shared_ptr<Mlt::Producer> ClipLoadTask::loadPlaylist(QString &resource)
{
    // since MLT 7.14.0, playlists with different fps can be used in a project without corrupting the profile
    return std::make_shared<Mlt::Producer>(pCore->getProjectProfile(), nullptr, resource.toUtf8().constData());
}

// Read the properties of the xml and pass them to the producer. Note that some properties like resource are ignored
void ClipLoadTask::processProducerProperties(const std::shared_ptr<Mlt::Producer> &prod, const QDomElement &xml)
{
    // TODO: there is some duplication with clipcontroller > updateproducer that also copies properties
    QString value;
    QStringList internalProperties;
    internalProperties << QStringLiteral("bypassDuplicate") << QStringLiteral("resource") << QStringLiteral("mlt_service") << QStringLiteral("audio_index")
                       << QStringLiteral("astream") << QStringLiteral("vstream") << QStringLiteral("video_index") << QStringLiteral("mlt_type")
                       << QStringLiteral("length");
    QDomNodeList props;

    if (xml.tagName() == QLatin1String("producer") || xml.tagName() == QLatin1String("chain")) {
        props = xml.childNodes();
    } else {
        QDomElement elem = xml.firstChildElement(QStringLiteral("chain"));
        if (elem.isNull()) {
            elem = xml.firstChildElement(QStringLiteral("producer"));
        }
        props = elem.childNodes();
    }
    for (int i = 0; i < props.count(); ++i) {
        if (props.at(i).toElement().tagName() != QStringLiteral("property")) {
            continue;
        }
        QString propertyName = props.at(i).toElement().attribute(QStringLiteral("name"));
        if (!internalProperties.contains(propertyName) && !propertyName.startsWith(QLatin1Char('_'))) {
            value = props.at(i).firstChild().nodeValue();
            if (propertyName.startsWith(QLatin1String("kdenlive-force."))) {
                // this is a special forced property, pass it
                propertyName.remove(0, 15);
            }
            prod->set(propertyName.toUtf8().constData(), value.toUtf8().constData());
        }
    }
}

void ClipLoadTask::processSlideShow(std::shared_ptr<Mlt::Producer> producer)
{
    int ttl = Xml::getXmlProperty(m_xml, QStringLiteral("ttl")).toInt();
    QString anim = Xml::getXmlProperty(m_xml, QStringLiteral("animation"));
    bool lowPass = Xml::getXmlProperty(m_xml, QStringLiteral("low-pass"), QStringLiteral("0")).toInt() == 1;
    if (lowPass) {
        auto *blur = new Mlt::Filter(pCore->getProjectProfile(), "avfilter.avgblur");
        if ((blur == nullptr) || !blur->is_valid()) {
            delete blur;
            blur = new Mlt::Filter(pCore->getProjectProfile(), "boxblur");
        }
        if ((blur != nullptr) && blur->is_valid()) {
            producer->attach(*blur);
        }
    }
    if (!anim.isEmpty()) {
        auto *filter = new Mlt::Filter(pCore->getProjectProfile(), "affine");
        if ((filter != nullptr) && filter->is_valid()) {
            int cycle = ttl;
            QString geometry = SlideshowClip::animationToGeometry(anim, cycle);
            if (!geometry.isEmpty()) {
                filter->set("transition.rect", geometry.toUtf8().data());
                filter->set("transition.cycle", cycle);
                filter->set("transition.mirror_off", 1);
                producer->attach(*filter);
            }
        }
    }
    QString fade = Xml::getXmlProperty(m_xml, QStringLiteral("fade"));
    if (fade == QLatin1String("1")) {
        // user wants a fade effect to slideshow
        auto *filter = new Mlt::Filter(pCore->getProjectProfile(), "luma");
        if ((filter != nullptr) && filter->is_valid()) {
            if (ttl != 0) {
                filter->set("cycle", ttl);
            }
            QString luma_duration = Xml::getXmlProperty(m_xml, QStringLiteral("luma_duration"));
            QString luma_file = Xml::getXmlProperty(m_xml, QStringLiteral("luma_file"));
            if (!luma_duration.isEmpty()) {
                filter->set("duration", luma_duration.toInt());
            }
            if (!luma_file.isEmpty()) {
                filter->set("luma.resource", luma_file.toUtf8().constData());
                QString softness = Xml::getXmlProperty(m_xml, QStringLiteral("softness"));
                if (!softness.isEmpty()) {
                    int soft = softness.toInt();
                    filter->set("luma.softness", double(soft) / 100.0);
                }
            }
            producer->attach(*filter);
        }
    }
    QString crop = Xml::getXmlProperty(m_xml, QStringLiteral("crop"));
    if (crop == QLatin1String("1")) {
        // user wants to center crop the slides
        auto *filter = new Mlt::Filter(pCore->getProjectProfile(), "crop");
        if ((filter != nullptr) && filter->is_valid()) {
            filter->set("center", 1);
            producer->attach(*filter);
        }
    }
}

void ClipLoadTask::generateThumbnail(std::shared_ptr<ProjectClip> binClip, std::shared_ptr<Mlt::Producer> producer)
{
    // Fetch thumbnail
    if (m_isCanceled.loadAcquire() || pCore->taskManager.isBlocked()) {
        return;
    }
    int frameNumber = m_in;
    if (frameNumber < 0) {
        frameNumber = binClip->getThumbFrame();
    }
    if (producer->get_int("video_index") > -1) {
        QImage thumb = ThumbnailCache::get()->getThumbnail(binClip->hashForThumbs(), QString::number(m_owner.itemId), frameNumber);
        if (!thumb.isNull()) {
            // Thumbnail found in cache
            qDebug() << "=== FOUND THUMB IN CACHe";
            QMetaObject::invokeMethod(binClip.get(), "setThumbnail", Qt::QueuedConnection, Q_ARG(QImage, thumb), Q_ARG(int, m_in), Q_ARG(int, m_out),
                                      Q_ARG(bool, true));
        } else {
            if (m_isCanceled.loadAcquire() || pCore->taskManager.isBlocked()) {
                return;
            }
            std::unique_ptr<Mlt::Producer> thumbProd = binClip->getThumbProducer();
            if (thumbProd && thumbProd->is_valid()) {
                if (frameNumber > 0) {
                    thumbProd->seek(frameNumber);
                }
                std::unique_ptr<Mlt::Frame> frame(thumbProd->get_frame());
                if ((frame != nullptr) && frame->is_valid()) {
                    frame->set("consumer.deinterlacer", "onefield");
                    frame->set("consumer.top_field_first", -1);
                    frame->set("consumer.rescale", "nearest");
                    int imageHeight(pCore->thumbProfile().height());
                    int imageWidth(pCore->thumbProfile().width());
                    int fullWidth(qRound(imageHeight * pCore->getCurrentDar()));
                    if (m_isCanceled.loadAcquire() || pCore->taskManager.isBlocked()) {
                        return;
                    }
                    QImage result = KThumb::getFrame(frame.get(), imageWidth, imageHeight, fullWidth);
                    if (result.isNull() && !m_isCanceled.loadAcquire()) {
                        qDebug() << "+++++\nINVALID RESULT IMAGE\n++++++++++++++";
                        result = QImage(fullWidth, imageHeight, QImage::Format_ARGB32_Premultiplied);
                        result.fill(Qt::red);
                        QPainter p(&result);
                        p.setPen(Qt::white);
                        p.drawText(0, 0, fullWidth, imageHeight, Qt::AlignCenter, i18n("Invalid"));
                        QMetaObject::invokeMethod(binClip.get(), "setThumbnail", Qt::QueuedConnection, Q_ARG(QImage, result), Q_ARG(int, m_in),
                                                  Q_ARG(int, m_out), Q_ARG(bool, false));
                    } else if (binClip.get() && !m_isCanceled.loadAcquire()) {
                        // We don't follow m_isCanceled there,
                        qDebug() << "=== GOT THUMB FOR: " << m_in << "x" << m_out;
                        QMetaObject::invokeMethod(binClip.get(), "setThumbnail", Qt::QueuedConnection, Q_ARG(QImage, result), Q_ARG(int, m_in),
                                                  Q_ARG(int, m_out), Q_ARG(bool, false));
                        ThumbnailCache::get()->storeThumbnail(QString::number(m_owner.itemId), frameNumber, result, false);
                    }
                }
            }
        }
    }
}

void ClipLoadTask::run()
{
    AbstractTaskDone whenFinished(m_owner.itemId, this);
    if (m_isCanceled.loadAcquire() == 1 || pCore->taskManager.isBlocked()) {
        abort();
        return;
    }
    QMutexLocker lock(&m_runMutex);
    // QThread::currentThread()->setPriority(QThread::HighestPriority);
    if (m_thumbOnly) {
        auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.itemId));
        if (binClip && binClip->statusReady()) {
            if (m_isCanceled.loadAcquire() == 1 || pCore->taskManager.isBlocked()) {
                abort();
                return;
            }
            generateThumbnail(binClip, binClip->originalProducer());
        }
        if (m_isCanceled.loadAcquire() == 1 || pCore->taskManager.isBlocked()) {
            abort();
        }
        return;
    }
    m_running = true;
    Q_EMIT pCore->projectItemModel()->resetPlayOrLoopZone(QString::number(m_owner.itemId));
    QString resource = Xml::getXmlProperty(m_xml, QStringLiteral("resource"));
    qDebug() << "============STARTING LOAD TASK FOR: " << m_owner.itemId << " = " << resource << "\n\n:::::::::::::::::::";
    int duration = 0;
    ClipType::ProducerType type = static_cast<ClipType::ProducerType>(m_xml.attribute(QStringLiteral("type")).toInt());
    QString service = Xml::getXmlProperty(m_xml, QStringLiteral("mlt_service"));
    if (type == ClipType::Unknown) {
        type = getTypeForService(service, resource);
    }
    if (type == ClipType::Playlist && Xml::getXmlProperty(m_xml, QStringLiteral("kdenlive:proxy")).length() > 2) {
        // If this is a proxied playlist, load as AV
        type = ClipType::AV;
        service.clear();
    }
    std::shared_ptr<Mlt::Producer> producer;
    switch (type) {
    case ClipType::Color:
        producer = loadResource(resource, QStringLiteral("color:"));
        break;
    case ClipType::Text:
    case ClipType::TextTemplate: {
        bool ok = false;
        int producerLength = 0;
        QString pLength = Xml::getXmlProperty(m_xml, QStringLiteral("length"));
        if (pLength.isEmpty()) {
            producerLength = m_xml.attribute(QStringLiteral("length")).toInt();
        } else {
            producerLength = pLength.toInt(&ok);
        }
        producer = loadResource(resource, QStringLiteral("kdenlivetitle:"));

        if (!resource.isEmpty()) {
            if (!ok) {
                producerLength = producer->time_to_frames(pLength.toUtf8().constData());
            }
            // Title from .kdenlivetitle file
            QDomDocument txtdoc(QStringLiteral("titledocument"));
            if (Xml::docContentFromFile(txtdoc, resource, false)) {
                if (txtdoc.documentElement().hasAttribute(QStringLiteral("duration"))) {
                    duration = txtdoc.documentElement().attribute(QStringLiteral("duration")).toInt();
                } else if (txtdoc.documentElement().hasAttribute(QStringLiteral("out"))) {
                    duration = txtdoc.documentElement().attribute(QStringLiteral("out")).toInt();
                }
            }
        } else {
            QString xmlDuration = Xml::getXmlProperty(m_xml, QStringLiteral("kdenlive:duration"));
            duration = xmlDuration.toInt(&ok);
            if (!ok) {
                // timecode duration
                duration = producer->time_to_frames(xmlDuration.toUtf8().constData());
            }
        }
        qDebug() << "===== GOT PRODUCER DURATION: " << duration << "; PROD: " << producerLength;
        if (duration <= 0) {
            if (producerLength > 0) {
                duration = producerLength;
            } else {
                duration = pCore->getDurationFromString(KdenliveSettings::title_duration());
            }
        }
        if (producerLength <= 0) {
            producerLength = duration;
        }
        producer->set("length", producerLength);
        producer->set("kdenlive:duration", producer->frames_to_time(duration));
        producer->set("out", producerLength - 1);
    } break;
    case ClipType::QText:
        producer = loadResource(resource, QStringLiteral("qtext:"));
        break;
    case ClipType::Qml: {
        bool ok;
        int producerLength = 0;
        QString pLength = Xml::getXmlProperty(m_xml, QStringLiteral("length"));
        if (pLength.isEmpty()) {
            producerLength = m_xml.attribute(QStringLiteral("length")).toInt();
        } else {
            producerLength = pLength.toInt(&ok);
        }
        if (producerLength <= 0) {
            producerLength = pCore->getDurationFromString(KdenliveSettings::title_duration());
        }
        producer = loadResource(resource, QStringLiteral("qml:"));
        producer->set("length", producerLength);
        producer->set("kdenlive:duration", producer->frames_to_time(producerLength));
        producer->set("out", producerLength - 1);
        break;
    }
    case ClipType::Playlist: {
        producer = loadPlaylist(resource);
        if (producer) {
            QFile f(resource);
            QDomDocument doc;
            doc.setContent(&f, false);
            f.close();
            if (resource.endsWith(QLatin1String(".kdenlive"))) {
                QDomElement pl = doc.documentElement().firstChildElement(QStringLiteral("playlist"));
                if (!pl.isNull()) {
                    QString offsetData = Xml::getXmlProperty(pl, QStringLiteral("kdenlive:docproperties.seekOffset"));
                    if (offsetData.isEmpty() && Xml::getXmlProperty(pl, QStringLiteral("kdenlive:docproperties.version")) == QLatin1String("0.98")) {
                        offsetData = QStringLiteral("30000");
                    }
                    if (!offsetData.isEmpty()) {
                        bool ok = false;
                        int offset = offsetData.toInt(&ok);
                        if (ok) {
                            qDebug() << " / / / FIXING OFFSET DATA: " << offset;
                            offset = producer->get_playtime() - offset - 1;
                            producer->set("out", offset - 1);
                            producer->set("length", offset);
                            producer->set("kdenlive:duration", producer->frames_to_time(offset));
                        }
                    } else {
                        qDebug() << "// NO OFFSET DAT FOUND\n\n";
                    }
                } else {
                    qDebug() << ":_______\n______<nEMPTY PLAYLIST\n----";
                }
            } else if (resource.endsWith(QLatin1String(".mlt"))) {
                // Find the last tractor and check if it has a duration
                QDomElement tractor = doc.documentElement().lastChildElement(QStringLiteral("tractor"));
                QString duration = Xml::getXmlProperty(tractor, QStringLiteral("kdenlive:duration"));
                if (!duration.isEmpty()) {
                    int frames = producer->time_to_frames(duration.toUtf8().constData());
                    producer->set("out", frames - 1);
                    producer->set("length", frames);
                    producer->set("kdenlive:duration", duration.toUtf8().constData());
                }
            }
        }
        break;
    }
    case ClipType::SlideShow:
    case ClipType::Image: {
        resource.prepend(QStringLiteral("qimage:"));
        producer = std::make_shared<Mlt::Producer>(pCore->getProjectProfile(), nullptr, resource.toUtf8().constData());
        break;
    }
    default:
        if (!service.isEmpty()) {
            service.append(QChar(':'));
            if (service == QLatin1String("avformat-novalidate:")) {
                service = QStringLiteral("avformat:");
            }
            producer = loadResource(resource, service);
        } else {
            producer = std::make_shared<Mlt::Producer>(pCore->getProjectProfile(), nullptr, resource.toUtf8().constData());
        }
        break;
    }
    if (m_isCanceled.loadAcquire() == 1 || pCore->taskManager.isBlocked()) {
        abort();
        return;
    }

    if (!producer || producer->is_blank() || !producer->is_valid()) {
        qCDebug(KDENLIVE_LOG) << " / / / / / / / / ERROR / / / / // CANNOT LOAD PRODUCER: " << resource;
        if (producer) {
            producer.reset();
        }
        auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.itemId));
        if (binClip && !binClip->isReloading) {
            QMetaObject::invokeMethod(pCore.get(), "displayBinMessage", Qt::QueuedConnection,
                                      Q_ARG(QString, m_errorMessage.isEmpty() ? i18n("Cannot open file %1", resource) : m_errorMessage),
                                      Q_ARG(int, int(KMessageWidget::Warning)));
        }
        Q_EMIT taskDone();
        abort();
        return;
    }
    const QString mltService = producer->get("mlt_service");
    if (producer->get_length() == INT_MAX && producer->get("eof") == QLatin1String("loop")) {
        // This is a live source or broken clip
        // Check for AV
        ClipType::ProducerType cType = type;
        if (producer) {
            if (mltService.startsWith(QLatin1String("avformat")) && cType == ClipType::Unknown) {
                // Check if it is an audio or video only clip
                if (producer->get_int("video_index") == -1) {
                    cType = ClipType::Audio;
                } else if (producer->get_int("audio_index") == -1) {
                    cType = ClipType::Video;
                } else {
                    cType = ClipType::AV;
                }
            }
            producer.reset();
        }
        QMetaObject::invokeMethod(pCore->bin(), "requestTranscoding", Qt::QueuedConnection, Q_ARG(QString, resource),
                                  Q_ARG(QString, QString::number(m_owner.itemId)), Q_ARG(int, cType), Q_ARG(bool, pCore->bin()->shouldCheckProfile),
                                  Q_ARG(QString, QString()),
                                  Q_ARG(QString, i18n("Duration of file <b>%1</b> cannot be determined.", QFileInfo(resource).fileName())));
        if (pCore->bin()->shouldCheckProfile) {
            pCore->bin()->shouldCheckProfile = false;
        }
        Q_EMIT taskDone();
        abort();
        return;
    }
    // Check external proxies
    if (pCore->currentDoc()->useExternalProxy() && mltService.contains(QLatin1String("avformat")) && !producer->property_exists("kdenlive:proxy")) {
        // We have a camcorder profile, check if we have opened a proxy clip
        QString path = producer->get("resource");
        if (!path.isEmpty() && QFileInfo(path).isRelative() && path != QLatin1String("<tractor>")) {
            path.prepend(pCore->currentDoc()->documentRoot());
            producer->set("resource", path.toUtf8().constData());
        }
        QString original = ProjectClip::getOriginalFromProxy(path);
        if (!original.isEmpty()) {
            // Check that original and proxy have the same count of audio streams (for example Sony's FX6 proxy have only 1 audio stream)
            auto info = std::make_unique<AudioInfo>(producer);
            int proxyAudioStreams = info->size();
            std::shared_ptr<Mlt::Producer> prod(new Mlt::Producer(pCore->getProjectProfile(), "avformat", original.toUtf8().constData()));
            // prod->set("video_index", -1);
            prod->probe();
            auto info2 = std::make_unique<AudioInfo>(prod);
            int sourceStreams = info2->size();
            if (proxyAudioStreams != sourceStreams) {
                // Rebuild a proxy with correct audio streams
                // Use source clip
                producer = std::move(prod);
                producer->set("kdenlive:originalurl", original.toUtf8().constData());
                producer->set("kdenlive:camcorderproxy", path.toUtf8().constData());
                const QString clipName = QFileInfo(original).fileName();
                producer->set("kdenlive:clipname", clipName.toUtf8().constData());
                // Trigger proxy rebuild
                producer->set("_replaceproxy", 1);
                producer->set("_reloadName", 1);
            } else {
                // Match, we opened a proxy clip
                producer->set("kdenlive:proxy", path.toUtf8().constData());
                producer->set("kdenlive:originalurl", original.toUtf8().constData());
                const QString clipName = QFileInfo(original).fileName();
                producer->set("kdenlive:clipname", clipName.toUtf8().constData());
            }
        }
    }
    processProducerProperties(producer, m_xml);
    QString clipName = Xml::getXmlProperty(m_xml, QStringLiteral("kdenlive:clipname"));
    if (clipName.isEmpty()) {
        clipName = QFileInfo(Xml::getXmlProperty(m_xml, QStringLiteral("kdenlive:originalurl"))).fileName();
    }
    if (!clipName.isEmpty()) {
        producer->set("kdenlive:clipname", clipName.toUtf8().constData());
    }
    QString groupId = Xml::getXmlProperty(m_xml, QStringLiteral("kdenlive:folderid"));
    if (!groupId.isEmpty()) {
        producer->set("kdenlive:folderid", groupId.toUtf8().constData());
    }
    int clipOut = 0;
    if (m_xml.hasAttribute(QStringLiteral("out"))) {
        clipOut = m_xml.attribute(QStringLiteral("out")).toInt();
    }
    // setup length here as otherwise default length (currently 15000 frames in MLT) will be taken even if outpoint is larger
    QMimeDatabase db;
    const QString mime = db.mimeTypeForFile(resource).name();
    const bool isGif = mime.contains(QLatin1String("image/gif"));
    if ((duration == 0 && (type == ClipType::Text || type == ClipType::TextTemplate || type == ClipType::QText || type == ClipType::Color ||
                           type == ClipType::Image || type == ClipType::SlideShow)) ||
        (isGif && mltService == QLatin1String("qimage"))) {
        int length;
        if (m_xml.hasAttribute(QStringLiteral("length"))) {
            length = m_xml.attribute(QStringLiteral("length")).toInt();
            clipOut = qMax(1, length - 1);
        } else {
            if (isGif && mltService == QLatin1String("qimage")) {
                length = pCore->getDurationFromString(KdenliveSettings::image_duration());
                clipOut = qMax(1, length - 1);
            } else {
                length = Xml::getXmlProperty(m_xml, QStringLiteral("length")).toInt();
                clipOut -= m_xml.attribute(QStringLiteral("in")).toInt();
                if (length < clipOut) {
                    length = clipOut == 1 ? 1 : clipOut + 1;
                }
            }
        }
        // Pass duration if it was forced
        if (m_xml.hasAttribute(QStringLiteral("duration"))) {
            duration = m_xml.attribute(QStringLiteral("duration")).toInt();
            if (length < duration) {
                length = duration;
                if (clipOut > 0) {
                    clipOut = length - 1;
                }
            }
        }
        if (duration == 0) {
            duration = length;
        }
        producer->set("length", producer->frames_to_time(length, mlt_time_clock));
        int kdenlive_duration = producer->time_to_frames(Xml::getXmlProperty(m_xml, QStringLiteral("kdenlive:duration")).toUtf8().constData());
        if (kdenlive_duration > 0) {
            producer->set("kdenlive:duration", producer->frames_to_time(kdenlive_duration, mlt_time_clock));
        } else {
            producer->set("kdenlive:duration", producer->frames_to_time(producer->get_int("length")));
        }
    }
    if (clipOut > 0) {
        producer->set_in_and_out(m_xml.attribute(QStringLiteral("in")).toInt(), clipOut);
    }

    if (m_xml.hasAttribute(QStringLiteral("templatetext"))) {
        producer->set("templatetext", m_xml.attribute(QStringLiteral("templatetext")).toUtf8().constData());
    }
    if (type == ClipType::SlideShow) {
        processSlideShow(producer);
    }
    double fps = -1;
    bool isVariableFrameRate = false;
    bool seekable = true;
    bool checkProfile = pCore->bin()->shouldCheckProfile;
    if (mltService == QLatin1String("xml") || mltService == QLatin1String("consumer")) {
        // MLT playlist, create producer with blank profile to get real profile info
        QString tmpPath = resource;
        if (tmpPath.startsWith(QLatin1String("consumer:"))) {
            tmpPath = "xml:" + tmpPath.section(QLatin1Char(':'), 1);
        }
        Mlt::Profile original_profile;
        std::unique_ptr<Mlt::Producer> tmpProd(new Mlt::Producer(original_profile, nullptr, tmpPath.toUtf8().constData()));
        original_profile.set_explicit(1);
        double originalFps = original_profile.fps();
        fps = originalFps;
        if (originalFps > 0 && !qFuzzyCompare(originalFps, pCore->getCurrentFps())) {
            int originalLength = tmpProd->get_length();
            int fixedLength = int(originalLength * pCore->getCurrentFps() / originalFps);
            producer->set("length", fixedLength);
            producer->set("out", fixedLength - 1);
        }
    } else if (mltService.startsWith(QLatin1String("avformat"))) {
        // Start probe to init properties
        int vindex = producer->get_int("video_index");
        bool hasAudio = false;
        bool hasVideo = false;
        // Work around MLT freeze on files with cover art
        if (vindex > -1) {
            QString key = QString("meta.media.%1.stream.frame_rate").arg(vindex);
            fps = producer->get_double(key.toLatin1().constData());
            key = QString("meta.media.%1.codec.name").arg(vindex);
            QString codec_name = producer->get(key.toLatin1().constData());
            key = QString("meta.media.%1.codec.frame_rate").arg(vindex);
            QString frame_rate = producer->get(key.toLatin1().constData());
            if (codec_name == QLatin1String("png") || (codec_name == "mjpeg" && frame_rate == QLatin1String("90000"))) {
                // Cover art
                producer->set("video_index", -1);
                producer->set("set.test_image", 1);
                vindex = -1;
            }
        }
        // Check audio / video
        producer->probe();
        hasAudio = producer->get_int("video_index") > -1;
        hasVideo = producer->get_int("audio_index") > -1;
        if (hasAudio) {
            if (hasVideo) {
                producer->set("kdenlive:clip_type", 0);
            } else {
                producer->set("kdenlive:clip_type", 1);
            }
        } else if (hasVideo) {
            producer->set("kdenlive:clip_type", 2);
        }
        // Check if file is seekable
        seekable = producer->get_int("seekable");
        if (vindex <= -1) {
            checkProfile = false;
        }
        if (!seekable) {
            if (checkProfile) {
                pCore->bin()->shouldCheckProfile = false;
            }
            ClipType::ProducerType cType = type;
            if (cType == ClipType::Unknown) {
                // Check if it is an audio or video only clip
                if (!hasVideo) {
                    cType = ClipType::Audio;
                } else if (!hasAudio) {
                    cType = ClipType::Video;
                } else {
                    cType = ClipType::AV;
                }
            }
            QMetaObject::invokeMethod(pCore->bin(), "requestTranscoding", Qt::QueuedConnection, Q_ARG(QString, resource),
                                      Q_ARG(QString, QString::number(m_owner.itemId)), Q_ARG(int, cType), Q_ARG(bool, checkProfile), Q_ARG(QString, QString()),
                                      Q_ARG(QString, i18n("File <b>%1</b> is not seekable.", QFileInfo(resource).fileName())));
        }

        // Check for variable frame rate
        isVariableFrameRate = producer->get_int("meta.media.variable_frame_rate");
        if (isVariableFrameRate && seekable) {
            if (checkProfile) {
                pCore->bin()->shouldCheckProfile = false;
            }
            QString adjustedFpsString;
            if (fps > 0) {
                int integerFps = qRound(fps);
                adjustedFpsString = QString("-%1fps").arg(integerFps);
            }
            ClipType::ProducerType cType = type;
            if (cType == ClipType::Unknown) {
                // Check if it is an audio or video only clip
                if (!hasVideo) {
                    cType = ClipType::Audio;
                } else if (!hasAudio) {
                    cType = ClipType::Video;
                } else {
                    cType = ClipType::AV;
                }
            }
            QMetaObject::invokeMethod(pCore->bin(), "requestTranscoding", Qt::QueuedConnection, Q_ARG(QString, resource),
                                      Q_ARG(QString, QString::number(m_owner.itemId)), Q_ARG(int, cType), Q_ARG(bool, checkProfile),
                                      Q_ARG(QString, adjustedFpsString),
                                      Q_ARG(QString, i18n("File <b>%1</b> has a variable frame rate.", QFileInfo(resource).fileName())));
        }

        if (fps <= 0 && !m_isCanceled.loadAcquire()) {
            if (producer->get_double("meta.media.frame_rate_den") > 0) {
                fps = producer->get_double("meta.media.frame_rate_num") / producer->get_double("meta.media.frame_rate_den");
            } else {
                fps = producer->get_double("source_fps");
            }
        }
    }
    if (fps <= 0 && type == ClipType::Unknown) {
        // something wrong, maybe audio file with embedded image
        if (mime.startsWith(QLatin1String("audio"))) {
            producer->set("video_index", -1);
        }
    }
    if (!m_isCanceled.loadAcquire()) {
        auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.itemId));
        if (binClip) {
            const QByteArray xmlData = ClipController::producerXml(*producer.get(), true, false);
            bool replaceProxy = false;
            bool replaceName = false;
            if (producer->property_exists("_replaceproxy")) {
                replaceProxy = true;
            }
            if (producer->property_exists("_reloadName")) {
                replaceName = true;
            }
            // Reset produccer to get rid of cached frame
            QMutexLocker lock(&pCore->xmlMutex);
            producer.reset(new Mlt::Producer(pCore->getProjectProfile(), "xml-string", xmlData.constData()));
            lock.unlock();
            if (replaceProxy) {
                producer->set("_replaceproxy", 1);
            }
            if (replaceName) {
                producer->set("_reloadName", 1);
            }
            QMetaObject::invokeMethod(binClip.get(), "setProducer", Qt::QueuedConnection, Q_ARG(std::shared_ptr<Mlt::Producer>, std::move(producer)),
                                      Q_ARG(bool, true));
            if (checkProfile && !isVariableFrameRate && seekable) {
                pCore->bin()->shouldCheckProfile = false;
                QMetaObject::invokeMethod(pCore->bin(), "slotCheckProfile", Qt::QueuedConnection, Q_ARG(QString, QString::number(m_owner.itemId)));
            }
            Q_EMIT taskDone();
            return;
        }
    }
    // Might be aborted by profile switch
    abort();
    return;
}

void ClipLoadTask::abort()
{
    m_progress = 100;
    if (pCore->taskManager.isBlocked()) {
        return;
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    if (!m_softDelete && !m_thumbOnly) {
        auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.itemId));
        if (binClip) {
            QMetaObject::invokeMethod(binClip.get(), "setInvalid", Qt::QueuedConnection);
            if (!m_isCanceled.loadAcquire() && !binClip->isReloading) {
                // User tried to add an invalid clip, remove it.
                pCore->projectItemModel()->requestBinClipDeletion(binClip, undo, redo);
            } else {
                // An existing clip just became invalid, mark it as missing.
                binClip->setClipStatus(FileStatus::StatusMissing);
            }
        }
    }
}
