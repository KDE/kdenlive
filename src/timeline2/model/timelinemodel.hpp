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
#include <QVector>
#include <QAbstractItemModel>
#include <mlt++/MltTractor.h>

class TrackModel;
class ClipModel;
class GroupsModel;

/* @brief This class represents a Timeline object, as viewed by the backend.
   In general, the Gui associated with it will send modification queries (such as resize or move), and this class authorize them or not depending on the validity of the modifications
*/
class TimelineModel : public QAbstractItemModel
{

public:
    /* @brief construct a timeline object and returns a pointer to the created object
     */
    static std::shared_ptr<TimelineModel> construct(bool populate = false);

protected:
    /* @brief this constructor should not be called. Call the static construct instead
     */
    TimelineModel();

public:
    friend class TrackModel;
    friend class ClipModel;
    friend class GroupsModel;

    ~TimelineModel();
    /// Two level model: tracks and clips on track
    enum {
        NameRole = Qt::UserRole + 1,
        ResourceRole,    /// clip only
        ServiceRole,     /// clip only
        IsBlankRole,     /// clip only
        StartRole,       /// clip only
        DurationRole,
        InPointRole,     /// clip only
        OutPointRole,    /// clip only
        FramerateRole,   /// clip only
        IsMuteRole,      /// track only
        IsHiddenRole,    /// track only
        IsAudioRole,
        AudioLevelsRole, /// clip only
        IsCompositeRole, /// track only
        IsLockedRole,    /// track only
        HeightRole,      /// track only
        FadeInRole,      /// clip only
        FadeOutRole,     /// clip only
        IsTransitionRole,/// clip only
        FileHashRole,    /// clip only
        SpeedRole        /// clip only
    };

    int rowCount(const QModelIndex & parent = QModelIndex()) const Q_DECL_OVERRIDE;
    int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    QHash<int, QByteArray> roleNames() const Q_DECL_OVERRIDE;
    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex makeIndex(int trackIndex, int clipIndex) const;
    QModelIndex parent(const QModelIndex &index) const;

    /* @brief returns the number of tracks */
    int getTracksCount() const;

    /* @brief returns the number of clips */
    int getClipsCount() const;

    /* @brief Delete track based on its id */
    void deleteTrackById(int id);

    /* @brief Delete clipq based on its id */
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

    /* @brief Change the track in which the clip is included
       Returns true on success. If it fails, nothing is modified.
       @param cid is the ID of the clip
       @param tid is the ID of the target track
       @param position is the position where we want to insert
       @param dry If this parameter is true, no action is actually executed, but we return true if it would be possible to do it.
    */
    bool requestClipChangeTrack(int cid, int tid, int position, bool dry = false);

    /* @brief Group together a set of ids
       Typically, ids would be ids of clips, but for convenience, some of them can be ids of groups as well.
       @param ids Set of ids to group
    */
    void groupClips(std::unordered_set<int>&& ids);

    /* @brief Destruct the topmost group containing clip
       @param id of the clip to degroup (all clips belonging to the same group will be ungrouped as well)
    */
    void ungroupClip(int id);

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

    /* @brief Returns next valid unique id to create an object
     */
    static int getNextId();
private:
    Mlt::Tractor *m_tractor;
    QVector<int> m_snapPoints; // this will be modified from a lot of different places, we will probably need a mutex

    std::list<std::unique_ptr<TrackModel>> m_allTracks;

    std::unordered_map<int, std::list<std::unique_ptr<TrackModel>>::iterator> m_iteratorTable; //this logs the iterator associated which each track id. This allows easy access of a track based on its id.

    std::unordered_map<int, std::shared_ptr<ClipModel>> m_allClips; //the keys are the clip id, and the values are the corresponding pointers

    static int next_id;//next valid id to assign

    std::unique_ptr<GroupsModel> m_groups;

    std::unordered_set<int> m_allGroups; //ids of all the groups

};

#endif
