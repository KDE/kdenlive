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
    connect(this, &AssetParameterModel::updateChildren, [&](const QString &name) {
        if (m_childEffects.size() == 0) {
            return;
        }
        qDebug() << "* * *SETTING EFFECT PARAM: " << name << " = " << m_asset->get(name.toUtf8().constData());
        QMapIterator<int, std::shared_ptr<EffectItemModel>> i(m_childEffects);
        while (i.hasNext()) {
            i.next();
            i.value()->filter().set(name.toUtf8().constData(), m_asset->get(name.toUtf8().constData()));
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

std::shared_ptr<EffectItemModel> EffectItemModel::construct(std::unique_ptr<Mlt::Properties> effect, std::shared_ptr<AbstractTreeModel> stack, QString originalDecimalPoint)
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
        int ret = ptr->attach(filter());
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
            int ret = ptr->attach(effect->filter());
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
        int ret = ptr->detach(filter());
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
        int ret = ptr->detach(filter());
        Q_ASSERT(ret == 0);
        int childId = ptr->get_int("_childid");
        auto effect = m_childEffects.take(childId);
        if (effect && effect->isValid()) {
            ptr->detach(effect->filter());
            effect.reset();
        }
    } else {
        qDebug() << "Error : Cannot plant effect because parent service is not available anymore";
        Q_ASSERT(false);
    }
}

Mlt::Filter &EffectItemModel::filter() const
{
    return *static_cast<Mlt::Filter *>(m_asset.get());
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
    emit dataChanged(start, end, QVector<int>());
    emit enabledChange(!isEnabled());
    // Update timeline child producers
    emit AssetParameterModel::updateChildren(QStringLiteral("disable"));
}

void EffectItemModel::setCollapsed(bool collapsed)
{
    filter().set("kdenlive:collapsed", collapsed ? 1 : 0);
}

bool EffectItemModel::isCollapsed()
{
    return filter().get_int("kdenlive:collapsed") == 1;
}

bool EffectItemModel::isAudio() const
{
    AssetListType::AssetType type = EffectsRepository::get()->getType(m_assetId);
    return  type == AssetListType::AssetType::Audio || type == AssetListType::AssetType::CustomAudio;
}

bool EffectItemModel::isUnique() const
{
    return EffectsRepository::get()->isUnique(m_assetId);
}
