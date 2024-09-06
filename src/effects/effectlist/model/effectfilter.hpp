/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "assets/assetlist/model/assetfilter.hpp"
#include "definitions.h"
#include <memory>

/** @brief This class is used as a proxy model to filter the effect tree based on given criterion (name, type).
   It simply adds a filter of type
 */
class EffectFilter : public AssetFilter
{
    Q_OBJECT

public:
    friend class KdenliveTests;

    EffectFilter(QObject *parent = nullptr);

    /** @brief Manage the type filter
       @param enabled whether to enable this filter
       @param type Effect type to display
    */
    void setFilterType(bool enabled, AssetListType::AssetType type);
    void reloadFilterOnFavorite() override;

protected:
    bool filterType(const std::shared_ptr<TreeItem> &item) const;
    bool applyAll(std::shared_ptr<TreeItem> item) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    /** @brief Sets the deprecated category index (m_deprecatedCategory) if found. */
    void getDeprecatedCategory();

    bool m_type_enabled;
    AssetListType::AssetType m_type_value;
    /** @brief Index of the deprecated category (if any) to allow displaying it last in the list. */
    QPersistentModelIndex m_deprecatedCategory;
};
