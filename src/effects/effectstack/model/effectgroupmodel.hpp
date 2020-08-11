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

#ifndef EFFECTGROUPMODEL_H
#define EFFECTGROUPMODEL_H

#include "abstracteffectitem.hpp"
#include "abstractmodel/treeitem.hpp"

class EffectStackModel;
/* @brief This represents a group of effects of the effectstack
 */
class EffectGroupModel : public AbstractEffectItem
{

public:
    /* This construct an effect of the given id
       @param is a ptr to the model this item belongs to. This is required to send update signals
     */
    static std::shared_ptr<EffectGroupModel> construct(const QString &name, std::shared_ptr<AbstractTreeModel> stack, bool isRoot = false);

    /* @brief Return true if the effect applies only to audio */
    bool isAudio() const override;
    bool isUnique() const override;

    /* @brief This function plants the effect into the given service in last position
     */
    void plant(const std::weak_ptr<Mlt::Service> &service) override;
    void plantClone(const std::weak_ptr<Mlt::Service> &service) override;
    /* @brief This function unplants (removes) the effect from the given service
     */
    void unplant(const std::weak_ptr<Mlt::Service> &service) override;
    void unplantClone(const std::weak_ptr<Mlt::Service> &service) override;

protected:
    EffectGroupModel(const QList<QVariant> &data, QString name, const std::shared_ptr<AbstractTreeModel> &stack, bool isRoot = false);

    void updateEnable(bool updateTimeline = true) override;

    QString m_name;
};

#endif
