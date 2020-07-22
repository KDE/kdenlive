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
#include "utils/thumbnailcache.hpp"
#include "doc/kthumb.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QQuickImageProvider>
#include <mlt++/MltFilter.h>
#include <mlt++/MltProfile.h>

ThumbnailProvider::ThumbnailProvider()
    : QQuickImageProvider(QQmlImageProviderBase::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
{
}

ThumbnailProvider::~ThumbnailProvider() = default;

QImage ThumbnailProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QImage result;
    // id is binID/#frameNumber
    QString binId = id.section('/', 0, 0);
    bool ok;
    int frameNumber = id.section('#', -1).toInt(&ok);
    if (ok) {
        if (ThumbnailCache::get()->hasThumbnail(binId, frameNumber, false)) {
            result = ThumbnailCache::get()->getThumbnail(binId, frameNumber);
            *size = result.size();
            return result;
        }
        std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(binId);
        if (binClip) {
            std::shared_ptr<Mlt::Producer> prod = binClip->thumbProducer();
            if (prod && prod->is_valid()) {
                result = makeThumbnail(prod, frameNumber, requestedSize);
                ThumbnailCache::get()->storeThumbnail(binId, frameNumber, result, false);
            }
        }
    }
    if (size) *size = result.size();
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
        key = QString("%1 %2 %3").arg(service).arg(resource, time);
        QCryptographicHash hash2(QCryptographicHash::Sha1);
        hash2.addData(key.toUtf8());
        key = hash2.result().toHex();
    } else {
        key = QString("%1 %2").arg(hash, time);
    }
    return key;
}

QImage ThumbnailProvider::makeThumbnail(const std::shared_ptr<Mlt::Producer> &producer, int frameNumber, const QSize &requestedSize)
{
    Q_UNUSED(requestedSize)
    producer->seek(frameNumber);
    QScopedPointer<Mlt::Frame> frame(producer->get_frame());
    if (frame == nullptr || !frame->is_valid()) {
        return QImage();
    }
    // TODO: cache these values ?
    int imageHeight = pCore->thumbProfile()->height();
    int imageWidth = pCore->thumbProfile()->width();
    int fullWidth = imageHeight * pCore->getCurrentDar() + 0.5;
    return KThumb::getFrame(frame.data(), imageWidth, imageHeight, fullWidth);
}
