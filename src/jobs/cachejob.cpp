/***************************************************************************
 *   Copyright (C) 2019 by Jean-Baptiste Mardelle                          *
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

#include "cachejob.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "bin/projectsubclip.h"
#include "core.h"
#include "doc/kthumb.h"
#include "klocalizedstring.h"
#include "macros.hpp"
#include "utils/thumbnailcache.hpp"
#include <mlt++/MltProducer.h>

#include <set>

#include <QImage>
#include <QScopedPointer>
#include <QThread>
#include <QtConcurrent>

CacheJob::CacheJob(const QString &binId, int thumbsCount, int inPoint, int outPoint)
    : AbstractClipJob(CACHEJOB, binId)
    , m_fullWidth(qFuzzyCompare(pCore->getCurrentSar(), 1.0) ? 0 : pCore->thumbProfile()->height() * pCore->getCurrentDar() + 0.5)
    , m_semaphore(1)
    , m_done(false)
    , m_thumbsCount(thumbsCount)
    , m_inPoint(inPoint)
    , m_outPoint(outPoint)

{
    if (m_fullWidth % 2 > 0) {
        m_fullWidth ++;
    }
    connect(this, &CacheJob::jobCanceled, [&] () {
        if (m_done) {
            return;
        }
        m_done = true;
        m_clipId.clear();
        m_semaphore.acquire();
    });
}

const QString CacheJob::getDescription() const
{
    return i18n("Extracting thumbs from clip %1", m_clipId);
}

bool CacheJob::startJob()
{
    // We reload here, because things may have changed since creation of this job
    if (m_done) {
        // Job aborted
        return false;
    }
    m_binClip = pCore->projectItemModel()->getClipByBinID(m_clipId);
    if (m_binClip == nullptr) {
        // Clip was deleted
        return false;
    }
    if (m_binClip->clipType() != ClipType::Video && m_binClip->clipType() != ClipType::AV && m_binClip->clipType() != ClipType::Playlist) {
        // Don't create thumbnail for audio clips
        m_done = true;
        return true;
    }
    m_prod = m_binClip->thumbProducer();
    if ((m_prod == nullptr) || !m_prod->is_valid()) {
        qDebug() << "********\nCOULD NOT READ THUMB PRODUCER\n********";
        return false;
    }
    int duration = m_outPoint > 0 ? m_outPoint - m_inPoint : (int)m_binClip->frameDuration();
    std::set<int> frames;
    int steps = qCeil(qMax(pCore->getCurrentFps(), (double)duration / m_thumbsCount));
    int pos = m_inPoint;
    for (int i = 1; i <= m_thumbsCount && pos <= m_inPoint + duration; ++i) {
        frames.insert(pos);
        pos = m_inPoint + (steps * i);
    }
    int size = (int)frames.size();
    int count = 0;
    for (int i : frames) {
        emit jobProgress(100 * count / size);
        count++;
        if (m_clipId.isEmpty() || ThumbnailCache::get()->hasThumbnail(m_clipId, i)) {
            continue;
        }
        if (m_done || !m_semaphore.tryAcquire(1)) {
            m_semaphore.release();
            break;
        }
        m_prod->seek(i);
        QScopedPointer<Mlt::Frame> frame(m_prod->get_frame());
        frame->set("deinterlace_method", "onefield");
        frame->set("top_field_first", -1);
        frame->set("rescale.interp", "nearest");
        if (frame != nullptr && frame->is_valid()) {
            QImage result = KThumb::getFrame(frame.data(), 0, 0, m_fullWidth);
            QtConcurrent::run(ThumbnailCache::get().get(), &ThumbnailCache::storeThumbnail, m_clipId, i, result, true);
        }
        m_semaphore.release(1);
    }
    m_done = true;
    return true;
}

bool CacheJob::commitResult(Fun &undo, Fun &redo)
{
    Q_UNUSED(undo)
    Q_UNUSED(redo)
    Q_ASSERT(!m_resultConsumed);
    m_resultConsumed = true;
    return m_done;
}
