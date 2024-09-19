/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

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

void AbstractEffectItem::markEnabled(bool enabled, Fun &undo, Fun &redo)
{
    Fun local_undo = [this, enabled]() {
        setAssetEnabled(!enabled);
        return true;
    };
    Fun local_redo = [this, enabled]() {
        setAssetEnabled(enabled);
        return true;
    };
    PUSH_LAMBDA(local_undo, undo);
    PUSH_LAMBDA(local_redo, redo);
}

void AbstractEffectItem::setAssetEnabled(bool enabled, bool update)
{
    m_enabled = enabled;
    if (update) {
        updateEnable();
    }
}

void AbstractEffectItem::setEffectStackEnabled(bool enabled)
{
    m_effectStackEnabled = enabled;
    for (int i = 0; i < childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(child(i))->setEffectStackEnabled(enabled);
    }
    updateEnable(false);
}

bool AbstractEffectItem::isAssetEnabled() const
{
    bool parentEnabled = true;
    if (auto ptr = std::static_pointer_cast<AbstractEffectItem>(m_parentItem.lock())) {
        parentEnabled = ptr->isAssetEnabled();
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
