/***************************************************************************
                        krender.cpp  -  description
                           -------------------
  begin                : Fri Nov 22 2002
  copyright            : (C) 2002 by Jason Wood
  email                : jasonwood@blueyonder.co.uk
  copyright            : (C) 2005 Lcio Fl�io Corr�	
  email                : lucio.correa@gmail.com
  copyright            : (C) Marco Gittler
  email                : g.marco@freenet.de

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
#include <sys/wait.h>

#include <qcolor.h>
#include <qpixmap.h>
#include <qxml.h>
#include <qapplication.h>
#include <qimage.h>
#include <qmutex.h>
#include <qevent.h>
#include <qtextstream.h>

#include <kio/netaccess.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <klocale.h>

#include "krender.h"
#include "avformatdescbool.h"
#include "avformatdesclist.h"
#include "avformatdesccontainer.h"
#include "avformatdesccodeclist.h"
#include "avformatdesccodec.h"
#include "effectparamdesc.h"

static QMutex mutex (true);

int m_refCount;

namespace {

    class KRenderThread:public QThread {
      protected:
	virtual void run() {
	    while (true) {
	    };
	}
      private:

    };

    static KRenderThread *s_renderThread = NULL;

}				// annonymous namespace

KRender::KRender(const QString & rendererName, KURL appPath,
                 unsigned int port, Gui::KdenliveApp *parent, const char *name):QObject(parent,
                 name), m_name(rendererName), m_renderName("unknown"), m_app(parent), 
m_renderVersion("unknown"), m_appPathInvalid(false), m_fileFormat(0),
m_desccodeclist(0), m_codec(0), m_effect(0), m_playSpeed(0.0),
m_parameter(0), m_portNum(0), m_appPath(""), m_mltMiracle(NULL),
m_mltConsumer(NULL), m_mltProducer(NULL), m_fileRenderer(NULL)
{
    startTimer(1000);
    m_parsing = false;
    m_setSceneListPending = false;

    m_fileFormats.setAutoDelete(true);
    m_codeclist.setAutoDelete(true);
    m_effectList.setAutoDelete(true);
/*
	connect( &m_socket, SIGNAL( error( int ) ), this, SLOT( error( int ) ) );
	connect( &m_socket, SIGNAL( connected() ), this, SLOT( slotConnected() ) );
	connect( &m_socket, SIGNAL( connectionClosed() ), this, SLOT( slotDisconnected() ) );
	connect( &m_socket, SIGNAL( readyRead() ), this, SLOT( readData() ) );

	connect( &m_process, SIGNAL( processExited( KProcess * ) ), this, SLOT( processExited() ) );
	connect( &m_process, SIGNAL( receivedStdout( KProcess *, char *, int ) ), this, SLOT( slotReadStdout( KProcess *, char *, int ) ) );
	connect( &m_process, SIGNAL( receivedStderr( KProcess *, char *, int ) ), this, SLOT( slotReadStderr( KProcess *, char *, int ) ) );
*/

    m_portNum = port;
    m_appPath = appPath;
    openMlt();


    // Build effects. We should find a more elegant way to do it, and ultimately 
    // retrieve it directly from mlt
    QXmlAttributes xmlAttr;

    EffectDesc *grey = new EffectDesc(i18n("Greyscale"), "greyscale");
    xmlAttr.append("type", QString::null, QString::null, "fixed");
    m_parameter = m_effectDescParamFactory.createParameter(xmlAttr);
    grey->addParameter(m_parameter);
    m_effectList.append(grey);

    EffectDesc *invert = new EffectDesc(i18n("Invert"), "invert");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "fixed");
    m_parameter = m_effectDescParamFactory.createParameter(xmlAttr);
    invert->addParameter(m_parameter);
    m_effectList.append(invert);

    EffectDesc *sepia = new EffectDesc(i18n("Sepia"), "sepia");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "u");
    xmlAttr.append("description", QString::null, QString::null,
	"The U parameter");
    xmlAttr.append("max", QString::null, QString::null, "255");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "75");
    m_parameter = m_effectDescParamFactory.createParameter(xmlAttr);
    sepia->addParameter(m_parameter);
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "v");
    xmlAttr.append("max", QString::null, QString::null, "255");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "150");
    m_parameter = m_effectDescParamFactory.createParameter(xmlAttr);
    sepia->addParameter(m_parameter);
    m_effectList.append(sepia);

    EffectDesc *charcoal = new EffectDesc(i18n("Charcoal"), "charcoal");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "x_scatter");
    xmlAttr.append("max", QString::null, QString::null, "10");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "2");
    m_parameter = m_effectDescParamFactory.createParameter(xmlAttr);
    charcoal->addParameter(m_parameter);
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "y_scatter");
    xmlAttr.append("max", QString::null, QString::null, "10");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "2");
    m_parameter = m_effectDescParamFactory.createParameter(xmlAttr);
    charcoal->addParameter(m_parameter);
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "scale");
    xmlAttr.append("max", QString::null, QString::null, "10");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "1");
    m_parameter = m_effectDescParamFactory.createParameter(xmlAttr);
    charcoal->addParameter(m_parameter);
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "mix");
    xmlAttr.append("max", QString::null, QString::null, "10");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "0");
    m_parameter = m_effectDescParamFactory.createParameter(xmlAttr);
    charcoal->addParameter(m_parameter);
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "invert");
    xmlAttr.append("max", QString::null, QString::null, "1");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "1");
    m_parameter = m_effectDescParamFactory.createParameter(xmlAttr);
    charcoal->addParameter(m_parameter);
    m_effectList.append(charcoal);


    EffectDesc *bright = new EffectDesc(i18n("Brightness"), "brightness");
    xmlAttr.clear();

    xmlAttr.append("type", QString::null, QString::null, "double");
    xmlAttr.append("name", QString::null, QString::null, "Intensity");
    xmlAttr.append("max", QString::null, QString::null, "3");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "1");
    m_parameter = m_effectDescParamFactory.createParameter(xmlAttr);
    bright->addParameter(m_parameter);
    m_effectList.append(bright);

    EffectDesc *volume = new EffectDesc(i18n("Volume"), "volume");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "double");
    xmlAttr.append("name", QString::null, QString::null, "gain");
    xmlAttr.append("starttag", QString::null, QString::null, "gain");
    xmlAttr.append("max", QString::null, QString::null, "3");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "1");
    m_parameter = m_effectDescParamFactory.createParameter(xmlAttr);
    volume->addParameter(m_parameter);
    m_effectList.append(volume);
    
    EffectDesc *mute = new EffectDesc(i18n("Mute"), "volume");
    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "constant");
    xmlAttr.append("name", QString::null, QString::null, "gain");
    xmlAttr.append("max", QString::null, QString::null, "0");
    xmlAttr.append("min", QString::null, QString::null, "0");
    xmlAttr.append("default", QString::null, QString::null, "0");
    m_parameter = m_effectDescParamFactory.createParameter(xmlAttr);
    mute->addParameter(m_parameter);
    m_effectList.append(mute);


    EffectDesc *obscure = new EffectDesc(i18n("Obscure"), "obscure");

    xmlAttr.clear();
    xmlAttr.append("type", QString::null, QString::null, "complex");
    xmlAttr.append("name", QString::null, QString::null,
	"X;Y;Width;Height;Averaging");
    xmlAttr.append("min", QString::null, QString::null, "0;0;0;0;3");
    xmlAttr.append("max", QString::null, QString::null,
	"720;576;1000;1000;100");
    xmlAttr.append("default", QString::null, QString::null,
	"360;260;100;100;20");
    m_parameter = m_effectDescParamFactory.createParameter(xmlAttr);
    obscure->addParameter(m_parameter);
    m_effectList.append(obscure);


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
    killTimers();
    quit();
}

