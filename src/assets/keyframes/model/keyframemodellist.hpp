/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "keyframemodel.hpp"
#include "undohelper.hpp"
#include "utils/gentime.h"

#include <QReadWriteLock>

#include <QObject>
#include <map>
#include <memory>
#include <unordered_map>

class AssetParameterModel;
class DocUndoStack;

/** @class KeyframeModelList
    @brief This class is a container for the keyframe models.
   If an asset has several keyframable parameters, each one has its own keyframeModel,
   but we regroup all of these in a common class to provide unified access.
 */
class KeyframeModelList : public QObject
{
    Q_OBJECT

public:
    /** @brief Construct a keyframe list bound to the given asset
       @param init_value and index correspond to the first parameter
     */
    explicit KeyframeModelList(std::weak_ptr<AssetParameterModel> model, const QModelIndex &index, std::weak_ptr<DocUndoStack> undo_stack, int in, int out);

    /** @brief Add a keyframable parameter to be managed by this model */
    void addParameter(const QModelIndex &index, int in, int out);

    /** @brief Adds a keyframe at the given position. If there is already one then we update it.
       @param pos defines the position of the keyframe, relative to the clip
       @param type is the type of the keyframe.
     */
    bool addKeyframe(GenTime pos, KeyframeType type);
    bool addKeyframe(int frame, double val);

    /** @brief Removes the keyframe at the given position. */
    bool removeKeyframe(GenTime pos);
    bool removeKeyframeWithUndo(GenTime pos, Fun &undo, Fun &redo);

    /** @brief Duplicate a keyframe at the given position. */
    bool duplicateKeyframeWithUndo(GenTime srcPos, GenTime destPos, Fun &undo, Fun &redo);
    /** @brief Delete all the keyframes of the model (except first) */
    bool removeAllKeyframes();
    /** @brief Delete all the keyframes after a certain position (except first) */
    bool removeNextKeyframes(GenTime pos);

    /** @brief moves a keyframe
       @param oldPos is the old position of the keyframe
       @param pos defines the new position of the keyframe, relative to the clip
       @param logUndo if true, then an undo object is created
    */
    bool moveKeyframe(GenTime oldPos, GenTime pos, bool logUndo, bool updateView = true);
    bool moveKeyframeWithUndo(GenTime oldPos, GenTime pos, Fun &undo, Fun &redo);

    /** @brief updates the value of a keyframe
       @param pos is the position of the keyframe
       @param value is the new value of the param
       @param index is the index of the wanted keyframe
    */
    bool updateKeyframe(GenTime pos, const QVariant &value, int ix, const QPersistentModelIndex &index, QUndoCommand *parentCommand = nullptr);
    /** @brief updates the value of a keyframe which contains multiple params, like Lift/Gamma/Gain
       @param pos is the position of the keyframe
       @param sourceValues is the list of previous values (used when undoing)
       @param values is the new values list
       @param indexes is the index list of the wanted keyframe
    */
    bool updateMultiKeyframe(GenTime pos, const QStringList &sourceValues, const QStringList &values, const QList<QModelIndex> &indexes,
                             QUndoCommand *parentCommand = nullptr);
    bool updateKeyframeType(GenTime pos, int type, const QPersistentModelIndex &index);
    bool updateKeyframe(GenTime oldPos, GenTime pos, const QVariant &normalizedVal, bool logUndo = true);
    KeyframeType keyframeType(GenTime pos) const;
    /** @brief Returns a keyframe data at given pos
       ok is a return parameter, set to true if everything went good
     */
    Keyframe getKeyframe(const GenTime &pos, bool *ok) const;

    /** @brief Returns true if we only have 1 keyframe
     */
    bool singleKeyframe() const;
    /** @brief Returns true if we only have no keyframe
     */
    bool isEmpty() const;
    /** @brief Returns the number of keyframes
     */
    int count() const;

    /** @brief Returns the keyframe located after given position.
       If there is a keyframe at given position it is ignored.
       @param ok is a return parameter to tell if a keyframe was found.
    */
    Keyframe getNextKeyframe(const GenTime &pos, bool *ok) const;

    /** @brief Returns the keyframe located before given position.
       If there is a keyframe at given position it is ignored.
       @param ok is a return parameter to tell if a keyframe was found.
    */
    Keyframe getPrevKeyframe(const GenTime &pos, bool *ok) const;

