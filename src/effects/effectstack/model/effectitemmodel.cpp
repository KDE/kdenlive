/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "effectitemmodel.hpp"

#include "core.h"
#include "effects/effectsrepository.hpp"
#include "effectstackmodel.hpp"
#include <utility>

EffectItemModel::EffectItemModel(const QList<QVariant> &effectData, std::unique_ptr<Mlt::Service> effect, const QDomElement &xml, const QString &effectId,
                                 const std::shared_ptr<AbstractTreeModel> &stack, bool isEnabled, QString originalDecimalPoint)
    : AbstractEffectItem(EffectItemType::Effect, effectData, stack, false, isEnabled)
    , AssetParameterModel(std::move(effect), xml, effectId, std::static_pointer_cast<EffectStackModel>(stack)->getOwnerId(), originalDecimalPoint)
    , m_childId(0)
{
    if (m_asset->property_exists("kdenlive:bin-disabled")) {
        // This effect was disabled by a global disable effects option
        m_effectStackEnabled = false;
        m_enabled = true;
    }
    connect(this, &AssetParameterModel::updateChildren, [&](const QStringList &names) {
        if (m_childEffects.size() == 0) {
            return;
        }
        // qDebug() << "* * *SETTING EFFECT PARAM: " << name << " = " << m_asset->get(name.toUtf8().constData());
        QMapIterator<int, std::shared_ptr<EffectItemModel>> i(m_childEffects);
        while (i.hasNext()) {
            i.next();
            for (const QString &name : names) {
                i.value()->getAsset()->set(name.toUtf8().constData(), m_asset->get(name.toUtf8().constData()));
            }
        }
    });
}

// static
std::shared_ptr<EffectItemModel> EffectItemModel::construct(const QString &effectId, std::shared_ptr<AbstractTreeModel> stack, bool effectEnabled)
{
    Q_ASSERT(EffectsRepository::get()->exists(effectId));
    QDomElement xml = EffectsRepository::get()->getXml(effectId);

    std::unique_ptr<Mlt::Service> effect = EffectsRepository::get()->getEffect(effectId);
    effect->set("kdenlive_id", effectId.toUtf8().constData());

    QList<QVariant> data;
    data << EffectsRepository::get()->getName(effectId) << effectId;
    bool isLink = effect->type() == mlt_service_link_type;

    std::shared_ptr<EffectItemModel> self(new EffectItemModel(data, std::move(effect), xml, effectId, stack, effectEnabled));
    self->m_isLink = isLink;

    baseFinishConstruct(self);
    return self;
}

