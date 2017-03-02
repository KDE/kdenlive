/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
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

#include "producerqueue.h"
#include "clipcontroller.h"
#include "bincontroller.h"
#include "kdenlivesettings.h"
#include "bin/projectclip.h"
#include "doc/kthumb.h"
#include "dialogs/profilesdialog.h"
#include "project/dialogs/slideshowclip.h"
#include "timeline/clip.h"

#include <QtConcurrent>

ProducerQueue::ProducerQueue(BinController *controller) : QObject(controller)
    , m_binController(controller)
{
    connect(this, SIGNAL(multiStreamFound(QString, QList<int>, QList<int>, stringMap)), this, SLOT(slotMultiStreamProducerFound(QString, QList<int>, QList<int>, stringMap)));
    connect(this, &ProducerQueue::refreshTimelineProducer, m_binController, &BinController::replaceTimelineProducer);
}

ProducerQueue::~ProducerQueue()
{
    abortOperations();
}

void ProducerQueue::getFileProperties(const QDomElement &xml, const QString &clipId, int imageHeight, bool replaceProducer)
{
    // Make sure we don't request the info for same clip twice
    m_infoMutex.lock();
    if (m_processingClipId.contains(clipId)) {
        m_infoMutex.unlock();
        return;
    }
    for (int i = 0; i < m_requestList.count(); ++i) {
        if (m_requestList.at(i).clipId == clipId) {
            // Clip is already queued
            m_infoMutex.unlock();
            return;
        }
    }
    requestClipInfo info;
    info.xml = xml;
    info.clipId = clipId;
    info.imageHeight = imageHeight;
    info.replaceProducer = replaceProducer;
    m_requestList.append(info);
    m_infoMutex.unlock();
    if (!m_infoThread.isRunning()) {
        m_infoThread = QtConcurrent::run(this, &ProducerQueue::processFileProperties);
    }
}

void ProducerQueue::forceProcessing(const QString &id)
{
    // Make sure we load the clip producer now so that we can use it in timeline
    QList<requestClipInfo> requestListCopy;
    if (m_processingClipId.contains(id)) {
        m_infoMutex.lock();
        requestListCopy = m_requestList;
        m_requestList.clear();
        m_infoMutex.unlock();
        m_infoThread.waitForFinished();
        emit infoProcessingFinished();
    } else {
        m_infoMutex.lock();
        for (int i = 0; i < m_requestList.count(); ++i) {
            requestClipInfo info = m_requestList.at(i);
            if (info.clipId == id) {
                m_requestList.removeAt(i);
                requestListCopy = m_requestList;
                m_requestList.clear();
                m_requestList.append(info);
                break;
            }
        }
        m_infoMutex.unlock();
        if (!m_infoThread.isRunning()) {
            m_infoThread = QtConcurrent::run(this, &ProducerQueue::processFileProperties);
        }
        m_infoThread.waitForFinished();
        emit infoProcessingFinished();
    }

    m_infoMutex.lock();
    m_requestList.append(requestListCopy);
    m_infoMutex.unlock();
    if (!m_infoThread.isRunning()) {
        m_infoThread = QtConcurrent::run(this, &ProducerQueue::processFileProperties);
    }
}

void ProducerQueue::slotProcessingDone(const QString &id)
{
    QMutexLocker lock(&m_infoMutex);
    m_processingClipId.removeAll(id);
}

bool ProducerQueue::isProcessing(const QString &id)
{
    if (m_processingClipId.contains(id)) {
        return true;
    }
    QMutexLocker lock(&m_infoMutex);
    for (int i = 0; i < m_requestList.count(); ++i) {
        if (m_requestList.at(i).clipId == id) {
            return true;
        }
    }
    return false;
}

