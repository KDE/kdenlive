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
#include "transitions/transitionsrepository.hpp"
#include "undohelper.hpp"
#include <QDebug>
#include <mlt++/MltTransition.h>

CompositionModel::CompositionModel(std::weak_ptr<TimelineModel> parent, Mlt::Transition *transition, int id, const QDomElement &transitionXml,
                                   const QString &transitionId)
    : AssetParameterModel(transition, transitionXml, transitionId)
    , MoveableItem<Mlt::Transition>(parent, id)
    , m_atrack(-1)
{
}

int CompositionModel::construct(const std::weak_ptr<TimelineModel> &parent, const QString &transitionId, int id)
{
    auto xml = TransitionsRepository::get()->getXml(transitionId);
    Mlt::Transition *transition = TransitionsRepository::get()->getTransition(transitionId);
    transition->set_in_and_out(0, 0);
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

bool CompositionModel::requestResize(int size, bool right, Fun &undo, Fun &redo)
{
    if (size <= 0) {
        return false;
    }
    int delta = getPlaytime() - size;
    qDebug() << "compo request resize " << size << right << delta;
    int in = getIn();
    int out = getOut();
    int old_in = in, old_out = out;
    if (right) {
        out -= delta;
    } else {
        in += delta;
    }
    // if the in becomes negative, we add the necessary length in out.
    if (in < 0) {
        out = out - in;
        in = 0;
    }

    std::function<bool(void)> track_operation = []() { return true; };
    std::function<bool(void)> track_reverse = []() { return true; };
    if (m_currentTrackId != -1) {
        if (auto ptr = m_parent.lock()) {
            track_operation = ptr->getTrackById(m_currentTrackId)->requestCompositionResize_lambda(m_id, in, out);
        } else {
            qDebug() << "Error : Moving composition failed because parent timeline is not available anymore";
            Q_ASSERT(false);
        }
    }
    Fun operation = [in, out, track_operation, this]() {
        if (track_operation()) {
            setInOut(in, out);
            return true;
        }
        return false;
    };
    if (operation()) {
        // Now, we are in the state in which the timeline should be when we try to revert current action. So we can build the reverse action from here
        auto ptr = m_parent.lock();
        if (m_currentTrackId != -1 && ptr) {
            track_reverse = ptr->getTrackById(m_currentTrackId)->requestCompositionResize_lambda(m_id, old_in, old_out);
        }
        Fun reverse = [old_in, old_out, track_reverse, this]() {
            if (track_reverse()) {
                setInOut(old_in, old_out);
                return true;
            }
            return false;
        };
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

const QString CompositionModel::getProperty(const QString &name) const
{
    return QString::fromUtf8(service()->get(name.toUtf8().constData()));
}

Mlt::Transition *CompositionModel::service() const
{
    return static_cast<Mlt::Transition *>(m_asset.get());
}

int CompositionModel::getPlaytime() const
{
    return getOut() - getIn() + 1;
}

int CompositionModel::getATrack() const
{
    return m_atrack;
}

void CompositionModel::setATrack(int trackId)
{
    Q_ASSERT(trackId != getCurrentTrackId()); // can't compose with same track
    m_atrack = trackId;
}
