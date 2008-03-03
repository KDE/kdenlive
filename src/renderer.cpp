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
  email                : jb@ader.ch

***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

// ffmpeg Header files

extern "C" {
#include <ffmpeg/avformat.h>
}
#include <QTimer>
#include <QDir>
#include <QApplication>
#include <QPainter>

#include <KDebug>
#include <KStandardDirs>
#include <KMessageBox>
#include <KLocale>

#include "renderer.h"
#include "kdenlivesettings.h"

static void consumer_frame_show(mlt_consumer, Render * self, mlt_frame frame_ptr) {
    // detect if the producer has finished playing. Is there a better way to do it ?
    //if (self->isBlocked) return;
    if (mlt_properties_get_double(MLT_FRAME_PROPERTIES(frame_ptr), "_speed") == 0.0) {
        self->emitConsumerStopped();
    } else {
        self->emitFrameNumber(mlt_frame_get_position(frame_ptr));
    }
}

Render::Render(const QString & rendererName, int winid, int extid, QWidget *parent): QObject(parent), m_name(rendererName), m_mltConsumer(NULL), m_mltProducer(NULL), m_mltTextProducer(NULL), m_sceneList(QDomDocument()), m_winid(-1), m_framePosition(0), m_generateScenelist(false), m_isBlocked(true) {
    kDebug() << "//////////  USING PROFILE: " << qstrdup(KdenliveSettings::current_profile().toUtf8());
    m_mltProfile = new Mlt::Profile((char*) qstrdup(KdenliveSettings::current_profile().toUtf8()));
    refreshTimer = new QTimer(this);
    connect(refreshTimer, SIGNAL(timeout()), this, SLOT(refresh()));

    m_connectTimer = new QTimer(this);
    connect(m_connectTimer, SIGNAL(timeout()), this, SLOT(connectPlaylist()));

    if (rendererName == "project") m_monitorId = 10000;
    else m_monitorId = 10001;
    osdTimer = new QTimer(this);
    connect(osdTimer, SIGNAL(timeout()), this, SLOT(slotOsdTimeout()));

    m_osdProfile =   KStandardDirs::locate("data", "kdenlive/profiles/metadata.properties");
    //if (rendererName == "clip")
    {
        //Mlt::Consumer *consumer = new Mlt::Consumer( profile , "sdl_preview");
        m_mltConsumer = new Mlt::Consumer(*m_mltProfile , "sdl_preview"); //consumer;
        m_mltConsumer->set("resize", 1);
        m_mltConsumer->set("window_id", winid);
        m_mltConsumer->set("terminate_on_pause", 1);
        m_externalwinid = extid;
        m_winid = winid;
        m_mltConsumer->listen("consumer-frame-show", this, (mlt_listener) consumer_frame_show);
        Mlt::Producer *producer = new Mlt::Producer(*m_mltProfile , "westley-xml", "<westley><playlist><producer mlt_service=\"colour\" colour=\"blue\" /></playlist></westley>");
        m_mltProducer = producer;
        m_mltConsumer->connect(*m_mltProducer);
        m_mltProducer->set_speed(0.0);
        //m_mltConsumer->start();
        //refresh();
        //initSceneList();
    }
    /*m_osdInfo = new Mlt::Filter("data_show");
    char *tmp = decodedString(m_osdProfile);
    m_osdInfo->set("resource", tmp);
    delete[] tmp;*/
    //      Does it do anything usefull? I mean, RenderThread doesn't do anything useful at the moment
    //      (except being cpu hungry :)

    /*      if(!s_renderThread) {
    s_renderThread = new RenderThread;
    s_renderThread->start();
    } */
}

Render::~Render() {
    closeMlt();
}


void Render::closeMlt() {
    delete m_connectTimer;
    delete osdTimer;
    delete refreshTimer;
    if (m_mltConsumer)
        delete m_mltConsumer;
    if (m_mltProducer)
        delete m_mltProducer;
    //delete m_osdInfo;
}



int Render::resetRendererProfile(char * profile) {
    if (!m_mltConsumer) return 0;
    if (!m_mltConsumer->is_stopped()) m_mltConsumer->stop();
    m_mltConsumer->set("refresh", 0);
    //m_mltConsumer->set("profile", profile);
    kDebug() << " + + RESET CONSUMER WITH PROFILE: " << profile;
    mlt_properties properties = MLT_CONSUMER_PROPERTIES(m_mltConsumer->get_consumer());
//    apply_profile_properties( m_profile, m_mltConsumer->get_consumer(), properties );
    refresh();
    return 1;
}

/** Wraps the VEML command of the same name; Seeks the renderer clip to the given time. */
void Render::seek(GenTime time) {
    sendSeekCommand(time);
    //emit positionChanged(time);
}

//static
char *Render::decodedString(QString str) {
    /*QCString fn = QFile::encodeName(str);
    char *t = new char[fn.length() + 1];
    strcpy(t, (const char *)fn);*/

    return qstrdup(str.toUtf8().data());   //toLatin1
}

//static
QPixmap Render::frameThumbnail(Mlt::Frame *frame, int width, int height, bool border) {
    QPixmap pix(width, height);

    mlt_image_format format = mlt_image_rgb24a;
    uint8_t *thumb = frame->get_image(format, width, height);
    QImage image(thumb, width, height, QImage::Format_ARGB32);

    if (!image.isNull()) {
        pix = pix.fromImage(image);
        if (border) {
            QPainter painter(&pix);
            painter.drawRect(0, 0, width - 1, height - 1);
        }
    } else pix.fill(Qt::black);
    return pix;
}


