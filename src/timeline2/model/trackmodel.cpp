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

int TrackModel::getClipsCount()
{
    int count = 0;
    for (int i = 0; i < m_playlist.count(); i++) {
        if (!m_playlist.is_blank(i)) {
            count++;
        }
    }
    Q_ASSERT(count == static_cast<int>(m_allClips.size()));
    return count;
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
    Q_ASSERT(!m_playlist.is_blank(target_clip));
    auto prod = m_playlist.replace_with_blank(target_clip);
    if (prod != nullptr) {
        m_playlist.consolidate_blanks();
        m_allClips.erase(cid);
        return true;
    }
    return false;
}

bool TrackModel::requestClipResize(int cid, int in, int out, bool right, bool dry)
{
    //Find index of clip
    Q_ASSERT(m_allClips.count(cid) > 0);
    int clip_position = m_allClips[cid]->getPosition();
    int target_clip = m_playlist.get_clip_index_at(clip_position);
    Q_ASSERT(target_clip < m_playlist.count());
    int size = out - in + 1;

    int delta = m_allClips[cid]->getPlaytime() - size;
    if (delta == 0) {
        return true;
    }
    if (delta > 0) {
        if (dry) {
            return true;
        }
        int blank_index = right ? (target_clip + 1) : target_clip;
        // insert blank to space that is going to be empty
        // The second is parameter is delta - 1 because this function expects an out time, which is basically size - 1
        m_playlist.insert_blank(blank_index, delta - 1);
        if (!right) {
            m_allClips[cid]->setPosition(clip_position + delta);
            //Because we inserted blank before, the index of our clip has increased
            target_clip++;
        }
        int err = m_playlist.resize_clip(target_clip, in, out);
        //make sure to do this after, to avoid messing the indexes
        m_playlist.consolidate_blanks();
        return err == 0;
    } else {
        int blank = -1;
        if (right) {
            if (target_clip == m_playlist.count() - 1) {
                //clip is last, it can always be extended
                int err = m_playlist.resize_clip(target_clip, in, out);
                return err == 0;
            }
            blank = target_clip + 1;
        } else {
            if (target_clip == 0) {
                //clip is first, it can never be extended on the left
                return false;
            }
            blank = target_clip - 1;
        }
        if (m_playlist.is_blank(blank)) {
            int blank_length = m_playlist.clip_length(blank);
            if (blank_length >= m_allClips[cid]->getPlaytime()) {
                if (dry) {
                    return true;
                }
                int err = 0;
                if (blank_length + delta == 0) {
                    err = m_playlist.remove(blank);
                    if (!right) {
                        target_clip--;
                    }
                } else {
                    err = m_playlist.resize_clip(blank, 0, blank_length + delta - 1);
                }
                if (err == 0) {
                    err = m_playlist.resize_clip(target_clip, in, out);
                }
                if (!right && err == 0) {
                    m_allClips[cid]->setPosition(m_playlist.clip_start(target_clip));
                }
                return err == 0;
            }
        }
    }
    return false;
}

int TrackModel::getId() const
{
    return m_id;
}

bool TrackModel::checkConsistency()
{
    std::vector<std::pair<int, int> > clips; //clips stored by (position, id)
    for (const auto& c : m_allClips) {
        if (c.second)
            clips.push_back({c.second->getPosition(), c.first});
    }
    std::sort(clips.begin(), clips.end());
    size_t current_clip = 0;
    for(int i = 0; i < m_playlist.get_playtime(); i++) {
        auto clip = m_allClips[clips[current_clip].second];
        int index = m_playlist.get_clip_index_at(i);
        if (current_clip < clips.size() && i >= clips[current_clip].first) {
            if (i >= clips[current_clip].first + clip->getPlaytime()) {
                current_clip++;
                i--;
                continue;
            } else {
                Mlt::Producer prod(m_playlist.get_clip(index));
                if (m_playlist.is_blank(index)) {
                    qDebug() << "ERROR: Found blank when clip was required at position " << i;
                    qDebug() << "Blank size" << m_playlist.clip_length(index);
                    return false;
                }
                if (!prod.same_clip(*clip)) {
                    qDebug() << "ERROR: Wrong clip at position " << i;
                    return false;
                }
            }
        } else {
            if (!m_playlist.is_blank_at(i)) {
                qDebug() << "ERROR: Found clip when blank was required at position " << i;
                return false;
            }
        }
    }
    return true;
}
