/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
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
