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
#include "mltcontroller/bincontroller.h"

#include <klocalizedstring.h>
#include <QDebug>
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


TimelineModel::TimelineModel(BinController *binController, std::weak_ptr<DocUndoStack> undo_stack) :
    QAbstractItemModel(),
    m_tractor(new Mlt::Tractor()),
    m_undoStack(undo_stack),
    m_binController(binController)
{
    Mlt::Profile profile;
    m_tractor->set_profile(profile);
}

std::shared_ptr<TimelineModel> TimelineModel::construct(BinController *binController, std::weak_ptr<DocUndoStack> undo_stack, bool populate)
{
    std::shared_ptr<TimelineModel> ptr(new TimelineModel(binController, undo_stack));
    ptr->m_groups = std::unique_ptr<GroupsModel>(new GroupsModel(ptr));
    if (populate) {
        // Testing: add a clip on first track
        Mlt::Profile profile;
        std::shared_ptr<Mlt::Producer> prod(new Mlt::Producer(profile,"color", "red"));
        prod->set("length", 200);
        prod->set("out", 24);
        int ix = TrackModel::construct(ptr);
        int ix2 = TrackModel::construct(ptr);
        int clipId = ClipModel::construct(ptr, prod);
        int clipId2 = ClipModel::construct(ptr, prod);
        int clipId3 = ClipModel::construct(ptr, prod);
        ptr->requestClipMove(clipId, ix, 100, false);
        ptr->requestClipMove(clipId2, ix, 50, false);
        ptr->requestClipMove(clipId3, ix, 250, false);
        ptr->getTrackById(ix)->setProperty("kdenlive:trackheight", "60");
        ptr->getTrackById(ix2)->setProperty("kdenlive:trackheight", "140");
    }
    return ptr;
}

TimelineModel::~TimelineModel()
{
    for(auto tracks : m_iteratorTable) {
        deleteTrackById(tracks.first);
    }
}

QModelIndex TimelineModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column > 0)
        return QModelIndex();
//    LOG_DEBUG() << __FUNCTION__ << row << column << parent;
    QModelIndex result;
    if (parent.isValid()) {
        int trackId = int(parent.internalId());
        Q_ASSERT(isTrack(trackId));
        int clipId = getTrackById_const(trackId)->getClipByRow(row);
        if (clipId != -1) {
            result = createIndex(row, 0, quintptr(clipId));
        }
    } else if (row < getTracksCount()) {
        auto it = m_allTracks.cbegin();
        std::advance(it, row);
        int trackId = (*it)->getId();
        result = createIndex(row, column, quintptr(trackId));
    }
    return result;
}

QModelIndex TimelineModel::makeIndex(int trackIndex, int clipIndex) const
{
    return index(clipIndex, 0, index(trackIndex));
}

QModelIndex TimelineModel::makeClipIndexFromID(int cid) const
{
    Q_ASSERT(m_allClips.count(cid) > 0);
    int tid = m_allClips.at(cid)->getCurrentTrackId();
    return index(getTrackById_const(tid)->getRowfromClip(cid), 0, makeTrackIndexFromID(tid) );
}

QModelIndex TimelineModel::makeTrackIndexFromID(int tid) const
{
    // we retrieve iterator
    Q_ASSERT(m_iteratorTable.count(tid) > 0);
    auto it = m_iteratorTable.at(tid);
    int ind = (int)std::distance<decltype(m_allTracks.cbegin())>(m_allTracks.begin(), it);
    return index(ind);
}

QModelIndex TimelineModel::parent(const QModelIndex &index) const
{
//    LOG_DEBUG() << __FUNCTION__ << index;
    const int id = static_cast<int>(index.internalId());
    if (!index.isValid() || isTrack(id)) {
        return QModelIndex();
    } else {
        Q_ASSERT(isClip(id)); //if id is not a track it must be a clip.
        const int trackId = getClipTrackId(id);
        auto it = m_iteratorTable.at(trackId); //iterator to the element
        decltype(m_allTracks.cbegin()) const_it(it);
        int row = (int)std::distance(m_allTracks.cbegin(), const_it); //compute index in list
        return createIndex(row, 0, quintptr(trackId));
    }
}


int TimelineModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        const int id = (int)parent.internalId();
        if (isClip(id) || !isTrack(id)) {
            //clips don't have children
            //if it is not a track and not a clip, it is something invalid
            return 0;
        }
        // return number of clip in a specific track
        return getTrackClipsCount(id);
    }
    return getTracksCount();
}

int TimelineModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QHash<int, QByteArray> TimelineModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[ResourceRole] = "resource";
    roles[ServiceRole] = "mlt_service";
    roles[IsBlankRole] = "blank";
    roles[StartRole] = "start";
    roles[DurationRole] = "duration";
    roles[InPointRole] = "in";
    roles[OutPointRole] = "out";
    roles[FramerateRole] = "fps";
    roles[IsMuteRole] = "mute";
    roles[IsHiddenRole] = "hidden";
    roles[IsAudioRole] = "audio";
    roles[AudioLevelsRole] = "audioLevels";
    roles[IsCompositeRole] = "composite";
    roles[IsLockedRole] = "locked";
    roles[FadeInRole] = "fadeIn";
    roles[FadeOutRole] = "fadeOut";
    roles[IsTransitionRole] = "isTransition";
    roles[FileHashRole] = "hash";
    roles[SpeedRole] = "speed";
    roles[HeightRole] = "trackHeight";
    roles[ItemIdRole] = "item";
    return roles;
}

