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
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QQuickImageProvider>
#include <mlt++/MltFilter.h>
#include <mlt++/MltProfile.h>

ThumbnailProvider::ThumbnailProvider()
    : QQuickImageProvider(QQmlImageProviderBase::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
    , m_profile(pCore->getCurrentProfilePath().toUtf8().constData())
{
    KImageCache::deleteCache(QStringLiteral("kdenlive-timeline-thumbs"));
    m_cache = new KImageCache(QStringLiteral("kdenlive-timeline-thumbs"), 10000000);
    m_cache->clear();
    m_cache->setEvictionPolicy(KSharedDataCache::EvictOldest);
    int width = 180 * m_profile.dar();
    width += width % 8;
    m_profile.set_height(180);
    m_profile.set_width(width);
    m_producers.setMaxCost(6);
}

ThumbnailProvider::~ThumbnailProvider()
{
    delete m_cache;
}

void ThumbnailProvider::resetProject()
{
    m_producers.clear();
    m_cache->clear();
}

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QImage result;

    // id is binID/mlt_service/resource#frameNumber
    int index = id.lastIndexOf('#');
    if (index != -1) {
        QString binId = id.section('/', 0, 0);
        QString service = id.section('/', 1, 1);
        QString resource = id.section('/', 2);
        int frameNumber = id.mid(index + 1).toInt();
        resource = resource.left(resource.lastIndexOf('#'));
        // properties.set("_profile", m_profile.get_profile(), 0);

        // QString key = cacheKey(properties, service, resource, hash, frameNumber);
        // result = getCachedThumbnail(key);
        const QString key = binId + "#" + QString::number(frameNumber);
        if (!m_cache->findImage(key, &result)) {
            if (service == "avformat-novalidate")
                service = "avformat";
            else if (service.startsWith("xml"))
                service = "xml-nogl";
            Mlt::Producer *producer = nullptr;
            if (m_producers.contains(binId.toInt())) {
                producer = m_producers.object(binId.toInt());
            } else {
                if (!resource.isEmpty()) {
                    producer = new Mlt::Producer(m_profile, service.toUtf8().constData(), resource.toUtf8().constData());
                } else {
                    producer = new Mlt::Producer(m_profile, service.toUtf8().constData());
                }
                std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(binId);
                if (binClip) {
                    std::shared_ptr<Mlt::Producer> projectProducer = binClip->originalProducer();
                    Mlt::Properties original(projectProducer->get_properties());
                    Mlt::Properties cloneProps(producer->get_properties());
                    cloneProps.pass_list(original, "video_index,force_aspect_num,force_aspect_den,force_aspect_ratio,force_fps,force_progressive,force_tff,"
                                                   "force_colorspace,set.force_full_luma,templatetext,autorotate,xmldata");
                }
                Mlt::Filter scaler(m_profile, "swscale");
                Mlt::Filter padder(m_profile, "resize");
                Mlt::Filter converter(m_profile, "avcolor_space");
                producer->attach(scaler);
                producer->attach(padder);
                producer->attach(converter);
                m_producers.insert(binId.toInt(), producer);
            }
            if ((producer != nullptr) && producer->is_valid()) {
                // result = KThumb::getFrame(producer, frameNumber, 0, 0);
                result = makeThumbnail(producer, frameNumber, requestedSize);
                m_cache->insertImage(key, result);
            } else {
                qDebug() << "INVALID PRODUCER; " << service << " / " << resource;
            }
        }
        if (size) *size = result.size();
    }
    return result;
}

QString ThumbnailProvider::cacheKey(Mlt::Properties &properties, const QString &service, const QString &resource, const QString &hash, int frameNumber)
{
    QString time = properties.frames_to_time(frameNumber, mlt_time_clock);
    // Reduce the precision to centiseconds to increase chance for cache hit
    // without much loss of accuracy.
    time = time.left(time.size() - 1);
    QString key;
    if (hash.isEmpty()) {
        key = QString("%1 %2 %3").arg(service).arg(resource).arg(time);
        QCryptographicHash hash2(QCryptographicHash::Sha1);
        hash2.addData(key.toUtf8());
        key = hash2.result().toHex();
    } else {
        key = QString("%1 %2").arg(hash).arg(time);
    }
    return key;
}

QImage ThumbnailProvider::makeThumbnail(Mlt::Producer *producer, int frameNumber, const QSize &requestedSize)
{
    producer->seek(frameNumber);
    Mlt::Frame *frame = producer->get_frame();
    mlt_image_format format = mlt_image_rgb24a;
    int ow = 0;
    int oh = 0;
    QImage result;
    const uchar *imagedata = frame->get_image(format, ow, oh);
    if (imagedata) {
        result = QImage(ow, oh, QImage::Format_RGBA8888);
        memcpy(result.bits(), imagedata, ow * oh * 4);
    }
    delete frame;
    return result;
}
