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
#include "timeline2/model/timelinemodel.hpp"
#include <profiles/profilemodel.hpp>
#include <stack>
#include <utility>
#include <vector>

EffectStackModel::EffectStackModel(std::weak_ptr<Mlt::Service> service, ObjectId ownerId, std::weak_ptr<DocUndoStack> undo_stack)
    : AbstractTreeModel()
    , m_effectStackEnabled(true)
    , m_ownerId(ownerId)
    , m_undoStack(undo_stack)
    , m_lock(QReadWriteLock::Recursive)
    , m_loadingExisting(false)
{
    m_masterService = std::move(service);
}

std::shared_ptr<EffectStackModel> EffectStackModel::construct(std::weak_ptr<Mlt::Service> service, ObjectId ownerId, std::weak_ptr<DocUndoStack> undo_stack)
{
    std::shared_ptr<EffectStackModel> self(new EffectStackModel(std::move(service), ownerId, undo_stack));
    self->rootItem = EffectGroupModel::construct(QStringLiteral("root"), self, true);
    return self;
}

void EffectStackModel::resetService(std::weak_ptr<Mlt::Service> service)
{
    QWriteLocker locker(&m_lock);
    m_masterService = std::move(service);
    m_childServices.clear();
    // replant all effects in new service
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::static_pointer_cast<EffectItemModel>(rootItem->child(i))->plant(m_masterService);
    }
}

void EffectStackModel::addService(std::weak_ptr<Mlt::Service> service)
{
    QWriteLocker locker(&m_lock);
    m_childServices.emplace_back(std::move(service));
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::static_pointer_cast<EffectItemModel>(rootItem->child(i))->plantClone(m_childServices.back());
    }
}

void EffectStackModel::loadService(std::weak_ptr<Mlt::Service> service)
{
    QWriteLocker locker(&m_lock);
    m_childServices.emplace_back(std::move(service));
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::static_pointer_cast<EffectItemModel>(rootItem->child(i))->loadClone(m_childServices.back());
    }
}

void EffectStackModel::removeService(std::shared_ptr<Mlt::Service> service)
{
    QWriteLocker locker(&m_lock);
    std::vector<int> to_delete;
    for (int i = int(m_childServices.size()) - 1; i >= 0; --i) {
        auto ptr = m_childServices[uint(i)].lock();
        if (service->get_int("_childid") == ptr->get_int("_childid")) {
            for (int j = 0; j < rootItem->childCount(); ++j) {
                std::static_pointer_cast<EffectItemModel>(rootItem->child(j))->unplantClone(ptr);
            }
            to_delete.push_back(i);
        }
    }
    for (int i : to_delete) {
        m_childServices.erase(m_childServices.begin() + i);
    }
}

void EffectStackModel::removeCurrentEffect()
{
    int ix = 0;
    if (auto ptr = m_masterService.lock()) {
        ix = ptr->get_int("kdenlive:activeeffect");
    }
    if (ix < 0) {
        return;
    }
    std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
    if (effect) {
        removeEffect(effect);
    }
}

void EffectStackModel::removeEffect(std::shared_ptr<EffectItemModel> effect)
{
    qDebug() << "* * ** REMOVING EFFECT FROM STACK!!!\n!!!!!!!!!";
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allItems.count(effect->getId()) > 0);
    int parentId = -1;
    if (auto ptr = effect->parentItem().lock()) parentId = ptr->getId();
    int current = 0;
    if (auto srv = m_masterService.lock()) {
        current = srv->get_int("kdenlive:activeeffect");
        if (current >= rootItem->childCount() - 1) {
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
        int inFades = int(fadeIns.size());
        int outFades = int(fadeOuts.size());
        fadeIns.erase(effect->getId());
        fadeOuts.erase(effect->getId());
        inFades = int(fadeIns.size()) - inFades;
        outFades = int(fadeOuts.size()) - outFades;
        QString effectName = EffectsRepository::get()->getName(effect->getAssetId());
        Fun update = [this, current, inFades, outFades]() {
            // Required to build the effect view
            if (current < 0 || rowCount() == 0) {
                // Stack is now empty
                emit dataChanged(QModelIndex(), QModelIndex(), {});
            } else {
                QVector<int> roles = {TimelineModel::EffectNamesRole};
                if (inFades < 0) {
                    roles << TimelineModel::FadeInRole;
                }
                if (outFades < 0) {
                    roles << TimelineModel::FadeOutRole;
                }
                qDebug() << "// EMITTING UNDO DATA CHANGE: " << roles;
                emit dataChanged(QModelIndex(), QModelIndex(), roles);
            }
            // TODO: only update if effect is fade or keyframe
            /*if (inFades < 0) {
                pCore->updateItemModel(m_ownerId, QStringLiteral("fadein"));
            } else if (outFades < 0) {
                pCore->updateItemModel(m_ownerId, QStringLiteral("fadeout"));
            }*/
            pCore->updateItemKeyframes(m_ownerId);
            return true;
        };
        Fun update2 = [this, inFades, outFades]() {
            // Required to build the effect view
            QVector<int> roles = {TimelineModel::EffectNamesRole};
            // TODO: only update if effect is fade or keyframe
            if (inFades < 0) {
                roles << TimelineModel::FadeInRole;
            } else if (outFades < 0) {
                roles << TimelineModel::FadeOutRole;
            }
            qDebug() << "// EMITTING REDO DATA CHANGE: " << roles;
            emit dataChanged(QModelIndex(), QModelIndex(), roles);
            pCore->updateItemKeyframes(m_ownerId);
            return true;
        };
        update();
        PUSH_LAMBDA(update, redo);
        PUSH_LAMBDA(update2, undo);
        PUSH_UNDO(undo, redo, i18n("Delete effect %1", effectName));
    } else {
        qDebug() << "..........FAILED EFFECT DELETION";
    }
}

