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
#include "compositionmodel.hpp"
#include "timelinemodel.hpp"
#include "trackmodel.hpp"
#include "undohelper.hpp"
#include <mlt++/MltTransition.h>
#include <QDebug>
#include "transitions/transitionsrepository.hpp"


CompositionModel::CompositionModel(std::weak_ptr<TimelineModel> parent, Mlt::Transition* transition, int id, const QDomElement &transitionXml, const QString &transitionId) :
    AssetParameterModel(transition, transitionXml, transitionId)
    , m_parent(parent)
    , m_id(id == -1 ? TimelineModel::getNextId() : id)
    , m_position(-1)
    , m_currentTrackId(-1)
{
}

int CompositionModel::construct(std::weak_ptr<TimelineModel> parent, const QString &transitionId, int id)
{
    auto xml = TransitionsRepository::get()->getXml(transitionId);
    Mlt::Transition *transition = TransitionsRepository::get()->getTransition(transitionId);
    std::shared_ptr<CompositionModel> composition(new CompositionModel(parent, transition, id, xml, transitionId));
    id = composition->m_id;
    if (auto ptr = parent.lock()) {
        ptr->registerComposition(composition);
    } else {
        qDebug() << "Error : construction of composition failed because parent timeline is not available anymore";
        Q_ASSERT(false);
    }

    return id;
}


int CompositionModel::getId() const
{
    return m_id;
}

int CompositionModel::getCurrentTrackId() const
{
    return m_currentTrackId;
}

int CompositionModel::getPosition() const
{
    return m_position;
}

int CompositionModel::getPlaytime() const
{
    return transition()->get_length();
}

const QString CompositionModel::getProperty(const QString &name) const
{
    return QString::fromUtf8(transition()->get(name.toUtf8().constData()));
}

std::pair<int, int> CompositionModel::getInOut() const
{
    return {transition()->get_in(), transition()->get_out()};
}

int CompositionModel::getIn() const
{
    return transition()->get_in();
}

int CompositionModel::getOut() const
{
    return transition()->get_out();
}

bool CompositionModel::isValid()
{
    return transition()->is_valid();
}

bool CompositionModel::requestResize(int size, bool right, Fun& undo, Fun& redo)
{
    if (size <= 0 || size > transition()->get_length()) {
        return false;
    }
    int delta = transition()->get_length() - size;
    int in = transition()->get_in();
    int out = transition()->get_out();
    int old_in = in, old_out = out;
    //check if there is enough space on the chosen side
    if ((!right && in + delta < 0) || (right &&  out - delta >= transition()->get_length())) {
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
            track_operation = ptr->requestCompositionResize_lambda(m_id, in, out, right);
        } else {
            qDebug() << "Error : Moving clip failed because parent timeline is not available anymore";
            Q_ASSERT(false);
        }
    }
    auto operation = [this, in, out, track_operation]() {
        if (track_operation()) {
            transition()->set_in_and_out(in, out);
            return true;
        }
        return false;
    };
    if (operation()) {
        // Now, we are in the state in which the timeline should be when we try to revert current action. So we can build the reverse action from here
        auto ptr = m_parent.lock();
        if (m_currentTrackId != -1 && ptr) {
            track_reverse = ptr->requestCompositionResize_lambda(m_id, old_in, old_out, right);
        }
        auto reverse = [this, old_in, old_out, track_reverse]() {
            if (track_reverse()) {
                transition()->set_in_and_out(old_in, old_out);
                return true;
            }
            return false;
        };
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }*/
    return false;
}

void CompositionModel::setPosition(int pos)
{
    m_position = pos;
    int length = transition()->get_length();
    transition()->set_in_and_out(pos, pos + length);
}

void CompositionModel::setInOut(int in, int out)
{
    m_position = in;
    transition()->set_in_and_out(in, out);
}

void CompositionModel::setCurrentTrackId(int tid)
{
    m_currentTrackId = tid;
    //transition()->set_tracks(transition()->get_a_track(), tid);
}

Mlt::Transition* CompositionModel::transition() const
{
    return static_cast<Mlt::Transition*>(m_asset.get());
}
