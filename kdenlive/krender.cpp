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

#include <iostream>

#include <mlt++/Mlt.h>

#include <qcolor.h>
#include <qpixmap.h>
#include <qxml.h>
#include <qapplication.h>
#include <qimage.h>
#include <qmutex.h>
#include <qevent.h>
#include <qtextstream.h>
#include <qstringlist.h>
#include <qdir.h>
#include <qcstring.h>

#include <kio/netaccess.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kdenlive.h>

#include "krender.h"

#include "effectparamdesc.h"
#include "initeffects.h"

static QMutex mutex (true);

KRender::KRender(const QString & rendererName, QWidget *parent, const char *name):QObject(parent, name), m_name(rendererName), m_mltConsumer(NULL), m_mltProducer(NULL), m_mltTextProducer(NULL), m_sceneList(QDomDocument()), m_winid(-1), m_framePosition(0), m_generateScenelist(false), isBlocked(true)
{
    refreshTimer = new QTimer( this );
    connect( refreshTimer, SIGNAL(timeout()), this, SLOT(refresh()) );

    m_connectTimer = new QTimer( this );
    connect( m_connectTimer, SIGNAL(timeout()), this, SLOT(connectPlaylist()) );

    if (rendererName == "Document") m_monitorId = 10000;
    else m_monitorId = 10001;
    osdTimer = new QTimer( this );
    connect( osdTimer, SIGNAL(timeout()), this, SLOT(slotOsdTimeout()) );

    m_osdProfile = locate("data", "kdenlive/profiles/metadata.properties");
    m_osdInfo = new Mlt::Filter("data_show");
    char *tmp = decodedString(m_osdProfile);
    m_osdInfo->set("resource", tmp);
    delete[] tmp;
    //      Does it do anything usefull? I mean, KRenderThread doesn't do anything useful at the moment
    //      (except being cpu hungry :)

    /*      if(!s_renderThread) {
    s_renderThread = new KRenderThread;
    s_renderThread->start();
    } */
}

KRender::~KRender()
{
    closeMlt();
}


void KRender::closeMlt()
{
    delete m_connectTimer;
    delete osdTimer;
    delete refreshTimer;
    if (m_mltConsumer)
        delete m_mltConsumer;
    if (m_mltProducer)
	delete m_mltProducer;
    delete m_osdInfo;
}

 

static void consumer_frame_show(mlt_consumer, KRender * self, mlt_frame frame_ptr)
{
    // detect if the producer has finished playing. Is there a better way to do it ?
    if (self->isBlocked) return;
    if (mlt_properties_get_double( MLT_FRAME_PROPERTIES( frame_ptr ), "_speed" ) == 0.0) {
        self->emitConsumerStopped();
    }
    else {
	self->emitFrameNumber(mlt_frame_get_position(frame_ptr));
    }
}

static void consumer_stopped(mlt_consumer, KRender * self, mlt_frame )
{
    //self->emitConsumerStopped();
}

void my_lock()
{
    //mutex.lock();
}
void my_unlock()
{
    //mutex.unlock();
}

/** Wraps the VEML command of the same name; requests that the renderer
should create a video window. If show is true, then the window should be
displayed, otherwise it should be hidden. KRender will emit the signal
replyCreateVideoXWindow() once the renderer has replied. */

void KRender::createVideoXWindow(WId winid, WId externalMonitor)
{
    if (m_mltConsumer) {
	delete m_mltConsumer;
    }

    m_mltConsumer = new Mlt::Consumer("sdl_preview");
    if (!m_mltConsumer || !m_mltConsumer->is_valid()) {
	KMessageBox::error(qApp->mainWidget(), i18n("Could not create the video preview window.\nThere is something wrong with your Kdenlive install.\n Exiting now..."));
	kdError()<<"Sorry, cannot create MLT consumer, check your MLT install you miss SDL libraries support in MLT"<<endl;
	exit(1);
    }

    //only as is saw, if we want to lock something with the sdl lock
    /*if (!KdenliveSettings::videoprofile().isEmpty()) 
	m_mltConsumer->set("profile", KdenliveSettings::videoprofile().ascii());*/
    kdDebug()<<" + + CREATING CONSUMER WITH PROFILE: "<<KdenliveSettings::videoprofile()<<endl;
    m_mltConsumer->set("profile", KdenliveSettings::videoprofile().ascii());
    /*m_mltConsumer->set("app_locked", 1);
    m_mltConsumer->set("app_lock", (void *) &my_lock, 0);
    m_mltConsumer->set("app_unlock", (void *) &my_unlock, 0);*/
    m_externalwinid = (int) externalMonitor;
    m_winid = (int) winid;
    if (KdenliveSettings::useexternalmonitor()) {
	m_mltConsumer->set("output_display", QString(":0." + QString::number(KdenliveSettings::externalmonitor())).ascii());
	m_mltConsumer->set("window_id", m_externalwinid);
    }
    else m_mltConsumer->set("window_id", m_winid);
    m_mltConsumer->set("resize", 1);
    m_mltConsumer->set("rescale", KdenliveSettings::previewquality().ascii());

    m_mltConsumer->set("terminate_on_pause", 1);
    QString aDevice = KdenliveSettings::audiodevice();
    if (!KdenliveSettings::videodriver().isEmpty()) m_mltConsumer->set("video_driver", KdenliveSettings::videodriver().ascii());
    if (!KdenliveSettings::audiodriver().isEmpty()) m_mltConsumer->set("audio_driver", KdenliveSettings::audiodriver().ascii());
    m_mltConsumer->set("audio_device", aDevice.section(";", 1).ascii());
    m_mltConsumer->set("progressive", 1);
    m_mltConsumer->set("audio_buffer", 1024);
    m_mltConsumer->set("frequency", 48000);

    m_mltConsumer->listen("consumer-frame-show", this, (mlt_listener) consumer_frame_show);

    //QTimer::singleShot(500, this, SLOT(initSceneList()));
    initSceneList();
//  m_mltConsumer->listen("consumer-stopped", this, (mlt_listener) consumer_stopped);
//  m_mltConsumer->set("buffer", 25);
}