QPixmap Render::extractFrame(int frame_position, int width, int height) {
    QPixmap pix(width, height);
    if (!m_mltProducer) {
        pix.fill(Qt::black);
        return pix;
    }
    Mlt::Producer *mlt_producer = m_mltProducer->cut(frame_position, frame_position + 1);
    Mlt::Filter m_convert(*m_mltProfile, "avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    mlt_producer->attach(m_convert);
    Mlt::Frame *frame = mlt_producer->get_frame();

    if (frame) {
        pix = frameThumbnail(frame, width, height);
        delete frame;
    } else pix.fill(Qt::black);
    delete mlt_producer;
    return pix;
}

QPixmap Render::getImageThumbnail(KUrl url, int width, int height) {
    QImage im;
    QPixmap pixmap;
    if (url.fileName().startsWith(".all.")) {  //  check for slideshow
        QString fileType = url.fileName().right(3);
        QStringList more;
        QStringList::Iterator it;

        QDir dir(url.directory());
        more = dir.entryList(QDir::Files);

        for (it = more.begin() ; it != more.end() ; ++it) {
            if ((*it).endsWith("." + fileType, Qt::CaseInsensitive)) {
                im.load(url.directory() + "/" + *it);
                break;
            }
        }
    } else im.load(url.path());
    //pixmap = im.scaled(width, height);
    return pixmap;
}

//static
QPixmap Render::getVideoThumbnail(char *profile, QString file, int frame_position, int width, int height) {
    QPixmap pix(width, height);
    char *tmp = decodedString(file);
    Mlt::Profile *prof = new Mlt::Profile(profile);
    Mlt::Producer m_producer(*prof, tmp);
    delete[] tmp;
    if (m_producer.is_blank()) {
        pix.fill(Qt::black);
        return pix;
    }

    Mlt::Filter m_convert(*prof, "avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    m_producer.seek(frame_position);
    Mlt::Frame * frame = m_producer.get_frame();
    if (frame) {
        pix = frameThumbnail(frame, width, height, true);
        delete frame;
    }
    if (prof) delete prof;
    return pix;
}

/*
void Render::getImage(KUrl url, int frame_position, QPoint size)
{
    char *tmp = decodedString(url.path());
    Mlt::Producer m_producer(tmp);
    delete[] tmp;
    if (m_producer.is_blank()) {
 return;
    }
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    m_producer.seek(frame_position);

    Mlt::Frame * frame = m_producer.get_frame();

    if (frame) {
 QPixmap pix = frameThumbnail(frame, size.x(), size.y(), true);
 delete frame;
 emit replyGetImage(url, frame_position, pix, size.x(), size.y());
    }
}*/

/* Create thumbnail for color */
/*void Render::getImage(int id, QString color, QPoint size)
{
    QPixmap pixmap(size.x() - 2, size.y() - 2);
    color = color.replace(0, 2, "#");
    color = color.left(7);
    pixmap.fill(QColor(color));
    QPixmap result(size.x(), size.y());
    result.fill(Qt::black);
    //copyBlt(&result, 1, 1, &pixmap, 0, 0, size.x() - 2, size.y() - 2);
    emit replyGetImage(id, result, size.x(), size.y());

}*/

/* Create thumbnail for image */
/*void Render::getImage(KUrl url, QPoint size)
{
    QImage im;
    QPixmap pixmap;
    if (url.fileName().startsWith(".all.")) {  //  check for slideshow
     QString fileType = url.fileName().right(3);
         QStringList more;
         QStringList::Iterator it;

            QDir dir( url.directory() );
            more = dir.entryList( QDir::Files );
            for ( it = more.begin() ; it != more.end() ; ++it ) {
                if ((*it).endsWith("."+fileType, Qt::CaseInsensitive)) {
   if (!im.load(url.directory() + "/" + *it))
       kDebug()<<"++ ERROR LOADIN IMAGE: "<<url.directory() + "/" + *it;
   break;
  }
     }
    }
    else im.load(url.path());

    //pixmap = im.smoothScale(size.x() - 2, size.y() - 2);
    QPixmap result(size.x(), size.y());
    result.fill(Qt::black);
    //copyBlt(&result, 1, 1, &pixmap, 0, 0, size.x() - 2, size.y() - 2);
    emit replyGetImage(url, 1, result, size.x(), size.y());
}*/


double Render::consumerRatio() const {
    if (!m_mltConsumer) return 1.0;
    return (m_mltConsumer->get_double("aspect_ratio_num") / m_mltConsumer->get_double("aspect_ratio_den"));
}


int Render::getLength() {

    if (m_mltProducer) {
        // kDebug()<<"//////  LENGTH: "<<mlt_producer_get_playtime(m_mltProducer->get_producer());
        return mlt_producer_get_playtime(m_mltProducer->get_producer());
    }
    return 0;
}

bool Render::isValid(KUrl url) {
    char *tmp = decodedString(url.path());
    Mlt::Producer producer(*m_mltProfile, tmp);
    delete[] tmp;
    if (producer.is_blank())
        return false;

    return true;
}

void Render::getFileProperties(const QDomElement &xml, int clipId) {
    int height = 40;
    int width = height * 16 / 9.0; //KdenliveSettings::displayratio();
    QDomDocument doc;
    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);
    westley.appendChild(doc.importNode(xml, true));
    kDebug() << "////////////\n" << doc.toString() << "////////////////\n";
    char *tmp = decodedString(doc.toString());

    Mlt::Producer producer(*m_mltProfile, "westley-xml", tmp);
    delete[] tmp;

    if (producer.is_blank()) {
        return;
    }
    int frameNumber = xml.attribute("frame_thumbnail", 0).toInt();
    if (frameNumber != 0) producer.seek(frameNumber);
    mlt_properties properties = MLT_PRODUCER_PROPERTIES(producer.get_producer());

    QMap < QString, QString > filePropertyMap;
    QMap < QString, QString > metadataPropertyMap;

    KUrl url = xml.attribute("resource", QString::null);
    filePropertyMap["filename"] = url.path();
    filePropertyMap["duration"] = QString::number(producer.get_playtime());
    kDebug() << "///////  PRODUCER: " << url.path() << " IS: " << producer.get_playtime();
    Mlt::Filter m_convert(*m_mltProfile, "avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    producer.attach(m_convert);
    Mlt::Frame * frame = producer.get_frame();

    filePropertyMap["fps"] =
        QString::number(mlt_producer_get_fps(producer.get_producer()));

    if (frame && frame->is_valid()) {
        filePropertyMap["width"] =
            QString::number(frame->get_int("width"));
        filePropertyMap["height"] =
            QString::number(frame->get_int("height"));
        filePropertyMap["frequency"] =
            QString::number(frame->get_int("frequency"));
        filePropertyMap["channels"] =
            QString::number(frame->get_int("channels"));

        if (frame->get_int("test_image") == 0) {
            if (url.path().endsWith(".westley") || url.path().endsWith(".kdenlive")) {
                filePropertyMap["type"] = "playlist";
                metadataPropertyMap["comment"] = QString::fromUtf8(mlt_properties_get(MLT_SERVICE_PROPERTIES(producer.get_service()), "title"));
            } else if (frame->get_int("test_audio") == 0)
                filePropertyMap["type"] = "av";
            else
                filePropertyMap["type"] = "video";

            // Generate thumbnail for this frame
            QPixmap pixmap = frameThumbnail(frame, width, height, true);

            emit replyGetImage(clipId, 0, pixmap, width, height);

        } else if (frame->get_int("test_audio") == 0) {
            QPixmap pixmap(KStandardDirs::locate("appdata", "graphics/music.png"));
            emit replyGetImage(clipId, 0, pixmap, width, height);
            filePropertyMap["type"] = "audio";
        }
    }

    // Retrieve audio / video codec name

    // Fetch the video_context
#if 0 //until the reason for the chrashs is found
    AVFormatContext *context = (AVFormatContext *) mlt_properties_get_data(properties, "video_context", NULL);
    if (context != NULL) {
        // Get the video_index
        int index = mlt_properties_get_int(properties, "video_index");
        if (context->streams && context->streams [index] && context->streams[ index ]->codec && context->streams[ index ]->codec->codec->name)
            filePropertyMap["videocodec"] = context->streams[ index ]->codec->codec->name;
    }
    context = (AVFormatContext *) mlt_properties_get_data(properties, "audio_context", NULL);
    if (context != NULL) {
        // Get the video_index
        int index = mlt_properties_get_int(properties, "audio_index");
        if (context->streams && context->streams [index] && context->streams[ index ]->codec && context->streams[ index ]->codec->codec->name)
            filePropertyMap["audiocodec"] = context->streams[ index ]->codec->codec->name;
    }
#endif


    // metadata

    mlt_properties metadata = mlt_properties_new();
    mlt_properties_pass(metadata, properties, "meta.attr.");
    int count = mlt_properties_count(metadata);
    for (int i = 0; i < count; i ++) {
        QString name = mlt_properties_get_name(metadata, i);
        QString value = QString::fromUtf8(mlt_properties_get_value(metadata, i));
        if (name.endsWith("markup") && !value.isEmpty())
            metadataPropertyMap[ name.section(".", 0, -2)] = value;
    }

    emit replyGetFileProperties(clipId, filePropertyMap, metadataPropertyMap);
    kDebug() << "REquested fuile info for: " << url.path();
    if (frame) delete frame;
}

QDomDocument Render::sceneList() const {
    return m_sceneList;
}

/** Create the producer from the Westley QDomDocument */
void Render::initSceneList() {
    kDebug() << "--------  INIT SCENE LIST ------_";
    QDomDocument doc;
    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);
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
    westley.appendChild(tractor);
    // kDebug()<<doc.toString();
    /*
       QString tmp = QString("<westley><producer resource=\"colour\" colour=\"red\" id=\"red\" /><tractor><multitrack><playlist></playlist><playlist></playlist><playlist /><playlist /><playlist></playlist></multitrack></tractor></westley>");*/
    setSceneList(doc, 0);
}


/** Create the producer from the Westley QDomDocument */
void Render::setSceneList(QDomDocument list, int position) {
    if (m_winid == -1) return;
    m_generateScenelist = true;

    kWarning() << "//////  RENDER, SET SCENE LIST";


    /*
        if (!clip.is_valid()) {
     kWarning()<<" ++++ WARNING, UNABLE TO CREATE MLT PRODUCER";
     m_generateScenelist = false;
     return;
        }*/

    if (m_mltConsumer) {
        m_mltConsumer->set("refresh", 0);
        if (!m_mltConsumer->is_stopped()) {
            //emitConsumerStopped();
            m_mltConsumer->stop();
        }
    }

    if (m_mltProducer) {
        m_mltProducer->set_speed(0.0);

        //if (KdenliveSettings::osdtimecode() && m_osdInfo) m_mltProducer->detach(*m_osdInfo);

        delete m_mltProducer;
        m_mltProducer = NULL;
        emit stopped();
    }

    char *tmp = decodedString(list.toString());
    //Mlt::Producer clip(profile, "westley-xml", tmp);

    m_mltProducer = new Mlt::Producer(*m_mltProfile, "westley-xml", tmp);
    delete[] tmp;
    //m_mltProducer->optimise();
    if (position != 0) m_mltProducer->seek(position);

    /*if (KdenliveSettings::osdtimecode()) {
    // Attach filter for on screen display of timecode
    delete m_osdInfo;
    QString attr = "attr_check";
    mlt_filter filter = mlt_factory_filter( "data_feed", (char*) attr.ascii() );
    mlt_properties_set_int( MLT_FILTER_PROPERTIES( filter ), "_fezzik", 1 );
    mlt_producer_attach( m_mltProducer->get_producer(), filter );
    mlt_filter_close( filter );

      m_osdInfo = new Mlt::Filter("data_show");
    tmp = decodedString(m_osdProfile);
      m_osdInfo->set("resource", tmp);
    delete[] tmp;
    mlt_properties properties = MLT_PRODUCER_PROPERTIES(m_mltProducer->get_producer());
    mlt_properties_set_int( properties, "meta.attr.timecode", 1);
    mlt_properties_set( properties, "meta.attr.timecode.markup", "#timecode#");
    m_osdInfo->set("dynamic", "1");

      if (m_mltProducer->attach(*m_osdInfo) == 1) kDebug()<<"////// error attaching filter";
    } else {
    m_osdInfo->set("dynamic", "0");
    }*/

    m_fps = m_mltProducer->get_fps();

    emit durationChanged(m_mltProducer->get_playtime());
    //m_connectTimer->start( 500 );
    connectPlaylist();
    m_generateScenelist = false;

}

const double Render::fps() const {
    return m_fps;
}

void Render::connectPlaylist() {

    m_connectTimer->stop();
    m_mltConsumer->connect(*m_mltProducer);
    m_mltProducer->set_speed(0.0);
    m_mltConsumer->start();
    //refresh();
    /*
     if (m_mltConsumer->start() == -1) {
          KMessageBox::error(qApp->activeWindow(), i18n("Could not create the video preview window.\nThere is something wrong with your Kdenlive install or your driver settings, please fix it."));
          m_mltConsumer = NULL;
     }
     else {
             refresh();
     }*/
}

void Render::refreshDisplay() {

    if (!m_mltProducer) return;
    m_mltConsumer->set("refresh", 0);

    mlt_properties properties = MLT_PRODUCER_PROPERTIES(m_mltProducer->get_producer());
    /*if (KdenliveSettings::osdtimecode()) {
        mlt_properties_set_int( properties, "meta.attr.timecode", 1);
        mlt_properties_set( properties, "meta.attr.timecode.markup", "#timecode#");
        m_osdInfo->set("dynamic", "1");
        m_mltProducer->attach(*m_osdInfo);
    }
    else {
        m_mltProducer->detach(*m_osdInfo);
        m_osdInfo->set("dynamic", "0");
    }*/
    refresh();
}

void Render::setVolume(double volume) {
    if (!m_mltConsumer || !m_mltProducer) return;
    /*osdTimer->stop();
    m_mltConsumer->set("refresh", 0);
    // Attach filter for on screen display of timecode
    mlt_properties properties = MLT_PRODUCER_PROPERTIES(m_mltProducer->get_producer());
    mlt_properties_set_double( properties, "meta.volume", volume );
    mlt_properties_set_int( properties, "meta.attr.osdvolume", 1);
    mlt_properties_set( properties, "meta.attr.osdvolume.markup", i18n("Volume: ") + QString::number(volume * 100));

    if (!KdenliveSettings::osdtimecode()) {
    m_mltProducer->detach(*m_osdInfo);
    mlt_properties_set_int( properties, "meta.attr.timecode", 0);
     if (m_mltProducer->attach(*m_osdInfo) == 1) kDebug()<<"////// error attaching filter";
    }*/
    refresh();
    osdTimer->setSingleShot(2500);
}

void Render::slotOsdTimeout() {
    mlt_properties properties = MLT_PRODUCER_PROPERTIES(m_mltProducer->get_producer());
    mlt_properties_set_int(properties, "meta.attr.osdvolume", 0);
    mlt_properties_set(properties, "meta.attr.osdvolume.markup", NULL);
    //if (!KdenliveSettings::osdtimecode()) m_mltProducer->detach(*m_osdInfo);
    refresh();
}

void Render::start() {
    kDebug() << "-----  STARTING MONITOR: " << m_name;
    if (m_winid == -1) {
        kDebug() << "-----  BROKEN MONITOR: " << m_name << ", RESTART";
        return;
    }

    if (m_mltConsumer->is_stopped()) {
        kDebug() << "-----  MONITOR: " << m_name << " WAS STOPPED";
        if (m_mltConsumer->start() == -1) {
            KMessageBox::error(qApp->activeWindow(), i18n("Could not create the video preview window.\nThere is something wrong with your Kdenlive install or your driver settings, please fix it."));
            m_mltConsumer = NULL;
            return;
        } else {
            kDebug() << "-----  MONITOR: " << m_name << " REFRESH";
            refresh();
        }
    }
    m_isBlocked = false;
}

void Render::clear() {
    if (m_mltConsumer) {
        m_mltConsumer->set("refresh", 0);
        if (!m_mltConsumer->is_stopped()) m_mltConsumer->stop();
    }

    if (m_mltProducer) {
        //if (KdenliveSettings::osdtimecode() && m_osdInfo) m_mltProducer->detach(*m_osdInfo);
        m_mltProducer->set_speed(0.0);
        delete m_mltProducer;
        m_mltProducer = NULL;
        emit stopped();
    }
}

void Render::stop() {
    if (m_mltConsumer && !m_mltConsumer->is_stopped()) {
        kDebug() << "/////////////   RENDER STOPPED: " << m_name;
        m_mltConsumer->set("refresh", 0);
        m_mltConsumer->stop();
    }
    kDebug() << "/////////////   RENDER STOP2-------";
    m_isBlocked = true;

    if (m_mltProducer) {
        m_mltProducer->set_speed(0.0);
        m_mltProducer->set("out", m_mltProducer->get_length() - 1);
    }
    kDebug() << "/////////////   RENDER STOP3-------";
}

void Render::stop(const GenTime & startTime) {
    kDebug() << "/////////////   RENDER STOP-------2";
    if (m_mltProducer) {
        m_mltProducer->set_speed(0.0);
        m_mltProducer->seek((int) startTime.frames(m_fps));
    }
    m_mltConsumer->purge();
}

void Render::switchPlay() {
    if (!m_mltProducer)
        return;
    if (m_mltProducer->get_speed() == 0.0) m_mltProducer->set_speed(1.0);
    else {
        m_mltProducer->set_speed(0.0);
        kDebug() << "// POSITON: " << m_framePosition;
        m_mltProducer->seek((int) m_framePosition);
    }

    /*if (speed == 0.0) {
    m_mltProducer->seek((int) m_framePosition + 1);
        m_mltConsumer->purge();
    }*/
    refresh();
}

void Render::play(double speed) {
    if (!m_mltProducer)
        return;
    if (speed == 0.0) m_mltProducer->set("out", m_mltProducer->get_length() - 1);
    m_mltProducer->set_speed(speed);
    /*if (speed == 0.0) {
    m_mltProducer->seek((int) m_framePosition + 1);
        m_mltConsumer->purge();
    }*/
    refresh();
}

void Render::play(double speed, const GenTime & startTime) {
    kDebug() << "/////////////   RENDER PLAY2-------" << speed;
    if (!m_mltProducer)
        return;
    //m_mltProducer->set("out", m_mltProducer->get_length() - 1);
    //if (speed == 0.0) m_mltConsumer->set("refresh", 0);
    m_mltProducer->set_speed(speed);
    m_mltProducer->seek((int)(startTime.frames(m_fps)));
    //m_mltConsumer->purge();
    //refresh();
}

void Render::play(double speed, const GenTime & startTime,
                  const GenTime & stopTime) {
    kDebug() << "/////////////   RENDER PLAY3-------" << speed;
    if (!m_mltProducer)
        return;
    m_mltProducer->set("out", stopTime.frames(m_fps));
    m_mltProducer->seek((int)(startTime.frames(m_fps)));
    m_mltConsumer->purge();
    m_mltProducer->set_speed(speed);
    refresh();
}

void Render::render(const KUrl & url) {
    QDomDocument doc;
    QDomElement elem = doc.createElement("render");
    elem.setAttribute("filename", url.path());
    doc.appendChild(elem);
}

void Render::sendSeekCommand(GenTime time) {
    if (!m_mltProducer)
        return;
    //kDebug()<<"//////////  KDENLIVE SEEK: "<<(int) (time.frames(m_fps));
    m_mltProducer->seek((int)(time.frames(m_fps)));
    refresh();
}

void Render::seekToFrame(int pos) {
    if (!m_mltProducer)
        return;
    //kDebug()<<"//////////  KDENLIVE SEEK: "<<(int) (time.frames(m_fps));
    m_mltProducer->seek(pos);
    refresh();
}

void Render::askForRefresh() {
    // Use a Timer so that we don't refresh too much
    refreshTimer->start(200);
}

void Render::doRefresh() {
    // Use a Timer so that we don't refresh too much
    refresh();
}

void Render::refresh() {
    if (!m_mltProducer || m_isBlocked)
        return;
    refreshTimer->stop();
    if (m_mltConsumer) {
        m_mltConsumer->set("refresh", 1);
    }
}

/** Sets the description of this renderer to desc. */
void Render::setDescription(const QString & description) {
    m_description = description;
}

/** Returns the description of this renderer */
QString Render::description() {
    return m_description;
}


double Render::playSpeed() {
    if (m_mltProducer) return m_mltProducer->get_speed();
    return 0.0;
}

const GenTime & Render::seekPosition() const {
    if (m_mltProducer) return GenTime((int) m_mltProducer->position(), m_fps);
    else return GenTime();
}


const QString & Render::rendererName() const {
    return m_name;
}


void Render::emitFrameNumber(double position) {
    //kDebug()<<"// POSITON: "<<m_framePosition;
    if (m_generateScenelist) return;
    m_framePosition = position;
    emit rendererPosition((int) position);
    //if (qApp->activeWindow()) QApplication::postEvent(qApp->activeWindow(), new PositionChangeEvent( GenTime((int) position, m_fps), m_monitorId));
}

void Render::emitConsumerStopped() {
    // This is used to know when the playing stopped
    if (m_mltProducer && !m_generateScenelist) {
        double pos = m_mltProducer->position();
        emit rendererStopped((int) pos);
        //if (qApp->activeWindow()) QApplication::postEvent(qApp->activeWindow(), new PositionChangeEvent(GenTime((int) pos, m_fps), m_monitorId + 100));
        //new QCustomEvent(10002));
    }
}



void Render::exportFileToFirewire(QString srcFileName, int port, GenTime startTime, GenTime endTime) {
    KMessageBox::sorry(0, i18n("Firewire is not enabled on your system.\n Please install Libiec61883 and recompile Kdenlive"));
}


void Render::exportCurrentFrame(KUrl url, bool notify) {
    if (!m_mltProducer) {
        KMessageBox::sorry(qApp->activeWindow(), i18n("There is no clip, cannot extract frame."));
        return;
    }

    int height = 1080;//KdenliveSettings::defaultheight();
    int width = 1940; //KdenliveSettings::displaywidth();

    QPixmap pix(width, height);
    Mlt::Filter m_convert(*m_mltProfile, "avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_mltProducer->attach(m_convert);
    Mlt::Frame * frame = m_mltProducer->get_frame();
    m_mltProducer->detach(m_convert);
    if (frame) {
        pix = frameThumbnail(frame, width, height);
        delete frame;
    }
    pix.save(url.path(), "PNG");
    //if (notify) QApplication::postEvent(qApp->activeWindow(), new UrlEvent(url, 10003));
}

/** MLT PLAYLIST DIRECT MANIPULATON  **/


void Render::mltCheckLength() {
    //kDebug()<<"checking track length: "<<track<<"..........";
    Mlt::Service service(m_mltProducer->get_service());
    Mlt::Tractor tractor(service);

    int trackNb = tractor.count();
    double duration = 0;
    double trackDuration;
    if (trackNb == 1) {
        Mlt::Producer trackProducer(tractor.track(0));
        Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
        duration = Mlt::Producer(trackPlaylist.get_producer()).get_playtime() - 1;
        m_mltProducer->set("out", duration);
        emit durationChanged(duration);
        return;
    }
    while (trackNb > 1) {
        Mlt::Producer trackProducer(tractor.track(trackNb - 1));
        Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
        trackDuration = Mlt::Producer(trackPlaylist.get_producer()).get_playtime() - 1;

        kDebug() << " / / /DURATON FOR TRACK " << trackNb - 1 << " = " << trackDuration;
        if (trackDuration > duration) duration = trackDuration;
        trackNb--;
    }

    Mlt::Producer blackTrackProducer(tractor.track(0));
    Mlt::Playlist blackTrackPlaylist((mlt_playlist) blackTrackProducer.get_service());
    double blackDuration = Mlt::Producer(blackTrackPlaylist.get_producer()).get_playtime() - 1;
    kDebug() << " / / /DURATON FOR TRACK 0 = " << blackDuration;
    if (blackDuration != duration) {
        blackTrackPlaylist.remove_region(0, blackDuration);
        int i = 0;
        int dur = duration;
        QDomDocument doc;
        QDomElement black = doc.createElement("producer");
        black.setAttribute("mlt_service", "colour");
        black.setAttribute("colour", "black");
        black.setAttribute("in", "0");
        black.setAttribute("out", "13999");
        while (dur > 14000) { // <producer mlt_service=\"colour\" colour=\"black\" in=\"0\" out=\"13999\" />
            mltInsertClip(0, GenTime(i * 14000, m_fps), black);
            dur = dur - 14000;
            i++;
        }
        black.setAttribute("out", QString::number(dur));
        mltInsertClip(0, GenTime(), black);

        m_mltProducer->set("out", duration);
        emit durationChanged(duration);
    }
}


void Render::mltInsertClip(int track, GenTime position, QDomElement element) {
    if (!m_mltProducer) {
        kDebug() << "PLAYLIST NOT INITIALISED //////";
        return;
    }
    Mlt::Producer parentProd(m_mltProducer->parent());
    if (parentProd.get_producer() == NULL) {
        kDebug() << "PLAYLIST BROKEN, CANNOT INSERT CLIP //////";
        return;
    }
    m_isBlocked = true;
    Mlt::Service service(parentProd.get_service());
    Mlt::Tractor tractor(service);

    QDomDocument doc;
    doc.appendChild(doc.importNode(element, true));
    QString resource = doc.toString();
    kDebug() << "///////  ADDING CLIP TMLNE: " << resource << " ON TRACK: " << track;
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    char *tmp = decodedString(resource);
    Mlt::Producer clip(*m_mltProfile, "westley-xml", tmp);
    //clip.set_in_and_out(in.frames(m_fps), out.frames(m_fps));
    delete[] tmp;

    trackPlaylist.insert_at(position.frames(m_fps), clip, 1);
    tractor.multitrack()->refresh();
    tractor.refresh();
    if (track != 0) mltCheckLength();
    m_isBlocked = false;
}

void Render::mltCutClip(int track, GenTime position) {
    m_isBlocked = true;
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    trackPlaylist.split_at(position.frames(m_fps));
    trackPlaylist.consolidate_blanks(0);
    kDebug() << "/ / / /CUTTING CLIP AT: " << position.frames(m_fps);
    m_isBlocked = false;
}


void Render::mltRemoveClip(int track, GenTime position) {
    m_isBlocked = true;
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    int clipIndex = trackPlaylist.get_clip_index_at(position.frames(m_fps));
    //trackPlaylist.remove(clipIndex);
    trackPlaylist.replace_with_blank(clipIndex);
    trackPlaylist.consolidate_blanks(0);
    if (track != 0) mltCheckLength();
    //emit durationChanged();
    m_isBlocked = false;
}

void Render::mltRemoveEffect(int track, GenTime position, QString index, bool doRefresh) {

    Mlt::Service service(m_mltProducer->parent().get_service());

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    //int clipIndex = trackPlaylist.get_clip_index_at(position.frames(m_fps));
    Mlt::Producer *clip = trackPlaylist.get_clip_at(position.frames(m_fps));
    if (!clip) {
        kDebug() << " / / / CANNOT FIND CLIP TO REMOVE EFFECT";
        return;
    }
    m_isBlocked = true;
    Mlt::Service clipService(clip->get_service());

//    if (tag.startsWith("ladspa")) tag = "ladspa";

    int ct = 0;
    Mlt::Filter *filter = clipService.filter(ct);
    while (filter) {
        if (index == "-1" || filter->get("kdenlive_ix") == index) {// && filter->get("kdenlive_id") == id) {
            clipService.detach(*filter);
            kDebug() << " / / / DLEETED EFFECT: " << ct;
        } else ct++;
        filter = clipService.filter(ct);
    }
    m_isBlocked = false;
    if (doRefresh) refresh();
}


void Render::mltAddEffect(int track, GenTime position, QMap <QString, QString> args, bool doRefresh) {
    Mlt::Service service(m_mltProducer->parent().get_service());
    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());

    Mlt::Producer *clip = trackPlaylist.get_clip_at(position.frames(m_fps));

    if (!clip) {
        kDebug() << "**********  CANNOT FIND CLIP TO APPLY EFFECT-----------";
        return;
    }
    Mlt::Service clipService(clip->get_service());
    m_isBlocked = true;
    // create filter
    QString tag = args.value("tag");
    //kDebug()<<" / / INSERTING EFFECT: "<<id;
    if (tag.startsWith("ladspa")) tag = "ladspa";
    char *filterId = decodedString(tag);
    Mlt::Filter *filter = new Mlt::Filter(*m_mltProfile, filterId);
    if (filter && filter->is_valid())
        filter->set("kdenlive_id", filterId);
    else {
        kDebug() << "filter is NULL";
        return;
    }

    QMap<QString, QString>::Iterator it;
    QString keyFrameNumber = "#0";

    for (it = args.begin(); it != args.end(); ++it) {
        //kDebug()<<" / / INSERTING EFFECT ARGS: "<<it.key()<<": "<<it.data();
        QString key;
        QString currentKeyFrameNumber;
        if (it.key().startsWith("#")) {
            currentKeyFrameNumber = it.key().section(":", 0, 0);
            if (currentKeyFrameNumber != keyFrameNumber) {
                // attach filter to the clip
                clipService.attach(*filter);
                filter = new Mlt::Filter(*m_mltProfile, filterId);
                filter->set("kdenlive_id", filterId);
                keyFrameNumber = currentKeyFrameNumber;
            }
            key = it.key().section(":", 1);
        } else key = it.key();
        char *name = decodedString(key);
        char *value = decodedString(it.value());
        filter->set(name, value);
        delete[] name;
        delete[] value;
    }
    // attach filter to the clip
    clipService.attach(*filter);
    delete[] filterId;
    m_isBlocked = false;
    if (doRefresh) refresh();

}

void Render::mltEditEffect(int track, GenTime position, QMap <QString, QString> args) {
    QString index = args.value("kdenlive_ix");
    QString tag =  args.value("tag");
    QMap<QString, QString>::Iterator it = args.begin();
    if (it.key().startsWith("#") || tag.startsWith("ladspa") || tag == "sox" || tag == "autotrack_rectangle") {
        // This is a keyframe effect, to edit it, we remove it and re-add it.
        mltRemoveEffect(track, position, index);
        mltAddEffect(track, position, args);
        return;
    }
    m_isBlocked = true;
    // create filter
    Mlt::Service service(m_mltProducer->parent().get_service());

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    //int clipIndex = trackPlaylist.get_clip_index_at(position.frames(m_fps));
    Mlt::Producer *clip = trackPlaylist.get_clip_at(position.frames(m_fps));
    if (!clip) {
        kDebug() << "WARINIG, CANNOT FIND CLIP ON track: " << track << ", AT POS: " << position.frames(m_fps);
        m_isBlocked = false;
        return;
    }

    Mlt::Service clipService(clip->get_service());

//    if (tag.startsWith("ladspa")) tag = "ladspa";

    int ct = 0;
    Mlt::Filter *filter = clipService.filter(ct);
    while (filter) {
        if (filter->get("kdenlive_ix") == index) {
            break;
        }
        ct++;
        filter = clipService.filter(ct);
    }


    if (!filter) {
        kDebug() << "WARINIG, FILTER FOR EDITING NOT FOUND, ADDING IT!!!!!";
        mltAddEffect(track, position, args);
        m_isBlocked = false;
        return;
    }

    for (it = args.begin(); it != args.end(); ++it) {
        kDebug() << " / / EDITING EFFECT ARGS: " << it.key() << ": " << it.value();
        char *name = decodedString(it.key());
        char *value = decodedString(it.value());
        filter->set(name, value);
        delete[] name;
        delete[] value;
    }
    m_isBlocked = false;
    refresh();
}

void Render::mltResizeClipEnd(int track, GenTime pos, GenTime in, GenTime out) {
    m_isBlocked = true;
    Mlt::Service service(m_mltProducer->parent().get_service());

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    if (trackPlaylist.is_blank_at(pos.frames(m_fps) + 1))
        kDebug() << "////////  ERROR RSIZING BLANK CLIP!!!!!!!!!!!";
    int clipIndex = trackPlaylist.get_clip_index_at(pos.frames(m_fps) + 1);

    int previousDuration = trackPlaylist.clip_length(clipIndex) - 1;
    int newDuration = out.frames(m_fps) - 1;

    kDebug() << " ** RESIZING CLIP END:" << clipIndex << " on track:" << track << ", mid pos: " << pos.frames(25) << ", in: " << in.frames(25) << ", out: " << out.frames(25) << ", PREVIOUS duration: " << previousDuration;
    trackPlaylist.resize_clip(clipIndex, in.frames(m_fps), newDuration);
    trackPlaylist.consolidate_blanks(0);
    if (previousDuration < newDuration) {
        // clip was made longer, trim next blank if there is one.
        if (trackPlaylist.is_blank(clipIndex + 1)) {
            trackPlaylist.split(clipIndex + 1, newDuration - previousDuration);
            trackPlaylist.remove(clipIndex + 1);
        }
    } else trackPlaylist.insert_blank(clipIndex + 1, previousDuration - newDuration - 1);

    trackPlaylist.consolidate_blanks(0);
    tractor.multitrack()->refresh();
    tractor.refresh();
    if (track != 0) mltCheckLength();
    m_isBlocked = false;
}

void Render::mltChangeTrackState(int track, bool mute, bool blind) {
    Mlt::Service service(m_mltProducer->parent().get_service());

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    if (mute) {
        if (blind) trackProducer.set("hide", 3);
        else trackProducer.set("hide", 2);
    } else if (blind) {
        trackProducer.set("hide", 1);
    } else {
        trackProducer.set("hide", 0);
    }
    tractor.multitrack()->refresh();
    tractor.refresh();
    refresh();
}

void Render::mltResizeClipStart(int track, GenTime pos, GenTime moveEnd, GenTime moveStart, GenTime in, GenTime out) {
    m_isBlocked = true;
    Mlt::Service service(m_mltProducer->parent().get_service());
    int moveFrame = (moveEnd - moveStart).frames(m_fps);

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    if (trackPlaylist.is_blank_at(pos.frames(m_fps) - 1))
        kDebug() << "////////  ERROR RSIZING BLANK CLIP!!!!!!!!!!!";
    int clipIndex = trackPlaylist.get_clip_index_at(pos.frames(m_fps) - 1);
    kDebug() << " ** RESIZING CLIP START:" << clipIndex << " on track:" << track << ", mid pos: " << pos.frames(25) << ", moving: " << moveFrame << ", in: " << in.frames(25) << ", out: " << out.frames(25);

    trackPlaylist.resize_clip(clipIndex, in.frames(m_fps), out.frames(m_fps));
    if (moveFrame > 0) trackPlaylist.insert_blank(clipIndex, moveFrame - 1);
    else {
        int midpos = moveStart.frames(m_fps) - 1; //+ (moveFrame / 2)
        int blankIndex = trackPlaylist.get_clip_index_at(midpos);
        int blankLength = trackPlaylist.clip_length(blankIndex);

        kDebug() << " + resizing blank: " << blankIndex << ", Mid: " << midpos << ", Length: " << blankLength << ", SIZE DIFF: " << moveFrame;


        if (blankLength + moveFrame == 0) trackPlaylist.remove(blankIndex);
        else trackPlaylist.resize_clip(blankIndex, 0, blankLength + moveFrame - 1);
    }
    trackPlaylist.consolidate_blanks(0);
    m_isBlocked = false;
    kDebug() << "-----------------\n" << "CLIP 0: " << trackPlaylist.clip_start(0) << ", LENGT: " << trackPlaylist.clip_length(0);
    kDebug() << "CLIP 1: " << trackPlaylist.clip_start(1) << ", LENGT: " << trackPlaylist.clip_length(1);
    kDebug() << "CLIP 2: " << trackPlaylist.clip_start(2) << ", LENGT: " << trackPlaylist.clip_length(2);
    kDebug() << "CLIP 3: " << trackPlaylist.clip_start(3) << ", LENGT: " << trackPlaylist.clip_length(3);
    kDebug() << "CLIP 4: " << trackPlaylist.clip_start(4) << ", LENGT: " << trackPlaylist.clip_length(4);
}

void Render::mltMoveClip(int startTrack, int endTrack, GenTime moveStart, GenTime moveEnd) {
    mltMoveClip(startTrack, endTrack, (int) moveStart.frames(m_fps), (int) moveEnd.frames(m_fps));
}


void Render::mltMoveClip(int startTrack, int endTrack, int moveStart, int moveEnd) {
    m_isBlocked = true;
    //m_mltConsumer->set("refresh", 0);
    Mlt::Service service(m_mltProducer->parent().get_service());

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(startTrack));
    Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
    int clipIndex = trackPlaylist.get_clip_index_at(moveStart + 1);
    mlt_field field = mlt_tractor_field(tractor.get_tractor());
    mlt_multitrack multitrack = mlt_field_multitrack(field); //mlt_tractor_multitrack(tractor.get_tractor());
    kDebug() << " --  CURRENT MULTIOTRACK HAS: " << mlt_multitrack_count(multitrack) << " tracks";;
    mlt_service multiprod = mlt_multitrack_service(multitrack);

    Mlt::Producer clipProducer(trackPlaylist.replace_with_blank(clipIndex));
    trackPlaylist.consolidate_blanks(0);
    mlt_events_block(MLT_PRODUCER_PROPERTIES(trackProducer.get_producer()), NULL);

    if (endTrack == startTrack) {
        if (!trackPlaylist.is_blank_at(moveEnd)) {
            kWarning() << "// ERROR, CLIP COLLISION----------";
            int ix = trackPlaylist.get_clip_index_at(moveEnd);
            kDebug() << "BAD CLIP STARTS AT: " << trackPlaylist.clip_start(ix) << ", LENGT: " << trackPlaylist.clip_length(ix);
        }
        trackPlaylist.insert_at(moveEnd, clipProducer, 1);
        trackPlaylist.consolidate_blanks(0);
    } else {
        trackPlaylist.consolidate_blanks(0);
        Mlt::Producer destTrackProducer(tractor.track(endTrack));
        Mlt::Playlist destTrackPlaylist((mlt_playlist) destTrackProducer.get_service());
        destTrackPlaylist.consolidate_blanks(1);
        destTrackPlaylist.insert_at(moveEnd, clipProducer, 1);
        destTrackPlaylist.consolidate_blanks(0);
    }

    mltCheckLength();
    mlt_events_unblock(MLT_PRODUCER_PROPERTIES(trackProducer.get_producer()), NULL);
    m_isBlocked = false;
    m_mltConsumer->set("refresh", 1);
}

