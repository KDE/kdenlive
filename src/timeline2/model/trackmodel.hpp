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

#include <memory>
#include <QSharedPointer>
#include <unordered_map>
#include <mlt++/MltPlaylist.h>

class TimelineModel;
class ClipModel;

/* @brief This class represents a Track object, as viewed by the backend.
   In general, the Gui associated with it will send modification queries (such as resize or move), and this class authorize them or not depending on the validity of the modifications
*/
class TrackModel
{

public:
    TrackModel() = delete;

    friend class TimelineModel;
private:
    /* This constructor is private, call the static construct instead */
    TrackModel(std::weak_ptr<TimelineModel> parent);
public:
    /* @brief Creates a track, which references itself to the parent
       Returns the (unique) id of the created track
       @param pos is the optional position of the track. If left to -1, it will be added at the end
     */
    static int construct(std::weak_ptr<TimelineModel> parent, int pos = -1);

    /* @brief The destructor. It asks the parent to be deleted
     */
    void destruct();

    /* @brief returns the number of clips */
    int getClipsCount() const;

    /* @brief Performs an insertion of the given clip.
       Returns true if the operation succeeded, and otherwise, the track is not modified.
       @param clip is a shared pointer to the clip
       @param position is the position where to insert the clip
    */
    bool requestClipInsertion(std::shared_ptr<ClipModel> clip, int position);

    /* @brief Performs an deletion of the given clip.
       Returns true if the operation succeeded, and otherwise, the track is not modified.
       @param cid is the id of the clip
    */
    bool requestClipDeletion(int cid);

    /* Perform a resize operation on a clip. Returns true if the operation succeeded*/
    bool requestClipResize(QSharedPointer<ClipModel> caller, int newSize);

    /* Perform a move operation on a clip. Returns true if the operation succeeded*/
    bool requestClipMove(QSharedPointer<ClipModel> caller, int newPosition);

    /* Perform a split at the requested position */
    bool splitClip(QSharedPointer<ClipModel> caller, int position);

    /* Implicit conversion operator to access the underlying producer
     */
    operator Mlt::Producer&(){ return m_playlist;}

protected:
    /*@brief Returns the (unique) construction id of the track*/
    int getId() const;

public slots:
    /*Delete the current track and all its associated clips */
    void slotDelete();

private:
    std::weak_ptr<TimelineModel> m_parent;
    int m_id; //this is the creation id of the track, used for book-keeping
    Mlt::Playlist m_playlist;



    std::unordered_map<int, std::shared_ptr<ClipModel>> m_allClips;

};