/** Recieves timer events */
void KRender::timerEvent(QTimerEvent * event)
{
    if (m_mltConsumer == NULL) {
	emit initialised();
	emit connected();
    }
    //if ( m_socket.state() == QSocket::Idle ) {
    //if ( !m_process.isRunning() ) {
    //if(m_refCount==1){
/*			if (m_mltConsumer==NULL){
				launchProcess();
				emit slotConnected();
			}*/
    //}
    //} else {
    //      m_socket.connectToHost( "127.0.0.1", m_portNum );
    //}
    //}
}

/** Catches errors from the socket. */
void KRender::error(int error)
{
    switch (error) {
    case QSocket::ErrConnectionRefused:
	emit renderWarning(m_name, "Connection Refused");
	break;
    case QSocket::ErrHostNotFound:
	emit renderWarning(m_name, "Host Not Found");
	break;
    case QSocket::ErrSocketRead:
	emit renderWarning(m_name, "Error Reading Socket");
	break;
    }
}

/** Called when we have connected to the renderer. */
void KRender::slotConnected()
{
    getCapabilities();

    emit renderDebug(m_name,
	"Connected on port " + QString::number(m_socket.port()) +
	" to host on port " + QString::number(m_socket.peerPort()));
    emit initialised();
    emit connected();
}

