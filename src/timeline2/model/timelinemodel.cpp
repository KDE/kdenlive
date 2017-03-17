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
#include "transitionmodel.hpp"
#include "groupsmodel.hpp"
#include "snapmodel.hpp"

#include "doc/docundostack.hpp"

#include <klocalizedstring.h>
#include <QDebug>
#include <QModelIndex>
#include <mlt++/MltTractor.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTransition.h>
#include <mlt++/MltField.h>
#include <queue>
#ifdef LOGGING
#include <sstream>
#endif

#include "macros.hpp"

int TimelineModel::next_id = 0;



TimelineModel::TimelineModel(Mlt::Profile *profile, std::weak_ptr<DocUndoStack> undo_stack) :
    QAbstractItemModel(),
    m_tractor(new Mlt::Tractor(*profile)),
    m_snaps(new SnapModel()),
    m_undoStack(undo_stack),
    m_profile(profile),
    m_blackClip(new Mlt::Producer(*profile,"color:black")),
    m_lock(QReadWriteLock::Recursive)
{
    // Create black background track
    m_blackClip->set("id", "black_track");
    m_blackClip->set("mlt_type", "producer");
    m_blackClip->set("aspect_ratio", 1);
    m_blackClip->set("set.test_audio", 0);
    m_tractor->insert_track(*m_blackClip, 0);

#ifdef LOGGING
    m_logFile = std::ofstream("log.txt");
    m_logFile << "TEST_CASE(\"Regression\") {"<<std::endl;
    m_logFile << "Mlt::Profile profile;"<<std::endl;
    m_logFile << "std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);" <<std::endl;
    m_logFile << "std::shared_ptr<TimelineModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);" <<std::endl;
    m_logFile << "TimelineModel::next_id = 0;" <<std::endl;
    m_logFile << "int dummy_id;" <<std::endl;
#endif
}

TimelineModel::~TimelineModel()
{
    //Remove black background
    //m_tractor->remove_track(0);
    std::vector<int> all_ids;
    for(auto tracks : m_iteratorTable) {
        all_ids.push_back(tracks.first);
    }
    for(auto tracks : all_ids) {
        deregisterTrack_lambda(tracks, false)();
    }
}

int TimelineModel::getTracksCount() const
{
    READ_LOCK();
    int count = m_tractor->count();
    Q_ASSERT(count >= 0);
    // don't count the black background track
    Q_ASSERT(count -1 == static_cast<int>(m_allTracks.size()));
    return count - 1;
}

int TimelineModel::getClipsCount() const
{
    READ_LOCK();
    int size = int(m_allClips.size());
    return size;
}


int TimelineModel::getClipTrackId(int cid) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(cid) > 0);
    const auto clip = m_allClips.at(cid);
    int tid = clip->getCurrentTrackId();
    return tid;
}

int TimelineModel::getClipPosition(int cid) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(cid) > 0);
    const auto clip = m_allClips.at(cid);
    int pos = clip->getPosition();
    return pos;
}

int TimelineModel::getClipPlaytime(int cid) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(cid) > 0);
    const auto clip = m_allClips.at(cid);
    int playtime = clip->getPlaytime();
    return playtime;
}

int TimelineModel::getTrackClipsCount(int tid) const
{
    READ_LOCK();
    int count = getTrackById_const(tid)->getClipsCount();
    return count;
}

int TimelineModel::getTrackPosition(int tid) const
{
    READ_LOCK();
    Q_ASSERT(m_iteratorTable.count(tid) > 0);
    auto it = m_allTracks.begin();
    int pos = (int)std::distance(it, (decltype(it))m_iteratorTable.at(tid));
    return pos;
}

