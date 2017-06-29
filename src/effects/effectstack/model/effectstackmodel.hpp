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

#ifndef EFFECTSTACKMODEL_H
#define EFFECTSTACKMODEL_H

#include "abstractmodel/abstracttreemodel.hpp"
#include "undohelper.hpp"
#include "definitions.h"

#include <QReadWriteLock>
#include <memory>
#include <mlt++/Mlt.h>

/* @brief This class an effect stack as viewed by the back-end.
   It is responsible for planting and managing effects into the producer it holds a pointer to.
 */
class TreeItem;
class EffectItemModel;
class AbstractEffectItem;

class EffectStackModel : public AbstractTreeModel
{

public:
    /* @brief Constructs an effect stack and returns a shared ptr to the constucted object
       @param service is the mlt object on which we will plant the effects
       @param ownerId is some information about the actual object to which the effects are applied
    */
    static std::shared_ptr<EffectStackModel> construct(std::weak_ptr<Mlt::Service> service, ObjectId ownerId);
    void resetService(std::weak_ptr<Mlt::Service> service);

protected:
    EffectStackModel(std::weak_ptr<Mlt::Service> service, ObjectId ownerId);

public:
    /* @brief Add an effect at the bottom of the stack */
    void appendEffect(const QString &effectId);
    /* @brief Copy an existing effect and append it at the bottom of the stack */
    void copyEffect(std::shared_ptr<AbstractEffectItem> sourceItem);
    /* @brief Import all effects from the given effect stack
     */
    void importEffects(std::shared_ptr<EffectStackModel> sourceStack);

    /* @brief This function change the global (timeline-wise) enabled state of the effects
     */
    void setEffectStackEnabled(bool enabled);

    /* @brief Returns an effect or group from the stack (at the given row) */
    std::shared_ptr<AbstractEffectItem> getEffectStackRow(int row, std::shared_ptr<TreeItem> parentItem = nullptr);

    /* @brief Move an effect in the stack */
    void moveEffect(int destRow, std::shared_ptr<AbstractEffectItem> item);

    /* @brief Set effect in row as current one */
    void setActiveEffect(int ix);
    /* @brief Get currently active effect row */
    int getActiveEffect() const;

    void slotCreateGroup(std::shared_ptr<EffectItemModel> childEffect);

    /* @brief Returns the id of the owner of the stack */
    ObjectId getOwnerId() const;

public slots:
    /* @brief Delete an effect from the stack */
    void removeEffect(std::shared_ptr<EffectItemModel> effect);

protected:
    /* @brief Register the existence of a new element
     */
    void registerItem(const std::shared_ptr<TreeItem> &item) override;
    /* @brief Deregister the existence of a new element*/
    void deregisterItem(int id, TreeItem *item) override;


    std::weak_ptr<Mlt::Service> m_service;
    bool m_effectStackEnabled;
    ObjectId m_ownerId;

private:
    mutable QReadWriteLock m_lock;
};

#endif
