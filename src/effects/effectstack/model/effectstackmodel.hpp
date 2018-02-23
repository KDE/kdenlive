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
#include "definitions.h"
#include "undohelper.hpp"

#include <QReadWriteLock>
#include <memory>
#include <mlt++/Mlt.h>

/* @brief This class an effect stack as viewed by the back-end.
   It is responsible for planting and managing effects into the producer it holds a pointer to.
 */
class AbstractEffectItem;
class AssetParameterModel;
class DocUndoStack;
class EffectItemModel;
class TreeItem;
class KeyframeModel;

class EffectStackModel : public AbstractTreeModel
{
    Q_OBJECT

public:
    /* @brief Constructs an effect stack and returns a shared ptr to the constucted object
       @param service is the mlt object on which we will plant the effects
       @param ownerId is some information about the actual object to which the effects are applied
    */
    static std::shared_ptr<EffectStackModel> construct(std::weak_ptr<Mlt::Service> service, ObjectId ownerId, std::weak_ptr<DocUndoStack> undo_stack);
    void resetService(std::weak_ptr<Mlt::Service> service);
    void loadEffects();

protected:
    EffectStackModel(std::weak_ptr<Mlt::Service> service, ObjectId ownerId, std::weak_ptr<DocUndoStack> undo_stack);

public:
    /* @brief Add an effect at the bottom of the stack */
    void appendEffect(const QString &effectId, bool makeCurrent = false);
    /* @brief Copy an existing effect and append it at the bottom of the stack */
    void copyEffect(std::shared_ptr<AbstractEffectItem> sourceItem);
    /* @brief Import all effects from the given effect stack
     */
    void importEffects(std::shared_ptr<EffectStackModel> sourceStack);
    bool removeFade(bool fromStart);

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
    /* @brief Adjust an effect duration (useful for fades) */
    bool adjustFadeLength(int duration, bool fromStart, bool audioFade, bool videoFade);
    bool adjustStackLength(bool adjustFromEnd, int oldIn, int newIn, int duration, bool hasAudio, bool audioOnly, Fun &undo, Fun &redo, bool logUndo);

    void slotCreateGroup(std::shared_ptr<EffectItemModel> childEffect);

    /* @brief Returns the id of the owner of the stack */
    ObjectId getOwnerId() const;

    int getFadePosition(bool fromStart);
    Q_INVOKABLE void adjust(const QString &effectId, const QString &effectName, double value);
    Q_INVOKABLE bool hasFilter(const QString &effectId);
    Q_INVOKABLE double getFilter(const QString &effectId, const QString &paramName);
    /** get the active effect's keyframe model */
    Q_INVOKABLE KeyframeModel *getEffectKeyframeModel();
    /** Remove unwanted fade effects, mostly after a cut operation */
    void cleanFadeEffects(bool outEffects, Fun &undo, Fun &redo);

public slots:
    /* @brief Delete an effect from the stack */
    void removeEffect(std::shared_ptr<EffectItemModel> effect);

protected:
    /* @brief Register the existence of a new element
     */
    void registerItem(const std::shared_ptr<TreeItem> &item) override;
    /* @brief Deregister the existence of a new element*/
    void deregisterItem(int id, TreeItem *item) override;

    /* @brief This is a convenience function that helps check if the tree is in a valid state */
    bool checkConsistency() override;

    std::weak_ptr<Mlt::Service> m_service;
    bool m_effectStackEnabled;
    ObjectId m_ownerId;

    std::weak_ptr<DocUndoStack> m_undoStack;

private:
    mutable QReadWriteLock m_lock;
    QList<int> fadeIns;
    QList<int> fadeOuts;
    /** @brief: When loading a project, we load filters/effects that are already planted
     *          in the producer, so we shouldn't plant them again. Setting this value to
     *          true will prevent planting in the producer */
    bool m_loadingExisting;

private slots:
    /** @brief: Some effects do not support dynamic changes like sox, and need to be unplugged / replugged on each param change
     */
    void replugEffect(std::shared_ptr<AssetParameterModel> asset);

signals:
    void modelChanged();
};

#endif
