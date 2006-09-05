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
#include <kmdcodec.h>
#include <kmessagebox.h>

#include <mlt++/Mlt.h>

#include <qxml.h>
#include <qimage.h>
#include <qlabel.h>
#include <qapplication.h>
#include <qprogressbar.h>
#include <qthread.h>

#include <loadprogress_ui.h>

#include "krender.h"
#include "kthumb.h"
#include "kdenlive.h"

KThumb::KThumb(QObject * parent, const char *name):QObject(parent,
    name), m_workingOnAudio(false)
{
}

KThumb::~KThumb()
{
}


void KThumb::getImage(KURL url, int frame, int width, int height)
{
    if (url.isEmpty()) return;
    QPixmap image(width, height);
    image.fill(Qt::black);

    Mlt::Producer m_producer(KRender::decodedString(url.path()));

    if (m_producer.is_blank()) {
	emit thumbReady(frame, image);
	return;
    }
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    m_producer.seek(frame);
    uint orig_width = width - 2;
    uint orig_height = height - 2;
    Mlt::Frame * m_frame = m_producer.get_frame();

    if (m_frame) {
	m_frame->set("rescale", "nearest");
	uchar *m_thumb = m_frame->fetch_image(mlt_image_rgb24a, orig_width, orig_height, 1);
	m_producer.set("thumb", m_thumb, orig_width * orig_height * 4,
	    mlt_pool_release);
	m_frame->set("image", m_thumb, 0, NULL, NULL);

	QImage m_image(m_thumb, orig_width, orig_height, 32, 0, 0, QImage::IgnoreEndian);

	delete m_frame;
	if (!m_image.isNull())
	    bitBlt(&image, 1, 1, &m_image, 0, 0, orig_width, orig_height);
	else
	    image.fill(Qt::black);
    }
    emit thumbReady(frame, image);
}

void KThumb::getAudioThumbs(KURL url, int channel, double frame, double frameLength, int arrayWidth){
	QMap <int, QMap <int, QByteArray> > storeIn;

	if (m_workingOnAudio) return;
	
       //FIXME: Hardcoded!!! 
	int m_frequency = 48000;
	int m_channels = channel; 
	KMD5 context ((KFileItem(url,"text/plain", S_IFREG).timeString() + url.fileName()).ascii());
	QString thumbname = KdenliveSettings::currentdefaultfolder() + "/" + context.hexDigest().data() + ".thumb";
	kdDebug()<<"+++++++++ THUMB GET READY FOR: "<<thumbname<<endl;
	QFile f(thumbname);
	if (f.open( IO_ReadOnly )) {
		//kdDebug()<<"--- READING AUDIO THUMB: "<<url.filename()<<", arrayW: "<<arrayWidth<<endl;
		//QByteArray channelarray(arrayWidth*(frame+frameLength)*m_channels);
		QByteArray channelarray = f.readAll();
		f.close();
		if (channelarray.size() != arrayWidth*(frame+frameLength)*m_channels) {
			kdDebug()<<"--- BROKEN THUMB FOR: "<<url.filename()<<" ---------------------- "<<endl;
			f.remove();
			return;

		}
		
		for (int z=frame;z<frame+frameLength;z++) {
			//kdDebug() << "frame=" << z << ", total: "<< frame+frameLength <<endl;
			for (int c=0;c< m_channels;c++){
				QByteArray m_array(arrayWidth);
				for (int i = 0; i < arrayWidth; i++)
					m_array[i] = channelarray[z*arrayWidth*m_channels + c*arrayWidth + i];
				storeIn[z][c] = m_array;
			}
		}
	}
	else {
		m_workingOnAudio = true;
		if (!f.open( IO_WriteOnly )) {
			kdDebug()<<"++++++++  ERROR WRITING TO FILE: "<<thumbname<<endl;
			kdDebug()<<"++++++++  DISABLING AUDIO THUMBS"<<endl;
			KdenliveSettings::setAudiothumbnails(false);
			return;
		}
		

		Mlt::Producer m_producer(KRender::decodedString(url.path()));
		QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(-1, 10005));

		int last_val = 0;
		int val = 0;
		for (int z=frame;z<frame+frameLength && m_producer.is_valid();z++){
			qApp->processEvents();
			
			val=(int)((z-frame)/(frame+frameLength)*100.0);
			if (last_val!=val){
				QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(val - last_val, 10005));
				last_val=val;
			}
			
			if (storeIn.find(z)==storeIn.end()){
			
				m_producer.seek( z );
				Mlt::Frame *m_frame = m_producer.get_frame();
				if ( m_frame->is_valid() )
				{
					double m_framesPerSecond = m_frame->get_double( "fps" );
					int m_samples = mlt_sample_calculator( m_framesPerSecond, m_frequency, 
							mlt_frame_get_position(m_frame->get_frame()) );
					mlt_audio_format m_audioFormat = mlt_audio_pcm;
				
					int16_t* m_pcm = m_frame->get_audio(m_audioFormat, m_frequency, m_channels, m_samples ); 

					for (int c=0;c< m_channels;c++){
						QByteArray m_array(arrayWidth);
						for (int i = 0; i < m_array.size(); i++){
							m_array[i] =  QABS((*( m_pcm + c + i * m_samples / m_array.size() ))>>8);
						}
						qApp->processEvents();
						f.writeBlock(m_array);
						storeIn[z][c]=m_array;
					}
				} else{
					storeIn[z][0]=QByteArray(arrayWidth);
					f.writeBlock(QByteArray(arrayWidth));
				}
				if (m_frame)
					delete m_frame;
			}
		}
		f.close();
		m_workingOnAudio = false;
		if (val != 100) QApplication::postEvent(qApp->mainWidget(), new ProgressEvent(100 - val, 10005));
	}
	emit audioThumbReady(storeIn);
}