/** Called when we have disconnected from the renderer. */
void KRender::slotDisconnected()
{
    emit renderWarning(m_name, "Disconnected");

    emit disconnected();
}

/** Called when some data has been recieved by the socket, reads the data and processes it. */
void KRender::readData()
{
}

/** Sends an XML command to the renderer. */
void KRender::sendCommand(QDomDocument command)
{
}

/** Generates the quit command */
void KRender::quit()
{
}

/** Called if the rendering process has exited. */
void KRender::processExited()
{
    emit renderWarning(m_name, "Render Process Exited");
}

/** Launches a renderer process. */
void KRender::launchProcess()
{
//   Removed some obsolete code that was responsible for artsd launch,
//   but wasn't used anymore.
    emit renderWarning(m_name, "Render should start");
}

void KRender::slotReadStdout(KProcess * proc, char *buffer, int buflen)
{
    QString mess;
    mess.setLatin1(buffer, buflen);
    emit recievedStdout(m_name, mess);
}

void KRender::slotReadStderr(KProcess * proc, char *buffer, int buflen)
{
    QString mess;
    mess.setLatin1(buffer, buflen);
    emit recievedStderr(m_name, mess);
}

/** Returns a list of all available file formats in this renderer. */
QPtrList < AVFileFormatDesc > &KRender::fileFormats()
{
    return m_fileFormats;
}



void KRender::openMlt()
{

    m_refCount++;
    if (m_refCount == 1) {
	Mlt::Factory::init();
	m_mltMiracle = new Mlt::Miracle("miracle", 5250);
	m_mltMiracle->start();
	std::cout << "Mlt inited" << std::endl;
    }
}

void KRender::closeMlt()
{
    m_refCount--;
    if (m_refCount == 1) {
	//m_mltMiracle->wait_for_shutdown();
	//delete(m_mltMiracle);
    }
    if (m_mltConsumer)
	delete m_mltConsumer;
    if (m_mltProducer);
        delete m_mltProducer;

}

static void consumer_frame_show(mlt_consumer, KRender * self,
    mlt_frame frame_ptr)
{
    mlt_position framePosition = mlt_frame_get_position(frame_ptr);
    self->emitFrameNumber(GenTime(framePosition, 25), false);
    // detect if the producer has finished playing. Is there a better way to do it ?
    if (mlt_properties_get_double( MLT_FRAME_PROPERTIES( frame_ptr ), "_speed" ) == 0)
        self->emitConsumerStopped();
}

static void file_consumer_frame_show(mlt_consumer, KRender * self,
                                mlt_frame frame_ptr)
{
    mlt_position framePosition = mlt_frame_get_position(frame_ptr);
    self->emitFrameNumber(GenTime(framePosition, 25), true);
    // detect if the producer has finished playing. Is there a better way to do it ?
    if (mlt_properties_get_double( MLT_FRAME_PROPERTIES( frame_ptr ), "_speed" ) == 0)
        self->emitFileConsumerStopped();
}

static void consumer_stopped(mlt_consumer, KRender * self,
                                mlt_frame)
{
    self->emitConsumerStopped();
}

static void file_consumer_stopped(mlt_consumer, KRender * self,
                             mlt_frame)
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

void KRender::createVideoXWindow(bool show, WId winid)
{


    m_mltConsumer = new Mlt::Consumer("sdl_preview");
    m_mltConsumer->listen("consumer-frame-show", this,
	(mlt_listener) consumer_frame_show);
    //m_mltConsumer->listen("consumer-stopped", this, (mlt_listener) consumer_stopped);

    //only as is saw, if we want to lock something with the sdl lock

    m_mltConsumer->set("app_locked", 1);
    m_mltConsumer->set("app_lock", (void *) &my_lock, 0);

    m_mltConsumer->set("app_unlock", (void *) &my_unlock, 0);
    m_mltConsumer->set("window_id", (int) winid);

    m_mltConsumer->set("resize", 1);
    //m_mltConsumer->set("audio_driver","dsp");

    m_mltConsumer->set("progressiv", 1);
//      m_mltConsumer->start ();

}