std::shared_ptr<EffectItemModel> EffectItemModel::construct(std::unique_ptr<Mlt::Service> effect, std::shared_ptr<AbstractTreeModel> stack,
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
            for (const QString &n : std::as_const(names)) {
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
        int ret;
        if (m_isLink) {
            Mlt::Chain fromChain(static_cast<Mlt::Producer *>(ptr.get())->parent());
            ret = fromChain.attach(link());
        } else {
            ret = ptr->attach(filter());
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
            const QString effName = filt->get("kdenlive_id");
            if (effName == m_assetId && filt->get_int("_kdenlive_processed") == 0) {
                if (auto ptr2 = m_model.lock()) {
                    effect = EffectItemModel::construct(std::move(filt), ptr2, QString());
                    if (getAsset()->get_int("disable") == 1) {
                        effect->getAsset()->set("disable", 1);
                    }
                    int childId = ptr->get_int("_childid");
                    if (childId == 0) {
                        childId = ++m_childId;
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

void EffectItemModel::plantClone(const std::weak_ptr<Mlt::Service> &service, int target)
{
    if (auto ptr = service.lock()) {
        const QString effectId = getAssetId();
        std::shared_ptr<EffectItemModel> effect = nullptr;
        bool targetHasAudio = ptr->get_int("set.test_audio") == 0;
        bool targetHasVideo = ptr->get_int("set.test_image") == 0;
        bool disable = false;
        if (isAudio()) {
            if (!targetHasAudio) {
                disable = true;
            }
        } else if (!targetHasVideo) {
            disable = true;
        }
        if (auto ptr2 = m_model.lock()) {
            effect = EffectItemModel::construct(effectId, ptr2);
            effect->setParameters(getAllParameters(), false);
            // ensure duplicated assets gets disabled if the original is
            if (disable || m_asset->get_int("disable") == 1) {
                effect->getAsset()->set("disable", 1);
            }
            int childId = ptr->get_int("_childid");
            if (childId == 0) {
                childId = ++m_childId;
                ptr->set("_childid", childId);
            }
            m_childEffects.insert(childId, effect);
            int ret;
            if (m_isLink) {
                Mlt::Chain fromChain(static_cast<Mlt::Producer *>(ptr.get())->parent());
                ret = fromChain.attach(effect->link());
            } else {
                ret = ptr->attach(effect->filter());
            }
            if (ret == 0 && target > -1) {
                ptr->move_filter(ptr->count() - 1, target);
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
        if (m_isLink) {
            Mlt::Chain fromChain(static_cast<Mlt::Producer *>(ptr.get())->parent());
            ret = fromChain.detach(link());
        } else {
            ret = ptr->detach(filter());
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
        if (m_isLink) {
            Mlt::Chain fromChain(static_cast<Mlt::Producer *>(ptr.get())->parent());
            ret = fromChain.detach(link());
        } else {
            ret = ptr->detach(filter());
        }
        Q_ASSERT(ret == 0);
        if (!ptr->property_exists("_childid")) {
            return;
        }
        int childId = ptr->get_int("_childid");
        auto effect = m_childEffects.take(childId);
        if (effect && effect->isValid()) {
            int ret;
            if (effect->m_isLink) {
                Mlt::Chain fromChain(static_cast<Mlt::Producer *>(ptr.get())->parent());
                ret = fromChain.detach(effect->link());
            } else {
                ret = ptr->detach(effect->filter());
            }
            effect.reset();
        } else {
            qDebug() << "TRYING TO REMOVE INVALID EFFECT!!!!!!!";
        }
    } else {
        qDebug() << "Error : Cannot plant effect because parent service is not available anymore";
        Q_ASSERT(false);
    }
}

int EffectItemModel::get_in() const
{
    return m_isLink ? link().get_in() : filter().get_in();
}

int EffectItemModel::get_out() const
{
    return m_isLink ? link().get_out() : filter().get_out();
}

int EffectItemModel::get_length() const
{
    return m_isLink ? link().get_length() : filter().get_length();
}

void EffectItemModel::set_in_and_out(int in, int out)
{
    if (m_isLink) {
        link().set_in_and_out(in, out);
    } else {
        filter().set_in_and_out(in, out);
    }
}

Mlt::Filter &EffectItemModel::filter() const
{
    return *static_cast<Mlt::Filter *>(m_asset.get());
}

Mlt::Link &EffectItemModel::link() const
{
    return *static_cast<Mlt::Link *>(m_asset.get());
}

bool EffectItemModel::isValid() const
{
    return m_asset && m_asset->is_valid();
}

void EffectItemModel::setBuiltIn()
{
    m_builtIn = true;
    getAsset()->set("kdenlive:builtin", 1);
}

void EffectItemModel::updateEnable(bool updateTimeline)
{
    bool enabled = isAssetEnabled();
    if (enabled) {
        getAsset()->clear("disable");
    } else {
        getAsset()->set("disable", 1);
    }
    if (updateTimeline && !isAudio()) {
        pCore->refreshProjectItem(m_ownerId);
        pCore->invalidateItem(m_ownerId);
    }
    const QModelIndex start = AssetParameterModel::index(0, 0);
    const QModelIndex end = AssetParameterModel::index(rowCount() - 1, 0);
    Q_EMIT dataChanged(start, end, QVector<int>());
    Q_EMIT enabledChange(enabled);
    // Update timeline child producers
    Q_EMIT AssetParameterModel::updateChildren({QStringLiteral("disable")});
}

void EffectItemModel::setCollapsed(bool collapsed)
{
    getAsset()->set("kdenlive:collapsed", collapsed ? 1 : 0);
}

bool EffectItemModel::isCollapsed() const
{
    return getAsset()->get_int("kdenlive:collapsed") == 1;
}

void EffectItemModel::setKeyframesHidden(bool hidden)
{
    Fun undo = [this, hidden]() {
        getAsset()->set("kdenlive:kfrhidden", hidden ? 0 : 1);
        Q_EMIT hideKeyframesChange(hidden ? false : true);
        return true;
    };
    Fun redo = [this, hidden]() {
        getAsset()->set("kdenlive:kfrhidden", hidden ? 1 : 0);
        Q_EMIT hideKeyframesChange(hidden ? true : false);
        return true;
    };
    redo();
    pCore->pushUndo(undo, redo, hidden ? i18n("Hide keyframes") : i18n("Show keyframes"));
}

bool EffectItemModel::isKeyframesHidden() const
{
    return getAsset()->get_int("kdenlive:kfrhidden") == 1;
}

bool EffectItemModel::keyframesHiddenUnset() const
{
    return getAsset()->property_exists("kdenlive:kfrhidden") == false;
}

bool EffectItemModel::hasForcedInOut() const
{
    return getAsset()->get_int("kdenlive:force_in_out") == 1 && getAsset()->get_int("out") > 0;
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
    Fun redo = [this, enabled, bounds, currentInOut]() {
        m_asset->set("kdenlive:force_in_out", enabled ? 1 : 0);
        m_asset->set("in", bounds.first);
        m_asset->set("out", bounds.second);
        if (!enabled) {
            // Store last used zone
            m_asset->set("_kdenlive_zone_in", currentInOut.first);
            m_asset->set("_kdenlive_zone_out", currentInOut.second);
        }
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

void EffectItemModel::setEffectStackEnabled(bool enabled)
{
    if (m_effectStackEnabled == enabled) {
        // nothing to do
        return;
    }
    if (enabled) {
        m_asset->clear("kdenlive:bin-disabled");
    } else if (m_enabled) {
        m_asset->set("kdenlive:bin-disabled", 1);
    }
    AbstractEffectItem::setEffectStackEnabled(enabled);
}

bool EffectItemModel::isBuiltIn() const
{
    return m_builtIn;
}

bool EffectItemModel::isHiddenBuiltIn() const
{
    return m_asset->get_int("kdenlive:hiddenbuiltin") == 1;
}
