/*
 * Copyright (c) 2013-2016 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "thumbnailprovider.h"
#include "core.h"
#include "doc/kthumb.h"

#include <QQuickImageProvider>
#include <QCryptographicHash>
#include <QDebug>
#include <mlt++/MltProfile.h>

ThumbnailProvider::ThumbnailProvider()
    : QQuickImageProvider(QQmlImageProviderBase::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
    , m_profile("atsc_720p_60")
{
    KImageCache::deleteCache(QStringLiteral("kdenlive-timeline-thumbs"));
    m_cache = new KImageCache(QStringLiteral("kdenlive-timeline-thumbs"), 10000000);
    m_cache->clear();
    m_cache->setEvictionPolicy(KSharedDataCache::EvictOldest);
}

ThumbnailProvider::~ThumbnailProvider()
{
    delete m_cache;
}

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QImage result;

    // id is [hash]/mlt_service/resource#frameNumber
    int index = id.lastIndexOf('#');

    if (index != -1) {
        QString hash = id.section('/', 0, 0);
        QString service = id.section('/', 1, 1);
        QString resource = id.section('/', 2);
        int frameNumber = id.mid(index + 1).toInt();
        Mlt::Properties properties;

        // Scale the frameNumber to ThumbnailProvider profile's fps.
        //TODO: get profile from pCore
        //frameNumber = qRound(frameNumber / pCore->profile().fps() * m_profile.fps());

        qDebug()<<"--------- REQUESTED FRAME: "<<frameNumber;
        resource = resource.left(resource.lastIndexOf('#'));
        properties.set("_profile", m_profile.get_profile(), 0);

        //QString key = cacheKey(properties, service, resource, hash, frameNumber);
        //result = getCachedThumbnail(key);
        const QString key = hash + QString::number(frameNumber);
        if (!m_cache->findImage(key, &result)) {
            if (service == "avformat-novalidate")
                service = "avformat";
            else if (service.startsWith("xml"))
                service = "xml-nogl";
            Mlt::Producer producer(m_profile, service.toUtf8().constData(), resource.toUtf8().constData());
            if (producer.is_valid()) {
                result = makeThumbnail(producer, frameNumber, requestedSize);
                m_cache->insertImage(key, result);
            } else {
                qDebug()<<"INVALID PRODUCER; "<<service<<" / "<<resource;
            }
        }
        if (size)
            *size = result.size();
    }
    return result;
}

QString ThumbnailProvider::cacheKey(Mlt::Properties& properties, const QString& service,
                                    const QString& resource, const QString& hash, int frameNumber)
{
    QString time = properties.frames_to_time(frameNumber, mlt_time_clock);
    // Reduce the precision to centiseconds to increase chance for cache hit
    // without much loss of accuracy.
    time = time.left(time.size() - 1);
    QString key;
    if (hash.isEmpty()) {
        key = QString("%1 %2 %3")
                .arg(service)
                .arg(resource)
                .arg(time);
        QCryptographicHash hash2(QCryptographicHash::Sha1);
        hash2.addData(key.toUtf8());
        key = hash2.result().toHex();
    } else {
        key = QString("%1 %2").arg(hash).arg(time);
    }
    return key;
}

QImage ThumbnailProvider::makeThumbnail(Mlt::Producer &producer, int frameNumber, const QSize& requestedSize)
{
    Mlt::Filter scaler(m_profile, "swscale");
    Mlt::Filter padder(m_profile, "resize");
    Mlt::Filter converter(m_profile, "avcolor_space");
    int height = 100;
    int width = 100 * m_profile.dar();

    if (!requestedSize.isEmpty()) {
        width = requestedSize.width();
        height = requestedSize.height();
    }

    producer.attach(scaler);
    producer.attach(padder);
    producer.attach(converter);
    return KThumb::getFrame(producer, frameNumber, width, height);
}
