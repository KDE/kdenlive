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

#ifndef EFFECTFILTER_H
#define EFFECTFILTER_H

#include <QSortFilterProxyModel>
#include <memory>
#include "effects/effectsrepository.hpp"

class TreeItem;
/* @brief This class is used as a proxy model to filter the effect tree based on given criterion (name, type)
 */
class EffectFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    EffectFilter(QObject *parent = nullptr);

    /* @brief Manage the name filter
       @param enabled whether to enable this filter
       @param pattern to match against effects' names
    */
    void setFilterName(bool enabled, const QString& pattern);

    /* @brief Manage the type filter
       @param enabled whether to enable this filter
       @param type Effect type to display
    */
    void setFilterType(bool enabled, EffectType type);

    /** @brief Returns true if the ModelIndex in the source model is visible after filtering
     */
    bool isVisible(const QModelIndex &sourceIndex);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool filterName(TreeItem* item) const;
    bool filterType(TreeItem* item) const;

    bool m_name_enabled;
    QString m_name_value;

    bool m_type_enabled;
    EffectType m_type_value;
};
#endif