/** Wraps the VEML command of the same name; Seeks the renderer clip to the given time. */
void KRender::seek(GenTime time)
{
    sendSeekCommand(time);
    //emit positionChanged(time);
}


void KRender::getImage(KURL url, int frame, QPixmap * image)
{
    Mlt::Producer m_producer(const_cast<char*>(url.path().ascii()));
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    m_producer.seek(frame);
    
    uint width = image->width();
    uint height = image->height();

    Mlt::Frame * m_frame = m_producer.get_frame();

    if (m_frame) {
	m_frame->set("rescale", "nearest");
	uchar *m_thumb = m_frame->fetch_image(mlt_image_rgb24a, width, height, 1);

        // what's the use of this ? I don't think we need it - commented out by jbm 
	m_producer.set("thumb", m_thumb, width * height * 4, mlt_pool_release);
        m_frame->set("image", m_thumb, 0, NULL, NULL);

	QImage m_image(m_thumb, width, height, 32, 0, 0, QImage::IgnoreEndian);

	delete m_frame;
	if (!m_image.isNull())
	    *image = (m_image.smoothScale(width, height));
	else
	    image->fill(Qt::black);
    }
}


/**
Filles a ByteArray with soundsampledata for channel, from frame , with a length of frameLength (zoom) up to the length of the array
*/

void KRender::getSoundSamples(const KURL & url, int channel, int frame,
    double frameLength, int arrayWidth, int x, int y, int h, int w,
    QPainter & painter)
{

    /*Mlt::Producer m_producer(const_cast<char*>((url.directory(false)+url.fileName()).ascii()));
       m_producer.seek( frame );
       Mlt::Frame *m_frame = m_producer.get_frame();

       //FIXME: Hardcoded!!! 
       int m_frequency = 32000;
       int m_channels = 2; */

    QByteArray m_array(arrayWidth);

    /*if ( m_frame->is_valid() )
       {
       double m_framesPerSecond = m_frame->get_double( "fps" );
       int m_samples = mlt_sample_calculator( m_framesPerSecond, m_frequency, 
       mlt_frame_get_position(m_frame->get_frame()) );
       mlt_audio_format m_audioFormat = mlt_audio_pcm;

       int16_t* m_pcm = m_frame->get_audio( m_audioFormat, m_frequency, m_channels, m_samples ); */

    for (int i = 0; i < m_array.size(); i++)
	m_array[i] = 0;
    //m_array[i]= *( m_pcm + channel + i * m_samples / m_array.size() );
    /*if (m_pcm)
       delete m_pcm;
       } */

    emit replyGetSoundSamples(url, channel, frame, frameLength, m_array, x,
	y, h, w, painter);
}


void KRender::getImage(KURL url, int frame, int width, int height)
{
    Mlt::Producer m_producer(const_cast <
	char *>(QString((url.directory(false) +
		    url.fileName())).ascii()));

    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    m_producer.seek(frame);

    Mlt::Frame * m_frame = m_producer.get_frame();

    if (m_frame) {
	m_frame->set("rescale", "nearest");
	/*double ratio = (double) height / m_frame->get_int("height");
	   double overSize = 1.0; */
	/* if we want a small thumbnail, oversize image a little bit and smooth rescale after, gives better results */
	//if (ratio < 0.3) overSize = 1.4;


	uchar *m_thumb =
	    m_frame->fetch_image(mlt_image_rgb24a, width, height, 1);
	/*m_producer.set("thumb", m_thumb, width * height * 4,
	    mlt_pool_release);
        m_frame->set("image", m_thumb, 0, NULL, NULL);*/

	QPixmap m_pixmap(width, height);

	QImage m_image(m_thumb, width, height, 32, 0, 0,
	    QImage::IgnoreEndian);

	delete m_frame;
	if (!m_image.isNull())
	    m_pixmap = m_image.smoothScale(width, height);
	else
	    m_pixmap.fill(Qt::black);

	//m_pixmap.convertFromImage( m_image );
	emit replyGetImage(url, frame, m_pixmap, width, height);
    }

}

