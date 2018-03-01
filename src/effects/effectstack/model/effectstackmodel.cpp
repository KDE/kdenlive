
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
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "core.h"
#include "doc/docundostack.hpp"
#include "effectgroupmodel.hpp"
#include "effectitemmodel.hpp"
#include "effects/effectsrepository.hpp"
#include "macros.hpp"
#include <stack>
#include <utility>
#include <vector>

EffectStackModel::EffectStackModel(std::weak_ptr<Mlt::Service> service, ObjectId ownerId, std::weak_ptr<DocUndoStack> undo_stack)
    : AbstractTreeModel()
    , m_service(std::move(service))
    , m_effectStackEnabled(true)
    , m_ownerId(ownerId)
    , m_undoStack(undo_stack)
    , m_loadingExisting(false)
{
}

std::shared_ptr<EffectStackModel> EffectStackModel::construct(std::weak_ptr<Mlt::Service> service, ObjectId ownerId, std::weak_ptr<DocUndoStack> undo_stack)
{
    std::shared_ptr<EffectStackModel> self(new EffectStackModel(std::move(service), ownerId, undo_stack));
    self->rootItem = EffectGroupModel::construct(QStringLiteral("root"), self, true);
    return self;
}

void EffectStackModel::loadEffects()
{
    auto ptr = m_service.lock();
    m_loadingExisting = true;
    if (ptr) {
        for (int i = 0; i < ptr->filter_count(); i++) {
            if (ptr->filter(i)->get("kdenlive_id") == nullptr) {
                // don't consider internal MLT stuff
                continue;
            }
            auto effect = EffectItemModel::construct(ptr->filter(i), shared_from_this());
            // effect->setParameters
            Fun redo = addItem_lambda(effect, rootItem->getId());
            redo();
            connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
            connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
        }
    } else {
        qDebug() << "// CANNOT LOCK CLIP SEEVCE";
    }
    this->modelChanged();
    m_loadingExisting = false;
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
    Q_ASSERT(m_allItems.count(effect->getId()) > 0);
    int parentId = -1;
    if (auto ptr = effect->parentItem().lock()) parentId = ptr->getId();
    int current = 0;
    bool currentChanged = false;
    if (auto srv = m_service.lock()) {
        current = srv->get_int("kdenlive:activeeffect");
        if (current >= rootItem->childCount() - 1) {
            currentChanged = true;
            srv->set("kdenlive:activeeffect", --current);
        }
    }
    int currentRow = effect->row();
    Fun undo = addItem_lambda(effect, parentId);
    if (currentRow != rowCount() - 1) {
        Fun move = moveItem_lambda(effect->getId(), currentRow, true);
        PUSH_LAMBDA(move, undo);
    }
    Fun redo = removeItem_lambda(effect->getId());
    bool res = redo();
    if (res) {
        fadeIns.removeAll(effect->getId());
        fadeOuts.removeAll(effect->getId());
        QString effectName = EffectsRepository::get()->getName(effect->getAssetId());
        
        Fun update = [this, current, currentChanged]() {
            // Required to build the effect view
            if (currentChanged) {
                if (current < 0 || rowCount() == 0) {
                    // Stack is now empty
                    dataChanged(QModelIndex(), QModelIndex(), QVector<int>());
                } else {
                    std::shared_ptr<EffectItemModel> effectItem = std::static_pointer_cast<EffectItemModel>(rootItem->child(current));
                    QModelIndex ix = getIndexFromItem(effectItem);
                    dataChanged(ix, ix, QVector<int>());
                }
            }
            //TODO: only update if effect is fade or keyframe
            pCore->updateItemKeyframes(m_ownerId);
            return true;
        };
        update();
        PUSH_LAMBDA(update, redo);
        PUSH_LAMBDA(update, undo);
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
    connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
    connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
    bool res = redo();
    if (res) {
        QString effectName = EffectsRepository::get()->getName(effectId);
        PUSH_UNDO(undo, redo, i18n("copy effect %1", effectName));
    }
}

void EffectStackModel::appendEffect(const QString &effectId, bool makeCurrent)
{
    auto effect = EffectItemModel::construct(effectId, shared_from_this());
    Fun undo = removeItem_lambda(effect->getId());
    // TODO the parent should probably not always be the root
    Fun redo = addItem_lambda(effect, rootItem->getId());
    connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
    connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
    int currentActive = getActiveEffect();
    if (makeCurrent) {
        auto srvPtr = m_service.lock();
        if (srvPtr) {
            srvPtr->set("kdenlive:activeeffect", rowCount());
        }
    }
    bool res = redo();
    if (res) {
        if (effectId == QLatin1String("fadein") || effectId == QLatin1String("fade_from_black")) {
            fadeIns << effect->getId();
        } else if (effectId == QLatin1String("fadeout") || effectId == QLatin1String("fade_to_black")) {
            fadeOuts << effect->getId();
        }
        QString effectName = EffectsRepository::get()->getName(effectId);
        Fun update = [this]() {
            //TODO: only update if effect is fade or keyframe
            pCore->updateItemKeyframes(m_ownerId);
            return true;
        };
        PUSH_LAMBDA(update, redo);
        PUSH_LAMBDA(update, undo);
        PUSH_UNDO(undo, redo, i18n("Add effect %1", effectName));
    } else if (makeCurrent) {
        auto srvPtr = m_service.lock();
        if (srvPtr) {
            srvPtr->set("kdenlive:activeeffect", currentActive);
        }
    }
}

bool EffectStackModel::adjustStackLength(bool adjustFromEnd, int oldIn, int newIn, int duration, bool hasAudio, bool audioOnly, Fun &undo, Fun &redo, bool logUndo)
{
    Q_UNUSED(hasAudio)
    Q_UNUSED(audioOnly)
    const int fadeInDuration = getFadePosition(true);
    const int fadeOutDuration = getFadePosition(false);
    QList<QModelIndex> indexes;
    auto ptr = m_service.lock();
    int out = newIn + duration;
    for (int i = 0; i < rootItem->childCount(); ++i) {
        if (fadeInDuration > 0 && fadeIns.contains(std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId())) {
            std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
            int oldEffectIn = qMax(0, effect->filter().get_int("in"));
            int oldEffectOut = effect->filter().get_int("out");
            int effectDuration = qMin(oldEffectOut - oldEffectIn, duration);
            indexes << getIndexFromItem(effect);
            if (!adjustFromEnd && oldIn != newIn) {
                // Clip start was resized, adjust effect in / out
                Fun operation = [this, effect, newIn, effectDuration]() {
                    effect->setParameter(QStringLiteral("in"), newIn, false);
                    effect->setParameter(QStringLiteral("out"), effectDuration, false);
                    return true;
                };
                operation();
                Fun reverse = [this, effect, oldEffectIn, oldEffectOut]() {
                    effect->setParameter(QStringLiteral("in"), oldEffectIn, false);
                    effect->setParameter(QStringLiteral("out"), oldEffectOut, false);
                    return true;
                };
                PUSH_LAMBDA(operation, redo);
                PUSH_LAMBDA(reverse, undo);
            } else if (effectDuration < oldEffectOut - oldEffectIn || (logUndo && effect->filter().get_int("_refout") > 0)) {
                // Clip length changed, shorter than effect length so resize
                int referenceEffectOut = effect->filter().get_int("_refout");
                if (referenceEffectOut <= 0) {
                    referenceEffectOut = oldEffectOut;
                    effect->filter().set("_refout", referenceEffectOut);
                }
                Fun operation = [this, effect, oldEffectIn, effectDuration]() {
                    effect->setParameter(QStringLiteral("out"), oldEffectIn + effectDuration, false);
                    return true;
                };
                if (operation() && logUndo) {
                    Fun reverse = [this, effect, referenceEffectOut]() {
                        effect->setParameter(QStringLiteral("out"), referenceEffectOut, false);
                        effect->filter().set("_refout", (char*) nullptr);
                        return true;
                    };
                    PUSH_LAMBDA(operation, redo);
                    PUSH_LAMBDA(reverse, undo);
                }
            }
        } else if (fadeOutDuration > 0 && fadeOuts.contains(std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId())) {
            std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
            int effectDuration = qMin(fadeOutDuration, duration);
            int newFadeIn = out - effectDuration;
            int oldFadeIn = effect->filter().get_int("in");
            int oldOut = effect->filter().get_int("out");
            int referenceEffectIn = effect->filter().get_int("_refin");
            if (referenceEffectIn <= 0) {
                referenceEffectIn = oldFadeIn;
                effect->filter().set("_refin", referenceEffectIn);
            }
            Fun operation = [this, effect, newFadeIn, out]() {
                effect->setParameter(QStringLiteral("in"), newFadeIn, false);
                effect->setParameter(QStringLiteral("out"), out, false);
                return true;
            };
            if (operation() && logUndo) {
                Fun reverse = [this, effect, referenceEffectIn, oldOut]() {
                    effect->setParameter(QStringLiteral("in"), referenceEffectIn, false);
                    effect->setParameter(QStringLiteral("out"), oldOut, false);
                    effect->filter().set("_refin", (char*) nullptr);
                    return true;
                };
                PUSH_LAMBDA(operation, redo);
                PUSH_LAMBDA(reverse, undo);
            }
            indexes << getIndexFromItem(effect);
        }
    }
    if (!indexes.isEmpty()) {
        emit dataChanged(indexes.first(), indexes.last(), QVector<int>());
    }
    return true;
}

bool EffectStackModel::adjustFadeLength(int duration, bool fromStart, bool audioFade, bool videoFade)
{
    if (fromStart) {
        // Fade in
        if (fadeIns.isEmpty()) {
            if (audioFade) {
                appendEffect(QStringLiteral("fadein"));
            }
            if (videoFade) {
                appendEffect(QStringLiteral("fade_from_black"));
            }
        }
        QList<QModelIndex> indexes;
        auto ptr = m_service.lock();
        int in = 0;
        if (ptr) {
            in = ptr->get_int("in");
        }
        for (int i = 0; i < rootItem->childCount(); ++i) {
            if (fadeIns.contains(std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId())) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                effect->filter().set("in", in);
                duration = qMin(pCore->getItemDuration(m_ownerId), duration);
                effect->filter().set("out", in + duration);
                indexes << getIndexFromItem(effect);
            }
        }
        if (!indexes.isEmpty()) {
            emit dataChanged(indexes.first(), indexes.last(), QVector<int>());
            pCore->updateItemKeyframes(m_ownerId);
        }
    } else {
        // Fade out
        if (fadeOuts.isEmpty()) {
            if (audioFade) {
                appendEffect(QStringLiteral("fadeout"));
            }
            if (videoFade) {
                appendEffect(QStringLiteral("fade_to_black"));
            }
        }
        int in = 0;
        auto ptr = m_service.lock();
        if (ptr) {
            in = ptr->get_int("in");
        }
        int out = in + pCore->getItemDuration(m_ownerId);
        QList<QModelIndex> indexes;
        for (int i = 0; i < rootItem->childCount(); ++i) {
            if (fadeOuts.contains(std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId())) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                effect->filter().set("out", out);
                duration = qMin(pCore->getItemDuration(m_ownerId), duration);
                effect->filter().set("in", out - duration);
                indexes << getIndexFromItem(effect);
            }
        }
        if (!indexes.isEmpty()) {
            emit dataChanged(indexes.first(), indexes.last(), QVector<int>());
            pCore->updateItemKeyframes(m_ownerId);
        }
    }
    return true;
}

