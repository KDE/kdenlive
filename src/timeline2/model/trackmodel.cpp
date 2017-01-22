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



TrackModel::TrackModel(std::weak_ptr<TimelineModel> parent) :
    m_parent(parent)
    , m_id(TimelineModel::getNextId())
{
}

int TrackModel::construct(std::weak_ptr<TimelineModel> parent, int pos)
{
    std::unique_ptr<TrackModel> track(new TrackModel(parent));
    int id = track->m_id;
    if (auto ptr = parent.lock()) {
        ptr->registerTrack(std::move(track), pos);
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
    }
}

int TrackModel::getClipsCount() const
{
    return static_cast<int>(m_allClips.size());
}

bool TrackModel::requestClipInsertion(std::shared_ptr<ClipModel> clip, int position, bool dry)
{
    if (!clip->isValid()) {
        return false;
    }
    // Find out the clip id at position
    int target_clip = m_playlist.get_clip_index_at(position);
    int count = m_playlist.count();

    if (target_clip >= count) {
        if (dry) {
            return true;
        }
        //In that case, we append after
        int index = m_playlist.insert_at(position, *clip, 1);
        if (index == -1) {
            return false;
        }
    } else {
        if (m_playlist.is_blank(target_clip)) {
            int blank_start = m_playlist.clip_start(target_clip);
            int blank_length = m_playlist.clip_length(target_clip);
            int length = clip->getPlaytime();
            if (blank_start + blank_length >= position + length) {
                if (dry) {
                    return true;
                }
                int index = m_playlist.insert_at(position, *clip, 1);
                if (index == -1) {
                    return false;
                }
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
    m_allClips[clip->getId()] = clip;
    clip->setPosition(position);
    return true;
}

bool TrackModel::requestClipDeletion(int cid, bool dry)
{
    Q_ASSERT(m_allClips.count(cid) > 0);
    if (dry) {
        return true;
    }
    //Find index of clip
    int clip_position = m_allClips[cid]->getPosition();
    int target_clip = m_playlist.get_clip_index_at(clip_position);
    Q_ASSERT(target_clip < m_playlist.count());
    auto prod = m_playlist.replace_with_blank(target_clip);
    if (prod != nullptr) {
        m_allClips.erase(cid);
        return true;
    }
    return false;
}


int TrackModel::getId() const
{
    return m_id;
}
