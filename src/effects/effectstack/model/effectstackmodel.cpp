/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "effectstackmodel.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "core.h"
#include "doc/docundostack.hpp"
#include "effectgroupmodel.hpp"
#include "effectitemmodel.hpp"
#include "effects/effectsrepository.hpp"
#include "macros.hpp"
#include "mainwindow.h"
#include "timeline2/model/timelinemodel.hpp"
#include <profiles/profilemodel.hpp>
#include <stack>
#include <utility>
#include <vector>

EffectStackModel::EffectStackModel(std::weak_ptr<Mlt::Service> service, ObjectId ownerId, std::weak_ptr<DocUndoStack> undo_stack)
    : AbstractTreeModel()
    , m_masterService(std::move(service))
    , m_effectStackEnabled(true)
    , m_ownerId(std::move(ownerId))
    , m_undoStack(std::move(undo_stack))
    , m_lock(QReadWriteLock::Recursive)
    , m_loadingExisting(false)
{
}

std::shared_ptr<EffectStackModel> EffectStackModel::construct(std::weak_ptr<Mlt::Service> service, ObjectId ownerId, std::weak_ptr<DocUndoStack> undo_stack)
{
    std::shared_ptr<EffectStackModel> self(new EffectStackModel(std::move(service), ownerId, std::move(undo_stack)));
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

void EffectStackModel::removeService(const std::shared_ptr<Mlt::Service> &service)
{
    QWriteLocker locker(&m_lock);
    std::vector<int> to_delete;
    for (int i = int(m_childServices.size()) - 1; i >= 0; --i) {
        auto ptr = m_childServices[uint(i)].lock();
        if (ptr && service->get_int("_childid") == ptr->get_int("_childid")) {
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
    int ix = getActiveEffect();
    if (ix < 0) {
        return;
    }
    std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
    if (effect) {
        removeEffect(effect);
    }
}

void EffectStackModel::removeAllEffects(Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    int current = getActiveEffect();
    while (rootItem->childCount() > 0) {
        std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(0));
        int parentId = -1;
        if (auto ptr = effect->parentItem().lock()) parentId = ptr->getId();
        Fun local_undo = addItem_lambda(effect, parentId);
        Fun local_redo = removeItem_lambda(effect->getId());
        local_redo();
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    }
    std::unordered_set<int> fadeIns = m_fadeIns;
    std::unordered_set<int> fadeOuts = m_fadeOuts;
    Fun undo_current = [this, current, fadeIns, fadeOuts]() {
        if (auto srv = m_masterService.lock()) {
            srv->set("kdenlive:activeeffect", current);
        }
        m_fadeIns = fadeIns;
        m_fadeOuts = fadeOuts;
        QVector<int> roles = {TimelineModel::EffectNamesRole};
        if (!m_fadeIns.empty()) {
            roles << TimelineModel::FadeInRole;
        }
        if (!m_fadeOuts.empty()) {
            roles << TimelineModel::FadeOutRole;
        }
        Q_EMIT dataChanged(QModelIndex(), QModelIndex(), roles);
        pCore->updateItemKeyframes(m_ownerId);
        return true;
    };
    Fun redo_current = [this]() {
        if (auto srv = m_masterService.lock()) {
            srv->set("kdenlive:activeeffect", -1);
        }
        QVector<int> roles = {TimelineModel::EffectNamesRole};
        if (!m_fadeIns.empty()) {
            roles << TimelineModel::FadeInRole;
        }
        if (!m_fadeOuts.empty()) {
            roles << TimelineModel::FadeOutRole;
        }
        m_fadeIns.clear();
        m_fadeOuts.clear();
        Q_EMIT dataChanged(QModelIndex(), QModelIndex(), roles);
        pCore->updateItemKeyframes(m_ownerId);
        return true;
    };
    redo_current();
    PUSH_LAMBDA(redo_current, redo);
    PUSH_LAMBDA(undo_current, undo);
}

void EffectStackModel::removeEffect(const std::shared_ptr<EffectItemModel> &effect)
{
    qDebug() << "* * ** REMOVING EFFECT FROM STACK!!!\n!!!!!!!!!";
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allItems.count(effect->getId()) > 0);
    int parentId = -1;
    if (auto ptr = effect->parentItem().lock()) parentId = ptr->getId();
    int current = getActiveEffect();
    if (current >= rootItem->childCount() - 1) {
        setActiveEffect(current - 1);
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
        int inFades = int(m_fadeIns.size());
        int outFades = int(m_fadeOuts.size());
        m_fadeIns.erase(effect->getId());
        m_fadeOuts.erase(effect->getId());
        inFades = int(m_fadeIns.size()) - inFades;
        outFades = int(m_fadeOuts.size()) - outFades;
        QString effectName = EffectsRepository::get()->getName(effect->getAssetId());
        Fun update = [this, inFades, outFades]() {
            // Required to build the effect view
            if (rowCount() == 0) {
                // Stack is now empty
                Q_EMIT dataChanged(QModelIndex(), QModelIndex(), {});
            } else {
                QVector<int> roles = {TimelineModel::EffectNamesRole};
                if (inFades < 0) {
                    roles << TimelineModel::FadeInRole;
                }
                if (outFades < 0) {
                    roles << TimelineModel::FadeOutRole;
                }
                Q_EMIT dataChanged(QModelIndex(), QModelIndex(), roles);
            }
            // TODO: only update if effect is fade or keyframe
            /*if (inFades < 0) {
                pCore->updateItemModel(m_ownerId, QStringLiteral("fadein"));
            } else if (outFades < 0) {
                pCore->updateItemModel(m_ownerId, QStringLiteral("fadeout"));
            }*/
            updateEffectZones();
            pCore->updateItemKeyframes(m_ownerId);
            return true;
        };
        Fun update2 = [this, inFades, outFades, current]() {
            // Required to build the effect view
            QVector<int> roles = {TimelineModel::EffectNamesRole};
            // TODO: only update if effect is fade or keyframe
            if (inFades < 0) {
                roles << TimelineModel::FadeInRole;
            } else if (outFades < 0) {
                roles << TimelineModel::FadeOutRole;
            }
            Q_EMIT dataChanged(QModelIndex(), QModelIndex(), roles);
            updateEffectZones();
            pCore->updateItemKeyframes(m_ownerId);
            setActiveEffect(current);
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

bool EffectStackModel::copyXmlEffect(const QDomElement &effect)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool result = fromXml(effect, undo, redo);
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Copy effect"));
    }
    return result;
}

QDomElement EffectStackModel::toXml(QDomDocument &document)
{
    QDomElement container = document.createElement(QStringLiteral("effects"));
    int currentIn = pCore->getItemIn(m_ownerId);
    container.setAttribute(QStringLiteral("parentIn"), currentIn);
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        QDomElement sub = document.createElement(QStringLiteral("effect"));
        sub.setAttribute(QStringLiteral("id"), sourceEffect->getAssetId());
        int filterIn = sourceEffect->filter().get_int("in");
        int filterOut = sourceEffect->filter().get_int("out");
        if (filterOut > filterIn) {
            sub.setAttribute(QStringLiteral("in"), filterIn);
            sub.setAttribute(QStringLiteral("out"), filterOut);
        }
        QStringList passProps{QStringLiteral("disable"), QStringLiteral("kdenlive:collapsed")};
        for (const QString &param : passProps) {
            int paramVal = sourceEffect->filter().get_int(param.toUtf8().constData());
            if (paramVal > 0) {
                Xml::setXmlProperty(sub, param, QString::number(paramVal));
            }
        }
        QVector<QPair<QString, QVariant>> params = sourceEffect->getAllParameters();
        for (const auto &param : qAsConst(params)) {
            Xml::setXmlProperty(sub, param.first, param.second.toString());
        }
        container.appendChild(sub);
    }
    return container;
}

QDomElement EffectStackModel::rowToXml(const QUuid &uuid, int row, QDomDocument &document)
{
    QDomElement container = document.createElement(QStringLiteral("effects"));
    if (row < 0 || row >= rootItem->childCount()) {
        return container;
    }
    int currentIn = pCore->getItemIn(uuid, m_ownerId);
    container.setAttribute(QStringLiteral("parentIn"), currentIn);
    std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(row));
    QDomElement sub = document.createElement(QStringLiteral("effect"));
    sub.setAttribute(QStringLiteral("id"), sourceEffect->getAssetId());
    int filterIn = sourceEffect->filter().get_int("in");
    int filterOut = sourceEffect->filter().get_int("out");
    if (filterOut > filterIn) {
        sub.setAttribute(QStringLiteral("in"), filterIn);
        sub.setAttribute(QStringLiteral("out"), filterOut);
    }
    QStringList passProps{QStringLiteral("disable"), QStringLiteral("kdenlive:collapsed")};
    for (const QString &param : passProps) {
        int paramVal = sourceEffect->filter().get_int(param.toUtf8().constData());
        if (paramVal > 0) {
            Xml::setXmlProperty(sub, param, QString::number(paramVal));
        }
    }
    QVector<QPair<QString, QVariant>> params = sourceEffect->getAllParameters();
    for (const auto &param : qAsConst(params)) {
        Xml::setXmlProperty(sub, param.first, param.second.toString());
    }
    container.appendChild(sub);
    return container;
}

