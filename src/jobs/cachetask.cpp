/*
Copyright (C) 2021  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cachetask.h"
#include "core.h"
#include "bin/projectitemmodel.h"
#include "bin/projectclip.h"
#include "kdenlivesettings.h"
#include "doc/kthumb.h"
#include "utils/thumbnailcache.hpp"

#include "xml/xml.hpp"
#include <QString>
#include <QImage>
#include <QFile>
#include <QtMath>
#include <klocalizedstring.h>
#include <set>

CacheTask::CacheTask(const ObjectId &owner, int thumbsCount, int in, int out, QObject* object)
    : AbstractTask(owner, AbstractTask::CACHEJOB, object)
    , m_fullWidth(qFuzzyCompare(pCore->getCurrentSar(), 1.0) ? 0 : int(pCore->thumbProfile()->height() * pCore->getCurrentDar() + 0.5))
    , m_thumbsCount(thumbsCount)
    , m_in(in)
    , m_out(out)
{
    if (m_fullWidth % 2 > 0) {
        m_fullWidth ++;
    }
}

CacheTask::~CacheTask()
{
}

void CacheTask::start(const ObjectId &owner, int thumbsCount, int in, int out, QObject* object, bool force)
{
    CacheTask* task = new CacheTask(owner, thumbsCount, in, out, object);
    if (pCore->taskManager.hasPendingJob(owner, AbstractTask::CACHEJOB)) {
        delete task;
        task = 0;
    }
    if (task) {
        // Otherwise, start a new audio levels generation thread.
        task->m_isForce = force;
        pCore->taskManager.startTask(owner.second, task);
    }
}

void CacheTask::generateThumbnail(std::shared_ptr<ProjectClip>binClip)
{
        // Fetch thumbnail
    if (binClip->clipType() != ClipType::Audio) {
        std::shared_ptr<Mlt::Producer> thumbProd(nullptr);
        int duration = m_out > 0 ? m_out - m_in : binClip->getFramePlaytime();
        std::set<int> frames;
        int steps = qCeil(qMax(pCore->getCurrentFps(), double(duration) / m_thumbsCount));
        int pos = m_in;
        for (int i = 1; i <= m_thumbsCount && pos <= m_in + duration; ++i) {
            frames.insert(pos);
            pos = m_in + (steps * i);
        }
        int size = int(frames.size());
        int count = 0;
        const QString clipId = QString::number(m_owner.second);
        for (int i : frames) {
            m_progress = 100 * count / size;
            QMetaObject::invokeMethod(m_object, "updateJobProgress");
            count++;
            if (m_isCanceled) {
                break;
            }
            if (ThumbnailCache::get()->hasThumbnail(clipId, i)) {
                continue;
            }
            if (thumbProd == nullptr) {
                thumbProd = binClip->thumbProducer();
            }
            if (thumbProd == nullptr) {
                // Thumb producer not available
                break;
            }
            thumbProd->seek(i);
            QScopedPointer<Mlt::Frame> frame(thumbProd->get_frame());
            frame->set("deinterlace_method", "onefield");
            frame->set("top_field_first", -1);
            frame->set("rescale.interp", "nearest");
            if (frame != nullptr && frame->is_valid()) {
                QImage result = KThumb::getFrame(frame.data(), 0, 0, m_fullWidth);
                if (!result.isNull()) {
                    qDebug()<<"==== CACHING FRAME: "<<i;
                    ThumbnailCache::get()->storeThumbnail(clipId, i, result, true);
                }
            }
        }
    }
}


void CacheTask::run()
{
    if (!m_isCanceled) {
        auto binClip = pCore->projectItemModel()->getClipByBinID(QString::number(m_owner.second));
        if (binClip) {
            generateThumbnail(binClip);
        }
    }
    pCore->taskManager.taskDone(m_owner.second, this);
    return;
}