/* Create thumbnail for text clip */
void KRender::getImage(int id, QString txt, uint size, int width, int height)
{
    Mlt::Producer m_producer("pango");
    m_producer.set("markup", txt.ascii());
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    m_producer.seek(1);
    Mlt::Frame * m_frame = m_producer.get_frame();

    if (m_frame) {
        m_frame->set("rescale", "nearest");
	/*double ratio = (double) height / m_frame->get_int("height");
        double overSize = 1.0; */
        /* if we want a small thumbnail, oversize image a little bit and smooth rescale after, gives better results */
	//if (ratio < 0.3) overSize = 1.4;


        uchar *m_thumb =
                m_frame->fetch_image(mlt_image_rgb24a, width, height, 1);
        /*
        m_producer.set("thumb", m_thumb, width * height * 4,
                       mlt_pool_release);
        m_frame->set("image", m_thumb, 0, NULL, NULL);*/

        QPixmap m_pixmap(width, height);

        QImage m_image(m_thumb, width, height, 32, 0, 0,
                       QImage::IgnoreEndian);

        delete m_frame;
        if (!m_image.isNull())
            m_pixmap = m_image.smoothScale(width, height);
        else
            m_pixmap.fill(Qt::black);

	//m_pixmap.convertFromImage( m_image );
        emit replyGetImage(id, m_pixmap, width, height);
    }

}

/* Create thumbnail for color */
void KRender::getImage(int id, QString color, int width, int height)
{
    QPixmap pixmap(width, height);
    color = color.replace(0, 2, "#");
    color = color.left(7);
    pixmap.fill(QColor(color));

    emit replyGetImage(id, pixmap, width, height);

}

/* Create thumbnail for image */
void KRender::getImage(KURL url, int width, int height)
{
    QPixmap pixmap(url.path());
    QImage im;
    im = pixmap;
    pixmap = im.smoothScale(width, height);

    emit replyGetImage(url, 1, pixmap, width, height);
}

void KRender::getImage(DocClipRef * clip)
{
    QPixmap pixmap(clip->fileURL().path());
    QImage im;
    im = pixmap;
    pixmap = im.smoothScale(63, 50);

    clip->updateThumbnail(0,pixmap);
}

void KRender::setTitlePreview(QString tmpFileName)
{

    m_mltConsumer->stop();
    int pos = 0;

	// If there is no clip in the monitor, use a black video as first track	
    if(m_mltProducer == NULL) {
        QString ctext2;
        ctext2="<producer><property name=\"mlt_service\">colour</property><property name=\"colour\">black</property></producer>";
        m_mltProducer = new Mlt::Producer ("westley-xml",const_cast<char*>(ctext2.ascii()));
    }
    else pos = m_mltProducer->position();
	
    // Create second producer with the png image created by the titler
    QString ctext;
    ctext="<producer><property name=\"resource\">"+tmpFileName+"</property></producer>";
    Mlt::Producer prod2("westley-xml",const_cast<char*>(ctext.ascii()));
    Mlt::Tractor tracks;

	// Add composite transition for overlaying the 2 tracks
    Mlt::Transition convert( "composite" ); 
    //convert.set("geometry","0,0:100%x100%:60");
    convert.set("distort",1);
    convert.set("progressive",1);
    convert.set("always_active",1);
	
	// Define the 2 tracks
    tracks.set_track(*m_mltProducer,0);
    tracks.set_track(prod2,1);
    tracks.plant_transition( convert ,0,1);
    tracks.seek(pos);	

	// Start playing preview
    
    
    //refresh();
    //m_mltConsumer->lock();
    tracks.set_speed(0.0);
    m_mltConsumer->connect(tracks);
    m_mltConsumer->start();
    refresh();
    
    //m_mltConsumer->unlock();
}

bool KRender::isValid(KURL url)
{
    Mlt::Producer producer(const_cast < char *>(url.path().ascii()));
    if (producer.is_blank())
	return false;

    return true;
}


