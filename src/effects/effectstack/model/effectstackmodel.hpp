/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractmodel/abstracttreemodel.hpp"
#include "definitions.h"
#include "undohelper.hpp"

#include <QReadWriteLock>
#include <QUuid>
#include <memory>
#include <mlt++/Mlt.h>
#include <unordered_set>

/** @brief This class an effect stack as viewed by the back-end.
   It is responsible for planting and managing effects into the list of producer it holds a pointer to.
   It can contains more than one producer for example if it represents the effect stack of a projectClip: this clips contains several producers (audio, video,
   ...)
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
    /** @brief Constructs an effect stack and returns a shared ptr to the constructed object
       @param service is the mlt object on which we will plant the effects
       @param ownerId is some information about the actual object to which the effects are applied
    */
    static std::shared_ptr<EffectStackModel> construct(std::weak_ptr<Mlt::Service> service, ObjectId ownerId, std::weak_ptr<DocUndoStack> undo_stack);

protected:
    EffectStackModel(std::weak_ptr<Mlt::Service> service, ObjectId ownerId, std::weak_ptr<DocUndoStack> undo_stack);

public:
    /** @brief Add an effect at the bottom of the stack */
    bool appendEffect(const QString &effectId, bool makeCurrent = false, stringMap params = {});
    bool appendEffectWithUndo(const QString &effectId, Fun &undo, Fun &redo);
    /** @brief Copy an existing effect and append it at the bottom of the stack
     */
    bool copyEffect(const std::shared_ptr<AbstractEffectItem> &sourceItem, PlaylistState::ClipState state, bool logUndo = true);
    bool copyXmlEffect(const QDomElement &effect);
    bool copyXmlEffectWithUndo(const QDomElement &effect, Fun &undo, Fun &redo);
    /** @brief Import all effects from the given effect stack
     */
    bool importEffects(const std::shared_ptr<EffectStackModel> &sourceStack, PlaylistState::ClipState state);
    void importEffects(const std::weak_ptr<Mlt::Service> &service, PlaylistState::ClipState state, bool alreadyExist = false,
                       const QString &originalDecimalPoint = QString(), const QUuid &uuid = QUuid());
    bool removeFade(bool fromStart);

    /** @brief This function change the global (timeline-wise) enabled state of the effects
     */
    void setEffectStackEnabled(bool enabled);

    /** @brief Returns an effect or group from the stack (at the given row) */
    std::shared_ptr<AbstractEffectItem> getEffectStackRow(int row, const std::shared_ptr<TreeItem> &parentItem = nullptr);
    std::shared_ptr<AssetParameterModel> getAssetModelById(const QString &effectId);

    /** @brief Move an effect in the stack */
    void moveEffect(int destRow, const std::shared_ptr<AbstractEffectItem> &item);

    /** @brief Set effect in row as current one */
    void setActiveEffect(int ix);
    /** @brief Get currently active effect row */
    int getActiveEffect() const;
    /** @brief Adjust an effect duration (useful for fades) */
    bool adjustFadeLength(int duration, bool fromStart, bool audioFade, bool videoFade, bool logUndo);
    bool adjustStackLength(bool adjustFromEnd, int oldIn, int oldDuration, int newIn, int duration, int offset, Fun &undo, Fun &redo, bool logUndo);

    void slotCreateGroup(const std::shared_ptr<EffectItemModel> &childEffect);

    /** @brief Returns the id of the owner of the stack */
    ObjectId getOwnerId() const;

    int getFadePosition(bool fromStart);
    Q_INVOKABLE void adjust(const QString &effectId, const QString &effectName, double value);

    /** @brief Returns true if the stack contains an effect with the given Id */
    Q_INVOKABLE bool hasFilter(const QString &effectId) const;
    // TODO: this break the encapsulation, remove
    Q_INVOKABLE double getFilterParam(const QString &effectId, const QString &paramName);
    /** @brief get the active effect's keyframe model */
    Q_INVOKABLE KeyframeModel *getEffectKeyframeModel();
    /** Add a keyframe in all model parameters */
    bool addEffectKeyFrame(int frame, double normalisedVal);
    /** @brief Remove a keyframe in all model parameters */
    bool removeKeyFrame(int frame);
    /** Update a keyframe in all model parameters (with value updated only in first parameter)*/
    bool updateKeyFrame(int oldFrame, int newFrame, QVariant normalisedVal);
    /** @brief Returns true if active effect has a keyframe at pos p*/
    bool hasKeyFrame(int frame);
    /** @brief Remove unwanted fade effects, mostly after a cut operation */
    void cleanFadeEffects(bool outEffects, Fun &undo, Fun &redo);

    /** @brief Remove all the services associated with this stack and replace them with the given one */
    void resetService(std::weak_ptr<Mlt::Service> service);

    /** @brief Append a new service to be managed by this stack */
    void addService(std::weak_ptr<Mlt::Service> service);
    /** @brief Append an existing service to be managed by this stack (on document load)*/
    void loadService(std::weak_ptr<Mlt::Service> service);

    /** @brief Remove a service from those managed by this stack */
    void removeService(const std::shared_ptr<Mlt::Service> &service);

    /** @brief Returns a comma separated list of effect names */
    const QString effectNames() const;

    /** @brief Returns a list of external file urls used by the effects (e.g. LUTs) */
    QStringList externalFiles() const;

    bool isStackEnabled() const;

    /** @brief Returns an XML representation of the effect stack with all parameters */
    QDomElement toXml(QDomDocument &document);
    /** @brief Returns an XML representation of one of the effect in the stack with all parameters */
    QDomElement rowToXml(const QUuid &uuid, int row, QDomDocument &document);
    /** @brief Load an effect stack from an XML representation */
    bool fromXml(const QDomElement &effectsXml, Fun &undo, Fun &redo);
    /** @brief Delete active effect from stack */
    void removeCurrentEffect();

    /** @brief This is a convenience function that helps check if the tree is in a valid state */
    bool checkConsistency() override;

    /** @brief Return true if an asset id is already added to this effect stack */
    bool hasEffect(const QString &assetId) const;

    /** @brief Remove all effects for this stack */
    void removeAllEffects(Fun &undo, Fun &redo);

    /** @brief Returns a list of zones for all effects */
    QVariantList getEffectZones() const;

    /** @brief Copy all Kdenlive effects of this track on a producer */
    void passEffects(Mlt::Producer *producer, const QString &exception = QString());

