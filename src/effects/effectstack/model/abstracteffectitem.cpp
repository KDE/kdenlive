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

#include "abstracteffectitem.hpp"

#include "effects/effectsrepository.hpp"
#include "effectstackmodel.hpp"
#include <utility>

AbstractEffectItem::AbstractEffectItem(const QList<QVariant> &data, const std::shared_ptr<AbstractTreeModel> &stack, const std::shared_ptr<TreeItem> &parent)
    : TreeItem(data, stack, parent)
    , m_enabled(true)
    , m_effectStackEnabled(true)
{
}

void AbstractEffectItem::setEnabled(bool enabled)
{
    m_enabled = enabled;
    updateEnable();
}

void AbstractEffectItem::setEffectStackEnabled(bool enabled)
{
    m_effectStackEnabled = enabled;
    for (int i = 0; i < childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(child(i))->setEffectStackEnabled(enabled);
    }
    updateEnable();
}

bool AbstractEffectItem::isEnabled() const
{
    bool parentEnabled = true;
    if (auto ptr = std::static_pointer_cast<AbstractEffectItem>(m_parentItem.lock())) {
        parentEnabled = ptr->isEnabled();
    }
    return m_enabled && m_effectStackEnabled && parentEnabled;
}