bool EffectStackModel::copyEffect(std::shared_ptr<AbstractEffectItem> sourceItem, PlaylistState::ClipState state, bool logUndo)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool result = copyEffect(sourceItem, state, undo, redo);
    if (result && logUndo) {
        std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(sourceItem);
        QString effectName = EffectsRepository::get()->getName(sourceEffect->getAssetId());
        PUSH_UNDO(undo, redo, i18n("copy effect %1", effectName));
    }
    return result;
}

QDomElement EffectStackModel::toXml(QDomDocument &document)
{
    QDomElement container = document.createElement(QStringLiteral("effects"));
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        QDomElement sub = document.createElement(QStringLiteral("effect"));
        sub.setAttribute(QStringLiteral("id"), sourceEffect->getAssetId());
        sub.setAttribute(QStringLiteral("in"), sourceEffect->filter().get_int("in"));
        sub.setAttribute(QStringLiteral("out"), sourceEffect->filter().get_int("out"));
        QVector<QPair<QString, QVariant>> params = sourceEffect->getAllParameters();
        QLocale locale;
        for (auto param : params) {
            if (param.second.type() == QVariant::Double) {
                Xml::setXmlProperty(sub, param.first, locale.toString(param.second.toDouble()));
            } else {
                Xml::setXmlProperty(sub, param.first, param.second.toString());
            }
        }
        container.appendChild(sub);
    }
    return container;
}

void EffectStackModel::fromXml(const QDomElement &effectsXml, Fun &undo, Fun &redo)
{
    QDomNodeList nodeList = effectsXml.elementsByTagName(QStringLiteral("effect"));
    for (int i = 0; i < nodeList.count(); ++i) {
        QDomElement node = nodeList.item(i).toElement();
        const QString effectId = node.attribute(QStringLiteral("id"));
        auto effect = EffectItemModel::construct(effectId, shared_from_this());
        int in = node.attribute(QStringLiteral("in")).toInt();
        int out = node.attribute(QStringLiteral("out")).toInt();
        if (out > 0) {
            effect->filter().set("in", in);
            effect->filter().set("out", out);
        }
        QVector<QPair<QString, QVariant>> parameters;
        QDomNodeList params = node.elementsByTagName(QStringLiteral("property"));
        for (int j = 0; j < params.count(); j++) {
            QDomElement pnode = params.item(j).toElement();
            parameters.append(QPair<QString, QVariant>(pnode.attribute(QStringLiteral("name")), QVariant(pnode.text())));
        }
        effect->setParameters(parameters);
        Fun local_undo = removeItem_lambda(effect->getId());
        // TODO the parent should probably not always be the root
        Fun local_redo = addItem_lambda(effect, rootItem->getId());
        effect->prepareKeyframes();
        connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
        connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
        if (effectId == QLatin1String("fadein") || effectId == QLatin1String("fade_from_black")) {
            fadeIns.insert(effect->getId());
        } else if (effectId == QLatin1String("fadeout") || effectId == QLatin1String("fade_to_black")) {
            fadeOuts.insert(effect->getId());
        }
        local_redo();
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    }
    if (true) {
        Fun update = [this]() {
            emit dataChanged(QModelIndex(), QModelIndex(), {});
            return true;
        };
        update();
        PUSH_LAMBDA(update, redo);
        PUSH_LAMBDA(update, undo);
    }
}

