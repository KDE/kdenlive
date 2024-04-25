/*
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "macros.hpp"
#include <utility>

template <typename Service>
MoveableItem<Service>::MoveableItem(std::weak_ptr<TimelineModel> parent, int id)
    : m_parent(std::move(parent))
    , m_id(id == -1 ? TimelineModel::getNextId() : id)
    , m_position(-1)
    , m_currentTrackId(-1)
    , m_grabbed(false)
    , m_lock(QReadWriteLock::Recursive)
{
}

template <typename Service> int MoveableItem<Service>::getId() const
{
    READ_LOCK();
    return m_id;
}

template <typename Service> QUuid MoveableItem<Service>::getUuid() const
{
    READ_LOCK();
    if (auto ptr = m_parent.lock()) {
        return ptr->uuid();
    }
    return QUuid();
}

template <typename Service> int MoveableItem<Service>::getCurrentTrackId() const
{
    READ_LOCK();
    return m_currentTrackId;
}

template <typename Service> int MoveableItem<Service>::getPosition() const
{
    READ_LOCK();
    return m_position;
}

template <typename Service> std::pair<int, int> MoveableItem<Service>::getInOut() const
{
    READ_LOCK();
    return {getIn(), getOut()};
}

template <typename Service> int MoveableItem<Service>::assetRow(const QString &assetId) const
{
    Q_UNUSED(assetId);
    return -1;
}

template <typename Service> int MoveableItem<Service>::getIn() const
{
    READ_LOCK();
    return service()->get_in();
}

template <typename Service> int MoveableItem<Service>::getOut() const
{
    READ_LOCK();
    return service()->get_out();
}

template <typename Service> bool MoveableItem<Service>::isValid()
{
    READ_LOCK();
    return service()->is_valid();
}

template <typename Service> void MoveableItem<Service>::setPosition(int pos)
{
    QWriteLocker locker(&m_lock);
    m_position = pos;
}

template <typename Service> void MoveableItem<Service>::setCurrentTrackId(int tid, bool finalMove)
{
    Q_UNUSED(finalMove);
    QWriteLocker locker(&m_lock);
    m_currentTrackId = tid;
}

template <typename Service> void MoveableItem<Service>::setInOut(int in, int out)
{
    QWriteLocker locker(&m_lock);
    service()->set_in_and_out(in, out);
}

template <typename Service> bool MoveableItem<Service>::isGrabbed() const
{
    READ_LOCK();
    return m_grabbed;
}

