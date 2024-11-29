/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "effectstackmodel.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/model/assetcommand.hpp"
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

void EffectStackModel::plugBuiltinEffects()
{
    if (KdenliveSettings::enableBuiltInEffects()) {
        auto avStack = pCore->assetHasAV(m_ownerId);
        if (avStack.first) {
            // Initialize built-in effects
            appendAudioBuildInEffects();
        }
        if (avStack.second) {
            // Initialize built-in effects
            appendVideoBuildInEffects();
        }
    }
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
    // Ensure child stack has the same effects count, otherwise re-sync
    if (auto ptr = service.lock()) {
        auto ms = m_masterService.lock();
        if (ptr->filter_count() != ms->filter_count()) {
            // Filters mismatch
            ms->unlock();
            // Remove all filters from service
            int ix = ptr->filter_count() - 1;
            while (ix >= 0) {
                std::unique_ptr<Mlt::Filter> filt(ptr->filter(ix));
                if (filt->property_exists("kdenlive_id")) {
                    ptr->detach(*filt.get());
                }
                ix--;
            }
            ptr->unlock();
            addService(service);
            return;
        }
    }
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
    if (ix < 0 || ix >= rootItem->childCount()) {
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
        QVector<int> roles = {TimelineModel::EffectNamesRole, TimelineModel::EffectCountRole};
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
        QVector<int> roles = {TimelineModel::EffectNamesRole, TimelineModel::EffectCountRole};
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
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    QString effectName;
    removeEffectWithUndo(effect, effectName, undo, redo);
    PUSH_UNDO(undo, redo, i18n("Delete effect %1", effectName));
}

void EffectStackModel::removeEffectWithUndo(const QString &assetId, QString &effectName, int assetRow, Fun &undo, Fun &redo)
{
    if (assetRow > -1 && assetRow < rootItem->childCount()) {
        std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(assetRow));
        if (assetId == sourceEffect->getAssetId()) {
            removeEffectWithUndo(sourceEffect, effectName, undo, redo);
            return;
        }
    }
    for (int i = 0; i < rootItem->childCount(); ++i) {
        std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        if (assetId == sourceEffect->getAssetId()) {
            removeEffectWithUndo(sourceEffect, effectName, undo, redo);
            break;
        }
    }
}

void EffectStackModel::removeEffectWithUndo(const std::shared_ptr<EffectItemModel> &effect, QString &effectName, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allItems.count(effect->getId()) > 0);
    int parentId = -1;
    if (auto ptr = effect->parentItem().lock()) parentId = ptr->getId();
    int current = getActiveEffect();
    if (current >= rootItem->childCount() - 1) {
        setActiveEffect(current - 1);
    }
    int currentRow = effect->row();
    Fun local_undo = addItem_lambda(effect, parentId);
    if (currentRow != rowCount() - 1) {
        Fun move = moveItem_lambda(effect->getId(), currentRow, true);
        PUSH_LAMBDA(move, undo);
    }
    Fun local_redo = removeItem_lambda(effect->getId());
    bool res = local_redo();
    if (res) {
        int inFades = int(m_fadeIns.size());
        int outFades = int(m_fadeOuts.size());
        m_fadeIns.erase(effect->getId());
        m_fadeOuts.erase(effect->getId());
        inFades = int(m_fadeIns.size()) - inFades;
        outFades = int(m_fadeOuts.size()) - outFades;
        effectName = EffectsRepository::get()->getName(effect->getAssetId());
        bool hasZone = effect->hasForcedInOut();
        Fun update = [this, inFades, outFades, hasZone]() {
            // Required to build the effect view
            if (rowCount() == 0) {
                // Stack is now empty
                Q_EMIT dataChanged(QModelIndex(), QModelIndex(), {});
            } else {
                QVector<int> roles = {TimelineModel::EffectNamesRole, TimelineModel::EffectCountRole};
                if (inFades < 0) {
                    roles << TimelineModel::FadeInRole;
                }
                if (outFades < 0) {
                    roles << TimelineModel::FadeOutRole;
                }
                Q_EMIT dataChanged(QModelIndex(), QModelIndex(), roles);
            }
            if (hasZone) {
                updateEffectZones();
            }
            pCore->updateItemKeyframes(m_ownerId);
            return true;
        };
        Fun update2 = [this, inFades, outFades, current, hasZone]() {
            // Required to build the effect view
            QVector<int> roles = {TimelineModel::EffectNamesRole, TimelineModel::EffectCountRole};
            // TODO: only update if effect is fade or keyframe
            if (inFades < 0) {
                roles << TimelineModel::FadeInRole;
            } else if (outFades < 0) {
                roles << TimelineModel::FadeOutRole;
            }
            Q_EMIT dataChanged(QModelIndex(), QModelIndex(), roles);
            if (hasZone) {
                updateEffectZones();
            }
            pCore->updateItemKeyframes(m_ownerId);
            setActiveEffect(current);
            return true;
        };
        update();
        PUSH_LAMBDA(update, local_redo);
        PUSH_LAMBDA(update2, local_undo);
        PUSH_LAMBDA(local_redo, redo);
        PUSH_LAMBDA(local_undo, undo);
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

bool EffectStackModel::copyXmlEffectWithUndo(const QDomElement &effect, Fun &undo, Fun &redo)
{
    bool result = fromXml(effect, undo, redo);
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
        QStringList passProps{QStringLiteral("disable"), QStringLiteral("kdenlive:collapsed"), QStringLiteral("kdenlive:builtin"),
                              QStringLiteral("kdenlive:hiddenbuiltin"), QStringLiteral("kdenlive:kfrhidden")};
        for (const QString &param : passProps) {
            int paramVal = sourceEffect->filter().get_int(param.toUtf8().constData());
            if (paramVal > 0) {
                Xml::setXmlProperty(sub, param, QString::number(paramVal));
            }
        }
        QVector<QPair<QString, QVariant>> params = sourceEffect->getAllParameters();
        for (const auto &param : std::as_const(params)) {
            Xml::setXmlProperty(sub, param.first, param.second.toString());
        }
        container.appendChild(sub);
    }
    return container;
}