bool EffectStackModel::copyEffect(std::shared_ptr<AbstractEffectItem> sourceItem, PlaylistState::ClipState state, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (sourceItem->childCount() > 0) {
        // TODO: group
        return false;
    }
    bool audioEffect = sourceItem->isAudio();
    if (audioEffect) {
        if (state == PlaylistState::VideoOnly) {
            // This effect cannot be used
            return false;
        }
    } else if (state == PlaylistState::AudioOnly) {
        return false;
    }
    std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(sourceItem);
    const QString effectId = sourceEffect->getAssetId();
    auto effect = EffectItemModel::construct(effectId, shared_from_this());
    effect->setParameters(sourceEffect->getAllParameters());
    effect->filter().set("in", sourceEffect->filter().get_int("in"));
    effect->filter().set("out", sourceEffect->filter().get_int("out"));
    Fun local_undo = removeItem_lambda(effect->getId());
    // TODO the parent should probably not always be the root
    Fun local_redo = addItem_lambda(effect, rootItem->getId());
    effect->prepareKeyframes();
    connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
    connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
    QVector<int> roles = {TimelineModel::EffectNamesRole};
    if (effectId == QLatin1String("fadein") || effectId == QLatin1String("fade_from_black")) {
        fadeIns.insert(effect->getId());
        roles << TimelineModel::FadeInRole;
    } else if (effectId == QLatin1String("fadeout") || effectId == QLatin1String("fade_to_black")) {
        fadeOuts.insert(effect->getId());
        roles << TimelineModel::FadeOutRole;
    }
    bool res = local_redo();
    if (res) {
        Fun update = [this, roles]() {
            emit dataChanged(QModelIndex(), QModelIndex(), roles);
            return true;
        };
        update();
        PUSH_LAMBDA(update, local_redo);
        PUSH_LAMBDA(update, local_undo);
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    }
    return res;
}

bool EffectStackModel::appendEffect(const QString &effectId, bool makeCurrent)
{
    QWriteLocker locker(&m_lock);
    auto effect = EffectItemModel::construct(effectId, shared_from_this());
    PlaylistState::ClipState state = pCore->getItemState(m_ownerId);
    if (effect->isAudio()) {
        if (state == PlaylistState::VideoOnly) {
            // Cannot add effect to this clip
            return false;
        }
    } else if (state == PlaylistState::AudioOnly) {
        // Cannot add effect to this clip
        return false;
    }
    Fun undo = removeItem_lambda(effect->getId());
    // TODO the parent should probably not always be the root
    Fun redo = addItem_lambda(effect, rootItem->getId());
    effect->prepareKeyframes();
    connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
    connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
    int currentActive = getActiveEffect();
    if (makeCurrent) {
        if (auto srvPtr = m_masterService.lock()) {
            srvPtr->set("kdenlive:activeeffect", rowCount());
        }
    }
    bool res = redo();
    if (res) {
        int inFades = 0;
        int outFades = 0;
        if (effectId == QLatin1String("fadein") || effectId == QLatin1String("fade_from_black")) {
            inFades++;
        } else if (effectId == QLatin1String("fadeout") || effectId == QLatin1String("fade_to_black")) {
            outFades++;
        }
        QString effectName = EffectsRepository::get()->getName(effectId);
        Fun update = [this, inFades, outFades]() {
            // TODO: only update if effect is fade or keyframe
            QVector<int> roles = {TimelineModel::EffectNamesRole};
            if (inFades > 0) {
                roles << TimelineModel::FadeInRole;
            } else if (outFades > 0) {
                roles << TimelineModel::FadeOutRole;
            }
            pCore->updateItemKeyframes(m_ownerId);
            emit dataChanged(QModelIndex(), QModelIndex(), roles);
            return true;
        };
        update();
        PUSH_LAMBDA(update, redo);
        PUSH_LAMBDA(update, undo);
        PUSH_UNDO(undo, redo, i18n("Add effect %1", effectName));
    } else if (makeCurrent) {
        if (auto srvPtr = m_masterService.lock()) {
            srvPtr->set("kdenlive:activeeffect", currentActive);
        }
    }
    return res;
}

