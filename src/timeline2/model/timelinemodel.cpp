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


#include "timelinemodel.hpp"
#include "trackmodel.hpp"
#include "clipmodel.hpp"
#include "groupsmodel.hpp"

#include "doc/docundostack.hpp"

#include <klocalizedstring.h>
#include <QDebug>
#include <QModelIndex>
#include <mlt++/MltTractor.h>
#include <mlt++/MltProfile.h>

int TimelineModel::next_id = 0;

#define PUSH_UNDO(undo, redo, text)                                     \
    if (auto ptr = m_undoStack.lock()) {                                \
        ptr->push(new FunctionalUndoCommand(undo, redo, text));         \
    } else {                                                            \
        qDebug() << "ERROR : unable to access undo stack";              \
        Q_ASSERT(false);                                                \
    }


TimelineModel::TimelineModel(std::weak_ptr<DocUndoStack> undo_stack) :
    m_tractor(new Mlt::Tractor()),
    m_undoStack(undo_stack)
{
    Mlt::Profile profile;
    m_tractor->set_profile(profile);
}

TimelineModel::~TimelineModel()
{
    for(auto tracks : m_iteratorTable) {
        deleteTrackById(tracks.first);
    }
}



int TimelineModel::getTracksCount() const
{
    int count = m_tractor->count();
    Q_ASSERT(count >= 0);
    Q_ASSERT(count == static_cast<int>(m_allTracks.size()));
    return count;
}

int TimelineModel::getClipsCount() const
{
    return static_cast<int>(m_allClips.size());
}

int TimelineModel::getTrackClipsCount(int tid) const
{
    return getTrackById_const(tid)->getClipsCount();
}

void TimelineModel::deleteTrackById(int id)
{
    Q_ASSERT(m_iteratorTable.count(id) > 0);
    auto it = m_iteratorTable[id];
    (*it)->destruct();
}

void TimelineModel::deleteClipById(int id)
{
    Q_ASSERT(m_allClips.count(id) > 0);
    m_allClips[id]->destruct();
}

int TimelineModel::getClipTrackId(int cid) const
{
    Q_ASSERT(m_allClips.count(cid) > 0);
    const auto clip = m_allClips.at(cid);
    return clip->getCurrentTrackId();
}

int TimelineModel::getClipPosition(int cid) const
{
    Q_ASSERT(m_allClips.count(cid) > 0);
    const auto clip = m_allClips.at(cid);
    return clip->getPosition();
}

bool TimelineModel::requestClipMove(int cid, int tid, int position, bool updateView, Fun &undo, Fun &redo)
{
    std::function<bool (void)> local_undo = [](){return true;};
    std::function<bool (void)> local_redo = [](){return true;};
    bool ok = true;
    int old_tid = m_allClips[cid]->getCurrentTrackId();
    int old_clip_index = -1;
    if (old_tid != -1) {
        old_clip_index = getTrackById(old_tid)->getRowfromClip(cid);
        ok = getTrackById(old_tid)->requestClipDeletion(cid, local_undo, local_redo);
        if (!ok) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    ok = getTrackById(tid)->requestClipInsertion(cid, position, local_undo, local_redo);
    if (!ok) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        return false;
    }
    int new_clip_index = getTrackById(tid)->getRowfromClip(cid);
    auto operation = [cid, tid, old_tid, old_clip_index, new_clip_index, updateView, this]() {
        m_allClips[cid]->setCurrentTrackId(tid);
        if (updateView) {
            if (tid == old_tid) {
                qDebug() << "Data changed for clip"<<cid;
                QModelIndex modelIndex = makeClipIndexFromID(cid);
                notifyChange(modelIndex, modelIndex, true, false);
            } else {
                if (old_tid != -1){
                    qDebug() << "Removed row "<<old_clip_index;
                    _beginRemoveRows(makeTrackIndexFromID(old_tid), old_clip_index, old_clip_index);
                    _endRemoveRows();
                }
                qDebug() << "inserted row "<<new_clip_index;
                _beginInsertRows(makeTrackIndexFromID(tid), new_clip_index, new_clip_index);
                _endInsertRows();
            }
        }
        return true;
    };
    auto reverse = [cid, old_tid, this]() {
        m_allClips[cid]->setCurrentTrackId(old_tid);
        return true;
    };
    //push the operation and its reverse in the undo/redo
    UPDATE_UNDO_REDO(operation, reverse, local_undo, local_redo);
    //We have to expend the local undo with the data modification signals. They have to be sent after the real operations occured in the model so that the GUI can query the correct new values
    local_undo = [local_undo, tid, old_tid, cid, old_clip_index, new_clip_index, updateView, this]() {
        bool v = local_undo();
        if (updateView) {
            if (tid == old_tid) {
                QModelIndex modelIndex = makeClipIndexFromID(cid);
                notifyChange(modelIndex, modelIndex, true, false);
            } else {
                _beginRemoveRows(makeTrackIndexFromID(tid), new_clip_index, new_clip_index);
                _endRemoveRows();
                if (old_tid != -1) {
                    _beginInsertRows(makeTrackIndexFromID(old_tid), old_clip_index, old_clip_index);
                    _endInsertRows();
                }
            }
        }
        return v;
    };
    bool result = operation();
    if (result) {
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    }
    return result;
}