bool EffectStackModel::fromXml(const QDomElement &effectsXml, Fun &undo, Fun &redo)
{
    QDomNodeList nodeList = effectsXml.elementsByTagName(QStringLiteral("effect"));
    int parentIn = effectsXml.attribute(QStringLiteral("parentIn")).toInt();
    qDebug() << "// GOT PREVIOUS PARENTIN: " << parentIn << "\n\n=======\n=======\n\n";
    int currentIn = pCore->getItemIn(m_ownerId);
    PlaylistState::ClipState state = pCore->getItemState(m_ownerId);
    bool effectAdded = false;
    for (int i = 0; i < nodeList.count(); ++i) {
        QDomElement node = nodeList.item(i).toElement();
        const QString effectId = node.attribute(QStringLiteral("id"));
        bool isAudioEffect = EffectsRepository::get()->isAudioEffect(effectId);
        if (state == PlaylistState::VideoOnly) {
            if (isAudioEffect) {
                continue;
            }
        } else if (state == PlaylistState::AudioOnly) {
            if (!isAudioEffect) {
                continue;
            }
        }
        if (m_ownerId.type == ObjectType::TimelineClip && EffectsRepository::get()->isUnique(effectId) && hasEffect(effectId)) {
            pCore->displayMessage(i18n("Effect %1 cannot be added twice.", EffectsRepository::get()->getName(effectId)), ErrorMessage);
            return false;
        }
        bool effectEnabled = true;
        if (Xml::hasXmlProperty(node, QLatin1String("disable"))) {
            effectEnabled = Xml::getXmlProperty(node, QLatin1String("disable")).toInt() != 1;
        }
        auto effect = EffectItemModel::construct(effectId, shared_from_this(), effectEnabled);
        const QString in = node.attribute(QStringLiteral("in"));
        const QString out = node.attribute(QStringLiteral("out"));
        if (!out.isEmpty()) {
            effect->filter().set("in", in.toUtf8().constData());
            effect->filter().set("out", out.toUtf8().constData());
        }
        QMap<QString, std::pair<ParamType, bool>> keyframeParams = effect->getKeyframableParameters();
        QVector<QPair<QString, QVariant>> parameters;
        QDomNodeList params = node.elementsByTagName(QStringLiteral("property"));
        for (int j = 0; j < params.count(); j++) {
            QDomElement pnode = params.item(j).toElement();
            const QString pName = pnode.attribute(QStringLiteral("name"));
            if (pName == QLatin1String("in") || pName == QLatin1String("out")) {
                continue;
            }
            if (keyframeParams.contains(pName)) {
                // This is a keyframable parameter, fix offset
                int currentDuration = pCore->getItemDuration(m_ownerId);
                if (currentDuration > 1) {
                    currentDuration--;
                    currentDuration += currentIn;
                }
                QString pValue = KeyframeModel::getAnimationStringWithOffset(effect, pnode.text(), currentIn - parentIn, currentDuration,
                                                                             keyframeParams.value(pName).first, keyframeParams.value(pName).second);
                parameters.append(QPair<QString, QVariant>(pName, QVariant(pValue)));
            } else {
                parameters.append(QPair<QString, QVariant>(pName, QVariant(pnode.text())));
            }
        }
        effect->setParameters(parameters);
        Fun local_undo = removeItem_lambda(effect->getId());
        // TODO the parent should probably not always be the root
        Fun local_redo = addItem_lambda(effect, rootItem->getId());
        effect->prepareKeyframes();
        connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
        connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
        connect(effect.get(), &AssetParameterModel::showEffectZone, this, &EffectStackModel::updateEffectZones);
        if (effectId.startsWith(QLatin1String("fadein")) || effectId.startsWith(QLatin1String("fade_from_"))) {
            m_fadeIns.insert(effect->getId());
            int duration = effect->filter().get_length() - 1;
            effect->filter().set("in", currentIn);
            effect->filter().set("out", currentIn + duration);
            if (effectId.startsWith(QLatin1String("fade_"))) {
                if (effect->filter().get("alpha") == QLatin1String("1")) {
                    // Adjust level value to match filter end
                    effect->filter().set("level", "0=0;-1=1");
                } else if (effect->filter().get("level") == QLatin1String("1")) {
                    effect->filter().set("alpha", "0=0;-1=1");
                }
            }
        } else if (effectId.startsWith(QLatin1String("fadeout")) || effectId.startsWith(QLatin1String("fade_to_"))) {
            m_fadeOuts.insert(effect->getId());
            int duration = effect->filter().get_length() - 1;
            int filterOut = pCore->getItemIn(m_ownerId) + pCore->getItemDuration(m_ownerId) - 1;
            effect->filter().set("in", filterOut - duration);
            effect->filter().set("out", filterOut);
            if (effectId.startsWith(QLatin1String("fade_"))) {
                if (effect->filter().get("alpha") == QLatin1String("1")) {
                    // Adjust level value to match filter end
                    effect->filter().set("level", "0=1;-1=0");
                } else if (effect->filter().get("level") == QLatin1String("1")) {
                    effect->filter().set("alpha", "0=1;-1=0");
                }
            }
        }
        local_redo();
        effectAdded = true;
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    }
    if (effectAdded) {
        Fun update = [this]() {
            Q_EMIT dataChanged(QModelIndex(), QModelIndex(), {});
            return true;
        };
        update();
        PUSH_LAMBDA(update, redo);
        PUSH_LAMBDA(update, undo);
    }
    return effectAdded;
}