bool EffectStackModel::adjustStackLength(bool adjustFromEnd, int oldIn, int oldDuration, int newIn, int duration, int offset, Fun &undo, Fun &redo, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    const int fadeInDuration = getFadePosition(true);
    const int fadeOutDuration = getFadePosition(false);
    int out = newIn + duration;
    for (const auto &leaf : rootItem->getLeaves()) {
        std::shared_ptr<AbstractEffectItem> item = std::static_pointer_cast<AbstractEffectItem>(leaf);
        if (item->effectItemType() == EffectItemType::Group) {
            // probably an empty group, ignore
            continue;
        }
        std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(leaf);
        if (fadeInDuration > 0 && fadeIns.count(leaf->getId()) > 0) {
            int oldEffectIn = qMax(0, effect->filter().get_in());
            int oldEffectOut = effect->filter().get_out();
            qDebug() << "--previous effect: " << oldEffectIn << "-" << oldEffectOut;
            int effectDuration = qMin(effect->filter().get_length() - 1, duration);
            if (!adjustFromEnd && (oldIn != newIn || duration != oldDuration)) {
                // Clip start was resized, adjust effect in / out
                Fun operation = [this, effect, newIn, effectDuration, logUndo]() {
                    effect->setParameter(QStringLiteral("in"), newIn, false);
                    effect->setParameter(QStringLiteral("out"), newIn + effectDuration, logUndo);
                    qDebug() << "--new effect: " << newIn << "-" << newIn + effectDuration;
                    return true;
                };
                bool res = operation();
                if (!res) {
                    return false;
                }
                Fun reverse = [this, effect, oldEffectIn, oldEffectOut, logUndo]() {
                    effect->setParameter(QStringLiteral("in"), oldEffectIn, false);
                    effect->setParameter(QStringLiteral("out"), oldEffectOut, logUndo);
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
                Fun operation = [this, effect, oldEffectIn, effectDuration, logUndo]() {
                    effect->setParameter(QStringLiteral("out"), oldEffectIn + effectDuration, logUndo);
                    return true;
                };
                bool res = operation();
                if (!res) {
                    return false;
                }
                if (logUndo) {
                    Fun reverse = [this, effect, referenceEffectOut]() {
                        effect->setParameter(QStringLiteral("out"), referenceEffectOut, true);
                        effect->filter().set("_refout", (char *)nullptr);
                        return true;
                    };
                    PUSH_LAMBDA(operation, redo);
                    PUSH_LAMBDA(reverse, undo);
                }
            }
        } else if (fadeOutDuration > 0 && fadeOuts.count(leaf->getId()) > 0) {
            int effectDuration = qMin(fadeOutDuration, duration);
            int newFadeIn = out - effectDuration;
            int oldFadeIn = effect->filter().get_int("in");
            int oldOut = effect->filter().get_int("out");
            int referenceEffectIn = effect->filter().get_int("_refin");
            if (referenceEffectIn <= 0) {
                referenceEffectIn = oldFadeIn;
                effect->filter().set("_refin", referenceEffectIn);
            }
            Fun operation = [this, effect, newFadeIn, out, logUndo]() {
                effect->setParameter(QStringLiteral("in"), newFadeIn, false);
                effect->setParameter(QStringLiteral("out"), out, logUndo);
                return true;
            };
            bool res = operation();
            if (!res) {
                return false;
            }
            if (logUndo) {
                Fun reverse = [this, effect, referenceEffectIn, oldOut]() {
                    effect->setParameter(QStringLiteral("in"), referenceEffectIn, false);
                    effect->setParameter(QStringLiteral("out"), oldOut, true);
                    effect->filter().set("_refin", (char *)nullptr);
                    return true;
                };
                PUSH_LAMBDA(operation, redo);
                PUSH_LAMBDA(reverse, undo);
            }
        } else {
            // Not a fade effect, check for keyframes
            std::shared_ptr<KeyframeModelList> keyframes = effect->getKeyframeModel();
            if (keyframes != nullptr) {
                // Effect has keyframes, update these
                keyframes->resizeKeyframes(oldIn, oldIn + oldDuration - 1, newIn, out - 1, offset, adjustFromEnd, undo, redo);
                QModelIndex index = getIndexFromItem(effect);
                Fun refresh = [this, effect, index]() {
                    effect->dataChanged(index, index, QVector<int>());
                    return true;
                };
                refresh();
                PUSH_LAMBDA(refresh, redo);
                PUSH_LAMBDA(refresh, undo);
            } else {
                qDebug() << "// NULL Keyframes---------";
            }
        }
    }
    return true;
}

bool EffectStackModel::adjustFadeLength(int duration, bool fromStart, bool audioFade, bool videoFade, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    if (fromStart) {
        // Fade in
        if (fadeIns.empty()) {
            if (audioFade) {
                appendEffect(QStringLiteral("fadein"));
            }
            if (videoFade) {
                appendEffect(QStringLiteral("fade_from_black"));
            }
        }
        QList<QModelIndex> indexes;
        auto ptr = m_masterService.lock();
        int in = 0;
        if (ptr) {
            in = ptr->get_int("in");
        }
        qDebug() << "//// SETTING CLIP FADIN: " << duration;
        int oldDuration = -1;
        for (int i = 0; i < rootItem->childCount(); ++i) {
            if (fadeIns.count(std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) > 0) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                if (oldDuration == -1) {
                    oldDuration = effect->filter().get_length();
                }
                effect->filter().set("in", in);
                duration = qMin(pCore->getItemDuration(m_ownerId), duration);
                effect->filter().set("out", in + duration);
                indexes << getIndexFromItem(effect);
            }
        }
        if (!indexes.isEmpty()) {
            emit dataChanged(indexes.first(), indexes.last(), QVector<int>());
            pCore->updateItemModel(m_ownerId, QStringLiteral("fadein"));
            if (videoFade) {
                int min = pCore->getItemPosition(m_ownerId);
                QSize range(min, min + qMax(duration, oldDuration));
                pCore->refreshProjectRange(range);
                if (logUndo) {
                    pCore->invalidateRange(range);
                }
            }
        }
    } else {
        // Fade out
        if (fadeOuts.empty()) {
            if (audioFade) {
                appendEffect(QStringLiteral("fadeout"));
            }
            if (videoFade) {
                appendEffect(QStringLiteral("fade_to_black"));
            }
        }
        int in = 0;
        auto ptr = m_masterService.lock();
        if (ptr) {
            in = ptr->get_int("in");
        }
        int itemDuration = pCore->getItemDuration(m_ownerId);
        int out = in + itemDuration;
        int oldDuration = -1;
        QList<QModelIndex> indexes;
        for (int i = 0; i < rootItem->childCount(); ++i) {
            if (fadeOuts.count(std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) > 0) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                if (oldDuration == -1) {
                    oldDuration = effect->filter().get_length();
                }
                effect->filter().set("out", out);
                duration = qMin(itemDuration, duration);
                effect->filter().set("in", out - duration);
                indexes << getIndexFromItem(effect);
            }
        }
        if (!indexes.isEmpty()) {
            emit dataChanged(indexes.first(), indexes.last(), QVector<int>());
            pCore->updateItemModel(m_ownerId, QStringLiteral("fadeout"));
            if (videoFade) {
                int min = pCore->getItemPosition(m_ownerId);
                QSize range(min + itemDuration - qMax(duration, oldDuration), min + itemDuration);
                pCore->refreshProjectRange(range);
                if (logUndo) {
                    pCore->invalidateRange(range);
                }
            }
        }
    }
    return true;
}