void ProducerQueue::processFileProperties()
{
    requestClipInfo info;
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    bool forceThumbScale = m_binController->profile()->sar() != 1;
    while (!m_requestList.isEmpty()) {
        m_infoMutex.lock();
        info = m_requestList.takeFirst();
        if (info.xml.hasAttribute(QStringLiteral("thumbnailOnly")) || info.xml.hasAttribute(QStringLiteral("refreshOnly"))) {
            m_infoMutex.unlock();
            // Special case, we just want the thumbnail for existing producer
            Mlt::Producer *prod = new Mlt::Producer(*m_binController->getBinProducer(info.clipId));
            if (!prod || !prod->is_valid()) {
                continue;
            }
            // Check if we are using GPU accel, then we need to use alternate producer
            if (KdenliveSettings::gpu_accel()) {
                QString service = prod->get("mlt_service");
                QString res = prod->get("resource");
                delete prod;
                prod = new Mlt::Producer(*m_binController->profile(), service.toUtf8().constData(), res.toUtf8().constData());
                Mlt::Filter scaler(*m_binController->profile(), "swscale");
                Mlt::Filter converter(*m_binController->profile(), "avcolor_space");
                prod->attach(scaler);
                prod->attach(converter);
            }
            int frameNumber = ProjectClip::getXmlProperty(info.xml, QStringLiteral("kdenlive:thumbnailFrame"), QStringLiteral("-1")).toInt();
            if (frameNumber > 0) {
                prod->seek(frameNumber);
            }
            Mlt::Frame *frame = prod->get_frame();
            if (frame && frame->is_valid()) {
                int fullWidth = info.imageHeight * m_binController->profile()->dar() + 0.5;
                QImage img = KThumb::getFrame(frame, fullWidth, info.imageHeight, forceThumbScale);
                emit replyGetImage(info.clipId, img);
            }
            delete frame;
            delete prod;
            if (info.xml.hasAttribute(QStringLiteral("refreshOnly"))) {
                // inform timeline about change
                emit refreshTimelineProducer(info.clipId);
            }
            continue;
        }
        m_processingClipId.append(info.clipId);
        m_infoMutex.unlock();
        //TODO: read all xml meta.kdenlive properties into a QMap or an MLT::Properties and pass them to the newly created producer

        QString path;
        bool proxyProducer;
        QString proxy = ProjectClip::getXmlProperty(info.xml, QStringLiteral("kdenlive:proxy"));
        if (!proxy.isEmpty()) {
            if (proxy == QLatin1String("-")) {
                path = ProjectClip::getXmlProperty(info.xml, QStringLiteral("kdenlive:originalurl"));
                if (QFileInfo(path).isRelative()) {
                    path.prepend(m_binController->documentRoot());
                }
                proxyProducer = false;
            } else {
                path = proxy;
                // Check for missing proxies
                if (QFileInfo(path).size() <= 0) {
                    // proxy is missing, re-create it
                    emit requestProxy(info.clipId);
                    proxyProducer = false;
                    //path = info.xml.attribute("resource");
                    path = ProjectClip::getXmlProperty(info.xml, QStringLiteral("resource"));
                } else {
                    proxyProducer = true;
                }
            }
        } else {
            path = ProjectClip::getXmlProperty(info.xml, QStringLiteral("resource"));
            //path = info.xml.attribute("resource");
            proxyProducer = false;
        }
        //qCDebug(KDENLIVE_LOG)<<" / / /CHECKING PRODUCER PATH: "<<path;
        QUrl url = QUrl::fromLocalFile(path);
        Mlt::Producer *producer = nullptr;
        ClipType type = (ClipType)info.xml.attribute(QStringLiteral("type")).toInt();
        if (type == Unknown) {
            type = getTypeForService(ProjectClip::getXmlProperty(info.xml, QStringLiteral("mlt_service")), path);
        }
        if (type == Color) {
            path.prepend(QStringLiteral("color:"));
            producer = new Mlt::Producer(*m_binController->profile(), nullptr, path.toUtf8().constData());
        } else if (type == Text || type == TextTemplate) {
            path.prepend(QStringLiteral("kdenlivetitle:"));
            producer = new Mlt::Producer(*m_binController->profile(), nullptr, path.toUtf8().constData());
        } else if (type == QText) {
            path.prepend(QStringLiteral("qtext:"));
            producer = new Mlt::Producer(*m_binController->profile(), nullptr, path.toUtf8().constData());
        } else if (type == Playlist && !proxyProducer) {
            //TODO: "xml" seems to corrupt project fps if different, and "consumer" crashed on audio transition
            Mlt::Profile *xmlProfile = new Mlt::Profile();
            xmlProfile->set_explicit(false);
            MltVideoProfile projectProfile = ProfilesDialog::getVideoProfile(*m_binController->profile());
            //path.prepend("consumer:");
            producer = new Mlt::Producer(*xmlProfile, "xml", path.toUtf8().constData());
            if (!producer->is_valid()) {
                delete producer;
                delete xmlProfile;
                m_processingClipId.removeAll(info.clipId);
                emit removeInvalidClip(info.clipId, info.replaceProducer);
                continue;
            }
            MltVideoProfile clipProfile = ProfilesDialog::getVideoProfile(*xmlProfile);
            delete producer;
            delete xmlProfile;
            if (clipProfile.isCompatible(projectProfile)) {
                // We can use the "xml" producer since profile is the same (using it with different profiles corrupts the project.
                // Beware that "consumer" currently crashes on audio mixes!
                path.prepend(QStringLiteral("xml:"));
            } else {
                path.prepend(QStringLiteral("consumer:"));
                // This is currently crashing so I guess we'd better reject it for now
                m_processingClipId.removeAll(info.clipId);
                emit removeInvalidClip(info.clipId, info.replaceProducer, i18n("Cannot import playlists with different profile."));
                continue;
            }
            m_binController->profile()->set_explicit(true);
            producer = new Mlt::Producer(*m_binController->profile(), nullptr, path.toUtf8().constData());
        } else if (type == SlideShow) {
            producer = new Mlt::Producer(*m_binController->profile(), nullptr, path.toUtf8().constData());
        } else if (!url.isValid()) {
            //WARNING: when is this case used? Not sure it is working.. JBM/
            QDomDocument doc;
            QDomElement mlt = doc.createElement(QStringLiteral("mlt"));
            QDomElement play = doc.createElement(QStringLiteral("playlist"));
            play.setAttribute(QStringLiteral("id"), QStringLiteral("playlist0"));
            doc.appendChild(mlt);
            mlt.appendChild(play);
            play.appendChild(doc.importNode(info.xml, true));
            QDomElement tractor = doc.createElement(QStringLiteral("tractor"));
            tractor.setAttribute(QStringLiteral("id"), QStringLiteral("tractor0"));
            QDomElement track = doc.createElement(QStringLiteral("track"));
            track.setAttribute(QStringLiteral("producer"), QStringLiteral("playlist0"));
            tractor.appendChild(track);
            mlt.appendChild(tractor);
            producer = new Mlt::Producer(*m_binController->profile(), "xml-string", doc.toString().toUtf8().constData());
        } else {
            producer = new Mlt::Producer(*m_binController->profile(), nullptr, path.toUtf8().constData());
            if (producer->is_valid() && info.xml.hasAttribute(QStringLiteral("checkProfile")) && producer->get_int("video_index") > -1) {
                // Check if clip profile matches
                QString service = producer->get("mlt_service");
                // Check for image producer
                if (service == QLatin1String("qimage") || service == QLatin1String("pixbuf")) {
                    // This is an image, create profile from image size
                    int width = producer->get_int("meta.media.width");
                    int height = producer->get_int("meta.media.height");
                    if (width > 100 && height > 100) {
                        MltVideoProfile projectProfile = ProfilesDialog::getVideoProfile(*m_binController->profile());
                        projectProfile.width = width;
                        projectProfile.height = height;
                        projectProfile.sample_aspect_num = 1;
                        projectProfile.sample_aspect_den = 1;
                        projectProfile.display_aspect_num = width;
                        projectProfile.display_aspect_den = height;
                        projectProfile.description.clear();
                        //delete producer;
                        //m_processingClipId.removeAll(info.clipId);
                        info.xml.removeAttribute(QStringLiteral("checkProfile"));
                        emit switchProfile(projectProfile, info.clipId, info.xml);
                    } else {
                        // Very small image, we probably don't want to use this as profile
                    }
                } else if (service.contains(QStringLiteral("avformat"))) {
                    Mlt::Profile *blankProfile = new Mlt::Profile();
                    blankProfile->set_explicit(false);
                    if (KdenliveSettings::gpu_accel()) {
                        Clip clp(*producer);
                        Mlt::Producer *glProd = clp.softClone(ClipController::getPassPropertiesList());
                        Mlt::Filter scaler(*m_binController->profile(), "swscale");
                        Mlt::Filter converter(*m_binController->profile(), "avcolor_space");
                        glProd->attach(scaler);
                        glProd->attach(converter);
                        blankProfile->from_producer(*glProd);
                        delete glProd;
                    }
                    else {
                        blankProfile->from_producer(*producer);
                    }
                    MltVideoProfile clipProfile = ProfilesDialog::getVideoProfile(*blankProfile);
                    MltVideoProfile projectProfile = ProfilesDialog::getVideoProfile(*m_binController->profile());
                    clipProfile.adjustWidth();
                    if (clipProfile != projectProfile) {
                        // Profiles do not match, propose profile adjustment
                        //delete producer;
                        delete blankProfile;
                        //m_processingClipId.removeAll(info.clipId);
                        info.xml.removeAttribute(QStringLiteral("checkProfile"));
                        emit switchProfile(clipProfile, info.clipId, info.xml);
                    } else if (KdenliveSettings::default_profile().isEmpty()) {
                        // Confirm default project format
                        KdenliveSettings::setDefault_profile(KdenliveSettings::current_profile());
                    }
                }
            }
        }
        if (producer == nullptr || producer->is_blank() || !producer->is_valid()) {
            qCDebug(KDENLIVE_LOG) << " / / / / / / / / ERROR / / / / // CANNOT LOAD PRODUCER: " << path;
            m_processingClipId.removeAll(info.clipId);
            if (proxyProducer) {
                // Proxy file is corrupted
                emit removeInvalidProxy(info.clipId, false);
            } else {
                emit removeInvalidClip(info.clipId, info.replaceProducer);
            }
            delete producer;
            continue;
        }
        // Pass useful properties
        processProducerProperties(producer, info.xml);
        QString clipName = ProjectClip::getXmlProperty(info.xml, QStringLiteral("kdenlive:clipname"));
        if (!clipName.isEmpty()) {
            producer->set("kdenlive:clipname", clipName.toUtf8().constData());
        }
        QString groupId = ProjectClip::getXmlProperty(info.xml, QStringLiteral("kdenlive:folderid"));
        if (!groupId.isEmpty()) {
            producer->set("kdenlive:folderid", groupId.toUtf8().constData());
        }

        if (proxyProducer && info.xml.hasAttribute(QStringLiteral("proxy_out"))) {
            producer->set("length", info.xml.attribute(QStringLiteral("proxy_out")).toInt() + 1);
            producer->set("out", info.xml.attribute(QStringLiteral("proxy_out")).toInt());
            if (producer->get_out() != info.xml.attribute(QStringLiteral("proxy_out")).toInt()) {
                // Proxy file length is different than original clip length, this will corrupt project so disable this proxy clip
                qCDebug(KDENLIVE_LOG) << "/ // PROXY LENGTH MISMATCH, DELETE PRODUCER";
                m_processingClipId.removeAll(info.clipId);
                emit removeInvalidProxy(info.clipId, true);
                delete producer;
                continue;
            }
        }
        //TODO: handle forced properties
        /*if (info.xml.hasAttribute("force_aspect_ratio")) {
            double aspect = info.xml.attribute("force_aspect_ratio").toDouble();
            if (aspect > 0) producer->set("force_aspect_ratio", aspect);
        }

        if (info.xml.hasAttribute("force_aspect_num") && info.xml.hasAttribute("force_aspect_den")) {
            int width = info.xml.attribute("frame_size").section('x', 0, 0).toInt();
            int height = info.xml.attribute("frame_size").section('x', 1, 1).toInt();
            int aspectNumerator = info.xml.attribute("force_aspect_num").toInt();
            int aspectDenominator = info.xml.attribute("force_aspect_den").toInt();
            if (aspectDenominator != 0 && width != 0)
                producer->set("force_aspect_ratio", double(height) * aspectNumerator / aspectDenominator / width);
        }

        if (info.xml.hasAttribute("force_fps")) {
            double fps = info.xml.attribute("force_fps").toDouble();
            if (fps > 0) producer->set("force_fps", fps);
        }

        if (info.xml.hasAttribute("force_progressive")) {
            bool ok;
            int progressive = info.xml.attribute("force_progressive").toInt(&ok);
            if (ok) producer->set("force_progressive", progressive);
        }
        if (info.xml.hasAttribute("force_tff")) {
            bool ok;
            int fieldOrder = info.xml.attribute("force_tff").toInt(&ok);
            if (ok) producer->set("force_tff", fieldOrder);
        }
        if (info.xml.hasAttribute("threads")) {
            int threads = info.xml.attribute("threads").toInt();
            if (threads != 1) producer->set("threads", threads);
        }
        if (info.xml.hasAttribute("video_index")) {
            int vindex = info.xml.attribute("video_index").toInt();
            if (vindex != 0) producer->set("video_index", vindex);
        }
        if (info.xml.hasAttribute("audio_index")) {
            int aindex = info.xml.attribute("audio_index").toInt();
            if (aindex != 0) producer->set("audio_index", aindex);
        }
        if (info.xml.hasAttribute("force_colorspace")) {
            int colorspace = info.xml.attribute("force_colorspace").toInt();
            if (colorspace != 0) producer->set("force_colorspace", colorspace);
        }
        if (info.xml.hasAttribute("full_luma")) {
            int full_luma = info.xml.attribute("full_luma").toInt();
            if (full_luma != 0) producer->set("set.force_full_luma", full_luma);
        }*/

        int clipOut = 0;
        int duration = 0;
        if (info.xml.hasAttribute(QStringLiteral("out"))) {
            clipOut = info.xml.attribute(QStringLiteral("out")).toInt();
        }
        // setup length here as otherwise default length (currently 15000 frames in MLT) will be taken even if outpoint is larger
        if (type == Color || type == Text || type == TextTemplate || type == QText || type == Image || type == SlideShow) {
            int length;
            if (info.xml.hasAttribute(QStringLiteral("length"))) {
                length = info.xml.attribute(QStringLiteral("length")).toInt();
                clipOut = qMax(1, length - 1);
            } else {
                length = EffectsList::property(info.xml, QStringLiteral("length")).toInt();
                clipOut -= info.xml.attribute(QStringLiteral("in")).toInt();
                if (length < clipOut) {
                    length = clipOut == 1 ? 1 : clipOut + 1;
                }
            }
            // Pass duration if it was forced
            if (info.xml.hasAttribute(QStringLiteral("duration"))) {
                duration = info.xml.attribute(QStringLiteral("duration")).toInt();
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
            producer->set("length", length);
            int kdenlive_duration = EffectsList::property(info.xml, QStringLiteral("kdenlive:duration")).toInt();
            producer->set("kdenlive:duration", kdenlive_duration > 0 ? kdenlive_duration : length);
        }
        if (clipOut > 0) {
            producer->set_in_and_out(info.xml.attribute(QStringLiteral("in")).toInt(), clipOut);
        }

        if (info.xml.hasAttribute(QStringLiteral("templatetext"))) {
            producer->set("templatetext", info.xml.attribute(QStringLiteral("templatetext")).toUtf8().constData());
        }

        int fullWidth = info.imageHeight * m_binController->profile()->dar() + 0.5;
        int frameNumber = ProjectClip::getXmlProperty(info.xml, QStringLiteral("kdenlive:thumbnailFrame"), QStringLiteral("-1")).toInt();

        if ((!info.replaceProducer && !EffectsList::property(info.xml, QStringLiteral("kdenlive:file_hash")).isEmpty()) || proxyProducer) {
            // Clip  already has all properties
            // We want to replace an existing producer. We MUST NOT set the producer's id property until
            // the old one has been removed.
            if (proxyProducer) {
                // Recreate clip thumb
                Mlt::Frame *frame = nullptr;
                QImage img;
                if (KdenliveSettings::gpu_accel()) {
                    Clip clp(*producer);
                    Mlt::Producer *glProd = clp.softClone(ClipController::getPassPropertiesList());
                    if (frameNumber > 0) {
                        glProd->seek(frameNumber);
                    }
                    Mlt::Filter scaler(*m_binController->profile(), "swscale");
                    Mlt::Filter converter(*m_binController->profile(), "avcolor_space");
                    glProd->attach(scaler);
                    glProd->attach(converter);
                    frame = glProd->get_frame();
                    if (frame && frame->is_valid()) {
                        img = KThumb::getFrame(frame, fullWidth, info.imageHeight);
                        emit replyGetImage(info.clipId, img);
                    }
                    delete glProd;
                } else {
                    if (frameNumber > 0) {
                        producer->seek(frameNumber);
                    }
                    frame = producer->get_frame();
                    if (frame && frame->is_valid()) {
                        img = KThumb::getFrame(frame, fullWidth, info.imageHeight, forceThumbScale);
                        emit replyGetImage(info.clipId, img);
                    }
                }
                if (frame) {
                    delete frame;
                }
            }
            // replace clip
            m_processingClipId.removeAll(info.clipId);

            // Store original properties in a kdenlive: prefixed format
            QDomNodeList props = info.xml.elementsByTagName(QStringLiteral("property"));
            for (int i = 0; i < props.count(); ++i) {
                QDomElement e = props.at(i).toElement();
                QString name = e.attribute(QStringLiteral("name"));
                if (name.startsWith(QLatin1String("meta."))) {
                    name.prepend(QStringLiteral("kdenlive:"));
                    producer->set(name.toUtf8().constData(), e.firstChild().nodeValue().toUtf8().constData());
                }
            }
            m_binController->replaceProducer(info.clipId, *producer);
            emit gotFileProperties(info, nullptr);
            continue;
        }
        // We are not replacing an existing producer, so set the id
        producer->set("id", info.clipId.toUtf8().constData());
        stringMap filePropertyMap;
        stringMap metadataPropertyMap;
        char property[200];

        if (frameNumber > 0) {
            producer->seek(frameNumber);
        }
        duration = duration > 0 ? duration : producer->get_playtime();
        //qCDebug(KDENLIVE_LOG) << "///////  PRODUCER: " << url.path() << " IS: " << producer->get_playtime();
        if (type == SlideShow) {
            int ttl = EffectsList::property(info.xml, QStringLiteral("ttl")).toInt();
            QString anim = EffectsList::property(info.xml, QStringLiteral("animation"));
            if (!anim.isEmpty()) {
                Mlt::Filter *filter = new Mlt::Filter(*m_binController->profile(), "affine");
                if (filter && filter->is_valid()) {
                    int cycle = ttl;
                    QString geometry = SlideshowClip::animationToGeometry(anim, cycle);
                    if (!geometry.isEmpty()) {
                        if (anim.contains(QStringLiteral("low-pass"))) {
                            Mlt::Filter *blur = new Mlt::Filter(*m_binController->profile(), "boxblur");
                            if (blur && blur->is_valid()) {
                                producer->attach(*blur);
                            }
                        }
                        filter->set("transition.geometry", geometry.toUtf8().data());
                        filter->set("transition.cycle", cycle);
                        producer->attach(*filter);
                    }
                }
            }
            QString fade = EffectsList::property(info.xml, QStringLiteral("fade"));
            if (fade == QLatin1String("1")) {
                // user wants a fade effect to slideshow
                Mlt::Filter *filter = new Mlt::Filter(*m_binController->profile(), "luma");
                if (filter && filter->is_valid()) {
                    if (ttl) {
                        filter->set("cycle", ttl);
                    }
                    QString luma_duration = EffectsList::property(info.xml, QStringLiteral("luma_duration"));
                    QString luma_file = EffectsList::property(info.xml, QStringLiteral("luma_file"));
                    if (!luma_duration.isEmpty()) {
                        filter->set("duration", luma_duration.toInt());
                    }
                    if (!luma_file.isEmpty()) {
                        filter->set("luma.resource", luma_file.toUtf8().constData());
                        QString softness = EffectsList::property(info.xml, QStringLiteral("softness"));
                        if (!softness.isEmpty()) {
                            int soft = softness.toInt();
                            filter->set("luma.softness", (double) soft / 100.0);
                        }
                    }
                    producer->attach(*filter);
                }
            }
            QString crop = EffectsList::property(info.xml, QStringLiteral("crop"));
            if (crop == QLatin1String("1")) {
                // user wants to center crop the slides
                Mlt::Filter *filter = new Mlt::Filter(*m_binController->profile(), "crop");
                if (filter && filter->is_valid()) {
                    filter->set("center", 1);
                    producer->attach(*filter);
                }
            }
        }
        int vindex = -1;
        const QString mltService = producer->get("mlt_service");
        if (mltService == QLatin1String("xml") || mltService == QLatin1String("consumer")) {
            // MLT playlist, create producer with blank profile to get real profile info
            if (path.startsWith(QLatin1String("consumer:"))) {
                path = "xml:" + path.section(QLatin1Char(':'), 1);
            }
            Mlt::Profile original_profile;
            Mlt::Producer *tmpProd = new Mlt::Producer(original_profile, nullptr, path.toUtf8().constData());
            original_profile.set_explicit(true);
            filePropertyMap[QStringLiteral("progressive")] = QString::number(original_profile.progressive());
            filePropertyMap[QStringLiteral("colorspace")] = QString::number(original_profile.colorspace());
            filePropertyMap[QStringLiteral("fps")] = QString::number(original_profile.fps());
            filePropertyMap[QStringLiteral("aspect_ratio")] = QString::number(original_profile.sar());
            double originalFps = original_profile.fps();
            if (originalFps > 0 && originalFps != m_binController->profile()->fps()) {
                // Warning, MLT detects an incorrect length in producer consumer when producer's fps != project's fps
                //TODO: report bug to MLT
                delete tmpProd;
                tmpProd = new Mlt::Producer(original_profile, nullptr, path.toUtf8().constData());
                int originalLength = tmpProd->get_length();
                int fixedLength = (int)(originalLength * m_binController->profile()->fps() / originalFps);
                producer->set("length", fixedLength);
                producer->set("out", fixedLength - 1);
            }
            delete tmpProd;
        } else if (mltService == QLatin1String("avformat")) {
            // Get frame rate
            vindex = producer->get_int("video_index");
            // List streams
            int streams = producer->get_int("meta.media.nb_streams");
            QList<int> audio_list;
            QList<int> video_list;
            for (int i = 0; i < streams; ++i) {
                QByteArray propertyName = QStringLiteral("meta.media.%1.stream.type").arg(i).toLocal8Bit();
                QString type = producer->get(propertyName.data());
                if (type == QLatin1String("audio")) {
                    audio_list.append(i);
                } else if (type == QLatin1String("video")) {
                    video_list.append(i);
                }
            }

            if (!info.xml.hasAttribute(QStringLiteral("video_index")) && video_list.count() > 1) {
                // Clip has more than one video stream, ask which one should be used
                QMap<QString, QString> data;
                if (info.xml.hasAttribute(QStringLiteral("group"))) {
                    data.insert(QStringLiteral("group"), info.xml.attribute(QStringLiteral("group")));
                }
                if (info.xml.hasAttribute(QStringLiteral("groupId"))) {
                    data.insert(QStringLiteral("groupId"), info.xml.attribute(QStringLiteral("groupId")));
                }
                emit multiStreamFound(path, audio_list, video_list, data);
                // Force video index so that when reloading the clip we don't ask again for other streams
                filePropertyMap[QStringLiteral("video_index")] = QString::number(vindex);
            }

            if (vindex > -1) {
                snprintf(property, sizeof(property), "meta.media.%d.stream.frame_rate", vindex);
                double fps = producer->get_double(property);
                if (fps > 0) {
                    filePropertyMap[QStringLiteral("fps")] = locale.toString(fps);
                }
            }

            if (!filePropertyMap.contains(QStringLiteral("fps"))) {
                if (producer->get_double("meta.media.frame_rate_den") > 0) {
                    filePropertyMap[QStringLiteral("fps")] = locale.toString(producer->get_double("meta.media.frame_rate_num") / producer->get_double("meta.media.frame_rate_den"));
                } else {
                    double fps = producer->get_double("source_fps");
                    if (fps > 0) {
                        filePropertyMap[QStringLiteral("fps")] = locale.toString(fps);
                    }
                }
            }
        }
        if (!filePropertyMap.contains(QStringLiteral("fps")) && type == Unknown) {
            // something wrong, maybe audio file with embedded image
            QMimeDatabase db;
            QString mime = db.mimeTypeForFile(path).name();
            if (mime.startsWith(QLatin1String("audio"))) {
                producer->set("video_index", -1);
                vindex = -1;
            }
        }
        Mlt::Frame *frame = producer->get_frame();
        if (frame && frame->is_valid()) {
            if (!mltService.contains(QStringLiteral("avformat"))) {
                // Fetch thumbnail
                QImage img;
                if (KdenliveSettings::gpu_accel()) {
                    delete frame;
                    Clip clp(*producer);
                    Mlt::Producer *glProd = clp.softClone(ClipController::getPassPropertiesList());
                    Mlt::Filter scaler(*m_binController->profile(), "swscale");
                    Mlt::Filter converter(*m_binController->profile(), "avcolor_space");
                    glProd->attach(scaler);
                    glProd->attach(converter);
                    frame = glProd->get_frame();
                    img = KThumb::getFrame(frame, fullWidth, info.imageHeight);
                    delete glProd;
                } else {
                    img = KThumb::getFrame(frame, fullWidth, info.imageHeight, forceThumbScale);
                }
                emit replyGetImage(info.clipId, img);
            } else {
                filePropertyMap[QStringLiteral("frame_size")] = QString::number(frame->get_int("width")) + QLatin1Char('x') + QString::number(frame->get_int("height"));
                int af = frame->get_int("audio_frequency");
                int ac = frame->get_int("audio_channels");
                // keep for compatibility with MLT <= 0.8.6
                if (af == 0) {
                    af = frame->get_int("frequency");
                }
                if (ac == 0) {
                    ac = frame->get_int("channels");
                }
                if (af > 0) {
                    filePropertyMap[QStringLiteral("frequency")] = QString::number(af);
                }
                if (ac > 0) {
                    filePropertyMap[QStringLiteral("channels")] = QString::number(ac);
                }
                if (!filePropertyMap.contains(QStringLiteral("aspect_ratio"))) {
                    filePropertyMap[QStringLiteral("aspect_ratio")] = frame->get("aspect_ratio");
                }

                if (frame->get_int("test_image") == 0 && vindex != -1) {
                    if (mltService == QLatin1String("xml") || mltService == QLatin1String("consumer")) {
                        filePropertyMap[QStringLiteral("type")] = QStringLiteral("playlist");
                        metadataPropertyMap[QStringLiteral("comment")] = QString::fromUtf8(producer->get("title"));
                    } else if (!mlt_frame_is_test_audio(frame->get_frame())) {
                        filePropertyMap[QStringLiteral("type")] = QStringLiteral("av");
                    } else {
                        filePropertyMap[QStringLiteral("type")] = QStringLiteral("video");
                    }
                    // Check if we are using GPU accel, then we need to use alternate producer
                    Mlt::Producer *tmpProd = nullptr;
                    if (KdenliveSettings::gpu_accel()) {
                        delete frame;
                        Clip clp(*producer);
                        tmpProd = clp.softClone(ClipController::getPassPropertiesList());
                        Mlt::Filter scaler(*m_binController->profile(), "swscale");
                        Mlt::Filter converter(*m_binController->profile(), "avcolor_space");
                        tmpProd->attach(scaler);
                        tmpProd->attach(converter);
                        frame = tmpProd->get_frame();
                    } else {
                        tmpProd = producer;
                    }
                    QImage img = KThumb::getFrame(frame, fullWidth, info.imageHeight, forceThumbScale);
                    if (frameNumber == -1) {
                        // No user specipied frame, look for best one
                        int variance = KThumb::imageVariance(img);
                        if (variance < 6) {
                            // Thumbnail is not interesting (for example all black, seek to fetch better thumb
                            delete frame;
                            frameNumber =  duration > 100 ? 100 : duration / 2;
                            tmpProd->seek(frameNumber);
                            frame = tmpProd->get_frame();
                            img = KThumb::getFrame(frame, fullWidth, info.imageHeight, forceThumbScale);
                        }
                    }
                    if (KdenliveSettings::gpu_accel()) {
                        delete tmpProd;
                    }
                    if (frameNumber > -1) {
                        filePropertyMap[QStringLiteral("thumbnailFrame")] = QString::number(frameNumber);
                    }
                    emit replyGetImage(info.clipId, img);
                } else if (frame->get_int("test_audio") == 0) {
                    filePropertyMap[QStringLiteral("type")] = QStringLiteral("audio");
                }
                delete frame;

                if (vindex > -1) {
                    /*if (context->duration == AV_NOPTS_VALUE) {
                    //qCDebug(KDENLIVE_LOG) << " / / / / / / / /ERROR / / / CLIP HAS UNKNOWN DURATION";
                    emit removeInvalidClip(clipId);
                    delete producer;
                    return;
                    }*/
                    // Get the video_index
                    int video_max = 0;
                    int default_audio = producer->get_int("audio_index");
                    int audio_max = 0;

                    int scan = producer->get_int("meta.media.progressive");
                    filePropertyMap[QStringLiteral("progressive")] = QString::number(scan);

                    // Find maximum stream index values
                    for (int ix = 0; ix < producer->get_int("meta.media.nb_streams"); ++ix) {
                        snprintf(property, sizeof(property), "meta.media.%d.stream.type", ix);
                        QString type = producer->get(property);
                        if (type == QLatin1String("video")) {
                            video_max = ix;
                        } else if (type == QLatin1String("audio")) {
                            audio_max = ix;
                        }
                    }
                    filePropertyMap[QStringLiteral("default_video")] = QString::number(vindex);
                    filePropertyMap[QStringLiteral("video_max")] = QString::number(video_max);
                    filePropertyMap[QStringLiteral("default_audio")] = QString::number(default_audio);
                    filePropertyMap[QStringLiteral("audio_max")] = QString::number(audio_max);

                    snprintf(property, sizeof(property), "meta.media.%d.codec.long_name", vindex);
                    if (producer->get(property)) {
                        filePropertyMap[QStringLiteral("videocodec")] = producer->get(property);
                    }
                    snprintf(property, sizeof(property), "meta.media.%d.codec.name", vindex);
                    if (producer->get(property)) {
                        filePropertyMap[QStringLiteral("videocodecid")] = producer->get(property);
                    }
                    QString query;
                    query = QStringLiteral("meta.media.%1.codec.pix_fmt").arg(vindex);
                    filePropertyMap[QStringLiteral("pix_fmt")] = producer->get(query.toUtf8().constData());
                    filePropertyMap[QStringLiteral("colorspace")] = producer->get("meta.media.colorspace");

                } else {
                    qCDebug(KDENLIVE_LOG) << " / / / / /WARNING, VIDEO CONTEXT IS nullptr!!!!!!!!!!!!!!";
                }
                if (producer->get_int("audio_index") > -1) {
                    // Get the audio_index
                    int index = producer->get_int("audio_index");
                    snprintf(property, sizeof(property), "meta.media.%d.codec.long_name", index);
                    if (producer->get(property)) {
                        filePropertyMap[QStringLiteral("audiocodec")] = producer->get(property);
                    } else {
                        snprintf(property, sizeof(property), "meta.media.%d.codec.name", index);
                        if (producer->get(property)) {
                            filePropertyMap[QStringLiteral("audiocodec")] = producer->get(property);
                        }
                    }
                }
                producer->set("mlt_service", "avformat-novalidate");
            }
        }
        // metadata
        Mlt::Properties metadata;
        metadata.pass_values(*producer, "meta.attr.");
        int count = metadata.count();
        for (int i = 0; i < count; i ++) {
            QString name = metadata.get_name(i);
            QString value = QString::fromUtf8(metadata.get(i));
            if (name.endsWith(QLatin1String(".markup")) && !value.isEmpty()) {
                metadataPropertyMap[ name.section(QLatin1Char('.'), 0, -2)] = value;
            }
        }
        producer->seek(0);
        if (m_binController->hasClip(info.clipId)) {
            // If controller already exists, we just want to update the producer
            m_binController->replaceProducer(info.clipId, *producer);
            emit gotFileProperties(info, nullptr);
        } else {
            // Create the controller
            ClipController *controller = new ClipController(m_binController, *producer);
            m_binController->addClipToBin(info.clipId, controller);
            emit gotFileProperties(info, controller);
        }
        m_processingClipId.removeAll(info.clipId);
    }
}

void ProducerQueue::abortOperations()
{
    m_infoMutex.lock();
    m_requestList.clear();
    m_infoMutex.unlock();
    m_infoThread.waitForFinished();
}

ClipType ProducerQueue::getTypeForService(const QString &id, const QString &path) const
{
    if (id.isEmpty()) {
        QString ext = path.section(QLatin1Char('.'), -1);
        if (ext == QLatin1String("mlt") || ext == QLatin1String("kdenlive")) {
            return Playlist;
        }
        return Unknown;
    }
    if (id == QLatin1String("color") || id == QLatin1String("colour")) {
        return Color;
    }
    if (id == QLatin1String("kdenlivetitle")) {
        return Text;
    }
    if (id == QLatin1String("qtext")) {
        return QText;
    }
    if (id == QLatin1String("xml") || id == QLatin1String("consumer")) {
        return Playlist;
    }
    if (id == QLatin1String("webvfx")) {
        return WebVfx;
    }
    return Unknown;
}

void ProducerQueue::processProducerProperties(Mlt::Producer *prod, const QDomElement &xml)
{
    //TODO: there is some duplication with clipcontroller > updateproducer that also copies properties
    QString value;
    QStringList internalProperties;
    internalProperties << QStringLiteral("bypassDuplicate") << QStringLiteral("resource") << QStringLiteral("mlt_service") << QStringLiteral("audio_index") << QStringLiteral("video_index") << QStringLiteral("mlt_type");
    QDomNodeList props;

    if (xml.tagName() == QLatin1String("producer")) {
        props = xml.childNodes();
    } else {
        props = xml.firstChildElement(QStringLiteral("producer")).childNodes();
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

void ProducerQueue::slotMultiStreamProducerFound(const QString &path, const QList<int> &audio_list, const QList<int> &video_list, stringMap data)
{
    if (KdenliveSettings::automultistreams()) {
        for (int i = 1; i < video_list.count(); ++i) {
            int vindex = video_list.at(i);
            int aindex = 0;
            if (i <= audio_list.count() - 1) {
                aindex = audio_list.at(i);
            }
            data.insert(QStringLiteral("video_index"), QString::number(vindex));
            data.insert(QStringLiteral("audio_index"), QString::number(aindex));
            data.insert(QStringLiteral("bypassDuplicate"), QStringLiteral("1"));
            emit addClip(path, data);
        }
        return;
    }

    int width = 60.0 * m_binController->profile()->dar();
    if (width % 2 == 1) {
        width++;
    }

    QPointer<QDialog> dialog = new QDialog(qApp->activeWindow());
    dialog->setWindowTitle(QStringLiteral("Multi Stream Clip"));
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QWidget *mainWidget = new QWidget(dialog);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    dialog->setLayout(mainLayout);
    mainLayout->addWidget(mainWidget);
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    okButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    dialog->connect(buttonBox, &QDialogButtonBox::accepted, dialog.data(), &QDialog::accept);
    dialog->connect(buttonBox, &QDialogButtonBox::rejected, dialog.data(), &QDialog::reject);
    okButton->setText(i18n("Import selected clips"));

    QLabel *lab1 = new QLabel(i18n("Additional streams for clip\n %1", path), mainWidget);
    mainLayout->addWidget(lab1);
    QList<QGroupBox *> groupList;
    QList<KComboBox *> comboList;
    // We start loading the list at 1, video index 0 should already be loaded
    for (int j = 1; j < video_list.count(); ++j) {
        Mlt::Producer multiprod(* m_binController->profile(), path.toUtf8().constData());
        multiprod.set("video_index", video_list.at(j));
        QImage thumb = KThumb::getFrame(&multiprod, 0, width, 60);
        QGroupBox *streamFrame = new QGroupBox(i18n("Video stream %1", video_list.at(j)), mainWidget);
        mainLayout->addWidget(streamFrame);
        streamFrame->setProperty("vindex", video_list.at(j));
        groupList << streamFrame;
        streamFrame->setCheckable(true);
        streamFrame->setChecked(true);
        QVBoxLayout *vh = new QVBoxLayout(streamFrame);
        QLabel *iconLabel = new QLabel(mainWidget);
        mainLayout->addWidget(iconLabel);
        iconLabel->setPixmap(QPixmap::fromImage(thumb));
        vh->addWidget(iconLabel);
        if (audio_list.count() > 1) {
            KComboBox *cb = new KComboBox(mainWidget);
            mainLayout->addWidget(cb);
            for (int k = 0; k < audio_list.count(); ++k) {
                cb->addItem(i18n("Audio stream %1", audio_list.at(k)), audio_list.at(k));
            }
            comboList << cb;
            cb->setCurrentIndex(qMin(j, audio_list.count() - 1));
            vh->addWidget(cb);
        }
        mainLayout->addWidget(streamFrame);
    }
    mainLayout->addWidget(buttonBox);
    if (dialog->exec() == QDialog::Accepted) {
        // import selected streams
        for (int i = 0; i < groupList.count(); ++i) {
            if (groupList.at(i)->isChecked()) {
                int vindex = groupList.at(i)->property("vindex").toInt();
                int aindex = comboList.at(i)->itemData(comboList.at(i)->currentIndex()).toInt();
                data.insert(QStringLiteral("kdenlive-force.video_index"), QString::number(vindex));
                data.insert(QStringLiteral("kdenlive-force.audio_index"), QString::number(aindex));
                data.insert(QStringLiteral("bypassDuplicate"), QStringLiteral("1"));
                emit addClip(path, data);
            }
        }
    }
    delete dialog;
}
