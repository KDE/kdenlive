/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "effectgroupmodel.hpp"

#include "effectstackmodel.hpp"
#include <utility>

EffectGroupModel::EffectGroupModel(const QList<QVariant> &data, QString name, const std::shared_ptr<AbstractTreeModel> &stack, bool isRoot)
    : AbstractEffectItem(EffectItemType::Group, data, stack, isRoot)
    , m_name(std::move(name))
{
}

// static
std::shared_ptr<EffectGroupModel> EffectGroupModel::construct(const QString &name, std::shared_ptr<AbstractTreeModel> stack, bool isRoot)
{
    QList<QVariant> data;
    data << name << name;

    std::shared_ptr<EffectGroupModel> self(new EffectGroupModel(data, name, stack, isRoot));

    baseFinishConstruct(self);

    return self;
}

void EffectGroupModel::updateEnable(bool updateTimeline)
{
    Q_UNUSED(updateTimeline);
    for (int i = 0; i < childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(child(i))->updateEnable();
    }
}

bool EffectGroupModel::isAudio() const
{
    bool result = false;
    for (int i = 0; i < childCount() && !result; ++i) {
        result = result || std::static_pointer_cast<AbstractEffectItem>(child(i))->isAudio();
    }
    return result;
}

bool EffectGroupModel::isUnique() const
{
    return false;
}

void EffectGroupModel::plant(const std::weak_ptr<Mlt::Service> &service)
{
    for (int i = 0; i < childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(child(i))->plant(service);
    }
}
void EffectGroupModel::plantClone(const std::weak_ptr<Mlt::Service> &service)
{
    for (int i = 0; i < childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(child(i))->plantClone(service);
    }
}
void EffectGroupModel::unplant(const std::weak_ptr<Mlt::Service> &service)
{
    for (int i = 0; i < childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(child(i))->unplant(service);
    }
}
void EffectGroupModel::unplantClone(const std::weak_ptr<Mlt::Service> &service)
{
    for (int i = 0; i < childCount(); ++i) {
        std::static_pointer_cast<AbstractEffectItem>(child(i))->unplantClone(service);
    }
}
