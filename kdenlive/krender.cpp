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

QPixmap KRender::getVideoThumbnail(KURL url, int frame_position, int width, int height)
{
    QPixmap pix(width, height);
    char *tmp = decodedString(url.path());
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
    if (m_mltProducer) return mlt_producer_get_playtime(m_mltProducer->get_producer());
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
	filePropertyMap["duration"] = QString::number(producer.get_length());
        
        Mlt::Filter m_convert("avcolour_space");
        m_convert.set("forced", mlt_image_rgb24a);
        producer.attach(m_convert);

	Mlt::Frame * frame = producer.get_frame();

	if (frame->is_valid()) {
	    filePropertyMap["fps"] =
		QString::number(frame->get_double("fps"));
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
void KRender::setSceneList(QDomDocument list, int position)
{
    if (!m_winid == -1) return;
    m_generateScenelist = true;

    m_sceneList = list;

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

    m_mltProducer = track.current();
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
    m_mltProducer->set("out", m_mltProducer->get_length() - 1);
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
    m_mltProducer->set("out", m_mltProducer->get_length() - 1);
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
        QApplication::postEvent(qApp->mainWidget(), new PositionChangeEvent( GenTime((int) position, m_fps), m_monitorId));
}

void KRender::emitConsumerStopped()
{
    // This is used to know when the playing stopped
    if (m_mltProducer && !m_generateScenelist) {
	double pos = m_mltProducer->position();
        QApplication::postEvent(qApp->mainWidget(), new PositionChangeEvent(GenTime((int) pos, m_fps), m_monitorId + 100));
	//new QCustomEvent(10002));
    }
}


/*                           FILE RENDERING STUFF                     */

#ifdef ENABLE_FIREWIRE
int readCount = 1;
int fileSize = 0;
int fileProgress = 0;
int droppedFrames = 0;

//  FIREWIRE EXPORT, REQUIRES LIBIECi61883
static int read_frame (unsigned char *data, int n, unsigned int dropped, void *callback_data)
{
    FILE *f = (FILE*) callback_data;


    if (n == 1)
        if (fread (data, 480, 1, f) < 1) {
        return -1;
        } else {
	    //long int position = ftell(f);
	    readCount++;
            return 0;
	}
            else {
                return 0;
	    }
}

static int g_done = 0;

static void sighandler (int sig)
{
    g_done = 1;
}

void KRender::dv_transmit( raw1394handle_t handle, FILE *f, int channel)
{	
    iec61883_dv_t dv;
    unsigned char data[480];
    int ispal;
	
    fread (data, 480, 1, f);
    ispal = (data[ 3 ] & 0x80) != 0;
    dv = iec61883_dv_xmit_init (handle, ispal, read_frame, (void *)f );

    if (dv) iec61883_dv_set_synch( dv, 1 );
	
    if (dv && iec61883_dv_xmit_start (dv, channel) == 0)
    {
        int fd = raw1394_get_fd (handle);
        struct timeval tv;
        fd_set rfds;
        int result = 0;
		
        signal (SIGINT, sighandler);
        signal (SIGPIPE, sighandler);
        fprintf (stderr, "Starting to transmit %s\n", ispal ? "PAL" : "NTSC");

        do {
            FD_ZERO (&rfds);
            FD_SET (fd, &rfds);
            tv.tv_sec = 0;
            tv.tv_usec = 20000;
            if (select (fd + 1, &rfds, NULL, NULL, &tv) > 0) {
                result = raw1394_loop_iterate (handle);
		if (((int)(readCount * 100 / fileSize )) != fileProgress) {
			fileProgress = readCount * 100 / fileSize;
	    		//kdDebug()<<"++ FRAME READ2: "<<fileProgress<<endl;
	    		QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(fileProgress, 10006));
			qApp->processEvents();
		}
	    }
			
        } while (g_done == 0 && result == 0);
	iec61883_dv_xmit_stop (dv);
	droppedFrames = iec61883_dv_get_dropped(dv);
        fprintf (stderr, "done.\n");
    }
    iec61883_dv_close (dv);
}
#endif

void KRender::exportFileToFirewire(QString srcFileName, int port, GenTime startTime, GenTime endTime)
{
#ifdef ENABLE_FIREWIRE
    //exportTimeline(QString::null);
    kdDebug()<<"START DV EXPORT ++++++++++++++: "<<srcFileName<<endl;

    fileSize = QFile(srcFileName).size() / 480;
    readCount = 1;
    fileProgress = 0;

    FILE *f = NULL;
    int oplug = -1, iplug = -1;
    f = fopen (decodedString(srcFileName), "rb");
    raw1394handle_t handle = raw1394_new_handle_on_port (port);
    if (f == NULL) {
        KMessageBox::sorry(0,i18n("Cannot open file: ")+srcFileName.ascii());
        return;
    }
    if (handle == NULL) {
        KMessageBox::sorry(0,i18n("NO firewire access on that port\nMake sure you have loaded the raw1394 module and that you have write access on /dev/raw1394..."));
        return;
    }
    nodeid_t node = 0xffc0;
    int bandwidth = -1;
    int channel = iec61883_cmp_connect (handle, raw1394_get_local_id (handle), &oplug, node, &iplug, &bandwidth);
    if (channel > -1)
    {
        dv_transmit (handle, f, channel);
        iec61883_cmp_disconnect (handle, raw1394_get_local_id (handle), oplug, node, iplug, channel, bandwidth);
    }
    else KMessageBox::sorry(0,i18n("NO DV device found"));
    fclose (f);
    raw1394_destroy_handle (handle);
    QApplication::postEvent(qApp->mainWidget(), new QCustomEvent(10003));
    if (droppedFrames > 0) KMessageBox::sorry(0, i18n("Transmission of dv file is finished.\n%1 frames were dropped during transfer.").arg(droppedFrames));
    else KMessageBox::information(0, i18n("Transmission of dv file finished successfully."));
#else
KMessageBox::sorry(0, i18n("Firewire is not enabled on your system.\n Please install Libiec61883 and recompile Kdenlive"));
#endif
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
    QApplication::postEvent(qApp->mainWidget(), new UrlEvent(url, 10003));
}

/**	MLT PLAYLIST DIRECT MANIPULATON		**/


void KRender::mltInsertClip(int track, int position, QString resource)
{
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() == playlist_type) kdDebug()<<"// PLAYLIST TYPE"<<endl;
    if (service.type() == tractor_type) kdDebug()<<"// TRACOT TYPE"<<endl;
    if (service.type() == multitrack_type) kdDebug()<<"// MULTITRACK TYPE"<<endl;
    if (service.type() == producer_type) kdDebug()<<"// PROD TYPE"<<endl;

    Mlt::Tractor tractor(service);
    kdDebug()<<"/ / / / TRACTOR: "<<tractor.count()<<endl;
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());
    char *tmp = decodedString(resource);
    Mlt::Producer *prod = new Mlt::Producer(tmp);
    delete[] tmp;
    trackPlaylist.insert_at(position, prod, 1);
    double duration = Mlt::Producer(trackPlaylist.get_producer()).get_playtime();
    kdDebug()<<"// +  +INSERTING CLIP: "<<resource<<" AT: "<<position<<" on track: "<<track<<", DURATION: "<<duration<<endl;
    tractor.multitrack()->refresh();
    tractor.refresh();
    int length = (int) mlt_producer_get_playtime(tractor.get_producer());
    m_mltProducer->set("out", length);
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
    Mlt::Service clipService(clip->get_service());

    // create filter
    kdDebug()<<" / / INSERTING EFFECT: "<<id<<endl;
    if (tag.startsWith("ladspa")) tag = "ladspa";
    char *filterId = decodedString(tag);
    Mlt::Filter *filter = new Mlt::Filter(filterId);

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
	filter->set("kdenlive_id", id);
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
    if (it.key().startsWith("#") || tag.startsWith("ladspa") || tag == "sox") {
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
    if (service.type() == playlist_type) kdDebug()<<"// PLAYLIST TYPE"<<endl;
    if (service.type() == tractor_type) kdDebug()<<"// TRACOT TYPE"<<endl;
    if (service.type() == multitrack_type) kdDebug()<<"// MULTITRACK TYPE"<<endl;
    if (service.type() == producer_type) kdDebug()<<"// PROD TYPE"<<endl;

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(track));
    Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());
    int clipIndex = trackPlaylist.get_clip_index_at(pos.frames(m_fps) + 1);

    int previousDuration = trackPlaylist.clip_length(clipIndex);
    int newDuration = out.frames(m_fps);

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
    else trackPlaylist.insert_blank(clipIndex + 1, previousDuration - newDuration);

    trackPlaylist.consolidate_blanks(0);
    tractor.multitrack()->refresh();
    tractor.refresh();
    int length = (int) mlt_producer_get_playtime(tractor.get_producer());
    kdDebug()<<"// LENGT AFTER RESIZE: "<<length<<endl;
    m_mltProducer->set("out", length);
}

