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

#ifndef PROFILEFILTER_H
#define PROFILEFILTER_H

#include <QSortFilterProxyModel>
#include <memory>

class ProfileModel;
/* @brief This class is used as a proxy model to filter the profile tree based on given criterion (fps, interlaced,...)
 */
class ProfileFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    ProfileFilter(QObject *parent = nullptr);

    /* @brief Manage the interlaced filter
       @param enabled whether to enable this filter
       @param interlaced whether we keep interlaced profiles or not
    */
    void setFilterInterlaced(bool enabled, bool interlaced);

    /* @brief Manage the fps filter
       @param enabled whether to enable this filter
       @param fps value of the fps of the profiles to keep
    */
    void setFilterFps(bool enabled, double fps);

    /** @brief Returns true if the ModelIndex in the source model is visible after filtering
     */
    bool isVisible(const QModelIndex &sourceIndex);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool filterInterlaced(std::unique_ptr<ProfileModel> &ptr) const;
    bool filterFps(std::unique_ptr<ProfileModel> &ptr) const;

    bool m_interlaced_enabled;
    bool m_interlaced_value;

    bool m_fps_enabled;
    double m_fps_value;
};
#endif
