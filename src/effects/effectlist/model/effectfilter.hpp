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

#include "assets/assetlist/model/assetfilter.hpp"
#include "effects/effectsrepository.hpp"
#include <memory>

/* @brief This class is used as a proxy model to filter the effect tree based on given criterion (name, type).
   It simply adds a filter of type
 */
class EffectFilter : public AssetFilter
{
    Q_OBJECT

public:
    EffectFilter(QObject *parent = nullptr);

    /* @brief Manage the type filter
       @param enabled whether to enable this filter
       @param type Effect type to display
    */
    void setFilterType(bool enabled, AssetListType::AssetType type);
    void reloadFilterOnFavorite() override;

protected:
    bool filterType(const std::shared_ptr<TreeItem> &item) const;
    bool applyAll(std::shared_ptr<TreeItem> item) const override;

    bool m_type_enabled;
    AssetListType::AssetType m_type_value;
};
#endif
