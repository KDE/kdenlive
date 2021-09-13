/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#include "transitionfilter.hpp"
#include "abstractmodel/treeitem.hpp"
#include "assets/assetlist/model/assettreemodel.hpp"

TransitionFilter::TransitionFilter(QObject *parent)
    : AssetFilter(parent)
{
    m_type_enabled = false;
}

void TransitionFilter::setFilterType(bool enabled, AssetListType::AssetType type)
{
    m_type_enabled = enabled;
    m_type_value = type;
    invalidateFilter();
}

void TransitionFilter::reloadFilterOnFavorite()
{
    if (m_type_enabled && m_type_value == AssetListType::AssetType::Favorites) {
        invalidateFilter();
    }
}

bool TransitionFilter::filterType(const std::shared_ptr<TreeItem> &item) const
{
    auto itemType = item->dataColumn(AssetTreeModel::typeCol).value<AssetListType::AssetType>();
    if (itemType == AssetListType::AssetType::Hidden) {
        return false;
    }
    if (!m_type_enabled) {
        return true;
    }
    if (m_type_value == AssetListType::AssetType::Favorites) {
        return item->dataColumn(AssetTreeModel::favCol).toBool();
    }
    return itemType == m_type_value;
}

bool TransitionFilter::applyAll(std::shared_ptr<TreeItem> item) const
{
    return filterName(item) && filterType(item);
}
