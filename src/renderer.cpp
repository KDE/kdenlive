/***************************************************************************
                        krender.cpp  -  description
                           -------------------
  begin                : Fri Nov 22 2002
  copyright            : (C) 2002 by Jason Wood
  email                : jasonwood@blueyonder.co.uk
  copyright            : (C) 2005 Lucio Flavio Correa
  email                : lucio.correa@gmail.com
  copyright            : (C) Marco Gittler
  email                : g.marco@freenet.de
  copyright            : (C) 2006 Jean-Baptiste Mardelle
  email                : jb@kdenlive.org

***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "renderer.h"
#include "kdenlivesettings.h"
#include "doc/kthumb.h"
#include "definitions.h"
#include "project/dialogs/slideshowclip.h"
#include "dialogs/profilesdialog.h"
#include "mltcontroller/bincontroller.h"
#include "bin/projectclip.h"
#include "timeline/clip.h"
#include "monitor/glwidget.h"
#include "mltcontroller/clipcontroller.h"
#include <mlt++/Mlt.h>

#include <QDebug>
#include <KMessageBox>
#include <KLocalizedString>
#include <QDialog>
#include <QPainter>
#include <QString>
#include <QApplication>
#include <QProcess>
#include <QtConcurrent>

#include <cstdlib>
#include <cstdarg>
#include <KConfigGroup>
#include <KRecentDirs>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

#define SEEK_INACTIVE (-1)

Render::Render(Kdenlive::MonitorId rendererName, BinController *binController, GLWidget *qmlView, QWidget *parent) :
    AbstractRender(rendererName, parent),
    requestedSeekPosition(SEEK_INACTIVE),
    showFrameSemaphore(1),
    externalConsumer(false),
    m_name(rendererName),
    m_mltConsumer(NULL),
    m_mltProducer(NULL),
    m_showFrameEvent(NULL),
    m_pauseEvent(NULL),
    m_binController(binController),
    m_qmlView(qmlView),
    m_isZoneMode(false),
    m_isLoopMode(false),
    m_isSplitView(false),
    m_blackClip(NULL),
    m_isActive(false),
    m_isRefreshing(false)
{
    qRegisterMetaType<stringMap> ("stringMap");
    analyseAudio = KdenliveSettings::monitor_audio();
    //buildConsumer();
    if (m_qmlView) {
        m_blackClip = new Mlt::Producer(*m_qmlView->profile(), "colour:black");
        m_blackClip->set("id", "black");
        m_blackClip->set("mlt_type", "producer");
        m_mltProducer = m_blackClip->cut(0, 1);
        m_qmlView->setProducer(m_mltProducer);
        m_mltConsumer = qmlView->consumer();
    }
    /*m_mltConsumer->connect(*m_mltProducer);
    m_mltProducer->set_speed(0.0);*/
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(50);
    connect(&m_refreshTimer, SIGNAL(timeout()), this, SLOT(refresh()));
    connect(this, SIGNAL(multiStreamFound(QString,QList<int>,QList<int>,stringMap)), this, SLOT(slotMultiStreamProducerFound(QString,QList<int>,QList<int>,stringMap)));
    connect(this, SIGNAL(checkSeeking()), this, SLOT(slotCheckSeeking()));
    if (m_name == Kdenlive::ProjectMonitor) {
        connect(m_binController, SIGNAL(prepareTimelineReplacement(QString)), this, SIGNAL(prepareTimelineReplacement(QString)), Qt::DirectConnection);
        connect(m_binController, SIGNAL(replaceTimelineProducer(QString)), this, SIGNAL(replaceTimelineProducer(QString)), Qt::DirectConnection);
	connect(m_binController, SIGNAL(updateTimelineProducer(QString)), this, SIGNAL(updateTimelineProducer(QString)));
        connect(m_binController, SIGNAL(createThumb(QDomElement,QString,int)), this, SLOT(getFileProperties(QDomElement,QString,int)));
        connect(m_binController, SIGNAL(setDocumentNotes(QString)), this, SIGNAL(setDocumentNotes(QString)));
    }
}

Render::~Render()
{
    closeMlt();
}

void Render::abortOperations()
{
    m_infoMutex.lock();
    m_requestList.clear();
    m_infoMutex.unlock();
    m_infoThread.waitForFinished();
}


void Render::closeMlt()
{
    m_infoMutex.lock();
    m_requestList.clear();
    m_infoMutex.unlock();
    m_infoThread.waitForFinished();
    delete m_showFrameEvent;
    delete m_pauseEvent;
    delete m_mltConsumer;
    delete m_mltProducer;
    delete m_blackClip;
}

void Render::slotSwitchFullscreen()
{
    if (m_mltConsumer)
        m_mltConsumer->set("full_screen", 1);
}

Mlt::Producer *Render::invalidProducer(const QString &id)
{
    Mlt::Producer *clip;
    QString txt = '+' + i18n("Missing clip") + ".txt";
    char *tmp = qstrdup(txt.toUtf8().constData());
    clip = new Mlt::Producer(*m_qmlView->profile(), tmp);
    delete[] tmp;
    if (clip == NULL) {
        clip = new Mlt::Producer(*m_qmlView->profile(), "colour", "red");
    } else {
        clip->set("bgcolour", "0xff0000ff");
        clip->set("pad", "10");
    }
    clip->set("id", id.toUtf8().constData());
    clip->set("mlt_type", "producer");
    return clip;
}

void Render::prepareProfileReset()
{
    m_refreshTimer.stop();
    if (m_isSplitView)
            slotSplitView(false);
    m_infoMutex.lock();
    m_requestList.clear();
    m_infoMutex.unlock();
    m_infoThread.waitForFinished();
}


void Render::seek(const GenTime &time)
{
    if (!m_mltProducer || !m_isActive)
        return;
    int pos = time.frames(m_fps);
    seek(pos);
}

void Render::seek(int time)
{
    resetZoneMode();
    if (requestedSeekPosition == SEEK_INACTIVE) {
        requestedSeekPosition = time;
        m_mltConsumer->purge();
        m_mltProducer->seek(time);
        if (!externalConsumer) {
            m_isRefreshing = true;
            m_mltConsumer->set("refresh", 1);
        }
    }
    else {
        requestedSeekPosition = time;
    }
}

int Render::frameRenderWidth() const
{
    return m_qmlView->profile()->width();
}

int Render::renderWidth() const
{
    return (int)(m_qmlView->profile()->height() * m_qmlView->profile()->dar() + 0.5);
}

int Render::renderHeight() const
{
    return m_qmlView->profile()->height();
}

QImage Render::extractFrame(int frame_position, const QString &path, int width, int height)
{
    if (width == -1) {
        width = frameRenderWidth();
        height = renderHeight();
    } else if (width % 2 == 1) width++;
    if (!path.isEmpty()) {
        Mlt::Producer *producer = new Mlt::Producer(*m_qmlView->profile(), path.toUtf8().constData());
        if (producer) {
            if (producer->is_valid()) {
                QImage img = KThumb::getFrame(producer, frame_position, width, height);
                delete producer;
                return img;
            }
            else delete producer;
        }
    }

    if (!m_mltProducer || !path.isEmpty()) {
        QImage pix(width, height, QImage::Format_RGB32);
        pix.fill(Qt::black);
        return pix;
    }
    Mlt::Frame *frame = NULL;
    if (KdenliveSettings::gpu_accel()) {
        QString service = m_mltProducer->get("mlt_service");
        //TODO: create duplicate prod from xml data
        Mlt::Producer *tmpProd = new Mlt::Producer(*m_qmlView->profile(), service.toUtf8().constData(), path.toUtf8().constData());
        Mlt::Filter scaler(*m_qmlView->profile(), "swscale");
        Mlt::Filter converter(*m_qmlView->profile(), "avcolor_space");
        tmpProd->attach(scaler);
        tmpProd->attach(converter);
        tmpProd->seek(m_mltProducer->position());
        frame = tmpProd->get_frame();
        delete tmpProd;
    }
    else {
        frame = m_mltProducer->get_frame();
    }
    QImage img = KThumb::getFrame(frame, width, height);
    delete frame;
    return img;
}

int Render::getLength()
{

    if (m_mltProducer) {
        // //qDebug()<<"//////  LENGTH: "<<mlt_producer_get_playtime(m_mltProducer->get_producer());
        return mlt_producer_get_playtime(m_mltProducer->get_producer());
    }
    return 0;
}

bool Render::isValid(const QUrl &url)
{
    Mlt::Producer producer(*m_qmlView->profile(), url.toLocalFile().toUtf8().constData());
    if (producer.is_blank())
        return false;

    return true;
}

double Render::dar() const
{
    return m_qmlView->profile()->dar();
}

double Render::sar() const
{
    return m_qmlView->profile()->sar();
}

void Render::slotSplitView(bool doit)
{
    m_isSplitView = doit;
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    if (service.type() != tractor_type || tractor.count() < 2) return;
    Mlt::Field *field = tractor.field();
    if (doit) {
        for (int i = 1, screen = 0; i < tractor.count() && screen < 4; ++i) {
            Mlt::Producer trackProducer(tractor.track(i));
            //qDebug() << "// TRACK: " << i << ", HIDE: " << trackProducer.get("hide");
            if (QString(trackProducer.get("hide")).toInt() != 1) {
                //qDebug() << "// ADIDNG TRACK: " << i;
                Mlt::Transition *transition = new Mlt::Transition(*m_qmlView->profile(), "composite");
                transition->set("mlt_service", "composite");
                transition->set("a_track", 0);
                transition->set("b_track", i);
                transition->set("distort", 0);
                transition->set("aligned", 0);
                transition->set("internal_added", "200");
                QString geometry;
                switch (screen) {
                case 0:
                    geometry = "0/0:50%x50%";
                    break;
                case 1:
                    geometry = "50%/0:50%x50%";
                    break;
                case 2:
                    geometry = "0/50%:50%x50%";
                    break;
                case 3:
                default:
                    geometry = "50%/50%:50%x50%";
                    break;
                }
                transition->set("geometry", geometry.toUtf8().constData());
                transition->set("always_active", "1");
                field->plant_transition(*transition, 0, i);
                screen++;
            }
        }
        m_isRefreshing = true;
        m_mltConsumer->set("refresh", 1);
    } else {
        mlt_service serv = m_mltProducer->parent().get_service();
        mlt_service nextservice = mlt_service_get_producer(serv);
        mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
        QString mlt_type = mlt_properties_get(properties, "mlt_type");
        QString resource = mlt_properties_get(properties, "mlt_service");
        mlt_service nextservicetodisconnect;

        while (mlt_type == "transition") {
            QString added = mlt_properties_get(MLT_SERVICE_PROPERTIES(nextservice), "internal_added");
            if (added == "200") {
                nextservicetodisconnect = nextservice;
                nextservice = mlt_service_producer(nextservice);
                mlt_field_disconnect_service(field->get_field(), nextservicetodisconnect);
            }
            else nextservice = mlt_service_producer(nextservice);
            if (nextservice == NULL) break;
            properties = MLT_SERVICE_PROPERTIES(nextservice);
            mlt_type = mlt_properties_get(properties, "mlt_type");
            resource = mlt_properties_get(properties, "mlt_service");
            m_isRefreshing = true;
            m_mltConsumer->set("refresh", 1);
        }
    }
}

void Render::getFileProperties(const QDomElement &xml, const QString &clipId, int imageHeight, bool replaceProducer)
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
        m_infoThread = QtConcurrent::run(this, &Render::processFileProperties);
    }
}

void Render::forceProcessing(const QString &id)
{
    // Make sure we load the clip producer now so that we can use it in timeline
    QList <requestClipInfo> requestListCopy;
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
	    m_infoThread = QtConcurrent::run(this, &Render::processFileProperties);
	}
	m_infoThread.waitForFinished();
	emit infoProcessingFinished();
    }
    
    m_infoMutex.lock();
    m_requestList.append(requestListCopy);
    m_infoMutex.unlock();
    if (!m_infoThread.isRunning()) {
        m_infoThread = QtConcurrent::run(this, &Render::processFileProperties);
    }
}

int Render::processingItems()
{
    QMutexLocker lock(&m_infoMutex);
    const int count = m_requestList.count() + m_processingClipId.count();
    return count;
}

void Render::slotProcessingDone(const QString &id)
{
    QMutexLocker lock(&m_infoMutex);
    m_processingClipId.removeAll(id);
}

bool Render::isProcessing(const QString &id)
{
    if (m_processingClipId.contains(id)) return true;
    QMutexLocker lock(&m_infoMutex);
    for (int i = 0; i < m_requestList.count(); ++i) {
        if (m_requestList.at(i).clipId == id) {
            return true;
        }
    }
    return false;
}

ClipType Render::getTypeForService(const QString &id, const QString &path) const
{
    if (id.isEmpty()) {
        QString ext = path.section(".", -1);
        if (ext == "mlt" || ext == "kdenlive") return Playlist;
        return Unknown;
    }
    if (id == "color" || id == "colour") return Color;
    if (id == "kdenlivetitle") return Text;
    if (id == "xml" || id == "consumer") return Playlist;
    if (id == "webvfx") return WebVfx;
    return Unknown;
}

void Render::processProducerProperties(Mlt::Producer *prod, QDomElement xml)
{
    //TODO: there is some duplication with clipcontroller > updateproducer that also copies properties 
    QString value;
    QStringList internalProperties;
    internalProperties << "bypassDuplicate" << "resource" << "mlt_service" << "audio_index" << "video_index" << "mlt_type";
    QDomNodeList props;
    if (xml.tagName() == "producer") {
	props = xml.childNodes();
    }
    else {
	props = xml.firstChildElement("producer").childNodes();
    }
    for (int i = 0; i < props.count(); ++i) {
        if (props.at(i).toElement().tagName() != "property") continue;
        QString propertyName = props.at(i).toElement().attribute("name");
        if (!internalProperties.contains(propertyName) && !propertyName.startsWith("_")) {
            value = props.at(i).firstChild().nodeValue();
            prod->set(propertyName.toUtf8().constData(), value.toUtf8().constData());
        }
    }
}

