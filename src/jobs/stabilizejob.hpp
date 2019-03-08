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
 * @class StabilizeJob
 * @brief Stabilize a clip using a mlt filter
 *
 */

class JobManager;
class StabilizeJob : public MeltJob
{
    Q_OBJECT

public:
    /** @brief Creates a stabilize job job for the given bin clip
        @brief filterName is the name of the actual melt filter to use
        @brief destUrl is the path to the file we are going to produce
        @brief filterParams is a map containing the xml parameters of the filter
     */
    StabilizeJob(const QString &binId, const QString &filterName, QString destUrl, std::unordered_map<QString, QString> filterparams);

    // This is a special function that prepares the stabilize job for a given list of clips.
    // Namely, it displays the required UI to configure the job and call startJob with the right set of parameters
    // Then the job is automatically put in queue. Its id is returned
    static int prepareJob(const std::shared_ptr<JobManager> &ptr, const std::vector<QString> &binIds, int parentId, QString undoString,
                          const QString &filterName);

    // Return the list of stabilization filters that we support
    static std::unordered_set<QString> supportedFilters();

    bool commitResult(Fun &undo, Fun &redo) override;
    const QString getDescription() const override;

protected:
    // @brief create and configure consumer
    void configureConsumer() override;

    // @brief create and configure filter
    void configureFilter() override;

protected:
    QString m_filterName;
    QString m_destUrl;
    std::unordered_map<QString, QString> m_filterParams;
};
