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

#ifndef TIMELINEMODEL_H
#define TIMELINEMODEL_H

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mlt++/MltTractor.h>
#include "undohelper.hpp"
#include <QReadWriteLock>
#include <QAbstractItemModel>


#define LOGGING 1 //If set to 1, we log the actions requested to the timeline as a reproducer script
#ifdef LOGGING
#include <fstream>
#endif

class TrackModel;
class ClipModel;
class CompositionModel;
class GroupsModel;
class DocUndoStack;
class SnapModel;
class TimelineItemModel;

/* @brief This class represents a Timeline object, as viewed by the backend.
   In general, the Gui associated with it will send modification queries (such as resize or move), and this class authorize them or not depending on the validity of the modifications.

   This class also serves to keep track of all objects. It holds pointers to all tracks and clips, and gives them unique IDs on creation. These Ids are used in any interactions with the objects and have nothing to do with Melt IDs.

   This is the entry point for any modifications that has to be made on an element. The dataflow beyond this entry point may vary, for example when the user request a clip resize, the call is deferred to the clip itself, that check if there is enough data to extend by the requested amount, compute the new in and out, and then asks the track if there is enough room for extension. To avoid any confusion on which function to call first, rembember to always call the version in timeline. This is also required to generate the Undo/Redo operators

   Generally speaking, we don't check ahead of time if an action is going to succeed or not before applying it.
   We just apply it naively, and if it fails at some point, we use the undo operator that we are constructing on the fly to revert what we have done so far.
   For example, when we move a group of clips, we apply the move operation to all the clips inside this group (in the right order). If none fails, we are good, otherwise we revert what we've already done.
   This kind of behaviour frees us from the burden of simulating the actions before actually applying theme. This is a good thing because this simulation step would be very sensitive to corruptions and small discrepancies, which we try to avoid at all cost.


   It derives from AbstractItemModel (indirectly through TimelineItemModel) to provide the model to the QML interface. An itemModel is organized with row and columns that contain the data. It can be hierarchical, meaning that a given index (row,column) can contain another level of rows and column.
   Our organization is as follows: at the top level, each row contains a track. These rows are in the same order as in the actual timeline.
   Then each of this row contains itself sub-rows that correspond to the clips.
   Here the order of these sub-rows is unrelated to the chronological order of the clips,
   but correspond to their Id order. For example, if you have three clips, with ids 12, 45 and 150, they will receive row index 0,1 and 2.
   This is because the order actually doesn't matter since the clips are rendered based on their positions rather than their row order.
   The id order has been choosed because it is consistant with a valid ordering of the clips.
   The columns are never used, so the data is always in column 0

   An ModelIndex in the ItemModel consists of a row number, a column number, and a parent index. In our case, tracks have always an empty parent, and the clip have a track index as parent.
   A ModelIndex can also store one additional integer, and we exploit this feature to store the unique ID of the object it corresponds to. 

*/
class TimelineModel : public QAbstractItemModel, public std::enable_shared_from_this<TimelineModel>
{
    Q_OBJECT

protected:
    /* @brief this constructor should not be called. Call the static construct instead
     */
    TimelineModel(Mlt::Profile *profile, std::weak_ptr<DocUndoStack> undo_stack);

public:
    friend class TrackModel;
    friend class ClipModel;
    friend class CompositionModel;
    friend class GroupsModel;

    virtual ~TimelineModel();
    Mlt::Tractor* tractor() const { return m_tractor.get(); }
    Mlt::Profile *getProfile();

    /* @brief returns the number of tracks */
    int getTracksCount() const;

    /* @brief returns the number of clips */
    int getClipsCount() const;

    /* @brief returns the number of compositions */
    int getCompositionsCount() const;

    /* @brief Returns the id of the track containing clip (-1 if it is not inserted)
       @param clipId Id of the clip to test
     */
    Q_INVOKABLE int getClipTrackId(int clipId) const;

    /* @brief Returns the position of clip (-1 if it is not inserted)
       @param clipId Id of the clip to test
    */
    Q_INVOKABLE int getClipPosition(int clipId) const;

    /* @brief Returns the duration of a clip
       @param clipId Id of the clip to test
    */
    int getClipPlaytime(int clipId) const;