int KRender::resetRendererProfile(char * profile)
{
    if (!m_mltConsumer) return 0;
    if (!m_mltConsumer->is_stopped()) m_mltConsumer->stop();
    m_mltConsumer->set("refresh", 0);
    m_mltConsumer->set("profile", profile);
    kdDebug()<<" + + RESET CONSUMER WITH PROFILE: "<<profile<<endl;
    m_fps = KdenliveSettings::defaultfps();
    m_mltConsumer->set("fps", m_fps);
    mlt_properties properties = MLT_CONSUMER_PROPERTIES( m_mltConsumer->get_consumer() );
    int result = mlt_consumer_profile( properties, profile );
    refresh();
    return result;
}

void KRender::restartConsumer()
{
    if (m_winid != -1) createVideoXWindow( m_winid, m_externalwinid);
}

/** Wraps the VEML command of the same name; Seeks the renderer clip to the given time. */
void KRender::seek(GenTime time)
{
    sendSeekCommand(time);
    //emit positionChanged(time);
}

//static
char *KRender::decodedString(QString str)
{
    QCString fn = QFile::encodeName(str);
    char *t = new char[fn.length() + 1];
    strcpy(t, (const char *)fn);
    return t;
}

QPixmap KRender::frameThumbnail(Mlt::Frame *frame, int width, int height, bool border)
{
    QPixmap pix(width, height);

    mlt_image_format format = mlt_image_rgb24a;
    if (border) {
	width = width -2;
	height = height -2;
    }
    uint8_t *thumb = frame->get_image(format, width, height);
    QImage image(thumb, width, height, 32, NULL, 0, QImage::IgnoreEndian);
    image.setAlphaBuffer( true );
    if (!image.isNull()) {
	if (!border) bitBlt(&pix, 0, 0, &image, 0, 0, width, height);
	else {
		pix.fill(black);
		bitBlt(&pix, 1, 1, &image, 0, 0, width, height);
	}
    }
    else pix.fill(black);
    return pix;
}


QPixmap KRender::extractFrame(int frame_position, int width, int height)
{
    QPixmap pix(width, height);
    if (!m_mltProducer) {
	pix.fill(black);
	return pix;
    }
    Mlt::Producer *mlt_producer = m_mltProducer->cut(frame_position, frame_position + 1);
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    mlt_producer->attach(m_convert);
    Mlt::Frame *frame = mlt_producer->get_frame();

    if (frame) {
	pix = frameThumbnail(frame, width, height);
	delete frame;
    }
    else pix.fill(black);
    delete mlt_producer;
    return pix;
}

QPixmap KRender::getImageThumbnail(KURL url, int width, int height)
{
    QImage im;
    QPixmap pixmap;
    if (url.filename().startsWith(".all.")) {  //  check for slideshow
	    QString fileType = url.filename().right(3);
    	    QStringList more;
    	    QStringList::Iterator it;

            QDir dir( url.directory() );
            more = dir.entryList( QDir::Files );
 
            for ( it = more.begin() ; it != more.end() ; ++it ) {
                if ((*it).endsWith("."+fileType, FALSE)) {
			im.load(url.directory() + "/" + *it);
			break;
		}
	    }
    }
    else im.load(url.path());
    pixmap = im.smoothScale(width, height);
    return pixmap;
}

QPixmap KRender::getVideoThumbnail(QString file, int frame_position, int width, int height)
{
    QPixmap pix(width, height);
    char *tmp = decodedString(file);
    Mlt::Producer m_producer(tmp);
    delete[] tmp;
    if (m_producer.is_blank()) {
	pix.fill(black);
	return pix;
    }

    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    m_producer.seek(frame_position);
    Mlt::Frame * frame = m_producer.get_frame();
    if (frame) {
	pix = frameThumbnail(frame, width, height, true);
	delete frame;
    }
    return pix;
}


void KRender::getImage(KURL url, int frame_position, QPoint size)
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
}

/* Create thumbnail for color */
void KRender::getImage(int id, QString color, QPoint size)
{
    QPixmap pixmap(size.x() - 2, size.y() - 2);
    color = color.replace(0, 2, "#");
    color = color.left(7);
    pixmap.fill(QColor(color));
    QPixmap result(size.x(), size.y());
    result.fill(Qt::black);
    copyBlt(&result, 1, 1, &pixmap, 0, 0, size.x() - 2, size.y() - 2);
    emit replyGetImage(id, result, size.x(), size.y());

}

/* Create thumbnail for image */
void KRender::getImage(KURL url, QPoint size)
{
    QImage im;
    QPixmap pixmap;
    if (url.filename().startsWith(".all.")) {  //  check for slideshow
	    QString fileType = url.filename().right(3);
    	    QStringList more;
    	    QStringList::Iterator it;

            QDir dir( url.directory() );
            more = dir.entryList( QDir::Files );
            for ( it = more.begin() ; it != more.end() ; ++it ) {
                if ((*it).endsWith("."+fileType, FALSE)) {
			if (!im.load(url.directory() + "/" + *it))
			    kdDebug()<<"++ ERROR LOADIN IMAGE: "<<url.directory() + "/" + *it<<endl;
			break;
		}
	    }
    }
    else im.load(url.path());

    pixmap = im.smoothScale(size.x() - 2, size.y() - 2);
    QPixmap result(size.x(), size.y());
    result.fill(Qt::black);
    copyBlt(&result, 1, 1, &pixmap, 0, 0, size.x() - 2, size.y() - 2);
    emit replyGetImage(url, 1, result, size.x(), size.y());
}


double KRender::consumerRatio() const
{
    if (!m_mltConsumer) return 1.0;
    return (m_mltConsumer->get_double("aspect_ratio_num")/m_mltConsumer->get_double("aspect_ratio_den"));
}


int KRender::getLength()
{

    if (m_mltProducer) 
    {
	kdDebug()<<"//////  LENGTH: "<<mlt_producer_get_playtime(m_mltProducer->get_producer())<<endl;
	return mlt_producer_get_playtime(m_mltProducer->get_producer());
    }
    return 0;
}

