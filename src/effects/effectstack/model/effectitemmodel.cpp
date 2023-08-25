/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "effectitemmodel.hpp"

#include "core.h"
#include "effects/effectsrepository.hpp"
#include "effectstackmodel.hpp"
#include <utility>

EffectItemModel::EffectItemModel(const QList<QVariant> &effectData, std::unique_ptr<Mlt::Properties> effect, const QDomElement &xml, const QString &effectId,
                                 const std::shared_ptr<AbstractTreeModel> &stack, bool isEnabled, QString originalDecimalPoint)
    : AbstractEffectItem(EffectItemType::Effect, effectData, stack, false, isEnabled)
    , AssetParameterModel(std::move(effect), xml, effectId, std::static_pointer_cast<EffectStackModel>(stack)->getOwnerId(), originalDecimalPoint)
    , m_childId(0)
{
    connect(this, &AssetParameterModel::updateChildren, [&](const QStringList &names) {
        if (m_childEffects.size() == 0) {
            return;
        }
        // qDebug() << "* * *SETTING EFFECT PARAM: " << name << " = " << m_asset->get(name.toUtf8().constData());
        QMapIterator<int, std::shared_ptr<EffectItemModel>> i(m_childEffects);
        while (i.hasNext()) {
            i.next();
            for (const QString &name : names) {
                i.value()->filter().set(name.toUtf8().constData(), m_asset->get(name.toUtf8().constData()));
            }
        }
    });
}

// static
std::shared_ptr<EffectItemModel> EffectItemModel::construct(const QString &effectId, std::shared_ptr<AbstractTreeModel> stack, bool effectEnabled)
{
    Q_ASSERT(EffectsRepository::get()->exists(effectId));
    QDomElement xml = EffectsRepository::get()->getXml(effectId);

    std::unique_ptr<Mlt::Properties> effect = EffectsRepository::get()->getEffect(effectId);
    effect->set("kdenlive_id", effectId.toUtf8().constData());

    QList<QVariant> data;
    data << EffectsRepository::get()->getName(effectId) << effectId;

    std::shared_ptr<EffectItemModel> self(new EffectItemModel(data, std::move(effect), xml, effectId, stack, effectEnabled));

    baseFinishConstruct(self);
    return self;
}

std::shared_ptr<EffectItemModel> EffectItemModel::construct(std::unique_ptr<Mlt::Properties> effect, std::shared_ptr<AbstractTreeModel> stack,
                                                            const QString &originalDecimalPoint)
{
    QString effectId = effect->get("kdenlive_id");
    if (effectId.isEmpty()) {
        effectId = effect->get("mlt_service");
    }
    Q_ASSERT(EffectsRepository::get()->exists(effectId));

    // Get the effect XML and add parameter values from the project file
    QDomElement xml = EffectsRepository::get()->getXml(effectId);
    QDomNodeList params = xml.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement currentParameter = params.item(i).toElement();
        QString paramName = currentParameter.attribute(QStringLiteral("name"));
        QString paramType = currentParameter.attribute(QStringLiteral("type"));
        if (paramType == QLatin1String("multiswitch")) {
            // multiswitch params have a composited param name, skip
            QStringList names = paramName.split(QLatin1Char('\n'));
            QStringList paramValues;
            for (const QString &n : qAsConst(names)) {
                paramValues << effect->get(n.toUtf8().constData());
            }
            currentParameter.setAttribute(QStringLiteral("value"), paramValues.join(QLatin1Char('\n')));
            continue;
        }
        QString paramValue = effect->get(paramName.toUtf8().constData());
        qDebug() << effectId << ": Setting parameter " << paramName << " to " << paramValue;
        currentParameter.setAttribute(QStringLiteral("value"), paramValue);
    }

    QList<QVariant> data;
    data << EffectsRepository::get()->getName(effectId) << effectId;

    bool disable = effect->get_int("disable") == 0;
    std::shared_ptr<EffectItemModel> self(new EffectItemModel(data, std::move(effect), xml, effectId, stack, disable, originalDecimalPoint));
    baseFinishConstruct(self);
    return self;
}

void EffectItemModel::plant(const std::weak_ptr<Mlt::Service> &service)
{
    if (auto ptr = service.lock()) {
        int ret = 0;
        if (isLink()) {
            Mlt::Chain fromChain(static_cast<Mlt::Producer *>(ptr.get())->parent());
            ret = fromChain.attach(getLink());
        } else {
            ret = ptr->attach(getFilter());
        }
        Q_ASSERT(ret == 0);
    } else {
        qDebug() << "Error : Cannot plant effect because parent service is not available anymore";
        Q_ASSERT(false);
    }
}

