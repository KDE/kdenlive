/*
    SPDX-FileCopyrightText: 2013-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2013-2016 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "thumbnailprovider.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kthumb.h"
#include "utils/thumbnailcache.hpp"

#include <QCryptographicHash>
#include <QDebug>
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
        std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(binId);
        if (binClip) {
            int duration = binClip->frameDuration();
            if (frameNumber > duration) {
                // for endless loopable clips, we rewrite the position
                frameNumber = frameNumber - ((frameNumber / duration) * duration);
            }
            result = ThumbnailCache::get()->getThumbnail(binClip->hashForThumbs(), binId, frameNumber);
            if (!result.isNull()) {

                *size = result.size();
                return result;
            }
            std::unique_ptr<Mlt::Producer> prod = binClip->getThumbProducer();
            if (prod && prod->is_valid()) {
                Mlt::Profile *prodProfile = (binClip->clipType() == ClipType::Timeline || binClip->clipType() == ClipType::Playlist)
                                                ? &pCore->getProjectProfile()
                                                : &pCore->thumbProfile();
                Mlt::Filter scaler(*prodProfile, "swscale");
                Mlt::Filter padder(*prodProfile, "resize");
                Mlt::Filter converter(*prodProfile, "avcolor_space");
                prod->attach(scaler);
                prod->attach(padder);
                prod->attach(converter);
                result = makeThumbnail(std::move(prod), frameNumber, requestedSize);
                ThumbnailCache::get()->storeThumbnail(binId, frameNumber, result, false);
            }
        }
    }
    if (size) *size = result.size();
    return result;
}

QImage ThumbnailProvider::makeThumbnail(std::unique_ptr<Mlt::Producer> producer, int frameNumber, const QSize &requestedSize)
{
    Q_UNUSED(requestedSize)
    producer->seek(frameNumber);
    std::unique_ptr<Mlt::Frame> frame(producer->get_frame());
    if (frame == nullptr || !frame->is_valid()) {
        return QImage();
    }
    // TODO: cache these values ?
    frame->set("consumer.deinterlacer", "onefield");
    frame->set("consumer.top_field_first", -1);
    frame->set("consumer.rescale", "nearest");
    int imageHeight = pCore->thumbProfile().height();
    int imageWidth = pCore->thumbProfile().width();
    int fullWidth = qRound(imageHeight * pCore->getCurrentDar());
    return KThumb::getFrame(frame.get(), imageWidth, imageHeight, fullWidth);
}