bool KRender::isValid(KURL url)
{
    char *tmp = decodedString(url.path());
    Mlt::Producer producer(tmp);
    delete[] tmp;
    if (producer.is_blank())
	return false;

    return true;
}


void KRender::getFileProperties(KURL url, uint framenb)
{
        int height = 40;
        int width = height * KdenliveSettings::displayratio();
	char *tmp = decodedString(url.path());
	Mlt::Producer producer(tmp);
	delete[] tmp;
    	if (producer.is_blank()) {
	    return;
    	}
	producer.seek( framenb );
	mlt_properties properties = MLT_PRODUCER_PROPERTIES( producer.get_producer() );

	QMap < QString, QString > filePropertyMap;
        QMap < QString, QString > metadataPropertyMap;

	filePropertyMap["filename"] = url.path();
	filePropertyMap["duration"] = QString::number(producer.get_playtime());
        
        Mlt::Filter m_convert("avcolour_space");
        m_convert.set("forced", mlt_image_rgb24a);
        producer.attach(m_convert);

	Mlt::Frame * frame = producer.get_frame();

	if (frame->is_valid()) {
	    filePropertyMap["fps"] =
		QString::number(mlt_producer_get_fps( producer.get_producer() ));
	    filePropertyMap["width"] =
		QString::number(frame->get_int("width"));
	    filePropertyMap["height"] =
		QString::number(frame->get_int("height"));
	    filePropertyMap["frequency"] =
		QString::number(frame->get_int("frequency"));
	    filePropertyMap["channels"] =
		QString::number(frame->get_int("channels"));

	// Retrieve audio / video codec name

	// Fetch the video_context
	AVFormatContext *context = (AVFormatContext *) mlt_properties_get_data( properties, "video_context", NULL );
	if (context != NULL) {
		// Get the video_index
		int index = mlt_properties_get_int( properties, "video_index" );
		filePropertyMap["videocodec"] = context->streams[ index ]->codec->codec->name;
	}
	context = (AVFormatContext *) mlt_properties_get_data( properties, "audio_context", NULL );
	if (context != NULL) {
		// Get the video_index
		int index = mlt_properties_get_int( properties, "audio_index" );
		filePropertyMap["audiocodec"] = context->streams[ index ]->codec->codec->name;
	}



	    // metadata

	    mlt_properties metadata = mlt_properties_new( );
	    mlt_properties_pass( metadata, properties, "meta.attr." );
	    int count = mlt_properties_count( metadata );
	    for ( int i = 0; i < count; i ++ )
	    {
		QString name = mlt_properties_get_name( metadata, i );
		QString value = QString::fromUtf8(mlt_properties_get_value( metadata, i ));
		if (name.endsWith("markup") && !value.isEmpty())
			metadataPropertyMap[ name.section(".", 0, -2) ] = value;
	    }

	    if (frame->get_int("test_image") == 0) {
		if (url.path().endsWith(".westley") || url.path().endsWith(".kdenlive")) {
		    filePropertyMap["type"] = "playlist";
		    metadataPropertyMap["comment"] = QString::fromUtf8(mlt_properties_get( MLT_SERVICE_PROPERTIES( producer.get_service() ), "title"));
		}
		else if (frame->get_int("test_audio") == 0)
		    filePropertyMap["type"] = "av";
		else
		    filePropertyMap["type"] = "video";

                // Generate thumbnail for this frame
		QPixmap pixmap = frameThumbnail(frame, width, height, true);

                emit replyGetImage(url, 0, pixmap, width, height);

	    } else if (frame->get_int("test_audio") == 0) {
                QPixmap pixmap(locate("appdata", "graphics/music.png"));
                emit replyGetImage(url, 0, pixmap, width, height);
		filePropertyMap["type"] = "audio";
            }
	}
	emit replyGetFileProperties(filePropertyMap, metadataPropertyMap);
	delete frame;
}

QDomDocument KRender::sceneList() const
{
    return m_sceneList;
}

/** Create the producer from the Westley QDomDocument */
void KRender::initSceneList()
{
    kdDebug()<<"--------  INIT SCENE LIST ------_"<<endl;
    QDomDocument doc;
    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);
    QDomElement prod = doc.createElement("producer");
    prod.setAttribute("resource", "colour");
    prod.setAttribute("colour", "black");
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
    kdDebug()<<doc.toString()<<endl;
/*
   QString tmp = QString("<westley><producer resource=\"colour\" colour=\"red\" id=\"red\" /><tractor><multitrack><playlist></playlist><playlist></playlist><playlist /><playlist /><playlist></playlist></multitrack></tractor></westley>");*/
    setSceneList(doc, 0);
}


/** Create the producer from the Westley QDomDocument */
void KRender::setSceneList(QDomDocument list, int position)
{
    if (!m_winid == -1) return;
    m_generateScenelist = true;

    kdWarning()<<"//////  RENDER, SET SCENE LIST"<<endl;

    Mlt::Playlist track;
    char *tmp = decodedString(list.toString());
    Mlt::Producer clip("westley-xml", tmp);
    delete[] tmp;

    if (!clip.is_valid()) {
	kdWarning()<<" ++++ WARNING, UNABLE TO CREATE MLT PRODUCER"<<endl;
	m_generateScenelist = false;
	return;
    }

    track.append(clip);

    if (m_mltConsumer) {
	m_mltConsumer->set("refresh", 0);
	if (!m_mltConsumer->is_stopped()) {
	//emitConsumerStopped();
	m_mltConsumer->stop();
	}
    }

    if (m_mltProducer) {
	m_mltProducer->set_speed(0.0);

	if (KdenliveSettings::osdtimecode() && m_osdInfo) m_mltProducer->detach(*m_osdInfo);
	//mlt_producer_clear(m_mltProducer->get_producer());
	delete m_mltProducer;
	m_mltProducer = NULL;
	emit stopped();
    }

    m_mltProducer = new Mlt::Producer(clip); //track.current();
    m_mltProducer->optimise();
    if (position != 0) m_mltProducer->seek(position);

    if (KdenliveSettings::osdtimecode()) {
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

    		if (m_mltProducer->attach(*m_osdInfo) == 1) kdDebug()<<"////// error attaching filter"<<endl;
	} else {
		m_osdInfo->set("dynamic", "0");
	}

	m_fps = m_mltProducer->get_fps();
        if (!m_mltConsumer) {
	    restartConsumer();
        }

	m_connectTimer->start(100, TRUE);
	m_generateScenelist = false;
  
}


