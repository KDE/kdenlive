/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "effectfilter.hpp"
#include "abstractmodel/treeitem.hpp"
#include "assets/assetlist/model/assettreemodel.hpp"
#include "effecttreemodel.hpp"
#include "kdenlivesettings.h"
#include <KLocalizedString>

EffectFilter::EffectFilter(QObject *parent)
    : AssetFilter(parent)
{
    m_type_enabled = false;
}

void EffectFilter::setFilterType(bool enabled, AssetListType::AssetType type)
{
    m_type_enabled = enabled;
    m_type_value = type;
    if (!m_deprecatedCategory.isValid()) {
        getDeprecatedCategory();
    }
    invalidateFilter();
}

bool EffectFilter::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    QString leftData = sourceModel()->data(left).toString();
    QString rightData = sourceModel()->data(right).toString();

    if (left == m_deprecatedCategory) {
        return false;
    } else if (right == m_deprecatedCategory) {
        return true;
    }
    return QString::localeAwareCompare(leftData, rightData) < 0;
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
        return itemType == m_type_value || itemType == AssetListType::AssetType::CustomAudio || itemType == AssetListType::TemplateCustom ||
               itemType == AssetListType::TemplateCustomAudio;
    }
    if (m_type_value == AssetListType::AssetType::Video) {
        return itemType == m_type_value || itemType == AssetListType::AssetType::Custom || itemType == AssetListType::Template ||
               itemType == AssetListType::TemplateCustom;
    }
    if (m_type_value == AssetListType::AssetType::Audio) {
        return itemType == m_type_value || itemType == AssetListType::AssetType::CustomAudio || itemType == AssetListType::TemplateAudio ||
               itemType == AssetListType::TemplateCustomAudio;
    }
    return itemType == m_type_value;
}

bool EffectFilter::applyAll(std::shared_ptr<TreeItem> item) const
{
    if (KdenliveSettings::effectsFilter() && KdenliveSettings::tenbitpipeline() &&
        item->dataColumn(AssetTreeModel::IdCol).toString() != QLatin1String("root")) {
        if (!item->dataColumn(AssetTreeModel::TenBitCol).toBool()) {
            return false;
        }
    }
    if (!m_name_value.isEmpty()) {
        if (m_type_value == AssetListType::AssetType::Preferred) {
            return filterName(item);
        }
        return filterType(item) && filterName(item);
    } else {
        return filterType(item);
    }
}

void EffectFilter::getDeprecatedCategory()
{
    auto *model = static_cast<AbstractTreeModel *>(sourceModel());
    for (int i = 0; i < sourceModel()->rowCount(); i++) {
        QModelIndex row = sourceModel()->index(i, 0, QModelIndex());
        std::shared_ptr<TreeItem> item = model->getItemById(int(row.internalId()));
        if (item->dataColumn(AssetTreeModel::NameCol).toString() == i18n("Deprecated")) {
            m_deprecatedCategory = row;
            break;
        }
    }
}