bool EffectStackModel::copyEffect(const std::shared_ptr<AbstractEffectItem> &sourceItem, PlaylistState::ClipState state, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    if (sourceItem->childCount() > 0) {
        // TODO: group
        return false;
    }
    bool audioEffect = sourceItem->isAudio();
    if (state == PlaylistState::VideoOnly) {
        if (audioEffect) {
            // This effect cannot be used
            return false;
        }
    } else if (state == PlaylistState::AudioOnly) {
        if (!audioEffect) {
            return false;
        }
    }
    std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(sourceItem);
    const QString effectId = sourceEffect->getAssetId();
    if (m_ownerId.type == ObjectType::TimelineClip && EffectsRepository::get()->isUnique(effectId) && hasEffect(effectId)) {
        pCore->displayMessage(i18n("Effect %1 cannot be added twice.", EffectsRepository::get()->getName(effectId)), ErrorMessage);
        return false;
    }
    bool enabled = sourceEffect->isEnabled();
    auto effect = EffectItemModel::construct(effectId, shared_from_this(), enabled);
    effect->setParameters(sourceEffect->getAllParameters());
    if (!enabled) {
        effect->filter().set("disable", 1);
    }
    if (m_ownerId.type == ObjectType::TimelineTrack || m_ownerId.type == ObjectType::Master) {
        effect->filter().set("in", 0);
        effect->filter().set("out", pCore->getItemDuration(m_ownerId) - 1);
    } else {
        effect->filter().set("in", sourceEffect->filter().get_int("in"));
        effect->filter().set("out", sourceEffect->filter().get_int("out"));
    }
    Fun local_undo = removeItem_lambda(effect->getId());
    // TODO the parent should probably not always be the root
    Fun local_redo = addItem_lambda(effect, rootItem->getId());
    effect->prepareKeyframes();
    connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
    connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
    connect(effect.get(), &AssetParameterModel::showEffectZone, this, &EffectStackModel::updateEffectZones);
    QVector<int> roles = {TimelineModel::EffectNamesRole};
    if (effectId.startsWith(QLatin1String("fadein")) || effectId.startsWith(QLatin1String("fade_from_"))) {
        m_fadeIns.insert(effect->getId());
        int duration = effect->filter().get_length() - 1;
        int in = pCore->getItemIn(m_ownerId);
        effect->filter().set("in", in);
        effect->filter().set("out", in + duration);
        roles << TimelineModel::FadeInRole;
    } else if (effectId.startsWith(QLatin1String("fadeout")) || effectId.startsWith(QLatin1String("fade_to_"))) {
        m_fadeOuts.insert(effect->getId());
        int duration = effect->filter().get_length() - 1;
        int out = pCore->getItemIn(m_ownerId) + pCore->getItemDuration(m_ownerId) - 1;
        effect->filter().set("in", out - duration);
        effect->filter().set("out", out);
        roles << TimelineModel::FadeOutRole;
    }
    bool res = local_redo();
    if (res) {
        Fun update = [this, roles]() {
            Q_EMIT dataChanged(QModelIndex(), QModelIndex(), roles);
            return true;
        };
        update();
        if (logUndo) {
            PUSH_LAMBDA(update, local_redo);
            PUSH_LAMBDA(update, local_undo);
            pCore->pushUndo(local_undo, local_redo, i18n("Paste effect"));
        }
    }
    return res;
}