void KRender::connectPlaylist() {
	if (m_mltConsumer->start() == -1) {
	    	KMessageBox::error(qApp->mainWidget(), i18n("Could not create the video preview window.\nThere is something wrong with your Kdenlive install or your driver settings, please fix it."));
	    	m_mltConsumer = NULL;
	}
	else {
    	    m_mltConsumer->connect(*m_mltProducer);
	    m_mltProducer->set_speed(0.0);
    	    refresh();
	}
}

void KRender::refreshDisplay() {
	if (!m_mltProducer) return;
	//m_mltConsumer->set("refresh", 0);
	//start();
	mlt_properties properties = MLT_PRODUCER_PROPERTIES(m_mltProducer->get_producer());
	if (KdenliveSettings::osdtimecode()) {
	    mlt_properties_set_int( properties, "meta.attr.timecode", 1);
	    mlt_properties_set( properties, "meta.attr.timecode.markup", "#timecode#");
	    m_osdInfo->set("dynamic", "1");
	    m_mltProducer->attach(*m_osdInfo);
	}
	else {
	    m_mltProducer->detach(*m_osdInfo);
	    m_osdInfo->set("dynamic", "0");
	}
	refresh();
}

void KRender::setVolume(double volume)
{
    if (!m_mltConsumer || !m_mltProducer) return;
    osdTimer->stop();
    // Attach filter for on screen display of timecode
    mlt_properties properties = MLT_PRODUCER_PROPERTIES(m_mltProducer->get_producer());
    mlt_properties_set_double( properties, "meta.volume", volume );
    mlt_properties_set_int( properties, "meta.attr.osdvolume", 1);
    mlt_properties_set( properties, "meta.attr.osdvolume.markup", i18n("Volume: ") + QString::number(volume * 100));

    if (!KdenliveSettings::osdtimecode()) {
	m_mltProducer->detach(*m_osdInfo);
	mlt_properties_set_int( properties, "meta.attr.timecode", 0);
    	if (m_mltProducer->attach(*m_osdInfo) == 1) kdDebug()<<"////// error attaching filter"<<endl;
    }
    refresh();
    osdTimer->start(2500, TRUE);
}

void KRender::slotOsdTimeout()
{
    mlt_properties properties = MLT_PRODUCER_PROPERTIES(m_mltProducer->get_producer());
    mlt_properties_set_int(properties, "meta.attr.osdvolume", 0);
    mlt_properties_set(properties, "meta.attr.osdvolume.markup", NULL);
    if (!KdenliveSettings::osdtimecode()) m_mltProducer->detach(*m_osdInfo);
    refresh();
}

void KRender::start()
{
    if (!m_mltConsumer || m_winid == -1) {
	restartConsumer();
	return;
    }

    if (m_mltConsumer->is_stopped()) {
	if (m_mltConsumer->start() == -1) {
	    KMessageBox::error(qApp->mainWidget(), i18n("Could not create the video preview window.\nThere is something wrong with your Kdenlive install or your driver settings, please fix it."));
	    m_mltConsumer = NULL;
	    return;
	}
    	else {
		refresh();
	}
    }
    isBlocked = false;
}

void KRender::clear()
{
    if (m_mltConsumer) {
	m_mltConsumer->set("refresh", 0);
	if (!m_mltConsumer->is_stopped()) m_mltConsumer->stop();
    }

    if (m_mltProducer) {
	if (KdenliveSettings::osdtimecode() && m_osdInfo) m_mltProducer->detach(*m_osdInfo);
	m_mltProducer->set_speed(0.0);
	delete m_mltProducer;
	m_mltProducer = NULL;
	emit stopped();
    }
}

void KRender::stop()
{
    if (m_mltConsumer && !m_mltConsumer->is_stopped()) {
	m_mltConsumer->set("refresh", 0);
	m_mltConsumer->stop();
    }

    isBlocked = true;

    if (m_mltProducer) {
	m_mltProducer->set_speed(0.0);
	m_mltProducer->set("out", m_mltProducer->get_length() - 1);
    }
}

void KRender::stop(const GenTime & startTime)
{
    if (m_mltProducer) {
	m_mltProducer->set_speed(0.0);
	m_mltProducer->seek((int) startTime.frames(m_fps));
    }
    m_mltConsumer->purge();
}


void KRender::play(double speed)
{
    if (!m_mltProducer)
	return;
    // m_mltProducer->set("out", m_mltProducer->get_length() - 1);
    m_mltProducer->set_speed(speed);
    /*if (speed == 0.0) {
	m_mltProducer->seek((int) m_framePosition + 1);
        m_mltConsumer->purge();
    }*/
    refresh();
}

void KRender::play(double speed, const GenTime & startTime)
{
    if (!m_mltProducer)
	return;
    // m_mltProducer->set("out", m_mltProducer->get_length() - 1);
    m_mltProducer->set_speed(speed);
    m_mltProducer->seek((int) (startTime.frames(m_fps)));
    m_mltConsumer->purge();
    refresh();
}

void KRender::play(double speed, const GenTime & startTime,
    const GenTime & stopTime)
{
    if (!m_mltProducer)
	return;
    m_mltProducer->set("out", stopTime.frames(m_fps));
    m_mltProducer->seek((int) (startTime.frames(m_fps)));
    m_mltConsumer->purge();
    m_mltProducer->set_speed(speed);
    refresh();
}

void KRender::render(const KURL & url)
{
    QDomDocument doc;
    QDomElement elem = doc.createElement("render");
    elem.setAttribute("filename", url.path());
    doc.appendChild(elem);
}

void KRender::sendSeekCommand(GenTime time)
{
    if (!m_mltProducer)
	return;
    //kdDebug()<<"//////////  KDENLIVE SEEK: "<<(int) (time.frames(m_fps))<<endl;
    m_mltProducer->seek((int) (time.frames(m_fps)));
    refresh();
}