    /** @brief Returns the closest keyframe from given position.
       @param ok is a return parameter to tell if a keyframe was found.
    */
    Keyframe getClosestKeyframe(const GenTime &pos, bool *ok) const;

    /** @brief Returns true if a keyframe exists at given pos
       Notice that add/remove queries are done in real time (gentime), but this request is made in frame
     */
    Q_INVOKABLE bool hasKeyframe(int frame) const;

    /** @brief Return the interpolated value of a parameter.
       @param pos is the position where we interpolate
       @param index is the index of the queried parameter. */
    QVariant getInterpolatedValue(int pos, const QPersistentModelIndex &index) const;
    /** @brief Return the interpolated value of a parameter.
       @param pos is the position where we interpolate
       @param index is the index of the queried parameter. */
    QVariant getInterpolatedValue(const GenTime &pos, const QPersistentModelIndex &index) const;

    /** @brief Load keyframes from the current parameter value. */
    void refresh();
    void setParametersFromTask(const paramVector &params);
    /** @brief Reset all keyframes and add a default one */
    void reset();
    Q_INVOKABLE KeyframeModel *getKeyModel();
    KeyframeModel *getKeyModel(const QPersistentModelIndex &index);
    /** @brief Returns parent asset owner id*/
    ObjectId getOwnerId() const;
    /** @brief Returns parent asset id*/
    const QString getAssetId();
    const QString getAssetRow();

    /** @brief Returns the list of selected keyframes */
    QVector<int> selectedKeyframes() const;
    /** @brief Remove a position from selected keyframes */
    void removeFromSelected(int pos);
    /** @brief Replace list of selected keyframes */
    void setSelectedKeyframes(const QVector<int> &list);
    /** @brief Append a keyframe to selection */
    void appendSelectedKeyframe(int frame);

    /** @brief Get the currently active keyframe */
    int activeKeyframe() const;
    /** @brief Set the currently active keyframe */
    void setActiveKeyframe(int pos);

    /** @brief Parent item size change, update keyframes*/
    void resizeKeyframes(int oldIn, int oldOut, int in, int out, int offset, bool adjustFromEnd, Fun &undo, Fun &redo);

    /** @brief Parent item size change, update keyframes*/
    void moveKeyframes(int oldIn, int in, Fun &undo, Fun &redo);

    /** @brief Return position of the nth keyframe (ix = nth)*/
    GenTime getPosAtIndex(int ix);
    int getIndexForPos(GenTime pos);
    QModelIndex getIndexAtRow(int row);

    /** @brief Check that all keyframable parameters have the same keyframes on loading
     *  (that's how our model works) */
    void checkConsistency();
    /** @brief Returns the indexes of all parameters */
    std::vector<QPersistentModelIndex> getIndexes();
    std::unordered_map<QPersistentModelIndex, std::shared_ptr<KeyframeModel>> getAllParameters() { return m_parameters; };
    /** @brief Returns the model currently displayed in timeline, nullptr if none */
    std::shared_ptr<KeyframeModel> modelInTimeline() const;
    /** @brief Returns true if this is the first keyframable model in the list */
    bool isFirstParameter(std::shared_ptr<KeyframeModel> param) const;

protected:
    /** @brief Helper function to apply a given operation on all parameters */
    bool applyOperation(const std::function<bool(std::shared_ptr<KeyframeModel>, bool, Fun &, Fun &)> &op, Fun &undo, Fun &redo);

Q_SIGNALS:
    void modelChanged();
    void modelDisplayChanged();

private:
    std::weak_ptr<AssetParameterModel> m_model;
    std::weak_ptr<DocUndoStack> m_undoStack;
    std::unordered_map<QPersistentModelIndex, std::shared_ptr<KeyframeModel>> m_parameters;
    /** @brief Index of the parameter that is displayed in timeline */
    QModelIndex m_inTimelineIndex;
    mutable QReadWriteLock m_lock; // This is a lock that ensures safety in case of concurrent access

private Q_SLOTS:
    void slotUpdateModels(const QModelIndex &ix1, const QModelIndex &ix2, const QVector<int> &roles);

public:
    // this is to enable for range loops
    auto begin() -> decltype(m_parameters.begin()->second->begin()) { return m_parameters.begin()->second->begin(); }
    auto end() -> decltype(m_parameters.begin()->second->end()) { return m_parameters.begin()->second->end(); }
};