void KRender::mltChangeTrackState(int track, bool mute, bool blind)
{
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() == playlist_type) kdDebug()<<"// PLAYLIST TYPE"<<endl;
    if (service.type() == tractor_type) kdDebug()<<"// TRACOT TYPE"<<endl;
    if (service.type() == multitrack_type) kdDebug()<<"// MULTITRACK TYPE"<<endl;
    if (service.type() == producer_type) kdDebug()<<"// PROD TYPE"<<endl;

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
    int clipIndex = trackPlaylist.get_clip_index_at(pos.frames(m_fps) - 1);
    kdDebug()<<" ** RESIZING CLIP START:" << clipIndex << " on track:"<< track <<", mid pos: "<<pos.frames(25)<<", moving: "<<moveFrame<<", in: "<<in.frames(25)<<", out: "<<out.frames(25)<<endl;

    trackPlaylist.resize_clip(clipIndex, in.frames(m_fps), out.frames(m_fps));
    if (moveFrame > 0) trackPlaylist.insert_blank(clipIndex, moveFrame);
    else {
	int midpos = moveStart.frames(m_fps) + (moveFrame / 2);
	int blankIndex = trackPlaylist.get_clip_index_at(midpos);
	int blankLength = trackPlaylist.clip_length(blankIndex);
	kdDebug()<<" + resizing blank: "<<blankIndex<<", Mid: "<<midpos<<", Length: "<<blankLength<< ", SIZE DIFF: "<<moveFrame<<endl;
	trackPlaylist.resize_clip(blankIndex, 0, blankLength + moveFrame);
    }
    trackPlaylist.consolidate_blanks(0);
}