void KRender::askForRefresh()
{
    // Use a Timer so that we don't refresh too much
    refreshTimer->start(200, TRUE);
}

void KRender::refresh()
{
    if (!m_mltProducer)
	return;
    refreshTimer->stop();
    if (m_mltConsumer) {
	m_mltConsumer->set("refresh", 1);
    }
}

/** Sets the description of this renderer to desc. */
void KRender::setDescription(const QString & description)
{
    m_description = description;
}

/** Returns the description of this renderer */
QString KRender::description()
{
    return m_description;
}


double KRender::playSpeed()
{
    if (m_mltProducer) return m_mltProducer->get_speed();
    return 0.0;
}

const GenTime & KRender::seekPosition() const
{
    if (m_mltProducer) return GenTime((int) m_mltProducer->position(), m_fps);
    else return GenTime();
}


const QString & KRender::rendererName() const
{
    return m_name;
}


void KRender::emitFrameNumber(double position)
{
	if (m_generateScenelist) return;
	m_framePosition = position;
        if (qApp->mainWidget()) QApplication::postEvent(qApp->mainWidget(), new PositionChangeEvent( GenTime((int) position, m_fps), m_monitorId));
}

void KRender::emitConsumerStopped()
{
    // This is used to know when the playing stopped
    if (m_mltProducer && !m_generateScenelist) {
	double pos = m_mltProducer->position();
        if (qApp->mainWidget()) QApplication::postEvent(qApp->mainWidget(), new PositionChangeEvent(GenTime((int) pos, m_fps), m_monitorId + 100));
	//new QCustomEvent(10002));
    }
}



void KRender::exportFileToFirewire(QString srcFileName, int port, GenTime startTime, GenTime endTime)
{
KMessageBox::sorry(0, i18n("Firewire is not enabled on your system.\n Please install Libiec61883 and recompile Kdenlive"));
}


void KRender::exportCurrentFrame(KURL url, bool notify) {
    if (!m_mltProducer) {
	KMessageBox::sorry(qApp->mainWidget(), i18n("There is no clip, cannot extract frame."));
	return;
    }

    int height = KdenliveSettings::defaultheight();
    int width = KdenliveSettings::displaywidth();

    QPixmap pix(width, height);

    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_mltProducer->attach(m_convert);
    Mlt::Frame * frame = m_mltProducer->get_frame();
    m_mltProducer->detach(m_convert);
    if (frame) {
	pix = frameThumbnail(frame, width, height);
	delete frame;
    }
    pix.save(url.path(), "PNG");
    if (notify) QApplication::postEvent(qApp->mainWidget(), new UrlEvent(url, 10003));
}

/**	MLT PLAYLIST DIRECT MANIPULATON		**/


void KRender::mltCheckLength()
{
    //kdDebug()<<"checking track length: "<<track<<".........."<<endl;
    Mlt::Service service(m_mltProducer->get_service());
    Mlt::Tractor tractor(service);

    int trackNb = tractor.count( );
    double duration = 0;
    double trackDuration;
    if (trackNb == 1) {
        Mlt::Producer trackProducer(tractor.track(0));
        Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());
        duration = Mlt::Producer(trackPlaylist.get_producer()).get_playtime() - 1;
	m_mltProducer->set("out", duration);
	emit durationChanged();
	return;
    }
    while (trackNb > 1) {
        Mlt::Producer trackProducer(tractor.track(trackNb - 1));
        Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());
        trackDuration = Mlt::Producer(trackPlaylist.get_producer()).get_playtime() - 1;

	kdDebug()<<" / / /DURATON FOR TRACK "<<trackNb - 1<<" = "<<trackDuration<<endl;
	if (trackDuration > duration) duration = trackDuration;
	trackNb--;
    }

    Mlt::Producer blackTrackProducer(tractor.track(0));
    Mlt::Playlist blackTrackPlaylist(( mlt_playlist ) blackTrackProducer.get_service());
    double blackDuration = Mlt::Producer(blackTrackPlaylist.get_producer()).get_playtime() - 1;
	kdDebug()<<" / / /DURATON FOR TRACK 0 = "<<blackDuration<<endl;
    if (blackDuration != duration) {
	blackTrackPlaylist.remove_region( 0, blackDuration );
	int i = 0;
	int dur = duration;
	
        while (dur > 14000) { // <producer mlt_service=\"colour\" colour=\"black\" in=\"0\" out=\"13999\" />
	    mltInsertClip(0, GenTime(i * 14000, m_fps), QString("<westley><producer mlt_service=\"colour\" colour=\"black\" in=\"0\" out=\"13999\" /></westley>"));
	    dur = dur - 14000;
	    i++;
        }

	mltInsertClip(0, GenTime(), QString("<westley><producer mlt_service=\"colour\" colour=\"black\" in=\"0\" out=\"" + QString::number(dur) + "\" /></westley>"));

	m_mltProducer->set("out", duration);
	emit durationChanged();
    }
}


void KRender::mltInsertClip(int track, GenTime position, QString resource)
{
    if (!m_mltProducer) {
	kdDebug()<<"PLAYLIST NOT INITIALISED //////"<<endl;
	return;
    }
    Mlt::Producer parentProd(m_mltProducer->parent());
    if (parentProd.get_producer() == NULL) {
	kdDebug()<<"PLAYLIST BROKEN, CANNOT INSERT CLIP //////"<<endl;
	return;
    }
    Mlt::Service service(parentProd.get_service());
    Mlt::Tractor tractor(service);

    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());
    char *tmp = decodedString(resource);
    Mlt::Producer clip("westley-xml", tmp);
    //clip.set_in_and_out(in.frames(m_fps), out.frames(m_fps));
    delete[] tmp;

    trackPlaylist.insert_at(position.frames(m_fps), clip, 1);
    tractor.multitrack()->refresh();
    tractor.refresh();
    if (track != 0) mltCheckLength();
    double duration = Mlt::Producer(trackPlaylist.get_producer()).get_playtime();
    kdDebug()<<"// +  +INSERTING CLIP: "<<resource<<" AT: "<<position.frames(m_fps)<<" on track: "<<track<<", DURATION: "<<duration<<endl;


}

