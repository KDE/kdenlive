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

#include "kthumb.h"
#include "clipmanager.h"
#include "renderer.h"
#include "kdenlivesettings.h"

#include <mlt++/Mlt.h>

#include <kio/netaccess.h>
#include <kdebug.h>
#include <klocale.h>
#include <kfileitem.h>
#include <kmessagebox.h>
#include <KStandardDirs>

#include <qxml.h>
#include <QImage>
#include <QApplication>
#include <QtConcurrentRun>

void MyThread::init(KUrl url, QString target, double frame, double frameLength, int frequency, int channels, int arrayWidth)
{
    stop_me = false;
    m_isWorking = false;
    f.setFileName(target);
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

    int last_val = 0;
    int val = 0;
    kDebug() << "for " << m_frame << " " << m_frameLength << " " << m_producer.is_valid();
    for (int z = (int) m_frame; z < (int)(m_frame + m_frameLength) && m_producer.is_valid(); z++) {
        if (stop_me) break;
        val = (int)((z - m_frame) / (m_frame + m_frameLength) * 100.0);
        if (last_val != val && val > 1) {
            emit audioThumbProgress(val);
            last_val = val;
        }
        m_producer.seek(z);
        Mlt::Frame *mlt_frame = m_producer.get_frame();
        if (mlt_frame && mlt_frame->is_valid()) {
            double m_framesPerSecond = mlt_producer_get_fps(m_producer.get_producer());
            int m_samples = mlt_sample_calculator(m_framesPerSecond, m_frequency, mlt_frame_get_position(mlt_frame->get_frame()));
            mlt_audio_format m_audioFormat = mlt_audio_pcm;
            qint16* m_pcm = static_cast<qint16*>(mlt_frame->get_audio(m_audioFormat, m_frequency, m_channels, m_samples));

            for (int c = 0; c < m_channels; c++) {
                QByteArray m_array;
                m_array.resize(m_arrayWidth);
                for (int i = 0; i < m_array.size(); i++) {
                    m_array[i] = ((*(m_pcm + c + i * m_samples / m_array.size())) >> 9) + 127 / 2 ;
                }
                f.write(m_array);

            }
        } else {
            f.write(QByteArray(m_arrayWidth, '\x00'));
        }
        delete mlt_frame;
    }
    //kDebug() << "done";
    f.close();
    m_isWorking = false;
    if (stop_me) {
        f.remove();
    } else emit audioThumbOver();
}


KThumb::KThumb(ClipManager *clipManager, KUrl url, const QString &id, const QString &hash, QObject * parent, const char */*name*/) :
        QObject(parent),
        m_audioThumbProducer(),
        m_url(url),
        m_thumbFile(),
        m_dar(1),
        m_producer(NULL),
        m_clipManager(clipManager),
        m_id(id)
{
    m_thumbFile = clipManager->projectFolder() + "/thumbs/" + hash + ".thumb";
    connect(&m_audioThumbProducer, SIGNAL(audioThumbProgress(const int)), this, SLOT(slotAudioThumbProgress(const int)));
    connect(&m_audioThumbProducer, SIGNAL(audioThumbOver()), this, SLOT(slotAudioThumbOver()));
}

KThumb::~KThumb()
{
    m_requestedThumbs.clear();
    if (m_audioThumbProducer.isRunning()) {
        m_audioThumbProducer.stop_me = true;
        m_audioThumbProducer.wait();
        slotAudioThumbOver();
    }
    m_future.waitForFinished();
}

void KThumb::setProducer(Mlt::Producer *producer)
{
    m_producer = producer;
    // FIXME: the profile() call leaks an object, but trying to free
    // it leads to a double-free in Profile::~Profile()
    m_dar = producer->profile()->dar();
}

void KThumb::clearProducer()
{
    m_producer = NULL;
}

bool KThumb::hasProducer() const
{
    return m_producer != NULL;
}

void KThumb::updateThumbUrl(const QString &hash)
{
    m_thumbFile = m_clipManager->projectFolder() + "/thumbs/" + hash + ".thumb";
}

void KThumb::updateClipUrl(KUrl url, const QString &hash)
{
    m_url = url;
    if (m_producer) {
        char *tmp = Render::decodedString(url.path());
        m_producer->set("resource", tmp);
        delete[] tmp;
    }
    m_thumbFile = m_clipManager->projectFolder() + "/thumbs/" + hash + ".thumb";
}

//static
QPixmap KThumb::getImage(KUrl url, int width, int height)
{
    if (url.isEmpty()) return QPixmap();
    return getImage(url, 0, width, height);
}

void KThumb::extractImage(int frame, int frame2)
{
    if (!KdenliveSettings::videothumbnails() || m_producer == NULL) return;
    if (frame != -1 && !m_requestedThumbs.contains(frame)) m_requestedThumbs.append(frame);
    if (frame2 != -1 && !m_requestedThumbs.contains(frame2)) m_requestedThumbs.append(frame2);
    if (!m_future.isRunning()) m_future = QtConcurrent::run(this, &KThumb::doGetThumbs);
}

