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

#include "project/clipmanager.h"
#include "renderer.h"
#include "kdenlivesettings.h"

#include <mlt++/Mlt.h>

#include <QDebug>
#include <kfileitem.h>
#include <kmessagebox.h>

#include <qxml.h>
#include <QImage>
#include <QVarLengthArray>
#include <QPainter>
#include <QtConcurrent>

static QMutex m_intraMutex;

KThumb::KThumb(ClipManager *clipManager, const QUrl &url, const QString &id, const QString &hash, QObject * parent) :
    QObject(parent),
    m_url(url),
    m_dar(1),
    m_ratio(1),
    m_producer(NULL),
    m_clipManager(clipManager),
    m_id(id)
{
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
    QMutexLocker lock(&m_mutex);
    m_producer = producer;
    // FIXME: the profile() call leaks an object, but trying to free
    // it leads to a double-free in Profile::~Profile()
    if (producer) {
        Mlt::Profile *profile = producer->profile();
        m_dar = profile->dar();
        m_ratio = (double) profile->width() / profile->height();
    }
}

void KThumb::clearProducer()
{
    if (m_producer) setProducer(NULL);
}

bool KThumb::hasProducer() const
{
    return m_producer != NULL;
}

//static
QPixmap KThumb::getImage(const QUrl &url, int width, int height)
{
    if (!url.isValid()) return QPixmap();
    return getImage(url, 0, width, height);
}

void KThumb::extractImage(const QList<int> &frames)
{
    if (!KdenliveSettings::videothumbnails() || m_producer == NULL) return;
    m_clipManager->slotRequestThumbs(m_id, frames);
}


void KThumb::getThumb(int frame)
{
    const int theight = Kdenlive::DefaultThumbHeight;
    const int dwidth = (int)(theight * m_dar + 0.5);
    QImage img = getProducerFrame(frame, dwidth, theight);
    emit thumbReady(frame, img);
}

QImage KThumb::extractImage(int frame, int width, int height)
{
    if (m_producer == NULL) {
        QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(Qt::black).rgb());
        return img;
    }
    return getProducerFrame(frame, width, height);
}

//static
QPixmap KThumb::getImage(const QUrl &url, int frame, int width, int height)
{
    Mlt::Profile profile(KdenliveSettings::current_profile().toUtf8().constData());
    if (height == -1) {
        height = width * profile.height() / profile.width();
    }
    QPixmap pix(width, height);
    if (!url.isValid()) return pix;
    Mlt::Producer *producer = new Mlt::Producer(profile, url.path().toUtf8().constData());
    pix = QPixmap::fromImage(getFrame(producer, frame, width, height));
    delete producer;
    return pix;
}

QImage KThumb::getProducerFrame(int framepos, int displayWidth, int height)
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
    QMutexLocker lock(&m_mutex);
    m_producer->seek(framepos);
    Mlt::Frame *frame = m_producer->get_frame();
    /*frame->set("rescale.interp", "nearest");
    frame->set("deinterlace_method", "onefield");
    frame->set("top_field_first", -1 );*/
    QImage p = getFrame(frame, displayWidth, height);
    delete frame;
    return p;
}

//static
QImage KThumb::getFrame(Mlt::Producer *producer, int framepos, int displayWidth, int height)
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
    const QImage p = getFrame(frame, displayWidth, height);
    delete frame;
    return p;
}