void Render::processFileProperties()
{
    requestClipInfo info;
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);

    while (!m_requestList.isEmpty()) {
        m_infoMutex.lock();
        info = m_requestList.takeFirst();
        if (info.xml.hasAttribute("thumbnailOnly")) {
            m_infoMutex.unlock();
            // Special case, we just want the thumbnail for existing producer
            Mlt::Producer *prod = new Mlt::Producer(*m_binController->getBinProducer(info.clipId));
	    if (!prod) {
		continue;
	    }
            // Check if we are using GPU accel, then we need to use alternate producer
            if (KdenliveSettings::gpu_accel()) {
                QString service = prod->get("mlt_service");
                QString res = prod->get("resource");
                delete prod;
                prod = new Mlt::Producer(*m_qmlView->profile(), service.toUtf8().constData(), res.toUtf8().constData());
                Mlt::Filter scaler(*m_qmlView->profile(), "swscale");
                Mlt::Filter converter(*m_qmlView->profile(), "avcolor_space");
                prod->attach(scaler);
                prod->attach(converter);
            }
            int frameNumber = ProjectClip::getXmlProperty(info.xml, "kdenlive:thumbnailFrame", "-1").toInt();
            if (frameNumber > 0) prod->seek(frameNumber);
            Mlt::Frame *frame = prod->get_frame();
            if (frame && frame->is_valid()) {
                int fullWidth = (int)((double) info.imageHeight * m_qmlView->profile()->dar() + 0.5);
                QImage img = KThumb::getFrame(frame, fullWidth, info.imageHeight);
                emit replyGetImage(info.clipId, img);
            }
            delete frame;
            delete prod;
            continue;
        }
        m_processingClipId.append(info.clipId);
        m_infoMutex.unlock();
        //TODO: read all xml meta.kdenlive properties into a QMap or an MLT::Properties and pass them to the newly created producer

        QString path;
        bool proxyProducer;
        QString proxy = ProjectClip::getXmlProperty(info.xml, "kdenlive:proxy");
        if (!proxy.isEmpty()) {
            if (proxy == "-") {
                path = ProjectClip::getXmlProperty(info.xml, "kdenlive:originalurl");
                proxyProducer = false;
            }
            else {
                path = proxy;
                // Check for missing proxies
                if (QFileInfo(path).size() <= 0) {
                    // proxy is missing, re-create it
                    emit requestProxy(info.clipId);
                    proxyProducer = false;
                    //path = info.xml.attribute("resource");
                    path = ProjectClip::getXmlProperty(info.xml, "resource");
                }
                else proxyProducer = true;
            }
        }
        else {
	    path = ProjectClip::getXmlProperty(info.xml, "resource");
            //path = info.xml.attribute("resource");
            proxyProducer = false;
        }
        //qDebug()<<" / / /CHECKING PRODUCER PATH: "<<path;
        QUrl url = QUrl::fromLocalFile(path);
        Mlt::Producer *producer = NULL;
        ClipType type = (ClipType)info.xml.attribute("type").toInt();
        if (type == Unknown) {
            type = getTypeForService(ProjectClip::getXmlProperty(info.xml, "mlt_service"), path);
        }
        if (type == Color) {
            path.prepend("color:");
            producer = new Mlt::Producer(*m_qmlView->profile(), 0, path.toUtf8().constData());
        } else if (type == Text) {
            path.prepend("kdenlivetitle:");
            producer = new Mlt::Producer(*m_qmlView->profile(), 0, path.toUtf8().constData());
        } else if (type == Playlist) {
	    //TODO: "xml" seems to corrupt project fps if different, and "consumer" crashed on audio transition
	    Mlt::Profile *xmlProfile = new Mlt::Profile();
	    xmlProfile->set_explicit(false);
	    MltVideoProfile projectProfile = ProfilesDialog::getVideoProfile(*m_qmlView->profile());
            //path.prepend("consumer:");
            producer = new Mlt::Producer(*xmlProfile, "xml", path.toUtf8().constData());
	    xmlProfile->from_producer(*producer);
	    MltVideoProfile clipProfile = ProfilesDialog::getVideoProfile(*xmlProfile);
	    delete producer;
	    delete xmlProfile;
	    if (clipProfile == projectProfile) {
		// We can use the "xml" producer since profile is the same (using it with different profiles corrupts the project.
		// Beware that "consumer" currently crashes on audio mixes!
		path.prepend("xml:");
	    }
	    else {
		path.prepend("consumer:");
	    }
	    producer = new Mlt::Producer(*m_qmlView->profile(), 0, path.toUtf8().constData());
        } else if (type == SlideShow) {
            producer = new Mlt::Producer(*m_qmlView->profile(), 0, path.toUtf8().constData());
        } else if (!url.isValid()) {
            //WARNING: when is this case used? Not sure it is working.. JBM/
            QDomDocument doc;
            QDomElement mlt = doc.createElement("mlt");
            QDomElement play = doc.createElement("playlist");
            play.setAttribute("id", "playlist0");
            doc.appendChild(mlt);
            mlt.appendChild(play);
            play.appendChild(doc.importNode(info.xml, true));
            QDomElement tractor = doc.createElement("tractor");
            tractor.setAttribute("id", "tractor0");
            QDomElement track = doc.createElement("track");
            track.setAttribute("producer", "playlist0");
            tractor.appendChild(track);
            mlt.appendChild(tractor);
            producer = new Mlt::Producer(*m_qmlView->profile(), "xml-string", doc.toString().toUtf8().constData());
        } else {
            producer = new Mlt::Producer(*m_qmlView->profile(), 0, path.toUtf8().constData());
        }
        if (producer == NULL || producer->is_blank() || !producer->is_valid()) {
            qDebug() << " / / / / / / / / ERROR / / / / // CANNOT LOAD PRODUCER: "<<path;
            m_processingClipId.removeAll(info.clipId);
            if (proxyProducer) {
                // Proxy file is corrupted
                emit removeInvalidProxy(info.clipId, false);
            }
            else emit removeInvalidClip(info.clipId, info.replaceProducer);
            delete producer;
            continue;
        }
        // Pass useful properties
        processProducerProperties(producer, info.xml);
        QString clipName = ProjectClip::getXmlProperty(info.xml, "kdenlive:clipname");
        if (!clipName.isEmpty()) {
            producer->set("kdenlive:clipname", clipName.toUtf8().constData());
        }
        QString groupId = ProjectClip::getXmlProperty(info.xml, "kdenlive:folderid");
        if (!groupId.isEmpty()) {
            producer->set("kdenlive:folderid", groupId.toUtf8().constData());
        }

        if (proxyProducer && info.xml.hasAttribute("proxy_out")) {
            producer->set("length", info.xml.attribute("proxy_out").toInt() + 1);
            producer->set("out", info.xml.attribute("proxy_out").toInt());
            if (producer->get_out() != info.xml.attribute("proxy_out").toInt()) {
                // Proxy file length is different than original clip length, this will corrupt project so disable this proxy clip
                qDebug()<<"/ // PROXY LENGTH MISMATCH, DELETE PRODUCER";
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
        if (info.xml.hasAttribute("out")) clipOut = info.xml.attribute("out").toInt();

        // setup length here as otherwise default length (currently 15000 frames in MLT) will be taken even if outpoint is larger
        if (type == Color || type == Text || type == Image || type == SlideShow) {
            int length;
            if (info.xml.hasAttribute("length")) {
                length = info.xml.attribute("length").toInt();
                clipOut = length - 1;
            }
            else length = info.xml.attribute("out").toInt() - info.xml.attribute("in").toInt() + 1;
            // Pass duration if it was forced
            if (info.xml.hasAttribute("duration")) {
                duration = info.xml.attribute("duration").toInt();
                if (length < duration) {
                    length = duration;
                    if (clipOut > 0) clipOut = length - 1;
                }
            }
            if (duration == 0) duration = length;
            producer->set("length", length);
        }

        if (clipOut > 0) producer->set_in_and_out(info.xml.attribute("in").toInt(), clipOut);

        if (info.xml.hasAttribute("templatetext"))
            producer->set("templatetext", info.xml.attribute("templatetext").toUtf8().constData());

        int fullWidth = (int)((double) info.imageHeight * m_qmlView->profile()->dar() + 0.5);
        int frameNumber = ProjectClip::getXmlProperty(info.xml, "kdenlive:thumbnailFrame", "-1").toInt();

        if ((!info.replaceProducer && !EffectsList::property(info.xml, "kdenlive:file_hash").isEmpty()) || proxyProducer) {
            // Clip  already has all properties
            // We want to replace an existing producer. We MUST NOT set the producer's id property until 
            // the old one has been removed.
            if (proxyProducer) {
                // Recreate clip thumb
                if (frameNumber > 0) producer->seek(frameNumber);
                Mlt::Frame *frame = producer->get_frame();
                if (frame && frame->is_valid()) {
                    QImage img = KThumb::getFrame(frame, fullWidth, info.imageHeight);
                    emit replyGetImage(info.clipId, img);
                }
                if (frame) delete frame;
            }
            // replace clip
            m_processingClipId.removeAll(info.clipId);
            m_binController->replaceProducer(info.clipId, *producer);
            emit gotFileProperties(info, NULL);
            continue;
        }
        // We are not replacing an existing producer, so set the id
        producer->set("id", info.clipId.toUtf8().constData());
        stringMap filePropertyMap;
        stringMap metadataPropertyMap;
        char property[200];

        if (frameNumber > 0) producer->seek(frameNumber);
        duration = duration > 0 ? duration : producer->get_playtime();
        ////qDebug() << "///////  PRODUCER: " << url.path() << " IS: " << producer->get_playtime();

        if (type == SlideShow) {
	    int ttl = EffectsList::property(info.xml,"ttl").toInt();
	    QString anim = EffectsList::property(info.xml,"animation");
            if (!anim.isEmpty()) {
                Mlt::Filter *filter = new Mlt::Filter(*m_qmlView->profile(), "affine");
                if (filter && filter->is_valid()) {
                    int cycle = ttl;
                    QString geometry = SlideshowClip::animationToGeometry(anim, cycle);
                    if (!geometry.isEmpty()) {
                        if (anim.contains("low-pass")) {
                            Mlt::Filter *blur = new Mlt::Filter(*m_qmlView->profile(), "boxblur");
                            if (blur && blur->is_valid())
                                producer->attach(*blur);
                        }
                        filter->set("transition.geometry", geometry.toUtf8().data());
                        filter->set("transition.cycle", cycle);
                        producer->attach(*filter);
                    }
                }
            }
            QString fade = EffectsList::property(info.xml,"fade");
	    if (fade == "1") {
                // user wants a fade effect to slideshow
                Mlt::Filter *filter = new Mlt::Filter(*m_qmlView->profile(), "luma");
                if (filter && filter->is_valid()) {
                    if (ttl) filter->set("cycle", ttl);
		    QString luma_duration = EffectsList::property(info.xml,"luma_duration");
		    QString luma_file = EffectsList::property(info.xml,"luma_file");
		    if (!luma_duration.isEmpty()) filter->set("duration", luma_duration.toInt());
                    if (!luma_file.isEmpty()) {
                        filter->set("luma.resource", luma_file.toUtf8().constData());
			QString softness = EffectsList::property(info.xml,"softness");
                        if (!softness.isEmpty()) {
                            int soft = softness.toInt();
                            filter->set("luma.softness", (double) soft / 100.0);
                        }
                    }
                    producer->attach(*filter);
                }
            }
            QString crop = EffectsList::property(info.xml,"crop");
            if (crop == "1") {
                // user wants to center crop the slides
                Mlt::Filter *filter = new Mlt::Filter(*m_qmlView->profile(), "crop");
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
                path = "xml:" + path.section(":", 1);
            }
            Mlt::Profile *original_profile = new Mlt::Profile();
            Mlt::Producer *tmpProd = new Mlt::Producer(*original_profile, 0, path.toUtf8().constData());
            original_profile->from_producer(*tmpProd);
            original_profile->set_explicit(true);
            filePropertyMap["progressive"] = QString::number(original_profile->progressive());
            filePropertyMap["colorspace"] = QString::number(original_profile->colorspace());
            filePropertyMap["fps"] = QString::number(original_profile->fps());
            filePropertyMap["aspect_ratio"] = QString::number(original_profile->sar());
            double originalFps = original_profile->fps();
            if (originalFps > 0 && originalFps != m_qmlView->profile()->fps()) {
                // Warning, MLT detects an incorrect length in producer consumer when producer's fps != project's fps
                //TODO: report bug to MLT
                delete tmpProd;
                tmpProd = new Mlt::Producer(*original_profile, 0, path.toUtf8().constData());
                int originalLength = tmpProd->get_length();
                int fixedLength = (int) (originalLength * m_qmlView->profile()->fps() / originalFps);
                producer->set("length", fixedLength);
                producer->set("out", fixedLength - 1);
            }
            delete tmpProd;
            delete original_profile;
        }
        else if (mltService == "avformat") {
            // Get frame rate
            vindex = producer->get_int("video_index");
            // List streams
            int streams = producer->get_int("meta.media.nb_streams");
            QList <int> audio_list;
            QList <int> video_list;
            for (int i = 0; i < streams; ++i) {
                QByteArray propertyName = QString("meta.media.%1.stream.type").arg(i).toLocal8Bit();
                QString type = producer->get(propertyName.data());
                if (type == "audio") audio_list.append(i);
                else if (type == "video") video_list.append(i);
            }

            if (!info.xml.hasAttribute("video_index") && video_list.count() > 1) {
                // Clip has more than one video stream, ask which one should be used
                QMap <QString, QString> data;
                if (info.xml.hasAttribute("group")) data.insert("group", info.xml.attribute("group"));
                if (info.xml.hasAttribute("groupId")) data.insert("groupId", info.xml.attribute("groupId"));
                emit multiStreamFound(path, audio_list, video_list, data);
                // Force video index so that when reloading the clip we don't ask again for other streams
                filePropertyMap["video_index"] = QString::number(vindex);
            }

            if (vindex > -1) {
                snprintf(property, sizeof(property), "meta.media.%d.stream.frame_rate", vindex);
		    double fps = producer->get_double(property);
		    if (fps > 0) {
			filePropertyMap["fps"] = locale.toString(fps);
		    }
            }

            if (!filePropertyMap.contains("fps")) {
                if (producer->get_double("meta.media.frame_rate_den") > 0) {
                    filePropertyMap["fps"] = locale.toString(producer->get_double("meta.media.frame_rate_num") / producer->get_double("meta.media.frame_rate_den"));
                } else {
		    double fps = producer->get_double("source_fps");
		    if (fps > 0) filePropertyMap["fps"] = locale.toString(fps);
		}
            }
        }
        if (!filePropertyMap.contains("fps") && type == Unknown) {
	      // something wrong, maybe audio file with embedded image
	      QMimeDatabase db;
	      QString mime = db.mimeTypeForFile(path).name();
	      if (mime.startsWith("audio")) {
		  producer->set("video_index", -1);
		  vindex = -1;
	      }
	}
	Mlt::Frame *frame = producer->get_frame();
        if (frame && frame->is_valid()) {
	    if (!mltService.contains("avformat")) {
		// Fetch thumbnail
                QImage img;
                if (KdenliveSettings::gpu_accel()) {
                    delete frame;
                    Clip clp(*producer);
                    Mlt::Producer *glProd = clp.softClone(ClipController::getPassPropertiesList());
                    Mlt::Filter scaler(*m_qmlView->profile(), "swscale");
                    Mlt::Filter converter(*m_qmlView->profile(), "avcolor_space");
                    glProd->attach(scaler);
                    glProd->attach(converter);
                    frame = glProd->get_frame();
                    img = KThumb::getFrame(frame, fullWidth, info.imageHeight);
                    delete glProd;
                } else {
                    img = KThumb::getFrame(frame, fullWidth, info.imageHeight);
                }
                emit replyGetImage(info.clipId, img);
	    }
	    else {
		filePropertyMap["frame_size"] = QString::number(frame->get_int("width")) + 'x' + QString::number(frame->get_int("height"));
		int af = frame->get_int("audio_frequency");
		int ac = frame->get_int("audio_channels");
		// keep for compatibility with MLT <= 0.8.6
		if (af == 0) af = frame->get_int("frequency");
		if (ac == 0) ac = frame->get_int("channels");
		if (af > 0) filePropertyMap["frequency"] = QString::number(af);
		if (ac > 0) filePropertyMap["channels"] = QString::number(ac);
		if (!filePropertyMap.contains("aspect_ratio")) filePropertyMap["aspect_ratio"] = frame->get("aspect_ratio");

		if (frame->get_int("test_image") == 0 && vindex != -1) {
		    if (mltService == "xml" || mltService == "consumer") {
			filePropertyMap["type"] = "playlist";
			metadataPropertyMap["comment"] = QString::fromUtf8(producer->get("title"));
		    } else if (!mlt_frame_is_test_audio(frame->get_frame()))
			filePropertyMap["type"] = "av";
		    else
			filePropertyMap["type"] = "video";
		    // Check if we are using GPU accel, then we need to use alternate producer
		    Mlt::Producer *tmpProd = NULL;
		    if (KdenliveSettings::gpu_accel()) {
                        delete frame;
                        Clip clp(*producer);
                        tmpProd = clp.softClone(ClipController::getPassPropertiesList());
                        Mlt::Filter scaler(*m_qmlView->profile(), "swscale");
                        Mlt::Filter converter(*m_qmlView->profile(), "avcolor_space");
                        tmpProd->attach(scaler);
                        tmpProd->attach(converter);
                        frame = tmpProd->get_frame();
		    }
		    else {
			tmpProd = producer;
		    }
		    QImage img = KThumb::getFrame(frame, fullWidth, info.imageHeight);
                    if (frameNumber == -1) {
                        // No user specipied frame, look for best one
                        int variance = KThumb::imageVariance(img);
                        if (variance < 6) {
			    // Thumbnail is not interesting (for example all black, seek to fetch better thumb
                            delete frame;
			    frameNumber =  duration > 100 ? 100 : duration / 2 ;
			    tmpProd->seek(frameNumber);
			    frame = tmpProd->get_frame();
                            img = KThumb::getFrame(frame, fullWidth, info.imageHeight);
			}
		    }
                    if (KdenliveSettings::gpu_accel()) {
                        delete tmpProd;
                    }
		    if (frameNumber > -1) filePropertyMap["thumbnailFrame"] = QString::number(frameNumber);
		    emit replyGetImage(info.clipId, img);
		} else if (frame->get_int("test_audio") == 0) {
		    QIcon icon = QIcon::fromTheme("audio-x-generic");
		    QImage img(fullWidth, info.imageHeight, QImage::Format_ARGB32_Premultiplied);
		    img.fill(Qt::transparent);
		    QPainter painter( &img );
		    icon.paint(&painter, 0, 0, img.width(), img.height());
		    emit replyGetImage(info.clipId, img);
		    filePropertyMap["type"] = "audio";
		}
                delete frame;

		if (vindex > -1) {
		    /*if (context->duration == AV_NOPTS_VALUE) {
		    //qDebug() << " / / / / / / / /ERROR / / / CLIP HAS UNKNOWN DURATION";
		    emit removeInvalidClip(clipId);
		    delete producer;
		    return;
		}*/
		    // Get the video_index
		    int video_max = 0;
		    int default_audio = producer->get_int("audio_index");
		    int audio_max = 0;

		    int scan = producer->get_int("meta.media.progressive");
		    filePropertyMap["progressive"] = QString::number(scan);

		    // Find maximum stream index values
		    for (int ix = 0; ix < producer->get_int("meta.media.nb_streams"); ++ix) {
			snprintf(property, sizeof(property), "meta.media.%d.stream.type", ix);
			QString type = producer->get(property);
			if (type == "video")
			    video_max = ix;
			else if (type == "audio")
			    audio_max = ix;
		    }
		    filePropertyMap["default_video"] = QString::number(vindex);
		    filePropertyMap["video_max"] = QString::number(video_max);
		    filePropertyMap["default_audio"] = QString::number(default_audio);
		    filePropertyMap["audio_max"] = QString::number(audio_max);

		    snprintf(property, sizeof(property), "meta.media.%d.codec.long_name", vindex);
		    if (producer->get(property)) {
			filePropertyMap["videocodec"] = producer->get(property);
		    }
		    snprintf(property, sizeof(property), "meta.media.%d.codec.name", vindex);
		    if (producer->get(property)) {
			filePropertyMap["videocodecid"] = producer->get(property);
		    }
		    QString query;
		    query = QString("meta.media.%1.codec.pix_fmt").arg(vindex);
		    filePropertyMap["pix_fmt"] = producer->get(query.toUtf8().constData());
		    filePropertyMap["colorspace"] = producer->get("meta.media.colorspace");

		} else qDebug() << " / / / / /WARNING, VIDEO CONTEXT IS NULL!!!!!!!!!!!!!!";
		if (producer->get_int("audio_index") > -1) {
		    // Get the audio_index
		    int index = producer->get_int("audio_index");
		    snprintf(property, sizeof(property), "meta.media.%d.codec.long_name", index);
		    if (producer->get(property)) {
			filePropertyMap["audiocodec"] = producer->get(property);
		    } else {
			snprintf(property, sizeof(property), "meta.media.%d.codec.name", index);
			if (producer->get(property))
			    filePropertyMap["audiocodec"] = producer->get(property);
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
            if (name.endsWith(QLatin1String(".markup")) && !value.isEmpty())
                metadataPropertyMap[ name.section('.', 0, -2)] = value;
        }
        producer->seek(0);
        if (m_binController->hasClip(info.clipId)) {
            // If controller already exists, we just want to update the producer
            m_binController->replaceProducer(info.clipId, *producer);
            emit gotFileProperties(info, NULL);
        }
        else {
            // Create the controller
            ClipController *controller = new ClipController(m_binController, *producer);
            m_binController->addClipToBin(info.clipId, controller);
            emit gotFileProperties(info, controller);
        }
        m_processingClipId.removeAll(info.clipId);
    }
}


#if 0
/** Create the producer from the MLT XML QDomDocument */
void Render::initSceneList()
{
    //qDebug() << "--------  INIT SCENE LIST ------_";
    QDomDocument doc;
    QDomElement mlt = doc.createElement("mlt");
    doc.appendChild(mlt);
    QDomElement prod = doc.createElement("producer");
    prod.setAttribute("resource", "colour");
    prod.setAttribute("colour", "red");
    prod.setAttribute("id", "black");
    prod.setAttribute("in", "0");
    prod.setAttribute("out", "0");

    QDomElement tractor = doc.createElement("tractor");
    QDomElement multitrack = doc.createElement("multitrack");

    QDomElement playlist1 = doc.createElement("playlist");
    playlist1.appendChild(prod);
    multitrack.appendChild(playlist1);
    QDomElement playlist2 = doc.createElement("playlist");
    multitrack.appendChild(playlist2);
    QDomElement playlist3 = doc.createElement("playlist");
    multitrack.appendChild(playlist3);
    QDomElement playlist4 = doc.createElement("playlist");
    multitrack.appendChild(playlist4);
    QDomElement playlist5 = doc.createElement("playlist");
    multitrack.appendChild(playlist5);
    tractor.appendChild(multitrack);
    mlt.appendChild(tractor);
    // //qDebug()<<doc.toString();
    /*
       QString tmp = QString("<mlt><producer resource=\"colour\" colour=\"red\" id=\"red\" /><tractor><multitrack><playlist></playlist><playlist></playlist><playlist /><playlist /><playlist></playlist></multitrack></tractor></mlt>");*/
    setSceneList(doc, 0);
}
#endif

void Render::loadUrl(const QString &url)
{
    Mlt::Producer *producer = new Mlt::Producer(*m_qmlView->profile(), url.toUtf8().constData());
    setProducer(producer, 0, true);
}

bool Render::updateProducer(Mlt::Producer *producer)
{
    if (m_mltProducer) {
        if (strcmp(m_mltProducer->get("resource"), "<tractor>") == 0) {
            // We need to make some cleanup
            Mlt::Tractor trac(*m_mltProducer);
            for (int i = 0; i < trac.count(); i++) {
                trac.set_track(*m_blackClip, i);
            }
        }
        delete m_mltProducer;
        m_mltProducer = NULL;
    }
    if (m_mltConsumer) {
        if (!m_mltConsumer->is_stopped()) {
            m_mltConsumer->stop();
        }
    }
    if (!producer || !producer->is_valid()) {
        return false;
    }
    m_fps = producer->get_fps();
    m_mltProducer = producer;
    if (m_qmlView) {
        m_qmlView->setProducer(producer, false);
        m_mltConsumer = m_qmlView->consumer();
    }
    return true;
}

bool Render::setProducer(Mlt::Producer *producer, int position, bool isActive)
{
    m_refreshTimer.stop();
    requestedSeekPosition = SEEK_INACTIVE;
    QMutexLocker locker(&m_mutex);
    QString currentId;
    int consumerPosition = 0;
    if (m_mltProducer) {
        currentId = m_mltProducer->get("id");
        m_mltProducer->set_speed(0);
        if (QString(m_mltProducer->get("resource")) == "<tractor>") {
            // We need to make some cleanup
            Mlt::Tractor trac(*m_mltProducer);
            for (int i = 0; i < trac.count(); i++) {
                trac.set_track(*m_blackClip, i);
            }
        }
        delete m_mltProducer;
        m_mltProducer = NULL;
    }
    if (m_mltConsumer) {
        if (!m_mltConsumer->is_stopped()) {
            isActive = true;
            m_mltConsumer->stop();
        }
        //m_mltConsumer->purge();
        consumerPosition = m_mltConsumer->position();
    }
    blockSignals(true);
    if (!producer || !producer->is_valid()) {
        producer = m_blackClip->cut(0,1);
    }

    emit stopped();
    if (position == -1 && producer->get("id") == currentId) position = consumerPosition;
    if (position != -1) producer->seek(position);
    m_fps = producer->get_fps();

    blockSignals(false);
    m_mltProducer = producer;
    m_mltProducer->set_speed(0);
    if (m_qmlView) {
        m_qmlView->setProducer(producer);
        m_mltConsumer = m_qmlView->consumer();
        //m_mltConsumer->set("refresh", 1);
    }
    //m_mltConsumer->connect(*producer);
    if (isActive) {
        startConsumer();
    }
    emit durationChanged(m_mltProducer->get_playtime() - 1, m_mltProducer->get_in());
    position = m_mltProducer->position();
    emit rendererPosition(position);
    return true;
}

void Render::startConsumer() {
    if (m_mltConsumer->is_stopped() && m_mltConsumer->start() == -1) {
        // ARGH CONSUMER BROKEN!!!!
        KMessageBox::error(qApp->activeWindow(), i18n("Could not create the video preview window.\nThere is something wrong with your Kdenlive install or your driver settings, please fix it."));
        if (m_showFrameEvent) delete m_showFrameEvent;
        m_showFrameEvent = NULL;
        if (m_pauseEvent) delete m_pauseEvent;
        m_pauseEvent = NULL;
        delete m_mltConsumer;
        m_mltConsumer = NULL;
        return;
    }
    m_isRefreshing = true;
    m_mltConsumer->set("refresh", 1);
    m_isActive = true;
}

int Render::setSceneList(const QDomDocument &list, int position)
{
    return setSceneList(list.toString(), position);
}

int Render::setSceneList(QString playlist, int position)
{
    requestedSeekPosition = SEEK_INACTIVE;
    m_refreshTimer.stop();
    QMutexLocker locker(&m_mutex);
    //if (m_winid == -1) return -1;
    int error = 0;

    //qDebug() << "//////  RENDER, SET SCENE LIST:\n" << playlist <<"\n..........:::.";

    // Remove previous profile info
    QDomDocument doc;
    doc.setContent(playlist);
    QDomElement profile = doc.documentElement().firstChildElement("profile");
    doc.documentElement().removeChild(profile);
    playlist = doc.toString();

    if (m_mltConsumer) {
        if (!m_mltConsumer->is_stopped()) {
            m_mltConsumer->stop();
        }
    } else {
        qWarning() << "///////  ERROR, TRYING TO USE NULL MLT CONSUMER";
        error = -1;
    }
    m_requestList.clear();
    m_infoThread.waitForFinished();

    if (m_mltProducer) {
        m_mltProducer->set_speed(0);
        qDeleteAll(m_slowmotionProducers.values());
        m_slowmotionProducers.clear();

        delete m_mltProducer;
        m_mltProducer = NULL;
        emit stopped();
    }
    m_binController->destroyBin();
    blockSignals(true);
    m_locale = QLocale();
    m_locale.setNumberOptions(QLocale::OmitGroupSeparator);
    m_mltProducer = new Mlt::Producer(*m_qmlView->profile(), "xml-string", playlist.toUtf8().constData());
    //qDebug()<<" + + +PLAYLIST: "<<playlist;
    //m_mltProducer = new Mlt::Producer(*m_qmlView->profile(), "xml-nogl-string", playlist.toUtf8().constData());
    if (!m_mltProducer || !m_mltProducer->is_valid()) {
        qDebug() << " WARNING - - - - -INVALID PLAYLIST: " << playlist.toUtf8().constData();
        m_mltProducer = m_blackClip->cut(0, 1);
        error = -1;
    }
    m_mltProducer->set("eof", "pause");
    checkMaxThreads();
    m_mltProducer->optimise();

    m_fps = m_mltProducer->get_fps();
    if (position != 0) {
        // Seek to correct place after opening project.
        m_mltProducer->seek(position);
    }

    // Fill Bin's playlist
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() != tractor_type) {
        qWarning() << "// TRACTOR PROBLEM";
    }
    blockSignals(false);
    Mlt::Tractor tractor(service);
    Mlt::Properties retainList((mlt_properties) tractor.get_data("xml_retain"));
    if (retainList.is_valid() && retainList.get_data(m_binController->binPlaylistId().toUtf8().constData())) {
        Mlt::Playlist playlist((mlt_playlist) retainList.get_data(m_binController->binPlaylistId().toUtf8().constData()));
        if (playlist.is_valid() && playlist.type() == playlist_type) {
            // Load bin clips
	    m_binController->initializeBin(playlist);
        }
    }
    // No Playlist found, create new one
    if (m_qmlView) {
        m_binController->createIfNeeded(m_qmlView->profile());
        QString retain = QString("xml_retain %1").arg(m_binController->binPlaylistId());
        tractor.set(retain.toUtf8().constData(), m_binController->service(), 0);
        //if (!m_binController->hasClip("black")) m_binController->addClipToBin("black", *m_blackClip);
        m_qmlView->setProducer(m_mltProducer);
        m_mltConsumer = m_qmlView->consumer();
    }

    //qDebug() << "// NEW SCENE LIST DURATION SET TO: " << m_mltProducer->get_playtime();
    //m_mltConsumer->connect(*m_mltProducer);
    m_mltProducer->set_speed(0);
    fillSlowMotionProducers();
    emit durationChanged(m_mltProducer->get_playtime() - 1);

    // Fill bin
    QStringList ids = m_binController->getClipIds();
    foreach(const QString &id, ids) {
        if (id == "black") {
            //TODO: delegate handling of black clip to bincontroller
            //delete m_blackClip;
            //m_blackClip = &original->parent();
        }
        else {
            // pass basic info, the others (folder, etc) will be taken from the producer itself
            requestClipInfo info;
            info.imageHeight = 0;
            info.clipId = id;
            info.replaceProducer = true;
            emit gotFileProperties(info, m_binController->getController(id));
        }
        //delete original;
    }

    ////qDebug()<<"// SETSCN LST, POS: "<<position;
    if (position != 0) emit rendererPosition(position);
    return error;
}

void Render::checkMaxThreads()
{
    // Make sure we don't use too much threads, MLT avformat does not cope with too much threads
    // Currently, Kdenlive uses the following avformat threads:
    // One thread to get info when adding a clip
    // One thread to create the timeline video thumbnails
    // One thread to create the audio thumbnails
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() != tractor_type) {
        qWarning() << "// TRACTOR PROBLEM"<<m_mltProducer->parent().get("mlt_service");
        return;
    }
    Mlt::Tractor tractor(service);
    int mltMaxThreads = mlt_service_cache_get_size(service.get_service(), "producer_avformat");
    int requestedThreads = tractor.count() + m_qmlView->realTime() + 2;
    if (requestedThreads > mltMaxThreads) {
        mlt_service_cache_set_size(service.get_service(), "producer_avformat", requestedThreads);
        //qDebug()<<"// MLT threads updated to: "<<mlt_service_cache_get_size(service.get_service(), "producer_avformat");
    }
}


const QString Render::sceneList()
{
    QString playlist;
    Mlt::Consumer xmlConsumer(*m_qmlView->profile(), "xml:kdenlive_playlist");
    //qDebug()<<" ++ + READY TO SAVE: "<<m_qmlView->profile()->width()<<" / "<<m_qmlView->profile()->description();
    if (!xmlConsumer.is_valid()) return QString();
    m_mltProducer->optimise();
    xmlConsumer.set("terminate_on_pause", 1);
    xmlConsumer.set("store", "kdenlive");
    Mlt::Producer prod(m_mltProducer->get_producer());
    if (!prod.is_valid()) return QString();
    bool split = m_isSplitView;
    if (split) slotSplitView(false);
    xmlConsumer.connect(prod);
    xmlConsumer.run();
    playlist = QString::fromUtf8(xmlConsumer.get("kdenlive_playlist"));
    if (split) slotSplitView(true);
    return playlist;
}

bool Render::saveSceneList(QString path, QDomElement kdenliveData)
{
    QFile file(path);
    QDomDocument doc;
    doc.setContent(sceneList(), false);
    if (doc.isNull()) return false;
    QDomElement root = doc.documentElement();
    if (!kdenliveData.isNull() && !root.isNull()) {
        // add Kdenlive specific tags
        root.appendChild(doc.importNode(kdenliveData, true));
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "//////  ERROR writing to file: " << path;
        return false;
    }
    file.write(doc.toString().toUtf8());
    if (file.error() != QFile::NoError) {
        file.close();
        return false;
    }
    file.close();
    return true;
}

void Render::saveZone(QPoint zone)
{
    QString clipFolder = KRecentDirs::dir(":KdenliveClipFolder");
    if (clipFolder.isEmpty()) {
        clipFolder = QDir::homePath();
    }
    QString url = QFileDialog::getSaveFileName(qApp->activeWindow(), i18n("Save Zone"), clipFolder, i18n("MLT playlist (*.mlt)"));
    Mlt::Consumer xmlConsumer(*m_qmlView->profile(), ("xml:" + url).toUtf8().constData());
    xmlConsumer.set("terminate_on_pause", 1);
    m_mltProducer->optimise();
    qDebug()<<" - - -- - SAVE ZONE SEVICE: "<<m_mltProducer->get("mlt_type");
    if (QString(m_mltProducer->get("mlt_type")) != "producer") {
	// TODO: broken
	QString scene = sceneList();
	Mlt::Producer duplicate(*m_mltProducer->profile(), "xml-string", scene.toUtf8().constData());
	duplicate.set_in_and_out(zone.x(), zone.y());
	qDebug()<<"/// CUT: "<<zone.x()<<"x"<< zone.y()<<" / "<<duplicate.get_length();
	xmlConsumer.connect(duplicate);
	xmlConsumer.run();
    }
    else {
	Mlt::Producer prod(m_mltProducer->get_producer());
	Mlt::Producer *prod2 = prod.cut(zone.x(), zone.y());
	Mlt::Playlist list;
	list.insert_at(0, *prod2, 0);
	//list.set("title", desc.toUtf8().constData());
	xmlConsumer.connect(list);
	xmlConsumer.run();
	delete prod2;
    }
}


bool Render::saveClip(int track, const GenTime &position, const QUrl &url, const QString &desc)
{
    // find clip
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    //TODO: memleak
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());

    int clipIndex = trackPlaylist.get_clip_index_at((int) position.frames(m_fps));
    QScopedPointer<Mlt::Producer> clip(trackPlaylist.get_clip(clipIndex));
    if (!clip) {
        //qDebug() << "WARINIG, CANNOT FIND CLIP ON track: " << track << ", AT POS: " << position.frames(m_fps);
        return false;
    }
    
    Mlt::Consumer xmlConsumer(*m_qmlView->profile(), ("xml:" + url.toLocalFile()).toUtf8().constData());
    xmlConsumer.set("terminate_on_pause", 1);
    Mlt::Playlist list;
    list.insert_at(0, clip.data(), 0);
    //delete clip;
    list.set("title", desc.toUtf8().constData());
    xmlConsumer.connect(list);
    xmlConsumer.run();
    //qDebug()<<"// SAVED: "<<url;
    return true;
}

double Render::fps() const
{
    return m_fps;
}

int Render::volume() const
{
    if (!m_mltConsumer || !m_mltProducer) return -1;
    if (m_mltConsumer->get("mlt_service") == QString("multi")) {
        return ((int) 100 * m_mltConsumer->get_double("0.volume"));
    }
    return ((int) 100 * m_mltConsumer->get_double("volume"));
}

void Render::start()
{
    m_refreshTimer.stop();
    QMutexLocker locker(&m_mutex);
    /*if (m_winid == -1) {
        //qDebug() << "-----  BROKEN MONITOR: " << m_name << ", RESTART";
        return;
    }*/
    if (!m_mltConsumer) {
        //qDebug()<<" / - - - STARTED BEFORE CONSUMER!!!";
        return;
    }
    if (m_mltConsumer->is_stopped()) {
        if (m_mltConsumer->start() == -1) {
            //KMessageBox::error(qApp->activeWindow(), i18n("Could not create the video preview window.\nThere is something wrong with your Kdenlive install or your driver settings, please fix it."));
            qWarning() << "/ / / / CANNOT START MONITOR";
        } else {
            m_mltConsumer->purge();
            m_isRefreshing = true;
            m_mltConsumer->set("refresh", 1);
        }
    }
}

void Render::stop()
{
    requestedSeekPosition = SEEK_INACTIVE;
    m_refreshTimer.stop();
    QMutexLocker locker(&m_mutex);
    m_isActive = false;
    if (m_mltProducer) {
        if (m_isZoneMode) resetZoneMode();
        m_mltProducer->set_speed(0.0);
    }
    if (m_mltConsumer) {
        m_mltConsumer->purge();
        if (!m_mltConsumer->is_stopped()) m_mltConsumer->stop();
    }
    m_isRefreshing = false;
}

void Render::stop(const GenTime & startTime)
{
    requestedSeekPosition = SEEK_INACTIVE;
    m_refreshTimer.stop();
    QMutexLocker locker(&m_mutex);
    m_isActive = false;
    if (m_mltProducer) {
        if (m_isZoneMode) resetZoneMode();
        m_mltProducer->set_speed(0.0);
        m_mltProducer->seek((int) startTime.frames(m_fps));
    }
    if (m_mltConsumer) {
        m_mltConsumer->purge();
    }
    m_isRefreshing = false;
}

void Render::pause()
{
    requestedSeekPosition = SEEK_INACTIVE;
    if (!m_mltProducer || !m_mltConsumer || !m_isActive)
        return;
    m_mltProducer->set_speed(0.0);
    //if (!m_mltConsumer->is_stopped()) m_mltConsumer->stop();
    //m_mltProducer->seek(m_mltConsumer->position());
}

void Render::setActiveMonitor()
{
    if (!m_isActive) emit activateMonitor(m_name);
}

void Render::switchPlay(bool play)
{
    QMutexLocker locker(&m_mutex);
    requestedSeekPosition = SEEK_INACTIVE;
    if (!m_mltProducer || !m_mltConsumer || !m_isActive)
        return;
    if (m_isZoneMode) resetZoneMode();
    m_mltProducer->set_speed(play ? 1.0 : 0.0);
    if (play) {
        if (m_name == Kdenlive::ClipMonitor && m_mltConsumer->position() == m_mltProducer->get_out()) m_mltProducer->seek(0);
        if (m_mltConsumer->get_int("real_time") != m_qmlView->realTime()) {
            m_mltConsumer->set("real_time", m_qmlView->realTime());
            m_mltConsumer->set("buffer", 25);
            m_mltConsumer->set("prefill", 1);
            // Changes to real_time require a consumer restart if running.
            if (!m_mltConsumer->is_stopped())
                m_mltConsumer->stop();
        }
        m_mltConsumer->start();
        m_isRefreshing = true;
        m_mltConsumer->set("refresh", 1);
    } else {
        m_mltConsumer->set("buffer", 0);
        m_mltConsumer->set("prefill", 0);
        m_mltConsumer->set("real_time", -1);
        m_mltProducer->seek(m_mltConsumer->position() + 1);
        m_mltConsumer->purge();
        m_mltConsumer->start();
    }
}

void Render::play(double speed)
{
    requestedSeekPosition = SEEK_INACTIVE;
    if (!m_mltProducer || !m_isActive) return;
    double current_speed = m_mltProducer->get_speed();
    if (current_speed == speed) return;
    if (m_isZoneMode) resetZoneMode();
    // if (speed == 0.0) m_mltProducer->set("out", m_mltProducer->get_length() - 1);
    m_mltProducer->set_speed(speed);
    if (m_mltConsumer->is_stopped() && speed != 0) {
        m_mltConsumer->start();
    }
    if (current_speed == 0 && speed != 0) {
        m_isRefreshing = true;
        m_mltConsumer->set("refresh", 1);
    }
}

void Render::play(const GenTime & startTime)
{
    requestedSeekPosition = SEEK_INACTIVE;
    if (!m_mltProducer || !m_mltConsumer || !m_isActive)
        return;
    m_mltProducer->seek((int)(startTime.frames(m_fps)));
    m_mltProducer->set_speed(1.0);
    m_isRefreshing = true;
    m_mltConsumer->set("refresh", 1);
}

void Render::loopZone(const GenTime & startTime, const GenTime & stopTime)
{
    requestedSeekPosition = SEEK_INACTIVE;
    if (!m_mltProducer || !m_mltConsumer || !m_isActive)
        return;
    //m_mltProducer->set("eof", "loop");
    m_isLoopMode = true;
    m_loopStart = startTime;
    playZone(startTime, stopTime);
}

bool Render::playZone(const GenTime & startTime, const GenTime & stopTime)
{
    requestedSeekPosition = SEEK_INACTIVE;
    if (!m_mltProducer || !m_mltConsumer || !m_isActive)
        return false;
    m_mltProducer->seek((int)(startTime.frames(m_fps)));
    m_mltProducer->set_speed(0);
    m_mltConsumer->purge();
    m_mltProducer->set("out", (int)(stopTime.frames(m_fps)));
    m_mltProducer->set_speed(1.0);
    if (m_mltConsumer->is_stopped()) m_mltConsumer->start();
    m_isRefreshing = true;
    m_mltConsumer->set("refresh", 1);
    m_isZoneMode = true;
    return true;
}

void Render::resetZoneMode()
{
    if (!m_isZoneMode && !m_isLoopMode) return;
    m_mltProducer->set("out", m_mltProducer->get_length());
    m_isZoneMode = false;
    m_isLoopMode = false;
}

void Render::seekToFrame(int pos)
{
    if (!m_mltProducer || !m_isActive)
        return;
    pos = qMax(0, pos - m_mltProducer->get_in());
    pos = qMin(m_mltProducer->get_length(), pos);
    seek(pos);
}

void Render::seekToFrameDiff(int diff)
{
    if (!m_mltProducer || !m_isActive)
        return;
    if (requestedSeekPosition == SEEK_INACTIVE) {
        seek(m_mltConsumer->position() + diff);
    }
    else {
        seek(requestedSeekPosition + diff);
    }
}

void Render::doRefresh()
{
    if (m_mltProducer && (playSpeed() == 0) && m_isActive) {
        if (m_isRefreshing) m_refreshTimer.start();
        else refresh();
    }
}

void Render::refresh()
{
    m_refreshTimer.stop();
    QMutexLocker locker(&m_mutex);
    if (!m_mltProducer || !m_isActive)
        return;
    if (m_mltConsumer) {
        m_isRefreshing = true;
        if (m_mltConsumer->is_stopped()) m_mltConsumer->start();
        m_mltConsumer->purge();
        m_isRefreshing = true;
        m_mltConsumer->set("refresh", 1);
        //m_mltConsumer->purge();
    }
}

void Render::setDropFrames(bool drop)
{
    QMutexLocker locker(&m_mutex);
    if (m_mltConsumer) {
        int dropFrames = m_qmlView->realTime();
        if (drop == false) dropFrames = -dropFrames;
        //m_mltConsumer->stop();
        m_mltConsumer->set("real_time", dropFrames);
        if (m_mltConsumer->start() == -1) {
            qWarning() << "ERROR, Cannot start monitor";
        }

    }
}

void Render::setConsumerProperty(const QString &name, const QString &value)
{
    QMutexLocker locker(&m_mutex);
    if (m_mltConsumer) {
        //m_mltConsumer->stop();
        m_mltConsumer->set(name.toUtf8().constData(), value.toUtf8().constData());
        if (m_isActive && m_mltConsumer->start() == -1) {
            qWarning() << "ERROR, Cannot start monitor";
        }

    }
}

bool Render::isPlaying() const
{
    if (!m_mltConsumer || m_mltConsumer->is_stopped()) return false;
    return playSpeed() != 0;
}

double Render::playSpeed() const
{
    if (m_mltProducer) return m_mltProducer->get_speed();
    return 0.0;
}

GenTime Render::seekPosition() const
{
    if (m_mltConsumer) return GenTime((int) m_mltConsumer->position(), m_fps);
    //if (m_mltProducer) return GenTime((int) m_mltProducer->position(), m_fps);
    else return GenTime();
}

int Render::seekFramePosition() const
{
    //if (m_mltProducer) return (int) m_mltProducer->position();
    if (m_mltConsumer) return (int) m_mltConsumer->position();
    return 0;
}

void Render::emitFrameUpdated(Mlt::Frame& frame)
{
    Q_UNUSED(frame)
    return;
    /*TODO: fix movit crash
    mlt_image_format format = mlt_image_rgb24;
    int width = 0;
    int height = 0;
    //frame.set("rescale.interp", "bilinear");
    //frame.set("deinterlace_method", "onefield");
    //frame.set("top_field_first", -1);
    const uchar* image = frame.get_image(format, width, height);
    QImage qimage(width, height, QImage::Format_RGB888);  //Format_ARGB32_Premultiplied);
    memcpy(qimage.scanLine(0), image, width * height * 3);
    emit frameUpdated(qimage);
    */
}

int Render::getCurrentSeekPosition() const
{
    if (requestedSeekPosition != SEEK_INACTIVE) return requestedSeekPosition;
    return (int) m_mltProducer->position();
}

bool Render::checkFrameNumber(int pos)
{
    if (pos == requestedSeekPosition) {
        requestedSeekPosition = SEEK_INACTIVE;
    }
    if (requestedSeekPosition != SEEK_INACTIVE) {
        double speed = m_mltProducer->get_speed();
        m_mltProducer->set_speed(0);
        m_mltProducer->seek(requestedSeekPosition);
        if (speed == 0) {
            m_mltConsumer->set("refresh", 1);
        }
        else m_mltProducer->set_speed(speed);
    } else {
        m_isRefreshing = false;
        if (m_isZoneMode) {
            if (pos >= m_mltProducer->get_int("out") - 1) {
                if (m_isLoopMode) {
                    m_mltConsumer->purge();
                    m_mltProducer->seek((int)(m_loopStart.frames(m_fps)));
                    m_mltProducer->set_speed(1.0);
                    m_mltConsumer->set("refresh", 1);
                } else {
                    if (m_mltProducer->get_speed() == 0) return false;
                }
            }
        }
    }
    return true;
}

void Render::emitFrameUpdated(QImage img)
{
    emit frameUpdated(img);
}

void Render::slotCheckSeeking()
{
    if (requestedSeekPosition != SEEK_INACTIVE) {
        m_mltProducer->seek(requestedSeekPosition);
        requestedSeekPosition = SEEK_INACTIVE;
    }
}

void Render::showAudio(Mlt::Frame& frame)
{
    if (!frame.is_valid() || frame.get_int("test_audio") != 0) {
        return;
    }

    mlt_audio_format audio_format = mlt_audio_s16;
    //FIXME: should not be hardcoded..
    int freq = 48000;
    int num_channels = 2;
    int samples = 0;
    qint16* data = (qint16*)frame.get_audio(audio_format, freq, num_channels, samples);

    if (!data) {
        return;
    }

    // Data format: [ c00 c10 c01 c11 c02 c12 c03 c13 ... c0{samples-1} c1{samples-1} for 2 channels.
    // So the vector is of size samples*channels.
    audioShortVector sampleVector(samples*num_channels);
    memcpy(sampleVector.data(), data, samples*num_channels*sizeof(qint16));

    if (samples > 0) {
        emit audioSamplesSignal(sampleVector, freq, num_channels, samples);
    }
}

/*
 * MLT playlist direct manipulation.
 */

void Render::mltCheckLength(Mlt::Tractor *tractor)
{
    int trackNb = tractor->count();
    int duration = 0;
    if (m_isZoneMode) resetZoneMode();
    if (trackNb == 1) {
        QScopedPointer<Mlt::Producer> trackProducer(tractor->track(0));
        duration = trackProducer->get_playtime() - 1;
        m_mltProducer->set("out", duration);
        emit durationChanged(duration);
        return;
    }
    while (trackNb > 1) {
        QScopedPointer<Mlt::Producer> trackProducer(tractor->track(trackNb - 1));
        int trackDuration = trackProducer->get_playtime() - 1;
        if (trackDuration > duration) duration = trackDuration;
        trackNb--;
    }
    QScopedPointer<Mlt::Producer> blackTrackProducer(tractor->track(0));

    if (blackTrackProducer->get_playtime() - 1 != duration) {
        Mlt::Playlist blackTrackPlaylist((mlt_playlist) blackTrackProducer->get_service());
        QScopedPointer<Mlt::Producer> blackclip(blackTrackPlaylist.get_clip(0));
        if (!blackclip || blackclip->is_blank() || blackTrackPlaylist.count() != 1) {
            blackTrackPlaylist.clear();
            m_blackClip->set("length", duration + 1);
            m_blackClip->set("out", duration);
            Mlt::Producer *black2 = m_blackClip->cut(0, duration);
            blackTrackPlaylist.insert_at(0, black2, 1);
            delete black2;
        } else {
            if (duration > blackclip->parent().get_length()) {
                blackclip->parent().set("length", duration + 1);
                blackclip->parent().set("out", duration);
                blackclip->set("length", duration + 1);
            }
            blackTrackPlaylist.resize_clip(0, 0, duration);
        }
        if (m_mltConsumer->position() > duration) {
            m_mltConsumer->purge();
            m_mltProducer->seek(duration);
        }
        m_mltProducer->set("out", duration);
        emit durationChanged(duration);
    }
}

Mlt::Producer *Render::getTrackProducer(const QString &id, int track, bool, bool)
{
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() != tractor_type) {
        qWarning() << "// TRACTOR PROBLEM";
        return NULL;
    }
    Mlt::Tractor tractor(service);
    // WARNING: Kdenlive's track numbering is 0 for top track, while in MLT 0 is black track and 1 is the bottom track so we MUST reverse track number
    // TODO: memleak
    Mlt::Producer destTrackProducer(tractor.track(tractor.count() - track - 1));
    Mlt::Playlist destTrackPlaylist((mlt_playlist) destTrackProducer.get_service());
    return getProducerForTrack(destTrackPlaylist, id);
}


