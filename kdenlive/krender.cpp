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

#include "krender.h"
#include "avformatdescbool.h"
#include "avformatdesclist.h"
#include "avformatdesccontainer.h"
#include "avformatdesccodeclist.h"
#include "avformatdesccodec.h"
#include "effectparamdesc.h"
#include "initeffects.h"

static QMutex mutex (true);
double m_fps;

KRender::KRender(const QString & rendererName, Gui::KdenliveApp *parent, const char *name):QObject(parent, name), m_name(rendererName), m_app(parent), m_renderingFormat(0),
m_mltTractor(NULL), m_mltConsumer(NULL), m_mltProducer(NULL), m_fileRenderer(NULL), m_mltFileProducer(NULL), m_mltTextProducer(NULL)
{
    openMlt();
    refreshTimer = new QTimer( this );
    connect( refreshTimer, SIGNAL(timeout()), this, SLOT(refresh()) );
    m_osdProfile = locate("data", "kdenlive/profiles/metadata.properties");

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
    //killTimers();
}


void KRender::openMlt()
{
	if (Mlt::Factory::init(NULL) != 0) kdWarning()<<"Error initializing MLT, Crash will follow"<<endl;
	else kdDebug() << "Mlt inited" << endl;
	m_mltTractor = new Mlt::Tractor::Tractor();
}

void KRender::closeMlt()
{
    m_mltTractor->block();
    delete refreshTimer;
    if (m_fileRenderer) delete m_fileRenderer;
    if (m_mltFileProducer) delete m_mltFileProducer;
    if (m_mltConsumer)
        delete m_mltConsumer;
    if (m_mltProducer);
    delete m_mltProducer;
}

 

static void consumer_frame_show(mlt_consumer, KRender * self, mlt_frame frame_ptr)
{
    // detect if the producer has finished playing. Is there a better way to do it ?
    if (mlt_properties_get_double( MLT_FRAME_PROPERTIES( frame_ptr ), "_speed" ) == 0)
        self->emitConsumerStopped();
    else {
	mlt_position framePosition = mlt_frame_get_position(frame_ptr);
	self->emitFrameNumber(GenTime(framePosition, m_fps), 10000);
    }
}

static void file_consumer_frame_show(mlt_consumer, KRender * self, mlt_frame frame_ptr)
{
    mlt_position framePosition = mlt_frame_get_position(frame_ptr);
    self->emitFileFrameNumber(GenTime(framePosition, KdenliveSettings::defaultfps()), 10001);
}

static void consumer_stopped(mlt_consumer, KRender * self, mlt_frame frame_ptr)
{
    mlt_position framePosition = mlt_frame_get_position(frame_ptr);
    self->emitConsumerStopped();
}

