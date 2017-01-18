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

#include "trackmodel.hpp"
#include "timelinemodel.hpp"
#include "clipmodel.hpp"
#include <QDebug>


int TrackModel::next_id = 0;

TrackModel::TrackModel(std::weak_ptr<TimelineModel> parent) :
    m_parent(parent)
    , m_id(TrackModel::next_id++)
{
}

int TrackModel::construct(std::weak_ptr<TimelineModel> parent)
{
    std::unique_ptr<TrackModel> track(new TrackModel(parent));
    int id = track->m_id;
    if (auto ptr = parent.lock()) {
        ptr->registerTrack(std::move(track));
    } else {
        qDebug() << "Error : construction of track failed because parent timeline is not available anymore";
        Q_ASSERT(false);
    }
    return id;
}

void TrackModel::destruct()
{
    if (auto ptr = m_parent.lock()) {
        ptr->deregisterTrack(m_id);
    } else {
        qDebug() << "Error : destruction of track failed because parent timeline is not available anymore";
        Q_ASSERT(false);
    }

}

int TrackModel::getId() const
{
    return m_id;
}