void KRender::mltCutClip(int track, GenTime position)
{
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() == playlist_type) kdDebug()<<"// PLAYLIST TYPE"<<endl;
    if (service.type() == tractor_type) kdDebug()<<"// TRACOT TYPE"<<endl;
    if (service.type() == multitrack_type) kdDebug()<<"// MULTITRACK TYPE"<<endl;
    if (service.type() == producer_type) kdDebug()<<"// PROD TYPE"<<endl;

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());
    trackPlaylist.split_at(position.frames(m_fps));
    trackPlaylist.consolidate_blanks(0);
    kdDebug()<<"/ / / /CUTTING CLIP AT: "<<position.frames(m_fps)<<endl;
}


void KRender::mltRemoveClip(int track, GenTime position)
{
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() == playlist_type) kdDebug()<<"// PLAYLIST TYPE"<<endl;
    if (service.type() == tractor_type) kdDebug()<<"// TRACOT TYPE"<<endl;
    if (service.type() == multitrack_type) kdDebug()<<"// MULTITRACK TYPE"<<endl;
    if (service.type() == producer_type) kdDebug()<<"// PROD TYPE"<<endl;

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());
    int clipIndex = trackPlaylist.get_clip_index_at(position.frames(m_fps));
    //trackPlaylist.remove(clipIndex);
    trackPlaylist.replace_with_blank(clipIndex);
    trackPlaylist.consolidate_blanks(0);
    if (track != 0) mltCheckLength();
    //emit durationChanged();
}

void KRender::mltRemoveEffect(int track, GenTime position, QString id, QString tag, int index)
{
    Mlt::Service service(m_mltProducer->parent().get_service());

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());
    //int clipIndex = trackPlaylist.get_clip_index_at(position.frames(m_fps));
    Mlt::Producer *clip = trackPlaylist.get_clip_at(position.frames(m_fps));
    if (!clip) {
	kdDebug()<<" / / / CANNOT FIND CLIP TO REMOVE EFFECT"<<endl;
	return;
    }
    Mlt::Service clipService(clip->get_service());

    if (tag.startsWith("ladspa")) tag = "ladspa";

    if (index == -1) {
	int ct = 0;
	Mlt::Filter *filter = clipService.filter( ct );
	while (filter) {
	    if (filter->get("mlt_service") == tag && filter->get("kdenlive_id") == id) {
		clipService.detach(*filter);
		kdDebug()<<" / / / DLEETED EFFECT: "<<ct<<endl;
	    }
	    else ct++;
	    filter = clipService.filter( ct );
	}
    }
    else {
        Mlt::Filter *filter = clipService.filter( index );
        if (filter && filter->get("mlt_service") == tag && filter->get("kdenlive_id") == id) clipService.detach(*filter);
        else {
	    kdDebug()<<"WARINIG, FILTER "<<id<<" NOT FOUND!!!!!"<<endl;
        }
    }
    refresh();
}


void KRender::mltAddEffect(int track, GenTime position, QString id, QString tag, QMap <QString, QString> args)
{
    Mlt::Service service(m_mltProducer->parent().get_service());

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());

    Mlt::Producer *clip = trackPlaylist.get_clip_at(position.frames(m_fps));

    if (!clip) {
	kdDebug()<<"**********  CANNOT FIND CLIP TO APPLY EFFECT-----------"<<endl;
	return;
    }
    Mlt::Service clipService(clip->get_service());

    // create filter
    kdDebug()<<" / / INSERTING EFFECT: "<<id<<endl;
    if (tag.startsWith("ladspa")) tag = "ladspa";
    char *filterId = decodedString(tag);
    Mlt::Filter *filter = new Mlt::Filter(filterId);
    filter->set("kdenlive_id", id);

    QMap<QString, QString>::Iterator it;
    QString keyFrameNumber = "#0";

    for ( it = args.begin(); it != args.end(); ++it ) {
    kdDebug()<<" / / INSERTING EFFECT ARGS: "<<it.key()<<": "<<it.data()<<endl;
	QString key;
	QString currentKeyFrameNumber;
	if (it.key().startsWith("#")) {
	    currentKeyFrameNumber = it.key().section(":", 0, 0);
	    if (currentKeyFrameNumber != keyFrameNumber) {
    		// attach filter to the clip
		clipService.attach(*filter);
	        filter = new Mlt::Filter(filterId);
		keyFrameNumber = currentKeyFrameNumber;
	    }
	    key = it.key().section(":", 1);
	}
	else key = it.key();
        char *name = decodedString(key);
        char *value = decodedString(it.data());
        filter->set(name, value);
	delete[] name;
	delete[] value;
    }
    // attach filter to the clip
    clipService.attach(*filter);
    delete[] filterId;
    refresh();

}

void KRender::mltEditEffect(int track, GenTime position, int index, QString id, QString tag, QMap <QString, QString> args)
{
    QMap<QString, QString>::Iterator it = args.begin();
    if (it.key().startsWith("#") || tag.startsWith("ladspa") || tag == "sox" || tag == "autotrack_rectangle") {
	// This is a keyframe effect, to edit it, we remove it and re-add it.
	mltRemoveEffect(track, position, id, tag, -1);
	mltAddEffect(track, position, id, tag, args);
	return;
    }

    // create filter
    Mlt::Service service(m_mltProducer->parent().get_service());

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());
    //int clipIndex = trackPlaylist.get_clip_index_at(position.frames(m_fps));
    Mlt::Producer *clip = trackPlaylist.get_clip_at(position.frames(m_fps));
    Mlt::Service clipService(clip->get_service());
    Mlt::Filter *filter = clipService.filter( index );


    if (!filter || filter->get("mlt_service") != tag) {
	kdDebug()<<"WARINIG, FILTER NOT FOUND!!!!!"<<endl;
	int index = 0;
	filter = clipService.filter( index );
	while (filter) {
	    if (filter->get("mlt_service") == tag && filter->get("kdenlive_id") == id) break;
	    index++;
	    filter = clipService.filter( index );
	}
    }
    if (!filter) {
	kdDebug()<<"WARINIG, FILTER "<<id<<" NOT FOUND!!!!!"<<endl;
	return;
    }

    for ( it = args.begin(); it != args.end(); ++it ) {
    kdDebug()<<" / / INSERTING EFFECT ARGS: "<<it.key()<<": "<<it.data()<<endl;
        char *name = decodedString(it.key());
        char *value = decodedString(it.data());
        filter->set(name, value);
	delete[] name;
	delete[] value;
    }
    refresh();
}