int EffectStackModel::getFadePosition(bool fromStart)
{
    QWriteLocker locker(&m_lock);
    if (fromStart) {
        if (fadeIns.empty()) {
            return 0;
        }
        for (int i = 0; i < rootItem->childCount(); ++i) {
            if (*(fadeIns.begin()) == std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                return effect->filter().get_length();
            }
        }
    } else {
        if (fadeOuts.empty()) {
            return 0;
        }
        for (int i = 0; i < rootItem->childCount(); ++i) {
            if (*(fadeOuts.begin()) == std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                return effect->filter().get_length();
            }
        }
    }
    return 0;
}

bool EffectStackModel::removeFade(bool fromStart)
{
    QWriteLocker locker(&m_lock);
    std::vector<int> toRemove;
    for (int i = 0; i < rootItem->childCount(); ++i) {
        if ((fromStart && fadeIns.count(std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) > 0) ||
            (!fromStart && fadeOuts.count(std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) > 0)) {
            toRemove.push_back(i);
        }
    }
    for (int i : toRemove) {
        std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        removeEffect(effect);
    }
    return true;
}

void EffectStackModel::moveEffect(int destRow, std::shared_ptr<AbstractEffectItem> item)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allItems.count(item->getId()) > 0);
    int oldRow = item->row();
    Fun undo = moveItem_lambda(item->getId(), oldRow);
    Fun redo = moveItem_lambda(item->getId(), destRow);
    bool res = redo();
    if (res) {
        Fun update = [this]() {
            this->dataChanged(QModelIndex(), QModelIndex(), {TimelineModel::EffectNamesRole});
            return true;
        };
        update();
        UPDATE_UNDO_REDO(update, update, undo, redo);
        auto effectId = std::static_pointer_cast<EffectItemModel>(item)->getAssetId();
        QString effectName = EffectsRepository::get()->getName(effectId);
        PUSH_UNDO(undo, redo, i18n("Move effect %1", effectName));
    }
}

