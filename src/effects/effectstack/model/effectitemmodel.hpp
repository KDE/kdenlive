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

#ifndef EFFECTITEMMODEL_H
#define EFFECTITEMMODEL_H

#include "abstracteffectitem.hpp"
#include "abstractmodel/treeitem.hpp"
#include "assets/model/assetparametermodel.hpp"
#include <mlt++/MltFilter.h>

class EffectStackModel;
/* @brief This represents an effect of the effectstack
 */
class EffectItemModel : public AbstractEffectItem, public AssetParameterModel
{

public:
    /* This construct an effect of the given id
       @param is a ptr to the model this item belongs to. This is required to send update signals
     */
    static std::shared_ptr<EffectItemModel> construct(const QString &effectId, std::shared_ptr<AbstractTreeModel> stack, bool effectEnabled = true);
    /* This construct an effect with an already existing filter
       Only used when loading an existing clip
     */
    static std::shared_ptr<EffectItemModel> construct(std::unique_ptr<Mlt::Properties> effect, std::shared_ptr<AbstractTreeModel> stack, QString originalDecimalPoint);

    /* @brief This function plants the effect into the given service in last position
     */
    void plant(const std::weak_ptr<Mlt::Service> &service) override;
    void plantClone(const std::weak_ptr<Mlt::Service> &service) override;
    void loadClone(const std::weak_ptr<Mlt::Service> &service);
    /* @brief This function unplants (removes) the effect from the given service
     */
    void unplant(const std::weak_ptr<Mlt::Service> &service) override;
    void unplantClone(const std::weak_ptr<Mlt::Service> &service) override;

    Mlt::Filter &filter() const;

    /* @brief Return true if the effect applies only to audio */
    bool isAudio() const override;
    bool isUnique() const override;

    void setCollapsed(bool collapsed);
    bool isCollapsed();
    bool isValid() const;

protected:
    EffectItemModel(const QList<QVariant> &effectData, std::unique_ptr<Mlt::Properties> effect, const QDomElement &xml, const QString &effectId,
                    const std::shared_ptr<AbstractTreeModel> &stack, bool isEnabled = true, QString originalDecimalPoint = QString());
    QMap<int, std::shared_ptr<EffectItemModel>> m_childEffects;
    void updateEnable(bool updateTimeline = true) override;
    int m_childId;
};

#endif