void KRender::getFileProperties(KURL url)
{
    if (!rendererOk()) {
	emit replyErrorGetFileProperties(url.path(),
	    i18n
	    ("The renderer is unavailable, the file properties cannot be determined."));
    } else {
	Mlt::Producer producer(const_cast < char *>(url.path().ascii()));

	m_filePropertyMap.clear();
	m_filePropertyMap["filename"] = url.path();
	m_filePropertyMap["duration"] =
	    QString::number(producer.get_length());
        
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
	    if (frame->get_int("test_image") == 0) {
		if (frame->get_int("test_audio") == 0)
		    m_filePropertyMap["type"] = "av";
		else
		    m_filePropertyMap["type"] = "video";
                
                // Generate thumbnail for this frame
                uint width = 50;
                uint height = 40;
                uchar *m_thumb = frame->fetch_image(mlt_image_rgb24a, width, height, 1);
                QPixmap m_pixmap(width, height);
                QImage m_image(m_thumb, width, height, 32, 0, 0,
                               QImage::IgnoreEndian);
                if (!m_image.isNull())
                    m_pixmap = m_image.smoothScale(width, height);
                else
                    m_pixmap.fill(Qt::black);
                emit replyGetImage(url, 0, m_pixmap, width, height);
                
	    } else if (frame->get_int("test_audio") == 0)
		m_filePropertyMap["type"] = "audio";
	}
	emit replyGetFileProperties(m_filePropertyMap);
	delete frame;

    }
}

/** Wraps the VEML command of the same name. Sets the current scene list to
be list. */
void KRender::setSceneList(QDomDocument list, bool resetPosition)
{
    GenTime pos = seekPosition();
    m_sceneList = list;
    m_setSceneListPending = true;

    if (m_mltProducer != NULL) {
	delete m_mltProducer;
	m_mltProducer = NULL;
	emit stopped();
    }
    m_mltProducer =
	new Mlt::Producer("westley-xml",
	const_cast < char *>(list.toString().ascii()));
    if (!resetPosition)
	seek(pos);
    m_mltProducer->set_speed(0.0);
    //if (m_mltConsumer) 
    {
	m_mltConsumer->start();
	m_mltConsumer->connect(*m_mltProducer);
	refresh();
    }
}

/** Wraps the VEML command of the same name - sends a <ping> command to the server, which
should reply with a <pong> - let's us determine the round-trip latency of the connection. */
void KRender::ping(QString & ID)
{
    QDomDocument doc;
    QDomElement elem = doc.createElement("ping");
    elem.setAttribute("id", ID);
    doc.appendChild(elem);
    sendCommand(doc);
}

void KRender::start()
{
    if (m_mltConsumer && m_mltConsumer->is_stopped()) {
	m_mltConsumer->start();
//refresh();
    }
}

void KRender::stop()
{
    if (m_mltProducer) {
	m_mltProducer->set_speed(0.0);
    }

    if (m_mltConsumer && !m_mltConsumer->is_stopped()) {
	m_mltConsumer->stop();
    }
}


void KRender::stop(const GenTime & startTime)
{

    if (m_mltProducer) {
	m_mltProducer->set_speed(0.0);
    }
//refresh();
}

void KRender::play(double speed)
{
    if (!m_mltProducer)
	return;

    m_playSpeed = speed;
    if (m_setSceneListPending) {
	sendSetSceneListCommand(m_sceneList);
    }
    m_mltProducer->set_speed(speed);
    refresh();
    /*if(m_playSpeed != 0.0) {
       m_mltProducer->set_speed(1.0);
       } else {
       m_mltProducer->set_speed(0.0);
       } */
}

void KRender::play(double speed, const GenTime & startTime)
{
    if (!m_mltProducer)
	return;

    m_playSpeed = speed;
    if (m_setSceneListPending) {
	sendSetSceneListCommand(m_sceneList);
    }
    m_mltProducer->set_speed(speed);
    m_mltProducer->seek((int) (startTime.frames(m_mltProducer->
		get_double("fps"))));
    refresh();

    /*if(m_playSpeed != 0.0) {
       m_mltProducer->set_speed(1.0);
       } else {
       m_mltProducer->set_speed(0.0);
       } */
}

void KRender::play(double speed, const GenTime & startTime,
    const GenTime & stopTime)
{
    if (!m_mltProducer)
	return;

    m_playSpeed = speed;

    if (m_setSceneListPending) {
	sendSetSceneListCommand(m_sceneList);
    }

    /* FIXME: Currently, only the startTime is considered. Playing will not stop at stopTime. Should find a clever way to do this... */

    m_mltProducer->set_speed(speed);
    m_mltProducer->seek((int) (startTime.frames(m_mltProducer->
		get_double("fps"))));
    refresh();

    /*if(m_playSpeed != 0.0) {
       m_mltProducer->set_speed(1.0);
       } else {
       m_mltProducer->set_speed(0.0);
       } */
}