void EffectStackModel::registerItem(const std::shared_ptr<TreeItem> &item)
{
    QWriteLocker locker(&m_lock);
    // qDebug() << "$$$$$$$$$$$$$$$$$$$$$ Planting effect";
    QModelIndex ix;
    if (!item->isRoot()) {
        auto effectItem = std::static_pointer_cast<EffectItemModel>(item);
        if (!m_loadingExisting) {
            // qDebug() << "$$$$$$$$$$$$$$$$$$$$$ Planting effect in " << m_childServices.size();
            effectItem->plant(m_masterService);
            for (const auto &service : m_childServices) {
                // qDebug() << "$$$$$$$$$$$$$$$$$$$$$ Planting CLONE effect in " << (void *)service.lock().get();
                effectItem->plantClone(service);
            }
        }
        effectItem->setEffectStackEnabled(m_effectStackEnabled);
        const QString &effectId = effectItem->getAssetId();
        if (effectId == QLatin1String("fadein") || effectId == QLatin1String("fade_from_black")) {
            fadeIns.insert(effectItem->getId());
        } else if (effectId == QLatin1String("fadeout") || effectId == QLatin1String("fade_to_black")) {
            fadeOuts.insert(effectItem->getId());
        }
        ix = getIndexFromItem(effectItem);
        if (!effectItem->isAudio() && !m_loadingExisting) {
            pCore->refreshProjectItem(m_ownerId);
            pCore->invalidateItem(m_ownerId);
        }
    }
    AbstractTreeModel::registerItem(item);
}

void EffectStackModel::deregisterItem(int id, TreeItem *item)
{
    QWriteLocker locker(&m_lock);
    if (!item->isRoot()) {
        auto effectItem = static_cast<AbstractEffectItem *>(item);
        effectItem->unplant(m_masterService);
        for (const auto &service : m_childServices) {
            effectItem->unplantClone(service);
        }
        if (!effectItem->isAudio()) {
            pCore->refreshProjectItem(m_ownerId);
            pCore->invalidateItem(m_ownerId);
        }
    }
    AbstractTreeModel::deregisterItem(id, item);
}

void EffectStackModel::setEffectStackEnabled(bool enabled)
{
    QWriteLocker locker(&m_lock);
    m_effectStackEnabled = enabled;

    // Recursively updates children states
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(rootItem->child(i))->setEffectStackEnabled(enabled);
    }
    emit dataChanged(QModelIndex(), QModelIndex(), {TimelineModel::EffectsEnabledRole});
    emit enabledStateChanged();
}

std::shared_ptr<AbstractEffectItem> EffectStackModel::getEffectStackRow(int row, std::shared_ptr<TreeItem> parentItem)
{
    return std::static_pointer_cast<AbstractEffectItem>(parentItem ? rootItem->child(row) : rootItem->child(row));
}

bool EffectStackModel::importEffects(std::shared_ptr<EffectStackModel> sourceStack, PlaylistState::ClipState state)
{
    QWriteLocker locker(&m_lock);
    // TODO: manage fades, keyframes if clips don't have same size / in point
    bool found = false;
    for (int i = 0; i < sourceStack->rowCount(); i++) {
        auto item = sourceStack->getEffectStackRow(i);
        // NO undo. this should only be used on project opening
        if (copyEffect(item, state, false)) {
            found = true;
        }
    }
    if (found) {
        modelChanged();
    }
    return found;
}

bool EffectStackModel::importEffects(std::shared_ptr<EffectStackModel> sourceStack, PlaylistState::ClipState state, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    // TODO: manage fades, keyframes if clips don't have same size / in point
    bool found = false;
    for (int i = 0; i < sourceStack->rowCount(); i++) {
        auto item = sourceStack->getEffectStackRow(i);
        if (copyEffect(item, state, undo, redo)) {
            found = true;
        }
    }
    if (found) {
        modelChanged();
    }
    return found;
}