int EffectStackModel::getFadePosition(bool fromStart)
{
    if (fromStart) {
        if (fadeIns.isEmpty()) {
            return 0;
        }
        for (int i = 0; i < rootItem->childCount(); ++i) {
            if (fadeIns.first() == std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                return effect->filter().get_int("out");
            }
        }
    } else {
        if (fadeOuts.isEmpty()) {
            return 0;
        }
        for (int i = 0; i < rootItem->childCount(); ++i) {
            if (fadeOuts.first() == std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                return effect->filter().get_int("out") - effect->filter().get_int("in");
            }
        }
    }
    return 0;
}

bool EffectStackModel::removeFade(bool fromStart)
{
    QList<int> toRemove;
    for (int i = 0; i < rootItem->childCount(); ++i) {
        if ((fromStart && fadeIns.contains(std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId())) ||
            (!fromStart && fadeOuts.contains(std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()))) {
            toRemove << i;
        }
    }
    qSort(toRemove.begin(), toRemove.end(), qGreater<int>());
    foreach (int i, toRemove) {
        std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        if (fromStart) {
            fadeIns.removeAll(rootItem->child(i)->getId());
        } else {
            fadeOuts.removeAll(rootItem->child(i)->getId());
        }
        removeEffect(effect);
    }
    return true;
}