Mlt::Producer *Render::getProducerForTrack(Mlt::Playlist &trackPlaylist, const QString &clipId)
{
    //TODO: find a better way to check if a producer is already inserted in a track ?
    QString trackName = trackPlaylist.get("id");
    QString clipIdWithTrack = clipId + "_" + trackName;
    Mlt::Producer *prod = NULL;
    for (int i = 0; i < trackPlaylist.count(); i++) {
	if (trackPlaylist.is_blank(i)) continue;
	QScopedPointer<Mlt::Producer> p(trackPlaylist.get_clip(i));
	QString id = p->parent().get("id");
	if (id == clipIdWithTrack) {
	    // This producer already exists in the track, reuse it
	    prod = &p->parent();
	    break;
	}
    }
    if (prod == NULL) prod = m_binController->getBinProducer(clipId);
    return prod;
}

Mlt::Tractor *Render::lockService()
{
    // we are going to replace some clips, purge consumer
    if (!m_mltProducer) return NULL;
    QMutexLocker locker(&m_mutex);
    if (m_mltConsumer) {
        m_mltConsumer->purge();
    }
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() != tractor_type) {
        return NULL;
    }
    service.lock();
    return new Mlt::Tractor(service);

}

void Render::unlockService(Mlt::Tractor *tractor)
{
    if (tractor) {
        delete tractor;
    }
    if (!m_mltProducer) return;
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() != tractor_type) {
        qWarning() << "// TRACTOR PROBLEM";
        return;
    }
    service.unlock();
}