QVariant TimelineModel::data(const QModelIndex &index, int role) const
{
    if (!m_tractor || !index.isValid()) {
        return QVariant();
    }
    const int id = (int)index.internalId();
    if (role == ItemIdRole) {
        return id;
    }
    //qDebug() << "DATA requested "<<index<<roleNames()[role];
    if (isClip(id)) {
        // Get data for a clip
        switch (role) {
        //TODO
        case NameRole:
        case ResourceRole:
        case Qt::DisplayRole:{
            QString result = QString::fromUtf8("clip name");
            return result;
        }
        case ServiceRole:
            return QString("service2");
            break;
        case IsBlankRole: //probably useless
            return false;
        case StartRole:
            return m_allClips.at(id)->getPosition();
        case DurationRole:
            return m_allClips.at(id)->getPlaytime();

            //Are these really needed ??
        case InPointRole:
            return 0;
        case OutPointRole:
            return 1;
        case FramerateRole:
            return 25;
        default:
            break;
        }
    } else if(isTrack(id)) {
        switch (role) {
            case NameRole:
            case Qt::DisplayRole:
                return QString("Track %1").arg(getTrackById_const(index.row())->getId());
            case DurationRole:
                return m_tractor->get_playtime();
            case IsMuteRole:
                return 0;
            case IsHiddenRole:
                return 0;
            case IsAudioRole:
                return false;
            case IsLockedRole:
                return 0;
            case HeightRole: {
                int height = getTrackById_const(index.row())->getProperty("kdenlive:trackheight").toInt();
                return (height > 0 ? height : 50);
            }
            case IsCompositeRole: {
                return Qt::Unchecked;
            }
            default:
                break;
        }
    }
    return QVariant();
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

bool TimelineModel::allowClipMove(int cid, int tid, int position)
{
    qDebug()<<"Checking clip move"<<cid<<tid<<position;
    if (!isClip(cid) || !isTrack(tid)) {
        qDebug() << "ERROR : Invalid clip or track";
        return false;
    }
    int length = m_allClips[cid]->getPlaytime();
    return getTrackById(tid)->allowClipMove(cid, position, length);
}

bool TimelineModel::requestClipMove(int cid, int tid, int position, Fun &undo, Fun &redo)
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
    ok = getTrackById(tid)->requestClipInsertion(m_allClips[cid], position, local_undo, local_redo);
    if (!ok) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        return false;
    }
    int new_clip_index = getTrackById(tid)->getRowfromClip(cid);
    auto operation = [cid, tid, old_tid, old_clip_index, new_clip_index, this]() {
        m_allClips[cid]->setCurrentTrackId(tid);
        if (tid == old_tid) {
            QModelIndex modelIndex = makeClipIndexFromID(cid);
            emit dataChanged(modelIndex, modelIndex, {StartRole});
        } else {
            if (old_tid != -1){
                qDebug() << "Removed row "<<old_clip_index;
                beginRemoveRows(makeTrackIndexFromID(old_tid), old_clip_index, old_clip_index);
                endRemoveRows();
            }
            beginInsertRows(makeTrackIndexFromID(tid), new_clip_index, new_clip_index);
            endInsertRows();
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
    local_undo = [local_undo, tid, old_tid, cid, old_clip_index, new_clip_index, this]() {
        bool v = local_undo();
        //qDebug()<<"DATA CHANGED SIGNAL";
        if (tid == old_tid) {
            QModelIndex modelIndex = makeClipIndexFromID(cid);
            emit dataChanged(modelIndex, modelIndex, {StartRole});
        } else {
            beginRemoveRows(makeTrackIndexFromID(tid), new_clip_index, new_clip_index);
            endRemoveRows();
            if (old_tid != -1) {
                beginInsertRows(makeTrackIndexFromID(old_tid), old_clip_index, old_clip_index);
                endInsertRows();
            }
        }
        //qDebug()<<"Moved back"<<cid<<"to position"<<m_allClips[cid]->getPosition();
        return v;
    };
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return operation();
}

bool TimelineModel::requestClipMove(int cid, int tid, int position, bool logUndo)
{
    Q_ASSERT(m_allClips.count(cid) > 0);
    if (m_allClips[cid]->getPosition() == position && m_allClips[cid]->getCurrentTrackId() == tid) {
        return true;
    }
    if (m_groups->getRootId(cid) != cid) {
        //element is in a group.
        int gid = m_groups->getRootId(cid);
        int delta_track = tid - m_allClips[cid]->getCurrentTrackId();
        int delta_pos = position - m_allClips[cid]->getPosition();
        return requestGroupMove(gid, delta_track, delta_pos);
    }
    qDebug()<<"clip move in model"<<cid<<tid<<position;
    std::function<bool (void)> undo = [](){return true;};
    std::function<bool (void)> redo = [](){return true;};
    bool res = requestClipMove(cid, tid, position, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move clip"));
    }
    return res;
}

bool TimelineModel::requestGroupMove(int gid, int delta_track, int delta_pos, bool logUndo)
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
            if (tid1 == tid2) {
                int p1 = m_allClips[cid1]->getPosition();
                int p2 = m_allClips[cid2]->getPosition();
                return !(p1 <= p2) == !(delta_pos <= 0);
            }
            return !(tid1 <= tid2) == !(delta_track <= 0);
        });
    for (int clip : sorted_clips) {
        int target_track = m_allClips[clip]->getCurrentTrackId() + delta_track;
        int target_position = m_allClips[clip]->getPosition() + delta_pos;
        ok = requestClipMove(clip, target_track, target_position, undo, redo);
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

bool TimelineModel::trimClip(int cid, int delta, bool right, bool ripple)
{
    return requestClipResize(cid, m_allClips[cid]->getPlaytime() - delta, right);
}


bool TimelineModel::requestClipResize(int cid, int size, bool right)
{
    std::function<bool (void)> undo = [](){return true;};
    std::function<bool (void)> redo = [](){return true;};
    bool result = m_allClips[cid]->requestResize(size, right, undo, redo);
    //qDebug()<<"clip resize in model"<<cid<<size<<right<<result;
    if (result) {
        if (getClipTrackId(cid) != -1) {
            QModelIndex modelIndex = makeClipIndexFromID(cid);
            if (right) {
                emit dataChanged(modelIndex, modelIndex, {DurationRole});
            } else {
                emit dataChanged(modelIndex, modelIndex, {StartRole,DurationRole});
            }
        }
        PUSH_UNDO(undo, redo, i18n("Resize clip"));
    }
    return result;
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
    //TODO send deletion order to the track containing the clip
    Q_ASSERT(m_allClips.count(id) > 0);
    m_allClips.erase(id);
    //TODO this is temporary while UNDO for clip deletion is not implemented
    std::function<bool (void)> undo = [](){return true;};
    std::function<bool (void)> redo = [](){return true;};
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

void TimelineModel::insertClip(std::shared_ptr<TimelineModel> tl, int track, int position, QString data)
{
    std::shared_ptr<Mlt::Producer> prod(new Mlt::Producer(m_binController->getBinProducer(data)));
    int clipId = ClipModel::construct(tl, prod);
    requestClipMove(clipId, track, position, true);   
}
