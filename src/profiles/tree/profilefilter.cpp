/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

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
