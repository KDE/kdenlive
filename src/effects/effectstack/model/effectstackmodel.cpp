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
#include "effectstackmodel.hpp"
#include "effectitemmodel.hpp"


EffectStackModel::EffectStackModel(std::weak_ptr<Mlt::Service> service) :
    AbstractTreeModel()
    , m_service(service)
    , m_timelineEffectsEnabled(true)
{
}

std::shared_ptr<EffectStackModel> EffectStackModel::construct(std::weak_ptr<Mlt::Service> service)
{
    return std::make_shared<EffectStackModel>(service);
}


void EffectStackModel::appendEffect(const QString& effectId)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    auto effect = EffectItemModel::construct(effectId);
    effect->setTimelineEffectsEnabled(m_timelineEffectsEnabled);
    rootItem->appendChild(effect);
    endInsertRows();
}

void EffectStackModel::setTimelineEffectsEnabled(bool enabled)
{
    m_timelineEffectsEnabled = enabled;

    //Recursively updates children states
    for (int i = 0; i < rootItem->childCount(); ++i) {
        static_cast<EffectItemModel*>(rootItem->child(i))->setTimelineEffectsEnabled(enabled);
    }
}
