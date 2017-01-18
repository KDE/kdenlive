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


#include "timelinemodel.hpp"
#include "trackmodel.hpp"

#include <mlt++/MltTractor.h>

TimelineModel::TimelineModel() :
    m_tractor()
{

}


void TimelineModel::registerTrack(std::unique_ptr<TrackModel>&& track, int pos)
{
    if (pos == -1) {
        pos = m_allTracks.size();
    }
    //effective insertion (MLT operation)
    int error = m_tractor.insert_track(*track ,pos);
    Q_ASSERT(error == 0); //we might need better error handling...

    // we now insert in the list
    auto it = m_allTracks.begin();
    std::advance(it, pos);
    m_allTracks.insert(it, std::move(track));

}

