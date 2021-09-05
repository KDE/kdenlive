/*
 *   SPDX-FileCopyrightText: 2017 Nicolas Carion *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#ifndef EFFECTFILTER_H
#define EFFECTFILTER_H

#include "assets/assetlist/model/assetfilter.hpp"
#include "effects/effectsrepository.hpp"
#include <memory>

/** @brief This class is used as a proxy model to filter the effect tree based on given criterion (name, type).
   It simply adds a filter of type
 */
class EffectFilter : public AssetFilter
{
    Q_OBJECT

public:
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

    bool m_type_enabled;
    AssetListType::AssetType m_type_value;
};
#endif