//static
QImage KThumb::getFrame(Mlt::Frame *frame, int width, int height)
{
    if (frame == NULL || !frame->is_valid()) {
        QImage p(width, height, QImage::Format_ARGB32_Premultiplied);
        p.fill(QColor(Qt::red).rgb());
        return p;
    }
    int ow = width;
    int oh = height;
    mlt_image_format format = mlt_image_rgb24a;
    //frame->set("progressive", "1");
    //if (ow % 2 == 1) ow++;
    const uchar* imagedata = frame->get_image(format, ow, oh);
    if (imagedata) {
        QImage image(ow, oh, QImage::Format_RGBA8888);
        memcpy(image.bits(), imagedata, ow * oh * 4);
        if (!image.isNull()) {
            if (ow > (2 * width)) {
                // there was a scaling problem, do it manually
                image = image.scaled(width, height);
            }
            return image;
            /*p.fill(QColor(100, 100, 100, 70));
            QPainter painter(&p);
            painter.drawImage(p.rect(), image);
            painter.end();*/
        }
    }
    QImage p(width, height, QImage::Format_ARGB32_Premultiplied);
    p.fill(QColor(Qt::red).rgb());
    return p;
}

//static
uint KThumb::imageVariance(const QImage &image )
{
    uint delta = 0;
    uint avg = 0;
    uint bytes = image.byteCount();
    uint STEPS = bytes/2;
    QVarLengthArray<uchar> pivot(STEPS);
    const uchar *bits=image.bits();
    // First pass: get pivots and taking average
    for( uint i=0; i<STEPS ; ++i ){
        pivot[i] = bits[2 * i];
        avg+=pivot.at(i);
    }
    if (STEPS)
        avg=avg/STEPS;
    // Second Step: calculate delta (average?)
    for (uint i=0; i<STEPS; ++i)
    {
        int curdelta=abs(int(avg - pivot.at(i)));
        delta+=curdelta;
    }
    if (STEPS)
        return delta/STEPS;
    else
        return 0;
}

/*
void KThumb::getImage(QUrl url, int frame, int width, int height)
{
    if (!url.isValid()) return;
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

void KThumb::getThumbs(QUrl url, int startframe, int endframe, int width, int height)
{
    if (!url.isValid()) return;
    QPixmap image(width, height);
    Mlt::Producer m_producer(url.path().toUtf8().constData());
    image.fill(QColor(Qt::black).rgb());

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

void KThumb::queryIntraThumbs(const QSet <int> &missingFrames)
{
    m_intraMutex.lock();
    if (m_intraFramesQueue.length() > 20) {
        // trim query list
        int maxsize = m_intraFramesQueue.length() - 10;
        if (missingFrames.contains(m_intraFramesQueue.first())) {
            for( int i = 0; i < maxsize; i ++ )
                m_intraFramesQueue.removeLast();
        }
        else if (missingFrames.contains(m_intraFramesQueue.last())) {
            for( int i = 0; i < maxsize; i ++ )
                m_intraFramesQueue.removeFirst();
        }
        else m_intraFramesQueue.clear();
    }
    QSet<int> set = m_intraFramesQueue.toSet();
    set.unite(missingFrames);
    m_intraFramesQueue = set.toList();
    qSort(m_intraFramesQueue);
    m_intraMutex.unlock();
    if (!m_intra.isRunning()) {
        m_intra = QtConcurrent::run(this, &KThumb::slotGetIntraThumbs);
    }
}

void KThumb::slotGetIntraThumbs()
{
    const int theight = KdenliveSettings::trackheight();
    const int displayWidth = (int)(theight * m_dar + 0.5);
    QString path = m_url.path() + '_';
    bool addedThumbs = false;
    while (true) {
        m_intraMutex.lock();
        if (m_intraFramesQueue.isEmpty()) {
            m_intraMutex.unlock();
            break;
        }
        int pos = m_intraFramesQueue.takeFirst();
        m_intraMutex.unlock();
        const QString key = path + QString::number(pos);
        if (!m_clipManager->pixmapCache->contains(key)) {
            QImage img = getProducerFrame(pos, displayWidth, theight);
            if (m_clipManager->pixmapCache->insertImage(key, img)) {
                addedThumbs = true;
            }
            else qDebug()<<"// INSERT FAILD FOR: "<<pos;
        }
    }
    if (addedThumbs) emit thumbsCached();
}