void KRender::mltResizeClipEnd(int track, GenTime pos, GenTime in, GenTime out)
{
    Mlt::Service service(m_mltProducer->parent().get_service());

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());
    if (trackPlaylist.is_blank_at(pos.frames(m_fps) + 1)) 
	kdDebug()<<"////////  ERROR RSIZING BLANK CLIP!!!!!!!!!!!"<<endl;
    int clipIndex = trackPlaylist.get_clip_index_at(pos.frames(m_fps) + 1);

    int previousDuration = trackPlaylist.clip_length(clipIndex) - 1;
    int newDuration = out.frames(m_fps) - 1;

    kdDebug()<<" ** RESIZING CLIP END:" << clipIndex << " on track:"<< track <<", mid pos: "<<pos.frames(25)<<", in: "<<in.frames(25)<<", out: "<<out.frames(25)<<", PREVIOUS duration: "<<previousDuration<<endl;
    trackPlaylist.resize_clip(clipIndex, in.frames(m_fps), newDuration);
    trackPlaylist.consolidate_blanks(0);
    if (previousDuration < newDuration) {
	// clip was made longer, trim next blank if there is one.
	if (trackPlaylist.is_blank(clipIndex + 1)) {
	    trackPlaylist.split(clipIndex + 1, newDuration - previousDuration);
	    trackPlaylist.remove(clipIndex + 1);
	}
    }
    else trackPlaylist.insert_blank(clipIndex + 1, previousDuration - newDuration - 1);

    trackPlaylist.consolidate_blanks(0);
    tractor.multitrack()->refresh();
    tractor.refresh();
    if (track != 0) mltCheckLength();

}

void KRender::mltChangeTrackState(int track, bool mute, bool blind)
{
    Mlt::Service service(m_mltProducer->parent().get_service());

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());
    if (mute) {
	if (blind) trackProducer.set("hide", 3);
	else trackProducer.set("hide", 2);
    }
    else if (blind) {
	trackProducer.set("hide", 1);
    }
    else {
	trackProducer.set("hide", 0);
    }
    tractor.multitrack()->refresh();
    tractor.refresh();
    refresh();
}

void KRender::mltResizeClipStart(int track, GenTime pos, GenTime moveEnd, GenTime moveStart, GenTime in, GenTime out)
{
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() == playlist_type) kdDebug()<<"// PLAYLIST TYPE"<<endl;
    if (service.type() == tractor_type) kdDebug()<<"// TRACOT TYPE"<<endl;
    if (service.type() == multitrack_type) kdDebug()<<"// MULTITRACK TYPE"<<endl;
    if (service.type() == producer_type) kdDebug()<<"// PROD TYPE"<<endl;

    int moveFrame = (moveEnd - moveStart).frames(m_fps);

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());
    if (trackPlaylist.is_blank_at(pos.frames(m_fps) - 1)) 
	kdDebug()<<"////////  ERROR RSIZING BLANK CLIP!!!!!!!!!!!"<<endl;
    int clipIndex = trackPlaylist.get_clip_index_at(pos.frames(m_fps) - 1);
    kdDebug()<<" ** RESIZING CLIP START:" << clipIndex << " on track:"<< track <<", mid pos: "<<pos.frames(25)<<", moving: "<<moveFrame<<", in: "<<in.frames(25)<<", out: "<<out.frames(25)<<endl;

    trackPlaylist.resize_clip(clipIndex, in.frames(m_fps), out.frames(m_fps));
    if (moveFrame > 0) trackPlaylist.insert_blank(clipIndex, moveFrame - 1);
    else {
	int midpos = moveStart.frames(m_fps) - 1; //+ (moveFrame / 2)
	int blankIndex = trackPlaylist.get_clip_index_at(midpos);
	int blankLength = trackPlaylist.clip_length(blankIndex);

	kdDebug()<<" + resizing blank: "<<blankIndex<<", Mid: "<<midpos<<", Length: "<<blankLength<< ", SIZE DIFF: "<<moveFrame<<endl;

	
	if (blankLength + moveFrame == 0) trackPlaylist.remove(blankIndex);
	else trackPlaylist.resize_clip(blankIndex, 0, blankLength + moveFrame -1);
    }
    trackPlaylist.consolidate_blanks(0);

    kdDebug()<<"-----------------\n"<<"CLIP 0: "<<trackPlaylist.clip_start(0)<<", LENGT: "<<trackPlaylist.clip_length(0)<<endl;
    kdDebug()<<"CLIP 1: "<<trackPlaylist.clip_start(1)<<", LENGT: "<<trackPlaylist.clip_length(1)<<endl;
    kdDebug()<<"CLIP 2: "<<trackPlaylist.clip_start(2)<<", LENGT: "<<trackPlaylist.clip_length(2)<<endl;
    kdDebug()<<"CLIP 3: "<<trackPlaylist.clip_start(3)<<", LENGT: "<<trackPlaylist.clip_length(3)<<endl;
   kdDebug()<<"CLIP 4: "<<trackPlaylist.clip_start(4)<<", LENGT: "<<trackPlaylist.clip_length(4)<<endl;
}

void KRender::mltMoveClip(int startTrack, int endTrack, GenTime moveStart, GenTime moveEnd)
{
    mltMoveClip(startTrack, endTrack, (int) moveStart.frames(m_fps), (int) moveEnd.frames(m_fps));
}