void EffectItemModel::loadClone(const std::weak_ptr<Mlt::Service> &service)
{
    if (auto ptr = service.lock()) {
        std::shared_ptr<EffectItemModel> effect = nullptr;
        for (int i = 0; i < ptr->filter_count(); i++) {
            std::unique_ptr<Mlt::Filter> filt(ptr->filter(i));
            QString effName = filt->get("kdenlive_id");
            if (effName == m_assetId && filt->get_int("_kdenlive_processed") == 0) {
                if (auto ptr2 = m_model.lock()) {
                    effect = EffectItemModel::construct(std::move(filt), ptr2, QString());
                    int childId = ptr->get_int("_childid");
                    if (childId == 0) {
                        childId = m_childId++;
                        ptr->set("_childid", childId);
                    }
                    m_childEffects.insert(childId, effect);
                }
                break;
            }
            filt->set("_kdenlive_processed", 1);
        }
        return;
    }
    qDebug() << "Error : Cannot plant effect because parent service is not available anymore";
    Q_ASSERT(false);
}

void EffectItemModel::plantClone(const std::weak_ptr<Mlt::Service> &service)
{
    if (auto ptr = service.lock()) {
        const QString effectId = getAssetId();
        std::shared_ptr<EffectItemModel> effect = nullptr;
        if (auto ptr2 = m_model.lock()) {
            effect = EffectItemModel::construct(effectId, ptr2);
            effect->setParameters(getAllParameters(), false);
            int childId = ptr->get_int("_childid");
            if (childId == 0) {
                childId = m_childId++;
                ptr->set("_childid", childId);
            }
            m_childEffects.insert(childId, effect);
            int ret = 0;
            if (isLink()) {
                Mlt::Chain fromChain(static_cast<Mlt::Producer *>(ptr.get())->parent());
                ret = fromChain.attach(effect->getLink());
            } else {
                ret = ptr->attach(effect->getFilter());
            }
            Q_ASSERT(ret == 0);
            return;
        }
    }
    qDebug() << "Error : Cannot plant effect because parent service is not available anymore";
    Q_ASSERT(false);
}

void EffectItemModel::unplant(const std::weak_ptr<Mlt::Service> &service)
{
    if (auto ptr = service.lock()) {
        int ret = 0;
        if (isLink()) {
            Mlt::Chain fromChain(static_cast<Mlt::Producer *>(ptr.get())->parent());
            ret = fromChain.detach(getLink());
        } else {
            ret = ptr->detach(getFilter());
        }
        Q_ASSERT(ret == 0);
    } else {
        qDebug() << "Error : Cannot plant effect because parent service is not available anymore";
        Q_ASSERT(false);
    }
}

void EffectItemModel::unplantClone(const std::weak_ptr<Mlt::Service> &service)
{
    if (m_childEffects.size() == 0) {
        return;
    }
    if (auto ptr = service.lock()) {
        int ret = 0;
        if (isLink()) {
            Mlt::Chain fromChain(static_cast<Mlt::Producer *>(ptr.get())->parent());
            ret = fromChain.detach(getLink());
        } else {
            ret = ptr->detach(getFilter());
        }
        Q_ASSERT(ret == 0);
        int childId = ptr->get_int("_childid");
        auto effect = m_childEffects.take(childId);
        if (effect && effect->isValid()) {
            if (effect->isLink()) {
                Mlt::Chain fromChain(static_cast<Mlt::Producer *>(ptr.get())->parent());
                ret = fromChain.detach(effect->getLink());
            } else {
                ret = ptr->detach(effect->getFilter());
            }
            effect.reset();
        }
    } else {
        qDebug() << "Error : Cannot plant effect because parent service is not available anymore";
        Q_ASSERT(false);
    }
}

Mlt::Properties &EffectItemModel::filter() const
{
    return *(m_asset.get());
}

Mlt::Filter &EffectItemModel::getFilter() const
{
    return *static_cast<Mlt::Filter *>(m_asset.get());
}

Mlt::Link &EffectItemModel::getLink() const
{
    return *static_cast<Mlt::Link *>(m_asset.get());
}

bool EffectItemModel::isLink() const
{
    return m_assetId.startsWith(QLatin1String("avfilter."));
}

bool EffectItemModel::isValid() const
{
    return m_asset && m_asset->is_valid();
}

