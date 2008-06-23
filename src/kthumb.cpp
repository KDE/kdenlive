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

#include <kio/netaccess.h>
#include <kdebug.h>
#include <klocale.h>
#include <kdenlivesettings.h>
#include <kfileitem.h>
#include <kmessagebox.h>
#include <KStandardDirs>

#include <mlt++/Mlt.h>

#include <qxml.h>
#include <qimage.h>

#include <QThread>
#include <QApplication>
#include <QCryptographicHash>

#include "clipmanager.h"
#include "renderer.h"
#include "kthumb.h"
#include "kdenlivesettings.h"
#include "events.h"

void MyThread::init(QObject *parent, KUrl url, QString target, double frame, double frameLength, int frequency, int channels, int arrayWidth) {
    stop_me = false;
    m_parent = parent;
    m_isWorking = false;
    f.setFileName(target);
    m_url = url;
    m_frame = frame;
    m_frameLength = frameLength;
    m_frequency = frequency;
    m_channels = channels;
    m_arrayWidth = arrayWidth;
}

bool MyThread::isWorking() {
    return m_isWorking;
}

void MyThread::run() {

    if (!f.open(QIODevice::WriteOnly)) {
        kDebug() << "++++++++  ERROR WRITING TO FILE: " << f.fileName() << endl;
        kDebug() << "++++++++  DISABLING AUDIO THUMBS" << endl;
        KdenliveSettings::setAudiothumbnails(false);
        return;
    }
    m_isWorking = true;
    Mlt::Profile prof((char*) KdenliveSettings::current_profile().toUtf8().data());
    Mlt::Producer m_producer(prof, m_url.path().toUtf8().data());


    if (KdenliveSettings::normaliseaudiothumbs()) {
        Mlt::Filter m_convert(prof, "volume");
        m_convert.set("gain", "normalise");
        m_producer.attach(m_convert);
    }

    //QApplication::postEvent(m_parent, new ProgressEvent(-1, (QEvent::Type)10005));

    int last_val = 0;
    int val = 0;
    kDebug() << "for " << m_frame << " " << m_frameLength << " " << m_producer.is_valid();
    for (int z = (int) m_frame;z < (int)(m_frame + m_frameLength) && m_producer.is_valid();z++) {
        if (stop_me) break;
        val = (int)((z - m_frame) / (m_frame + m_frameLength) * 100.0);
        if (last_val != val & val > 1) {
            QApplication::postEvent(m_parent, new ProgressEvent(val, (QEvent::Type)10005));

            last_val = val;
        }
        m_producer.seek(z);
        Mlt::Frame *mlt_frame = m_producer.get_frame();
        if (mlt_frame && mlt_frame->is_valid()) {
            double m_framesPerSecond = mlt_producer_get_fps(m_producer.get_producer());   //mlt_frame->get_double( "fps" );
            int m_samples = mlt_sample_calculator(m_framesPerSecond, m_frequency, mlt_frame_get_position(mlt_frame->get_frame()));
            mlt_audio_format m_audioFormat = mlt_audio_pcm;

            int16_t* m_pcm = mlt_frame->get_audio(m_audioFormat, m_frequency, m_channels, m_samples);

            for (int c = 0;c < m_channels;c++) {
                QByteArray m_array;
                m_array.resize(m_arrayWidth);
                for (uint i = 0; i < m_array.size(); i++) {
                    m_array[i] = ((*(m_pcm + c + i * m_samples / m_array.size())) >> 9) + 127 / 2 ;
                }
                f.write(m_array);

            }
        } else {
            f.write(QByteArray(m_arrayWidth, '\x00'));
        }
        if (mlt_frame)
            delete mlt_frame;
    }
    kDebug() << "done";
    f.close();
    m_isWorking = false;
    if (stop_me) {
        f.remove();
    }
    QApplication::postEvent(m_parent, new ProgressEvent(-1, (QEvent::Type)10005));

}


#define _S(a)           (a)>255 ? 255 : (a)<0 ? 0 : (a)
#define _R(y,u,v) (0x2568*(y)                          + 0x3343*(u)) /0x2000
#define _G(y,u,v) (0x2568*(y) - 0x0c92*(v) - 0x1a1e*(u)) /0x2000
#define _B(y,u,v) (0x2568*(y) + 0x40cf*(v))                                          /0x2000

KThumb::KThumb(ClipManager *clipManager, KUrl url, QObject * parent, const char *name): QObject(parent), m_clipManager(clipManager), m_url(url) {

    m_profile = new Mlt::Profile((char*) KdenliveSettings::current_profile().data());
    QCryptographicHash context(QCryptographicHash::Sha1);
    context.addData((KFileItem(m_url, "text/plain", S_IFREG).timeString() + m_url.fileName()).toAscii().data());
    m_thumbFile = KGlobal::dirs()->saveLocation("tmp" , "kdenlive") + context.result().toHex() + ".thumb";
    kDebug() << "thumbfile=" << m_thumbFile;
}

KThumb::~KThumb() {
    if (m_profile) delete m_profile;
    if (thumbProducer.isRunning()) thumbProducer.exit();
}

void KThumb::updateClipUrl(KUrl url) {
    m_url = url;
}

//static
QPixmap KThumb::getImage(KUrl url, int width, int height) {
    if (url.isEmpty()) return QPixmap();
    return getImage(url, 0, width, height);
}

