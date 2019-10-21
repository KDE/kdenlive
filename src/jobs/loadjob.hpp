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

#include <QDomElement>
#include <memory>

/* @brief This class represents the job that corresponds to loading a clip from xml
 */

class ProjectClip;
namespace Mlt {
class Producer;
}

class LoadJob : public AbstractClipJob
{
    Q_OBJECT

public:
    /* @brief Extract a thumb for given clip.
       @param frameNumber is the frame to extract. Leave to -1 for default
       @param persistent: if true, we will use the persistent cache (for query and saving)
    */
    LoadJob(const QString &binId, const QDomElement &xml, const std::function<void()> &readyCallBack = []() {});

    const QString getDescription() const override;

    bool startJob() override;

    /** @brief This is to be called after the job finished.
        By design, the job should store the result of the computation but not share it with the rest of the code. This happens when we call commitResult */
    bool commitResult(Fun &undo, Fun &redo) override;

    // Do some checks on the profile
    static void checkProfile(const QString &clipId, const QDomElement &xml, const std::shared_ptr<Mlt::Producer> &producer);

protected:
    // helper to load some kind of resources such as color. This will modify resource if needs be (for eg., in the case of color, it will prepend "color:" if
    // needed)
    static std::shared_ptr<Mlt::Producer> loadResource(QString resource, const QString &type);

    std::shared_ptr<Mlt::Producer> loadPlaylist(QString &resource);

    // Create the required filter for a slideshow
    void processSlideShow();

    // This should be called from commitResult (that is, from the GUI thread) to deal with multi stream videos
    void processMultiStream();

private:
    QDomElement m_xml;

    bool m_done{false}, m_successful{false};
    std::function<void()> m_readyCallBack;

    std::shared_ptr<Mlt::Producer> m_producer;
    QList<int> m_audio_list, m_video_list;
    QString m_resource;
};