void EffectStackModel::importEffects(std::weak_ptr<Mlt::Service> service, PlaylistState::ClipState state, bool alreadyExist)
{
    QWriteLocker locker(&m_lock);
    m_loadingExisting = alreadyExist;
    if (auto ptr = service.lock()) {
        for (int i = 0; i < ptr->filter_count(); i++) {
            std::unique_ptr<Mlt::Service> filter(ptr->filter(i));
            if (filter->get("kdenlive_id") == nullptr) {
                // don't consider internal MLT stuff
                continue;
            }
            const QString effectId = qstrdup(filter->get("kdenlive_id"));
            // The MLT filter already exists, use it directly to create the effect
            std::shared_ptr<EffectItemModel> effect;
            if (alreadyExist) {
                // effect is already plugged in the service
                effect = EffectItemModel::construct(std::move(filter), shared_from_this());
            } else {
                // duplicate effect
                std::unique_ptr<Mlt::Filter> asset = EffectsRepository::get()->getEffect(effectId);
                asset->inherit(*(filter));
                effect = EffectItemModel::construct(std::move(asset), shared_from_this());
            }
            if (effect->isAudio()) {
                if (state == PlaylistState::VideoOnly) {
                    // Don't import effect
                    continue;
                }
            } else if (state == PlaylistState::AudioOnly) {
                // Don't import effect
                continue;
            }
            connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
            connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
            Fun redo = addItem_lambda(effect, rootItem->getId());
            effect->prepareKeyframes();
            if (redo()) {
                if (effectId == QLatin1String("fadein") || effectId == QLatin1String("fade_from_black")) {
                    fadeIns.insert(effect->getId());
                } else if (effectId == QLatin1String("fadeout") || effectId == QLatin1String("fade_to_black")) {
                    fadeOuts.insert(effect->getId());
                }
            }
        }
    }
    m_loadingExisting = false;
    modelChanged();
}

void EffectStackModel::setActiveEffect(int ix)
{
    QWriteLocker locker(&m_lock);
    if (auto ptr = m_masterService.lock()) {
        ptr->set("kdenlive:activeeffect", ix);
    }
    pCore->updateItemKeyframes(m_ownerId);
}

int EffectStackModel::getActiveEffect() const
{
    QWriteLocker locker(&m_lock);
    if (auto ptr = m_masterService.lock()) {
        return ptr->get_int("kdenlive:activeeffect");
    }
    return 0;
}

void EffectStackModel::slotCreateGroup(std::shared_ptr<EffectItemModel> childEffect)
{
    QWriteLocker locker(&m_lock);
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

    for (const auto &service : m_childServices) {
        auto ptr = service.lock();
        if (!ptr) {
            qDebug() << "ERROR: unavailable service";
            return false;
        }
        if (ptr->filter_count() != (int)allFilters.size()) {
            // MLT inserts some default normalizer filters that are not managed by Kdenlive, which explains  why the filter count is not equal
            int kdenliveFilterCount = 0;
            for (int i = 0; i < ptr->filter_count(); i++) {
                std::shared_ptr<Mlt::Filter> filt(ptr->filter(i));
                if (filt->get("kdenlive_id") != NULL) {
                    kdenliveFilterCount++;
                }
                // qDebug() << "FILTER: "<<i<<" : "<<ptr->filter(i)->get("mlt_service");
            }
            if (kdenliveFilterCount != (int)allFilters.size()) {
                qDebug() << "ERROR: Wrong filter count: " << kdenliveFilterCount << " = " << allFilters.size();
                return false;
            }
        }

        int ct = 0;
        for (uint i = 0; i < allFilters.size(); ++i) {
            while (ptr->filter(ct)->get("kdenlive_id") == NULL && ct < ptr->filter_count()) {
                ct++;
            }
            auto mltFilter = ptr->filter(ct);
            auto currentFilter = allFilters[i]->filter();
            if (QString(currentFilter.get("mlt_service")) != QLatin1String(mltFilter->get("mlt_service"))) {
                qDebug() << "ERROR: filter " << i << "differ: " << ct << ", " << currentFilter.get("mlt_service") << " = " << mltFilter->get("mlt_service");
                return false;
            }
            QVector<QPair<QString, QVariant>> params = allFilters[i]->getAllParameters();
            for (auto val : params) {
                // Check parameters values
                if (val.second != QVariant(mltFilter->get(val.first.toUtf8().constData()))) {
                    qDebug() << "ERROR: filter " << i << "PARAMETER MISMATCH: " << val.first << " = " << val.second
                             << " != " << mltFilter->get(val.first.toUtf8().constData());
                    return false;
                }
            }
            ct++;
        }
    }

    return true;
}

void EffectStackModel::adjust(const QString &effectId, const QString &effectName, double value)
{
    QWriteLocker locker(&m_lock);
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        if (effectId == sourceEffect->getAssetId()) {
            sourceEffect->setParameter(effectName, QString::number(value));
            return;
        }
    }
}