QDomElement EffectStackModel::rowToXml(int row, QDomDocument &document)
{
    QDomElement container = document.createElement(QStringLiteral("effects"));
    if (row < 0 || row >= rootItem->childCount()) {
        return container;
    }
    int currentIn = pCore->getItemIn(m_ownerId);
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
    for (const auto &param : std::as_const(params)) {
        Xml::setXmlProperty(sub, param.first, param.second.toString());
    }
    container.appendChild(sub);
    return container;
}

bool EffectStackModel::fromXml(const QDomElement &effectsXml, Fun &undo, Fun &redo)
{
    QDomNodeList nodeList = effectsXml.elementsByTagName(QStringLiteral("effect"));
    int parentIn = effectsXml.attribute(QStringLiteral("parentIn")).toInt();
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
        if (m_ownerId.type == KdenliveObjectType::TimelineClip && EffectsRepository::get()->isUnique(effectId) && hasFilter(effectId)) {
            pCore->displayMessage(i18n("Effect %1 cannot be added twice.", EffectsRepository::get()->getName(effectId)), ErrorMessage);
            return false;
        }
        bool effectEnabled = true;
        if (Xml::hasXmlProperty(node, QLatin1String("disable"))) {
            effectEnabled = Xml::getXmlProperty(node, QLatin1String("disable")).toInt() != 1;
        }
        if (!effectEnabled && !KdenliveSettings::enableBuiltInEffects() && Xml::getXmlProperty(node, QLatin1String("kdenlive:builtin")).toInt() == 1) {
            continue;
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
        if (Xml::hasXmlProperty(node, QLatin1String("kdenlive:builtin"))) {
            effect->setBuiltIn();
            if (Xml::hasXmlProperty(node, QLatin1String("kdenlive:hiddenbuiltin"))) {
                effect->filter().set("kdenlive:hiddenbuiltin", Xml::getXmlProperty(node, QLatin1String("kdenlive:hiddenbuiltin")).toInt());
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
                const QString keyframeString = Xml::getXmlProperty(node, QLatin1String("level\nalpha"));
                const QChar mod = AssetParameterModel::getKeyframeType(keyframeString);
                if (effect->filter().get("alpha") == QLatin1String("1")) {
                    // Adjust level value to match filter end
                    if (!mod.isNull()) {
                        QString val = QStringLiteral("0%1=0;-1%1=1").arg(mod);
                        effect->filter().set("level", val.toUtf8().constData());
                    } else {
                        effect->filter().set("level", "0=0;-1=1");
                    }
                } else if (effect->filter().get("level") == QLatin1String("1")) {
                    if (!mod.isNull()) {
                        QString val = QStringLiteral("0%1=0;-1%1=1").arg(mod);
                        effect->filter().set("alpha", val.toUtf8().constData());
                    } else {
                        effect->filter().set("alpha", "0=0;-1=1");
                    }
                }
            }
        } else if (effectId.startsWith(QLatin1String("fadeout")) || effectId.startsWith(QLatin1String("fade_to_"))) {
            m_fadeOuts.insert(effect->getId());
            int duration = effect->filter().get_length() - 1;
            int filterOut = pCore->getItemIn(m_ownerId) + pCore->getItemDuration(m_ownerId) - 1;
            effect->filter().set("in", filterOut - duration);
            effect->filter().set("out", filterOut);
            if (effectId.startsWith(QLatin1String("fade_"))) {
                const QString keyframeString = Xml::getXmlProperty(node, QLatin1String("level\nalpha"));
                const QChar mod = AssetParameterModel::getKeyframeType(keyframeString);
                if (effect->filter().get("alpha") == QLatin1String("1")) {
                    // Adjust level value to match filter end
                    if (!mod.isNull()) {
                        QString val = QStringLiteral("0%1=1;-1%1=0").arg(mod);
                        effect->filter().set("level", val.toUtf8().constData());
                    } else {
                        effect->filter().set("level", "0=1;-1=0");
                    }
                } else if (effect->filter().get("level") == QLatin1String("1")) {
                    if (!mod.isNull()) {
                        QString val = QStringLiteral("0%1=1;-1%1=0").arg(mod);
                        effect->filter().set("alpha", val.toUtf8().constData());
                    } else {
                        effect->filter().set("alpha", "0=1;-1=0");
                    }
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

bool EffectStackModel::copyEffectWithUndo(const std::shared_ptr<AbstractEffectItem> &sourceItem, PlaylistState::ClipState state, Fun &undo, Fun &redo)
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
    if (m_ownerId.type == KdenliveObjectType::TimelineClip && EffectsRepository::get()->isUnique(effectId) && hasFilter(effectId)) {
        pCore->displayMessage(i18n("Effect %1 cannot be added twice.", EffectsRepository::get()->getName(effectId)), ErrorMessage);
        return false;
    }
    bool enabled = sourceEffect->isAssetEnabled();
    auto effect = EffectItemModel::construct(effectId, shared_from_this(), enabled);
    effect->setParameters(sourceEffect->getAllParameters());
    if (sourceEffect->isBuiltIn()) {
        effect->setBuiltIn();
    }
    if (sourceEffect->filter().property_exists("kdenlive:kfrhidden")) {
        effect->filter().set("kdenlive:kfrhidden", sourceEffect->filter().get_int("kdenlive:kfrhidden"));
    }
    if (sourceEffect->filter().property_exists("kdenlive:collapsed")) {
        effect->filter().set("kdenlive:collapsed", sourceEffect->filter().get_int("kdenlive:collapsed"));
    }
    if (sourceEffect->filter().property_exists("kdenlive:builtin")) {
        int builtin = sourceEffect->filter().get_int("kdenlive:builtin");
        if (builtin == 1 && !enabled && !KdenliveSettings::enableBuiltInEffects()) {
            // Ignore disabled builtin effects
            return false;
        }
        effect->setBuiltIn();
        if (sourceEffect->filter().property_exists("kdenlive:hiddenbuiltin")) {
            effect->filter().set("kdenlive:hiddenbuiltin", sourceEffect->filter().get_int("kdenlive:hiddenbuiltin"));
        }
    }
    if (!enabled) {
        effect->filter().set("disable", 1);
    }
    if (m_ownerId.type == KdenliveObjectType::TimelineTrack || m_ownerId.type == KdenliveObjectType::Master) {
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
    QVector<int> roles = {TimelineModel::EffectNamesRole, TimelineModel::EffectCountRole};
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
        PUSH_LAMBDA(update, local_redo);
        PUSH_LAMBDA(update, local_undo);
        PUSH_LAMBDA(local_redo, redo);
        PUSH_LAMBDA(local_undo, undo);
    }
    return res;
}

bool EffectStackModel::copyEffect(const std::shared_ptr<AbstractEffectItem> &sourceItem, PlaylistState::ClipState state, bool logUndo)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = copyEffectWithUndo(sourceItem, state, undo, redo);
    if (res && logUndo) {
        pCore->pushUndo(undo, redo, i18n("Paste effect"));
    }
    return res;
}

std::pair<bool, bool> EffectStackModel::appendEffectWithUndo(const QString &effectId, Fun &undo, Fun &redo)
{
    return doAppendEffect(effectId, true, {}, undo, redo);
}

bool EffectStackModel::appendEffect(const QString &effectId, bool makeCurrent, stringMap params)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = doAppendEffect(effectId, makeCurrent, params, undo, redo).first;
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Add effect %1", EffectsRepository::get()->getName(effectId)));
    }
    return result;
}

std::pair<bool, bool> EffectStackModel::doAppendEffect(const QString &effectId, bool makeCurrent, stringMap params, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if ((m_ownerId.type == KdenliveObjectType::TimelineClip || m_ownerId.type == KdenliveObjectType::TimelineTrack) && pCore->window() &&
        pCore->window()->effectIsMasterOnly(effectId)) {
        pCore->displayMessage(i18n("Effect %1 can only be added to master", EffectsRepository::get()->getName(effectId)), ErrorMessage);
        return {false, true};
    }
    if (m_ownerId.type == KdenliveObjectType::TimelineClip && EffectsRepository::get()->isUnique(effectId) && hasFilter(effectId)) {
        pCore->displayMessage(i18n("Effect %1 cannot be added twice.", EffectsRepository::get()->getName(effectId)), ErrorMessage);
        return {false, true};
    }
    std::unordered_set<int> previousFadeIn = m_fadeIns;
    std::unordered_set<int> previousFadeOut = m_fadeOuts;
    if (EffectsRepository::get()->isGroup(effectId)) {
        QDomElement doc = EffectsRepository::get()->getXml(effectId);
        return {copyXmlEffect(doc), false};
    }
    auto effect = EffectItemModel::construct(effectId, shared_from_this());
    PlaylistState::ClipState state = pCore->getItemState(m_ownerId);
    if (state == PlaylistState::VideoOnly) {
        if (effect->isAudio()) {
            // Cannot add effect to this clip
            pCore->displayMessage(i18n("Cannot add effect to clip"), ErrorMessage);
            return {false, true};
        }
    } else if (state == PlaylistState::AudioOnly) {
        if (!effect->isAudio()) {
            // Cannot add effect to this clip
            pCore->displayMessage(i18n("Cannot add effect to clip"), ErrorMessage);
            return {false, true};
        }
    }
    QMapIterator<QString, QString> i(params);
    while (i.hasNext()) {
        i.next();
        effect->filter().set(i.key().toUtf8().constData(), i.value().toUtf8().constData());
    }
    if (params.keys().contains(QLatin1String("kdenlive:builtin"))) {
        effect->setBuiltIn();
    }
    Fun local_undo = removeItem_lambda(effect->getId());
    // TODO the parent should probably not always be the root
    Fun local_redo = addItem_lambda(effect, rootItem->getId());
    effect->prepareKeyframes();
    connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
    connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
    connect(effect.get(), &AssetParameterModel::showEffectZone, this, &EffectStackModel::updateEffectZones);
    int currentActive = getActiveEffect();
    bool res = local_redo();
    if (makeCurrent || currentActive == -1) {
        setActiveEffect(rowCount() - 1);
    }
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
        } else if (m_ownerId.type == KdenliveObjectType::TimelineTrack) {
            effect->filter().set("out", pCore->getItemDuration(m_ownerId));
        }
        Fun update = [this, inFades, outFades]() {
            // TODO: only update if effect is fade or keyframe
            QVector<int> roles = {TimelineModel::EffectNamesRole, TimelineModel::EffectCountRole};
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
            QVector<int> roles = {TimelineModel::EffectNamesRole, TimelineModel::EffectCountRole};
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
        if (!effect->isBuiltIn()) {
            update();
            PUSH_LAMBDA(update, local_redo);
            PUSH_LAMBDA(update_undo, local_undo);
        }
        PUSH_LAMBDA(local_redo, redo);
        PUSH_LAMBDA(local_undo, undo);
    } else if (makeCurrent) {
        setActiveEffect(currentActive);
    }
    return {res, false};
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
            bool hasZone = effect->filter().get_int("kdenlive:force_in_out") == 1 && effect->filter().get_int("out");
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
            if (m_ownerId.type == KdenliveObjectType::TimelineTrack && !hasZone) {
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
            } else if (m_ownerId.type == KdenliveObjectType::TimelineClip && effect->data(QModelIndex(), AssetParameterModel::RequiresInOut).toBool() == true) {
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
                    const QString current = effect->filter().get("level");
                    const QChar mod = AssetParameterModel::getKeyframeType(current);
                    if (!mod.isNull()) {
                        const QString val = QStringLiteral("0%1=0;-1%1=1").arg(mod);
                        effect->filter().set("level", val.toUtf8().constData());
                    } else {
                        effect->filter().set("level", "0=0;-1=1");
                    }
                } else if (effect->filter().get("level") == QLatin1String("1")) {
                    // Adjust level value to match filter end
                    const QString current = effect->filter().get("alpha");
                    const QChar mod = AssetParameterModel::getKeyframeType(current);
                    if (!mod.isNull()) {
                        const QString val = QStringLiteral("0%1=0;-1%1=1").arg(mod);
                        effect->filter().set("alpha", val.toUtf8().constData());
                    } else {
                        effect->filter().set("alpha", "0=0;-1=1");
                    }
                }
            }
        }
        if (!indexes.isEmpty()) {
            Q_EMIT dataChanged(indexes.first(), indexes.last(), QVector<int>());
            pCore->updateItemModel(m_ownerId, QStringLiteral("fadein"), QStringLiteral("out"));
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
                    const QString current = effect->filter().get("level");
                    const QChar mod = AssetParameterModel::getKeyframeType(current);
                    if (!mod.isNull()) {
                        const QString val = QStringLiteral("0%1=1;-1%1=0").arg(mod);
                        effect->filter().set("level", val.toUtf8().constData());
                    } else {
                        effect->filter().set("level", "0=1;-1=0");
                    }
                } else if (effect->filter().get("level") == QLatin1String("1")) {
                    const QString current = effect->filter().get("alpha");
                    const QChar mod = AssetParameterModel::getKeyframeType(current);
                    if (!mod.isNull()) {
                        const QString val = QStringLiteral("0%1=1;-1%1=0").arg(mod);
                        effect->filter().set("alpha", val.toUtf8().constData());
                    } else {
                        effect->filter().set("alpha", "0=1;-1=0");
                    }
                }
            }
        }
        if (!indexes.isEmpty()) {
            Q_EMIT dataChanged(indexes.first(), indexes.last(), QVector<int>());
            pCore->updateItemModel(m_ownerId, QStringLiteral("fadeout"), QStringLiteral("in"));
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

int EffectStackModel::keyframeTypeFromSeparator(const QChar mod)
{
    if (mod == '~') {
        return int(KeyframeType::CurveSmooth);
    }
    if (mod == 'g') {
        return int(KeyframeType::CubicIn);
    }
    if (mod == 'h') {
        return int(KeyframeType::CubicOut);
    }
    if (mod == 'p') {
        return int(KeyframeType::ExponentialIn);
    }
    if (mod == 'q') {
        return int(KeyframeType::ExponentialOut);
    }
    if (mod == 's') {
        return int(KeyframeType::CircularIn);
    }
    if (mod == 'y') {
        return int(KeyframeType::ElasticIn);
    }
    if (mod == 'B') {
        return int(KeyframeType::BounceIn);
    }
    return 0;
}

int EffectStackModel::getFadeMethod(bool fromStart)
{
    QWriteLocker locker(&m_lock);
    if (fromStart) {
        if (m_fadeIns.empty()) {
            return 0;
        }
        for (int i = 0; i < rootItem->childCount(); ++i) {
            if (*(m_fadeIns.begin()) == std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                QString fadeData = effect->filter().get("alpha");
                if (!fadeData.contains(QLatin1Char('='))) {
                    fadeData = effect->filter().get("level");
                    if (!fadeData.contains(QLatin1Char('='))) {
                        return 0;
                    }
                }
                QChar mod = fadeData.section(QLatin1Char('='), 0, -2).back();
                qDebug() << "RRRR GOT FADE METHOD: " << fadeData << "\nMOD: " << mod;
                if (!mod.isDigit()) {
                    return keyframeTypeFromSeparator(mod);
                }
                return 0;
            }
        }
    } else {
        if (m_fadeOuts.empty()) {
            return 0;
        }
        for (int i = 0; i < rootItem->childCount(); ++i) {
            if (*(m_fadeOuts.begin()) == std::static_pointer_cast<TreeItem>(rootItem->child(i))->getId()) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                QString fadeData = effect->filter().get("alpha");
                if (!fadeData.contains(QLatin1Char('='))) {
                    fadeData = effect->filter().get("level");
                    if (!fadeData.contains(QLatin1Char('='))) {
                        return 0;
                    }
                }
                QChar mod = fadeData.section(QLatin1Char('='), 0, -2).back();
                if (!mod.isDigit()) {
                    return keyframeTypeFromSeparator(mod);
                }
                return 0;
            }
        }
    }
    return 0;
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

void EffectStackModel::moveEffectByRow(int destRow, int srcRow)
{
    moveEffect(destRow, getEffectStackRow(srcRow));
}

void EffectStackModel::moveEffect(int destRow, const std::shared_ptr<AbstractEffectItem> &item)
{
    QWriteLocker locker(&m_lock);
    int itemId = item->getId();
    Q_ASSERT(m_allItems.count(itemId) > 0);
    int oldRow = item->row();
    Fun redo = moveItem_lambda(itemId, destRow);
    bool res = redo();
    if (res) {
        Fun undo = moveItem_lambda(itemId, oldRow);
        Fun update_redo = [this, row = destRow > oldRow ? destRow - 1 : destRow]() {
            setActiveEffect(row);
            Q_EMIT this->dataChanged(QModelIndex(), QModelIndex(), {TimelineModel::EffectNamesRole, TimelineModel::EffectCountRole});
            return true;
        };
        Fun update_undo = [this, oldRow]() {
            setActiveEffect(oldRow);
            Q_EMIT this->dataChanged(QModelIndex(), QModelIndex(), {TimelineModel::EffectNamesRole, TimelineModel::EffectCountRole});
            return true;
        };
        update_redo();
        PUSH_LAMBDA(update_redo, redo);
        PUSH_LAMBDA(update_undo, undo);
        auto effectId = std::static_pointer_cast<EffectItemModel>(item)->getAssetId();
        PUSH_UNDO(undo, redo, i18n("Move effect %1", EffectsRepository::get()->getName(effectId)));
    }
}

bool EffectStackModel::moveEffectWithUndo(int destRow, const std::shared_ptr<AbstractEffectItem> &item, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    int itemId = item->getId();
    Q_ASSERT(m_allItems.count(itemId) > 0);
    int oldRow = item->row();
    Fun local_redo = moveItem_lambda(itemId, destRow);
    bool res = local_redo();
    if (res) {
        Fun local_undo = moveItem_lambda(itemId, oldRow);
        Fun update_redo = [this, row = destRow > oldRow ? destRow - 1 : destRow]() {
            setActiveEffect(row);
            Q_EMIT this->dataChanged(QModelIndex(), QModelIndex(), {TimelineModel::EffectNamesRole, TimelineModel::EffectCountRole});
            return true;
        };
        Fun update_undo = [this, oldRow]() {
            setActiveEffect(oldRow);
            Q_EMIT this->dataChanged(QModelIndex(), QModelIndex(), {TimelineModel::EffectNamesRole, TimelineModel::EffectCountRole});
            return true;
        };
        update_redo();
        PUSH_LAMBDA(update_redo, local_redo);
        PUSH_LAMBDA(update_undo, local_undo);
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    }
    return res;
}

void EffectStackModel::registerItem(const std::shared_ptr<TreeItem> &item)
{
    QWriteLocker locker(&m_lock);
    if (!item->isRoot()) {
        auto effectItem = std::static_pointer_cast<EffectItemModel>(item);
        // qDebug() << "$$$$$$$$$$$$$$$$$$$$$ Planting effect: " << effectItem->getAssetId();
        if (effectItem->data(QModelIndex(), AssetParameterModel::RequiresInOut).toBool() == true) {
            int in = pCore->getItemIn(m_ownerId);
            int out = in + pCore->getItemDuration(m_ownerId) - 1;
            effectItem->filter().set_in_and_out(in, out);
        }
        if (!m_loadingExisting) {
            // qDebug() << "$$$$$$$$$$$$$$$$$$$$$ Planting effect in " << m_childServices.size();
            effectItem->plant(m_masterService);
            // Check if we have an internal effect that needs to stay on top
            if (m_ownerId.type == KdenliveObjectType::Master || m_ownerId.type == KdenliveObjectType::TimelineTrack) {
                // check for subtitle effect
                auto ms = m_masterService.lock();
                int ct = ms->filter_count();
                QVector<int> ixToMove;
                for (int i = 0; i < ct; i++) {
                    std::shared_ptr<Mlt::Filter> ft(ms->filter(i));
                    if (ft->get_int("internal_added") > 0) {
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
            int target = -1;
            if (effectItem->isBuiltIn()) {
                // Ensure builtin effects are always applied before the user effects
                int max = rootItem->childCount();
                auto ms = m_masterService.lock();
                int currentEffectPos = ms->filter_count() - 1;
                int firstEffectPos = ms->filter_count() - max;
                if (max > 1 && firstEffectPos >= 0) {
                    // Find first kdenlive effect
                    std::shared_ptr<Mlt::Filter> ft(ms->filter(firstEffectPos));
                    const QString firstId = ft->get("mlt_service");
                    target = firstEffectPos;
                    // Flip effect should always come before others
                    if (firstId.contains(QLatin1String("flip"))) {
                        target++;
                    }
                    if (currentEffectPos != target) {
                        ms->move_filter(currentEffectPos, target);
                        if (!effectItem->isHiddenBuiltIn()) {
                            // Get position in Kdenlive's effect stack
                            target -= firstEffectPos;
                            rootItem->moveChild(target, effectItem);
                        }
                    }
                }
            }
            for (const auto &service : m_childServices) {
                // qDebug() << "$$$$$$$$$$$$$$$$$$$$$ Planting CLONE effect in " << (void *)service.lock().get();
                effectItem->plantClone(service, target);
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
    int builtIn = 0;
    for (int i = 0; i < sourceStack->rowCount(); i++) {
        auto item = sourceStack->getEffectStackRow(i);
        // No undo. this should only be used on project opening
        if (copyEffect(item, state, false)) {
            found = true;
            if (item->isAssetEnabled()) {
                effectEnabled = true;
            } else {
                std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(item);
                if (sourceEffect->isBuiltIn()) {
                    builtIn++;
                }
            }
            imported++;
        }
    }
    if (!effectEnabled && imported == builtIn) {
        effectEnabled = true;
    }
    m_effectStackEnabled = effectEnabled;
    if (!m_effectStackEnabled) {
        // Mark all effects as disabled by stack
        for (int i = 0; i < rootItem->childCount(); ++i) {
            std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
            sourceEffect->setEffectStackEnabled(false);
            sourceEffect->setAssetEnabled(true);
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
    bool effectEnabled = false;
    bool disabledStack = true;
    int imported = 0;
    int builtin = 0;
    if (auto ptr = service.lock()) {
        int max = ptr->filter_count();
        for (int i = 0; i < max; i++) {
            std::unique_ptr<Mlt::Filter> filter(ptr->filter(i));
            if (filter->get_int("internal_added") > 0 && m_ownerId.type != KdenliveObjectType::TimelineTrack) {
                // Required to load master audio effects
                if (m_ownerId.type == KdenliveObjectType::Master && filter->get("mlt_service") == QLatin1String("avfilter.subtitles")) {
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
            effectEnabled = filter->get_int("disable") == 0;
            bool forcedInOut = filter->get_int("kdenlive:force_in_out") == 1 && filter->get_int("out") > 0;
            if (disabledStack && filter->get_int("kdenlive:bin-disabled") == 0) {
                disabledStack = false;
            }
            if (!effectEnabled && filter->get_int("kdenlive::builtin") == 1) {
                if (!KdenliveSettings::enableBuiltInEffects()) {
                    // don't consider disabled builtin effects
                    continue;
                }
                builtin++;
            }
            const QString effectId = qstrdup(filter->get("kdenlive_id"));
            if (m_ownerId.type == KdenliveObjectType::TimelineClip && EffectsRepository::get()->isUnique(effectId) && hasFilter(effectId)) {
                pCore->displayMessage(i18n("Effect %1 cannot be added twice.", EffectsRepository::get()->getName(effectId)), ErrorMessage);
                continue;
            }
            // The MLT filter already exists, use it directly to create the effect
            int filterIn = filter->get_in();
            int filterOut = filter->get_out();
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
                if (forcedInOut) {
                    effect->filter().set_in_and_out(filterIn, filterOut);
                } else if (effectId.startsWith(QLatin1String("fadein")) || effectId.startsWith(QLatin1String("fade_from_"))) {
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
        if (imported == builtin) {
            effectEnabled = true;
        }
    }
    if (disabledStack && (imported + builtin > 0)) {
        m_effectStackEnabled = false;
    } else {
        m_effectStackEnabled = true;
    }
    if (!m_effectStackEnabled) {
        // Mark all effects as disabled by stack
        for (int i = 0; i < rootItem->childCount(); ++i) {
            std::shared_ptr<EffectItemModel> sourceEffect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
            sourceEffect->setEffectStackEnabled(false);
            sourceEffect->setAssetEnabled(true);
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
            pCore->updateItemKeyframes(m_ownerId);
            locker.unlock();
            Q_EMIT currentChanged(getIndexFromItem(effect), false);
        }
    }
    // Activate new effect
    if (ix > -1 && ix < rootItem->childCount()) {
        std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(ix));
        if (effect) {
            effect->setActive(true);
            pCore->updateItemKeyframes(m_ownerId);
            locker.unlock();
            Q_EMIT currentChanged(getIndexFromItem(effect), true);
        }
    }
}

int EffectStackModel::getActiveEffect() const
{
    QWriteLocker locker(&m_lock);
    if (auto ptr = m_masterService.lock()) {
        if (ptr->property_exists("kdenlive:activeeffect")) {
            return ptr->get_int("kdenlive:activeeffect");
        }
        if (rootItem->childCount() > 0) {
            return 0;
        }
        return -1;
    }
    return -1;
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
            for (const auto &val : std::as_const(params)) {
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
            QVector<int> roles = {TimelineModel::EffectNamesRole, TimelineModel::EffectCountRole};
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
        const auto effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        if (!effect->isAssetEnabled() && effect->isBuiltIn()) {
            continue;
        }
        effects.append(EffectsRepository::get()->getName(effect->getAssetId()));
    }
    return effects.join(QLatin1Char('/'));
}

bool EffectStackModel::hasEffects() const
{
    for (int i = 0; i < rootItem->childCount(); ++i) {
        const auto effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        if (!effect->isAssetEnabled() && effect->isBuiltIn()) {
            continue;
        }
        return true;
    }
    return false;
}

bool EffectStackModel::hasBuiltInEffect(const QString effectId) const
{
    for (int i = 0; i < rootItem->childCount(); ++i) {
        const auto effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        if (effect->isBuiltIn() && effect->isAssetEnabled() && effect->getAssetId() == effectId) {
            return true;
        }
    }
    return false;
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
    if (m_ownerId.type == KdenliveObjectType::TimelineTrack) {
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
    if (m_ownerId.type == KdenliveObjectType::TimelineTrack) {
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

int EffectStackModel::effectRow(const QString &assetId, int eid, bool enabledOnly) const
{
    for (int i = 0; i < rootItem->childCount(); ++i) {
        if (eid > -1) {
            if (rootItem->child(i)->getId() == eid) {
                return i;
            }
        } else {
            auto effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
            if (effect->getAssetId() == assetId) {
                if (!enabledOnly || effect->isAssetEnabled()) {
                    return i;
                }
            }
        }
    }
    return -1;
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
    if (m_ownerId.type == KdenliveObjectType::Master) {
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

void EffectStackModel::applyAssetCommand(int row, const QModelIndex &index, const QString &previousValue, QString value, QUndoCommand *command)
{

    auto item = getEffectStackRow(row);
    if (!item || item->childCount() > 0) {
        // group, error
        return;
    }
    std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
    const std::shared_ptr<AssetParameterModel> effectParamModel = std::static_pointer_cast<AssetParameterModel>(eff);
    if (KdenliveSettings::applyEffectParamsToGroupWithSameValue()) {
        const QString currentValue = effectParamModel->data(index, AssetParameterModel::ValueRole).toString();
        if (previousValue != currentValue) {
            // Dont't apply change on this effect, the start value is not the same
            return;
        }
    }
    new AssetCommand(effectParamModel, index, value, command);
}

void EffectStackModel::applyAssetKeyframeCommand(int row, const QModelIndex &index, GenTime pos, const QVariant &previousValue, QVariant value, int ix,
                                                 QUndoCommand *command)
{
    auto item = getEffectStackRow(row);
    if (!item || item->childCount() > 0) {
        // group, error
        return;
    }
    std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
    const std::shared_ptr<AssetParameterModel> effectParamModel = std::static_pointer_cast<AssetParameterModel>(eff);
    if (KdenliveSettings::applyEffectParamsToGroupWithSameValue()) {
        if (!effectParamModel->getKeyframeModel()->hasKeyframe(pos.frames(pCore->getCurrentFps()))) {
            return;
        }
        const QVariant currentValue = effectParamModel->getKeyframeModel()->getKeyModel(index)->getInterpolatedValue(pos);
        switch (ix) {
        case -1:
            if (previousValue != currentValue) {
                // Dont't apply change on this effect, the start value is not the same
                return;
            }
            break;
        case 0: {
            QStringList vals = currentValue.toString().split(QLatin1Char(' '));
            if (!vals.isEmpty() && previousValue.toString().section(QLatin1Char(' '), 0, 0) == vals.at(0)) {
                vals[0] = value.toString().section(QLatin1Char(' '), 0, 0);
                value = QVariant(vals.join(QLatin1Char(' ')));
            } else {
                return;
            }
            break;
        }
        default: {
            QStringList vals = currentValue.toString().split(QLatin1Char(' '));
            if (vals.size() > ix && previousValue.toString().section(QLatin1Char(' '), ix, ix) == vals.at(ix)) {
                vals[ix] = value.toString().section(QLatin1Char(' '), ix, ix);
                value = QVariant(vals.join(QLatin1Char(' ')));
            } else {
                return;
            }
            break;
        }
        }
    }
    new AssetKeyframeCommand(effectParamModel, index, value, pos, command);
}

void EffectStackModel::applyAssetMultiKeyframeCommand(int row, const QList<QModelIndex> &indexes, GenTime pos, const QStringList &sourceValues,
                                                      const QStringList &values, QUndoCommand *command)
{
    auto item = getEffectStackRow(row);
    if (!item || item->childCount() > 0) {
        // group, error
        return;
    }
    std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
    const std::shared_ptr<AssetParameterModel> effectParamModel = std::static_pointer_cast<AssetParameterModel>(eff);
    if (KdenliveSettings::applyEffectParamsToGroupWithSameValue()) {
        QStringList currentValue;
        for (auto &ix : indexes) {
            int r = ix.row();
            QModelIndex mappedIx = effectParamModel->getKeyframeModel()->getIndexAtRow(r);
            currentValue << effectParamModel->getKeyframeModel()->getKeyModel(mappedIx)->getInterpolatedValue(pos).toString();
        }
        if (sourceValues.count() != currentValue.count()) {
            return;
        }
        // The multikeyframe method is used by the lift/gamma/gain filter. It passes double values for r/g/b.
        // Due to conversion, there may be some slight differences in values, so compare rounded values
        for (int ix = 0; ix < sourceValues.count(); ix++) {
            if (int(sourceValues.at(ix).toDouble() * 10000) != int(currentValue.at(ix).toDouble() * 10000)) {
                return;
            }
        }
    }
    new AssetMultiKeyframeCommand(effectParamModel, indexes, sourceValues, values, pos, command);
}

void EffectStackModel::appendAudioBuildInEffects()
{
    // Check if an effect has not been added yet
    if (rootItem->childCount() > 0) {
        for (int i = 0; i < rootItem->childCount(); i++) {
            std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
            if (effect->isBuiltIn() && effect->getAssetId() == QLatin1String("volume")) {
                // Already added, abort
                return;
            }
        }
    }
    QWriteLocker lock(&m_lock);
    auto effect = EffectItemModel::construct(QStringLiteral("volume"), shared_from_this(), false);
    effect->filter().set("disable", 1);
    effect->filter().set("kdenlive:kfrhidden", 1);
    effect->setBuiltIn();
    effect->prepareKeyframes();
    connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
    connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
    connect(effect.get(), &AssetParameterModel::showEffectZone, this, &EffectStackModel::updateEffectZones);
    Fun local_undo = addItem_lambda(effect, rootItem->getId());
    local_undo();
}

void EffectStackModel::appendVideoBuildInEffects()
{
    if (m_ownerId.type == KdenliveObjectType::TimelineClip || m_ownerId.type == KdenliveObjectType::BinClip) {
        if (rootItem->childCount() > 0) {
            for (int i = 0; i < rootItem->childCount(); i++) {
                std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
                if (effect->isBuiltIn() && effect->getAssetId() == QLatin1String("qtblend")) {
                    // Already added, abort
                    return;
                }
            }
        }
        qDebug() << "=======APPENDING VIDEO EFFECTS!!!!";
        QWriteLocker locker(&m_lock);
        std::shared_ptr<EffectItemModel> effect = EffectItemModel::construct(QStringLiteral("qtblend"), shared_from_this(), false);
        effect->prepareKeyframes();
        effect->filter().set("disable", 1);
        // effect->filter().set("kdenlive:kfrhidden", 1);
        effect->filter().set("kdenlive:collapsed", 1);
        effect->setBuiltIn();
        connect(effect.get(), &AssetParameterModel::modelChanged, this, &EffectStackModel::modelChanged);
        connect(effect.get(), &AssetParameterModel::replugEffect, this, &EffectStackModel::replugEffect, Qt::DirectConnection);
        connect(effect.get(), &AssetParameterModel::showEffectZone, this, &EffectStackModel::updateEffectZones);
        Fun local_redo = addItem_lambda(effect, rootItem->getId());
        local_redo();
    }
}

void EffectStackModel::setBuildInSize(const QSize size)
{
    qDebug() << "/// ADJUSTING EFFECT SIZE TO: " << size;
    plugBuiltinEffects();
    if (rootItem->childCount() == 0) {
        // No built-in effects, abort
        return;
    }
    for (int i = 0; i < rootItem->childCount(); i++) {
        std::shared_ptr<EffectItemModel> effect = std::static_pointer_cast<EffectItemModel>(rootItem->child(i));
        if (effect->isBuiltIn() && effect->getAssetId() == QLatin1String("qtblend")) {
            effect->setAssetEnabled(true, true);
            effect->setParameter(QStringLiteral("disable"), QStringLiteral("0"));
            effect->setParameter(QStringLiteral("rect"), QStringLiteral("0=0 0 %1 %2 1").arg(size.width()).arg(size.height()), true);
            break;
        }
    }
}