int Render::mltTrackDuration(int track)
{
    if (!m_mltProducer) {
        //qDebug() << "PLAYLIST NOT INITIALISED //////";
        return -1;
    }
    Mlt::Producer parentProd(m_mltProducer->parent());
    if (parentProd.get_producer() == NULL) {
        //qDebug() << "PLAYLIST BROKEN, CANNOT INSERT CLIP //////";
        return -1;
    }

    Mlt::Service service(parentProd.get_service());
    Mlt::Tractor tractor(service);
    //TODO: memleak
    Mlt::Producer trackProducer(tractor.track(track));
    return trackProducer.get_playtime() - 1;
}

void Render::mltInsertSpace(QMap <int, int> trackClipStartList, QMap <int, int> trackTransitionStartList, int track, const GenTime &duration, const GenTime &timeOffset)
{
    if (!m_mltProducer) {
        //qDebug() << "PLAYLIST NOT INITIALISED //////";
        return;
    }
    Mlt::Producer parentProd(m_mltProducer->parent());
    if (parentProd.get_producer() == NULL) {
        //qDebug() << "PLAYLIST BROKEN, CANNOT INSERT CLIP //////";
        return;
    }
    ////qDebug()<<"// CLP STRT LST: "<<trackClipStartList;
    ////qDebug()<<"// TRA STRT LST: "<<trackTransitionStartList;

    Mlt::Service service(parentProd.get_service());
    Mlt::Tractor tractor(service);
    service.lock();
    int diff = duration.frames(m_fps);
    int offset = timeOffset.frames(m_fps);
    int insertPos;

    if (track != -1) {
        // insert space in one track only
	//TODO: memleak
        Mlt::Producer trackProducer(tractor.track(track));
        Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
        insertPos = trackClipStartList.value(track);
        if (insertPos != -1) {
            insertPos += offset;
            int clipIndex = trackPlaylist.get_clip_index_at(insertPos);
            if (diff > 0) {
                trackPlaylist.insert_blank(clipIndex, diff - 1);
            } else {
                if (!trackPlaylist.is_blank(clipIndex)) clipIndex --;
                if (!trackPlaylist.is_blank(clipIndex)) {
                    //qDebug() << "//// ERROR TRYING TO DELETE SPACE FROM " << insertPos;
                }
                int position = trackPlaylist.clip_start(clipIndex);
                int blankDuration = trackPlaylist.clip_length(clipIndex);
                if (blankDuration + diff == 0) {
                    trackPlaylist.remove(clipIndex);
                } else trackPlaylist.remove_region(position, -diff);
            }
            trackPlaylist.consolidate_blanks(0);
        }
        // now move transitions
        mlt_service serv = m_mltProducer->parent().get_service();
        mlt_service nextservice = mlt_service_get_producer(serv);
        mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
        QString mlt_type = mlt_properties_get(properties, "mlt_type");
        QString resource = mlt_properties_get(properties, "mlt_service");

        while (mlt_type == "transition") {
            mlt_transition tr = (mlt_transition) nextservice;
            int currentTrack = mlt_transition_get_b_track(tr);
            int currentIn = (int) mlt_transition_get_in(tr);
            int currentOut = (int) mlt_transition_get_out(tr);
            insertPos = trackTransitionStartList.value(track);
            if (insertPos != -1) {
                insertPos += offset;
                if (track == currentTrack && currentOut > insertPos && resource != "mix") {
                    mlt_transition_set_in_and_out(tr, currentIn + diff, currentOut + diff);
                }
            }
            nextservice = mlt_service_producer(nextservice);
            if (nextservice == NULL) break;
            properties = MLT_SERVICE_PROPERTIES(nextservice);
            mlt_type = mlt_properties_get(properties, "mlt_type");
            resource = mlt_properties_get(properties, "mlt_service");
        }
    } else {
        for (int trackNb = tractor.count() - 1; trackNb >= 1; --trackNb) {
            Mlt::Producer trackProducer(tractor.track(trackNb));
            Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());

            //int clipNb = trackPlaylist.count();
            insertPos = trackClipStartList.value(trackNb);
            if (insertPos != -1) {
                insertPos += offset;

                /* //qDebug()<<"-------------\nTRACK "<<trackNb<<" HAS "<<clipNb<<" CLPIS";
                 //qDebug() << "INSERT SPACE AT: "<<insertPos<<", DIFF: "<<diff<<", TK: "<<trackNb;
                        for (int i = 0; i < clipNb; ++i) {
                            //qDebug()<<"CLIP "<<i<<", START: "<<trackPlaylist.clip_start(i)<<", END: "<<trackPlaylist.clip_start(i) + trackPlaylist.clip_length(i);
                     if (trackPlaylist.is_blank(i)) //qDebug()<<"++ BLANK ++ ";
                     //qDebug()<<"-------------";
                 }
                 //qDebug()<<"END-------------";*/


                int clipIndex = trackPlaylist.get_clip_index_at(insertPos);
                if (diff > 0) {
                    trackPlaylist.insert_blank(clipIndex, diff - 1);
                } else {
                    if (!trackPlaylist.is_blank(clipIndex)) {
                        clipIndex --;
                    }
                    if (!trackPlaylist.is_blank(clipIndex)) {
                        //qDebug() << "//// ERROR TRYING TO DELETE SPACE FROM " << insertPos;
                    }
                    int position = trackPlaylist.clip_start(clipIndex);
                    int blankDuration = trackPlaylist.clip_length(clipIndex);
                    if (diff + blankDuration == 0) {
                        trackPlaylist.remove(clipIndex);
                    } else trackPlaylist.remove_region(position, - diff);
                }
                trackPlaylist.consolidate_blanks(0);
            }
        }
        // now move transitions
        mlt_service serv = m_mltProducer->parent().get_service();
        mlt_service nextservice = mlt_service_get_producer(serv);
        mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
        QString mlt_type = mlt_properties_get(properties, "mlt_type");
        QString resource = mlt_properties_get(properties, "mlt_service");

        while (mlt_type == "transition") {
            mlt_transition tr = (mlt_transition) nextservice;
            int currentIn = (int) mlt_transition_get_in(tr);
            int currentOut = (int) mlt_transition_get_out(tr);
            int currentTrack = mlt_transition_get_b_track(tr);
            insertPos = trackTransitionStartList.value(currentTrack);
            if (insertPos != -1) {
                insertPos += offset;
                if (currentOut > insertPos && resource != "mix") {
                    mlt_transition_set_in_and_out(tr, currentIn + diff, currentOut + diff);
                }
            }
            nextservice = mlt_service_producer(nextservice);
            if (nextservice == NULL) break;
            properties = MLT_SERVICE_PROPERTIES(nextservice);
            mlt_type = mlt_properties_get(properties, "mlt_type");
            resource = mlt_properties_get(properties, "mlt_service");
        }
    }
    service.unlock();
    mltCheckLength(&tractor);
    m_isRefreshing = true;
    m_mltConsumer->set("refresh", 1);
}


