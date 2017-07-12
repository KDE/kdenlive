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

#include "core.h"
#include "doc/docundostack.hpp"
#include "effectgroupmodel.hpp"
#include "effectitemmodel.hpp"
#include "macros.hpp"
#include <utility>

EffectStackModel::EffectStackModel(std::weak_ptr<Mlt::Service> service, ObjectId ownerId, std::weak_ptr<DocUndoStack> undo_stack)
    : AbstractTreeModel()
    , m_service(std::move(service))
    , m_effectStackEnabled(true)
    , m_ownerId(ownerId)
    , m_undoStack(undo_stack)
{
}

std::shared_ptr<EffectStackModel> EffectStackModel::construct(std::weak_ptr<Mlt::Service> service, ObjectId ownerId, std::weak_ptr<DocUndoStack> undo_stack)
{
    std::shared_ptr<EffectStackModel> self(new EffectStackModel(std::move(service), ownerId, undo_stack));
    self->rootItem = EffectGroupModel::construct(QStringLiteral("root"), self, true);
    return self;
}

void EffectStackModel::resetService(std::weak_ptr<Mlt::Service> service)
{
    m_service = std::move(service);
    // replant all effects in new service
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::static_pointer_cast<EffectItemModel>(rootItem->child(i))->plant(m_service);
    }
}

void EffectStackModel::removeEffect(std::shared_ptr<EffectItemModel> effect)
{
    int parentId = -1;
    if (auto ptr = effect->parentItem().lock()) parentId = ptr->getId();
    Fun undo = addItem_lambda(effect, parentId);
    Fun redo = removeItem_lambda(effect->getId());
    bool res = redo();
    if (res) {
        QString effectName = EffectsRepository::get()->getName(effect->getAssetId());
        PUSH_UNDO(undo, redo, i18n("Delete effect %1", effectName));
    }
}

void EffectStackModel::copyEffect(std::shared_ptr<AbstractEffectItem> sourceItem)
{
    if (sourceItem->childCount() > 0) {
        // TODO: group
        return;
    }
    std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(sourceItem);
    QString effectId = sourceEffect->getAssetId();
    auto effect = EffectItemModel::construct(effectId, shared_from_this());
    effect->setParameters(sourceEffect->getAllParameters());
    Fun undo = removeItem_lambda(effect->getId());
    // TODO the parent should probably not always be the root
    Fun redo = addItem_lambda(effect, rootItem->getId());
    bool res = redo();
    if (res) {
        QString effectName = EffectsRepository::get()->getName(effectId);
        PUSH_UNDO(undo, redo, i18n("copy effect %1", effectName));
    }
}

void EffectStackModel::appendEffect(const QString &effectId)
{
    auto effect = EffectItemModel::construct(effectId, shared_from_this());
    Fun undo = removeItem_lambda(effect->getId());
    // TODO the parent should probably not always be the root
    Fun redo = addItem_lambda(effect, rootItem->getId());
    bool res = redo();
    if (res) {
        QString effectName = EffectsRepository::get()->getName(effectId);
        PUSH_UNDO(undo, redo, i18n("Add effect %1", effectName));
    }
}

bool EffectStackModel::adjustEffectLength(const QString &effectName, int duration)
{
    int row = -1;
    for (int i = 0; i < rootItem->childCount(); ++i) {
        if (std::static_pointer_cast<AbstractEffectItem>(rootItem->child(i))->dataColumn(1) == effectName) {
            row = i;
            break;
        }
    }
    if (row == -1) {
        appendEffect(effectName);
        row = rootItem->childCount() - 1;
    }
    std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(getEffectStackRow(row));
    if (effectName == QLatin1String("fadein")) {
        effect->filter().set("out", duration);
        QModelIndex ix = getIndexFromItem(effect);
        emit dataChanged(ix, ix, QVector<int>());
    }
    else if (effectName == QLatin1String("fadeout")) {
        int out = pCore->getItemDuration(m_ownerId);
        effect->filter().set("out", out);
        effect->filter().set("in", out - duration);
        QModelIndex ix = getIndexFromItem(effect);
        emit dataChanged(ix, ix, QVector<int>());
    }
    return true;
}

int EffectStackModel::getFadeIn()
{
    for (int i = 0; i < rootItem->childCount(); ++i) {
        if (std::static_pointer_cast<AbstractEffectItem>(rootItem->child(i))->dataColumn(1) == QLatin1String("fadein")) {
            std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(getEffectStackRow(i));
            return effect->filter().get_int("out");
        }
    }
    return 0;
}

int EffectStackModel::getFadeOut()
{
    for (int i = 0; i < rootItem->childCount(); ++i) {
        if (std::static_pointer_cast<AbstractEffectItem>(rootItem->child(i))->dataColumn(1) == QLatin1String("fadeout")) {
            std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(getEffectStackRow(i));
            return effect->filter().get_int("out") - effect->filter().get_int("in");
        }
    }
    return 0;
}

