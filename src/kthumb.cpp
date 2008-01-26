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

#include <qapplication.h>

#include <kio/netaccess.h>
#include <kdebug.h>
#include <klocale.h>
#include <kdenlivesettings.h>
#include <kfileitem.h>
#include <kmessagebox.h>

#include <mlt++/Mlt.h>

#include <qxml.h>
#include <qimage.h>
#include <qlabel.h>
#include <qthread.h>
#include <qapplication.h>



#include "renderer.h"
#include "kthumb.h"

/*
    void MyThread::init(KUrl url, QString target, double frame, double frameLength, int frequency, int channels, int arrayWidth)
    {
	stop_me = false;
	m_isWorking = false;
	f.setName(target);
	m_url = url;
	m_frame = frame;
	m_frameLength = frameLength;
	m_frequency = frequency;
	m_channels = channels;
	m_arrayWidth = arrayWidth;

    }

    bool MyThread::isWorking()
    {
	return m_isWorking;
    }

    void MyThread::run()
    {
		if (!f.open( IO_WriteOnly )) {
			kdDebug()<<"++++++++  ERROR WRITING TO FILE: "<<f.name()<<endl;
			kdDebug()<<"++++++++  DISABLING AUDIO THUMBS"<<endl;
			KdenliveSettings::setAudiothumbnails(false);
			return;
		}
		m_isWorking = true;
		char *tmp = KRender::decodedString(m_url.path());
		Mlt::Producer m_producer(tmp);
		delete tmp;

		if (KdenliveSettings::normaliseaudiothumbs()) {
    		    Mlt::Filter m_convert("volume");
    		    m_convert.set("gain", "normalise");
    		    m_producer.attach(m_convert);
		}

		if (qApp->mainWidget()) 
		    QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(-1, 10005));

		int last_val = 0;
		int val = 0;
		for (int z=(int) m_frame;z<(int) (m_frame+m_frameLength) && m_producer.is_valid();z++){
			if (stop_me) break;
			val=(int)((z-m_frame)/(m_frame+m_frameLength)*100.0);
			if (last_val!=val & val > 1){
				QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(val, 10005));
				last_val=val;
			}
				m_producer.seek( z );
				Mlt::Frame *mlt_frame = m_producer.get_frame();
				if ( mlt_frame->is_valid() )
				{
					double m_framesPerSecond = mlt_producer_get_fps( m_producer.get_producer() ); //mlt_frame->get_double( "fps" );
					int m_samples = mlt_sample_calculator( m_framesPerSecond, m_frequency, mlt_frame_get_position(mlt_frame->get_frame()) );
					mlt_audio_format m_audioFormat = mlt_audio_pcm;
				
					int16_t* m_pcm = mlt_frame->get_audio(m_audioFormat, m_frequency, m_channels, m_samples ); 

					for (int c=0;c< m_channels;c++){
						QByteArray m_array(m_arrayWidth);
						for (uint i = 0; i < m_array.size(); i++){
							m_array[i] =  QABS((*( m_pcm + c + i * m_samples / m_array.size() ))>>8);
						}
						f.writeBlock(m_array);
					}
				} else{
					f.writeBlock(QByteArray(m_arrayWidth));
				}
				if (mlt_frame)
					delete mlt_frame;
		}
		f.close();
		m_isWorking = false;
		if (stop_me) {
		    f.remove();
		    QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(-1, 10005));
		}
		else QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(0, 10005));
    }


#define _S(a)           (a)>255 ? 255 : (a)<0 ? 0 : (a)
#define _R(y,u,v) (0x2568*(y)                          + 0x3343*(u)) /0x2000
#define _G(y,u,v) (0x2568*(y) - 0x0c92*(v) - 0x1a1e*(u)) /0x2000
#define _B(y,u,v) (0x2568*(y) + 0x40cf*(v))                                          /0x2000
*/
KThumb::KThumb(KUrl url, QObject * parent, const char *name):QObject(parent), m_url(url)
{
}

KThumb::~KThumb()
{
    //if (thumbProducer.running ()) thumbProducer.exit();
}


//static
QPixmap KThumb::getImage(KUrl url, int width, int height)
{
    if (url.isEmpty()) return QPixmap();
    QPixmap pix(width, height);
    char *tmp = Render::decodedString(url.path());
    Mlt::Producer m_producer(tmp);
    delete tmp;

    if (m_producer.is_blank()) {
	pix.fill(Qt::black);
	return pix;
    }
    Mlt::Frame * m_frame;
    mlt_image_format format = mlt_image_rgb24a;
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    //m_producer.seek(frame);
    m_frame = m_producer.get_frame();
    if (m_frame && m_frame->is_valid()) {
      uint8_t *thumb = m_frame->get_image(format, width, height);
      QImage image(thumb, width, height, QImage::Format_ARGB32);
      if (!image.isNull()) {
	pix = pix.fromImage(image);
      }
      else pix.fill(Qt::black);
    }
    if (m_frame) delete m_frame;
    return pix;
}

