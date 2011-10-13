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
#include <QVarLengthArray>

KThumb::KThumb(ClipManager *clipManager, KUrl url, const QString &id, const QString &hash, QObject * parent, const char */*name*/) :
    QObject(parent),
    m_audioThumbProducer(),
    m_url(url),
    m_thumbFile(),
    m_dar(1),
    m_ratio(1),
    m_producer(NULL),
    m_clipManager(clipManager),
    m_id(id),
    m_stopAudioThumbs(false)
{
    m_thumbFile = clipManager->projectFolder() + "/thumbs/" + hash + ".thumb";
}

KThumb::~KThumb()
{
    m_requestedThumbs.clear();
    m_intraFramesQueue.clear();
    if (m_audioThumbProducer.isRunning()) {
        m_stopAudioThumbs = true;
        m_audioThumbProducer.waitForFinished();
        slotAudioThumbOver();
    }
    m_future.waitForFinished();
    m_intra.waitForFinished();
}

void KThumb::setProducer(Mlt::Producer *producer)
{
    m_mutex.lock();
    m_requestedThumbs.clear();
    m_intraFramesQueue.clear();
    m_future.waitForFinished();
    m_intra.waitForFinished();
    m_producer = producer;
    // FIXME: the profile() call leaks an object, but trying to free
    // it leads to a double-free in Profile::~Profile()
    if (producer) {
        m_dar = producer->profile()->dar();
        m_ratio = (double) producer->profile()->width() / producer->profile()->height();
    }
    m_mutex.unlock();
}

void KThumb::clearProducer()
{
    setProducer(NULL);
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
    //if (m_producer)
        //m_producer->set("resource", url.path().toUtf8().constData());
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
    if (!m_future.isRunning()) {
        m_mutex.lock();
        m_future = QtConcurrent::run(this, &KThumb::doGetThumbs);
        m_mutex.unlock();
    }
}

void KThumb::doGetThumbs()
{
    const int theight = KdenliveSettings::trackheight();
    const int swidth = (int)(theight * m_ratio + 0.5);
    const int dwidth = (int)(theight * m_dar + 0.5);

    while (!m_requestedThumbs.isEmpty()) {
        int frame = m_requestedThumbs.takeFirst();
        if (frame != -1) {
            emit thumbReady(frame, getFrame(m_producer, frame, swidth, dwidth, theight));
        }
    }
}

QPixmap KThumb::extractImage(int frame, int width, int height)
{
    m_mutex.lock();
    QImage img = getFrame(m_producer, frame, (int) (height * m_ratio + 0.5), width, height);
    m_mutex.unlock();
    return QPixmap::fromImage(img);
}

//static
QPixmap KThumb::getImage(KUrl url, int frame, int width, int height)
{
    Mlt::Profile profile(KdenliveSettings::current_profile().toUtf8().constData());
    QPixmap pix(width, height);
    if (url.isEmpty()) return pix;

    //"<mlt><playlist><producer resource=\"" + url.path() + "\" /></playlist></mlt>");
    //Mlt::Producer producer(profile, "xml-string", tmp);
    Mlt::Producer *producer = new Mlt::Producer(profile, url.path().toUtf8().constData());
    double swidth = (double) profile.width() / profile.height();
    pix = QPixmap::fromImage(getFrame(producer, frame, (int) (height * swidth + 0.5), width, height));
    delete producer;
    return pix;
}

//static
QImage KThumb::getFrame(Mlt::Producer *producer, int framepos, int frameWidth, int displayWidth, int height)
{
    QImage p(displayWidth, height, QImage::Format_ARGB32_Premultiplied);
    if (producer == NULL || !producer->is_valid()) {
        p.fill(Qt::red);
        return p;
    }

    if (producer->is_blank()) {
        p.fill(Qt::black);
        return p;
    }

    producer->seek(framepos);
    Mlt::Frame *frame = producer->get_frame();
    p = getFrame(frame, frameWidth, displayWidth, height);
    delete frame;
    return p;
}


