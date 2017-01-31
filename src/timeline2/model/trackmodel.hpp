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
    friend class ClipModel;
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
    int getClipsCount();

    /* @brief Performs an insertion of the given clip.
       Returns true if the operation succeeded, and otherwise, the track is not modified.
       @param clip is a shared pointer to the clip
       @param position is the position where to insert the clip
       @param dry If this parameter is true, no action is actually executed, but we return true if it would be possible to do it.
    */
    bool requestClipInsertion(std::shared_ptr<ClipModel> clip, int position, bool dry = false);

    /* @brief Performs an deletion of the given clip.
       Returns true if the operation succeeded, and otherwise, the track is not modified.
       @param cid is the id of the clip
       @param dry If this parameter is true, no action is actually executed, but we return true if it would be possible to do it.
    */
    bool requestClipDeletion(int cid, bool dry = false);


    /* Perform a move operation on a clip. Returns true if the operation succeeded*/
    bool requestClipMove(QSharedPointer<ClipModel> caller, int newPosition);

    /* Perform a split at the requested position */
    bool splitClip(QSharedPointer<ClipModel> caller, int position);

    /* Implicit conversion operator to access the underlying producer
     */
    operator Mlt::Producer&(){ return m_playlist;}

    /* Implicit conversion operator to access the underlying producer
     */    
    // TODO make protected
    QVariant getProperty(const QString &name);
    void setProperty(const QString &name, const QString &value);

protected:
    /* @brief Performs a resize of the given clip.
       Returns true if the operation succeeded, and otherwise nothing is modified
       This method is protected because it shouldn't be called directly. Call the function in the clip instead.
       @param cid is the id of the clip
       @param in is the new starting on the clip
       @param out is the new ending on the clip
       @param right is true if we change the right side of the clip, false otherwise
       @param dry If this parameter is true, no action is actually executed, but we return true if it would be possible to do it.
    */
    bool requestClipResize(int cid, int in, int out, bool right, bool dry = false);

    /*@brief Returns the (unique) construction id of the track*/
    int getId() const;

    /*@brief This function is used only by the QAbstractItemModel
      Given a row in the model, retrieves the corresponding clip id. If it does not exist, returns -1
    */
    int getClipByRow(int row) const;

    /*@brief This is an helper function that test frame level consistancy with the MLT structures */
    bool checkConsistency();
public slots:
    /*Delete the current track and all its associated clips */
    void slotDelete();

private:
    std::weak_ptr<TimelineModel> m_parent;
    int m_id; //this is the creation id of the track, used for book-keeping
    Mlt::Playlist m_playlist;

    int m_currentInsertionOrder;


    std::unordered_map<int, std::shared_ptr<ClipModel>> m_allClips;

    std::map<int, int> m_clipsByInsertionOrder; //This map stores the order in which the clips were inserted. This implicitly gives the model Row of each clip since they remain sorted even after deletion. Note that it is important to keep an ordered data structure such has std::map (QMap is NOT ordered).

    std::unordered_map<int, int> m_insertionOrder; //This is a reverse map of m_clipsByInsertionOrder. Note that order is not important thus we can use an unordered_map.

};
