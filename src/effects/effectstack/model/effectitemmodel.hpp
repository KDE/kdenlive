/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstracteffectitem.hpp"
#include "abstractmodel/treeitem.hpp"
#include "assets/model/assetparametermodel.hpp"
#include <mlt++/MltFilter.h>

class EffectStackModel;
/** @brief This represents an effect of the effectstack
 */
class EffectItemModel : public AbstractEffectItem, public AssetParameterModel
{

public:
    /** @brief This construct an effect of the given id
       @param is a ptr to the model this item belongs to. This is required to send update signals
     */
    static std::shared_ptr<EffectItemModel> construct(const QString &effectId, std::shared_ptr<AbstractTreeModel> stack, bool effectEnabled = true);
    /** @brief This construct an effect with an already existing filter
       Only used when loading an existing clip
     */
    static std::shared_ptr<EffectItemModel> construct(std::unique_ptr<Mlt::Service> effect, std::shared_ptr<AbstractTreeModel> stack,
                                                      const QString &originalDecimalPoint);

    /** @brief This function plants the effect into the given service in last position
     */
    void plant(const std::weak_ptr<Mlt::Service> &service) override;
    void plantClone(const std::weak_ptr<Mlt::Service> &service, int target = -1) override;
    void loadClone(const std::weak_ptr<Mlt::Service> &service);
    /** @brief This function unplants (removes) the effect from the given service
     */
    void unplant(const std::weak_ptr<Mlt::Service> &service) override;
    void unplantClone(const std::weak_ptr<Mlt::Service> &service) override;

    Mlt::Filter &filter() const;
    Mlt::Link &link() const;

    int get_in() const;
    int get_out() const;
    int get_length() const;
    void set_in_and_out(int in, int out);

    void setEffectStackEnabled(bool enabled) override;
    /** @brief Return true if the effect applies only to audio */
    bool isAudio() const override;
    bool isUnique() const override;

    void setCollapsed(bool collapsed);
    bool isCollapsed() const;
    void setKeyframesHidden(bool hidden);
    bool isKeyframesHidden() const;
    bool keyframesHiddenUnset() const;
    bool hasForcedInOut() const;
    bool isValid() const;
    bool isBuiltIn() const;
    bool isHiddenBuiltIn() const;
    QPair<int, int> getInOut() const;
    void setInOut(const QString &effectName, QPair<int, int> bounds, bool enabled, bool withUndo);
    /** @brief This is a built-in asset */
    void setBuiltIn();

protected:
    EffectItemModel(const QList<QVariant> &effectData, std::unique_ptr<Mlt::Service> effect, const QDomElement &xml, const QString &effectId,
                    const std::shared_ptr<AbstractTreeModel> &stack, bool isEnabled = true, QString originalDecimalPoint = QString());
    QMap<int, std::shared_ptr<EffectItemModel>> m_childEffects;
    void updateEnable(bool updateTimeline = true) override;
    int m_childId;
    bool m_isLink{false};
};