void Render::mltMoveTransition(QString type, int startTrack, int trackOffset, GenTime oldIn, GenTime oldOut, GenTime newIn, GenTime newOut) {
    m_isBlocked = true;
    m_mltConsumer->set("refresh", 0);
    mlt_service serv = m_mltProducer->parent().get_service();

    mlt_service nextservice = mlt_service_get_producer(serv);
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString mlt_type = mlt_properties_get(properties, "mlt_type");
    QString resource = mlt_properties_get(properties, "mlt_service");
    int old_pos = (oldIn.frames(m_fps) + oldOut.frames(m_fps)) / 2;

    int new_in = newIn.frames(m_fps);
    int new_out = newOut.frames(m_fps) - 1;
    while (mlt_type == "transition") {
        mlt_transition tr = (mlt_transition) nextservice;
        int currentTrack = mlt_transition_get_b_track(tr);
        int currentIn = (int) mlt_transition_get_in(tr);
        int currentOut = (int) mlt_transition_get_out(tr);
        kDebug() << "// FOUND EXISTING TRANS, IN: " << currentIn << ", OUT: " << currentOut << ", TRACK: " << currentTrack;
        //kDebug()<<"// LOOKING FOR IN: "<<old_in<<", OUT: "<<old_out;
        kDebug() << "// OLD IN: " << oldIn.frames(m_fps) << " // OLD OUT: " << oldOut.frames(m_fps) << ", TRACK: " << startTrack << ", MID POS: " << old_pos;
        if (resource == type && startTrack == currentTrack && currentIn <= old_pos && currentOut >= old_pos) {
            mlt_transition_set_in_and_out(tr, new_in, new_out);
            if (trackOffset != 0) {
                mlt_properties properties = MLT_TRANSITION_PROPERTIES(tr);
                mlt_properties_set_int(properties, "a_track", mlt_transition_get_a_track(tr) + trackOffset);
                mlt_properties_set_int(properties, "b_track", mlt_transition_get_b_track(tr) + trackOffset);
            }
            break;
        }
        nextservice = mlt_service_producer(nextservice);
        properties = MLT_SERVICE_PROPERTIES(nextservice);
        mlt_type = mlt_properties_get(properties, "mlt_type");
        resource = mlt_properties_get(properties, "mlt_service");
    }
    m_isBlocked = false;
}