void Render::mltPasteEffects(Mlt::Producer *source, Mlt::Producer *dest)
{
    if (source == dest) return;
    Mlt::Service sourceService(source->get_service());
    Mlt::Service destService(dest->get_service());

    // move all effects to the correct producer
    int ct = 0;
    Mlt::Filter *filter = sourceService.filter(ct);
    while (filter) {
        if (filter->get_int("kdenlive_ix") != 0) {
            sourceService.detach(*filter);
            destService.attach(*filter);
        } else ct++;
        filter = sourceService.filter(ct);
    }
}


bool Render::mltRemoveTrackEffect(int track, int index, bool updateIndex)
{
    Mlt::Service service(m_mltProducer->parent().get_service());
    bool success = false;
    Mlt::Tractor tractor(service);
    //TODO: memleak
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    Mlt::Service clipService(trackPlaylist.get_service());

    service.lock();
    int ct = 0;
    Mlt::Filter *filter = clipService.filter(ct);
    while (filter) {
        if ((index == -1 && strcmp(filter->get("kdenlive_id"), ""))  || filter->get_int("kdenlive_ix") == index) {
            if (clipService.detach(*filter) == 0) {
                delete filter;
                success = true;
            }
        } else if (updateIndex) {
            // Adjust the other effects index
            if (filter->get_int("kdenlive_ix") > index) filter->set("kdenlive_ix", filter->get_int("kdenlive_ix") - 1);
            ct++;
        } else ct++;
        filter = clipService.filter(ct);
    }
    service.unlock();
    refresh();
    return success;
}