void EffectStackModel::moveEffect(int destRow, std::shared_ptr<AbstractEffectItem> item)
{
    Q_ASSERT(m_allItems.count(item->getId()) > 0);
    int oldRow = item->row();
    Fun undo = moveItem_lambda(item->getId(), oldRow);
    Fun redo = moveItem_lambda(item->getId(), destRow);
    bool res = redo();
    if (res) {
        auto effectId = std::static_pointer_cast<EffectItemModel>(item)->getAssetId();
        QString effectName = EffectsRepository::get()->getName(effectId);
        PUSH_UNDO(undo, redo, i18n("Move effect %1", effectName));
    }
}

void EffectStackModel::registerItem(const std::shared_ptr<TreeItem> &item)
{
    QModelIndex ix;
    if (!item->isRoot()) {
        auto effectItem = std::static_pointer_cast<AbstractEffectItem>(item);
        if (!m_loadingExisting) {
            effectItem->plant(m_service);
        }
        effectItem->setEffectStackEnabled(m_effectStackEnabled);
        ix = getIndexFromItem(effectItem);
        if (!effectItem->isAudio() && !m_loadingExisting) {
            pCore->refreshProjectItem(m_ownerId);
            pCore->invalidateItem(m_ownerId);
        }
    }
    AbstractTreeModel::registerItem(item);
    if (ix.isValid()) {
        // Required to build the effect view
        dataChanged(ix, ix, QVector<int>());
    }
}
void EffectStackModel::deregisterItem(int id, TreeItem *item)
{
    if (!item->isRoot()) {
        auto effectItem = static_cast<AbstractEffectItem *>(item);
        effectItem->unplant(this->m_service);
        if (!effectItem->isAudio()) {
            pCore->refreshProjectItem(m_ownerId);
            pCore->invalidateItem(m_ownerId);
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
        clone->filter().set("in", effect->filter().get_int("in"));
        clone->filter().set("out", effect->filter().get_int("out"));
        const QString effectId = effect->getAssetId();
        if (effectId == QLatin1String("fadein") || effectId == QLatin1String("fade_from_black")) {
            fadeIns << clone->getId();
        } else if (effectId == QLatin1String("fadeout") || effectId == QLatin1String("fade_to_black")) {
            fadeOuts << clone->getId();
        }
        // TODO parent should not always be root
        Fun redo = addItem_lambda(clone, rootItem->getId());
        connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
        connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
        redo();
    }
}

void EffectStackModel::setActiveEffect(int ix)
{
    auto ptr = m_service.lock();
    if (ptr) {
        ptr->set("kdenlive:activeeffect", ix);
    }
    pCore->updateItemKeyframes(m_ownerId);
}

int EffectStackModel::getActiveEffect() const
{
    auto ptr = m_service.lock();
    if (ptr) {
        return ptr->get_int("kdenlive:activeeffect");
    }
    return 0;
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

bool EffectStackModel::checkConsistency()
{
    if (!AbstractTreeModel::checkConsistency()) {
        return false;
    }

    std::vector<std::shared_ptr<EffectItemModel>> allFilters;
    // We do a DFS on the tree to retrieve all the filters
    std::stack<std::shared_ptr<AbstractEffectItem>> stck;
    stck.push(std::static_pointer_cast<AbstractEffectItem>(rootItem));

    while (!stck.empty()) {
        auto current = stck.top();
        stck.pop();

        if (current->effectItemType() == EffectItemType::Effect) {
            if (current->childCount() > 0) {
                qDebug() << "ERROR: Found an effect with children";
                return false;
            }
            allFilters.push_back(std::static_pointer_cast<EffectItemModel>(current));
            continue;
        }
        for (int i = current->childCount() - 1; i >= 0; --i) {
            stck.push(std::static_pointer_cast<AbstractEffectItem>(current->child(i)));
        }
    }

    auto ptr = m_service.lock();
    if (!ptr) {
        qDebug() << "ERROR: unavailable service";
        return false;
    }
    if (ptr->filter_count() != (int)allFilters.size()) {
        qDebug() << "ERROR: Wrong filter count";
        return false;
    }

    for (uint i = 0; i < allFilters.size(); ++i) {
        auto mltFilter = ptr->filter((int)i)->get_filter();
        auto currentFilter = allFilters[i]->filter().get_filter();
        if (mltFilter != currentFilter) {
            qDebug() << "ERROR: filter " << i << "differ";
            return false;
        }
    }

    return true;
}

void EffectStackModel::adjust(const QString &effectId, const QString &effectName, double value)
{
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        if (effectId == sourceEffect->getAssetId()) {
            sourceEffect->setParameter(effectName, QString::number(value));
            return;
        }
    }
}

bool EffectStackModel::hasFilter(const QString &effectId)
{
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        if (effectId == sourceEffect->getAssetId()) {
            return true;
        }
    }
    return false;
}

