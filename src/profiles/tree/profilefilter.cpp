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

#include "profilefilter.hpp"
#include "../profilemodel.hpp"
#include "../profilerepository.hpp"
#include "profiletreemodel.hpp"

ProfileFilter::ProfileFilter(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    m_interlaced_enabled = m_fps_enabled = false;
}

void ProfileFilter::setFilterInterlaced(bool enabled, bool interlaced)
{
    m_interlaced_enabled = enabled;
    m_interlaced_value = interlaced;
    invalidateFilter();
}

bool ProfileFilter::filterInterlaced(std::unique_ptr<ProfileModel> &ptr) const
{
    return !m_interlaced_enabled || ptr->progressive() != m_interlaced_value;
}

void ProfileFilter::setFilterFps(bool enabled, double fps)
{
    m_fps_enabled = enabled;
    m_fps_value = fps;
    invalidateFilter();
}

bool ProfileFilter::filterFps(std::unique_ptr<ProfileModel> &ptr) const
{
    return !m_fps_enabled || qFuzzyCompare(ptr->fps(), m_fps_value);
}

bool ProfileFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (sourceParent == QModelIndex()) {
        // In that case, we have a category. We hide it if it does not have children.
        QModelIndex category = sourceModel()->index(sourceRow, 0, sourceParent);
        bool accepted = false;
        for (int i = 0; i < sourceModel()->rowCount(category) && !accepted; ++i) {
            accepted = filterAcceptsRow(i, category);
        }
        return accepted;
    }
    QModelIndex row = sourceModel()->index(sourceRow, 0, sourceParent);
    QString profile_path = static_cast<ProfileTreeModel *>(sourceModel())->getProfile(row);
    if (profile_path.isEmpty()) {
        return true;
    }

    std::unique_ptr<ProfileModel> &profile = ProfileRepository::get()->getProfile(profile_path);

    return filterInterlaced(profile) && filterFps(profile);
}

bool ProfileFilter::isVisible(const QModelIndex &sourceIndex)
{
    auto parent = sourceModel()->parent(sourceIndex);
    return filterAcceptsRow(sourceIndex.row(), parent);
}
