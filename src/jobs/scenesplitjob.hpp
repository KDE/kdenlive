/***************************************************************************
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *                                                                         *
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

#include "meltjob.h"
#include <unordered_map>
#include <unordered_set>

/**
 * @class SceneSplitJob
 * @brief Detects the scenes of a clip using a mlt filter
 *
 */

class JobManager;
class SceneSplitJob : public MeltJob
{
    Q_OBJECT

public:
    /** @brief Creates a scenesplit job for the given bin clip
        @param subClips if true, we create a subclip per found scene
        @param markersType The type of markers that will be created to denote scene. Leave -1 for no markers
     */
    SceneSplitJob(const QString &binId, bool subClips, int markersType = -1, int minInterval = 0);

    // This is a special function that prepares the stabilize job for a given list of clips.
    // Namely, it displays the required UI to configure the job and call startJob with the right set of parameters
    // Then the job is automatically put in queue. Its id is returned
    static int prepareJob(const std::shared_ptr<JobManager> &ptr, const std::vector<QString> &binIds, int parentId, QString undoString);

    bool commitResult(Fun &undo, Fun &redo) override;
    const QString getDescription() const override;

protected:
    // @brief create and configure consumer
    void configureConsumer() override;

    // @brief create and configure filter
    void configureFilter() override;

    // @brief extra configuration of the profile (eg: resize the profile)
    void configureProfile() override;

    bool m_subClips;
    int m_markersType;
    // @brief minimum scene duration.
    int m_minInterval;
};