    /* @brief Returns the number of clips in a given track
       @param trackId Id of the track to test
    */
    int getTrackClipsCount(int trackId) const;
    int getTrackCompositionsCount(int trackId) const;

    /* @brief Returns the position of the track in the order of the tracks
       @param trackId Id of the track to test
    */
    int getTrackPosition(int trackId) const;

    /* @brief Move a clip to a specific position
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       If the clip is not in inserted in a track yet, it gets inserted for the first time.
       If the clip is in a group, the call is deferred to requestGroupMove
       @param clipId is the ID of the clip
       @param trackId is the ID of the target track
       @param position is the position where we want to move
       @param updateView if set to false, no signal is sent to qml
       @param logUndo if set to false, no undo object is stored
    */
    Q_INVOKABLE bool requestClipMove(int clipId, int trackId, int position, bool updateView = true, bool logUndo = true);

    /* @brief Move a composition to a specific position
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       If the clip is not in inserted in a track yet, it gets inserted for the first time.
       If the clip is in a group, the call is deferred to requestGroupMove
       @param transid is the ID of the composition
       @param trackId is the ID of the track
    */
    Q_INVOKABLE bool requestCompositionMove(int compoId, int trackId, int position, bool updateView = true, bool logUndo = true);

    Q_INVOKABLE int getCompositionTrackId(int compoId) const;
    Q_INVOKABLE int getCompositionPosition(int compoId) const;
    Q_INVOKABLE int getCompositionPlaytime(int compoId) const;
    Q_INVOKABLE int suggestCompositionMove(int compoId, int trackId, int position);

protected:
    /* Same function, but accumulates undo and redo, and doesn't check for group*/
    bool requestClipMove(int clipId, int trackId, int position, bool updateView, Fun &undo, Fun &redo);
    bool requestCompositionMove(int transid, int trackId, int position, bool updateView, Fun &undo, Fun &redo);
public:

    /* @brief Given an intended move, try to suggest a more valid one (accounting for snaps and missing UI calls)
       @param clipId id of the clip to move
       @param trackId id of the target track
       @param position target position of the clip
     */
    Q_INVOKABLE int suggestClipMove(int clipId, int trackId, int position);

    /* @brief Request clip insertion at given position.
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param prod Producer of the element to insert
       @param track Id of the track where to insert
       @param Requested position
       @param ID return parameter of the id of the inserted clip
       @param logUndo if set to false, no undo object is stored
    */
    bool requestClipInsertion(std::shared_ptr<Mlt::Producer> prod, int trackId, int position, int &id, bool logUndo = true);
    /* Same function, but accumulates undo and redo*/
    bool requestClipInsertion(std::shared_ptr<Mlt::Producer> prod, int trackId, int position, int &id, Fun& undo, Fun& redo);

    /* @brief Deletes the given clip or composition from the timeline
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       If the clip/composition is in a group, the call is deferred to requestGroupDeletion
       @param clipId is the ID of the clip/composition
       @param logUndo if set to false, no undo object is stored
    */
    Q_INVOKABLE bool requestItemDeletion(int clipId, bool logUndo = true);
    /* Same function, but accumulates undo and redo, and doesn't check for group*/
    bool requestClipDeletion(int clipId, Fun &undo, Fun &redo);
    bool requestCompositionDeletion(int compositionId, Fun& undo, Fun& redo);