//static
QImage KThumb::getFrame(Mlt::Frame *frame, int frameWidth, int displayWidth, int height)
{
    QImage p(displayWidth, height, QImage::Format_ARGB32_Premultiplied);
    if (frame == NULL || !frame->is_valid()) {
        p.fill(Qt::red);
        return p;
    }

    int ow = frameWidth;
    int oh = height;
    mlt_image_format format = mlt_image_rgb24a;
    uint8_t *data = frame->get_image(format, ow, oh, 0);
    QImage image((uchar *)data, ow, oh, QImage::Format_ARGB32_Premultiplied);
    if (!image.isNull()) {
        if (ow > (2 * displayWidth)) {
            // there was a scaling problem, do it manually
            QImage scaled = image.scaled(displayWidth, height);
            image = scaled.rgbSwapped();
        } else {
            image = image.scaled(displayWidth, height, Qt::IgnoreAspectRatio).rgbSwapped();
        }
        QPainter painter(&p);
        painter.fillRect(p.rect(), Qt::black);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage(p.rect(), image);
        painter.end();
    } else
        p.fill(Qt::red);
    return p;
}

//static
uint KThumb::imageVariance(QImage image )
{
    uint delta = 0;
    uint avg = 0;
    uint bytes = image.numBytes();
    uint STEPS = bytes/2;
    QVarLengthArray<uchar> pivot(STEPS);
    const uchar *bits=image.bits();
    // First pass: get pivots and taking average
    for( uint i=0; i<STEPS ; i++ ){
        pivot[i] = bits[2 * i];
#if QT_VERSION >= 0x040700
        avg+=pivot.at(i);
#else
        avg+=pivot[i];
#endif
    }
    avg=avg/STEPS;
    // Second Step: calculate delta (average?)
    for (uint i=0; i<STEPS; i++)
    {
#if QT_VERSION >= 0x040700
        int curdelta=abs(int(avg - pivot.at(i)));
#else
        int curdelta=abs(int(avg - pivot[i]));
#endif
        delta+=curdelta;
    }
    return delta/STEPS;
}

/*
void KThumb::getImage(KUrl url, int frame, int width, int height)
{
    if (url.isEmpty()) return;
    QPixmap image(width, height);
    Mlt::Producer m_producer(url.path().toUtf8().constData());
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
    Mlt::Producer m_producer(url.path().toUtf8().constData());
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
        m_stopAudioThumbs = true;
        m_audioThumbProducer.waitForFinished();
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
    if (m_audioThumbProducer.isRunning()) {
        return;
    }

    QMap <int, QMap <int, QByteArray> > storeIn;
    //FIXME: Hardcoded!!!
    m_frequency = 48000;
    m_channels = channel;

    QFile f(m_thumbFile);
    if (f.open(QIODevice::ReadOnly)) {
        const QByteArray channelarray = f.readAll();
        f.close();
        if (channelarray.size() != arrayWidth*(frame + frameLength)*m_channels) {
            kDebug() << "--- BROKEN THUMB FOR: " << m_url.fileName() << " ---------------------- " << endl;
            f.remove();
            slotAudioThumbOver();
            return;
        }

        kDebug() << "reading audio thumbs from file";

        int h1 = arrayWidth * m_channels;
        int h2 = (int) frame * h1;
        int h3;
        for (int z = (int) frame; z < (int)(frame + frameLength); z++) {
            h3 = 0;
            for (int c = 0; c < m_channels; c++) {
                QByteArray m_array(arrayWidth, '\x00');
                for (int i = 0; i < arrayWidth; i++) {
                    m_array[i] = channelarray.at(h2 + h3 + i);
                }
                h3 += arrayWidth;
                storeIn[z][c] = m_array;
            }
            h2 += h1;
        }
        emit audioThumbReady(storeIn);
        slotAudioThumbOver();
    } else {
        if (m_audioThumbProducer.isRunning()) return;
        m_audioThumbFile.setFileName(m_thumbFile);
        m_frame = frame;
        m_frameLength = frameLength;
        m_arrayWidth = arrayWidth;
        m_audioThumbProducer = QtConcurrent::run(this, &KThumb::slotCreateAudioThumbs);
        /*m_audioThumbProducer.init(m_url, m_thumbFile, frame, frameLength, m_frequency, m_channels, arrayWidth);
        m_audioThumbProducer.start(QThread::LowestPriority);*/
        // kDebug() << "STARTING GENERATE THMB FOR: " <<m_id<<", URL: "<< m_url << " ................................";
    }
}

