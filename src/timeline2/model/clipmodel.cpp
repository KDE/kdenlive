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
#include "clipmodel.hpp"
#include "timelinemodel.hpp"
#include "trackmodel.hpp"
#include "undohelper.hpp"
#include <mlt++/MltProducer.h>
#include <QDebug>


ClipModel::ClipModel(std::weak_ptr<TimelineModel> parent, std::weak_ptr<Mlt::Producer> prod, int id) :
    m_parent(parent)
    , m_id(id == -1 ? TimelineModel::getNextId() : id)
    , m_position(-1)
    , m_currentTrackId(-1)
    , m_producer(prod)
{
}

int ClipModel::construct(std::weak_ptr<TimelineModel> parent, std::shared_ptr<Mlt::Producer> prod, int id)
{
    std::shared_ptr<ClipModel> clip(new ClipModel(parent, prod, id));
    id = clip->m_id;
    if (auto ptr = parent.lock()) {
        ptr->registerClip(clip);
    } else {
        qDebug() << "Error : construction of clip failed because parent timeline is not available anymore";
        Q_ASSERT(false);
    }

    return id;
}


void ClipModel::destruct()
{
    if (auto ptr = m_parent.lock()) {
        ptr->deregisterClip(m_id);
    } else {
        qDebug() << "Error : destruction of clip failed because parent timeline is not available anymore";
        Q_ASSERT(false);
    }
}

int ClipModel::getId() const
{
    return m_id;
}

int ClipModel::getCurrentTrackId() const
{
    return m_currentTrackId;
}

int ClipModel::getPosition() const
{
    return m_position;
}

std::pair<int, int> ClipModel::getInOut() const
{
    return {m_producer->get_in(), m_producer->get_out()};
}

bool ClipModel::isValid()
{
    return m_producer->is_valid();
}

bool ClipModel::requestResize(int size, bool right, Fun& undo, Fun& redo)
{
    if (size <= 0 || size > m_producer->get_length()) {
        return false;
    }
    int delta = getPlaytime() - size;
    int in = m_producer->get_in();
    int out = m_producer->get_out();
    int old_in = in, old_out = out;
    //check if there is enough space on the chosen side
    if ((!right && in + delta < 0) || (right &&  out - delta >= m_producer->get_length())) {
        return false;
    }
    if (right) {
        out -= delta;
    } else {
        in += delta;
    }

    std::function<bool (void)> track_operation = [](){return true;};
    std::function<bool (void)> track_reverse = [](){return true;};
    if (m_currentTrackId != -1) {
        if (auto ptr = m_parent.lock()) {
            track_operation = ptr->getTrackById(m_currentTrackId)->requestClipResize_lambda(m_id, in, out, right);
        } else {
            qDebug() << "Error : Moving clip failed because parent timeline is not available anymore";
            Q_ASSERT(false);
        }
    }
    auto operation = [this, in, out, track_operation]() {
        if (track_operation()) {
            m_producer.reset(m_producer->cut(in, out));
            return true;
        }
        return false;
    };
    if (operation()) {
        // Now, we are in the state in which the timeline should be when we try to revert current action. So we can build the reverse action from here
        auto ptr = m_parent.lock();
        if (m_currentTrackId != -1 && ptr) {
            track_reverse = ptr->getTrackById(m_currentTrackId)->requestClipResize_lambda(m_id, old_in, old_out, right);
        }
        auto reverse = [this, old_in, old_out, track_reverse]() {
            if (track_reverse()) {
                m_producer.reset(m_producer->cut(old_in, old_out));
                return true;
            }
            return false;
        };
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

int ClipModel::getPlaytime()
{
    return m_producer->get_playtime();
}

void ClipModel::setPosition(int pos)
{
    m_position = pos;
}

void ClipModel::setCurrentTrackId(int tid)
{
    m_currentTrackId = tid;
}
