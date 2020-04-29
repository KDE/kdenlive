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

#ifndef MOVEABLEITEM_H
#define MOVEABLEITEM_H

#include "timelinemodel.hpp"
#include "undohelper.hpp"
#include <QReadWriteLock>
#include <memory>

/* @brief This is the base class for objects that can move, for example clips and compositions
 */
template <typename Service> class MoveableItem
{
    MoveableItem() = delete;

protected:
    virtual ~MoveableItem() = default;

public:
    MoveableItem(std::weak_ptr<TimelineModel> parent, int id = -1);

    /* @brief returns (unique) id of current item
     */
    int getId() const;

    /* @brief returns the length of the item on the timeline
     */
    virtual int getPlaytime() const = 0;

    /* @brief returns the id of the track in which this items is inserted (-1 if none)
     */
    int getCurrentTrackId() const;

    /* @brief returns the current position of the item (-1 if not inserted)
     */
    int getPosition() const;

    /* @brief returns the in and out times of the item
     */
    std::pair<int, int> getInOut() const;
    virtual int getIn() const;
    virtual int getOut() const;

    /* Set grab status */
    virtual void setGrab(bool grab) = 0;

    friend class TrackModel;
    friend class TimelineModel;
    /* Implicit conversion operator to access the underlying producer
     */
    operator Service &() { return *service(); }

    /* Returns true if the underlying producer is valid
     */
    bool isValid();

    /* @brief returns a property of the current item
     */
    virtual const QString getProperty(const QString &name) const = 0;

    /* Set if the item is in grab state */
    bool isGrabbed() const;

    /* True if item is selected in timeline */
    bool selected {false};
    /* Set selected status */
    virtual void setSelected(bool sel) = 0;

protected:
    /* @brief Returns a pointer to the service. It may be used but do NOT store it*/
    virtual Service *service() const = 0;

    /* @brief Performs a resize of the given item.
       Returns true if the operation succeeded, and otherwise nothing is modified
       This method is protected because it shouldn't be called directly. Call the function in the timeline instead.
       If a snap point is within reach, the operation will be coerced to use it.
       @param size is the new size of the item
       @param right is true if we change the right side of the item, false otherwise
       @param undo Lambda function containing the current undo stack. Will be updated with current operation
       @param redo Lambda function containing the current redo queue. Will be updated with current operation
    */
    virtual bool requestResize(int size, bool right, Fun &undo, Fun &redo, bool logUndo = true) = 0;

    /* Updates the stored position of the item
      This function is meant to be called by the trackmodel, not directly by the user.
      If you wish to actually move the item, use the requestMove slot.
    */
    virtual void setPosition(int position);
    /* Updates the stored track id of the item
       This function is meant to be called by the timeline, not directly by the user.
       If you wish to actually change the track the item, use the slot in the timeline
       slot.
    */
    virtual void setCurrentTrackId(int tid, bool finalMove = true);

    /* Set in and out of service */
    virtual void setInOut(int in, int out);

protected:
    std::weak_ptr<TimelineModel> m_parent;
    int m_id; // this is the creation id of the item, used for book-keeping
    int m_position;
    int m_currentTrackId;
    bool m_grabbed;
    mutable QReadWriteLock m_lock; // This is a lock that ensures safety in case of concurrent access
};

#include "moveableItem.ipp"
#endif
