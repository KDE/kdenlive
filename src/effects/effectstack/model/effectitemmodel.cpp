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

#include "effectitemmodel.hpp"
#include "effects/effectsrepository.hpp"
#include "effectstackmodel.hpp"

EffectItemModel::EffectItemModel(const QList<QVariant> &data, Mlt::Properties *effect, const QDomElement &xml, const QString &effectId, EffectStackModel *stack)
    : TreeItem(data, static_cast<AbstractTreeModel *>(stack))
    , AssetParameterModel(effect, xml, effectId)
    , m_enabled(true)
    , m_timelineEffectsEnabled(true)
{
}

// static
EffectItemModel *EffectItemModel::construct(const QString &effectId, EffectStackModel *stack)
{
    Q_ASSERT(EffectsRepository::get()->exists(effectId));
    QDomElement xml = EffectsRepository::get()->getXml(effectId);

    Mlt::Properties *effect = EffectsRepository::get()->getEffect(effectId);

    QList<QVariant> data;
    data << EffectsRepository::get()->getName(effectId) << effectId;

    return new EffectItemModel(data, effect, xml, effectId, stack);
}

void EffectItemModel::plant(const std::weak_ptr<Mlt::Service> &service)
{
    if (auto ptr = service.lock()) {
        int ret = ptr->attach(filter());
        Q_ASSERT(ret == 0);
    } else {
        qDebug() << "Error : Cannot plant effect because parent service is not available anymore";
        Q_ASSERT(false);
    }
}

Mlt::Filter &EffectItemModel::filter() const
{
    return *static_cast<Mlt::Filter *>(m_asset.get());
}

void EffectItemModel::setEnabled(bool enabled)
{
    m_enabled = enabled;
    updateEnable();
}

void EffectItemModel::setTimelineEffectsEnabled(bool enabled)
{
    m_timelineEffectsEnabled = enabled;
    updateEnable();
}

bool EffectItemModel::isEnabled() const
{
    return m_enabled && m_timelineEffectsEnabled;
}

void EffectItemModel::updateEnable()
{
    filter().set("disable", isEnabled() ? 0 : 1);
}