bool Render::mltRemoveEffect(int track, GenTime position, int index, bool updateIndex, bool doRefresh)
{
    if (position < GenTime()) {
        // Remove track effect
        return mltRemoveTrackEffect(track, index, updateIndex);
    }
    Mlt::Service service(m_mltProducer->parent().get_service());
    bool success = false;
    Mlt::Tractor tractor(service);
    //TODO: memleak
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());

    int clipIndex = trackPlaylist.get_clip_index_at((int) position.frames(m_fps));
    QScopedPointer<Mlt::Producer> clip(trackPlaylist.get_clip(clipIndex));
    if (!clip) {
        qDebug() << " / / / CANNOT FIND CLIP TO REMOVE EFFECT";
        return false;
    }

    Mlt::Service clipService(clip->get_service());
    
    success = removeFilterFromService(clipService, index, updateIndex);
    
    int duration = clip->get_playtime();
    if (doRefresh) {
        // Check if clip is visible in monitor
        int diff = trackPlaylist.clip_start(clipIndex) + duration - m_mltProducer->position();
        if (diff < 0 || diff > duration) doRefresh = false;
    }
    if (doRefresh) refresh();
    return success;
}

//static
bool Render::removeFilterFromService(Mlt::Service service, int effectIndex, bool updateIndex)
{
    service.lock();
    bool success = false;
    int ct = 0;
    Mlt::Filter *filter = service.filter(ct);
    while (filter) {
        if ((effectIndex == -1 && strcmp(filter->get("kdenlive_id"), "")) || filter->get_int("kdenlive_ix") == effectIndex) {
            if (service.detach(*filter) == 0) {
                delete filter;
                success = true;
            }
        } else if (updateIndex) {
            // Adjust the other effects index
            if (filter->get_int("kdenlive_ix") > effectIndex) filter->set("kdenlive_ix", filter->get_int("kdenlive_ix") - 1);
            ct++;
        } else ct++;
        filter = service.filter(ct);
    }
    service.unlock();
    return success;
}

bool Render::mltAddTrackEffect(int track, EffectsParameterList params)
{
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    //TODO: memleak
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    Mlt::Service trackService(trackProducer.get_service()); //trackPlaylist
    return mltAddEffect(trackService, params, trackProducer.get_playtime() - 1, true);
}


bool Render::mltAddEffect(int track, GenTime position, EffectsParameterList params, bool doRefresh)
{

    Mlt::Service service(m_mltProducer->parent().get_service());

    Mlt::Tractor tractor(service);
    //TODO: memleak
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());

    int clipIndex = trackPlaylist.get_clip_index_at((int) position.frames(m_fps));
    QScopedPointer<Mlt::Producer> clip(trackPlaylist.get_clip(clipIndex));
    if (!clip) {
        return false;
    }

    Mlt::Service clipService(clip->get_service());
    int duration = clip->get_playtime();
    if (doRefresh) {
        // Check if clip is visible in monitor
        int diff = trackPlaylist.clip_start(clipIndex) + duration - m_mltProducer->position();
        if (diff < 0 || diff > duration) doRefresh = false;
    }
    return mltAddEffect(clipService, params, duration, doRefresh);
}

bool Render::mltAddEffect(Mlt::Service service, EffectsParameterList params, int duration, bool doRefresh)
{
    bool updateIndex = false;
    const int filter_ix = params.paramValue("kdenlive_ix").toInt();
    int ct = 0;
    service.lock();

    Mlt::Filter *filter = service.filter(ct);
    while (filter) {
        if (filter->get_int("kdenlive_ix") == filter_ix) {
            // A filter at that position already existed, so we will increase all indexes later
            updateIndex = true;
            break;
        }
        ct++;
        filter = service.filter(ct);
    }

    if (params.paramValue("id") == "speed") {
        // special case, speed effect is not really inserted, we just update the other effects index (kdenlive_ix)
        ct = 0;
        filter = service.filter(ct);
        while (filter) {
            if (filter->get_int("kdenlive_ix") >= filter_ix) {
                if (updateIndex) filter->set("kdenlive_ix", filter->get_int("kdenlive_ix") + 1);
            }
            ct++;
            filter = service.filter(ct);
        }
        service.unlock();
        if (doRefresh) refresh();
        return true;
    }


    // temporarily remove all effects after insert point
    QList <Mlt::Filter *> filtersList;
    ct = 0;
    filter = service.filter(ct);
    while (filter) {
        if (filter->get_int("kdenlive_ix") >= filter_ix) {
            filtersList.append(filter);
            service.detach(*filter);
        } else ct++;
        filter = service.filter(ct);
    }

    bool success = addFilterToService(service, params, duration);

    // re-add following filters
    for (int i = 0; i < filtersList.count(); ++i) {
        Mlt::Filter *filter = filtersList.at(i);
        if (updateIndex)
            filter->set("kdenlive_ix", filter->get_int("kdenlive_ix") + 1);
        service.attach(*filter);
    }
    qDeleteAll(filtersList);
    service.unlock();
    if (doRefresh) refresh();
    return success;
}

// static
bool Render::addFilterToService(Mlt::Service service, EffectsParameterList params, int duration)
{
    // create filter
    QString tag =  params.paramValue("tag");
    QLocale locale;
    ////qDebug() << " / / INSERTING EFFECT: " << tag << ", REGI: " << region;
    QString kfr = params.paramValue("keyframes");
    if (!kfr.isEmpty()) {
        QStringList keyFrames = kfr.split(';', QString::SkipEmptyParts);
        ////qDebug() << "// ADDING KEYFRAME EFFECT: " << params.paramValue("keyframes");
        char *starttag = qstrdup(params.paramValue("starttag", "start").toUtf8().constData());
        char *endtag = qstrdup(params.paramValue("endtag", "end").toUtf8().constData());
        ////qDebug() << "// ADDING KEYFRAME TAGS: " << starttag << ", " << endtag;
        //double max = params.paramValue("max").toDouble();
        double min = params.paramValue("min").toDouble();
        double factor = params.paramValue("factor", "1").toDouble();
        double paramOffset = params.paramValue("offset", "0").toDouble();
        params.removeParam("starttag");
        params.removeParam("endtag");
        params.removeParam("keyframes");
        params.removeParam("min");
        params.removeParam("max");
        params.removeParam("factor");
        params.removeParam("offset");
        // Special case, only one keyframe, means we want a constant value
        if (keyFrames.count() == 1) {
            Mlt::Filter *filter = new Mlt::Filter(*service.profile(), qstrdup(tag.toUtf8().constData()));
            if (filter && filter->is_valid()) {
                filter->set("kdenlive_id", qstrdup(params.paramValue("id").toUtf8().constData()));
                int x1 = keyFrames.at(0).section('=', 0, 0).toInt();
                double y1 = keyFrames.at(0).section('=', 1, 1).toDouble();
                for (int j = 0; j < params.count(); ++j) {
                    filter->set(params.at(j).name().toUtf8().constData(), params.at(j).value().toUtf8().constData());
                }
                filter->set("in", x1);
                ////qDebug() << "// ADDING KEYFRAME vals: " << min<<" / "<<max<<", "<<y1<<", factor: "<<factor;
                filter->set(starttag, locale.toString(((min + y1) - paramOffset) / factor).toUtf8().data());
                service.attach(*filter);
                delete filter;
            } else {
                delete[] starttag;
                delete[] endtag;
                //qDebug() << "filter is NULL";
                service.unlock();
                return false;
            }
        } else for (int i = 0; i < keyFrames.size() - 1; ++i) {
            Mlt::Filter *filter = new Mlt::Filter(*service.profile(), qstrdup(tag.toUtf8().constData()));
            if (filter && filter->is_valid()) {
                filter->set("kdenlive_id", qstrdup(params.paramValue("id").toUtf8().constData()));
                int x1 = keyFrames.at(i).section('=', 0, 0).toInt();
                double y1 = keyFrames.at(i).section('=', 1, 1).toDouble();
                int x2 = keyFrames.at(i + 1).section('=', 0, 0).toInt();
                double y2 = keyFrames.at(i + 1).section('=', 1, 1).toDouble();
                if (x2 == -1) x2 = duration;

                for (int j = 0; j < params.count(); ++j) {
                    filter->set(params.at(j).name().toUtf8().constData(), params.at(j).value().toUtf8().constData());
                }

                filter->set("in", x1);
                filter->set("out", x2);
                ////qDebug() << "// ADDING KEYFRAME vals: " << min<<" / "<<max<<", "<<y1<<", factor: "<<factor;
                filter->set(starttag, locale.toString(((min + y1) - paramOffset) / factor).toUtf8().data());
                filter->set(endtag, locale.toString(((min + y2) - paramOffset) / factor).toUtf8().data());
                service.attach(*filter);
                delete filter;
            } else {
                delete[] starttag;
                delete[] endtag;
                //qDebug() << "filter is NULL";
                service.unlock();
                return false;
            }
        }
        delete[] starttag;
        delete[] endtag;
    } else {
        Mlt::Filter *filter;
        QString prefix;
        filter = new Mlt::Filter(*service.profile(), qstrdup(tag.toUtf8().constData()));
        if (filter && filter->is_valid()) {
            filter->set("kdenlive_id", qstrdup(params.paramValue("id").toUtf8().constData()));
        } else {
            //qDebug() << "filter is NULL";
            service.unlock();
            return false;
        }
        params.removeParam("kdenlive_id");
        if (params.hasParam("_sync_in_out")) {
            // This effect must sync in / out with parent clip
            params.removeParam("_sync_in_out");
            filter->set_in_and_out(service.get_int("in"), service.get_int("out"));
        }

        for (int j = 0; j < params.count(); ++j) {
            filter->set((prefix + params.at(j).name()).toUtf8().constData(), params.at(j).value().toUtf8().constData());
            //qDebug()<<" / / SET PARAM: "<<params.at(j).name()<<" = "<<params.at(j).value();
        }

        if (tag == "sox") {
            QString effectArgs = params.paramValue("id").section('_', 1);

            params.removeParam("id");
            params.removeParam("kdenlive_ix");
            params.removeParam("tag");
            params.removeParam("disable");
            params.removeParam("region");

            for (int j = 0; j < params.count(); ++j) {
                effectArgs.append(' ' + params.at(j).value());
            }
            ////qDebug() << "SOX EFFECTS: " << effectArgs.simplified();
            filter->set("effect", effectArgs.simplified().toUtf8().constData());
        }
        // attach filter to the clip
        service.attach(*filter);
        delete filter;
    }
    return true;
}

bool Render::mltEditTrackEffect(int track, EffectsParameterList params)
{
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    //TODO: memleak
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    Mlt::Service clipService(trackPlaylist.get_service());
    int ct = 0;
    QString index = params.paramValue("kdenlive_ix");
    QString tag =  params.paramValue("tag");

    Mlt::Filter *filter = clipService.filter(ct);
    while (filter) {
        if (filter->get_int("kdenlive_ix") == index.toInt()) {
            break;
        }
        delete filter;
        ct++;
        filter = clipService.filter(ct);
    }

    if (!filter) {
        //qDebug() << "WARINIG, FILTER FOR EDITING NOT FOUND, ADDING IT! " << index << ", " << tag;
        // filter was not found, it was probably a disabled filter, so add it to the correct place...

        bool success = false;//mltAddTrackEffect(track, params);
        return success;
    }
    QString prefix;
    QString ser = filter->get("mlt_service");
    if (ser == "region") prefix = "filter0.";
    service.lock();
    for (int j = 0; j < params.count(); ++j) {
        filter->set((prefix + params.at(j).name()).toUtf8().constData(), params.at(j).value().toUtf8().constData());
    }
    service.unlock();

    refresh();
    return true;
}

bool Render::mltEditEffect(int track, const GenTime &position, EffectsParameterList params, bool replaceEffect)
{
    int index = params.paramValue("kdenlive_ix").toInt();
    QString tag =  params.paramValue("tag");

    if (!params.paramValue("keyframes").isEmpty() || replaceEffect || tag.startsWith(QLatin1String("ladspa")) || tag == "sox" || tag == "autotrack_rectangle") {
        // This is a keyframe effect, to edit it, we remove it and re-add it.
        if (mltRemoveEffect(track, position, index, false)) {
            if (position < GenTime())
                return mltAddTrackEffect(track, params);
            else
                return mltAddEffect(track, position, params);
        }
    }
    if (position < GenTime()) {
        return mltEditTrackEffect(track, params);
    }

    // find filter
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    //TODO: memleak
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());

    int clipIndex = trackPlaylist.get_clip_index_at((int) position.frames(m_fps));
    QScopedPointer<Mlt::Producer> clip(trackPlaylist.get_clip(clipIndex));
    if (!clip) {
        //qDebug() << "WARINIG, CANNOT FIND CLIP ON track: " << track << ", AT POS: " << position.frames(m_fps);
        return false;
    }

    int duration = clip->get_playtime();
    bool needRefresh = true;
    // Check if clip is visible in monitor
    int diff = trackPlaylist.clip_start(clipIndex) + duration - m_mltProducer->position();
    if (diff < 0 || diff > duration)
        needRefresh = false;
    int ct = 0;

    Mlt::Filter *filter = clip->filter(ct);
    while (filter) {
        if (filter->get_int("kdenlive_ix") == index) {
            break;
        }
        delete filter;
        ct++;
        filter = clip->filter(ct);
    }

    if (!filter) {
        qDebug() << "WARINIG, FILTER FOR EDITING NOT FOUND, ADDING IT! " << index << ", " << tag;
        // filter was not found, it was probably a disabled filter, so add it to the correct place...

        bool success = mltAddEffect(track, position, params);
        return success;
    }
    ct = 0;
    QString ser = filter->get("mlt_service");
    QList <Mlt::Filter *> filtersList;
    service.lock();
    if (ser != tag) {
        // Effect service changes, delete effect and re-add it
        clip->detach(*filter);
        delete filter;
        // Delete all effects after deleted one
        filter = clip->filter(ct);
        while (filter) {
            if (filter->get_int("kdenlive_ix") > index) {
                filtersList.append(filter);
                clip->detach(*filter);
            }
            else ct++;
            filter = clip->filter(ct);
        }

        // re-add filter
        addFilterToService(*clip, params, clip->get_playtime());
        service.unlock();

        if (needRefresh)
            refresh();
        return true;
    }
    if (params.hasParam("_sync_in_out")) {
        // This effect must sync in / out with parent clip
        params.removeParam("_sync_in_out");
        filter->set_in_and_out(clip->get_in(), clip->get_out());
    }

    for (int j = 0; j < params.count(); ++j) {
        filter->set(params.at(j).name().toUtf8().constData(), params.at(j).value().toUtf8().constData());
    }

    for (int j = 0; j < filtersList.count(); ++j) {
        clip->attach(*(filtersList.at(j)));
    }
    qDeleteAll(filtersList);
    service.unlock();

    if (needRefresh)
        doRefresh();

    return true;
}

