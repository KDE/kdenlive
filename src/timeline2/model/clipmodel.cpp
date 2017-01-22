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
#include <mlt++/MltProducer.h>
#include <QDebug>


ClipModel::ClipModel(std::weak_ptr<TimelineModel> parent, std::weak_ptr<Mlt::Producer> prod) :
    m_parent(parent)
    , m_id(TimelineModel::getNextId())
    , m_position(-1)
    , m_currentTrackId(-1)
    , m_producer(prod)
{
}

int ClipModel::construct(std::weak_ptr<TimelineModel> parent, std::shared_ptr<Mlt::Producer> prod)
{
    std::shared_ptr<ClipModel> clip(new ClipModel(parent, prod));
    int id = clip->m_id;
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