bool EffectStackModel::hasFilter(const QString &effectId) const
{
    READ_LOCK();
    return rootItem->accumulate_const(false, [effectId](bool b, std::shared_ptr<const TreeItem> it) {
        if (b) return true;
        auto item = std::static_pointer_cast<const AbstractEffectItem>(it);
        if (item->effectItemType() == EffectItemType::Group) {
            return false;
        }
        auto sourceEffect = std::static_pointer_cast<const EffectItemModel>(it);
        return effectId == sourceEffect->getAssetId();
    });
}

double EffectStackModel::getFilterParam(const QString &effectId, const QString &paramName)
{
    READ_LOCK();
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
    int ix = 0;
    if (auto ptr = m_masterService.lock()) {
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
    QWriteLocker locker(&m_lock);
    auto effectItem = std::static_pointer_cast<EffectItemModel>(asset);
    int oldRow = effectItem->row();
    int count = rowCount();
    for (int ix = oldRow; ix < count; ix++) {
        auto item = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
        item->unplant(m_masterService);
        for (const auto &service : m_childServices) {
            item->unplantClone(service);
        }
    }
    std::unique_ptr<Mlt::Properties> effect = EffectsRepository::get()->getEffect(effectItem->getAssetId());
    effect->inherit(effectItem->filter());
    effectItem->resetAsset(std::move(effect));
    for (int ix = oldRow; ix < count; ix++) {
        auto item = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
        item->unplant(m_masterService);
        for (const auto &service : m_childServices) {
            item->plantClone(service);
        }
    }
}

void EffectStackModel::cleanFadeEffects(bool outEffects, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    const auto &toDelete = outEffects ? fadeOuts : fadeIns;
    for (int id : toDelete) {
        auto effect = std::static_pointer_cast<EffectItemModel>(getItemById(id));
        Fun operation = removeItem_lambda(id);
        if (operation()) {
            Fun reverse = addItem_lambda(effect, rootItem->getId());
            UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        }
    }
    if (!toDelete.empty()) {
        Fun updateRedo = [this, toDelete, outEffects]() {
            for (int id : toDelete) {
                if (outEffects) {
                    fadeOuts.erase(id);
                } else {
                    fadeIns.erase(id);
                }
            }
            QVector<int> roles = {TimelineModel::EffectNamesRole};
            roles << (outEffects ? TimelineModel::FadeOutRole : TimelineModel::FadeInRole);
            emit dataChanged(QModelIndex(), QModelIndex(), roles);
            pCore->updateItemKeyframes(m_ownerId);
            return true;
        };
        updateRedo();
        PUSH_LAMBDA(updateRedo, redo);
    }
}

const QString EffectStackModel::effectNames() const
{
    QStringList effects;
    for (int i = 0; i < rootItem->childCount(); ++i) {
        effects.append(EffectsRepository::get()->getName(std::static_pointer_cast<EffectItemModel>(rootItem->child(i))->getAssetId()));
    }
    return effects.join(QLatin1Char('/'));
}

bool EffectStackModel::isStackEnabled() const
{
    return m_effectStackEnabled;
}

bool EffectStackModel::addEffectKeyFrame(int frame, double normalisedVal)
{
    if (rootItem->childCount() == 0) return false;
    int ix = 0;
    if (auto ptr = m_masterService.lock()) {
        ix = ptr->get_int("kdenlive:activeeffect");
    }
    if (ix < 0) {
        return false;
    }
    std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
    std::shared_ptr<KeyframeModelList> listModel = sourceEffect->getKeyframeModel();
    return listModel->addKeyframe(frame, normalisedVal);
}

bool EffectStackModel::removeKeyFrame(int frame)
{
    if (rootItem->childCount() == 0) return false;
    int ix = 0;
    if (auto ptr = m_masterService.lock()) {
        ix = ptr->get_int("kdenlive:activeeffect");
    }
    if (ix < 0) {
        return false;
    }
    std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
    std::shared_ptr<KeyframeModelList> listModel = sourceEffect->getKeyframeModel();
    return listModel->removeKeyframe(GenTime(frame, pCore->getCurrentFps()));
}

bool EffectStackModel::updateKeyFrame(int oldFrame, int newFrame, QVariant normalisedVal)
{
    if (rootItem->childCount() == 0) return false;
    int ix = 0;
    if (auto ptr = m_masterService.lock()) {
        ix = ptr->get_int("kdenlive:activeeffect");
    }
    if (ix < 0) {
        return false;
    }
    std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
    std::shared_ptr<KeyframeModelList> listModel = sourceEffect->getKeyframeModel();
    return listModel->updateKeyframe(GenTime(oldFrame, pCore->getCurrentFps()), GenTime(newFrame, pCore->getCurrentFps()), normalisedVal);
}