void Render::mltAddTransition(QString tag, int a_track, int b_track, GenTime in, GenTime out, QMap <QString, QString> args) {
    m_isBlocked = true;
    Mlt::Service service(m_mltProducer->parent().get_service());

    Mlt::Tractor tractor(service);
    Mlt::Field *field = tractor.field();
    char *transId = decodedString(tag);
    Mlt::Transition *transition = new Mlt::Transition(*m_mltProfile, transId);
    transition->set_in_and_out((int) in.frames(m_fps), (int) out.frames(m_fps));
    QMap<QString, QString>::Iterator it;
    QString key;

    kDebug() << " ------  ADDING TRANSITION PARAMs: " << args.count();

    for (it = args.begin(); it != args.end(); ++it) {
        key = it.key();
        char *name = decodedString(key);
        char *value = decodedString(it.value());
        transition->set(name, value);
        kDebug() << " ------  ADDING TRANS PARAM: " << name << ": " << value;
        //filter->set("kdenlive_id", id);
        delete[] name;
        delete[] value;
    }
    // attach filter to the clip
    field->plant_transition(*transition, a_track, b_track);
    delete[] transId;
    m_isBlocked = false;
    refresh();

}

void Render::mltSavePlaylist() {
    kWarning() << "// UPDATING PLAYLIST TO DISK++++++++++++++++";
    Mlt::Consumer *fileConsumer = new Mlt::Consumer(*m_mltProfile, "westley");
    fileConsumer->set("resource", "/home/one/playlist.xml");

    Mlt::Service service(m_mltProducer->get_service());
    Mlt::Tractor tractor(service);

    fileConsumer->connect(service);
    fileConsumer->start();

}

#include "renderer.moc"
