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
#include <unordered_map>
#include <unordered_set>
#include <mlt++/MltTractor.h>
#include "undohelper.hpp"

class TrackModel;
class ClipModel;
class GroupsModel;
class DocUndoStack;

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
class TimelineModel : public std::enable_shared_from_this<TimelineModel>
{

protected:
    /* @brief this constructor should not be called. Call the static construct instead
     */
    TimelineModel(std::weak_ptr<DocUndoStack> undo_stack);

public:
    friend class TrackModel;
    friend class ClipModel;
    friend class GroupsModel;

    virtual ~TimelineModel();
    Mlt::Tractor* tractor() const { return m_tractor.get(); }

    /* @brief returns the number of tracks */
    int getTracksCount() const;

    /* @brief returns the number of clips */
    int getClipsCount() const;

    /* @brief Delete track based on its id */
    void deleteTrackById(int id);

    /* @brief Delete clip based on its id */
    void deleteClipById(int id);

    /* @brief Returns the id of the track containing clip (-1 if it is not inserted)
       @param cid Id of the clip to test
     */
    int getClipTrackId(int cid) const;

    /* @brief Returns the position of clip (-1 if it is not inserted)
       @param cid Id of the clip to test
    */
    int getClipPosition(int cid) const;

    /* @brief Returns the number of clips in a given track
       @param tid Id of the track to test
    */
    int getTrackClipsCount(int tid) const;

    /* @brief Move a clip to a specific position
       Returns true on success. If it fails, nothing is modified.
       If the clip is not in inserted in a track yet, it gets inserted for the first time.
       @param cid is the ID of the clip
       @param tid is the ID of the target track
       @param position is the position where we want to move
       @param updateView if set to false, no signal is sent to qml
       @param logUndo if set to false, no undo object is stored
    */
    bool requestClipMove(int cid, int tid, int position, bool updateView = true, bool logUndo = true);
protected:
    /* Same function, but accumulates undo and redo, and doesn't check for group*/
    bool requestClipMove(int cid, int tid, int position, bool updateView, Fun &undo, Fun &redo);
public:


    /* @brief Request clip insertion at given position.
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param prod Producer of the element to insert
       @param track Id of the track where to insert
       @param Requested position
       @param ID return parameter of the id of the inserted clip
    */
    bool requestClipInsert(std::shared_ptr<Mlt::Producer> prod, int trackId, int position, int &id);

    /* @brief Move a group to a specific position
       Returns true on success. If it fails, nothing is modified.
       If the clips in the group are not in inserted in a track yet, they get inserted for the first time.
       @param cid is the id of the clip that triggers the group move
       @param gid is the id of the group
       @param delta_track is the delta applied to the track index
       @param delta_pos is the requested position change
       @param updateView if set to false, no signal is sent to qml for the clip cid
       @param logUndo if set to true, an undo object is created
    */
    bool requestGroupMove(int cid, int gid, int delta_track, int delta_pos, bool updateView = true, bool logUndo = true);

    /* @brief Change the duration of a clip
       Returns true on success. If it fails, nothing is modified.
       @param cid is the ID of the clip
       @param size is the new size of the clip
       @param right is true if we change the right side of the clip, false otherwise
       @param logUndo if set to true, an undo object is created
    */
    bool requestClipResize(int cid, int size, bool right, bool logUndo = true);

    /* @brief Similar to requestClipResize but takes a delta instead of absolute size
       Returns true on success. If it fails, nothing is modified.
       @param cid is the ID of the clip
       @param delta is the delta to be applied to the length of the clip
       @param right is true if we change the right side of the clip, false otherwise
       @param ripple TODO document this
       @param test_only if set to true, the undo is not created and no signal is sent to qml
     */
    bool requestClipTrim(int cid, int delta, bool right, bool ripple = false, bool test_only = false);

    /* @brief Group together a set of ids
       Returns true on success. If it fails, nothing is modified.
       Typically, ids would be ids of clips, but for convenience, some of them can be ids of groups as well.
       @param ids Set of ids to group
    */
    bool requestGroupClips(const std::unordered_set<int>& ids);

    /* @brief Destruct the topmost group containing clip
       Returns true on success. If it fails, nothing is modified.
       @param id of the clip to degroup (all clips belonging to the same group will be ungrouped as well)
    */
    bool requestUngroupClip(int id);

    /* @brief Get project duration
       Returns the duration in frames
    */
    int duration() const;

protected:
    /* @brief Register a new track. This is a call-back meant to be called from TrackModel
       @param pos indicates the number of the track we are adding. If this is -1, then we add at the end.
     */
    void registerTrack(std::unique_ptr<TrackModel>&& track, int pos = -1);

    /* @brief Register a new track. This is a call-back meant to be called from ClipModel
    */
    void registerClip(std::shared_ptr<ClipModel> clip);

    /* @brief Register a new group. This is a call-back meant to be called from GroupsModel
     */
    void registerGroup(int groupId);

    /* @brief Deregister and destruct the track with given id.
     */
    void deregisterTrack(int id);

    /* @brief Deregister and destruct the clip with given id.
     */
    void deregisterClip(int id);

    /* @brief Deregister a group with given id
     */
    void deregisterGroup(int id);

    /* @brief Helper function to get a pointer to the track, given its id
     */
    std::unique_ptr<TrackModel>& getTrackById(int tid);
    const std::unique_ptr<TrackModel>& getTrackById_const(int tid) const;

    /*@brief Helper function to get a pointer to a clip, given its id*/
    std::shared_ptr<ClipModel> getClipPtr(int cid) const;

    /* @brief Returns next valid unique id to create an object
     */
    static int getNextId();

    /* @brief Helper function that returns true if the given ID correspond to a clip
     */
    bool isClip(int id) const;

    /* @brief Helper function that returns true if the given ID correspond to a track
     */
    bool isTrack(int id) const;
protected:
    std::unique_ptr<Mlt::Tractor> m_tractor;

    std::list<std::unique_ptr<TrackModel>> m_allTracks;

    std::unordered_map<int, std::list<std::unique_ptr<TrackModel>>::iterator> m_iteratorTable; //this logs the iterator associated which each track id. This allows easy access of a track based on its id.

    std::unordered_map<int, std::shared_ptr<ClipModel>> m_allClips; //the keys are the clip id, and the values are the corresponding pointers

    static int next_id;//next valid id to assign

    std::unique_ptr<GroupsModel> m_groups;

    std::unordered_set<int> m_allGroups; //ids of all the groups

    std::weak_ptr<DocUndoStack> m_undoStack;


    //what follows are some virtual function that corresponds to the QML. They are implemented in TimelineItemModel
protected:
    virtual void _beginRemoveRows(const QModelIndex&, int , int) = 0;
    virtual void _beginInsertRows(const QModelIndex&, int , int) = 0;
    virtual void _endRemoveRows() = 0;
    virtual void _endInsertRows() = 0;
    virtual void notifyChange(const QModelIndex& topleft, const QModelIndex& bottomright, bool start, bool duration) = 0;
    virtual QModelIndex makeClipIndexFromID(int) const = 0;
    virtual QModelIndex makeTrackIndexFromID(int) const = 0;
};
#endif

