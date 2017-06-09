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
#include "effects/effectsrepository.hpp"

#include "effectitemmodel.hpp"
#include "effectgroupmodel.hpp"
#include "core.h"
#include <utility>

EffectStackModel::EffectStackModel(std::weak_ptr<Mlt::Service> service)
    : AbstractTreeModel()
    , m_service(std::move(service))
    , m_effectStackEnabled(true)
{
}

std::shared_ptr<EffectStackModel> EffectStackModel::construct(std::weak_ptr<Mlt::Service> service)
{
    std::shared_ptr<EffectStackModel> self(new EffectStackModel(std::move(service)));
    self->rootItem = EffectGroupModel::construct(QStringLiteral("root"), self, std::shared_ptr<TreeItem>());
    return self;
}

void EffectStackModel::appendEffect(const QString &effectId, int cid)
{
    auto effect = EffectItemModel::construct(effectId, shared_from_this(), rootItem);
    bool isAudioEffect = EffectsRepository::get()->getType(effectId) == EffectType::Audio;
    Fun undo = [effect, cid, this]() {
        QModelIndex ix = this->getIndexFromItem(effect);
        this->rootItem->removeChild(effect);
        effect->unplant(this->m_service);
        this->dataChanged(ix, ix, QVector<int>());
        pCore->refreshProjectItem(cid);
        return true; 
    };
    Fun redo = [effect, cid, this]() {
        effect->setParentId(cid);
        effect->plant(this->m_service);
        this->rootItem->appendChild(effect);
        effect->setEffectStackEnabled(m_effectStackEnabled);
        QModelIndex ix = this->getIndexFromItem(effect);
        connect(effect.get(), &EffectItemModel::dataChanged, this, &EffectStackModel::dataChanged);
        this->dataChanged(ix, ix, QVector<int>());
        pCore->refreshProjectItem(cid);
        return true; 
    };
    redo();

    QString effectName = EffectsRepository::get()->getName(effectId);
    pCore->pushUndo(undo, redo, i18n("Add effect %1", effectName));
}

void EffectStackModel::setEffectStackEnabled(bool enabled)
{
    m_effectStackEnabled = enabled;

    // Recursively updates children states
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::static_pointer_cast<EffectItemModel>(rootItem->child(i))->setEffectStackEnabled(enabled);
    }
}

std::shared_ptr<EffectItemModel> EffectStackModel::effect(int row)
{
    return std::static_pointer_cast<EffectItemModel>(rootItem->child(row));
}

