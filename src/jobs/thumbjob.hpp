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

#pragma once

#include "abstractclipjob.h"

#include <QImage>
#include <QMutex>
#include <memory>

/* @brief This class represents the job that corresponds to computing the thumb of a clip
 */

class ProjectClip;
namespace Mlt {
class Producer;
}

class ThumbJob : public AbstractClipJob
{
    Q_OBJECT

public:
    /* @brief Extract a thumb for given clip.
       @param frameNumber is the frame to extract. Leave to -1 for default
       @param persistent: if true, we will use the persistent cache (for query and saving)
    */
    ThumbJob(const QString &binId, int frameNumber = -1, bool persistent = false, bool reloadAllThumbs = false);

    const QString getDescription() const override;

    bool startJob() override;

    /** @brief This is to be called after the job finished.
        By design, the job should store the result of the computation but not share it with the rest of the code. This happens when we call commitResult */
    bool commitResult(Fun &undo, Fun &redo) override;

private:
    int m_frameNumber;
    int m_imageHeight;
    int m_imageWidth;
    int m_fullWidth;

    std::shared_ptr<ProjectClip> m_binClip;
    std::shared_ptr<Mlt::Producer> m_prod;

    QImage m_result;
    QMutex m_mutex;
    bool m_done{false};
    bool m_persistent;
    bool m_reloadAll;
    bool m_inCache{false};
    bool m_subClip{false}; // true if we operate on a subclip
};