void KRender::render(const KURL & url)
{
    if (m_setSceneListPending) {
	sendSetSceneListCommand(m_sceneList);
    }
    QDomDocument doc;
    QDomElement elem = doc.createElement("render");
    elem.setAttribute("filename", url.path());
    doc.appendChild(elem);
    sendCommand(doc);
}

void KRender::sendSeekCommand(GenTime time)
{

    if (!m_mltProducer)
	return;
    m_mltProducer->seek((int) (time.frames(m_mltProducer->
		get_double("fps"))));
    refresh();

    /*if ( m_setSceneListPending ) {
       sendSetSceneListCommand( m_sceneList );
       }

       QDomDocument doc;
       QDomElement elem = doc.createElement( "seek" );
       elem.setAttribute( "time", QString::number( time.seconds() ) );
       doc.appendChild( elem );
       sendCommand( doc ); */

    m_seekPosition = time;
}

void KRender::refresh()
{
    if (m_mltConsumer) {
	m_mltConsumer->lock();
	m_mltConsumer->set("refresh", 1);
	m_mltConsumer->unlock();
    }
}

void KRender::sendSetSceneListCommand(const QDomDocument & list)
{
    m_setSceneListPending = false;

    QDomDocument doc;
    QDomElement elem = doc.createElement("setSceneList");
    elem.appendChild(doc.importNode(list.documentElement(), true));
    doc.appendChild(elem);
    sendCommand(doc);
}

void KRender::getCapabilities()
{
    QDomDocument doc;
    QDomElement elem = doc.createElement("getCapabilities");
    doc.appendChild(elem);
    sendCommand(doc);
}

void KRender::pushIgnore()
{
    StackValue val;
    val.element = "ignore";
    val.funcStartElement = 0;
    val.funcEndElement = 0;
    m_parseStack.push(val);
}

// Pushes a value onto the stack.
void KRender::pushStack(QString element,
    bool(KRender::*funcStartElement) (const QString & localName,
	const QString & qName, const QXmlAttributes & atts),
    bool(KRender::*funcEndElement) (const QString & localName,
	const QString & qName))
{
    StackValue val;
    val.element = element;
    val.funcStartElement = funcStartElement;
    val.funcEndElement = funcEndElement;
    m_parseStack.push(val);
}

/** Returns the codec with the given name */
AVFormatDescCodec *KRender::findCodec(const QString & name)
{
    QPtrListIterator < AVFormatDescCodec > itt(m_codeclist);

    while (itt.current()) {
	emit renderWarning(m_name,
	    "Comparing " + name + " with " + itt.current()->name());
	if (name == itt.current()->name())
	    return itt.current();
	++itt;
    }
    return 0;
}

/** Returns the effect list. */
const EffectDescriptionList & KRender::effectList() const
{
    return m_effectList;
}

/** Sets the renderer version for this renderer. */
void KRender::setVersion(QString version)
{
    m_version = version;
}

