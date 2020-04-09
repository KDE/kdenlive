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

#include "core.h"
#include "effects/effectsrepository.hpp"
#include "effectstackmodel.hpp"
#include <KLocalizedString>
#include <utility>

AbstractEffectItem::AbstractEffectItem(EffectItemType type, const QList<QVariant> &data, const std::shared_ptr<AbstractTreeModel> &stack, bool isRoot,
                                       bool isEnabled)
    : TreeItem(data, stack, isRoot)
    , m_effectItemType(type)
    , m_enabled(isEnabled)
    , m_effectStackEnabled(true)
{
}

void AbstractEffectItem::markEnabled(const QString &name, bool enabled)
{
    Fun undo = [this, enabled]() {
        setEnabled(!enabled);
        return true;
    };
    Fun redo = [this, enabled]() {
        setEnabled(enabled);
        return true;
    };
    redo();
    pCore->pushUndo(undo, redo, enabled ? i18n("Enable %1", name) : i18n("Disable %1", name));
}

void AbstractEffectItem::setEnabled(bool enabled)
{
    m_enabled = enabled;
    updateEnable();
}

void AbstractEffectItem::setEffectStackEnabled(bool enabled)
{
    if (m_effectStackEnabled == enabled) {
        // nothing to do
        return;
    }
    m_effectStackEnabled = enabled;
    for (int i = 0; i < childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(child(i))->setEffectStackEnabled(enabled);
    }
    updateEnable(false);
}

bool AbstractEffectItem::isEnabled() const
{
    bool parentEnabled = true;
    if (auto ptr = std::static_pointer_cast<AbstractEffectItem>(m_parentItem.lock())) {
        parentEnabled = ptr->isEnabled();
    } else {
        // Root item, always return true
        return true;
    }
    return m_enabled && m_effectStackEnabled && parentEnabled;
}

EffectItemType AbstractEffectItem::effectItemType() const
{
    return m_effectItemType;
}
