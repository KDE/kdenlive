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

#ifndef TRACKMODEL_H
#define TRACKMODEL_H

#include <memory>
#include <QSharedPointer>
#include <unordered_map>
#include <mlt++/MltPlaylist.h>
#include <mlt++/MltTractor.h>
#include "undohelper.hpp"

class TimelineModel;
class ClipModel;

/* @brief This class represents a Track object, as viewed by the backend.
   To allow same track transitions, a Track object corresponds to two Mlt::Playlist, between which we can switch when required by the transitions.
   In general, the Gui associated with it will send modification queries (such as resize or move), and this class authorize them or not depending on the validity of the modifications
*/
class TrackModel
{

public:
    TrackModel() = delete;
    ~TrackModel();

    friend class TimelineModel;
    friend class TimelineItemModel;
    friend class ClipModel;
private:
    /* This constructor is private, call the static construct instead */
    TrackModel(std::weak_ptr<TimelineModel> parent, int id = -1);
public:
    /* @brief Creates a track, which references itself to the parent
       Returns the (unique) id of the created track
       @param id Requested id of the track. Automatic if id = -1
       @param pos is the optional position of the track. If left to -1, it will be added at the end
     */
    static int construct(std::weak_ptr<TimelineModel> parent, int id = -1, int pos = -1);

    /* @brief returns the number of clips */
    int getClipsCount();

    /* Perform a split at the requested position */
    bool splitClip(QSharedPointer<ClipModel> caller, int position);

    /* Implicit conversion operator to access the underlying producer
     */
    operator Mlt::Producer&(){ return m_track;}

    // TODO make protected
    QVariant getProperty(const QString &name);
    void setProperty(const QString &name, const QString &value);

protected:
    /* @brief Returns a lambda that performs a resize of the given clip.
       The lamda returns true if the operation succeeded, and otherwise nothing is modified
       This method is protected because it shouldn't be called directly. Call the function in the timeline instead.
       @param cid is the id of the clip
       @param in is the new starting on the clip
       @param out is the new ending on the clip
       @param right is true if we change the right side of the clip, false otherwise
    */
    Fun requestClipResize_lambda(int cid, int in, int out, bool right);

    /* @brief Performs an insertion of the given clip.
       Returns true if the operation succeeded, and otherwise, the track is not modified.
       This method is protected because it shouldn't be called directly. Call the function in the timeline instead.
       @param clip is the id of the clip
       @param position is the position where to insert the clip
       @param undo Lambda function containing the current undo stack. Will be updated with current operation
       @param redo Lambda function containing the current redo queue. Will be updated with current operation
    */
    bool requestClipInsertion(int cid, int position, Fun& undo, Fun& redo);
    /* @brief This function returns a lambda that performs the requested operation */
    Fun requestClipInsertion_lambda(int cid, int position);

    /* @brief Performs an deletion of the given clip.
       Returns true if the operation succeeded, and otherwise, the track is not modified.
       This method is protected because it shouldn't be called directly. Call the function in the timeline instead.
       @param cid is the id of the clip
       @param undo Lambda function containing the current undo stack. Will be updated with current operation
       @param redo Lambda function containing the current redo queue. Will be updated with current operation
    */
    bool requestClipDeletion(int cid, Fun& undo, Fun& redo);
    /* @brief This function returns a lambda that performs the requested operation */
    Fun requestClipDeletion_lambda(int cid);

    /* @brief Returns the size of the blank before or after the given clip
       @param cid is the id of the clip
       @param after is true if we query the blank after, false otherwise
    */
    int getBlankSizeNearClip(int cid, bool after);

    /*@brief Returns the (unique) construction id of the track*/
    int getId() const;

    /*@brief This function is used only by the QAbstractItemModel
      Given a row in the model, retrieves the corresponding clip id. If it does not exist, returns -1
    */
    int getClipByRow(int row) const;
    /*@brief This function is used only by the QAbstractItemModel
      Given a clip ID, returns the row of the clip.
    */
    int getRowfromClip(int cid) const;

    /*@brief This is an helper function that test frame level consistancy with the MLT structures */
    bool checkConsistency();


    /* @brief This is an helper function that returns the sub-playlist in which the clip is inserted, along with its index in the playlist
     @param position the position of the target clip*/
    std::pair<int, int> getClipIndexAt(int position);

    /* @brief This is an helper function that checks in all playlists if the given position is a blank */
    bool isBlankAt(int position);

    /* @brief This is an helper function that returns the end of the blank that covers given position */
    int getBlankEnd(int position);
    /* Same, but we restrict to a specific track*/
    int getBlankEnd(int position, int track);
public slots:
    /*Delete the current track and all its associated clips */
    void slotDelete();

private:
    std::weak_ptr<TimelineModel> m_parent;
    int m_id; //this is the creation id of the track, used for book-keeping

    // We fake two playlists to allow same track transitions.
    Mlt::Tractor m_track;
    Mlt::Playlist m_playlists[2];

    int m_currentInsertionOrder;


    std::map<int, std::shared_ptr<ClipModel>> m_allClips; /*this is important to keep an
                                                            ordered structure to store the clips, since we use their ids order as row order*/


};

#endif