    /* @brief Move a group to a specific position
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       If the clips in the group are not in inserted in a track yet, they get inserted for the first time.
       @param clipId is the id of the clip that triggers the group move
       @param groupId is the id of the group
       @param delta_track is the delta applied to the track index
       @param delta_pos is the requested position change
       @param updateView if set to false, no signal is sent to qml for the clip clipId
       @param logUndo if set to true, an undo object is created
    */
    bool requestGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView = true, bool logUndo = true);

    /* @brief Deletes all clips inside the group that contains the given clip.
       This action is undoable
       Note that if their is a hierarchy of groups, all of them will be deleted.
       Returns true on success. If it fails, nothing is modified.
       @param clipId is the id of the clip that triggers the group deletion
    */
    bool requestGroupDeletion(int clipId);

    /* @brief Change the duration of a clip
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param clipId is the ID of the clip
       @param size is the new size of the clip
       @param right is true if we change the right side of the clip, false otherwise
       @param logUndo if set to true, an undo object is created
       @param snap if set to true, the resize order will be coerced to use the snapping grid
    */
    Q_INVOKABLE bool requestClipResize(int clipId, int size, bool right, bool logUndo = true, bool snap = false);

    /* @brief Similar to requestClipResize but takes a delta instead of absolute size
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param clipId is the ID of the clip
       @param delta is the delta to be applied to the length of the clip
       @param right is true if we change the right side of the clip, false otherwise
       @param ripple TODO document this
       @param test_only if set to true, the undo is not created and no signal is sent to qml
     */
    bool requestClipTrim(int clipId, int delta, bool right, bool ripple = false, bool test_only = false);

    /* @brief Group together a set of ids
       The ids are either a group ids or clip ids. The involved clip must already be inserted in a track
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       Typically, ids would be ids of clips, but for convenience, some of them can be ids of groups as well.
       @param ids Set of ids to group
    */
    bool requestClipsGroup(const std::unordered_set<int>& ids);

    /* @brief Destruct the topmost group containing clip
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param id of the clip to degroup (all clips belonging to the same group will be ungrouped as well)
    */
    bool requestClipUngroup(int id);
    /* Same function, but accumulates undo and redo*/
    bool requestClipUngroup(int id, Fun& undo, Fun& redo);

    /* @brief Create a track at given position
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param Requested position (order). If set to -1, the track is inserted last.
       @param id is a return parameter that holds the id of the resulting track (-1 on failure)
    */
    bool requestTrackInsertion(int pos, int& id);
    /* Same function, but accumulates undo and redo*/
    bool requestTrackInsertion(int pos, int& id, Fun& undo, Fun& redo);

    /* @brief Delete track with given id
       This also deletes all the clips contained in the track.
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param trackId id of the track to delete
    */
    bool requestTrackDeletion(int trackId);
    /* Same function, but accumulates undo and redo*/
    bool requestTrackDeletion(int trackId, Fun& undo, Fun& redo);

    /* @brief Get project duration
       Returns the duration in frames
    */
    int duration() const;

    /* @brief Get all the elements of the same group as the given clip.
       If there is a group hierarchy, only the topmost group is considered.
       @param clipId id of the clip to test
    */
    std::unordered_set<int> getGroupElements(int clipId);

    /* @brief Removes all the elements on the timeline (tracks and clips)
     */
    bool requestReset(Fun& undo, Fun& redo);
    /* @brief Updates the current the pointer to the current undo_stack
       Must be called for example when the doc change
    */
    void setUndoStack(std::weak_ptr<DocUndoStack> undo_stack);

    /* @brief Requests the best snapped position for a clip
       @param pos is the clip's requested position
       @param length is the clip's duration
       @param pts snap points to ignore (for example currently moved clip)
       @returns best snap position or -1 if no snap point is near
     */
    int requestBestSnapPos(int pos, int length, const std::vector<int>& pts = std::vector<int>());

    /* @brief Requests the next snapped point
       @param pos is the current position
     */
    int requestNextSnapPos(int pos);

    /* @brief Requests the previous snapped point
       @param pos is the current position
     */
    int requestPreviousSnapPos(int pos);

    /* @brief Request composition insertion at given position.
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param transitionId Identifier of the Mlt transition to insert (as given by repository)
       @param track Id of the track where to insert
       @param position Requested position
       @param id return parameter of the id of the inserted composition
       @param logUndo if set to false, no undo object is stored
    */
    bool requestCompositionInsertion(const QString& transitionId, int trackId, int position, int &id, bool logUndo = true);
    /* Same function, but accumulates undo and redo*/
    bool requestCompositionInsertion(const QString& transitionId, int trackId, int position, int &id, Fun& undo, Fun& redo);