void EffectItemModel::updateEnable(bool updateTimeline)
{
    filter().set("disable", isEnabled() ? 0 : 1);
    if (updateTimeline) {
        pCore->refreshProjectItem(m_ownerId);
        pCore->invalidateItem(m_ownerId);
    }
    const QModelIndex start = AssetParameterModel::index(0, 0);
    const QModelIndex end = AssetParameterModel::index(rowCount() - 1, 0);
    Q_EMIT dataChanged(start, end, QVector<int>());
    Q_EMIT enabledChange(!isEnabled());
    // Update timeline child producers
    Q_EMIT AssetParameterModel::updateChildren({QStringLiteral("disable")});
}

void EffectItemModel::setCollapsed(bool collapsed)
{
    filter().set("kdenlive:collapsed", collapsed ? 1 : 0);
}

bool EffectItemModel::isCollapsed() const
{
    return filter().get_int("kdenlive:collapsed") == 1;
}

void EffectItemModel::setKeyframesHidden(bool hidden)
{
    Fun undo = [this, hidden]() {
        filter().set("kdenlive:kfrhidden", hidden ? 0 : 1);
        Q_EMIT hideKeyframesChange(hidden ? false : true);
        return true;
    };
    Fun redo = [this, hidden]() {
        filter().set("kdenlive:kfrhidden", hidden ? 1 : 0);
        Q_EMIT hideKeyframesChange(hidden ? true : false);
        return true;
    };
    redo();
    pCore->pushUndo(undo, redo, hidden ? i18n("Hide keyframes") : i18n("Show keyframes"));
}

bool EffectItemModel::isKeyframesHidden() const
{
    return filter().get_int("kdenlive:kfrhidden") == 1;
}

bool EffectItemModel::keyframesHiddenUnset() const
{
    return filter().property_exists("kdenlive:kfrhidden") == false;
}

bool EffectItemModel::hasForcedInOut() const
{
    return filter().get_int("kdenlive:force_in_out") == 1;
}

bool EffectItemModel::isAudio() const
{
    return EffectsRepository::get()->isAudioEffect(m_assetId);
}

bool EffectItemModel::isUnique() const
{
    return EffectsRepository::get()->isUnique(m_assetId);
}

QPair<int, int> EffectItemModel::getInOut() const
{
    return {m_asset->get_int("in"), m_asset->get_int("out")};
}

void EffectItemModel::setInOut(const QString &effectName, QPair<int, int> bounds, bool enabled, bool withUndo)
{
    QPair<int, int> currentInOut = {m_asset->get_int("in"), m_asset->get_int("out")};
    int currentState = m_asset->get_int("kdenlive:force_in_out");
    Fun undo = [this, currentState, currentInOut]() {
        m_asset->set("kdenlive:force_in_out", currentState);
        m_asset->set("in", currentInOut.first);
        m_asset->set("out", currentInOut.second);
        Q_EMIT AssetParameterModel::updateChildren({QStringLiteral("in"), QStringLiteral("out")});
        if (!isAudio()) {
            pCore->refreshProjectItem(m_ownerId);
            pCore->invalidateItem(m_ownerId);
        }
        Q_EMIT showEffectZone(m_ownerId, currentInOut, currentState == 1);
        return true;
    };
    Fun redo = [this, enabled, bounds]() {
        m_asset->set("kdenlive:force_in_out", enabled ? 1 : 0);
        m_asset->set("in", bounds.first);
        m_asset->set("out", bounds.second);
        Q_EMIT AssetParameterModel::updateChildren({QStringLiteral("in"), QStringLiteral("out")});
        if (!isAudio()) {
            pCore->refreshProjectItem(m_ownerId);
            pCore->invalidateItem(m_ownerId);
        }
        Q_EMIT showEffectZone(m_ownerId, bounds, enabled);
        return true;
    };
    std::shared_ptr<KeyframeModelList> keyframes = getKeyframeModel();
    if (keyframes != nullptr) {
        // Effect has keyframes, update these
        const QModelIndex start = AssetParameterModel::index(0, 0);
        const QModelIndex end = AssetParameterModel::index(rowCount() - 1, 0);
        Fun refresh = [this, start, end]() {
            Q_EMIT dataChanged(start, end, QVector<int>());
            return true;
        };
        refresh();
        PUSH_LAMBDA(refresh, redo);
        PUSH_LAMBDA(refresh, undo);
    }
    redo();
    if (withUndo) {
        pCore->pushUndo(undo, redo, i18n("Update zone for %1", effectName));
    }
}