bool EffectStackModel::removeEffectById(const QString &effectName)
{
    int row = -1;
    for (int i = 0; i < rootItem->childCount(); ++i) {
        if (std::static_pointer_cast<AbstractEffectItem>(rootItem->child(i))->dataColumn(1) == effectName) {
            row = i;
            break;
        }
    }
    if (row == -1) {
        return false;
    }
    std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(getEffectStackRow(row));
    removeEffect(effect);
    return true;
}

void EffectStackModel::moveEffect(int destRow, std::shared_ptr<AbstractEffectItem> item)
{
    if (item->childCount() > 0) {
        // TODO: group
        return;
    }
    std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(item);
    QModelIndex ix = getIndexFromItem(effect);
    QModelIndex ix2 = ix;
    rootItem->moveChild(destRow, effect);
    QList<std::shared_ptr<EffectItemModel>> effects;
    // TODO: define parent group target
    for (int i = destRow; i < rootItem->childCount(); i++) {
        auto item = getEffectStackRow(i);
        if (item->childCount() > 0) {
            // TODO: group
            continue;
        }
        std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
        eff->unplant(m_service);
        effects << eff;
    }
    for (int i = 0; i < effects.count(); i++) {
        auto eff = effects.at(i);
        eff->plant(m_service);
        if (i == effects.count() - 1) {
            ix2 = getIndexFromItem(eff);
        }
    }
    pCore->refreshProjectItem(m_ownerId);
    emit dataChanged(ix, ix2, QVector<int>());
}

void EffectStackModel::registerItem(const std::shared_ptr<TreeItem> &item)
{
    if (!item->isRoot()) {
        auto effectItem = std::static_pointer_cast<AbstractEffectItem>(item);
        QModelIndex ix;
        effectItem->plant(m_service);
        effectItem->setEffectStackEnabled(m_effectStackEnabled);
        ix = getIndexFromItem(effectItem);
        effectItem->connectDataChanged();
        if (!effectItem->isAudio()) {
            pCore->refreshProjectItem(m_ownerId);
        }
        if (ix.isValid()) {
            // Required to build the effect view
            dataChanged(ix, ix, QVector<int>());
        }
    }
    AbstractTreeModel::registerItem(item);
}
void EffectStackModel::deregisterItem(int id, TreeItem *item)
{
    if (!item->isRoot()) {
        auto effectItem = static_cast<AbstractEffectItem*>(item);
        effectItem->unplant(this->m_service);
        if (!effectItem->isAudio()) {
            pCore->refreshProjectItem(m_ownerId);
        }
    }
    AbstractTreeModel::deregisterItem(id, item);
}

void EffectStackModel::setEffectStackEnabled(bool enabled)
{
    m_effectStackEnabled = enabled;

    // Recursively updates children states
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(rootItem->child(i))->setEffectStackEnabled(enabled);
    }
}

std::shared_ptr<AbstractEffectItem> EffectStackModel::getEffectStackRow(int row, std::shared_ptr<TreeItem> parentItem)
{
    return std::static_pointer_cast<AbstractEffectItem>(parentItem ? rootItem->child(row) : rootItem->child(row));
}

void EffectStackModel::importEffects(std::shared_ptr<EffectStackModel> sourceStack)
{
    // TODO: manage fades, keyframes if clips don't have same size / in point
    for (int i = 0; i < sourceStack->rowCount(); i++) {
        auto item = sourceStack->getEffectStackRow(i);
        if (item->childCount() > 0) {
            // TODO: group
            continue;
        }
        std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(item);
        auto clone = EffectItemModel::construct(effect->getAssetId(), shared_from_this());
        rootItem->appendChild(clone);
        clone->setParameters(effect->getAllParameters());
        // TODO parent should not always be root
        Fun redo = addItem_lambda(clone, rootItem->getId());
        redo();
    }
}

void EffectStackModel::setActiveEffect(int ix)
{
    auto ptr = m_service.lock();
    if (ptr) {
        ptr->set("kdenlive:activeeffect", ix);
    }
}

int EffectStackModel::getActiveEffect() const
{
    auto ptr = m_service.lock();
    if (ptr) {
        return ptr->get_int("kdenlive:activeeffect");
    }
    return -1;
}

void EffectStackModel::slotCreateGroup(std::shared_ptr<EffectItemModel> childEffect)
{
    auto groupItem = EffectGroupModel::construct(QStringLiteral("group"), shared_from_this());
    rootItem->appendChild(groupItem);
    groupItem->appendChild(childEffect);
}

ObjectId EffectStackModel::getOwnerId() const
{
    return m_ownerId;
}
