/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "effectfilter.hpp"
#include "abstractmodel/treeitem.hpp"
#include "assets/assetlist/model/assettreemodel.hpp"
#include "effecttreemodel.hpp"

EffectFilter::EffectFilter(QObject *parent)
    : AssetFilter(parent)
{
    m_type_enabled = false;
}

void EffectFilter::setFilterType(bool enabled, AssetListType::AssetType type)
{
    m_type_enabled = enabled;
    m_type_value = type;
    invalidateFilter();
}

void EffectFilter::reloadFilterOnFavorite()
{
    if (m_type_enabled && m_type_value == AssetListType::AssetType::Favorites) {
        invalidateFilter();
    }
}

bool EffectFilter::filterType(const std::shared_ptr<TreeItem> &item) const
{
    auto itemType = item->dataColumn(AssetTreeModel::TypeCol).value<AssetListType::AssetType>();
    if (itemType == AssetListType::AssetType::Hidden) {
        return false;
    }
    if (!m_type_enabled) {
        return true;
    }
    if (m_type_value == AssetListType::AssetType::Favorites) {
        return item->dataColumn(AssetTreeModel::FavCol).toBool();
    }
    if (m_type_value == AssetListType::AssetType::Preferred) {
        return item->dataColumn(AssetTreeModel::PreferredCol).toBool();
    }
    if (m_type_value == AssetListType::AssetType::Custom) {
        return itemType == m_type_value || itemType == AssetListType::AssetType::CustomAudio || itemType == AssetListType::Template || itemType == AssetListType::TemplateAudio;
    }
    return itemType == m_type_value;
}

bool EffectFilter::applyAll(std::shared_ptr<TreeItem> item) const
{
    if (!m_name_value.isEmpty()) {
        if (m_type_value == AssetListType::AssetType::Preferred) {
            return filterName(item);
        }
        return filterType(item) && filterName(item);
    } else {
        return filterType(item);
    }
}
