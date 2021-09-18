/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef EFFECTGROUPMODEL_H
#define EFFECTGROUPMODEL_H

#include "abstracteffectitem.hpp"
#include "abstractmodel/treeitem.hpp"

class EffectStackModel;
/** @brief This represents a group of effects of the effectstack
 */
class EffectGroupModel : public AbstractEffectItem
{

public:
    /** @brief This construct an effect of the given id
       @param is a ptr to the model this item belongs to. This is required to send update signals
     */
    static std::shared_ptr<EffectGroupModel> construct(const QString &name, std::shared_ptr<AbstractTreeModel> stack, bool isRoot = false);

    /** @brief Return true if the effect applies only to audio */
    bool isAudio() const override;
    bool isUnique() const override;

    /** @brief This function plants the effect into the given service in last position
     */
    void plant(const std::weak_ptr<Mlt::Service> &service) override;
    void plantClone(const std::weak_ptr<Mlt::Service> &service) override;
    /** @brief This function unplants (removes) the effect from the given service
     */
    void unplant(const std::weak_ptr<Mlt::Service> &service) override;
    void unplantClone(const std::weak_ptr<Mlt::Service> &service) override;

protected:
    EffectGroupModel(const QList<QVariant> &data, QString name, const std::shared_ptr<AbstractTreeModel> &stack, bool isRoot = false);

    void updateEnable(bool updateTimeline = true) override;

    QString m_name;
};

#endif