void KRender::mltMoveClip(int startTrack, int endTrack, int moveStart, int moveEnd)
{
    m_mltConsumer->set("refresh", 0);
    Mlt::Service service(m_mltProducer->parent().get_service());

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(startTrack));
    Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());
    int clipIndex = trackPlaylist.get_clip_index_at(moveStart + 1);


    mlt_field field = mlt_tractor_field(tractor.get_tractor());

    mlt_multitrack multitrack = mlt_field_multitrack(field); //mlt_tractor_multitrack(tractor.get_tractor());
    kdDebug()<<" --  CURRENT MULTIOTRACK HAS: "<<mlt_multitrack_count(multitrack)<<" tracks"<<endl;;
    mlt_service multiprod = mlt_multitrack_service( multitrack );

    Mlt::Producer clipProducer(trackPlaylist.replace_with_blank(clipIndex));
    trackPlaylist.consolidate_blanks(0);
    mlt_events_block( MLT_PRODUCER_PROPERTIES(clipProducer.get_producer()), NULL );

    if (endTrack == startTrack) {
	if (!trackPlaylist.is_blank_at(moveEnd)) {
	    kdWarning()<<"// ERROR, CLIP COLLISION----------"<<endl;
	    int ix = trackPlaylist.get_clip_index_at(moveEnd);
		kdDebug()<<"BAD CLIP STARTS AT: "<<trackPlaylist.clip_start(ix)<<", LENGT: "<<trackPlaylist.clip_length(ix)<<endl;
	}
	trackPlaylist.insert_at(moveEnd, clipProducer, 1);
	trackPlaylist.consolidate_blanks(0);
    }
    else {
    	trackPlaylist.consolidate_blanks(0);
	Mlt::Producer destTrackProducer(tractor.track(endTrack));
	Mlt::Playlist destTrackPlaylist(( mlt_playlist ) destTrackProducer.get_service());
	destTrackPlaylist.consolidate_blanks(1);
	destTrackPlaylist.insert_at(moveEnd, clipProducer, 1);
	destTrackPlaylist.consolidate_blanks(0);
    }

    mltCheckLength();
    mlt_events_unblock( MLT_PRODUCER_PROPERTIES(clipProducer.get_producer()), NULL );
}

void KRender::mltMoveTransition(QString type, int startTrack, int trackOffset, GenTime oldIn, GenTime oldOut, GenTime newIn, GenTime newOut)
{
    m_mltConsumer->set("refresh", 0);
    mlt_service serv = m_mltProducer->parent().get_service();

    mlt_service nextservice = mlt_service_get_producer(serv);
    mlt_properties properties = MLT_SERVICE_PROPERTIES( nextservice );
    QString mlt_type = mlt_properties_get( properties, "mlt_type" );
    QString resource = mlt_properties_get( properties, "mlt_service");
    int old_pos = (oldIn.frames(m_fps) + oldOut.frames(m_fps)) / 2;

    int new_in = newIn.frames(m_fps);
    int new_out = newOut.frames(m_fps) - 1;
    while (mlt_type == "transition") {
	mlt_transition tr = (mlt_transition) nextservice;
	int currentTrack = mlt_transition_get_b_track(tr);
	int currentIn = (int) mlt_transition_get_in(tr);
        int currentOut = (int) mlt_transition_get_out(tr);
	kdDebug()<<"// FOUND EXISTING TRANS, IN: "<<currentIn<<", OUT: "<<currentOut<<", TRACK_A: "<<mlt_transition_get_a_track(tr)<<"B_TRACK: "<<mlt_transition_get_b_track(tr)<<", starttrack: "<<startTrack<<endl;
	//kdDebug()<<"// LOOKING FOR IN: "<<old_in<<", OUT: "<<old_out<<endl;
	kdDebug()<<"// OLD POS: "<<old_pos<<" // NEW IN: "<<new_in<<", OUT: "<<new_out<<endl;
	if (resource == type && startTrack == currentTrack && currentIn <= old_pos && currentOut >= old_pos) {
            mlt_transition_set_in_and_out(tr, new_in, new_out);
            if (trackOffset != 0) {
		mlt_properties properties = MLT_TRANSITION_PROPERTIES( tr );
		mlt_properties_set_int( properties, "a_track", mlt_transition_get_a_track(tr) + trackOffset );
		mlt_properties_set_int( properties, "b_track", mlt_transition_get_b_track(tr) + trackOffset );
            }
            break;
        }
	nextservice = mlt_service_producer(nextservice);
	properties = MLT_SERVICE_PROPERTIES( nextservice );
	mlt_type = mlt_properties_get( properties, "mlt_type" );
	resource = mlt_properties_get( properties, "mlt_service" );
    }
}

void KRender::mltAddTransition(QString tag, int a_track, int b_track, GenTime in, GenTime out, QMap <QString, QString> args)
{
    Mlt::Service service(m_mltProducer->parent().get_service());

    Mlt::Tractor tractor(service);
    Mlt::Field *field = tractor.field();

    char *transId = decodedString(tag);
    Mlt::Transition *transition = new Mlt::Transition(transId);
    transition->set_in_and_out((int) in.frames(m_fps), (int) out.frames(m_fps));
    QMap<QString, QString>::Iterator it;
    QString key;

    kdDebug()<<" ------  ADDING TRANSITION PARAMs: "<<args.count()<<endl;

    for ( it = args.begin(); it != args.end(); ++it ) {
	key = it.key();
        char *name = decodedString(key);
        char *value = decodedString(it.data());
        transition->set(name, value);
	kdDebug()<<" ------  ADDING TRANS PARAM: "<<name<<": "<<value<<endl;
	//filter->set("kdenlive_id", id);
	delete[] name;
	delete[] value;
    }
    // attach filter to the clip
    field->plant_transition(*transition, a_track, b_track);
    delete[] transId;
    refresh();

}

void KRender::mltSavePlaylist()
{
    kdWarning()<<"// UPDATING PLAYLIST TO DISK++++++++++++++++"<<endl;
    Mlt::Consumer *fileConsumer = new Mlt::Consumer("westley");
    fileConsumer->set("resource", "/home/one/playlist.xml");

    Mlt::Service service(m_mltProducer->get_service());
    Mlt::Tractor tractor(service);

    fileConsumer->connect(service);
    fileConsumer->start();

}

