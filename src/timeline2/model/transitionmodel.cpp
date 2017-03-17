/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
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
#include "transitionmodel.hpp"
#include "timelinemodel.hpp"
#include "trackmodel.hpp"
#include "undohelper.hpp"
#include <mlt++/MltTransition.h>
#include <QDebug>


TransitionModel::TransitionModel(std::weak_ptr<TimelineModel> parent, std::weak_ptr<Mlt::Transition> trans, int id) :
    m_parent(parent)
    , m_id(id == -1 ? TimelineModel::getNextId() : id)
    , m_position(-1)
    , m_currentTrackId(-1)
    , m_transition(trans)
{
}

int TransitionModel::construct(std::weak_ptr<TimelineModel> parent, std::shared_ptr<Mlt::Transition> trans, int id)
{
    std::shared_ptr<TransitionModel> transition(new TransitionModel(parent, trans, id));
    id = transition->m_id;
    if (auto ptr = parent.lock()) {
        ptr->registerTransition(transition);
    } else {
        qDebug() << "Error : construction of transition failed because parent timeline is not available anymore";
        Q_ASSERT(false);
    }

    return id;
}


int TransitionModel::getId() const
{
    return m_id;
}

int TransitionModel::getCurrentTrackId() const
{
    return m_currentTrackId;
}

int TransitionModel::getPosition() const
{
    return m_position;
}

int TransitionModel::getPlaytime() const
{
    return m_transition->get_length();
}

const QString TransitionModel::getProperty(const QString &name) const
{
    return QString::fromUtf8(m_transition->get(name.toUtf8().constData()));
}

std::pair<int, int> TransitionModel::getInOut() const
{
    return {m_transition->get_in(), m_transition->get_out()};
}

int TransitionModel::getIn() const
{
    return m_transition->get_in();
}

int TransitionModel::getOut() const
{
    return m_transition->get_out();
}

bool TransitionModel::isValid()
{
    return m_transition->is_valid();
}

bool TransitionModel::requestResize(int size, bool right, Fun& undo, Fun& redo)
{
    if (size <= 0 || size > m_transition->get_length()) {
        return false;
    }
    int delta = m_transition->get_length() - size;
    int in = m_transition->get_in();
    int out = m_transition->get_out();
    int old_in = in, old_out = out;
    //check if there is enough space on the chosen side
    if ((!right && in + delta < 0) || (right &&  out - delta >= m_transition->get_length())) {
        return false;
    }
    if (right) {
        out -= delta;
    } else {
        in += delta;
    }

    std::function<bool (void)> track_operation = [](){return true;};
    std::function<bool (void)> track_reverse = [](){return true;};
    /*if (m_currentTrackId != -1) {
        if (auto ptr = m_parent.lock()) {
            track_operation = ptr->requestTransitionResize_lambda(m_id, in, out, right);
        } else {
            qDebug() << "Error : Moving clip failed because parent timeline is not available anymore";
            Q_ASSERT(false);
        }
    }
    auto operation = [this, in, out, track_operation]() {
        if (track_operation()) {
            m_transition->set_in_and_out(in, out);
            return true;
        }
        return false;
    };
    if (operation()) {
        // Now, we are in the state in which the timeline should be when we try to revert current action. So we can build the reverse action from here
        auto ptr = m_parent.lock();
        if (m_currentTrackId != -1 && ptr) {
            track_reverse = ptr->requestTransitionResize_lambda(m_id, old_in, old_out, right);
        }
        auto reverse = [this, old_in, old_out, track_reverse]() {
            if (track_reverse()) {
                m_transition->set_in_and_out(old_in, old_out);
                return true;
            }
            return false;
        };
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }*/
    return false;
}

void TransitionModel::setPosition(int pos)
{
    m_position = pos;
    int length = m_transition->get_length();
    m_transition->set_in_and_out(pos, pos + length);
}

void TransitionModel::setInOut(int in, int out)
{
    m_position = in;
    m_transition->set_in_and_out(in, out);
}

void TransitionModel::setCurrentTrackId(int tid)
{
    m_currentTrackId = tid;
    //m_transition->set_tracks(m_transition->get_a_track(), tid);
}