bool TimelineModel::requestClipMove(int cid, int tid, int position,  bool updateView, bool logUndo)
{
    Q_ASSERT(m_allClips.count(cid) > 0);
    if (m_allClips[cid]->getPosition() == position && m_allClips[cid]->getCurrentTrackId() == tid) {
        return true;
    }
    if (m_groups->getRootId(cid) != cid) {
        //element is in a group.
        int gid = m_groups->getRootId(cid);
        int current_tid = m_allClips[cid]->getCurrentTrackId();
        int track_pos1 = (int)std::distance(m_allTracks.begin(), m_iteratorTable[tid]);
        int track_pos2 = (int)std::distance(m_allTracks.begin(), m_iteratorTable[current_tid]);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_allClips[cid]->getPosition();
        return requestGroupMove(cid, gid, delta_track, delta_pos, updateView, logUndo);
    }
    std::function<bool (void)> undo = [](){return true;};
    std::function<bool (void)> redo = [](){return true;};
    bool res = requestClipMove(cid, tid, position, updateView, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move clip"));
    }
    for (const auto& ptr : m_allTracks) {
        ptr->checkConsistency();
    }
    return res;
}

int TimelineModel::suggestClipMove(int cid, int tid, int position)
{
    Q_ASSERT(isClip(cid));
    Q_ASSERT(isTrack(tid));
    int currentPos = getClipPosition(cid);
    int currentTrack = getClipTrackId(cid);
    if (currentPos == position || currentTrack != tid) {
        return position;
    }
    qDebug() << "Starting suggestion "<<cid << position << currentPos;
    //we check if move is possible
    Fun undo = [](){return true;};
    Fun redo = [](){return true;};
    bool possible = requestClipMove(cid, tid, position, false, undo, redo);
    qDebug() << "Original move success" << possible;
    if (possible) {
        undo();
        return position;
    }
    bool after = position > currentPos;
    int blank_length = getTrackById(tid)->getBlankSizeNearClip(cid, after);
    qDebug() << "Found blank" << blank_length;
    if (blank_length < INT_MAX) {
        if (after) {
            return currentPos + blank_length;
        } else {
            return currentPos - blank_length;
        }
    }
    return position;
}

bool TimelineModel::requestClipInsert(std::shared_ptr<Mlt::Producer> prod, int trackId, int position, int &id)
{
    int clipId = TimelineModel::getNextId();
    id = clipId;
    Fun undo = [clipId, this](){
        m_allClips[clipId]->destruct();
        return true;
    };
    Fun redo = [clipId, prod, this](){
        ClipModel::construct(shared_from_this(), prod, clipId);
        return true;
    };
    redo();
    bool res = requestClipMove(clipId, trackId, position, false, undo, redo);
    if (!res) {
        undo();
        id = -1;
        return false;
    }
    PUSH_UNDO(undo, redo, i18n("Insert Clip"));
    return true;
}