void KThumb::extractImage(int frame, int frame2, int width, int height)
{
    if (m_url.isEmpty()) return;
    QPixmap pix(width, height);
    char *tmp = Render::decodedString(m_url.path());
    Mlt::Producer m_producer(tmp);
    delete tmp;

    if (m_producer.is_blank()) {
	pix.fill(Qt::black);
	emit thumbReady(frame, pix);
	return;
    }
    Mlt::Frame * m_frame;
    mlt_image_format format = mlt_image_rgb24a;
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    if (frame != -1) {
      m_producer.seek(frame);
      m_frame = m_producer.get_frame();
      if (m_frame && m_frame->is_valid()) {
	uint8_t *thumb = m_frame->get_image(format, width, height);
	QImage image(thumb, width, height, QImage::Format_ARGB32);
	if (!image.isNull()) {
	  pix = pix.fromImage(image);
	}
	else pix.fill(Qt::black);
      }
      if (m_frame) delete m_frame;
      emit thumbReady(frame, pix);
    }
    if (frame2 == -1) return;
    m_producer.seek(frame2);
    m_frame = m_producer.get_frame();
    if (m_frame && m_frame->is_valid()) {
      uint8_t *thumb = m_frame->get_image(format, width, height);
      QImage image(thumb, width, height, QImage::Format_ARGB32);
      if (!image.isNull()) {
	pix = pix.fromImage(image);
      }
      else pix.fill(Qt::black);
    }
    if (m_frame) delete m_frame;
    emit thumbReady(frame2, pix);

}
/*
void KThumb::getImage(KUrl url, int frame, int width, int height)
{
    if (url.isEmpty()) return;
    QPixmap image(width, height);
    char *tmp = KRender::decodedString(url.path());
    Mlt::Producer m_producer(tmp);
    delete tmp;
    image.fill(Qt::black);

    if (m_producer.is_blank()) {
	emit thumbReady(frame, image);
	return;
    }
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    m_producer.seek(frame);
    Mlt::Frame * m_frame = m_producer.get_frame();
    mlt_image_format format = mlt_image_rgb24a;
    width = width - 2;
    height = height - 2;
    if (m_frame && m_frame->is_valid()) {
    	uint8_t *thumb = m_frame->get_image(format, width, height);
    	QImage tmpimage(thumb, width, height, 32, NULL, 0, QImage::IgnoreEndian);
    	if (!tmpimage.isNull()) bitBlt(&image, 1, 1, &tmpimage, 0, 0, width + 2, height + 2);
    }
    if (m_frame) delete m_frame;
    emit thumbReady(frame, image);
}

void KThumb::getThumbs(KUrl url, int startframe, int endframe, int width, int height)
{
    if (url.isEmpty()) return;
    QPixmap image(width, height);
    char *tmp = KRender::decodedString(url.path());
    Mlt::Producer m_producer(tmp);
    delete tmp;
    image.fill(Qt::black);

    if (m_producer.is_blank()) {
	emit thumbReady(startframe, image);
	emit thumbReady(endframe, image);
	return;
    }
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    m_producer.seek(startframe);
    Mlt::Frame * m_frame = m_producer.get_frame();
    mlt_image_format format = mlt_image_rgb24a;
    width = width - 2;
    height = height - 2;

    if (m_frame && m_frame->is_valid()) {
    	uint8_t *thumb = m_frame->get_image(format, width, height);
    	QImage tmpimage(thumb, width, height, 32, NULL, 0, QImage::IgnoreEndian);
    	if (!tmpimage.isNull()) bitBlt(&image, 1, 1, &tmpimage, 0, 0, width - 2, height - 2);
    }
    if (m_frame) delete m_frame;
    emit thumbReady(startframe, image);

    image.fill(Qt::black);
    m_producer.seek(endframe);
    m_frame = m_producer.get_frame();

    if (m_frame && m_frame->is_valid()) {
    	uint8_t *thumb = m_frame->get_image(format, width, height);
    	QImage tmpimage(thumb, width, height, 32, NULL, 0, QImage::IgnoreEndian);
    	if (!tmpimage.isNull()) bitBlt(&image, 1, 1, &tmpimage, 0, 0, width - 2, height - 2);
    }
    if (m_frame) delete m_frame;
    emit thumbReady(endframe, image);
}

void KThumb::stopAudioThumbs()
{
	if (thumbProducer.running ()) thumbProducer.stop_me = true;
}


void KThumb::removeAudioThumb()
{
	if (m_thumbFile.isEmpty()) return;
	stopAudioThumbs();
	QFile f(m_thumbFile);
	f.remove();
}

void KThumb::getAudioThumbs(KUrl url, int channel, double frame, double frameLength, int arrayWidth){
	if ((thumbProducer.running () && thumbProducer.isWorking()) || channel == 0) {
	    return;
	}

	QMap <int, QMap <int, QByteArray> > storeIn;
       //FIXME: Hardcoded!!! 
	int m_frequency = 48000;
	int m_channels = channel;
	if (m_url != url) {
		m_url = url;
		KMD5 context ((KFileItem(m_url,"text/plain", S_IFREG).timeString() + m_url.fileName()).ascii());
		m_thumbFile = KdenliveSettings::currenttmpfolder() + context.hexDigest().data() + ".thumb";
	}
	QFile f(m_thumbFile);
	if (f.open( IO_ReadOnly )) {
		QByteArray channelarray = f.readAll();
		f.close();
		if (channelarray.size() != arrayWidth*(frame+frameLength)*m_channels) {
			kdDebug()<<"--- BROKEN THUMB FOR: "<<m_url.filename()<<" ---------------------- "<<endl;
			f.remove();
			return;
		}
		
		for (int z=(int) frame;z<(int) (frame+frameLength);z++) {
			for (int c=0;c< m_channels;c++){
				QByteArray m_array(arrayWidth);
				for (int i = 0; i < arrayWidth; i++)
					m_array[i] = channelarray[z*arrayWidth*m_channels + c*arrayWidth + i];
				storeIn[z][c] = m_array;
			}
		}
		emit audioThumbReady(storeIn);
	}
	else {
		if (thumbProducer.running()) return;
		thumbProducer.init(m_url, m_thumbFile, frame, frameLength, m_frequency, m_channels, arrayWidth);
		thumbProducer.start(QThread::LowestPriority );
	}
}
*/


