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

#include <klocalizedstring.h>
#include <QDebug>
#include <mlt++/MltTractor.h>
#include <mlt++/MltProfile.h>

int TimelineModel::next_id = 0;
static const quintptr NO_PARENT_ID = quintptr(-1);

TimelineModel::TimelineModel() : QAbstractItemModel(), 
    m_tractor(new Mlt::Tractor())
{
}

std::shared_ptr<TimelineModel> TimelineModel::construct(bool populate)
{
    std::shared_ptr<TimelineModel> ptr(new TimelineModel());
    ptr->m_groups = std::unique_ptr<GroupsModel>(new GroupsModel(ptr));
    if (populate) {
        TrackModel::construct(ptr);
        int ix = TrackModel::construct(ptr);
        // Testing: add a clip on first track
        Mlt::Profile profile;
        std::shared_ptr<Mlt::Producer> prod(new Mlt::Producer(profile,"color", "red"));
        int clipId = ClipModel::construct(ptr, prod);
        // Not sure this is the right way to insert a clip...
        std::shared_ptr<ClipModel> clip(ptr->getClip(clipId));
        std::unique_ptr<TrackModel>& track(ptr->getTrackById(ix));
        track->requestClipInsertion(clip, 100, false);
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
        //TODO: do we need a separate index like shotcut?
        int i = row; //m_trackList.at(parent.row()).mlt_index;
        QScopedPointer<Mlt::Producer> track(m_tractor->track(i));
        if (track) {
            Mlt::Playlist playlist((mlt_playlist) track->get_producer());
            if (row < playlist.count())
                result = createIndex(row, column, parent.row());
        }
    } else if (row < getTracksCount()) {
        result = createIndex(row, column, NO_PARENT_ID);
    }
    return result;
}

QModelIndex TimelineModel::makeIndex(int trackIndex, int clipIndex) const
{
    return index(clipIndex, 0, index(trackIndex));
}

QModelIndex TimelineModel::parent(const QModelIndex &index) const
{
//    LOG_DEBUG() << __FUNCTION__ << index;
    if (!index.isValid() || index.internalId() == NO_PARENT_ID)
        return QModelIndex();
    else
        return createIndex(index.internalId(), 0, NO_PARENT_ID);
}


int TimelineModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (parent.isValid()) {
        // return number of clip in a specific track
        if (parent.internalId() != NO_PARENT_ID)
            return 0;
        QScopedPointer<Mlt::Producer> track(m_tractor->track(parent.row()));
        if (track) {
            Mlt::Playlist playlist((mlt_playlist) track->get_producer());
            return playlist.count();
        }
        return 0;
        /*int i = m_trackList.at(parent.row()).mlt_index;
        QScopedPointer<Mlt::Producer> track(m_tractor->track(i));
        if (track) {
            Mlt::Playlist playlist(*track);
            int n = playlist.count();
//            LOG_DEBUG() << __FUNCTION__ << parent << i << n;
            return n;
        } else {
            return 0;
        }*/
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
    return roles;
}

QVariant TimelineModel::data(const QModelIndex &index, int role) const
{
    if (!m_tractor || !index.isValid()) {
        return QVariant();
    }
    if (index.parent().isValid()) {
        // Get data for a clip
        switch (role) {
        //TODO
        case NameRole: {
                return QString("service");
            }
            case ResourceRole:
            case Qt::DisplayRole: {
                return QString("resource");
            }
            case ServiceRole:
                return QString("service2");
                break;
            case IsBlankRole:
                return false;
                //return playlist.is_blank(index.row());
            case StartRole:
                return 50;
            case DurationRole:
                return 100;
            case InPointRole:
                return 0;
            case OutPointRole:
                return 50;
            case FramerateRole:
                return 25;
            default:
                break;
        }
    } else {
        switch (role) {
            case NameRole:
            case Qt::DisplayRole:
                return QString("Track %1").arg(getTrackById_const(index.row())->getId());
            case DurationRole:
                return 100;
            case IsMuteRole:
                return 0;
            case IsHiddenRole:
                return 0;
            case IsAudioRole:
                return false;
            case IsLockedRole:
                return 0;
            case IsCompositeRole: {
                return Qt::Unchecked;
            }
            default:
                break;
        }
    }
    return QVariant();
}

int TimelineModel::trackHeight() const
{
    //TODO
    return 50;
}

void TimelineModel::setTrackHeight(int height)
{
    //TODO
}

double TimelineModel::scaleFactor() const
{
    //TODO
    return 3.0;
}

void TimelineModel::setScaleFactor(double scale)
{
    //TODO
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

bool TimelineModel::requestClipChangeTrack(int cid, int tid, int position, bool dry)
{
    if (!dry && !requestClipChangeTrack(cid, tid, position, true)) {
        return false;
    }
    Q_ASSERT(m_allClips.count(cid) > 0);
    bool ok = true;
    int old_tid = m_allClips[cid]->getCurrentTrackId();
    if (old_tid != -1) {
        ok = getTrackById(old_tid)->requestClipDeletion(cid, dry);
        if (!ok) {
            return false;
        }
    }
    ok = getTrackById(tid)->requestClipInsertion(m_allClips[cid], position, dry);
    if (ok && !dry) {
        m_allClips[cid]->setCurrentTrackId(tid);
    }
    return ok;
}

void TimelineModel::groupClips(std::unordered_set<int>&& ids)
{
    m_groups->groupItems(std::forward<std::unordered_set<int>>(ids));
}

void TimelineModel::ungroupClip(int id)
{
    m_groups->ungroupItem(id);
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

std::shared_ptr<ClipModel> TimelineModel::getClip(int id)
{
    // is there a cleaner way to get a clip to add it to a track?
    Q_ASSERT(m_allClips.count(id) > 0);
    return m_allClips[id];
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
    m_groups->destructGroupItem(id, true);
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