bool EffectStackModel::appendEffect(const QString &effectId, bool makeCurrent, stringMap params)
{
    QWriteLocker locker(&m_lock);
    if (m_ownerId.type == ObjectType::TimelineClip && EffectsRepository::get()->isUnique(effectId) && hasEffect(effectId)) {
        pCore->displayMessage(i18n("Effect %1 cannot be added twice.", EffectsRepository::get()->getName(effectId)), ErrorMessage);
        return false;
    }
    std::unordered_set<int> previousFadeIn = m_fadeIns;
    std::unordered_set<int> previousFadeOut = m_fadeOuts;
    if (EffectsRepository::get()->isGroup(effectId)) {
        QDomElement doc = EffectsRepository::get()->getXml(effectId);
        return copyXmlEffect(doc);
    }
    auto effect = EffectItemModel::construct(effectId, shared_from_this());
    PlaylistState::ClipState state = pCore->getItemState(m_ownerId);
    if (state == PlaylistState::VideoOnly) {
        if (effect->isAudio()) {
            // Cannot add effect to this clip
            pCore->displayMessage(i18n("Cannot add effect to clip"), ErrorMessage);
            return false;
        }
    } else if (state == PlaylistState::AudioOnly) {
        if (!effect->isAudio()) {
            // Cannot add effect to this clip
            pCore->displayMessage(i18n("Cannot add effect to clip"), ErrorMessage);
            return false;
        }
    }
    QMapIterator<QString, QString> i(params);
    while (i.hasNext()) {
        i.next();
        effect->filter().set(i.key().toUtf8().constData(), i.value().toUtf8().constData());
    }
    Fun undo = removeItem_lambda(effect->getId());
    // TODO the parent should probably not always be the root
    Fun redo = addItem_lambda(effect, rootItem->getId());
    effect->prepareKeyframes();
    connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
    connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
    connect(effect.get(), &AssetParameterModel::showEffectZone, this, &EffectStackModel::updateEffectZones);
    int currentActive = getActiveEffect();
    if (makeCurrent) {
        setActiveEffect(rowCount());
    }
    bool res = redo();
    if (res) {
        int inFades = 0;
        int outFades = 0;
        if (effectId.startsWith(QLatin1String("fadein")) || effectId.startsWith(QLatin1String("fade_from_"))) {
            int duration = effect->filter().get_length() - 1;
            int in = pCore->getItemIn(m_ownerId);
            effect->filter().set("in", in);
            effect->filter().set("out", in + duration);
            inFades++;
        } else if (effectId.startsWith(QLatin1String("fadeout")) || effectId.startsWith(QLatin1String("fade_to_"))) {
            /*int duration = effect->filter().get_length() - 1;
            int out = pCore->getItemIn(m_ownerId) + pCore->getItemDuration(m_ownerId) - 1;
            effect->filter().set("in", out - duration);
            effect->filter().set("out", out);*/
            outFades++;
        } else if (m_ownerId.type == ObjectType::TimelineTrack) {
            effect->filter().set("out", pCore->getItemDuration(m_ownerId));
        }
        Fun update = [this, inFades, outFades]() {
            // TODO: only update if effect is fade or keyframe
            QVector<int> roles = {TimelineModel::EffectNamesRole};
            if (inFades > 0) {
                roles << TimelineModel::FadeInRole;
            } else if (outFades > 0) {
                roles << TimelineModel::FadeOutRole;
            }
            pCore->updateItemKeyframes(m_ownerId);
            Q_EMIT dataChanged(QModelIndex(), QModelIndex(), roles);
            return true;
        };
        Fun update_undo = [this, inFades, outFades, previousFadeIn, previousFadeOut]() {
            // TODO: only update if effect is fade or keyframe
            QVector<int> roles = {TimelineModel::EffectNamesRole};
            if (inFades > 0) {
                m_fadeIns = previousFadeIn;
                roles << TimelineModel::FadeInRole;
            } else if (outFades > 0) {
                m_fadeOuts = previousFadeOut;
                roles << TimelineModel::FadeOutRole;
            }
            pCore->updateItemKeyframes(m_ownerId);
            Q_EMIT dataChanged(QModelIndex(), QModelIndex(), roles);
            return true;
        };
        update();
        PUSH_LAMBDA(update, redo);
        PUSH_LAMBDA(update_undo, undo);
        PUSH_UNDO(undo, redo, i18n("Add effect %1", EffectsRepository::get()->getName(effectId)));
    } else if (makeCurrent) {
        setActiveEffect(currentActive);
    }
    return res;
}