double EffectStackModel::getFilter(const QString &effectId, const QString &paramName)
{
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        if (effectId == sourceEffect->getAssetId()) {
            return sourceEffect->filter().get_double(paramName.toUtf8().constData());
        }
    }
    return 0.0;
}

KeyframeModel *EffectStackModel::getEffectKeyframeModel()
{
    if (rootItem->childCount() == 0) return nullptr;
    auto ptr = m_service.lock();
    int ix = 0;
    if (ptr) {
        ix = ptr->get_int("kdenlive:activeeffect");
    }
    if (ix < 0) {
        return nullptr;
    }
    std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
    std::shared_ptr<KeyframeModelList> listModel = sourceEffect->getKeyframeModel();
    if (listModel) {
        return listModel->getKeyModel();
    }
    return nullptr;
}

void EffectStackModel::replugEffect(std::shared_ptr<AssetParameterModel> asset)
{
    auto effectItem = std::static_pointer_cast<EffectItemModel>(asset);
    int oldRow = effectItem->row();
    int count = rowCount();
    for (int ix = oldRow; ix < count; ix++) {
        auto item = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
        item->unplant(this->m_service);
    }
    Mlt::Properties *effect = EffectsRepository::get()->getEffect(effectItem->getAssetId());
    effect->inherit(effectItem->filter());
    effectItem->resetAsset(effect);
    for (int ix = oldRow; ix < count; ix++) {
        auto item = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
        item->plant(this->m_service);
    }
}

void EffectStackModel::cleanFadeEffects(bool outEffects, Fun &undo, Fun &redo)
{
    QList<int> toDelete = outEffects ? fadeOuts : fadeIns;
    for (int id : toDelete) {
        auto effect = std::static_pointer_cast<EffectItemModel>(getItemById(id));
        Fun operation = removeItem_lambda(id);
        if (operation()) {
            Fun reverse = addItem_lambda(effect, rootItem->getId());
            PUSH_LAMBDA(operation, redo);
            PUSH_LAMBDA(reverse, undo);
        }
    }
    if (!toDelete.isEmpty()) {
        Fun update = [this]() {
            //TODO: only update if effect is fade or keyframe
            pCore->updateItemKeyframes(m_ownerId);
            return true;
        };
        update();
        PUSH_LAMBDA(update, redo);
    }
}