static void file_consumer_stopped(mlt_consumer, KRender * self, mlt_frame)
{
    self->emitFileConsumerStopped();
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

void KRender::createVideoXWindow(bool , WId winid)
{
    m_mltConsumer = new Mlt::Consumer("sdl_preview");
    if (!m_mltConsumer->is_valid()) kdError()<<"Sorry, cannot create MLT consumer, check your MLT install"<<endl;
    m_mltConsumer->listen("consumer-frame-show", this, (mlt_listener) consumer_frame_show);
    //m_mltConsumer->listen("consumer-stopped", this, (mlt_listener) consumer_stopped);

    //only as is saw, if we want to lock something with the sdl lock
    if (!KdenliveSettings::videoprofile().isEmpty()) 
	m_mltConsumer->set("profile", KdenliveSettings::videoprofile());
    m_mltConsumer->set("app_locked", 1);
    m_mltConsumer->set("app_lock", (void *) &my_lock, 0);

    m_mltConsumer->set("app_unlock", (void *) &my_unlock, 0);
    m_mltConsumer->set("window_id", (int) winid);

    m_mltConsumer->set("resize", 1);
    m_mltConsumer->set("rescale", "nearest");

    //m_mltConsumer->set("audio_driver","dsp");
    m_mltConsumer->set("progressive", 1);
    m_mltConsumer->set("audio_buffer", 1024);
    m_mltConsumer->set("frequency", 44100);
    m_mltConsumer->set("buffer", 1);

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

QPixmap KRender::extractFrame(int frame, int width, int height)
{
    QPixmap pix(width, height);
    if (!m_mltProducer) {
	pix.fill(black);
	return pix;
    }
    Mlt::Producer mlt_producer(m_mltProducer->get_producer());
    mlt_producer.seek(frame);
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    mlt_producer.attach(m_convert);
    pix.fill(Qt::black);

    Mlt::Frame *m_frame = mlt_producer.get_frame();

    if (m_frame) {
	m_frame->set("rescale", "nearest");
	uchar *m_thumb = m_frame->fetch_image(mlt_image_rgb24a, width, height, 1);
	QImage m_image(m_thumb, width, height, 32, 0, 0, QImage::LittleEndian);
	delete m_frame;
	
	if (!m_image.isNull()) {
	    bitBlt(&pix, 0, 0, &m_image, 0, 0, width, height);
	}
    }
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

QPixmap KRender::getVideoThumbnail(KURL url, int frame, int width, int height)
{
    QPixmap pixmap(width, height);
    Mlt::Producer m_producer(decodedString(url.path()));
    if (m_producer.is_blank()) {
	return 0;
    }
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    m_producer.seek(frame);
    Mlt::Frame * m_frame = m_producer.get_frame();

    if (m_frame) {
	m_frame->set("rescale", "nearest");
	uchar *m_thumb = m_frame->fetch_image(mlt_image_rgb24a, width, height, 1);

        // what's the use of this ? I don't think we need it - commented out by jbm 
	//m_producer.set("thumb", m_thumb, width * height * 4, mlt_pool_release);
        //m_frame->set("image", m_thumb, 0, NULL, NULL);
	QImage m_image(m_thumb, width, height, 32, 0, 0, QImage::IgnoreEndian);
	delete m_frame;
	if (!m_image.isNull())
	    bitBlt(&pixmap, 0, 0, &m_image, 0, 0, width, height);
    }
    return pixmap;
}


void KRender::getImage(KURL url, int frame, int width, int height)
{
    Mlt::Producer m_producer(decodedString(url.path()));
    if (m_producer.is_blank()) {
	return;
    }
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    m_producer.seek(frame);

    Mlt::Frame * m_frame = m_producer.get_frame();

    if (m_frame) {
	m_frame->set("rescale", "nearest");
	uchar *m_thumb = m_frame->fetch_image(mlt_image_rgb24a, width - 2, height - 2, 1);
	
	QPixmap m_pixmap(width, height);
	m_pixmap.fill(Qt::black);
	QImage m_image(m_thumb, width, height, 32, 0, 0, QImage::IgnoreEndian);
	delete m_frame;
	if (!m_image.isNull())
	    bitBlt(&m_pixmap, 1, 1, &m_image, 0, 0, width - 2, height - 2);
	emit replyGetImage(url, frame, m_pixmap, width, height);
    }
}

/* Create thumbnail for text clip */
void KRender::getImage(int id, QString txt, uint size, int width, int height)
{
    Mlt::Producer m_producer("pango");
    m_producer.set("markup", decodedString(txt));
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    m_producer.seek(1);
    Mlt::Frame * m_frame = m_producer.get_frame();

    if (m_frame) {
        m_frame->set("rescale", "nearest");

        uchar *m_thumb = m_frame->fetch_image(mlt_image_rgb24a, width - 2, height - 2, 1);
        
        //m_producer.set("thumb", m_thumb, width * height * 4, mlt_pool_release);
        //m_frame->set("image", m_thumb, 0, NULL, NULL);

        QPixmap m_pixmap(width, height);
	m_pixmap.fill(Qt::black);

        QImage m_image(m_thumb, width - 2, height - 2, 32, 0, 0,                       QImage::IgnoreEndian);

        delete m_frame;
        if (!m_image.isNull())
	    bitBlt(&m_pixmap, 1, 1, &m_image, 0, 0, width - 2, height - 2);

        emit replyGetImage(id, m_pixmap, width, height);
    }

}

/* Create thumbnail for color */
void KRender::getImage(int id, QString color, int width, int height)
{
    QPixmap pixmap(width-2, height-2);
    color = color.replace(0, 2, "#");
    color = color.left(7);
    pixmap.fill(QColor(color));
    QPixmap result(width, height);
    result.fill(Qt::black);
    copyBlt(&result, 1, 1, &pixmap, 0, 0, width - 2, height - 2);
    emit replyGetImage(id, result, width, height);

}

/* Create thumbnail for image */
void KRender::getImage(KURL url, int width, int height)
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

    pixmap = im.smoothScale(width-2, height-2);
    QPixmap result(width, height);
    result.fill(Qt::black);
    copyBlt(&result, 1, 1, &pixmap, 0, 0, width - 2, height - 2);
    emit replyGetImage(url, 1, result, width, height);
}


double KRender::consumerRatio()
{
    if (!m_mltConsumer) return 1.0;
    return (m_mltConsumer->get_double("aspect_ratio_num")/m_mltConsumer->get_double("aspect_ratio_den"));
}

void KRender::restoreProducer()
{
    if (m_mltConsumer && !m_mltConsumer->is_stopped()) m_mltConsumer->stop();
    if (m_mltTextProducer) delete m_mltTextProducer;
    m_mltTextProducer = 0;
    m_mltConsumer->purge();
    if(m_mltProducer == NULL) return;
    m_mltProducer->set_speed(0.0);
    m_mltConsumer->start();
    m_mltConsumer->connect(*m_mltProducer);
    refresh();
}

void KRender::setTitlePreview(QString tmpFileName)
{
    if (m_mltConsumer && !m_mltConsumer->is_stopped()) {
        m_mltConsumer->stop();
    }
    m_mltConsumer->purge();
    int pos = 0;
    if (m_mltTextProducer) delete m_mltTextProducer;
    m_mltTextProducer = 0;
	// If there is no clip in the monitor, use a black video as first track	
    if(m_mltProducer == NULL) {
        QString ctext2;
        ctext2="<producer><property name=\"mlt_service\">colour</property><property name=\"colour\">black</property></producer>";
        m_mltTextProducer = new Mlt::Producer("westley-xml",decodedString(ctext2));
    }
    else {
        pos = m_mltProducer->position();
        m_mltTextProducer = new Mlt::Producer(m_mltProducer->get_producer());
    }
    // Create second producer with the png image created by the titler
    QString ctext;
    ctext="<producer><property name=\"resource\">"+tmpFileName+"</property></producer>";
    Mlt::Producer prod2("westley-xml",decodedString(ctext));
    Mlt::Tractor tracks;

	// Add composite transition for overlaying the 2 tracks
    Mlt::Transition convert( "composite" ); 
    convert.set("distort",1);
    convert.set("progressive",1);
    convert.set("always_active",1);
    
	// Define the 2 tracks
    tracks.set_track(*m_mltTextProducer,0);
    tracks.set_track(prod2,1);
    tracks.plant_transition( convert ,0,1);
    tracks.seek(pos);
    m_mltTextProducer->seek(pos);
    tracks.set_speed(0.0);
    m_mltConsumer->connect(tracks);
    m_mltConsumer->start();
    refresh();
}

int KRender::getLength()
{
    if (m_mltProducer) return m_mltProducer->get_length();
    return 0;
}

bool KRender::isValid(KURL url)
{
    Mlt::Producer producer(decodedString(url.path()));
    if (producer.is_blank())
	return false;

    return true;
}


void KRender::getFileProperties(KURL url)
{
        uint width = 50;
        uint height = 40;
	Mlt::Producer producer(decodedString(url.path()));
    	if (producer.is_blank()) {
	    return;
    	}

	mlt_properties properties = MLT_PRODUCER_PROPERTIES( producer.get_producer() );
	m_filePropertyMap.clear();
	m_filePropertyMap["filename"] = url.path();
	m_filePropertyMap["duration"] = QString::number(producer.get_length());
        
        Mlt::Filter m_convert("avcolour_space");
        m_convert.set("forced", mlt_image_rgb24a);
        producer.attach(m_convert);

	Mlt::Frame * frame = producer.get_frame();

	if (frame->is_valid()) {
	    m_filePropertyMap["fps"] =
		QString::number(frame->get_double("fps"));
	    m_filePropertyMap["width"] =
		QString::number(frame->get_int("width"));
	    m_filePropertyMap["height"] =
		QString::number(frame->get_int("height"));
	    m_filePropertyMap["frequency"] =
		QString::number(frame->get_int("frequency"));
	    m_filePropertyMap["channels"] =
		QString::number(frame->get_int("channels"));

	    // metadata
	    QStringList metadata_tags;
	    metadata_tags<<"album"<<"description"<<"copyright"<<"title"<<"author"<<"artist"<<"tracknumber"<<"comment";

	    for ( QStringList::Iterator it = metadata_tags.begin(); it != metadata_tags.end(); ++it ) {
	        m_filePropertyMap[*it] = mlt_properties_get(properties, *it);
	    }

	    if (frame->get_int("test_image") == 0) {
		if (frame->get_int("test_audio") == 0)
		    m_filePropertyMap["type"] = "av";
		else
		    m_filePropertyMap["type"] = "video";
                
                // Generate thumbnail for this frame
                uchar *m_thumb = frame->fetch_image(mlt_image_rgb24a, width - 2, height - 2, 1);
                QPixmap pixmap(width, height);
		pixmap.fill(Qt::black);
                QImage m_image(m_thumb, width - 2, height - 2, 32, 0, 0,                            QImage::IgnoreEndian);
                if (!m_image.isNull())
		    bitBlt(&pixmap, 1, 1, &m_image, 0, 0, width - 2, height - 2);
                    //pixmap = m_image.smoothScale(width, height);
                    
                emit replyGetImage(url, 0, pixmap, width, height);
                
	    } else if (frame->get_int("test_audio") == 0) {
                QPixmap pixmap(locate("appdata", "graphics/music.png"));
                emit replyGetImage(url, 0, pixmap, width, height);
		m_filePropertyMap["type"] = "audio";
            }
	}
	emit replyGetFileProperties(m_filePropertyMap);
	delete frame;
}

QDomDocument KRender::sceneList()
{
    return m_sceneList;
}

/** Create the producer from the Westley QDomDocument */
void KRender::setSceneList(QDomDocument list, bool resetPosition)
{
    GenTime pos = seekPosition();
    m_sceneList = list;

    m_mltTractor->block();

    if (m_mltProducer != NULL) {
	delete m_mltProducer;
	m_mltProducer = NULL;
	emit stopped();
    }

    Mlt::Playlist track;
    m_mltTractor->set_track( track, 0 );
    Mlt::Producer clip("westley-xml", decodedString(list.toString()));

    if (!clip.is_valid()) 
	kdWarning()<<" ++++ WARNING, UNABLE TO CREATE MLT PRODUCER"<<endl;
    else {

	m_mltProducer = m_mltTractor->track( 0 );
        Mlt::Playlist firsttrack( *m_mltProducer );
        firsttrack.append(clip);

	if (KdenliveSettings::osdtimecode()) {
		// Attach filter for on screen display of timecode
		mlt_properties properties = MLT_PRODUCER_PROPERTIES(firsttrack.current()->get_producer());
		mlt_properties_set_int( properties, "meta.attr.timecode", 1);
		mlt_properties_set( properties, "meta.attr.timecode.markup", "#tc#");

    		Mlt::Filter m_convert("data_show");
    		m_convert.set("resource", m_osdProfile);
    		if (m_mltProducer->attach(m_convert) == 1) kdDebug()<<"////// error attaching filter"<<endl;
	}

    	if (!resetPosition) seek(pos);
	m_mltProducer->set_speed(0.0);
	m_fps = m_mltProducer->get_fps();
        m_mltTractor->unblock();
    	if (m_mltConsumer) 
    	{
	    m_mltConsumer->start();
	    m_mltConsumer->connect(*m_mltProducer);
	    refresh();
    	}
    	else kdDebug()<<"++++++++ WARNING, SET SCENE LIST BUT CONSUMER NOT READY"<<endl;
    }
}


void KRender::start()
{
    if (m_mltConsumer && m_mltConsumer->is_stopped()) {
	m_mltConsumer->start();
        refresh();
    }
}

void KRender::stop()
{
    m_mltTractor->block();

    if (m_mltConsumer && !m_mltConsumer->is_stopped()) {
	m_mltConsumer->stop();
    }

    if (m_mltProducer) {
	m_mltProducer->set_speed(0.0);
	m_mltProducer->set("out", m_mltProducer->get_length() - 1);
    }
}

void KRender::stop(const GenTime & startTime)
{
    if (m_mltProducer) {
	m_mltProducer->set_speed(0.0);
	m_mltProducer->seek(startTime.frames(m_fps));
    }
//refresh();
}

void KRender::play(double speed)
{
    if (!m_mltProducer)
	return;
    m_mltProducer->set("out", m_mltProducer->get_length() - 1);
    m_mltProducer->set_speed(speed);
    refresh();
}

void KRender::play(double speed, const GenTime & startTime)
{
    if (!m_mltProducer)
	return;
    m_mltProducer->set("out", m_mltProducer->get_length() - 1);
    m_mltProducer->set_speed(speed);
    m_mltProducer->seek((int) (startTime.frames(m_fps)));
    refresh();
}

void KRender::play(double speed, const GenTime & startTime,
    const GenTime & stopTime)
{
    if (!m_mltProducer)
	return;
    m_mltProducer->set("out", stopTime.frames(m_fps));
    m_mltProducer->seek((int) (startTime.frames(m_fps)));
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
//    kdDebug()<<"++++++++  START REFRESH ++++++"<<endl;
}

void KRender::refresh()
{
    refreshTimer->stop();
    if (m_mltConsumer) {
	//m_mltConsumer->lock();
	m_mltConsumer->set("refresh", 1);
	//m_mltConsumer->unlock();
    }
}


/** Returns the effect list. */
const EffectDescriptionList & KRender::effectList() const
{
    return m_app->effectList();
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
    return GenTime(0);
}


const QString & KRender::rendererName() const
{
    return m_name;
}


void KRender::emitFrameNumber(const GenTime & time, int eventType)
{
        QApplication::postEvent(m_app, new PositionChangeEvent(time, eventType));
}

void KRender::emitFileFrameNumber(const GenTime & time, int eventType)
{
    if (m_fileRenderer) {
        QApplication::postEvent(m_app, new PositionChangeEvent(GenTime(m_mltFileProducer->position(), m_mltFileProducer->get_fps()), eventType));
    }
}

void KRender::emitConsumerStopped()
{
    //kdDebug()<<"+++++++++++  SDL CONSUMER STOPPING ++++++++++++++++++"<<endl;
    // This is used to know when the playing stopped
    if (m_mltProducer) {
	double pos = m_mltProducer->position();
        QApplication::postEvent(m_app, new PositionChangeEvent(GenTime(pos, m_fps), 10002));
	//new QCustomEvent(10002));
    }
}

void KRender::emitFileConsumerStopped()
{
    if (m_fileRenderer) {
        delete m_fileRenderer;
        m_fileRenderer = NULL;
    }
    if (m_mltFileProducer) {
        delete m_mltFileProducer;
        m_mltFileProducer = NULL;
    }

        // This is used when exporting to a file so that we know when the export is finished
    if (!m_exportedFile.isEmpty()) {
	QApplication::postEvent(m_app, new UrlEvent(m_exportedFile, 10003));
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
	    		QApplication::postEvent(m_app, new ProgressEvent(fileProgress, 10006));
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
    QApplication::postEvent(m_app, new QCustomEvent(10003));
    if (droppedFrames > 0) KMessageBox::sorry(0, i18n("Transmission of dv file is finished.\n%1 frames were dropped during transfer.").arg(droppedFrames));
    else KMessageBox::information(0, i18n("Transmission of dv file finished successfully."));
#else
KMessageBox::sorry(0, i18n("Firewire is not enabled on your system.\n Please install Libiec61883 and recompile Kdenlive"));
#endif
}


void KRender::exportCurrentFrame(KURL url, bool notify) {
    m_mltProducer->set_speed(0.0);

    QString frequency;
    if (m_fileRenderer) {
        delete m_fileRenderer;
        m_fileRenderer = NULL;
    }
    if (m_mltFileProducer) {
        delete m_mltFileProducer;
        m_mltFileProducer = NULL;
    }
    if (notify) m_exportedFile = url;
    else m_exportedFile = QString::null;
    m_fileRenderer=new Mlt::Consumer("avformat");
    m_fileRenderer->set ("target",decodedString(url.path()));
    if (!KdenliveSettings::videoprofile().isEmpty()) 
	m_fileRenderer->set("profile", KdenliveSettings::videoprofile());
    m_fileRenderer->set ("real_time","0");
    m_fileRenderer->set ("progressive","1");
    m_fileRenderer->set ("vcodec","png");
    m_fileRenderer->set ("format","rawvideo");
    m_fileRenderer->set ("vframes","1");
    m_fileRenderer->set ("terminate_on_pause", 1);

    m_fileRenderer->listen("consumer-stopped", this, (mlt_listener) file_consumer_stopped);

    m_mltFileProducer = new Mlt::Producer(m_mltProducer->cut((int) m_mltProducer->position(), (int) m_mltProducer->position()));
    
    m_fileRenderer->connect(*m_mltFileProducer);
    m_mltFileProducer->set_speed(1.0);
    m_fileRenderer->start();
}