void KThumb::extractImage(int frame, int frame2) {
    if (m_url.isEmpty()) return;
    char *tmp = Render::decodedString("<westley><playlist><producer resource=\"" + m_url.path() + "\" /></playlist></westley>");
    Mlt::Producer producer(*m_profile, "westley-xml", tmp);
    delete[] tmp;

    int twidth = (int)(KdenliveSettings::trackheight() * m_profile->dar());
    if (producer.is_blank()) {
        QPixmap pix(twidth, KdenliveSettings::trackheight());
        pix.fill(Qt::black);
        emit thumbReady(frame, pix);
        return;
    }
    if (frame != -1) {
        QPixmap pix = getFrame(producer, frame, twidth, KdenliveSettings::trackheight());
        emit thumbReady(frame, pix);
    }
    if (frame2 != -1) {
        QPixmap pix = getFrame(producer, frame2, twidth , KdenliveSettings::trackheight());
        emit thumbReady(frame2, pix);
    }
}

//static
QPixmap KThumb::getImage(KUrl url, int frame, int width, int height) {
    Mlt::Profile profile((char*) KdenliveSettings::current_profile().data());
    QPixmap pix(width, height);
    if (url.isEmpty()) return pix;

    char *tmp = Render::decodedString("<westley><playlist><producer resource=\"" + url.path() + "\" /></playlist></westley>");
    Mlt::Producer producer(profile, "westley-xml", tmp);
    delete[] tmp;


    if (producer.is_blank()) {

        pix.fill(Qt::black);
        return pix;
    }
    return getFrame(producer, frame, width, height);
}

//static
QPixmap KThumb::getImage(QDomElement xml, int frame, int width, int height) {
    Mlt::Profile profile((char*) KdenliveSettings::current_profile().data());
    QPixmap pix(width, height);
    QDomDocument doc;
    QDomElement westley = doc.createElement("westley");
    QDomElement play = doc.createElement("playlist");
    doc.appendChild(westley);
    westley.appendChild(play);
    play.appendChild(doc.importNode(xml, true));
    char *tmp = Render::decodedString(doc.toString());
    Mlt::Producer producer(profile, "westley-xml", tmp);
    delete[] tmp;

    if (producer.is_blank()) {
        pix.fill(Qt::black);
        return pix;
    }
    return getFrame(producer, frame, width, height);
}

//static
QPixmap KThumb::getFrame(Mlt::Producer producer, int framepos, int width, int height) {

    producer.seek(framepos);
    Mlt::Frame *frame = producer.get_frame();
    mlt_image_format format = mlt_image_yuv422;
    int frame_width = 0;
    int frame_height = 0;
    frame->set("normalised_height", height);
    frame->set("normalised_width", width);
    QPixmap pix(width, height);

    uint8_t *data = frame->get_image(format, frame_width, frame_height, 0);
    uint8_t *new_image = (uint8_t *)mlt_pool_alloc(frame_width * (frame_height + 1) * 4);
    mlt_convert_yuv422_to_rgb24a((uint8_t *)data, new_image, frame_width * frame_height);
    QImage image((uchar *)new_image, frame_width, frame_height, QImage::Format_ARGB32);

    if (!image.isNull()) {
        pix = pix.fromImage(image.rgbSwapped());
    } else
        pix.fill(Qt::black);

    mlt_pool_release(new_image);
    delete frame;
    return pix;
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
*/
void KThumb::stopAudioThumbs() {
    if (thumbProducer.isRunning()) thumbProducer.stop_me = true;
}


void KThumb::removeAudioThumb() {
    if (m_thumbFile.isEmpty()) return;
    stopAudioThumbs();
    QFile f(m_thumbFile);
    f.remove();
}

void KThumb::getAudioThumbs(int channel, double frame, double frameLength, int arrayWidth) {

    if ((thumbProducer.isRunning() && thumbProducer.isWorking()) || channel == 0) {
        return;
    }

    QMap <int, QMap <int, QByteArray> > storeIn;
    //FIXME: Hardcoded!!!
    int m_frequency = 48000;
    int m_channels = channel;

    QFile f(m_thumbFile);
    if (f.open(QIODevice::ReadOnly)) {
        QByteArray channelarray = f.readAll();
        f.close();
        if (channelarray.size() != arrayWidth*(frame + frameLength)*m_channels) {
            kDebug() << "--- BROKEN THUMB FOR: " << m_url.fileName() << " ---------------------- " << endl;
            f.remove();
            return;
        }
        kDebug() << "reading audio thumbs from file";
        for (int z = (int) frame;z < (int)(frame + frameLength);z++) {
            for (int c = 0;c < m_channels;c++) {
                QByteArray m_array(arrayWidth, '\x00');
                for (int i = 0; i < arrayWidth; i++)
                    m_array[i] = channelarray[z*arrayWidth*m_channels + c*arrayWidth + i];
                storeIn[z][c] = m_array;
            }
        }
        emit audioThumbReady(storeIn);
    } else {
        if (thumbProducer.isRunning()) return;
        thumbProducer.init(this, m_url, m_thumbFile, frame, frameLength, m_frequency, m_channels, arrayWidth);
        thumbProducer.start(QThread::LowestPriority);
        kDebug() << "STARTING GENERATE THMB FOR: " << m_url << " ................................";
    }
}

void KThumb::customEvent(QEvent * event) {
    if (event->type() == 10005) {
        ProgressEvent* p = (ProgressEvent*) event;
        m_clipManager->setThumbsProgress(i18n("Creating thumbnail for %1", m_url.fileName()), p->value());
    }
}



#include "kthumb.moc"