bool TimelineModel::requestGroupMove(int cid, int gid, int delta_track, int delta_pos, bool updateView, bool logUndo)
{
    std::function<bool (void)> undo = [](){return true;};
    std::function<bool (void)> redo = [](){return true;};
    Q_ASSERT(m_allGroups.count(gid) > 0);
    bool ok = true;
    auto all_clips = m_groups->getLeaves(gid);
    std::vector<int> sorted_clips(all_clips.begin(), all_clips.end());
    //we have to sort clip in an order that allows to do the move without self conflicts
    //If we move up, we move first the clips on the upper tracks (and conversely).
    //If we move left, we move first the leftmost clips (and conversely).
    std::sort(sorted_clips.begin(), sorted_clips.end(), [delta_track, delta_pos, this](int cid1, int cid2){
            int tid1 = m_allClips[cid1]->getCurrentTrackId();
            int tid2 = m_allClips[cid2]->getCurrentTrackId();
            int track_pos1 = (int)std::distance(m_allTracks.begin(), m_iteratorTable[tid1]);
            int track_pos2 = (int)std::distance(m_allTracks.begin(), m_iteratorTable[tid2]);
            if (tid1 == tid2) {
                int p1 = m_allClips[cid1]->getPosition();
                int p2 = m_allClips[cid2]->getPosition();
                return !(p1 <= p2) == !(delta_pos <= 0);
            }
            return !(track_pos1 <= track_pos2) == !(delta_track <= 0);
        });
    for (int clip : sorted_clips) {
        int current_track_id = m_allClips[clip]->getCurrentTrackId();
        int current_track_position = (int)std::distance(m_allTracks.begin(), m_iteratorTable[current_track_id]);
        int target_track_position = current_track_position + delta_track;
        if (target_track_position >= 0 && target_track_position < (int)m_allTracks.size()) {
            auto it = m_allTracks.cbegin();
            std::advance(it, target_track_position);
            int target_track = (*it)->getId();
            int target_position = m_allClips[clip]->getPosition() + delta_pos;
            ok = requestClipMove(clip, target_track, target_position, updateView || (clip != cid), undo, redo);
        } else {
            ok = false;
        }
        if (!ok) {
            bool undone = undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    if (logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move group"));
    }
    return true;
}



bool TimelineModel::requestClipResize(int cid, int size, bool right, bool logUndo)
{
    Fun undo = [](){return true;};
    Fun redo = [](){return true;};
    Fun update_model = [cid, right, this]() {
        if (getClipTrackId(cid) != -1) {
            QModelIndex modelIndex = makeClipIndexFromID(cid);
            if (right) {
                notifyChange(modelIndex, modelIndex, false, true);
            } else {
                notifyChange(modelIndex, modelIndex, true, true);
            }
        }
        return true;
    };
    bool result = m_allClips[cid]->requestResize(size, right, undo, redo);
    if (result) {
        PUSH_LAMBDA(update_model, undo);
        PUSH_LAMBDA(update_model, redo);
        update_model();
        if (logUndo) {
            PUSH_UNDO(undo, redo, i18n("Resize clip"));
        }
    }
    return result;
}

bool TimelineModel::requestClipTrim(int cid, int delta, bool right, bool ripple, bool logUndo)
{
    return requestClipResize(cid, m_allClips[cid]->getPlaytime() - delta, right, logUndo);
}

bool TimelineModel::requestGroupClips(const std::unordered_set<int>& ids)
{
    std::function<bool (void)> undo = [](){return true;};
    std::function<bool (void)> redo = [](){return true;};
    int gid = m_groups->groupItems(ids, undo, redo);
    if (gid != -1) {
        PUSH_UNDO(undo, redo, i18n("Group clips"));
    }
    return (gid != -1);
}

bool TimelineModel::requestUngroupClip(int id)
{
    std::function<bool (void)> undo = [](){return true;};
    std::function<bool (void)> redo = [](){return true;};
    bool result = m_groups->ungroupItem(id, undo, redo);
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Ungroup clips"));
    }
    return result;
}

void TimelineModel::registerTrack(std::unique_ptr<TrackModel>&& track, int pos)
{
    int id = track->getId();
    if (pos == -1) {
        pos = static_cast<int>(m_allTracks.size());
    }
    Q_ASSERT(pos >= 0);
    Q_ASSERT(pos <= static_cast<int>(m_allTracks.size()));

    //effective insertion (MLT operation)
    int error = m_tractor->insert_track(*track ,pos);
    Q_ASSERT(error == 0); //we might need better error handling...

    // we now insert in the list
    auto posIt = m_allTracks.begin();
    std::advance(posIt, pos);
    auto it = m_allTracks.insert(posIt, std::move(track));
    //it now contains the iterator to the inserted element, we store it
    Q_ASSERT(m_iteratorTable.count(id) == 0); //check that id is not used (shouldn't happen)
    m_iteratorTable[id] = it;
}

void TimelineModel::registerClip(std::shared_ptr<ClipModel> clip)
{
    int id = clip->getId();
    Q_ASSERT(m_allClips.count(id) == 0);
    m_allClips[id] = clip;
    m_groups->createGroupItem(id);
}

void TimelineModel::registerGroup(int groupId)
{
    Q_ASSERT(m_allGroups.count(groupId) == 0);
    m_allGroups.insert(groupId);
}

void TimelineModel::deregisterTrack(int id)
{
    auto it = m_iteratorTable[id]; //iterator to the element
    m_iteratorTable.erase(id);  //clean table
    auto index = std::distance(m_allTracks.begin(), it); //compute index in list
    m_tractor->remove_track(static_cast<int>(index)); //melt operation
    m_allTracks.erase(it);  //actual deletion of object
}

void TimelineModel::deregisterClip(int id)
{
    Q_ASSERT(m_allClips.count(id) > 0);
    //TODO this is temporary while UNDO for clip deletion is not implemented
    std::function<bool (void)> undo = [](){return true;};
    std::function<bool (void)> redo = [](){return true;};
    int tid = m_allClips[id]->getCurrentTrackId();
    if (tid != -1) {
        getTrackById(m_allClips[id]->getCurrentTrackId())->requestClipDeletion(id, undo, redo);
    }
    m_allClips.erase(id);
    m_groups->destructGroupItem(id, true, undo, redo);
}

void TimelineModel::deregisterGroup(int id)
{
    Q_ASSERT(m_allGroups.count(id) > 0);
    m_allGroups.erase(id);
}

std::unique_ptr<TrackModel>& TimelineModel::getTrackById(int tid)
{
    Q_ASSERT(m_iteratorTable.count(tid) > 0);
    return *m_iteratorTable[tid];
}

const std::unique_ptr<TrackModel>& TimelineModel::getTrackById_const(int tid) const
{
    Q_ASSERT(m_iteratorTable.count(tid) > 0);
    return *m_iteratorTable.at(tid);
}

std::shared_ptr<ClipModel> TimelineModel::getClipPtr(int cid) const
{
    Q_ASSERT(m_allClips.count(cid) > 0);
    return m_allClips.at(cid);
}

int TimelineModel::getNextId()
{
    return TimelineModel::next_id++;
}

bool TimelineModel::isClip(int id) const
{
    return m_allClips.count(id) > 0;
}

bool TimelineModel::isTrack(int id) const
{
    return m_iteratorTable.count(id) > 0;
}

int TimelineModel::duration() const
{
    return m_tractor->get_playtime();
}