/** Returns the renderer version. */
QString KRender::version()
{
    return m_version;
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

/** Occurs upon starting to parse an XML document */
bool KRender::startDocument()
{
    //  emit renderDebug(m_name, "Starting to parse document");
    return true;
}

/** Occurs upon finishing reading an XML document */
bool KRender::endDocument()
{
    //  emit renderDebug(m_name, "Finishing parsing document");
    return true;
}

bool KRender::rendererOk()
{
    //if ( m_appPathInvalid ) return false;

    return true;
}

double KRender::playSpeed()
{
    return m_playSpeed;
}

const GenTime & KRender::seekPosition() const
{
    return m_seekPosition;
}

void KRender::sendDebugVemlCommand(const QString & name)
{
    if (m_socket.state() == QSocket::Connected) {
	kdWarning() << "Sending debug command " << name << endl;
	QCString str = (name + "\n\n").latin1();
	m_socket.writeBlock(str, strlen(str));
    } else {
	emit renderWarning(m_name,
	    "Socket not connected, not sending Command " + name);
    }
}


const QString & KRender::rendererName() const
{
    return m_name;
}

void KRender::setCapture()
{
    QDomDocument doc;
    QDomElement elem = doc.createElement("setCapture");
    doc.appendChild(elem);

    sendCommand(doc);
}

void KRender::emitFrameNumber(const GenTime & time, bool isFile)
{
    m_seekPosition = time;
    QApplication::postEvent(m_app, new PositionChangeEvent(m_seekPosition, isFile));
}

void KRender::emitConsumerStopped()
{
    // This is used to know when the export is finished
    QApplication::postEvent(m_app, new QCustomEvent(10001));
}

void KRender::emitFileConsumerStopped()
{
    kdDebug()<<"+++++++++++  FILE CONSUMER STOPPING ++++++++++++++++++"<<endl;
    if (m_fileRenderer && !m_fileRenderer->is_stopped()) {
        mlt_properties_set_int( MLT_PRODUCER_PROPERTIES( m_fileRenderer->get_consumer() ), "done", 1 );
        delete m_fileRenderer;
        m_fileRenderer = 0;
    }
    // This is used when exporting to a file so that we know when the export is finished
         QApplication::postEvent(m_app, new QCustomEvent(10001));
}

/*                           FILE RENDERING STUFF                     */

#ifdef ENABLE_FIREWIRE
//  FIREWIRE EXPORT, REQUIRES LIBIECi61883
static int read_frame (unsigned char *data, int n, unsigned int dropped, void *callback_data)
{
    FILE *f = (FILE*) callback_data;

    if (n == 1)
        if (fread (data, 480, 1, f) < 1) {
        return -1;
        } else
            return 0;
            else
                return 0;
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
			
            if (select (fd + 1, &rfds, NULL, NULL, &tv) > 0)
                result = raw1394_loop_iterate (handle);
			
        } while (g_done == 0 && result == 0);
        fprintf (stderr, "done.\n");
    }
    iec61883_dv_close (dv);
}
#endif

void KRender::exportFileToFirewire(QString srcFileName, int port)
{
#ifdef ENABLE_FIREWIRE
    //exportTimeline(QString::null);
    kdDebug()<<"START DV EXPORT ++++++++++++++: "<<srcFileName<<endl;

    FILE *f = NULL;
    int oplug = -1, iplug = -1;
    f = fopen (srcFileName.ascii(), "rb");
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
#else
KMessageBox::sorry(0, i18n("Firewire is not enabled on your system.\n Please install Libiec61883 and recompile Kdenlive"));
#endif
}


void KRender::stopExport()
{
    if (m_fileRenderer && !m_fileRenderer->is_stopped()) {
        m_mltProducer->set_speed(0.0);
    }
}

void KRender::exportTimeline(const QString &url, const QString &format, const QString &videoSize, GenTime exportStart, GenTime exportEnd)
{
    kdDebug()<<"+++++++++  START EXPORT: "<<format<<endl;
    m_mltConsumer->stop();
    m_mltProducer->set_speed(0.0);
    if (m_fileRenderer) delete m_fileRenderer;
    m_fileRenderer = 0;
    if (format == "dv") m_fileRenderer=new Mlt::Consumer("libdv");
    else if (format == "mpeg") {
        m_fileRenderer=new Mlt::Consumer("avformat");
        m_fileRenderer->set ("format", "mpeg");
        m_fileRenderer->set ("vcodec", "mpeg2video");
        m_fileRenderer->set ("acodec", "mp3");
        if (videoSize != QString::null) m_fileRenderer->set("size",videoSize.ascii());
    }
    else if (format == "westley") {
        QFile *file = new QFile();
        file->setName(url);
        file->open(IO_WriteOnly);
        QTextStream stream( file );
        stream.setEncoding (QTextStream::UnicodeUTF8);
        stream << m_sceneList.toString();
        file->close();
        emitFileConsumerStopped();
        return;
    }
    
    m_fileRenderer->set ("target",url.ascii());
    m_fileRenderer->set ("real_time","0");
    
    m_fileRenderer->listen("consumer-frame-show", this, (mlt_listener) file_consumer_frame_show);
    m_fileRenderer->listen("consumer-stopped", this, (mlt_listener) file_consumer_stopped);
    
    m_fileRenderer->connect(*m_mltProducer);

    firstExportFrame = exportStart.frames(m_mltProducer->get_double("fps"));
    lastExportFrame = exportEnd.frames(m_mltProducer->get_double("fps"));
    exportDuration = lastExportFrame - firstExportFrame;

    m_mltProducer->seek(firstExportFrame);
    m_fileRenderer->start();
    m_mltProducer->set_speed(1.0);
}