bool Render::mltEnableEffects(int track, const GenTime &position, const QList <int> &effectIndexes, bool disable)
{
    if (position < GenTime()) {
        return mltEnableTrackEffects(track, effectIndexes, disable);
    }
    // find filter
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    //TODO: memleak
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());

    int clipIndex = trackPlaylist.get_clip_index_at((int) position.frames(m_fps));
    QScopedPointer<Mlt::Producer> clip(trackPlaylist.get_clip(clipIndex));
    if (!clip) {
        //qDebug() << "WARINIG, CANNOT FIND CLIP ON track: " << track << ", AT POS: " << position.frames(m_fps);
        return false;
    }

    int duration = clip->get_playtime();
    bool doRefresh = true;
    // Check if clip is visible in monitor
    int diff = trackPlaylist.clip_start(clipIndex) + duration - m_mltProducer->position();
    if (diff < 0 || diff > duration)
        doRefresh = false;
    int ct = 0;

    Mlt::Filter *filter = clip->filter(ct);
    service.lock();
    while (filter) {
        if (effectIndexes.contains(filter->get_int("kdenlive_ix"))) {
            filter->set("disable", (int) disable);
        }
        ct++;
        filter = clip->filter(ct);
    }
    service.unlock();

    if (doRefresh) refresh();
    return true;
}

bool Render::mltEnableTrackEffects(int track, const QList <int> &effectIndexes, bool disable)
{
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    //TODO: memleak
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    Mlt::Service clipService(trackPlaylist.get_service());
    int ct = 0;

    Mlt::Filter *filter = clipService.filter(ct);
    service.lock();
    while (filter) {
        if (effectIndexes.contains(filter->get_int("kdenlive_ix"))) {
            filter->set("disable", (int) disable);
        }
        ct++;
        filter = clipService.filter(ct);
    }
    service.unlock();

    refresh();
    return true;
}

void Render::mltUpdateEffectPosition(int track, const GenTime &position, int oldPos, int newPos)
{
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    //TODO: memleak
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());

    int clipIndex = trackPlaylist.get_clip_index_at((int) position.frames(m_fps));
    QScopedPointer<Mlt::Producer> clip(trackPlaylist.get_clip(clipIndex));
    if (!clip) {
        //qDebug() << "WARINIG, CANNOT FIND CLIP ON track: " << track << ", AT POS: " << position.frames(m_fps);
        return;
    }

    Mlt::Service clipService(clip->get_service());
    int duration = clip->get_playtime();
    bool doRefresh = true;
    // Check if clip is visible in monitor
    int diff = trackPlaylist.clip_start(clipIndex) + duration - m_mltProducer->position();
    if (diff < 0 || diff > duration) doRefresh = false;

    int ct = 0;
    Mlt::Filter *filter = clipService.filter(ct);
    while (filter) {
        int pos = filter->get_int("kdenlive_ix");
        if (pos == oldPos) {
            filter->set("kdenlive_ix", newPos);
        } else ct++;
        filter = clipService.filter(ct);
    }
    if (doRefresh) refresh();
}

void Render::mltMoveEffect(int track, const GenTime &position, int oldPos, int newPos)
{
    if (position < GenTime()) {
        mltMoveTrackEffect(track, oldPos, newPos);
        return;
    }
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    //TODO: memleak
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());

    int clipIndex = trackPlaylist.get_clip_index_at((int) position.frames(m_fps));
    QScopedPointer<Mlt::Producer> clip(trackPlaylist.get_clip(clipIndex));
    if (!clip) {
        //qDebug() << "WARINIG, CANNOT FIND CLIP ON track: " << track << ", AT POS: " << position.frames(m_fps);
        return;
    }

    Mlt::Service clipService(clip->get_service());
    int duration = clip->get_playtime();
    bool doRefresh = true;
    // Check if clip is visible in monitor
    int diff = trackPlaylist.clip_start(clipIndex) + duration - m_mltProducer->position();
    if (diff < 0 || diff > duration) doRefresh = false;

    int ct = 0;
    QList <Mlt::Filter *> filtersList;
    Mlt::Filter *filter = clipService.filter(ct);
    if (newPos > oldPos) {
        bool found = false;
        while (filter) {
            if (!found && filter->get_int("kdenlive_ix") == oldPos) {
                filter->set("kdenlive_ix", newPos);
                filtersList.append(filter);
                clipService.detach(*filter);
                filter = clipService.filter(ct);
                while (filter && filter->get_int("kdenlive_ix") <= newPos) {
                    filter->set("kdenlive_ix", filter->get_int("kdenlive_ix") - 1);
                    ct++;
                    filter = clipService.filter(ct);
                }
                found = true;
            }
            if (filter && filter->get_int("kdenlive_ix") > newPos) {
                filtersList.append(filter);
                clipService.detach(*filter);
            } else ct++;
            filter = clipService.filter(ct);
        }
    } else {
        while (filter) {
            if (filter->get_int("kdenlive_ix") == oldPos) {
                filter->set("kdenlive_ix", newPos);
                filtersList.append(filter);
                clipService.detach(*filter);
            } else ct++;
            filter = clipService.filter(ct);
        }

        ct = 0;
        filter = clipService.filter(ct);
        while (filter) {
            int pos = filter->get_int("kdenlive_ix");
            if (pos >= newPos) {
                if (pos < oldPos) filter->set("kdenlive_ix", pos + 1);
                filtersList.append(filter);
                clipService.detach(*filter);
            } else ct++;
            filter = clipService.filter(ct);
        }
    }

    for (int i = 0; i < filtersList.count(); ++i) {
        clipService.attach(*(filtersList.at(i)));
    }
    qDeleteAll(filtersList);
    if (doRefresh) refresh();
}

void Render::mltMoveTrackEffect(int track, int oldPos, int newPos)
{
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    //TODO: memleak
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    Mlt::Service clipService(trackPlaylist.get_service());
    int ct = 0;
    QList <Mlt::Filter *> filtersList;
    Mlt::Filter *filter = clipService.filter(ct);
    if (newPos > oldPos) {
        bool found = false;
        while (filter) {
            if (!found && filter->get_int("kdenlive_ix") == oldPos) {
                filter->set("kdenlive_ix", newPos);
                filtersList.append(filter);
                clipService.detach(*filter);
                filter = clipService.filter(ct);
                while (filter && filter->get_int("kdenlive_ix") <= newPos) {
                    filter->set("kdenlive_ix", filter->get_int("kdenlive_ix") - 1);
                    ct++;
                    filter = clipService.filter(ct);
                }
                found = true;
            }
            if (filter && filter->get_int("kdenlive_ix") > newPos) {
                filtersList.append(filter);
                clipService.detach(*filter);
            } else ct++;
            filter = clipService.filter(ct);
        }
    } else {
        while (filter) {
            if (filter->get_int("kdenlive_ix") == oldPos) {
                filter->set("kdenlive_ix", newPos);
                filtersList.append(filter);
                clipService.detach(*filter);
            } else ct++;
            filter = clipService.filter(ct);
        }

        ct = 0;
        filter = clipService.filter(ct);
        while (filter) {
            int pos = filter->get_int("kdenlive_ix");
            if (pos >= newPos) {
                if (pos < oldPos) filter->set("kdenlive_ix", pos + 1);
                filtersList.append(filter);
                clipService.detach(*filter);
            } else ct++;
            filter = clipService.filter(ct);
        }
    }

    for (int i = 0; i < filtersList.count(); ++i) {
        clipService.attach(*(filtersList.at(i)));
    }
    qDeleteAll(filtersList);
    refresh();
}

bool Render::mltResizeClipCrop(ItemInfo info, GenTime newCropStart)
{
    Mlt::Service service(m_mltProducer->parent().get_service());
    int newCropFrame = (int) newCropStart.frames(m_fps);
    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(info.track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    if (trackPlaylist.is_blank_at(info.startPos.frames(m_fps))) {
        //qDebug() << "////////  ERROR RSIZING BLANK CLIP!!!!!!!!!!!";
        return false;
    }
    service.lock();
    int clipIndex = trackPlaylist.get_clip_index_at(info.startPos.frames(m_fps));
    QScopedPointer<Mlt::Producer> clip(trackPlaylist.get_clip(clipIndex));
    if (clip == NULL) {
        //qDebug() << "////////  ERROR RSIZING NULL CLIP!!!!!!!!!!!";
        service.unlock();
        return false;
    }
    int previousStart = clip->get_in();
    int previousOut = clip->get_out();
    if (previousStart == newCropFrame) {
        //qDebug() << "////////  No ReSIZING Required";
        service.unlock();
        return true;
    }
    int frameOffset = newCropFrame - previousStart;
    trackPlaylist.resize_clip(clipIndex, newCropFrame, previousOut + frameOffset);
    service.unlock();
    m_isRefreshing = true;
    m_mltConsumer->set("refresh", 1);
    return true;
}

QList <int> Render::checkTrackSequence(int track)
{
    QList <int> list;
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() != tractor_type) {
        qWarning() << "// TRACTOR PROBLEM";
        return list;
    }
    Mlt::Tractor tractor(service);
    service.lock();
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    int clipNb = trackPlaylist.count();
    ////qDebug() << "// PARSING SCENE TRACK: " << t << ", CLIPS: " << clipNb;
    for (int i = 0; i < clipNb; ++i) {
        QScopedPointer<Mlt::Producer> c(trackPlaylist.get_clip(i));
        int pos = trackPlaylist.clip_start(i);
        if (!list.contains(pos)) list.append(pos);
        pos += c->get_playtime();
        if (!list.contains(pos)) list.append(pos);
    }
    return list;
}



void Render::cloneProperties(Mlt::Properties &dest, Mlt::Properties &source)
{
    int count = source.count();
    int i = 0;
    for ( i = 0; i < count; i ++ )
    {
        char *value = source.get(i);
        if ( value != NULL )
        {
            char *name = source.get_name( i );
            if (name != NULL && name[0] != '_') dest.set(name, value);
        }
    }
}

// adds the transition by keeping the instance order from topmost track down to background
void Render::mltPlantTransition(Mlt::Field *field, Mlt::Transition &tr, int a_track, int b_track)
{
    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString mlt_type = mlt_properties_get(properties, "mlt_type");
    QString resource = mlt_properties_get(properties, "mlt_service");
    QList <Mlt::Transition *> trList;
    mlt_properties insertproperties = tr.get_properties();
    QString insertresource = mlt_properties_get(insertproperties, "mlt_service");
    bool isMixTransition = insertresource == "mix";

    while (mlt_type == "transition") {
        Mlt::Transition transition((mlt_transition) nextservice);
        nextservice = mlt_service_producer(nextservice);
        int aTrack = transition.get_a_track();
        int bTrack = transition.get_b_track();
        if ((isMixTransition || resource != "mix") && (aTrack < a_track || (aTrack == a_track && bTrack > b_track))) {
            Mlt::Properties trans_props(transition.get_properties());
            Mlt::Transition *cp = new Mlt::Transition(*m_qmlView->profile(), transition.get("mlt_service"));
            Mlt::Properties new_trans_props(cp->get_properties());
            //new_trans_props.inherit(trans_props);
            cloneProperties(new_trans_props, trans_props);
            trList.append(cp);
            field->disconnect_service(transition);
        }
        //else qDebug() << "// FOUND TRANS OK, "<<resource<< ", A_: " << aTrack << ", B_ "<<bTrack;

        if (nextservice == NULL) break;
        properties = MLT_SERVICE_PROPERTIES(nextservice);
        mlt_type = mlt_properties_get(properties, "mlt_type");
        resource = mlt_properties_get(properties, "mlt_service");
    }
    field->plant_transition(tr, a_track, b_track);

    // re-add upper transitions
    for (int i = trList.count() - 1; i >= 0; --i) {
        ////qDebug()<< "REPLANT ON TK: "<<trList.at(i)->get_a_track()<<", "<<trList.at(i)->get_b_track();
        field->plant_transition(*trList.at(i), trList.at(i)->get_a_track(), trList.at(i)->get_b_track());
    }
    qDeleteAll(trList);
}



QMap<QString, QString> Render::mltGetTransitionParamsFromXml(const QDomElement &xml)
{
    QDomNodeList attribs = xml.elementsByTagName("parameter");
    QMap<QString, QString> map;
    for (int i = 0; i < attribs.count(); ++i) {
        QDomElement e = attribs.item(i).toElement();
        QString name = e.attribute("name");
        ////qDebug()<<"-- TRANSITION PARAM: "<<name<<" = "<< e.attribute("name")<<" / " << e.attribute("value");
        map[name] = e.attribute("default");
        if (!e.attribute("value").isEmpty()) {
            map[name] = e.attribute("value");
        }
        if (e.attribute("type") != "addedgeometry" && (e.attribute("factor", "1") != "1" || e.attribute("offset", "0") != "0")) {
            map[name] = QLocale().toString((map.value(name).toDouble() - e.attribute("offset", "0").toDouble()) / e.attribute("factor", "1").toDouble());
            //map[name]=map[name].replace(".",","); //FIXME how to solve locale conversion of . ,
        }

        if (e.attribute("namedesc").contains(';')) {
            QString format = e.attribute("format");
            QStringList separators = format.split("%d", QString::SkipEmptyParts);
            QStringList values = e.attribute("value").split(QRegExp("[,:;x]"));
            QString neu;
            QTextStream txtNeu(&neu);
            if (values.size() > 0)
                txtNeu << (int)values[0].toDouble();
            int i = 0;
            for (i = 0; i < separators.size() && i + 1 < values.size(); ++i) {
                txtNeu << separators[i];
                txtNeu << (int)(values[i+1].toDouble());
            }
            if (i < separators.size())
                txtNeu << separators[i];
            map[e.attribute("name")] = neu;
        }

    }
    return map;
}