protected:
    /* @brief Register a new track. This is a call-back meant to be called from TrackModel
       @param pos indicates the number of the track we are adding. If this is -1, then we add at the end.
     */
    void registerTrack(std::shared_ptr<TrackModel> track, int pos = -1);

    /* @brief Register a new clip. This is a call-back meant to be called from ClipModel
    */
    void registerClip(std::shared_ptr<ClipModel> clip);

    /* @brief Register a new composition. This is a call-back meant to be called from CompositionModel
    */
    void registerComposition(std::shared_ptr<CompositionModel> composition);

    /* @brief Register a new group. This is a call-back meant to be called from GroupsModel
     */
    void registerGroup(int groupId);

    /* @brief Deregister and destruct the track with given id.
       @parame updateView Whether to send updates to the model. Must be false when called from a constructor/destructor
     */
    Fun deregisterTrack_lambda(int id, bool updateView = false);

    /* @brief Return a lambda that deregisters and destructs the clip with given id.
       Note that the clip must already be deleted from its track and groups.
     */
    Fun deregisterClip_lambda(int id);

    /* @brief Return a lambda that deregisters and destructs the composition with given id.
     */
    Fun deregisterComposition_lambda(int compoId);

    /* @brief Deregister a group with given id
     */
    void deregisterGroup(int id);

    /* @brief Helper function to get a pointer to the track, given its id
     */
    std::shared_ptr<TrackModel> getTrackById(int trackId);
    const std::shared_ptr<TrackModel> getTrackById_const(int trackId) const;

    /*@brief Helper function to get a pointer to a clip, given its id*/
    std::shared_ptr<ClipModel> getClipPtr(int clipId) const;

    /*@brief Helper function to get a pointer to a composition, given its id*/
    std::shared_ptr<CompositionModel> getCompositionPtr(int compoId) const;

    /* @brief Returns next valid unique id to create an object
     */
    static int getNextId();

    /* @brief Helper function that returns true if the given ID corresponds to a clip
     */
    bool isClip(int id) const;

    /* @brief Helper function that returns true if the given ID corresponds to a composition
     */
    bool isComposition(int id) const;

    /* @brief Helper function that returns true if the given ID corresponds to a track
     */
    bool isTrack(int id) const;

    /* @brief Helper function that returns true if the given ID corresponds to a track
     */
    bool isGroup(int id) const;

    void plantComposition(Mlt::Transition &tr, int a_track, int b_track);
    bool removeComposition(int compoId, int pos);

protected:
    std::unique_ptr<Mlt::Tractor> m_tractor;

    std::list<std::shared_ptr<TrackModel>> m_allTracks;

    std::unordered_map<int, std::list<std::shared_ptr<TrackModel>>::iterator> m_iteratorTable; //this logs the iterator associated which each track id. This allows easy access of a track based on its id.

    std::unordered_map<int, std::shared_ptr<ClipModel>> m_allClips; //the keys are the clip id, and the values are the corresponding pointers

    std::unordered_map<int, std::shared_ptr<CompositionModel>> m_allCompositions; //the keys are the composition id, and the values are the corresponding pointers

    static int next_id;//next valid id to assign

    std::unique_ptr<GroupsModel> m_groups;
    std::unique_ptr<SnapModel> m_snaps;

    std::unordered_set<int> m_allGroups; //ids of all the groups

    std::weak_ptr<DocUndoStack> m_undoStack;

    Mlt::Profile *m_profile;

    // The black track producer. It's length / out should always be adjusted to the projects's length
    std::unique_ptr<Mlt::Producer> m_blackClip;

    mutable QReadWriteLock m_lock; //This is a lock that ensures safety in case of concurrent access

    std::ofstream m_logFile; //this is a temporary debug member to help reproduce issues

    //what follows are some virtual function that corresponds to the QML. They are implemented in TimelineItemModel
protected:
    virtual void _beginRemoveRows(const QModelIndex&, int , int) = 0;
    virtual void _beginInsertRows(const QModelIndex&, int , int) = 0;
    virtual void _endRemoveRows() = 0;
    virtual void _endInsertRows() = 0;
    virtual void notifyChange(const QModelIndex& topleft, const QModelIndex& bottomright, bool start, bool duration, bool updateThumb) = 0;
    virtual QModelIndex makeClipIndexFromID(int) const = 0;
    virtual QModelIndex makeCompositionIndexFromID(int) const = 0;
    virtual QModelIndex makeTrackIndexFromID(int) const = 0;
    virtual void _resetView() = 0;
};
#endif

