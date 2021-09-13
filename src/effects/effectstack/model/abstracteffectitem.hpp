/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef ABSTRACTEFFECTITEM_H
#define ABSTRACTEFFECTITEM_H

#include "abstractmodel/treeitem.hpp"
#include <mlt++/MltFilter.h>

enum class EffectItemType { Effect, Group };
class EffectStackModel;
/** @brief This represents an effect of the effectstack
 */
class AbstractEffectItem : public TreeItem
{

public:
    AbstractEffectItem(EffectItemType type, const QList<QVariant> &data, const std::shared_ptr<AbstractTreeModel> &stack, bool isRoot = false,
                       bool isEnabled = true);

    /** @brief This function change the individual enabled state of the effect, creating an undo/redo entry*/
    void markEnabled(const QString &name, bool enabled);
    /** @brief This function directly change the individual enabled state of the effect */
    void setEnabled(bool enabled);

    /** @brief This function change the global (effectstack-wise) enabled state of the effect */
    void setEffectStackEnabled(bool enabled);

    /** @brief Returns whether the effect is enabled */
    bool isEnabled() const;

    friend class EffectGroupModel;

    EffectItemType effectItemType() const;

    /** @brief Return true if the effect or effect group applies only to audio */
    virtual bool isAudio() const = 0;
    /** @brief Return true if this effect can only have one instance in an effect stack */
    virtual bool isUnique() const = 0;

    /** @brief This function plants the effect into the given service in last position
     */
    virtual void plant(const std::weak_ptr<Mlt::Service> &service) = 0;
    virtual void plantClone(const std::weak_ptr<Mlt::Service> &service) = 0;
    /** @brief This function unplants (removes) the effect from the given service
     */
    virtual void unplant(const std::weak_ptr<Mlt::Service> &service) = 0;
    virtual void unplantClone(const std::weak_ptr<Mlt::Service> &service) = 0;

protected:
    /** @brief Toggles the mlt effect according to the current activation state*/
    virtual void updateEnable(bool updateTimeline = true) = 0;

    EffectItemType m_effectItemType;
    bool m_enabled;
    bool m_effectStackEnabled;
};

#endif