bool EffectStackModel::adjustStackLength(bool adjustFromEnd, int oldIn, int oldDuration, int newIn, int duration, int offset, Fun &undo, Fun &redo,
                                         bool logUndo)
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
        if (fadeInDuration > 0 && m_fadeIns.count(leaf->getId()) > 0) {
            // Adjust fade in
            int oldEffectIn = qMax(0, effect->filter().get_in());
            int oldEffectOut = effect->filter().get_out();
            qDebug() << "--previous effect: " << oldEffectIn << "-" << oldEffectOut;
            int effectDuration = qMin(effect->filter().get_length() - 1, duration);
            if (!adjustFromEnd && (oldIn != newIn || duration != oldDuration)) {
                // Clip start was resized, adjust effect in / out
                Fun operation = [effect, newIn, effectDuration, logUndo]() {
                    effect->setParameter(QStringLiteral("in"), newIn, false);
                    effect->setParameter(QStringLiteral("out"), newIn + effectDuration, logUndo);
                    qDebug() << "--new effect: " << newIn << "-" << newIn + effectDuration;
                    return true;
                };
                bool res = operation();
                if (!res) {
                    return false;
                }
                Fun reverse = [effect, oldEffectIn, oldEffectOut, logUndo]() {
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
                Fun operation = [effect, oldEffectIn, effectDuration, logUndo]() {
                    effect->setParameter(QStringLiteral("out"), oldEffectIn + effectDuration, logUndo);
                    return true;
                };
                bool res = operation();
                if (!res) {
                    return false;
                }
                if (logUndo) {
                    Fun reverse = [effect, referenceEffectOut]() {
                        effect->setParameter(QStringLiteral("out"), referenceEffectOut, true);
                        effect->filter().set("_refout", nullptr);
                        return true;
                    };
                    PUSH_LAMBDA(operation, redo);
                    PUSH_LAMBDA(reverse, undo);
                }
            }
        } else if (fadeOutDuration > 0 && m_fadeOuts.count(leaf->getId()) > 0) {
            // Adjust fade out
            int effectDuration = qMin(fadeOutDuration, duration);
            int newFadeIn = out - effectDuration;
            int oldFadeIn = effect->filter().get_int("in");
            int oldOut = effect->filter().get_int("out");
            int referenceEffectIn = effect->filter().get_int("_refin");
            if (referenceEffectIn <= 0) {
                referenceEffectIn = oldFadeIn;
                effect->filter().set("_refin", referenceEffectIn);
            }
            Fun operation = [effect, newFadeIn, out, logUndo]() {
                effect->setParameter(QStringLiteral("in"), newFadeIn, false);
                effect->setParameter(QStringLiteral("out"), out, logUndo);
                return true;
            };
            bool res = operation();
            if (!res) {
                return false;
            }
            if (logUndo) {
                Fun reverse = [effect, referenceEffectIn, oldOut]() {
                    effect->setParameter(QStringLiteral("in"), referenceEffectIn, false);
                    effect->setParameter(QStringLiteral("out"), oldOut, true);
                    effect->filter().set("_refin", nullptr);
                    return true;
                };
                PUSH_LAMBDA(operation, redo);
                PUSH_LAMBDA(reverse, undo);
            }
        } else {
            // Not a fade effect, check for keyframes
            bool hasZone = effect->filter().get_int("kdenlive:force_in_out") == 1;
            std::shared_ptr<KeyframeModelList> keyframes = effect->getKeyframeModel();
            if (keyframes != nullptr) {
                // Effect has keyframes, update these
                keyframes->resizeKeyframes(oldIn, oldIn + oldDuration, newIn, out - 1, offset, adjustFromEnd, undo, redo);
                QModelIndex index = getIndexFromItem(effect);
                Fun refresh = [effect, index]() {
                    Q_EMIT effect->dataChanged(index, QModelIndex(), QVector<int>());
                    return true;
                };
                refresh();
                PUSH_LAMBDA(refresh, redo);
                PUSH_LAMBDA(refresh, undo);
            } else {
                qDebug() << "// NULL Keyframes---------";
            }
            if (m_ownerId.type == ObjectType::TimelineTrack && !hasZone) {
                int oldEffectOut = effect->filter().get_out();
                Fun operation = [effect, out, logUndo]() {
                    effect->setParameter(QStringLiteral("out"), out, logUndo);
                    return true;
                };
                bool res = operation();
                if (!res) {
                    return false;
                }
                if (logUndo) {
                    Fun reverse = [effect, oldEffectOut]() {
                        effect->setParameter(QStringLiteral("out"), oldEffectOut, true);
                        return true;
                    };
                    PUSH_LAMBDA(operation, redo);
                    PUSH_LAMBDA(reverse, undo);
                }
            } else if (m_ownerId.type == ObjectType::TimelineClip && effect->data(QModelIndex(), AssetParameterModel::RequiresInOut).toBool() == true) {
                int oldEffectIn = qMax(0, effect->filter().get_in());
                int oldEffectOut = effect->filter().get_out();
                int newIn = pCore->getItemIn(m_ownerId);
                int newOut = newIn + pCore->getItemDuration(m_ownerId) - 1;
                Fun operation = [effect, newIn, newOut]() {
                    effect->filter().set_in_and_out(newIn, newOut);
                    qDebug() << "--new effect: " << newIn << "-" << newOut;
                    return true;
                };
                bool res = operation();
                if (!res) {
                    return false;
                }
                Fun reverse = [effect, oldEffectIn, oldEffectOut]() {
                    effect->filter().set_in_and_out(oldEffectIn, oldEffectOut);
                    return true;
                };
                PUSH_LAMBDA(operation, redo);
                PUSH_LAMBDA(reverse, undo);
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
        if (m_fadeIns.empty()) {
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
        int oldDuration = -1;
        for (int i = 0; i < rootItem->childCount(); ++i) {
            if (m_fadeIns.count(std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) > 0) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                if (oldDuration == -1) {
                    oldDuration = effect->filter().get_length();
                }
                effect->filter().set("in", in);
                duration = qMin(pCore->getItemDuration(m_ownerId), duration);
                effect->filter().set("out", in + duration);
                indexes << getIndexFromItem(effect);
                if (effect->filter().get("alpha") == QLatin1String("1")) {
                    // Adjust level value to match filter end
                    effect->filter().set("level", "0=0;-1=1");
                } else if (effect->filter().get("level") == QLatin1String("1")) {
                    effect->filter().set("alpha", "0=0;-1=1");
                }
            }
        }
        if (!indexes.isEmpty()) {
            Q_EMIT dataChanged(indexes.first(), indexes.last(), QVector<int>());
            pCore->updateItemModel(m_ownerId, QStringLiteral("fadein"));
            if (videoFade) {
                int min = pCore->getItemPosition(m_ownerId);
                QPair<int, int> range = {min, min + qMax(duration, oldDuration)};
                pCore->refreshProjectRange(range);
                if (logUndo) {
                    pCore->invalidateRange(range);
                }
            }
        }
    } else {
        // Fade out
        if (m_fadeOuts.empty()) {
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
        int out = in + itemDuration - 1;
        int oldDuration = -1;
        QList<QModelIndex> indexes;
        for (int i = 0; i < rootItem->childCount(); ++i) {
            if (m_fadeOuts.count(std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) > 0) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                if (oldDuration == -1) {
                    oldDuration = effect->filter().get_length();
                }
                effect->filter().set("out", out);
                duration = qMin(itemDuration, duration);
                effect->filter().set("in", out - duration);
                indexes << getIndexFromItem(effect);
                if (effect->filter().get("alpha") == QLatin1String("1")) {
                    // Adjust level value to match filter end
                    effect->filter().set("level", "0=1;-1=0");
                } else if (effect->filter().get("level") == QLatin1String("1")) {
                    effect->filter().set("alpha", "0=1;-1=0");
                }
            }
        }
        if (!indexes.isEmpty()) {
            Q_EMIT dataChanged(indexes.first(), indexes.last(), QVector<int>());
            pCore->updateItemModel(m_ownerId, QStringLiteral("fadeout"));
            if (videoFade) {
                int min = pCore->getItemPosition(m_ownerId);
                QPair<int, int> range = {min + itemDuration - qMax(duration, oldDuration), min + itemDuration};
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
        if (m_fadeIns.empty()) {
            return 0;
        }
        for (int i = 0; i < rootItem->childCount(); ++i) {
            if (*(m_fadeIns.begin()) == std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                return effect->filter().get_length() - 1;
            }
        }
    } else {
        if (m_fadeOuts.empty()) {
            return 0;
        }
        for (int i = 0; i < rootItem->childCount(); ++i) {
            if (*(m_fadeOuts.begin()) == std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                return effect->filter().get_length() - 1;
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
        if ((fromStart && m_fadeIns.count(std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) > 0) ||
            (!fromStart && m_fadeOuts.count(std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) > 0)) {
            toRemove.push_back(i);
        }
    }
    // Let's put index in reverse order so we don't mess when deleting
    std::reverse(toRemove.begin(), toRemove.end());
    for (int i : toRemove) {
        std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        removeEffect(effect);
    }
    return true;
}

void EffectStackModel::moveEffect(int destRow, const std::shared_ptr<AbstractEffectItem> &item)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allItems.count(item->getId()) > 0);
    int oldRow = item->row();
    Fun undo = moveItem_lambda(item->getId(), oldRow);
    Fun redo = moveItem_lambda(item->getId(), destRow);
    bool res = redo();
    if (res) {
        Fun update = [this]() {
            Q_EMIT this->dataChanged(QModelIndex(), QModelIndex(), {TimelineModel::EffectNamesRole});
            return true;
        };
        update();
        UPDATE_UNDO_REDO(update, update, undo, redo);
        auto effectId = std::static_pointer_cast<EffectItemModel>(item)->getAssetId();
        PUSH_UNDO(undo, redo, i18n("Move effect %1", EffectsRepository::get()->getName(effectId)));
    }
}

void EffectStackModel::registerItem(const std::shared_ptr<TreeItem> &item)
{
    QWriteLocker locker(&m_lock);
    // qDebug() << "$$$$$$$$$$$$$$$$$$$$$ Planting effect";
    if (!item->isRoot()) {
        auto effectItem = std::static_pointer_cast<EffectItemModel>(item);
        if (effectItem->data(QModelIndex(), AssetParameterModel::RequiresInOut).toBool() == true) {
            int in = pCore->getItemIn(m_ownerId);
            int out = in + pCore->getItemDuration(m_ownerId) - 1;
            effectItem->filter().set_in_and_out(in, out);
        }
        if (!m_loadingExisting) {
            // qDebug() << "$$$$$$$$$$$$$$$$$$$$$ Planting effect in " << m_childServices.size();
            effectItem->plant(m_masterService);
            // Check if we have an internal effect that needs to stay on top
            if (m_ownerId.type == ObjectType::Master || m_ownerId.type == ObjectType::TimelineTrack) {
                // check for subtitle effect
                auto ms = m_masterService.lock();
                int ct = ms->filter_count();
                QVector<int> ixToMove;
                for (int i = 0; i < ct; i++) {
                    if (ms->filter(i)->get_int("internal_added") > 0) {
                        ixToMove << i;
                    }
                }
                std::sort(ixToMove.rbegin(), ixToMove.rend());
                for (auto &ix : ixToMove) {
                    if (ix < ct - 1) {
                        ms->move_filter(ix, ct - 1);
                    }
                }
            }
            for (const auto &service : m_childServices) {
                // qDebug() << "$$$$$$$$$$$$$$$$$$$$$ Planting CLONE effect in " << (void *)service.lock().get();
                effectItem->plantClone(service);
            }
        }
        effectItem->setEffectStackEnabled(m_effectStackEnabled);
        const QString &effectId = effectItem->getAssetId();
        if (effectId.startsWith(QLatin1String("fadein")) || effectId.startsWith(QLatin1String("fade_from_"))) {
            m_fadeIns.insert(effectItem->getId());
        } else if (effectId.startsWith(QLatin1String("fadeout")) || effectId.startsWith(QLatin1String("fade_to_"))) {
            m_fadeOuts.insert(effectItem->getId());
        }
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

    QList<QModelIndex> indexes;
    // Recursively updates children states
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::shared_ptr<AbstractEffectItem> item = std::static_pointer_cast<AbstractEffectItem>(rootItem->child(i));
        item->setEffectStackEnabled(enabled);
        indexes << getIndexFromItem(item);
    }
    if (indexes.isEmpty()) {
        return;
    }
    pCore->refreshProjectItem(m_ownerId);
    pCore->invalidateItem(m_ownerId);
    Q_EMIT dataChanged(indexes.first(), indexes.last(), {TimelineModel::EffectsEnabledRole});
    Q_EMIT enabledStateChanged();
}

std::shared_ptr<AbstractEffectItem> EffectStackModel::getEffectStackRow(int row, const std::shared_ptr<TreeItem> &parentItem)
{
    return std::static_pointer_cast<AbstractEffectItem>(parentItem ? parentItem->child(row) : rootItem->child(row));
}

bool EffectStackModel::importEffects(const std::shared_ptr<EffectStackModel> &sourceStack, PlaylistState::ClipState state)
{
    QWriteLocker locker(&m_lock);
    // TODO: manage fades, keyframes if clips don't have same size / in point
    bool found = false;
    bool effectEnabled = rootItem->childCount() > 0;
    int imported = 0;
    for (int i = 0; i < sourceStack->rowCount(); i++) {
        auto item = sourceStack->getEffectStackRow(i);
        // NO undo. this should only be used on project opening
        if (copyEffect(item, state, false)) {
            found = true;
            if (item->isEnabled()) {
                effectEnabled = true;
            }
            imported++;
        }
    }
    if (!effectEnabled && imported == 0) {
        effectEnabled = true;
    }
    m_effectStackEnabled = effectEnabled;
    if (!m_effectStackEnabled) {
        // Mark all effects as disabled by stack
        for (int i = 0; i < rootItem->childCount(); ++i) {
            std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
            sourceEffect->setEffectStackEnabled(false);
            sourceEffect->setEnabled(true);
        }
    }
    if (found) {
        Q_EMIT modelChanged();
    }
    return found;
}

void EffectStackModel::importEffects(const std::weak_ptr<Mlt::Service> &service, PlaylistState::ClipState state, bool alreadyExist,
                                     const QString &originalDecimalPoint, const QUuid &uuid)
{
    QWriteLocker locker(&m_lock);
    m_loadingExisting = alreadyExist;
    bool effectEnabled = true;
    if (auto ptr = service.lock()) {
        int max = ptr->filter_count();
        int imported = 0;
        for (int i = 0; i < max; i++) {
            std::unique_ptr<Mlt::Filter> filter(ptr->filter(i));
            if (filter->get_int("internal_added") > 0 && m_ownerId.type != ObjectType::TimelineTrack) {
                // Required to load master audio effects
                if (m_ownerId.type == ObjectType::Master && filter->get("mlt_service") == QLatin1String("avfilter.subtitles")) {
                    // A subtitle filter, update project
                    QMap<QString, QString> subProperties;
                    // subProperties.insert(QStringLiteral("av.filename"), filter->get("av.filename"));
                    subProperties.insert(QStringLiteral("disable"), filter->get("disable"));
                    subProperties.insert(QStringLiteral("kdenlive:locked"), filter->get("kdenlive:locked"));
                    const QString style = filter->get("av.force_style");
                    if (!style.isEmpty()) {
                        subProperties.insert(QStringLiteral("av.force_style"), style);
                    }
                    pCore->window()->slotInitSubtitle(subProperties, uuid);
                } else if (auto ms = m_masterService.lock()) {
                    ms->attach(*filter.get());
                }
                continue;
            }
            if (!filter->property_exists("kdenlive_id")) {
                // don't consider internal MLT stuff
                continue;
            }
            const QString effectId = qstrdup(filter->get("kdenlive_id"));
            if (m_ownerId.type == ObjectType::TimelineClip && EffectsRepository::get()->isUnique(effectId) && hasEffect(effectId)) {
                pCore->displayMessage(i18n("Effect %1 cannot be added twice.", EffectsRepository::get()->getName(effectId)), ErrorMessage);
                continue;
            }
            if (filter->get_int("disable") == 0) {
                effectEnabled = true;
            }
            // The MLT filter already exists, use it directly to create the effect
            std::shared_ptr<EffectItemModel> effect;
            if (alreadyExist) {
                // effect is already plugged in the service
                effect = EffectItemModel::construct(std::move(filter), shared_from_this(), originalDecimalPoint);
            } else {
                // duplicate effect
                std::unique_ptr<Mlt::Filter> asset = EffectsRepository::get()->getEffect(effectId);
                asset->inherit(*(filter));
                effect = EffectItemModel::construct(std::move(asset), shared_from_this(), originalDecimalPoint);
            }
            if (state == PlaylistState::VideoOnly) {
                if (effect->isAudio()) {
                    // Don't import effect
                    continue;
                }
            } else if (state == PlaylistState::AudioOnly) {
                if (!effect->isAudio()) {
                    // Don't import effect
                    continue;
                }
            }
            imported++;
            connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
            connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
            connect(effect.get(), &AssetParameterModel::showEffectZone, this, &EffectStackModel::updateEffectZones);
            Fun redo = addItem_lambda(effect, rootItem->getId());
            int clipIn = ptr->get_int("in");
            int clipOut = ptr->get_int("out");
            if (clipOut <= clipIn) {
                clipOut = ptr->get_int("length") - 1;
            }
            effect->prepareKeyframes(clipIn, clipOut);
            if (redo()) {
                if (effectId.startsWith(QLatin1String("fadein")) || effectId.startsWith(QLatin1String("fade_from_"))) {
                    m_fadeIns.insert(effect->getId());
                    if (effect->filter().get_int("in") != clipIn) {
                        // Broken fade, fix
                        int filterLength = effect->filter().get_length() - 1;
                        effect->filter().set("in", clipIn);
                        effect->filter().set("out", clipIn + filterLength);
                    }
                } else if (effectId.startsWith(QLatin1String("fadeout")) || effectId.startsWith(QLatin1String("fade_to_"))) {
                    m_fadeOuts.insert(effect->getId());
                    if (effect->filter().get_int("out") != clipOut) {
                        // Broken fade, fix
                        int filterLength = effect->filter().get_length() - 1;
                        effect->filter().set("in", clipOut - filterLength);
                        effect->filter().set("out", clipOut);
                    }
                }
            }
        }
        if (imported == 0) {
            effectEnabled = true;
        }
    }
    m_effectStackEnabled = effectEnabled;
    if (!m_effectStackEnabled) {
        // Mark all effects as disabled by stack
        for (int i = 0; i < rootItem->childCount(); ++i) {
            std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
            sourceEffect->setEffectStackEnabled(false);
            sourceEffect->setEnabled(true);
        }
    }
    m_loadingExisting = false;
    Q_EMIT modelChanged();
}

void EffectStackModel::setActiveEffect(int ix)
{
    QWriteLocker locker(&m_lock);
    int current = -1;
    if (auto ptr = m_masterService.lock()) {
        current = ptr->get_int("kdenlive:activeeffect");
        ptr->set("kdenlive:activeeffect", ix);
    }
    // Desactivate previous effect
    if (current > -1 && current != ix && current < rootItem->childCount()) {
        std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(current));
        if (effect) {
            effect->setActive(false);
            Q_EMIT currentChanged(getIndexFromItem(effect), false);
        }
    }
    // Activate new effect
    if (ix > -1 && ix < rootItem->childCount()) {
        std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
        if (effect) {
            effect->setActive(true);
            Q_EMIT currentChanged(getIndexFromItem(effect), true);
        }
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

void EffectStackModel::slotCreateGroup(const std::shared_ptr<EffectItemModel> &childEffect)
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
        // MLT inserts some default normalizer filters that are not managed by Kdenlive, which explains  why the filter count is not equal
        int kdenliveFilterCount = 0;
        for (int i = 0; i < ptr->filter_count(); i++) {
            std::shared_ptr<Mlt::Filter> filt(ptr->filter(i));
            if (filt->property_exists("kdenlive_id")) {
                kdenliveFilterCount++;
            }
            // qDebug() << "FILTER: "<<i<<" : "<<ptr->filter(i)->get("mlt_service");
        }
        if (kdenliveFilterCount != int(allFilters.size())) {
            qDebug() << "ERROR: Wrong filter count: " << kdenliveFilterCount << " = " << allFilters.size();
            return false;
        }

        int ct = 0;
        for (uint i = 0; i < allFilters.size(); ++i) {
            while (ptr->filter(ct)->get("kdenlive_id") == nullptr && ct < ptr->filter_count()) {
                ct++;
            }
            auto mltFilter = ptr->filter(ct);
            auto currentFilter = allFilters[i]->filter();
            if (QString(currentFilter.get("mlt_service")) != QLatin1String(mltFilter->get("mlt_service"))) {
                qDebug() << "ERROR: filter " << i << "differ: " << ct << ", " << currentFilter.get("mlt_service") << " = " << mltFilter->get("mlt_service");
                return false;
            }
            QVector<QPair<QString, QVariant>> params = allFilters[i]->getAllParameters();
            for (const auto &val : qAsConst(params)) {
                // Check parameters values
                if (val.second.toString() != QString(mltFilter->get(val.first.toUtf8().constData()))) {
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

std::shared_ptr<AssetParameterModel> EffectStackModel::getAssetModelById(const QString &effectId)
{
    QWriteLocker locker(&m_lock);
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        if (effectId == sourceEffect->getAssetId()) {
            return std::static_pointer_cast<AssetParameterModel>(sourceEffect);
        }
    }
    return nullptr;
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
    int ix = getActiveEffect();
    if (ix < 0 || ix >= rootItem->childCount()) {
        return nullptr;
    }
    std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
    std::shared_ptr<KeyframeModelList> listModel = sourceEffect->getKeyframeModel();
    if (listModel) {
        return listModel->getKeyModel();
    }
    return nullptr;
}

void EffectStackModel::replugEffect(const std::shared_ptr<AssetParameterModel> &asset)
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
        item->plant(m_masterService);
        for (const auto &service : m_childServices) {
            item->plantClone(service);
        }
    }
}

void EffectStackModel::cleanFadeEffects(bool outEffects, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    const auto &toDelete = outEffects ? m_fadeOuts : m_fadeIns;
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
                    m_fadeOuts.erase(id);
                } else {
                    m_fadeIns.erase(id);
                }
            }
            QVector<int> roles = {TimelineModel::EffectNamesRole};
            roles << (outEffects ? TimelineModel::FadeOutRole : TimelineModel::FadeInRole);
            Q_EMIT dataChanged(QModelIndex(), QModelIndex(), roles);
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

QStringList EffectStackModel::externalFiles() const
{
    QStringList urls;
    for (int i = 0; i < rootItem->childCount(); ++i) {
        auto filter = std::static_pointer_cast<EffectItemModel>(rootItem->child(i))->filter();
        QString url;
        if (filter.property_exists("av.file")) {
            url = filter.get("av.file");
        } else if (filter.property_exists("luma.resource")) {
            url = filter.get("luma.resource");
        } else if (filter.property_exists("resource")) {
            url = filter.get("resource");
        }
        if (!url.isEmpty()) {
            urls << url;
        }
    }
    urls.removeDuplicates();
    return urls;
}

bool EffectStackModel::isStackEnabled() const
{
    return m_effectStackEnabled;
}

bool EffectStackModel::addEffectKeyFrame(int frame, double normalisedVal)
{
    if (rootItem->childCount() == 0) return false;
    int ix = getActiveEffect();
    if (ix < 0) {
        return false;
    }
    std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
    std::shared_ptr<KeyframeModelList> listModel = sourceEffect->getKeyframeModel();
    if (m_ownerId.type == ObjectType::TimelineTrack) {
        sourceEffect->filter().set("out", pCore->getItemDuration(m_ownerId));
    }
    return listModel->addKeyframe(frame, normalisedVal);
}

bool EffectStackModel::removeKeyFrame(int frame)
{
    if (rootItem->childCount() == 0) return false;
    int ix = getActiveEffect();
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
    int ix = getActiveEffect();
    if (ix < 0) {
        return false;
    }
    std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
    std::shared_ptr<KeyframeModelList> listModel = sourceEffect->getKeyframeModel();
    if (m_ownerId.type == ObjectType::TimelineTrack) {
        sourceEffect->filter().set("out", pCore->getItemDuration(m_ownerId));
    }
    return listModel->updateKeyframe(GenTime(oldFrame, pCore->getCurrentFps()), GenTime(newFrame, pCore->getCurrentFps()), std::move(normalisedVal));
}

bool EffectStackModel::hasKeyFrame(int frame)
{
    if (rootItem->childCount() == 0) return false;
    int ix = getActiveEffect();
    if (ix < 0) {
        return false;
    }
    std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
    std::shared_ptr<KeyframeModelList> listModel = sourceEffect->getKeyframeModel();
    return listModel->hasKeyframe(frame);
}

bool EffectStackModel::hasEffect(const QString &assetId) const
{
    for (int i = 0; i < rootItem->childCount(); ++i) {
        if (std::static_pointer_cast<EffectItemModel>(rootItem->child(i))->getAssetId() == assetId) {
            return true;
        }
    }
    return false;
}

QVariantList EffectStackModel::getEffectZones() const
{
    QVariantList effectZones;
    for (int i = 0; i < rootItem->childCount(); ++i) {
        auto item = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        if (item->hasForcedInOut()) {
            QPair<int, int> z = item->getInOut();
            effectZones << QPoint(z.first, z.second);
        }
    }
    return effectZones;
}

void EffectStackModel::updateEffectZones()
{
    Q_EMIT dataChanged(QModelIndex(), QModelIndex(), {TimelineModel::EffectZonesRole});
    if (m_ownerId.type == ObjectType::Master) {
        Q_EMIT updateMasterZones();
    }
}

void EffectStackModel::passEffects(Mlt::Producer *producer, const QString &exception)
{
    auto ms = m_masterService.lock();
    int ct = ms->filter_count();
    for (int i = 0; i < ct; i++) {
        if (ms->filter(i)->get_int("internal_added") > 0 || !ms->filter(i)->property_exists("kdenlive_id")) {
            continue;
        }
        if (!exception.isEmpty() && QString(ms->filter(i)->get("mlt_service")) == exception) {
            continue;
        }
        auto *filter = new Mlt::Filter(*ms->filter(i));
        producer->attach(*filter);
    }
}