void KThumb::slotCreateAudioThumbs()
{
    Mlt::Profile prof((char*) KdenliveSettings::current_profile().toUtf8().data());
    Mlt::Producer producer(prof, m_url.path().toUtf8().data());
    if (!producer.is_valid()) {
        kDebug() << "++++++++  INVALID CLIP: " << m_url.path();
        return;
    }
    if (!m_audioThumbFile.open(QIODevice::WriteOnly)) {
        kDebug() << "++++++++  ERROR WRITING TO FILE: " << m_audioThumbFile.fileName();
        kDebug() << "++++++++  DISABLING AUDIO THUMBS";
        KdenliveSettings::setAudiothumbnails(false);
        return;
    }

    if (KdenliveSettings::normaliseaudiothumbs()) {
        Mlt::Filter m_convert(prof, "volume");
        m_convert.set("gain", "normalise");
        producer.attach(m_convert);
    }

    int last_val = 0;
    int val = 0;
    //kDebug() << "for " << m_frame << " " << m_frameLength << " " << m_producer.is_valid();
    for (int z = (int) m_frame; z < (int)(m_frame + m_frameLength) && producer.is_valid(); z++) {
        if (m_stopAudioThumbs) break;
        val = (int)((z - m_frame) / (m_frame + m_frameLength) * 100.0);
        if (last_val != val && val > 1) {
            m_clipManager->setThumbsProgress(i18n("Creating thumbnail for %1", m_url.fileName()), val);
            last_val = val;
        }
        producer.seek(z);
        Mlt::Frame *mlt_frame = producer.get_frame();
        if (mlt_frame && mlt_frame->is_valid()) {
            double m_framesPerSecond = mlt_producer_get_fps(producer.get_producer());
            int m_samples = mlt_sample_calculator(m_framesPerSecond, m_frequency, mlt_frame_get_position(mlt_frame->get_frame()));
            mlt_audio_format m_audioFormat = mlt_audio_pcm;
            qint16* m_pcm = static_cast<qint16*>(mlt_frame->get_audio(m_audioFormat, m_frequency, m_channels, m_samples));

            for (int c = 0; c < m_channels; c++) {
                QByteArray m_array;
                m_array.resize(m_arrayWidth);
                for (int i = 0; i < m_array.size(); i++) {
                    m_array[i] = ((*(m_pcm + c + i * m_samples / m_array.size())) >> 9) + 127 / 2 ;
                }
                m_audioThumbFile.write(m_array);

            }
        } else {
            m_audioThumbFile.write(QByteArray(m_arrayWidth, '\x00'));
        }
        delete mlt_frame;
    }
    m_audioThumbFile.close();
    if (m_stopAudioThumbs) {
        m_audioThumbFile.remove();
    } else {
        slotAudioThumbOver();
    }
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

#if KDE_IS_VERSION(4,5,0)
void KThumb::queryIntraThumbs(QList <int> missingFrames)
{
    foreach (int i, missingFrames) {
        if (!m_intraFramesQueue.contains(i)) m_intraFramesQueue.append(i);
    }
    qSort(m_intraFramesQueue);
    if (!m_intra.isRunning()) {
        m_mutex.lock();
        m_intra = QtConcurrent::run(this, &KThumb::slotGetIntraThumbs);
        m_mutex.unlock();
    }
}

void KThumb::slotGetIntraThumbs()
{
    const int theight = KdenliveSettings::trackheight();
    const int frameWidth = (int)(theight * m_ratio + 0.5);
    const int displayWidth = (int)(theight * m_dar + 0.5);
    QString path = m_url.path() + "_";
    bool addedThumbs = false;

    while (!m_intraFramesQueue.isEmpty()) {
        int pos = m_intraFramesQueue.takeFirst();
        if (!m_clipManager->pixmapCache->contains(path + QString::number(pos))) {
            if (m_clipManager->pixmapCache->insertImage(path + QString::number(pos), getFrame(m_producer, pos, frameWidth, displayWidth, theight))) {
                addedThumbs = true;
            }
            else kDebug()<<"// INSERT FAILD FOR: "<<pos;
        }
        m_intraFramesQueue.removeAll(pos);
    }
    if (addedThumbs) emit thumbsCached();
}

QImage KThumb::findCachedThumb(const QString path)
{
    QImage img;
    m_clipManager->pixmapCache->findImage(path, &img);
    return img;
}
#endif

#include "kthumb.moc"