public Q_SLOTS:
    /** @brief Delete an effect from the stack */
    void removeEffect(const std::shared_ptr<EffectItemModel> &effect);
    /** @brief Move an effect in the stack */
    void moveEffectByRow(int destRow, int srcRow);

protected:
    /** @brief Register the existence of a new element
     */
    void registerItem(const std::shared_ptr<TreeItem> &item) override;
    /** @brief Deregister the existence of a new element*/
    void deregisterItem(int id, TreeItem *item) override;

    std::weak_ptr<Mlt::Service> m_masterService;
    std::vector<std::weak_ptr<Mlt::Service>> m_childServices;
    bool m_effectStackEnabled;
    ObjectId m_ownerId;

    std::weak_ptr<DocUndoStack> m_undoStack;

private:
    mutable QReadWriteLock m_lock;
    std::unordered_set<int> m_fadeIns;
    std::unordered_set<int> m_fadeOuts;

    /** @brief: When loading a project, we load filters/effects that are already planted
     *          in the producer, so we shouldn't plant them again. Setting this value to
     *          true will prevent planting in the producer */
    bool m_loadingExisting;
    bool doAppendEffect(const QString &effectId, bool makeCurrent, stringMap params, Fun &undo, Fun &redo);

private Q_SLOTS:
    /** @brief: Some effects do not support dynamic changes like sox, and need to be unplugged / replugged on each param change
     */
    void replugEffect(const std::shared_ptr<AssetParameterModel> &asset);
    /** @brief: Some effect zones changed, ensure to update master effect zones
     */
    void updateEffectZones();

Q_SIGNALS:
    /** @brief: This signal is connected to the project clip for bin clips and activates the reload of effects on child (timeline) producers
     */
    void modelChanged();
    void enabledStateChanged();
    /** @brief: The master effect stack zones changed, update */
    void updateMasterZones();
    /** @brief: Currently active effect changed */
    void currentChanged(QModelIndex ix, bool active);
};
