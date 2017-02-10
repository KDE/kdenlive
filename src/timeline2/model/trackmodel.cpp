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
    , m_currentInsertionOrder(0)
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
    Q_ASSERT(count == static_cast<int>(m_clipsByInsertionOrder.size()));
    return count;
}

Fun TrackModel::requestClipInsertion_lambda(std::shared_ptr<ClipModel> clip, int position)
{
    if (!clip->isValid()) {
        return [](){return false;};
    }
    // Find out the clip id at position
    int target_clip = m_playlist.get_clip_index_at(position);
    int count = m_playlist.count();

    //we create the function that has to be executed after the melt order. This is essentially book-keeping
    auto end_function = [clip, this, position]() {
        m_allClips[clip->getId()] = clip;  //store clip
        qDebug() << "INSERTED CLIP "<<m_allClips[clip->getId()]->getPosition();
        //update insertion order of the clip
        m_insertionOrder[clip->getId()] = m_currentInsertionOrder;
        m_clipsByInsertionOrder[m_currentInsertionOrder] = clip->getId();
        m_currentInsertionOrder++;
        //update clip position
        clip->setPosition(position);
        return true;
    };
    if (target_clip >= count) {
        //In that case, we append after
        return [this, position, clip, end_function]() {
            int index = m_playlist.insert_at(position, *clip, 1);
            return index != -1 && end_function();
        };
    } else {
        if (m_playlist.is_blank(target_clip)) {
            int blank_start = m_playlist.clip_start(target_clip);
            int blank_length = m_playlist.clip_length(target_clip);
            int length = clip->getPlaytime();
            if (blank_start + blank_length >= position + length) {
                return [this, position, clip, end_function]() {
                    int index = m_playlist.insert_at(position, *clip, 1);
                    return index != -1 && end_function();
                };
            }
        }
    }
    return [](){return false;};
}

