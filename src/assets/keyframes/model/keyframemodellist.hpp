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

#ifndef KEYFRAMELISTMODELLIST_H
#define KEYFRAMELISTMODELLIST_H

#include "definitions.h"
#include "gentime.h"
#include "keyframemodel.hpp"
#include "undohelper.hpp"

#include <QReadWriteLock>

#include <QObject>
#include <map>
#include <memory>
#include <unordered_map>

class AssetParameterModel;
class DocUndoStack;

/* @brief This class is a container for the keyframe models.
   If an asset has several keyframable parameters, each one has its own keyframeModel,
   but we regroup all of these in a common class to provide unified access.
 */

class KeyframeModelList : public QObject
{
    Q_OBJECT

public:
    /* @brief Construct a keyframe list bound to the given asset
       @param init_value and index correspond to the first parameter
     */
    explicit KeyframeModelList(std::weak_ptr<AssetParameterModel> model, const QModelIndex &index, std::weak_ptr<DocUndoStack> undo_stack);

    /* @brief Add a keyframable parameter to be managed by this model */
    void addParameter(const QModelIndex &index);

    /* @brief Adds a keyframe at the given position. If there is already one then we update it.
       @param pos defines the position of the keyframe, relative to the clip
       @param type is the type of the keyframe.
     */
    bool addKeyframe(GenTime pos, KeyframeType type);
    bool addKeyframe(int frame, double val);

    /* @brief Removes the keyframe at the given position. */
    bool removeKeyframe(GenTime pos);
    /* @brief Delete all the keyframes of the model (except first) */
    bool removeAllKeyframes();
    /* @brief Delete all the keyframes after a certain position (except first) */
    bool removeNextKeyframes(GenTime pos);

    /* @brief moves a keyframe
       @param oldPos is the old position of the keyframe
       @param pos defines the new position of the keyframe, relative to the clip
       @param logUndo if true, then an undo object is created
    */
    bool moveKeyframe(GenTime oldPos, GenTime pos, bool logUndo);

    /* @brief updates the value of a keyframe
       @param old is the position of the keyframe
       @param value is the new value of the param
       @param index is the index of the wanted keyframe
    */
    bool updateKeyframe(GenTime pos, const QVariant &value, const QPersistentModelIndex &index);
    bool updateKeyframeType(GenTime pos, int type, const QPersistentModelIndex &index);
    bool updateKeyframe(GenTime oldPos, GenTime pos, const QVariant &normalizedVal, bool logUndo = true);
    KeyframeType keyframeType(GenTime pos) const;
    /* @brief Returns a keyframe data at given pos
       ok is a return parameter, set to true if everything went good
     */
    Keyframe getKeyframe(const GenTime &pos, bool *ok) const;

    /* @brief Returns true if we only have 1 keyframe
     */
    bool singleKeyframe() const;
    /* @brief Returns true if we only have no keyframe
     */
    bool isEmpty() const;
    /* @brief Returns the number of keyframes
     */
    int count() const;

    /* @brief Returns the keyframe located after given position.
       If there is a keyframe at given position it is ignored.
       @param ok is a return parameter to tell if a keyframe was found.
    */
    Keyframe getNextKeyframe(const GenTime &pos, bool *ok) const;

    /* @brief Returns the keyframe located before given position.
       If there is a keyframe at given position it is ignored.
       @param ok is a return parameter to tell if a keyframe was found.
    */
    Keyframe getPrevKeyframe(const GenTime &pos, bool *ok) const;

    /* @brief Returns the closest keyframe from given position.
       @param ok is a return parameter to tell if a keyframe was found.
    */
    Keyframe getClosestKeyframe(const GenTime &pos, bool *ok) const;

    /* @brief Returns true if a keyframe exists at given pos
       Notice that add/remove queries are done in real time (gentime), but this request is made in frame
     */
    Q_INVOKABLE bool hasKeyframe(int frame) const;

    /* @brief Return the interpolated value of a parameter.
       @param pos is the position where we interpolate
       @param index is the index of the queried parameter. */
    QVariant getInterpolatedValue(int pos, const QPersistentModelIndex &index) const;

    /* @brief Load keyframes from the current parameter value. */
    void refresh();
    /* @brief Reset all keyframes and add a default one */
    void reset();
    Q_INVOKABLE KeyframeModel *getKeyModel();
    KeyframeModel *getKeyModel(const QPersistentModelIndex &index);
    /** @brief Returns parent asset owner id*/
    ObjectId getOwnerId() const;

    /** @brief Parent item size change, update keyframes*/
    void resizeKeyframes(int oldIn, int oldOut, int in, int out, int offset, bool adjustFromEnd, Fun &undo, Fun &redo);
    
    /** @brief Return position of the nth keyframe (ix = nth)*/
    GenTime getPosAtIndex(int ix);

    /** @brief Check that all keyframable parameters have the same keyframes on loading
     *  (that's how our model works) */
    void checkConsistency();

protected:
    /** @brief Helper function to apply a given operation on all parameters */
    bool applyOperation(const std::function<bool(std::shared_ptr<KeyframeModel>, Fun &, Fun &)> &op, const QString &undoString);

signals:
    void modelChanged();

private:
    std::weak_ptr<AssetParameterModel> m_model;
    std::weak_ptr<DocUndoStack> m_undoStack;
    std::unordered_map<QPersistentModelIndex, std::shared_ptr<KeyframeModel>> m_parameters;
    // Index of the parameter that is displayed in timeline
    QModelIndex m_inTimelineIndex;
    mutable QReadWriteLock m_lock; // This is a lock that ensures safety in case of concurrent access

public:
    // this is to enable for range loops
    auto begin() -> decltype(m_parameters.begin()->second->begin()) { return m_parameters.begin()->second->begin(); }
    auto end() -> decltype(m_parameters.begin()->second->end()) { return m_parameters.begin()->second->end(); }
};

#endif