bool TimelineModel::requestClipMove(int cid, int tid, int position, bool updateView, Fun &undo, Fun &redo)
{
    Q_ASSERT(isClip(cid));
    std::function<bool (void)> local_undo = [](){return true;};
    std::function<bool (void)> local_redo = [](){return true;};
    bool ok = true;
    int old_tid = getClipTrackId(cid);
    if (old_tid != -1) {
        ok = getTrackById(old_tid)->requestClipDeletion(cid, updateView, local_undo, local_redo);
        if (!ok) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    ok = getTrackById(tid)->requestClipInsertion(cid, position, updateView, local_undo, local_redo);
    if (!ok) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestClipMove(int cid, int tid, int position,  bool updateView, bool logUndo)
{
#ifdef LOGGING
    m_logFile << "timeline->requestClipMove("<<cid<<","<<tid<<" ,"<<position<<", "<<(updateView ? "true" : "false")<<", "<<(logUndo ? "true" : "false")<<" ); " <<std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allClips.count(cid) > 0);
    if (m_allClips[cid]->getPosition() == position && getClipTrackId(cid) == tid) {
        return true;
    }
    if (m_groups->isInGroup(cid)) {
        //element is in a group.
        int gid = m_groups->getRootId(cid);
        int current_tid = getClipTrackId(cid);
        int track_pos1 = getTrackPosition(tid);
        int track_pos2 = getTrackPosition(current_tid);
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
    return res;
}

int TimelineModel::suggestClipMove(int cid, int tid, int position)
{
#ifdef LOGGING
    m_logFile << "timeline->suggestClipMove("<<cid<<","<<tid<<" ,"<<position<<"); " <<std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isClip(cid));
    Q_ASSERT(isTrack(tid));
    int currentPos = getClipPosition(cid);
    int currentTrack = getClipTrackId(cid);
    if (currentPos == position || currentTrack != tid) {
        return position;
    }

    //For snapping, we must ignore all in/outs of the clips of the group being moved
    std::vector<int> ignored_pts;
    if (m_groups->isInGroup(cid)) {
        int gid = m_groups->getRootId(cid);
        auto all_clips = m_groups->getLeaves(gid);
        for (int current_cid : all_clips) {
            int in = getClipPosition(current_cid);
            int out = in + getClipPlaytime(current_cid) - 1;
            ignored_pts.push_back(in);
            ignored_pts.push_back(out);
        }
    } else {
        int in = getClipPosition(cid);
        int out = in + getClipPlaytime(cid) - 1;
        ignored_pts.push_back(in);
        ignored_pts.push_back(out);
    }

    int snapped = requestBestSnapPos(position, m_allClips[cid]->getPlaytime(), ignored_pts);
    qDebug() << "Starting suggestion "<<cid << position << currentPos << "snapped to "<<snapped;
    if (snapped >= 0) {
        position = snapped;
    }
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

int TimelineModel::suggestTransitionMove(int cid, int tid, int position)
{
#ifdef LOGGING
    m_logFile << "timeline->suggestTransitionMove("<<cid<<","<<tid<<" ,"<<position<<"); " <<std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isTransition(cid));
    Q_ASSERT(isTrack(tid));
    int currentPos = getTransitionPosition(cid);
    int currentTrack = getTransitionTrackId(cid);
    if (currentPos == position || currentTrack != tid) {
        return position;
    }

    //For snapping, we must ignore all in/outs of the clips of the group being moved
    std::vector<int> ignored_pts;
    if (m_groups->isInGroup(cid)) {
        int gid = m_groups->getRootId(cid);
        auto all_clips = m_groups->getLeaves(gid);
        for (int current_cid : all_clips) {
            //TODO: fix for transition
            int in = getClipPosition(current_cid);
            int out = in + getClipPlaytime(current_cid) - 1;
            ignored_pts.push_back(in);
            ignored_pts.push_back(out);
        }
    } else {
        int in = currentPos;
        int out = in + getTransitionPlaytime(cid) - 1;
        qDebug()<<" * ** IGNORING SNAP PTS: "<<in<<"-"<<out;
        ignored_pts.push_back(in);
        ignored_pts.push_back(out);
    }

    int snapped = requestBestSnapPos(position, m_allTransitions[cid]->getPlaytime(), ignored_pts);
    qDebug() << "Starting suggestion "<<cid << position << currentPos << "snapped to "<<snapped;
    if (snapped >= 0) {
        position = snapped;
    }
    //we check if move is possible
    Fun undo = [](){return true;};
    Fun redo = [](){return true;};
    bool possible = requestTransitionMove(cid, tid, position, false, undo, redo);
    qDebug() << "Original move success" << possible;
    if (possible) {
        undo();
        return position;
    }
    bool after = position > currentPos;
    int blank_length = getTrackById(tid)->getBlankSizeNearTransition(cid, after);
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

bool TimelineModel::requestClipInsertion(std::shared_ptr<Mlt::Producer> prod, int trackId, int position, int &id, bool logUndo)
{
#ifdef LOGGING
    m_logFile << "{" <<std::endl<< "std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile, \"color\", \"red\");" << std::endl;
    m_logFile << "producer->set(\"length\", "<<prod->get_playtime()<<");" << std::endl;
    m_logFile << "producer->set(\"out\", "<<prod->get_playtime()-1<<");" << std::endl;
    m_logFile << "timeline->requestClipInsertion(producer,"<<trackId<<" ,"<<position<<", dummy_id );" <<std::endl;
    m_logFile<<"}"<<std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Fun undo = [](){return true;};
    Fun redo = [](){return true;};
    bool result = requestClipInsertion(prod, trackId, position, id, undo, redo);
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Insert Clip"));
    }
    return result;
}

bool TimelineModel::requestClipInsertion(std::shared_ptr<Mlt::Producer> prod, int trackId, int position, int &id, Fun& undo, Fun& redo)
{
    int clipId = TimelineModel::getNextId();
    id = clipId;
    Fun local_undo = deregisterClip_lambda(clipId);
    ClipModel::construct(shared_from_this(), prod, clipId);
    auto clip = m_allClips[clipId];
    Fun local_redo = [clip, this](){
        // We capture a shared_ptr to the clip, which means that as long as this undo object lives, the clip object is not deleted. To insert it back it is sufficient to register it.
        registerClip(clip);
        return true;
    };
    bool res = requestClipMove(clipId, trackId, position, true, local_undo, local_redo);
    if (!res) {
        Q_ASSERT(undo());
        id = -1;
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestClipDeletion(int cid, bool logUndo)
{
#ifdef LOGGING
    m_logFile << "timeline->requestClipDeletion("<<cid<<"); " <<std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isClip(cid));
    if (m_groups->isInGroup(cid)) {
        return requestGroupDeletion(cid);
    }
    Fun undo = [](){return true;};
    Fun redo = [](){return true;};
    bool res = requestClipDeletion(cid, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Delete Clip"));
    }
    return res;
}

bool TimelineModel::requestClipDeletion(int cid, Fun& undo, Fun& redo)
{
    int tid = getClipTrackId(cid);
    if (tid != -1) {
        bool res = getTrackById(tid)->requestClipDeletion(cid, true, undo, redo);
        if (!res) {
            undo();
            return false;
        }
    }
    auto operation = deregisterClip_lambda(cid);
    auto clip = m_allClips[cid];
    auto reverse = [this, clip]() {
        // We capture a shared_ptr to the clip, which means that as long as this undo object lives, the clip object is not deleted. To insert it back it is sufficient to register it.
        registerClip(clip);
        return true;
    };
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    undo();
    return false;
}

bool TimelineModel::requestGroupMove(int cid, int gid, int delta_track, int delta_pos, bool updateView, bool logUndo)
{
#ifdef LOGGING
    m_logFile << "timeline->requestGroupMove("<<cid<<","<<gid<<" ,"<<delta_track<<", "<<delta_pos<<", "<<(updateView ? "true" : "false")<<", "<<(logUndo ? "true" : "false")<<" ); " <<std::endl;
#endif
    QWriteLocker locker(&m_lock);
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
            int tid1 = getClipTrackId(cid1);
            int tid2 = getClipTrackId(cid2);
            int track_pos1 = getTrackPosition(tid1);
            int track_pos2 = getTrackPosition(tid2);
            if (tid1 == tid2) {
                int p1 = m_allClips[cid1]->getPosition();
                int p2 = m_allClips[cid2]->getPosition();
                return !(p1 <= p2) == !(delta_pos <= 0);
            }
            return !(track_pos1 <= track_pos2) == !(delta_track <= 0);
        });
    for (int clip : sorted_clips) {
        int current_track_id = getClipTrackId(clip);
        int current_track_position = getTrackPosition(current_track_id);
        int target_track_position = current_track_position + delta_track;
        if (target_track_position >= 0 && target_track_position < getTracksCount()) {
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

bool TimelineModel::requestGroupDeletion(int cid)
{
#ifdef LOGGING
    m_logFile << "timeline->requestGroupDeletion("<<cid<<" ); " <<std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Fun undo = [](){return true;};
    Fun redo = [](){return true;};
    // we do a breadth first exploration of the group tree, ungroup (delete) every inner node, and then delete all the leaves.
    std::queue<int> group_queue;
    group_queue.push(m_groups->getRootId(cid));
    std::unordered_set<int> all_clips;
    while (!group_queue.empty()) {
        int current_group = group_queue.front();
        group_queue.pop();
        Q_ASSERT(isGroup(current_group));
        auto children = m_groups->getDirectChildren(current_group);
        int one_child = -1; //we need the id on any of the indices of the elements of the group
        for(int c : children) {
            if (isClip(c)) {
                all_clips.insert(c);
                one_child = c;
            } else {
                Q_ASSERT(isGroup(c));
                one_child = c;
                group_queue.push(c);
            }
        }
        if (one_child != -1) {
            bool res = m_groups->ungroupItem(one_child, undo, redo);
            if (!res) {
                undo();
                return false;
            }
        }
    }
    for(int clipId : all_clips) {
        bool res = requestClipDeletion(clipId, undo, redo);
        if (!res) {
            undo();
            return false;
        }
    }
    PUSH_UNDO(undo, redo, i18n("Remove group"));
    return true;
}


bool TimelineModel::requestClipResize(int cid, int size, bool right, bool logUndo, bool snapping)
{
#ifdef LOGGING
    m_logFile << "timeline->requestClipResize("<<cid<<","<<size<<" ,"<<(right ? "true" : "false")<<", "<<(logUndo ? "true" : "false")<<", "<<(snapping ? "true" : "false")<<" ); " <<std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isClip(cid));
    if (snapping) {
        Fun temp_undo = [](){return true;};
        Fun temp_redo = [](){return true;};
        int in = getClipPosition(cid);
        int out = in + getClipPlaytime(cid) - 1;
        m_snaps->ignore({in,out});
        int proposed_size = -1;
        if (right) {
            int target_pos = in + size - 1;
            int snapped_pos = m_snaps->getClosestPoint(target_pos);
            //TODO Make the number of frames adjustable
            if (snapped_pos != -1 && qAbs(target_pos - snapped_pos) <= 10) {
                proposed_size = snapped_pos - in;
            }
        } else {
            int target_pos = out + 1 - size;
            int snapped_pos = m_snaps->getClosestPoint(target_pos);
            //TODO Make the number of frames adjustable
            if (snapped_pos != -1 && qAbs(target_pos - snapped_pos) <= 10) {
                proposed_size = out + 2 - snapped_pos;
            }
        }
        m_snaps->unIgnore();
        if (proposed_size != -1) {
            if (m_allClips[cid]->requestResize(proposed_size, right, temp_undo, temp_redo)) {
                temp_undo(); //undo temp move
                size = proposed_size;
            }
        }
    }
    Fun undo = [](){return true;};
    Fun redo = [](){return true;};
    Fun update_model = [cid, right, logUndo, this]() {
        if (getClipTrackId(cid) != -1) {
            QModelIndex modelIndex = makeClipIndexFromID(cid);
            if (right) {
                notifyChange(modelIndex, modelIndex, false, true, logUndo);
            } else {
                notifyChange(modelIndex, modelIndex, true, true, logUndo);
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

bool TimelineModel::requestClipsGroup(const std::unordered_set<int>& ids)
{
#ifdef LOGGING
    std::stringstream group;
    m_logFile << "{"<<std::endl;
    m_logFile << "auto group = {";
    bool deb = true;
    for (int cid : ids) {
        if(deb) deb = false;
        else group << ", ";
        group<<cid;
    }
    m_logFile << group.str() << "};" << std::endl;
    m_logFile <<"timeline->requestClipsGroup(group);" <<std::endl;
    m_logFile <<std::endl<< "}"<<std::endl;
#endif
    QWriteLocker locker(&m_lock);
    for (int id : ids) {
        if (isClip(id)) {
            if (getClipTrackId(id) == -1) {
                return false;
            }
        } else if (!isGroup(id)) {
            return false;
        }
    }
    Fun undo = [](){return true;};
    Fun redo = [](){return true;};
    int gid = m_groups->groupItems(ids, undo, redo);
    if (gid != -1) {
        PUSH_UNDO(undo, redo, i18n("Group clips"));
    }
    return (gid != -1);
}

bool TimelineModel::requestClipUngroup(int id)
{
#ifdef LOGGING
    m_logFile << "timeline->requestClipUngroup("<<id<<" ); " <<std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Fun undo = [](){return true;};
    Fun redo = [](){return true;};
    bool result = requestClipUngroup(id, undo, redo);
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Ungroup clips"));
    }
    return result;
}

bool TimelineModel::requestClipUngroup(int id, Fun& undo, Fun& redo)
{
    return m_groups->ungroupItem(id, undo, redo);
}

bool TimelineModel::requestTrackInsertion(int position, int &id)
{
#ifdef LOGGING
    m_logFile << "timeline->requestTrackInsertion("<<position<<", dummy_id ); " <<std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Fun undo = [](){return true;};
    Fun redo = [](){return true;};
    bool result = requestTrackInsertion(position, id, undo, redo);
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Insert Track"));
    }
    return result;
}

bool TimelineModel::requestTrackInsertion(int position, int &id, Fun& undo, Fun& redo)
{
    if (position == -1) {
        position = (int)(m_allTracks.size());
    }
    if (position < 0 || position > (int)m_allTracks.size()) {
        return false;
    }
    int trackId = TimelineModel::getNextId();
    id = trackId;
    Fun local_undo = deregisterTrack_lambda(trackId);
    TrackModel::construct(shared_from_this(), trackId, position);
    auto track = getTrackById(trackId);
    Fun local_redo = [track, position, this](){
        // We capture a shared_ptr to the track, which means that as long as this undo object lives, the track object is not deleted. To insert it back it is sufficient to register it.
        registerTrack(track, position);
        return true;
    };
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestTrackDeletion(int tid)
{
#ifdef LOGGING
    m_logFile << "timeline->requestTrackDeletion("<<tid<<"); " <<std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Fun undo = [](){return true;};
    Fun redo = [](){return true;};
    bool result = requestTrackDeletion(tid, undo, redo);
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Delete Track"));
    }
    return result;
}

bool TimelineModel::requestTrackDeletion(int tid, Fun& undo, Fun& redo)
{
    Q_ASSERT(isTrack(tid));
    std::vector<int> clips_to_delete;
    for(const auto& it : getTrackById(tid)->m_allClips) {
        clips_to_delete.push_back(it.first);
    }
    Fun local_undo = [](){return true;};
    Fun local_redo = [](){return true;};
    for(int clip : clips_to_delete) {
        bool res = true;
        while (res && m_groups->isInGroup(clip)) {
            res = requestClipUngroup(clip, local_undo, local_redo);
        }
        if (res) {
            res = requestClipDeletion(clip, local_undo, local_redo);
        }
        if (!res) {
            bool u = local_undo();
            Q_ASSERT(u);
            return false;
        }
    }
    int old_position = getTrackPosition(tid);
    auto operation = deregisterTrack_lambda(tid, true);
    std::shared_ptr<TrackModel> track = getTrackById(tid);
    auto reverse = [this, track, old_position]() {
        // We capture a shared_ptr to the track, which means that as long as this undo object lives, the track object is not deleted. To insert it back it is sufficient to register it.
        registerTrack(track, old_position);
        return true;
    };
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, local_undo, local_redo);
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
        return true;
    }
    local_undo();
    return false;

}

void TimelineModel::registerTrack(std::shared_ptr<TrackModel> track, int pos)
{
    int id = track->getId();
    if (pos == -1) {
        pos = static_cast<int>(m_allTracks.size());
    }
    Q_ASSERT(pos >= 0);
    Q_ASSERT(pos <= static_cast<int>(m_allTracks.size()));

    //effective insertion (MLT operation), add 1 to account for black background track
    int error = m_tractor->insert_track(*track ,pos + 1);
    Q_ASSERT(error == 0); //we might need better error handling...

    // we now insert in the list
    auto posIt = m_allTracks.begin();
    std::advance(posIt, pos);
    auto it = m_allTracks.insert(posIt, std::move(track));
    //it now contains the iterator to the inserted element, we store it
    Q_ASSERT(m_iteratorTable.count(id) == 0); //check that id is not used (shouldn't happen)
    m_iteratorTable[id] = it;
    _resetView();
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

Fun TimelineModel::deregisterTrack_lambda(int id, bool updateView)
{
    return [this, id, updateView]() {
        auto it = m_iteratorTable[id]; //iterator to the element
        int index = getTrackPosition(id); //compute index in list
        if (updateView) {
            QModelIndex root;
            _resetView();
        }
        m_tractor->remove_track(static_cast<int>(index + 1)); //melt operation, add 1 to account for black background track
        //send update to the model
        m_allTracks.erase(it);  //actual deletion of object
        m_iteratorTable.erase(id);  //clean table
        return true;
    };
}

Fun TimelineModel::deregisterClip_lambda(int cid)
{
    return [this, cid]() {
        Q_ASSERT(m_allClips.count(cid) > 0);
        Q_ASSERT(getClipTrackId(cid) == -1); //clip must be deleted from its track at this point
        Q_ASSERT(!m_groups->isInGroup(cid)); //clip must be ungrouped at this point
        m_allClips.erase(cid);
        m_groups->destructGroupItem(cid);
        return true;
    };
}

void TimelineModel::deregisterGroup(int id)
{
    Q_ASSERT(m_allGroups.count(id) > 0);
    m_allGroups.erase(id);
}

std::shared_ptr<TrackModel> TimelineModel::getTrackById(int tid)
{
    Q_ASSERT(m_iteratorTable.count(tid) > 0);
    return *m_iteratorTable[tid];
}

const std::shared_ptr<TrackModel> TimelineModel::getTrackById_const(int tid) const
{
    Q_ASSERT(m_iteratorTable.count(tid) > 0);
    return *m_iteratorTable.at(tid);
}

std::shared_ptr<ClipModel> TimelineModel::getClipPtr(int cid) const
{
    Q_ASSERT(m_allClips.count(cid) > 0);
    return m_allClips.at(cid);
}

std::shared_ptr<TransitionModel> TimelineModel::getTransitionPtr(int tid) const
{
    Q_ASSERT(m_allTransitions.count(tid) > 0);
    return m_allTransitions.at(tid);
}

int TimelineModel::getNextId()
{
    return TimelineModel::next_id++;
}

bool TimelineModel::isClip(int id) const
{
    return m_allClips.count(id) > 0;
}

bool TimelineModel::isTransition(int id) const
{
    return m_allTransitions.count(id) > 0;
}

bool TimelineModel::isTrack(int id) const
{
    return m_iteratorTable.count(id) > 0;
}

bool TimelineModel::isGroup(int id) const
{
    return m_allGroups.count(id) > 0;
}

int TimelineModel::duration() const
{
    return m_tractor->get_playtime();
}


std::unordered_set<int> TimelineModel::getGroupElements(int cid)
{
    int gid = m_groups->getRootId(cid);
    return m_groups->getLeaves(gid);
}

Mlt::Profile *TimelineModel::getProfile()
{
    return m_profile;
}

bool TimelineModel::requestReset(Fun& undo, Fun& redo)
{
    std::vector<int> all_ids;
    for (const auto& track : m_iteratorTable) {
        all_ids.push_back(track.first);
    }
    bool ok = true;
    for (int tid : all_ids) {
        ok = ok && requestTrackDeletion(tid, undo, redo);
    }
    return ok;
}

void TimelineModel::setUndoStack(std::weak_ptr<DocUndoStack> undo_stack)
{
    m_undoStack = undo_stack;
}

int TimelineModel::requestBestSnapPos(int pos, int length, const std::vector<int>& pts)
{
    if (pts.size() > 0) {
        m_snaps->ignore(pts);
    }
    int snapped_start = m_snaps->getClosestPoint(pos);
    qDebug() << "snapping start suggestion" <<snapped_start;
    int snapped_end = m_snaps->getClosestPoint(pos + length);
    m_snaps->unIgnore();
    
    int startDiff = qAbs(pos - snapped_start);
    int endDiff = qAbs(pos + length - snapped_end);
    if (startDiff < endDiff && snapped_start >= 0) {
        // snap to start
        if (startDiff < 10) {
            return snapped_start;
        }
    } else {
        // snap to end
        if (endDiff < 10 && snapped_end >= 0) {
            return snapped_end - length;
        }
    }
    return -1;
}

int TimelineModel::requestNextSnapPos(int pos)
{
    return m_snaps->getNextPoint(pos);
}

int TimelineModel::requestPreviousSnapPos(int pos)
{
    return m_snaps->getPreviousPoint(pos);
}

void TimelineModel::registerTransition(std::shared_ptr<TransitionModel> transition)
{
    int id = transition->getId();
    Q_ASSERT(m_allTransitions.count(id) == 0);
    m_allTransitions[id] = transition;
    m_groups->createGroupItem(id);
}

bool TimelineModel::requestTransitionInsertion(std::shared_ptr<Mlt::Transition> trans, int tid, int &id, Fun& undo, Fun& redo)
{
    int transitionId = TimelineModel::getNextId();
    id = transitionId;
    Fun local_undo = deregisterTransition_lambda(transitionId);
    TransitionModel::construct(shared_from_this(), trans, transitionId);
    auto transition = m_allTransitions[transitionId];
    Fun local_redo = [transition, this](){
        // We capture a shared_ptr to the clip, which means that as long as this undo object lives, the clip object is not deleted. To insert it back it is sufficient to register it.
        registerTransition(transition);
        return true;
    };
    bool res = requestTransitionMove(transitionId, tid, trans->get_in(), true, local_undo, local_redo);
    if (!res) {
        Q_ASSERT(undo());
        id = -1;
        return false;
    }
    _resetView();
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

Fun TimelineModel::deregisterTransition_lambda(int tid)
{
    return [this, tid]() {
        Q_ASSERT(m_allTransitions.count(tid) > 0);
        Q_ASSERT(!m_groups->isInGroup(tid)); //clip must be ungrouped at this point
        m_allTransitions.erase(tid);
        m_groups->destructGroupItem(tid);
        return true;
    };
}

int TimelineModel::getTransitionTrackId(int tid) const
{
    Q_ASSERT(m_allTransitions.count(tid) > 0);
    const auto trans = m_allTransitions.at(tid);
    return trans->getCurrentTrackId();
}

int TimelineModel::getTransitionPosition(int tid) const
{
    Q_ASSERT(m_allTransitions.count(tid) > 0);
    const auto trans = m_allTransitions.at(tid);
    return trans->getPosition();
}

int TimelineModel::getTransitionPlaytime(int tid) const
{
    READ_LOCK();
    Q_ASSERT(m_allTransitions.count(tid) > 0);
    const auto trans = m_allTransitions.at(tid);
    int playtime = trans->getPlaytime();
    return playtime;
}

int TimelineModel::getTrackTransitionsCount(int tid) const
{
    return getTrackById_const(tid)->getTransitionsCount();
}

bool TimelineModel::requestTransitionMove(int cid, int tid, int position,  bool updateView, bool logUndo)
{
#ifdef LOGGING
    m_logFile << "timeline->requestTransitionMove("<<cid<<","<<tid<<" ,"<<position<<", "<<(updateView ? "true" : "false")<<", "<<(logUndo ? "true" : "false")<<" ); " <<std::endl;
#endif
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allTransitions.count(cid) > 0);
    if (m_allTransitions[cid]->getPosition() == position && getTransitionTrackId(cid) == tid) {
        return true;
    }
    if (m_groups->isInGroup(cid)) {
        //element is in a group.
        int gid = m_groups->getRootId(cid);
        int current_tid = getTransitionTrackId(cid);
        int track_pos1 = getTrackPosition(tid);
        int track_pos2 = getTrackPosition(current_tid);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_allTransitions[cid]->getPosition();
        return requestGroupMove(cid, gid, delta_track, delta_pos, updateView, logUndo);
    }
    std::function<bool (void)> undo = [](){return true;};
    std::function<bool (void)> redo = [](){return true;};
    bool res = requestTransitionMove(cid, tid, position, updateView, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move transition"));
    }
    return res;
}


bool TimelineModel::requestTransitionMove(int transid, int tid, int position, bool updateView, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isTransition(transid));
    std::function<bool (void)> local_undo = [](){return true;};
    std::function<bool (void)> local_redo = [](){return true;};
    bool ok = true;
    int old_tid = getTransitionTrackId(transid);
    if (old_tid != -1) {
        if (old_tid == tid) {
            // Simply setting in/out is enough
            local_undo = getTrackById(old_tid)->requestTransitionResize_lambda(transid, position);
            if (!ok) {
                qDebug()<<"------------\nFAILED TO RESIZE TRANS: "<<old_tid;
                bool undone = local_undo();
                Q_ASSERT(undone);
                return false;
            }
            UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
            return true;
        }
        ok = getTrackById(old_tid)->requestTransitionDeletion(transid, updateView, local_undo, local_redo);
        if (!ok) {
            qDebug()<<"------------\nFAILED TO DELETE TRANS: "<<old_tid;
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    ok = getTrackById(tid)->requestTransitionInsertion(transid, position, updateView, local_undo, local_redo);
    if (!ok) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

void TimelineModel::plantTransition(Mlt::Transition &tr, int a_track, int b_track)
{
    //qDebug()<<"* * PLANT TRANSITION: "<<tr.get("mlt_service")<<", TRACK: "<<a_track<<"x"<<b_track<<" ON POS: "<<tr.get_in();
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    mlt_service nextservice = mlt_service_get_producer(field.data()->get_service());
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString resource = mlt_properties_get(properties, "mlt_service");
    QList<Mlt::Transition *> trList;
    mlt_properties insertproperties = tr.get_properties();
    QString insertresource = mlt_properties_get(insertproperties, "mlt_service");
    bool isMixTransition = insertresource == QLatin1String("mix");

    mlt_service_type mlt_type = mlt_service_identify(nextservice);
    while (mlt_type == transition_type) {
        Mlt::Transition transition((mlt_transition) nextservice);
        nextservice = mlt_service_producer(nextservice);
        int aTrack = transition.get_a_track();
        int bTrack = transition.get_b_track();
        int internal = transition.get_int("internal_added");
        if ((isMixTransition || resource != QLatin1String("mix")) && (internal > 0 || aTrack < a_track || (aTrack == a_track && bTrack > b_track))) {
            Mlt::Properties trans_props(transition.get_properties());
            Mlt::Transition *cp = new Mlt::Transition(*m_tractor->profile(), transition.get("mlt_service"));
            Mlt::Properties new_trans_props(cp->get_properties());
            //new_trans_props.inherit(trans_props);
            new_trans_props.inherit(trans_props);
            trList.append(cp);
            field->disconnect_service(transition);
        }
        //else qCDebug(KDENLIVE_LOG) << "// FOUND TRANS OK, "<<resource<< ", A_: " << aTrack << ", B_ "<<bTrack;

        if (nextservice == nullptr) {
            break;
        }
        properties = MLT_SERVICE_PROPERTIES(nextservice);
        mlt_type = mlt_service_identify(nextservice);
        resource = mlt_properties_get(properties, "mlt_service");
    }
    field->plant_transition(tr, a_track, b_track);

    // re-add upper transitions
    for (int i = trList.count() - 1; i >= 0; --i) {
        ////qCDebug(KDENLIVE_LOG)<< "REPLANT ON TK: "<<trList.at(i)->get_a_track()<<", "<<trList.at(i)->get_b_track();
        field->plant_transition(*trList.at(i), trList.at(i)->get_a_track(), trList.at(i)->get_b_track());
    }
    qDeleteAll(trList);
}

bool TimelineModel::removeTransition(int tid, int pos)
{
    //qDebug()<<"* * * TRYING TO DELETE TRANSITION: "<<tid<<" / "<<pos;
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();
    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString resource = mlt_properties_get(properties, "mlt_service");
    bool found = false;
    ////qCDebug(KDENLIVE_LOG) << " del trans pos: " << in.frames(25) << '-' << out.frames(25);

    mlt_service_type mlt_type = mlt_service_identify(nextservice);
    while (mlt_type == transition_type) {
        mlt_transition tr = (mlt_transition) nextservice;
        int currentTrack = mlt_transition_get_b_track(tr);
        int currentATrack = mlt_transition_get_a_track(tr);
        int currentIn = (int) mlt_transition_get_in(tr);
        int currentOut = (int) mlt_transition_get_out(tr);
        //qDebug() << "// FOUND EXISTING TRANS, IN: " << currentIn << ", OUT: " << currentOut << ", TRACK: " << currentTrack<<" / "<<currentATrack;

        if (tid == currentTrack && currentIn == pos) {
            found = true;
            mlt_field_disconnect_service(field->get_field(), nextservice);
            break;
        }
        nextservice = mlt_service_producer(nextservice);
        if (nextservice == nullptr) {
            break;
        }
        properties = MLT_SERVICE_PROPERTIES(nextservice);
        mlt_type = mlt_service_identify(nextservice);
        resource = mlt_properties_get(properties, "mlt_service");
    }
    field->unlock();
    return found;
}

