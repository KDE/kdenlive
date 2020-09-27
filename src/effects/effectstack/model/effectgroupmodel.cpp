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

#include "effectgroupmodel.hpp"

#include "effectstackmodel.hpp"
#include <utility>

EffectGroupModel::EffectGroupModel(const QList<QVariant> &data, QString name, const std::shared_ptr<AbstractTreeModel> &stack, bool isRoot)
    : AbstractEffectItem(EffectItemType::Group, data, stack, isRoot)
    , m_name(std::move(name))
{
}

// static
std::shared_ptr<EffectGroupModel> EffectGroupModel::construct(const QString &name, std::shared_ptr<AbstractTreeModel> stack, bool isRoot)
{
    QList<QVariant> data;
    data << name << name;

    std::shared_ptr<EffectGroupModel> self(new EffectGroupModel(data, name, stack, isRoot));

    baseFinishConstruct(self);

    return self;
}

void EffectGroupModel::updateEnable(bool updateTimeline)
{
    Q_UNUSED(updateTimeline);
    for (int i = 0; i < childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(child(i))->updateEnable();
    }
}

bool EffectGroupModel::isAudio() const
{
    bool result = false;
    for (int i = 0; i < childCount() && !result; ++i) {
        result = result || std::static_pointer_cast<AbstractEffectItem>(child(i))->isAudio();
    }
    return result;
}

bool EffectGroupModel::isUnique() const
{
    return false;
}

void EffectGroupModel::plant(const std::weak_ptr<Mlt::Service> &service)
{
    for (int i = 0; i < childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(child(i))->plant(service);
    }
}
void EffectGroupModel::plantClone(const std::weak_ptr<Mlt::Service> &service)
{
    for (int i = 0; i < childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(child(i))->plantClone(service);
    }
}
void EffectGroupModel::unplant(const std::weak_ptr<Mlt::Service> &service)
{
    for (int i = 0; i < childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(child(i))->unplant(service);
    }
}
void EffectGroupModel::unplantClone(const std::weak_ptr<Mlt::Service> &service)
{
    for (int i = 0; i < childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(child(i))->unplantClone(service);
    }
}
