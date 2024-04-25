/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "timelinemodel.hpp"
#include "undohelper.hpp"
#include <QReadWriteLock>
#include <memory>

/** @brief This is the base class for objects that can move, for example clips and compositions
 */
template <typename Service> class MoveableItem
{
    MoveableItem() = delete;

protected:
    virtual ~MoveableItem() = default;

public:
    MoveableItem(std::weak_ptr<TimelineModel> parent, int id = -1);

    /** @brief returns (unique) id of current item
     */
    int getId() const;

    /** @brief returns parent timeline UUID
     */
    QUuid getUuid() const;

    /** @brief returns the length of the item on the timeline
     */
    virtual int getPlaytime() const = 0;

    /** @brief returns the id of the track in which this items is inserted (-1 if none)
     */
    int getCurrentTrackId() const;

    /** @brief returns the current position of the item (-1 if not inserted)
     */
    int getPosition() const;

    /** @brief returns the in and out times of the item
     */
    std::pair<int, int> getInOut() const;
    virtual int getIn() const;
    virtual int getOut() const;

    /** @brief Does a clip contain this asset in its effectstack */
    virtual int assetRow(const QString &assetId) const;

    /** @brief Set grab status */
    virtual void setGrab(bool grab) = 0;

    friend class TrackModel;
    friend class TimelineModel;
    /** @brief Implicit conversion operator to access the underlying producer
     */
    operator Service &() { return *service(); }

    /** @brief Returns true if the underlying producer is valid
     */
    bool isValid();

    /** @brief returns a property of the current item
     */
    virtual const QString getProperty(const QString &name) const = 0;

    /** @brief Set if the item is in grab state */
    bool isGrabbed() const;

    /** @brief True if item is selected in timeline */
    bool selected{false};
    /** @brief Set selected status */
    virtual void setSelected(bool sel) = 0;

protected:
    /** @brief Returns a pointer to the service. It may be used but do NOT store it*/
    virtual Service *service() const = 0;

    /** @brief Performs a resize of the given item.
       Returns true if the operation succeeded, and otherwise nothing is modified
       This method is protected because it shouldn't be called directly. Call the function in the timeline instead.
       If a snap point is within reach, the operation will be coerced to use it.
       @param size is the new size of the item
       @param right is true if we change the right side of the item, false otherwise
       @param undo Lambda function containing the current undo stack. Will be updated with current operation
       @param redo Lambda function containing the current redo queue. Will be updated with current operation
    */
    virtual bool requestResize(int size, bool right, Fun &undo, Fun &redo, bool logUndo = true, bool hasMix = false) = 0;

    /** @brief Updates the stored position of the item
      This function is meant to be called by the trackmodel, not directly by the user.
      If you wish to actually move the item, use the requestMove slot.
    */
    virtual void setPosition(int position);
    /** @brief Updates the stored track id of the item
       This function is meant to be called by the timeline, not directly by the user.
       If you wish to actually change the track the item, use the slot in the timeline
       slot.
    */
    virtual void setCurrentTrackId(int tid, bool finalMove = true);

    /** @brief Set in and out of service */
    virtual void setInOut(int in, int out);

protected:
    std::weak_ptr<TimelineModel> m_parent;
    /** @brief this is the creation id of the item, used for book-keeping */
    int m_id;
    int m_position;
    int m_currentTrackId;
    bool m_grabbed;
    /** @brief This is a lock that ensures safety in case of concurrent access */
    mutable QReadWriteLock m_lock;
};

#include "moveableItem.ipp"
