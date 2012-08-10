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

KThumb::KThumb(ClipManager *clipManager, KUrl url, const QString &id, const QString &hash, QObject * parent) :
    QObject(parent),
    m_url(url),
    m_thumbFile(),
    m_dar(1),
    m_ratio(1),
    m_producer(NULL),
    m_clipManager(clipManager),
    m_id(id)
{
    m_thumbFile = clipManager->projectFolder() + "/thumbs/" + hash + ".thumb";
}

KThumb::~KThumb()
{
    if (m_producer) m_clipManager->stopThumbs(m_id);
    m_producer = NULL;
    m_intraFramesQueue.clear();
    m_intra.waitForFinished();
}

void KThumb::setProducer(Mlt::Producer *producer)
{
    if (m_producer) m_clipManager->stopThumbs(m_id);
    m_intraFramesQueue.clear();
    m_intra.waitForFinished();
    m_mutex.lock();
    m_producer = producer;
    // FIXME: the profile() call leaks an object, but trying to free
    // it leads to a double-free in Profile::~Profile()
    if (producer) {
        Mlt::Profile *profile = producer->profile();
        m_dar = profile->dar();
        m_ratio = (double) profile->width() / profile->height();
    }
    m_mutex.unlock();
}

void KThumb::clearProducer()
{
    if (m_producer) setProducer(NULL);
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
    m_thumbFile = m_clipManager->projectFolder() + "/thumbs/" + hash + ".thumb";
}

//static
QPixmap KThumb::getImage(KUrl url, int width, int height)
{
    if (url.isEmpty()) return QPixmap();
    return getImage(url, 0, width, height);
}

void KThumb::extractImage(QList <int>frames)
{
    if (!KdenliveSettings::videothumbnails() || m_producer == NULL) return;
    m_clipManager->requestThumbs(m_id, frames);
}


void KThumb::getThumb(int frame)
{
    const int theight = KdenliveSettings::trackheight();
    const int swidth = (int)(theight * m_ratio + 0.5);
    const int dwidth = (int)(theight * m_dar + 0.5);

    QImage img = getProducerFrame(frame, swidth, dwidth, theight);
    emit thumbReady(frame, img);
}

QImage KThumb::extractImage(int frame, int width, int height)
{
    if (m_producer == NULL) {
        QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::black);
        return img;
    }
    return getProducerFrame(frame, (int) (height * m_ratio + 0.5), width, height);
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


QImage KThumb::getProducerFrame(int framepos, int frameWidth, int displayWidth, int height)
{
    if (m_producer == NULL || !m_producer->is_valid()) {
        QImage p(displayWidth, height, QImage::Format_ARGB32_Premultiplied);
        p.fill(QColor(Qt::red).rgb());
        return p;
    }
    if (m_producer->is_blank()) {
        QImage p(displayWidth, height, QImage::Format_ARGB32_Premultiplied);
        p.fill(QColor(Qt::black).rgb());
        return p;
    }
    m_mutex.lock();
    m_producer->seek(framepos);
    Mlt::Frame *frame = m_producer->get_frame();
    QImage p = getFrame(frame, frameWidth, displayWidth, height);
    delete frame;
    m_mutex.unlock();
    return p;
}

//static
QImage KThumb::getFrame(Mlt::Producer *producer, int framepos, int frameWidth, int displayWidth, int height)
{
    if (producer == NULL || !producer->is_valid()) {
        QImage p(displayWidth, height, QImage::Format_ARGB32_Premultiplied);
        p.fill(QColor(Qt::red).rgb());
        return p;
    }
    if (producer->is_blank()) {
        QImage p(displayWidth, height, QImage::Format_ARGB32_Premultiplied);
        p.fill(QColor(Qt::black).rgb());
        return p;
    }

    producer->seek(framepos);
    Mlt::Frame *frame = producer->get_frame();
    QImage p = getFrame(frame, frameWidth, displayWidth, height);
    delete frame;
    return p;
}


//static
QImage KThumb::getFrame(Mlt::Frame *frame, int frameWidth, int displayWidth, int height)
{
    QImage p(displayWidth, height, QImage::Format_ARGB32_Premultiplied);
    if (frame == NULL || !frame->is_valid()) {
        p.fill(QColor(Qt::red).rgb());
        return p;
    }

    int ow = frameWidth;
    int oh = height;
    mlt_image_format format = mlt_image_rgb24a;

    const uchar* imagedata = frame->get_image(format, ow, oh);
    QImage image(ow, oh, QImage::Format_ARGB32_Premultiplied);
    memcpy(image.bits(), imagedata, ow * oh * 4);//.byteCount());
    
    //const uchar* imagedata = frame->get_image(format, ow, oh);
    //QImage image(imagedata, ow, oh, QImage::Format_ARGB32_Premultiplied);
    
    if (!image.isNull()) {
        if (ow > (2 * displayWidth)) {
            // there was a scaling problem, do it manually
            image = image.scaled(displayWidth, height).rgbSwapped();
        } else {
            image = image.scaled(displayWidth, height, Qt::IgnoreAspectRatio).rgbSwapped();
        }
        p.fill(QColor(Qt::black).rgb());
        QPainter painter(&p);
        painter.drawImage(p.rect(), image);
        painter.end();
    } else
        p.fill(QColor(Qt::red).rgb());
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
    if (STEPS)
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
    if (STEPS)
        return delta/STEPS;
    else
        return 0;
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

void KThumb::slotCreateAudioThumbs()
{
    m_clipManager->askForAudioThumb(m_id);
}

#if KDE_IS_VERSION(4,5,0)
void KThumb::queryIntraThumbs(QList <int> missingFrames)
{
    foreach (int i, missingFrames) {
        if (!m_intraFramesQueue.contains(i)) m_intraFramesQueue.append(i);
    }
    qSort(m_intraFramesQueue);
    if (!m_intra.isRunning()) {
        m_intra = QtConcurrent::run(this, &KThumb::slotGetIntraThumbs);
    }
}

void KThumb::slotGetIntraThumbs()
{
    const int theight = KdenliveSettings::trackheight();
    const int frameWidth = (int)(theight * m_ratio + 0.5);
    const int displayWidth = (int)(theight * m_dar + 0.5);
    QString path = m_url.path() + '_';
    bool addedThumbs = false;

    while (!m_intraFramesQueue.isEmpty()) {
        int pos = m_intraFramesQueue.takeFirst();
        if (!m_clipManager->pixmapCache->contains(path + QString::number(pos))) {
            if (m_clipManager->pixmapCache->insertImage(path + QString::number(pos), getProducerFrame(pos, frameWidth, displayWidth, theight))) {
                addedThumbs = true;
            }
            else kDebug()<<"// INSERT FAILD FOR: "<<pos;
        }
        m_intraFramesQueue.removeAll(pos);
    }
    if (addedThumbs) emit thumbsCached();
}

QImage KThumb::findCachedThumb(const QString &path)
{
    QImage img;
    m_clipManager->pixmapCache->findImage(path, &img);
    return img;
}
#endif

#include "kthumb.moc"