bool TrackModel::requestClipInsertion(std::shared_ptr<ClipModel> clip, int position, Fun& undo, Fun& redo)
{
    if (!clip->isValid()) {
        return false;
    }
    int cid = clip->getId();

    auto operation = requestClipInsertion_lambda(clip, position);
    if (operation()) {
        auto reverse = requestClipDeletion_lambda(cid);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

Fun TrackModel::requestClipDeletion_lambda(int cid)
{
    //Find index of clip
    int clip_position = m_allClips[cid]->getPosition();
    return [clip_position, cid, this]() {
        int target_clip = m_playlist.get_clip_index_at(clip_position);
        Q_ASSERT(target_clip < m_playlist.count());
        Q_ASSERT(!m_playlist.is_blank(target_clip));
        auto prod = m_playlist.replace_with_blank(target_clip);
        if (prod != nullptr) {
            m_playlist.consolidate_blanks();
            m_allClips.erase(cid);
            m_clipsByInsertionOrder.erase(m_insertionOrder[cid]);
            m_insertionOrder.erase(cid);
            delete prod;
            return true;
        }
        return false;
    };
}

bool TrackModel::requestClipDeletion(int cid, Fun& undo, Fun& redo)
{
    Q_ASSERT(m_allClips.count(cid) > 0);
    auto old_clip = m_allClips[cid];
    int old_position = old_clip->getPosition();
    auto operation = requestClipDeletion_lambda(cid);
    if (operation()) {
        auto reverse = requestClipInsertion_lambda(old_clip, old_position);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

Fun TrackModel::requestClipResize_lambda(int cid, int in, int out, bool right)
{
    int clip_position = m_allClips[cid]->getPosition();
    int target_clip = m_playlist.get_clip_index_at(clip_position);
    Q_ASSERT(target_clip < m_playlist.count());
    int size = out - in + 1;

    int delta = m_allClips[cid]->getPlaytime() - size;
    if (delta == 0) {
        return [](){return true;};
    }
    if (delta > 0) { //we shrink clip
        return [right, target_clip, clip_position, delta, in, out, cid, this](){
            int target_clip_mutable = target_clip;
            int blank_index = right ? (target_clip_mutable + 1) : target_clip_mutable;
            // insert blank to space that is going to be empty
            // The second is parameter is delta - 1 because this function expects an out time, which is basically size - 1
            m_playlist.insert_blank(blank_index, delta - 1);
            if (!right) {
                m_allClips[cid]->setPosition(clip_position + delta);
                //Because we inserted blank before, the index of our clip has increased
                target_clip_mutable++;
            }
            int err = m_playlist.resize_clip(target_clip_mutable, in, out);
            //make sure to do this after, to avoid messing the indexes
            m_playlist.consolidate_blanks();
            return err == 0;
        };
    } else {
        int blank = -1;
        if (right) {
            if (target_clip == m_playlist.count() - 1) {
                //clip is last, it can always be extended
                return [this, target_clip, in, out]() {
                    int err = m_playlist.resize_clip(target_clip, in, out);
                    return err == 0;
                };
            }
            blank = target_clip + 1;
        } else {
            if (target_clip == 0) {
                //clip is first, it can never be extended on the left
                return [](){return false;};
            }
            blank = target_clip - 1;
        }
        if (m_playlist.is_blank(blank)) {
            int blank_length = m_playlist.clip_length(blank);
            if (blank_length + delta >= 0) {
                return [blank_length, blank, right, cid, delta, this, in, out, target_clip](){
                    int target_clip_mutable = target_clip;
                    int err = 0;
                    if (blank_length + delta == 0) {
                        err = m_playlist.remove(blank);
                        if (!right) {
                            target_clip_mutable--;
                        }
                    } else {
                        err = m_playlist.resize_clip(blank, 0, blank_length + delta - 1);
                    }
                    if (err == 0) {
                        err = m_playlist.resize_clip(target_clip_mutable, in, out);
                    }
                    if (!right && err == 0) {
                        m_allClips[cid]->setPosition(m_playlist.clip_start(target_clip_mutable));
                    }
                    return err == 0;
                };
            }
        }
    }
    return [](){return false;};
}

int TrackModel::getId() const
{
    return m_id;
}

int TrackModel::getClipByRow(int row) const
{
    if (row >= static_cast<int>(m_allClips.size())) {
        return -1;
    }
    auto it = m_clipsByInsertionOrder.cbegin();
    std::advance(it, row);
    return (*it).second;
}

int TrackModel::getRowfromClip(int cid) const
{
    Q_ASSERT(m_allClips.count(cid) > 0);
    int order = m_insertionOrder.at(cid);
    auto it = m_clipsByInsertionOrder.find(order);
    Q_ASSERT(it != m_clipsByInsertionOrder.end());
    return (int)std::distance(m_clipsByInsertionOrder.begin(), it);
}

QVariant TrackModel::getProperty(const QString &name)
{
    return m_playlist.get(name.toUtf8().constData());
}

void TrackModel::setProperty(const QString &name, const QString &value)
{
    m_playlist.set(name.toUtf8().constData(), value.toUtf8().constData());
}

bool TrackModel::checkConsistency()
{
    std::vector<std::pair<int, int> > clips; //clips stored by (position, id)
    for (const auto& c : m_allClips) {
        if (c.second) {
            clips.push_back({c.second->getPosition(), c.first});
        }
    }
    std::sort(clips.begin(), clips.end());
    size_t current_clip = 0;
    for(int i = 0; i < m_playlist.get_playtime(); i++) {
        int index = m_playlist.get_clip_index_at(i);
        if (current_clip < clips.size() && i >= clips[current_clip].first) {
            auto clip = m_allClips[clips[current_clip].second];
            if (i >= clips[current_clip].first + clip->getPlaytime()) {
                current_clip++;
                i--;
                continue;
            } else {
                if (m_playlist.is_blank(index)) {
                    qDebug() << "ERROR: Found blank when clip was required at position " << i;
                    qDebug() << "Blank size" << m_playlist.clip_length(index);
                    return false;
                }
                auto pr = m_playlist.get_clip(index);
                Mlt::Producer prod(pr);
                if (!prod.same_clip(*clip)) {
                    qDebug() << "ERROR: Wrong clip at position " << i;
                    delete pr;
                    return false;
                }
                delete pr;
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

bool TrackModel::allowClipMove(int cid, int position, int length)
{
    // Check position is blank or is in cid
    int ix = m_playlist.get_clip_index_at(position);
    int blankEnd = 0;
    int target_clip = -1;
    if (m_allClips.count(cid) > 0) { //check if the clip is in the track
        int clip_position = m_allClips[cid]->getPosition();
        target_clip = m_playlist.get_clip_index_at(clip_position);
    }
    while (blankEnd < position + length && ix < m_playlist.count()) {
        if (m_playlist.is_blank(ix) || target_clip == ix) {
            blankEnd = m_playlist.clip_start(++ix) - 1;
        } else break;
    }
    // Return true if we are at playlist end or if there is enough space for the move
    return (ix == m_playlist.count() || blankEnd >= position + length);
}