void KRender::mltMoveClip(int startTrack, int endTrack, GenTime moveStart, GenTime moveEnd)
{
    mltMoveClip(startTrack, endTrack, (int) moveStart.frames(m_fps), (int) moveEnd.frames(m_fps));
}

void KRender::mltMoveClip(int startTrack, int endTrack, int moveStart, int moveEnd)
{
    Mlt::Service service(m_mltProducer->parent().get_service());
    if (service.type() == playlist_type) kdDebug()<<"// PLAYLIST TYPE"<<endl;
    if (service.type() == tractor_type) kdDebug()<<"// TRACOT TYPE"<<endl;
    if (service.type() == multitrack_type) kdDebug()<<"// MULTITRACK TYPE"<<endl;
    if (service.type() == producer_type) kdDebug()<<"// PROD TYPE"<<endl;

    Mlt::Tractor tractor(service);
    Mlt::Producer trackProducer(tractor.track(startTrack));
    Mlt::Playlist trackPlaylist(( mlt_playlist ) trackProducer.get_service());
    int clipIndex = trackPlaylist.get_clip_index_at(moveStart + 1);

    kdDebug()<<"// Playtime for start track : "<<startTrack<<" is: "<<mlt_producer_get_playtime(trackPlaylist.get_producer())<<endl;

    Mlt::Producer clipProducer(trackPlaylist.replace_with_blank(clipIndex));
    trackPlaylist.consolidate_blanks(1);
    if (endTrack == startTrack) {
	kdDebug()<<" / / / moving clip from: "<<moveStart<<" to: "<<moveEnd<<endl;
	trackPlaylist.insert_at(moveEnd, clipProducer, 1);
	trackPlaylist.consolidate_blanks(0);
    }
    else {
	Mlt::Producer destTrackProducer(tractor.track(endTrack));
	Mlt::Playlist destTrackPlaylist(( mlt_playlist ) destTrackProducer.get_service());
	destTrackPlaylist.consolidate_blanks(1);
	destTrackPlaylist.insert_at(moveEnd, clipProducer, 1);
	destTrackPlaylist.consolidate_blanks(0);
    }

    /*
    Mlt::Field *field = tractor.field();
    Mlt::Service service2(m_mltProducer->get_service());
    //Mlt::Service service2(mlt_service_get_producer(tractor.field()->get_service()));

    kdDebug()<<"--------------------------------- FIELD TYPE IS: "<<service2.get("resource")<<endl;
    if (service2.type() == playlist_type) kdDebug()<<"// PLAYLIST TYPE"<<endl;
    else if (service2.type() == tractor_type) kdDebug()<<"// TRACTOR TYPE"<<endl;
    else if (service2.type() == multitrack_type) kdDebug()<<"// MULTITRACK TYPE"<<endl;
    else if (service2.type() == producer_type) kdDebug()<<"// PROD TYPE"<<endl;
    else if (service2.type() == transition_type) kdDebug()<<"//WELL DONE,  TRANSITION TYPE"<<endl;
    else kdDebug()<<"//UNKNOWN SERVICE..."<<endl;
    kdDebug()<<"-------------------------------------------------------- "<<endl;

    Mlt::Producer prod2(service2);
    //Mlt::Service service4(mlt_producer_service(prod2.get_parent()));
    mlt_field fld = mlt_tractor_field((mlt_tractor) mlt_producer_service(prod2.get_parent()));
    //Mlt::Field field2 = tractor.field();
    
    Mlt::Service *service4 = new Mlt::Service(mlt_field_service(fld));
    while (service4 && service4->type() == producer_type) service4 = new Mlt::Service (mlt_service_get_producer(service4->get_service()));


    mlt_field fld2 = mlt_tractor_field((mlt_tractor) service4);

    Mlt::Service *service3 = new Mlt::Service(mlt_field_service(fld2));

	
    if (service3) 
    {

    //kdDebug()<<"--------------------------------- FIELD SERICE TYPE IS: "<<service3->get("resource")<<endl;
    if (service3->type() == playlist_type) kdDebug()<<"// PLAYLIST TYPE"<<endl;
    else if (service3->type() == tractor_type) kdDebug()<<"// TRACTOR TYPE"<<endl;
    else if (service3->type() == multitrack_type) kdDebug()<<"// MULTITRACK TYPE"<<endl;
    else if (service3->type() == producer_type) kdDebug()<<"// PROD TYPE"<<endl;
    else if (service3->type() == transition_type) kdDebug()<<"//WELL DONE,  TRANSITION TYPE"<<endl;
    else if (service3->type() == filter_type) kdDebug()<<"// FILTER TYPE"<<endl;
    else if (service3->type() == unknown_type) kdDebug()<<"// UNKNWN TYPE"<<endl;
    else kdDebug()<<"//UNKNOWN SERVICE..."<<endl;
    kdDebug()<<"-------------------------------------------------------- "<<endl;

    }*/

    tractor.multitrack()->refresh();
    tractor.refresh();

    int length = (int) mlt_producer_get_playtime(tractor.get_producer());
    m_mltProducer->set("out", length);

}