/*const QList <Mlt::Producer *> Render::producersList()
{
    QList <Mlt::Producer *> prods;
    if (m_mltProducer == NULL) return prods;
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() != tractor_type) return prods;
    Mlt::Tractor tractor(service);
    QStringList ids;

    int trackNb = tractor.count();
    for (int t = 1; t < trackNb; ++t) {
        Mlt::Producer *tt = tractor.track(t);
        Mlt::Producer trackProducer(tt);
        delete tt;
        Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
        if (!trackPlaylist.is_valid()) continue;
        int clipNb = trackPlaylist.count();
        for (int i = 0; i < clipNb; ++i) {
            Mlt::Producer *c = trackPlaylist.get_clip(i);
            if (c == NULL) continue;
            QString prodId = c->parent().get("id");
            if (!c->is_blank() && !ids.contains(prodId) && !prodId.startsWith(QLatin1String("slowmotion")) && !prodId.isEmpty()) {
                Mlt::Producer *nprod = new Mlt::Producer(c->get_parent());
                if (nprod) {
                    ids.append(prodId);
                    prods.append(nprod);
                }
            }
            delete c;
        }
    }
    return prods;
}*/

void Render::fillSlowMotionProducers()
{
    if (m_mltProducer == NULL) return;
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() != tractor_type) return;

    Mlt::Tractor tractor(service);

    int trackNb = tractor.count();
    for (int t = 1; t < trackNb; ++t) {
        Mlt::Producer *tt = tractor.track(t);
        Mlt::Producer trackProducer(tt);
        delete tt;
        Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
        if (!trackPlaylist.is_valid()) continue;
        int clipNb = trackPlaylist.count();
        for (int i = 0; i < clipNb; ++i) {
            QScopedPointer<Mlt::Producer> c(trackPlaylist.get_clip(i));
            Mlt::Producer *nprod = new Mlt::Producer(c->get_parent());
            if (nprod) {
                QString id = nprod->parent().get("id");
                if (id.startsWith(QLatin1String("slowmotion:")) && !nprod->is_blank()) {
                    // this is a slowmotion producer, add it to the list
                    QString url = QString::fromUtf8(nprod->get("resource"));
                    int strobe = nprod->get_int("strobe");
                    if (strobe > 1) url.append("&strobe=" + QString::number(strobe));
                    if (!m_slowmotionProducers.contains(url)) {
                        m_slowmotionProducers.insert(url, nprod);
                    }
                } else delete nprod;
            }
        }
    }
}

//Updates all transitions
QList <TransitionInfo> Render::mltInsertTrack(int ix, const QString &name, bool videoTrack)
{
    QList <TransitionInfo> transitionInfos;
    // Track add / delete was only added recently in MLT (pre 0.9.8 release).
#if (LIBMLT_VERSION_INT < 0x0908)
    Q_UNUSED(ix)
    Q_UNUSED(name)
    Q_UNUSED(videoTrack)
    qDebug()<<"Track insertion requires a more recent MLT version";
    return transitionInfos;
#else
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() != tractor_type) {
        qWarning() << "// TRACTOR PROBLEM";
        return QList <TransitionInfo> ();
    }
    blockSignals(true);
    service.lock();
    Mlt::Tractor tractor(service);
    Mlt::Playlist playlist;
    playlist.set("kdenlive:track_name", name.toUtf8().constData());
    int ct = tractor.count();
    if (ix > ct) {
        //qDebug() << "// ERROR, TRYING TO insert TRACK " << ix << ", max: " << ct;
        ix = ct;
    }

    tractor.insert_track(playlist, ix);
    Mlt::Producer newProd(tractor.track(ix));
    if (!videoTrack) {
        newProd.set("kdenlive:audio_track", 1);
        newProd.set("hide", 1);
    }
    checkMaxThreads();
    tractor.refresh();

    Mlt::Field *field = tractor.field();
    // Move transitions
    /*mlt_service serv = m_mltProducer->parent().get_service();
    mlt_service nextservice = mlt_service_get_producer(serv);
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString mlt_type = mlt_properties_get(properties, "mlt_type");
    QString resource = mlt_properties_get(properties, "mlt_service");
    QList <Mlt::Transition *> trList;

    while (mlt_type == "transition") {
        if (resource != "mix") {
            Mlt::Transition transition((mlt_transition) nextservice);
            nextservice = mlt_service_producer(nextservice);
            int currentbTrack = transition.get_b_track();
            int currentaTrack = transition.get_a_track();
            bool trackChanged = false;
            bool forceTransitionTrack = false;
            if (currentbTrack >= ix) {
                if (currentbTrack == ix && currentaTrack < ix) forceTransitionTrack = true;
                currentbTrack++;
                trackChanged = true;
            }
            if (currentaTrack >= ix) {
                currentaTrack++;
                trackChanged = true;
            }
            //qDebug()<<"// Newtrans: "<<currentaTrack<<"/"<<currentbTrack;

            // disconnect all transitions
            Mlt::Properties trans_props(transition.get_properties());
            Mlt::Transition *cp = new Mlt::Transition(*m_qmlView->profile(), transition.get("mlt_service"));
            Mlt::Properties new_trans_props(cp->get_properties());
            cloneProperties(new_trans_props, trans_props);
            //new_trans_props.inherit(trans_props);

            if (trackChanged) {
                // Transition track needs to be adjusted
                cp->set("a_track", currentaTrack);
                cp->set("b_track", currentbTrack);
                // Check if transition track was changed and needs to be forced
                if (forceTransitionTrack) cp->set("force_track", 1);
                TransitionInfo trInfo;
                trInfo.startPos = GenTime(transition.get_in(), m_fps);
                trInfo.a_track = currentaTrack;
                trInfo.b_track = currentbTrack;
                trInfo.forceTrack = cp->get_int("force_track");
                transitionInfos.append(trInfo);
            }
            trList.append(cp);
            field->disconnect_service(transition);
        }
        else nextservice = mlt_service_producer(nextservice);
        if (nextservice == NULL) break;
        properties = MLT_SERVICE_PROPERTIES(nextservice);
        mlt_type = mlt_properties_get(properties, "mlt_type");
        resource = mlt_properties_get(properties, "mlt_service");
    }*/

    // Add audio mix transition to last track
    Mlt::Transition mix(*m_qmlView->profile(), "mix");
    mix.set("a_track", 0);
    mix.set("b_track", ix);
    mix.set("always_active", 1);
    mix.set("internal_added", 237);
    mix.set("combine", 1);
    field->plant_transition(mix, 0, ix);

    if (videoTrack) {
        Mlt::Transition composite(*m_qmlView->profile(), KdenliveSettings::gpu_accel() ? "movit.overlay" : "frei0r.cairoblend");
        composite.set("a_track", ix - 1);
        composite.set("b_track", ix);
        composite.set("internal_added", 237);
        field->plant_transition(composite, ix - 1, ix);
        //mltPlantTransition(field, composite, ct-1, ct);
    }

    /*
    // re-add transitions
    for (int i = trList.count() - 1; i >= 0; --i) {
        field->plant_transition(*trList.at(i), trList.at(i)->get_a_track(), trList.at(i)->get_b_track());
    }
    qDeleteAll(trList);
    */
    
    service.unlock();
    blockSignals(false);
    return transitionInfos;
#endif
}

void Render::sendFrameUpdate()
{
    if (m_mltProducer) {
        Mlt::Frame * frame = m_mltProducer->get_frame();
        emitFrameUpdated(*frame);
        delete frame;
    }
}

Mlt::Producer* Render::getProducer()
{
    return m_mltProducer;
}

const QString Render::activeClipId()
{
    if (m_mltProducer) return m_mltProducer->get("id");
    return QString();
}

//static 
bool Render::getBlackMagicDeviceList(KComboBox *devicelist, bool force)
{
    if (!force && !KdenliveSettings::decklink_device_found()) return false;
    Mlt::Profile profile;
    Mlt::Producer bm(profile, "decklink");
    int found_devices = 0;
    if (bm.is_valid()) {
        bm.set("list_devices", 1);
        found_devices = bm.get_int("devices");
    }
    else KdenliveSettings::setDecklink_device_found(false);
    if (found_devices <= 0) {
        devicelist->setEnabled(false);
        return false;
    }
    KdenliveSettings::setDecklink_device_found(true);
    for (int i = 0; i < found_devices; ++i) {
        char *tmp = qstrdup(QString("device.%1").arg(i).toUtf8().constData());
        devicelist->addItem(bm.get(tmp));
        delete[] tmp;
    }
    return true;
}

bool Render::getBlackMagicOutputDeviceList(KComboBox *devicelist, bool force)
{
    if (!force && !KdenliveSettings::decklink_device_found()) return false;
    Mlt::Profile profile;
    Mlt::Consumer bm(profile, "decklink");
    int found_devices = 0;
    if (bm.is_valid()) {
        bm.set("list_devices", 1);;
        found_devices = bm.get_int("devices");
    }
    else KdenliveSettings::setDecklink_device_found(false);
    if (found_devices <= 0) {
        devicelist->setEnabled(false);
        return false;
    }
    KdenliveSettings::setDecklink_device_found(true);
    for (int i = 0; i < found_devices; ++i) {
        char *tmp = qstrdup(QString("device.%1").arg(i).toUtf8().constData());
        devicelist->addItem(bm.get(tmp));
        delete[] tmp;
    }
    return true;
}

void Render::slotMultiStreamProducerFound(const QString &path, QList<int> audio_list, QList<int> video_list, stringMap data)
{ 
    if (KdenliveSettings::automultistreams()) {
        for (int i = 1; i < video_list.count(); ++i) {
            int vindex = video_list.at(i);
            int aindex = 0;
            if (i <= audio_list.count() -1) {
                aindex = audio_list.at(i);
            }
            data.insert("video_index", QString::number(vindex));
            data.insert("audio_index", QString::number(aindex));
            data.insert("bypassDuplicate", "1");
            emit addClip(path, data);
        }
        return;
    }

    int width = 60.0 * m_qmlView->profile()->dar();
    if (width % 2 == 1) width++;

    QPointer<QDialog> dialog = new QDialog(qApp->activeWindow());
    dialog->setWindowTitle("Multi Stream Clip");
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    QWidget *mainWidget = new QWidget(dialog);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    dialog->setLayout(mainLayout);
    mainLayout->addWidget(mainWidget);
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    okButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    dialog->connect(buttonBox, SIGNAL(accepted()), dialog, SLOT(accept()));
    dialog->connect(buttonBox, SIGNAL(rejected()), dialog, SLOT(reject()));
    okButton->setText(i18n("Import selected clips"));
    
    QLabel *lab1 = new QLabel(i18n("Additional streams for clip\n %1", path), mainWidget);
    mainLayout->addWidget(lab1);
    QList <QGroupBox*> groupList;
    QList <KComboBox*> comboList;
    // We start loading the list at 1, video index 0 should already be loaded
    for (int j = 1; j < video_list.count(); ++j) {
        Mlt::Producer multiprod(* m_qmlView->profile(), path.toUtf8().constData());
        multiprod.set("video_index", video_list.at(j));
        QImage thumb = KThumb::getFrame(&multiprod, 0, width, 60);
        QGroupBox *streamFrame = new QGroupBox(i18n("Video stream %1", video_list.at(j)), mainWidget);
        mainLayout->addWidget(streamFrame);
        streamFrame->setProperty("vindex", video_list.at(j));
        groupList << streamFrame;
        streamFrame->setCheckable(true);
        streamFrame->setChecked(true);
        QVBoxLayout *vh = new QVBoxLayout( streamFrame );
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
                data.insert("video_index", QString::number(vindex));
                data.insert("audio_index", QString::number(aindex));
                data.insert("bypassDuplicate", "1");
                emit addClip(path, data);
            }
        }
    }
    delete dialog;
}

//static 
bool Render::checkX11Grab()
{
    if (KdenliveSettings::rendererpath().isEmpty() || KdenliveSettings::ffmpegpath().isEmpty()) return false;
    QProcess p;
    QStringList args;
    args << "avformat:f-list";
    p.start(KdenliveSettings::rendererpath(), args);
    if (!p.waitForStarted()) return false;
    if (!p.waitForFinished()) return false;
    QByteArray result = p.readAllStandardError();
    return result.contains("x11grab");
}

double Render::getMltVersionInfo(const QString &tag)
{
    double version = 0;
    Mlt::Properties *metadata = m_binController->mltRepository()->metadata(producer_type, tag.toUtf8().data());
    if (metadata && metadata->is_valid()) {
	version = metadata->get_double("version");
    }
    if (metadata) delete metadata;
    return version;
}

Mlt::Producer *Render::getBinProducer(const QString &id)
{
    return m_binController->getBinProducer(id);
}

Mlt::Producer *Render::getBinVideoProducer(const QString &id)
{
    return m_binController->getBinVideoProducer(id);
}

void Render::loadExtraProducer(const QString &id, Mlt::Producer *prod)
{
    m_binController->loadExtraProducer(id, prod);
}

const QString Render::getBinProperty(const QString &name)
{
    return m_binController->getProperty(name);
}


void Render::setVolume(double volume)
{
    if (m_mltConsumer) {
        if (m_mltConsumer->get("mlt_service") == QString("multi")) {
            m_mltConsumer->set("0.volume", volume);
        } else {
            m_mltConsumer->set("volume", volume);
        }
    }
}

void Render::storeSlowmotionProducer(const QString &url, Mlt::Producer *prod, bool replace)
{
      if (!m_slowmotionProducers.contains(url)) {
	    m_slowmotionProducers.insert(url, prod);
      }
      else if (replace) {
	    Mlt::Producer *old = m_slowmotionProducers.take(url);
	    delete old;
	    m_slowmotionProducers.insert(url, prod);
      }
}

Mlt::Producer *Render::getSlowmotionProducer(const QString &url)
{
      if (m_slowmotionProducers.contains(url)) {
	    return m_slowmotionProducers.value(url);
      }
      return NULL;
}

void Render::updateSlowMotionProducers(const QString &id, QMap <QString, QString> passProperties)
{
    QMapIterator<QString, Mlt::Producer *> i(m_slowmotionProducers);
    Mlt::Producer *prod;
    while (i.hasNext()) {
        i.next();
	prod = i.value();
	QString currentId = prod->get("id");
	if (currentId.startsWith("slowmotion:" + id + ":")) {
	  QMapIterator<QString, QString> j(passProperties);
            while (j.hasNext()) {
                j.next();
                prod->set(j.key().toUtf8().constData(), j.value().toUtf8().constData());
            }
	}
    }
}