void KThumb::doGetThumbs()
{
    const int twidth = (int)(KdenliveSettings::trackheight() * m_dar);
    const int theight = KdenliveSettings::trackheight();

    while (!m_requestedThumbs.isEmpty()) {
        int frame = m_requestedThumbs.takeFirst();
        if (frame != -1) {
            QImage img = getFrame(m_producer, frame, twidth, theight);
            emit thumbReady(frame, img);
        }
    }
}

QPixmap KThumb::extractImage(int frame, int width, int height)
{
    return QPixmap::fromImage(getFrame(m_producer, frame, width, height));
}

//static
QPixmap KThumb::getImage(KUrl url, int frame, int width, int height)
{
    char *tmp = Render::decodedString(KdenliveSettings::current_profile());
    Mlt::Profile profile(tmp);
    delete[] tmp;
    QPixmap pix(width, height);
    if (url.isEmpty()) return pix;

    tmp = Render::decodedString(url.path());
    //"<mlt><playlist><producer resource=\"" + url.path() + "\" /></playlist></mlt>");
    //Mlt::Producer producer(profile, "xml-string", tmp);
    Mlt::Producer *producer = new Mlt::Producer(profile, tmp);
    delete[] tmp;

    pix = QPixmap::fromImage(getFrame(producer, frame, width, height));
    delete producer;
    return pix;
}


//static
QImage KThumb::getFrame(Mlt::Producer *producer, int framepos, int width, int height)
{
    QImage p(width, height, QImage::Format_ARGB32);
    if (producer == NULL) {
        p.fill(Qt::red);
        return p;
    }

    if (producer->is_blank()) {
        p.fill(Qt::black);
        return p;
    }

    producer->seek(framepos);
    Mlt::Frame *frame = producer->get_frame();
    if (!frame) {
        kDebug() << "///// BROKEN FRAME";
        p.fill(Qt::red);
        return p;
    }

    /*Mlt::Producer parentProd(producer->parent());
    Mlt::Service service(parentProd.get_service());
    mlt_service_lock(service.get_service());*/

    mlt_image_format format = mlt_image_rgb24a;
    uint8_t *data = frame->get_image(format, width, height, 0);
    QImage image((uchar *)data, width, height, QImage::Format_ARGB32);
    //mlt_service_unlock(service.get_service());

    if (!image.isNull()) {
        p = image.rgbSwapped();
    } else
        p.fill(Qt::red);

    delete frame;
    return p;
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
void KThumb::stopAudioThumbs()
{
    if (m_audioThumbProducer.isRunning()) {
        m_audioThumbProducer.stop_me = true;
        slotAudioThumbOver();
    }
}

void KThumb::removeAudioThumb()
{
    if (m_thumbFile.isEmpty()) return;
    stopAudioThumbs();
    QFile f(m_thumbFile);
    f.remove();
}

void KThumb::getAudioThumbs(int channel, double frame, double frameLength, int arrayWidth)
{
    if (channel == 0) {
        slotAudioThumbOver();
        return;
    }
    if ((m_audioThumbProducer.isRunning() && m_audioThumbProducer.isWorking())) {
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
            slotAudioThumbOver();
            return;
        }
        kDebug() << "reading audio thumbs from file";
        for (int z = (int) frame; z < (int)(frame + frameLength); z++) {
            for (int c = 0; c < m_channels; c++) {
                QByteArray m_array(arrayWidth, '\x00');
                for (int i = 0; i < arrayWidth; i++)
                    m_array[i] = channelarray[z*arrayWidth*m_channels + c*arrayWidth + i];
                storeIn[z][c] = m_array;
            }
        }
        emit audioThumbReady(storeIn);
        slotAudioThumbOver();
    } else {
        if (m_audioThumbProducer.isRunning()) return;
        m_audioThumbProducer.init(m_url, m_thumbFile, frame, frameLength, m_frequency, m_channels, arrayWidth);
        m_audioThumbProducer.start(QThread::LowestPriority);
        kDebug() << "STARTING GENERATE THMB FOR: " << m_url << " ................................";
    }
}

void KThumb::slotAudioThumbProgress(const int progress)
{
    m_clipManager->setThumbsProgress(i18n("Creating thumbnail for %1", m_url.fileName()), progress);
}

void KThumb::slotAudioThumbOver()
{
    m_clipManager->setThumbsProgress(i18n("Creating thumbnail for %1", m_url.fileName()), -1);
    m_clipManager->endAudioThumbsGeneration(m_id);
}

void KThumb::askForAudioThumbs(const QString &id)
{
    m_clipManager->askForAudioThumb(id);
}


#include "kthumb.moc"

