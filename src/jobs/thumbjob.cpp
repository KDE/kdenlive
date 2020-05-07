/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "thumbjob.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "bin/projectsubclip.h"
#include "core.h"
#include "doc/kthumb.h"
#include "klocalizedstring.h"
#include "macros.hpp"
#include "utils/thumbnailcache.hpp"
#include <QPainter>
#include <QScopedPointer>
#include <mlt++/MltProducer.h>

ThumbJob::ThumbJob(const QString &binId, int frameNumber, bool persistent, bool reloadAllThumbs)
    : AbstractClipJob(THUMBJOB, binId)
    , m_frameNumber(frameNumber)
    , m_imageHeight(pCore->thumbProfile()->height())
    , m_imageWidth(pCore->thumbProfile()->width())
    , m_fullWidth(m_imageHeight * pCore->getCurrentDar() + 0.5)
    , m_persistent(persistent)
    , m_reloadAll(reloadAllThumbs)

{
    if (m_fullWidth % 2 > 0) {
        m_fullWidth ++;
    }
    m_imageHeight += m_imageHeight % 2;
    auto item = pCore->projectItemModel()->getItemByBinId(binId);
    Q_ASSERT(item->itemType() == AbstractProjectItem::ClipItem || item->itemType() == AbstractProjectItem::SubClipItem);
    if (item->itemType() == AbstractProjectItem::ClipItem) {
        m_binClip = pCore->projectItemModel()->getClipByBinID(binId);
    } else if (item->itemType() == AbstractProjectItem::SubClipItem) {
        m_subClip = true;
    }
    connect(this, &ThumbJob::jobCanceled, [&] () {
        QMutexLocker lk(&m_mutex);
        m_done = true;
        m_clipId.clear();
    });
}

const QString ThumbJob::getDescription() const
{
    return i18n("Extracting thumb at frame %1 from clip %2", m_frameNumber, m_clipId);
}

bool ThumbJob::startJob()
{
    if (m_done) {
        return true;
    }
    // We reload here, because things may have changed since creation of this job
    if (m_subClip) {
        auto item = pCore->projectItemModel()->getItemByBinId(m_clipId);
        if (item == nullptr) {
            // Clip was deleted
            return false;
        }
        m_binClip = std::static_pointer_cast<ProjectClip>(item->parent());
        m_frameNumber = item->zone().x();
    } else {
        m_binClip = pCore->projectItemModel()->getClipByBinID(m_clipId);
        if (m_binClip == nullptr) {
            // Clip was deleted
            return false;
        }
        if (m_frameNumber < 0) {
            m_frameNumber = qMax(0, m_binClip->getProducerIntProperty(QStringLiteral("kdenlive:thumbnailFrame")));
        }
    }
    if (m_binClip->clipType() == ClipType::Audio) {
        // Don't create thumbnail for audio clips
        m_done = false;
        return true;
    }
    m_inCache = false;
    if (ThumbnailCache::get()->hasThumbnail(m_clipId, m_frameNumber, !m_persistent)) {
        m_done = true;
        m_result = ThumbnailCache::get()->getThumbnail(m_clipId, m_frameNumber);
        m_inCache = true;
        return true;
    }
    m_mutex.lock();
    m_prod = m_binClip->thumbProducer();
    if ((m_prod == nullptr) || !m_prod->is_valid()) {
        qDebug() << "********\nCOULD NOT READ THUMB PRODUCER\n********";
        return false;
    }
    int max = m_prod->get_length();
    m_frameNumber = m_binClip->clipType() == ClipType::Image ? 0 : qMin(m_frameNumber, max - 1);

    if (m_frameNumber > 0) {
        m_prod->seek(m_frameNumber);
    }
    if (m_done) {
        m_mutex.unlock();
        return true;
    }
    QScopedPointer<Mlt::Frame> frame(m_prod->get_frame());
    frame->set("deinterlace_method", "onefield");
    frame->set("top_field_first", -1);
    frame->set("rescale.interp", "nearest");
    if ((frame != nullptr) && frame->is_valid()) {
        m_result = KThumb::getFrame(frame.data(), m_imageWidth, m_imageHeight, m_fullWidth);
        m_done = true;
    }
    m_mutex.unlock();
    return m_done;
}

bool ThumbJob::commitResult(Fun &undo, Fun &redo)
{
    Q_ASSERT(!m_resultConsumed);
    if (!m_done) {
        if (m_binClip->clipType() == ClipType::Audio) {
            // audio files get standard audio icon, ok
            return true;
        }
        qDebug() << "ERROR: Trying to consume invalid results";
        return false;
    }
    if (!m_inCache) {
        if (m_result.isNull()) {
            qDebug() << "+++++\nINVALID RESULT IMAGE\n++++++++++++++";
            m_result = QImage(m_fullWidth, m_imageHeight, QImage::Format_ARGB32_Premultiplied);
            m_result.fill(Qt::red);
            QPainter p(&m_result);
            p.setPen(Qt::white);
            p.drawText(0, 0, m_fullWidth, m_imageHeight, Qt::AlignCenter, i18n("Invalid"));
        } else {
            ThumbnailCache::get()->storeThumbnail(m_clipId, m_frameNumber, m_result, m_persistent);
        }
    }
    m_resultConsumed = true;

    // TODO a refactor of ProjectClip and ProjectSubClip should make that possible without branching (both classes implement setThumbnail)
    bool ok = false;
    if (m_subClip) {
        auto subClip = std::static_pointer_cast<ProjectSubClip>(pCore->projectItemModel()->getItemByBinId(m_clipId));
        if (subClip == nullptr) {
            // Subclip was deleted
            return ok;
        }
        QImage old = subClip->thumbnail(m_result.width(), m_result.height()).toImage();

        // note that the image is moved into lambda, it won't be available from this class anymore
        auto operation = [clip = subClip, image = std::move(m_result)]() {
            clip->setThumbnail(image);
            return true;
        };
        auto reverse = [clip = subClip, image = std::move(old)]() {
            clip->setThumbnail(image);
            return true;
        };
        ok = operation();
        if (ok) {
            UPDATE_UNDO_REDO_NOLOCK(operation, reverse, undo, redo);
        }
    } else {
        QImage old = m_binClip->thumbnail(m_result.width(), m_result.height()).toImage();

        // note that the image is moved into lambda, it won't be available from this class anymore
        auto operation = [clip = m_binClip, image = std::move(m_result), this]() {
            clip->setThumbnail(image);
            if (m_reloadAll) {
                clip->updateTimelineClips({TimelineModel::ReloadThumbRole});
            }
            return true;
        };
        auto reverse = [clip = m_binClip, image = std::move(old), this]() {
            clip->setThumbnail(image);
            if (m_reloadAll) {
                clip->updateTimelineClips({TimelineModel::ReloadThumbRole});
            }
            return true;
        };
        ok = operation();
        if (ok) {
            UPDATE_UNDO_REDO_NOLOCK(operation, reverse, undo, redo);
        }
    }
    return ok;
}
