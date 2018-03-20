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
#include "clipmodel.hpp"
#include "compositionmodel.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "kdenlivesettings.h"
#include "snapmodel.hpp"
#include "timelinemodel.hpp"
#include <QDebug>
#include <QModelIndex>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTransition.h>

TrackModel::TrackModel(const std::weak_ptr<TimelineModel> &parent, int id, const QString &trackName, bool audioTrack)
    : m_parent(parent)
    , m_id(id == -1 ? TimelineModel::getNextId() : id)
    , m_lock(QReadWriteLock::Recursive)
{
    if (auto ptr = parent.lock()) {
        m_track = std::shared_ptr<Mlt::Tractor>(new Mlt::Tractor(*ptr->getProfile()));
        m_playlists[0].set_profile(*ptr->getProfile());
        m_playlists[1].set_profile(*ptr->getProfile());
        m_track->insert_track(m_playlists[0], 0);
        m_track->insert_track(m_playlists[1], 1);
        if (!trackName.isEmpty()) {
            m_track->set("kdenlive:track_name", trackName.toUtf8().constData());
        }
        if (audioTrack) {
            m_track->set("kdenlive:audio_track", 1);
            for (int i = 0; i < 2; i++) {
                m_playlists[i].set("hide", 1);
            }
        }
        m_track->set("kdenlive:trackheight", KdenliveSettings::trackheight());
        m_effectStack = EffectStackModel::construct(m_track, {ObjectType::TimelineTrack, m_id}, ptr->m_undoStack);
    } else {
        qDebug() << "Error : construction of track failed because parent timeline is not available anymore";
        Q_ASSERT(false);
    }
}

TrackModel::TrackModel(const std::weak_ptr<TimelineModel> &parent, Mlt::Tractor mltTrack, int id)
    : m_parent(parent)
    , m_id(id == -1 ? TimelineModel::getNextId() : id)
{
    if (auto ptr = parent.lock()) {
        m_track = std::shared_ptr<Mlt::Tractor>(new Mlt::Tractor(mltTrack));
        m_playlists[0] = *m_track->track(0);
        m_playlists[1] = *m_track->track(1);
        m_effectStack = EffectStackModel::construct(m_track, {ObjectType::TimelineTrack, m_id}, ptr->m_undoStack);
    } else {
        qDebug() << "Error : construction of track failed because parent timeline is not available anymore";
        Q_ASSERT(false);
    }
}

TrackModel::~TrackModel()
{
    m_track->remove_track(1);
    m_track->remove_track(0);
}

int TrackModel::construct(const std::weak_ptr<TimelineModel> &parent, int id, int pos, const QString &trackName, bool audioTrack)
{
    std::shared_ptr<TrackModel> track(new TrackModel(parent, id, trackName, audioTrack));
    id = track->m_id;
    if (auto ptr = parent.lock()) {
        ptr->registerTrack(std::move(track), pos);
    } else {
        qDebug() << "Error : construction of track failed because parent timeline is not available anymore";
        Q_ASSERT(false);
    }
    return id;
}

int TrackModel::getClipsCount()
{
    READ_LOCK();
#ifdef QT_DEBUG
    int count = 0;
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < m_playlists[j].count(); i++) {
            if (!m_playlists[j].is_blank(i)) {
                count++;
            }
        }
    }
    Q_ASSERT(count == static_cast<int>(m_allClips.size()));
#else
    int count = m_allClips.size();
#endif
    return count;
}

Fun TrackModel::requestClipInsertion_lambda(int clipId, int position, bool updateView, bool finalMove)
{
    QWriteLocker locker(&m_lock);
    // By default, insertion occurs in topmost track
    // Find out the clip id at position
    int target_clip = m_playlists[0].get_clip_index_at(position);
    int count = m_playlists[0].count();

    // we create the function that has to be executed after the melt order. This is essentially book-keeping
    auto end_function = [clipId, this, position, updateView, finalMove]() {
        if (auto ptr = m_parent.lock()) {
            std::shared_ptr<ClipModel> clip = ptr->getClipPtr(clipId);
            m_allClips[clip->getId()] = clip; // store clip
            // update clip position and track
            clip->setPosition(position);
            clip->setCurrentTrackId(getId());
            int new_in = clip->getPosition();
            int new_out = new_in + clip->getPlaytime();
            ptr->m_snaps->addPoint(new_in);
            ptr->m_snaps->addPoint(new_out);
            if (updateView) {
                int clip_index = getRowfromClip(clipId);
                ptr->_beginInsertRows(ptr->makeTrackIndexFromID(getId()), clip_index, clip_index);
                ptr->_endInsertRows();
                int state = m_track->get_int("hide");
                bool audioOnly = clip->isAudioOnly();
                if (!audioOnly && (state == 0 || state == 2) && !isAudioTrack()) {
                    // only refresh monitor if not an audio track and not hidden
                    ptr->checkRefresh(new_in, new_out);
                }
                if (!audioOnly && finalMove && !isAudioTrack()) {
                    ptr->invalidateZone(new_in, new_out);
                }
            }
            return true;
        }
        qDebug() << "Error : Clip Insertion failed because timeline is not available anymore";
        return false;

    };
    if (target_clip >= count && isBlankAt(position)) {
        // In that case, we append after, in the first playlist
        return [this, position, clipId, end_function, finalMove]() {
            if (auto ptr = m_parent.lock()) {
                // Lock MLT playlist so that we don't end up with an invalid frame being displayed
                m_playlists[0].lock();
                std::shared_ptr<ClipModel> clip = ptr->getClipPtr(clipId);
                int index = m_playlists[0].insert_at(position, *clip, 1);
                m_playlists[0].consolidate_blanks();
                m_playlists[0].unlock();
                if (finalMove) {
                    ptr->updateDuration();
                }
                return index != -1 && end_function();
            }
            qDebug() << "Error : Clip Insertion failed because timeline is not available anymore";
            return false;

        };
    }
    if (isBlankAt(position)) {
        int blank_end = getBlankEnd(position);
        int length = -1;
        if (auto ptr = m_parent.lock()) {
            std::shared_ptr<ClipModel> clip = ptr->getClipPtr(clipId);
            length = clip->getPlaytime();
        }
        if (blank_end >= position + length) {
            return [this, position, clipId, end_function]() {
                if (auto ptr = m_parent.lock()) {
                    // Lock MLT playlist so that we don't end up with an invalid frame being displayed
                    m_playlists[0].lock();
                    std::shared_ptr<ClipModel> clip = ptr->getClipPtr(clipId);
                    int index = m_playlists[0].insert_at(position, *clip, 1);
                    m_playlists[0].consolidate_blanks();
                    m_playlists[0].unlock();
                    return index != -1 && end_function();
                }
                qDebug() << "Error : Clip Insertion failed because timeline is not available anymore";
                return false;

            };
        }
    }

    return []() { return false; };
}

bool TrackModel::requestClipInsertion(int clipId, int position, bool updateView, bool finalMove,  Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    auto operation = requestClipInsertion_lambda(clipId, position, updateView, finalMove);
    if (operation()) {
        auto reverse = requestClipDeletion_lambda(clipId, updateView, finalMove);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

void TrackModel::replugClip(int clipId)
{
    int clip_position = m_allClips[clipId]->getPosition();
    auto clip_loc = getClipIndexAt(clip_position);
    int target_track = clip_loc.first;
    int target_clip = clip_loc.second;
    // lock MLT playlist so that we don't end up with invalid frames in monitor
    m_playlists[target_track].lock();
    Q_ASSERT(target_clip < m_playlists[target_track].count());
    Q_ASSERT(!m_playlists[target_track].is_blank(target_clip));
    auto prod = m_playlists[target_track].replace_with_blank(target_clip);
    if (auto ptr = m_parent.lock()) {
        std::shared_ptr<ClipModel> clip = ptr->getClipPtr(clipId);
        m_playlists[target_track].insert_at(clip_position, *clip, 1);
    }
    m_playlists[target_track].consolidate_blanks();
    delete prod;
    m_playlists[target_track].unlock();
}

Fun TrackModel::requestClipDeletion_lambda(int clipId, bool updateView, bool finalMove)
{
    QWriteLocker locker(&m_lock);
    // Find index of clip
    int clip_position = m_allClips[clipId]->getPosition();
    bool audioOnly = m_allClips[clipId]->isAudioOnly();
    int old_in = clip_position;
    int old_out = old_in + m_allClips[clipId]->getPlaytime();
    return [clip_position, clipId, old_in, old_out, updateView, audioOnly, finalMove, this]() {
        auto clip_loc = getClipIndexAt(clip_position);
        if (updateView) {
            int old_clip_index = getRowfromClip(clipId);
            auto ptr = m_parent.lock();
            ptr->_beginRemoveRows(ptr->makeTrackIndexFromID(getId()), old_clip_index, old_clip_index);
            ptr->_endRemoveRows();
        }
        int target_track = clip_loc.first;
        int target_clip = clip_loc.second;
        // lock MLT playlist so that we don't end up with invalid frames in monitor
        m_playlists[target_track].lock();
        Q_ASSERT(target_clip < m_playlists[target_track].count());
        Q_ASSERT(!m_playlists[target_track].is_blank(target_clip));
        auto prod = m_playlists[target_track].replace_with_blank(target_clip);
        if (prod != nullptr) {
            if (finalMove && !audioOnly && !isAudioTrack()) {
                if (auto ptr = m_parent.lock()) {
                    //qDebug() << "/// INVALIDATE CLIP ON DELETE!!!!!!";
                    ptr->invalidateZone(old_in, old_out);
                }
            }
            m_playlists[target_track].consolidate_blanks();
            m_allClips[clipId]->setCurrentTrackId(-1);
            m_allClips.erase(clipId);
            delete prod;
            m_playlists[target_track].unlock();
            if (auto ptr = m_parent.lock()) {
                ptr->m_snaps->removePoint(old_in);
                ptr->m_snaps->removePoint(old_out);
                int state = m_track->get_int("hide");
                if (finalMove && target_clip >= m_playlists[target_track].count()) {
                    // deleted last clip in playlist
                    ptr->updateDuration();
                }
                if (!audioOnly && (state == 0 || state == 2) && !isAudioTrack()) {
                    // only refresh monitor if not an audio track and not hidden
                    ptr->checkRefresh(old_in, old_out);
                }
            }
            return true;
        }
        m_playlists[target_track].unlock();
        return false;
    };
}

bool TrackModel::requestClipDeletion(int clipId, bool updateView, bool finalMove, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allClips.count(clipId) > 0);
    auto old_clip = m_allClips[clipId];
    int old_position = old_clip->getPosition();
    qDebug()<<"/// REQUESTOING CLIP DELETION_: "<<updateView;
    auto operation = requestClipDeletion_lambda(clipId, updateView, finalMove);
    if (operation()) {
        auto reverse = requestClipInsertion_lambda(clipId, old_position, updateView, finalMove);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

int TrackModel::getBlankSizeAtPos(int frame)
{
    READ_LOCK();
    int min_length = 0;
    for (int i = 0; i < 2; ++i) {
        int ix = m_playlists[i].get_clip_index_at(frame);
        if (m_playlists[i].is_blank(ix)) {
            int blank_length = m_playlists[i].clip_length(ix);
            if (min_length == 0 || (blank_length > 0 && blank_length < min_length)) {
                min_length = blank_length;
            }
        }
    }
    return min_length;
}

int TrackModel::getBlankSizeNearClip(int clipId, bool after)
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    int clip_position = m_allClips[clipId]->getPosition();
    auto clip_loc = getClipIndexAt(clip_position);
    int track = clip_loc.first;
    int index = clip_loc.second;
    int other_index; // index in the other track
    int other_track = (track + 1) % 2;
    if (after) {
        int first_pos = m_playlists[track].clip_start(index) + m_playlists[track].clip_length(index);
        other_index = m_playlists[other_track].get_clip_index_at(first_pos);
        index++;
    } else {
        int last_pos = m_playlists[track].clip_start(index) - 1;
        other_index = m_playlists[other_track].get_clip_index_at(last_pos);
        index--;
    }
    if (index < 0) return 0;
    int length = INT_MAX;
    if (index < m_playlists[track].count()) {
        if (!m_playlists[track].is_blank(index)) {
            return 0;
        }
        length = std::min(length, m_playlists[track].clip_length(index));
    }
    if (other_index < m_playlists[other_track].count()) {
        if (!m_playlists[other_track].is_blank(other_index)) {
            return 0;
        }
        length = std::min(length, m_playlists[other_track].clip_length(other_index));
    }
    return length;
}

int TrackModel::getBlankSizeNearComposition(int compoId, bool after)
{
    READ_LOCK();
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    int clip_position = m_allCompositions[compoId]->getPosition();
    Q_ASSERT(m_compoPos.count(clip_position) > 0);
    Q_ASSERT(m_compoPos[clip_position] == compoId);
    auto it = m_compoPos.find(clip_position);
    int clip_length = m_allCompositions[compoId]->getPlaytime();
    int length = INT_MAX;
    if (after) {
        ++it;
        if (it != m_compoPos.end()) {
            return it->first - clip_position - clip_length;
        }
    } else {
        if (it != m_compoPos.begin()) {
            --it;
            return clip_position - it->first - m_allCompositions[it->second]->getPlaytime();
        }
        return clip_position;
    }
    return length;
}

Fun TrackModel::requestClipResize_lambda(int clipId, int in, int out, bool right)
{
    QWriteLocker locker(&m_lock);
    int clip_position = m_allClips[clipId]->getPosition();
    int old_in = clip_position;
    int old_out = old_in + m_allClips[clipId]->getPlaytime();
    auto clip_loc = getClipIndexAt(clip_position);
    int target_track = clip_loc.first;
    int target_clip = clip_loc.second;
    Q_ASSERT(target_clip < m_playlists[target_track].count());
    int size = out - in + 1;
    int state = m_track->get_int("hide");
    bool checkRefresh = false;
    if ((state == 0 || state == 2) && !isAudioTrack()) {
        checkRefresh = true;
    }

    auto update_snaps = [clipId, old_in, old_out, checkRefresh, this](int new_in, int new_out) {
        if (auto ptr = m_parent.lock()) {
            ptr->m_snaps->removePoint(old_in);
            ptr->m_snaps->removePoint(old_out);
            ptr->m_snaps->addPoint(new_in);
            ptr->m_snaps->addPoint(new_out);
            if (checkRefresh) {
                ptr->checkRefresh(old_in, old_out);
                ptr->checkRefresh(new_in, new_out);
                ptr->adjustAssetRange(clipId, new_in, new_out);
            }
        } else {
            qDebug() << "Error : clip resize failed because parent timeline is not available anymore";
            Q_ASSERT(false);
        }
    };

    int delta = m_allClips[clipId]->getPlaytime() - size;
    if (delta == 0) {
        return []() { return true; };
    }
    //qDebug() << "RESIZING CLIP: " << clipId << " FROM: " << delta;
    if (delta > 0) { // we shrink clip
        return [right, target_clip, target_track, clip_position, delta, in, out, clipId, update_snaps, this]() {
            int target_clip_mutable = target_clip;
            int blank_index = right ? (target_clip_mutable + 1) : target_clip_mutable;
            // insert blank to space that is going to be empty
            // The second is parameter is delta - 1 because this function expects an out time, which is basically size - 1
            m_playlists[target_track].insert_blank(blank_index, delta - 1);
            if (!right) {
                m_allClips[clipId]->setPosition(clip_position + delta);
                // Because we inserted blank before, the index of our clip has increased
                target_clip_mutable++;
            }
            int err = m_playlists[target_track].resize_clip(target_clip_mutable, in, out);
            // make sure to do this after, to avoid messing the indexes
            m_playlists[target_track].consolidate_blanks();
            if (err == 0) {
                update_snaps(m_allClips[clipId]->getPosition(), m_allClips[clipId]->getPosition() + out - in + 1);
                if (right && m_playlists[target_track].count() - 1 == target_clip_mutable) {
                    // deleted last clip in playlist
                    if (auto ptr = m_parent.lock()) {
                        ptr->updateDuration();
                    }
                }
            }
            return err == 0;
        };
    }
    int blank = -1;
    int other_blank_end = getBlankEnd(clip_position, (target_track + 1) % 2);
    if (right) {
        if (target_clip == m_playlists[target_track].count() - 1 && other_blank_end >= out) {
            // clip is last, it can always be extended
            return [this, target_clip, target_track, in, out, update_snaps, clipId]() {
                // color, image and title clips can have unlimited resize
                QScopedPointer<Mlt::Producer> clip(m_playlists[target_track].get_clip(target_clip));
                if (out >= clip->get_length()) {
                    clip->parent().set("length", out + 1);
                    clip->parent().set("out", out);
                    clip->set("length", out + 1);
                }
                int err = m_playlists[target_track].resize_clip(target_clip, in, out);
                if (err == 0) {
                    update_snaps(m_allClips[clipId]->getPosition(), m_allClips[clipId]->getPosition() + out - in + 1);
                }
                m_playlists[target_track].consolidate_blanks();
                if (m_playlists[target_track].count() - 1 == target_clip) {
                    // deleted last clip in playlist
                    if (auto ptr = m_parent.lock()) {
                        ptr->updateDuration();
                    }
                }
                return err == 0;
            };
        }

        blank = target_clip + 1;
    } else {
        if (target_clip == 0) {
            // clip is first, it can never be extended on the left
            return []() { return false; };
        }
        blank = target_clip - 1;
    }
    if (m_playlists[target_track].is_blank(blank)) {
        int blank_length = m_playlists[target_track].clip_length(blank);
        if (blank_length + delta >= 0 && other_blank_end >= out) {
            return [blank_length, blank, right, clipId, delta, update_snaps, this, in, out, target_clip, target_track]() {
                int target_clip_mutable = target_clip;
                int err = 0;
                if (blank_length + delta == 0) {
                    err = m_playlists[target_track].remove(blank);
                    if (!right) {
                        target_clip_mutable--;
                    }
                } else {
                    err = m_playlists[target_track].resize_clip(blank, 0, blank_length + delta - 1);
                }
                if (err == 0) {
                    QScopedPointer<Mlt::Producer> clip(m_playlists[target_track].get_clip(target_clip_mutable));
                    if (out >= clip->get_length()) {
                        clip->parent().set("length", out + 1);
                        clip->parent().set("out", out);
                        clip->set("length", out + 1);
                    }
                    err = m_playlists[target_track].resize_clip(target_clip_mutable, in, out);
                }
                if (!right && err == 0) {
                    m_allClips[clipId]->setPosition(m_playlists[target_track].clip_start(target_clip_mutable));
                }
                if (err == 0) {
                    update_snaps(m_allClips[clipId]->getPosition(), m_allClips[clipId]->getPosition() + out - in + 1);
                }
                m_playlists[target_track].consolidate_blanks();
                return err == 0;
            };
        }
    }

    return []() { return false; };
}

int TrackModel::getId() const
{
    return m_id;
}

int TrackModel::getClipByPosition(int position)
{
    READ_LOCK();
    QSharedPointer<Mlt::Producer> prod(nullptr);
    if (m_playlists[0].count() > 0) {
        prod = QSharedPointer<Mlt::Producer>(m_playlists[0].get_clip_at(position));
    }
    if ((!prod || prod->is_blank()) && m_playlists[1].count() > 0) {
        prod = QSharedPointer<Mlt::Producer>(m_playlists[1].get_clip_at(position));
    }
    if (!prod || prod->is_blank()) {
        return -1;
    }
    return prod->get_int("_kdenlive_cid");
}

int TrackModel::getCompositionByPosition(int position)
{
    READ_LOCK();
    for (const auto &comp : m_compoPos) {
        if (comp.first == position) {
            return comp.second;
        } else if (comp.first < position) {
            if (comp.first + m_allCompositions[comp.second]->getPlaytime() >= position) {
                return comp.second;
            }
        }
    }
    return -1;
}

int TrackModel::getClipByRow(int row) const
{
    READ_LOCK();
    if (row >= static_cast<int>(m_allClips.size())) {
        return -1;
    }
    auto it = m_allClips.cbegin();
    std::advance(it, row);
    return (*it).first;
}

std::unordered_set<int> TrackModel::getClipsAfterPosition(int position, int end)
{
    READ_LOCK();
    // TODO: this function doesn't take into accounts the fact that there are two tracks
    std::unordered_set<int> ids;
    int ix = m_playlists[0].get_clip_index_at(position);
    if (end > -1) {
        end = m_playlists[0].get_clip_index_at(end);
        if (end < m_playlists[0].count()) {
            end++;
        }
    } else {
        end = m_playlists[0].count();
    }
    while (ix < end) {
        QSharedPointer<Mlt::Producer> prod(m_playlists[0].get_clip(ix));
        ix++;
        if (prod->is_blank()) {
            continue;
        }
        ids.insert(prod->get_int("_kdenlive_cid"));
    }
    return ids;
}

int TrackModel::getRowfromClip(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return (int)std::distance(m_allClips.begin(), m_allClips.find(clipId));
}

std::unordered_set<int> TrackModel::getCompositionsAfterPosition(int position, int end)
{
    READ_LOCK();
    // TODO: this function doesn't take into accounts the fact that there are two tracks
    std::unordered_set<int> ids;
    for (const auto &compo : m_allCompositions) {
        int pos = compo.second->getPosition();
        if (pos > position && pos < end) {
            if (compo.second->getPlaytime() < end - position) {
                ids.insert(compo.first);
            }
        }
    }
    return ids;
}

int TrackModel::getRowfromComposition(int tid) const
{
    READ_LOCK();
    Q_ASSERT(m_allCompositions.count(tid) > 0);
    return (int)m_allClips.size() + (int)std::distance(m_allCompositions.begin(), m_allCompositions.find(tid));
}

QVariant TrackModel::getProperty(const QString &name) const
{
    READ_LOCK();
    return QVariant(m_track->get(name.toUtf8().constData()));
}

void TrackModel::setProperty(const QString &name, const QString &value)
{
    QWriteLocker locker(&m_lock);
    m_track->set(name.toUtf8().constData(), value.toUtf8().constData());
    // Hide property mus be defined at playlist level or it won't be saved
    if (name == QLatin1String("kdenlive:audio_track") || name == QLatin1String("hide")) {
        for (int i = 0; i < 2; i++) {
            m_playlists[i].set(name.toUtf8().constData(), value.toInt());
        }
    }
}

bool TrackModel::checkConsistency()
{
    auto ptr = m_parent.lock();
    if (!ptr) {
        return false;
    }
    std::vector<std::pair<int, int>> clips; // clips stored by (position, id)
    for (const auto &c : m_allClips) {
        Q_ASSERT(c.second);
        Q_ASSERT(c.second.get() == ptr->getClipPtr(c.first).get());
        clips.push_back({c.second->getPosition(), c.first});
    }
    std::sort(clips.begin(), clips.end());
    size_t current_clip = 0;
    int playtime = std::max(m_playlists[0].get_playtime(), m_playlists[1].get_playtime());
    for (int i = 0; i < playtime; i++) {
        int track, index;
        if (isBlankAt(i)) {
            track = 0;
            index = m_playlists[0].get_clip_index_at(i);
        } else {
            auto clip_loc = getClipIndexAt(i);
            track = clip_loc.first;
            index = clip_loc.second;
        }
        Q_ASSERT(m_playlists[(track + 1) % 2].is_blank_at(i));
        if (current_clip < clips.size() && i >= clips[current_clip].first) {
            auto clip = m_allClips[clips[current_clip].second];
            if (i >= clips[current_clip].first + clip->getPlaytime()) {
                current_clip++;
                i--;
                continue;
            }
            if (isBlankAt(i)) {
                qDebug() << "ERROR: Found blank when clip was required at position " << i;
                return false;
            }
            auto pr = m_playlists[track].get_clip(index);
            Mlt::Producer prod(pr);
            if (!prod.same_clip(*clip)) {
                qDebug() << "ERROR: Wrong clip at position " << i;
                delete pr;
                return false;
            }
            delete pr;

        } else {
            if (!isBlankAt(i)) {
                qDebug() << "ERROR: Found clip when blank was required at position " << i;
                return false;
            }
        }
    }
    // We now check compositions positions
    if (m_allCompositions.size() != m_compoPos.size()) {
        qDebug() << "Error: the number of compositions position doesn't match number of compositions";
        return false;
    }
    for (const auto &compo : m_allCompositions) {
        int pos = compo.second->getPosition();
        if (m_compoPos.count(pos) == 0) {
            qDebug() << "Error: the position of composition " << compo.first << " is not properly stored";
            return false;
        }
        if (m_compoPos[pos] != compo.first) {
            qDebug() << "Error: found composition" << m_compoPos[pos] << "instead of " << compo.first << "at position" << pos;
            return false;
        }
    }
    for (auto it = m_compoPos.begin(); it != m_compoPos.end(); ++it) {
        int compoId = it->second;
        int cur_in = m_allCompositions[compoId]->getPosition();
        Q_ASSERT(cur_in == it->first);
        int cur_out = cur_in + m_allCompositions[compoId]->getPlaytime() - 1;
        ++it;
        if (it != m_compoPos.end()) {
            int next_compoId = it->second;
            int next_in = m_allCompositions[next_compoId]->getPosition();
            int next_out = next_in + m_allCompositions[next_compoId]->getPlaytime() - 1;
            if (next_in <= cur_out) {
                qDebug() << "Error: found collision between composition " << compoId << "[ " << cur_in << ", " << cur_out << "] and " << next_compoId << "[ "
                         << next_in << ", " << next_out << "]";
                return false;
            }
        }
        --it;
    }
    return true;
}

std::pair<int, int> TrackModel::getClipIndexAt(int position)
{
    READ_LOCK();
    for (int j = 0; j < 2; j++) {
        if (!m_playlists[j].is_blank_at(position)) {
            return {j, m_playlists[j].get_clip_index_at(position)};
        }
    }
    Q_ASSERT(false);
    return {-1, -1};
}

bool TrackModel::isBlankAt(int position)
{
    READ_LOCK();
    return m_playlists[0].is_blank_at(position) && m_playlists[1].is_blank_at(position);
}

int TrackModel::getBlankStart(int position)
{
    READ_LOCK();
    int result = 0;
    for (int j = 0; j < 2; j++) {
        if (m_playlists[j].count() == 0) {
            break;
        }
        if (!m_playlists[j].is_blank_at(position)) {
            result = position;
            break;
        }
        int clip_index = m_playlists[j].get_clip_index_at(position);
        int start = m_playlists[j].clip_start(clip_index);
        if (start > result) {
            result = start;
        }
    }
    return result;
}

int TrackModel::getBlankEnd(int position, int track)
{
    READ_LOCK();
    // Q_ASSERT(m_playlists[track].is_blank_at(position));
    if (!m_playlists[track].is_blank_at(position)) {
        return position;
    }
    int clip_index = m_playlists[track].get_clip_index_at(position);
    int count = m_playlists[track].count();
    if (clip_index < count) {
        int blank_start = m_playlists[track].clip_start(clip_index);
        int blank_length = m_playlists[track].clip_length(clip_index);
        return blank_start + blank_length;
    }
    return INT_MAX;
}

int TrackModel::getBlankEnd(int position)
{
    READ_LOCK();
    int end = INT_MAX;
    for (int j = 0; j < 2; j++) {
        end = std::min(getBlankEnd(position, j), end);
    }
    return end;
}

Fun TrackModel::requestCompositionResize_lambda(int compoId, int in, int out)
{
    QWriteLocker locker(&m_lock);
    int compo_position = m_allCompositions[compoId]->getPosition();
    Q_ASSERT(m_compoPos.count(compo_position) > 0);
    Q_ASSERT(m_compoPos[compo_position] == compoId);
    int old_in = compo_position;
    int old_out = old_in + m_allCompositions[compoId]->getPlaytime();
    qDebug() << "compo resize " << compoId << in << "-" << out << " / " << old_in << "-" << old_out;
    if (out == -1) {
        out = in + old_out - old_in;
    }

    auto update_snaps = [compoId, old_in, old_out, this](int new_in, int new_out) {
        if (auto ptr = m_parent.lock()) {
            ptr->m_snaps->removePoint(old_in);
            ptr->m_snaps->removePoint(old_out);
            ptr->m_snaps->addPoint(new_in);
            ptr->m_snaps->addPoint(new_out);
            ptr->checkRefresh(old_in, old_out);
            ptr->checkRefresh(new_in, new_out);
            ptr->adjustAssetRange(compoId, new_in, new_out);
        } else {
            qDebug() << "Error : Composition resize failed because parent timeline is not available anymore";
            Q_ASSERT(false);
        }
    };

    if (in == compo_position && (out == -1 || out == old_out)) {
        return []() { return true; };
    }

    // temporary remove of current compo to check collisions
    m_compoPos.erase(compo_position);
    bool intersecting = hasIntersectingComposition(in, out);
    // put it back
    m_compoPos[compo_position] = compoId;

    if (intersecting) {
        return []() { return false; };
    }

    return [in, out, compoId, update_snaps, this]() {
        m_compoPos.erase(m_allCompositions[compoId]->getPosition());
        m_allCompositions[compoId]->setInOut(in, out);
        update_snaps(in, out + 1);
        m_compoPos[m_allCompositions[compoId]->getPosition()] = compoId;
        return true;
    };
}

bool TrackModel::requestCompositionInsertion(int compoId, int position, bool updateView, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    auto operation = requestCompositionInsertion_lambda(compoId, position, updateView);
    if (operation()) {
        auto reverse = requestCompositionDeletion_lambda(compoId, updateView);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

bool TrackModel::requestCompositionDeletion(int compoId, bool updateView, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    auto old_composition = m_allCompositions[compoId];
    int old_position = old_composition->getPosition();
    Q_ASSERT(m_compoPos.count(old_position) > 0);
    Q_ASSERT(m_compoPos[old_position] == compoId);
    auto operation = requestCompositionDeletion_lambda(compoId, updateView);
    if (operation()) {
        auto reverse = requestCompositionInsertion_lambda(compoId, old_position, updateView);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

Fun TrackModel::requestCompositionDeletion_lambda(int compoId, bool updateView)
{
    QWriteLocker locker(&m_lock);
    // Find index of clip
    int clip_position = m_allCompositions[compoId]->getPosition();
    int old_in = clip_position;
    int old_out = old_in + m_allCompositions[compoId]->getPlaytime();
    return [clip_position, compoId, old_in, old_out, updateView, this]() {
        int old_clip_index = getRowfromComposition(compoId);
        auto ptr = m_parent.lock();
        if (updateView) {
            ptr->_beginRemoveRows(ptr->makeTrackIndexFromID(getId()), old_clip_index, old_clip_index);
            ptr->_endRemoveRows();
        }
        m_allCompositions[compoId]->setCurrentTrackId(-1);
        m_allCompositions.erase(compoId);
        m_compoPos.erase(old_in);
        ptr->m_snaps->removePoint(old_in);
        ptr->m_snaps->removePoint(old_out);
        return true;
    };
}

int TrackModel::getCompositionByRow(int row) const
{
    READ_LOCK();
    if (row < (int)m_allClips.size()) {
        return -1;
    }
    // Q_ASSERT(row <= (int)m_allClips.size() + m_allCompositions.size());
    auto it = m_allCompositions.cbegin();
    std::advance(it, row - (int)m_allClips.size());
    return (*it).first;
}

int TrackModel::getCompositionsCount() const
{
    READ_LOCK();
    return (int)m_allCompositions.size();
}

Fun TrackModel::requestCompositionInsertion_lambda(int compoId, int position, bool updateView)
{
    QWriteLocker locker(&m_lock);
    bool intersecting = true;
    if (auto ptr = m_parent.lock()) {
        intersecting = hasIntersectingComposition(position, position + ptr->getCompositionPlaytime(compoId) - 1);
    } else {
        qDebug() << "Error : Composition Insertion failed because timeline is not available anymore";
    }
    if (!intersecting) {
        return [compoId, this, position, updateView]() {
            if (auto ptr = m_parent.lock()) {
                std::shared_ptr<CompositionModel> composition = ptr->getCompositionPtr(compoId);
                m_allCompositions[composition->getId()] = composition; // store clip
                // update clip position and track
                composition->setCurrentTrackId(getId());
                int new_in = position;
                int new_out = new_in + composition->getPlaytime();
                composition->setInOut(new_in, new_out - 1);
                if (updateView) {
                    int composition_index = getRowfromComposition(composition->getId());
                    ptr->_beginInsertRows(ptr->makeTrackIndexFromID(composition->getCurrentTrackId()), composition_index, composition_index);
                    ptr->_endInsertRows();
                }
                ptr->m_snaps->addPoint(new_in);
                ptr->m_snaps->addPoint(new_out);
                m_compoPos[new_in] = composition->getId();
                return true;
            }
            qDebug() << "Error : Composition Insertion failed because timeline is not available anymore";
            return false;

        };
    }
    return []() { return false; };
}

bool TrackModel::hasIntersectingComposition(int in, int out) const
{
    READ_LOCK();
    auto it = m_compoPos.lower_bound(in);
    if (m_compoPos.empty()) {
        return false;
    }
    if (it != m_compoPos.end() && it->first <= out) {
        // compo at it intersects
        return true;
    }
    if (it == m_compoPos.begin()) {
        return false;
    }
    --it;
    int end = it->first + m_allCompositions.at(it->second)->getPlaytime() - 1;
    return end >= in;

    return false;
}

bool TrackModel::addEffect(const QString &effectId)
{
    READ_LOCK();
    m_effectStack->appendEffect(effectId);
    return true;
}

int TrackModel::trackDuration()
{
    return m_track->get_length();
}

bool TrackModel::isLocked() const
{
    READ_LOCK();
    return m_track->get_int("kdenlive:locked_track");
}

bool TrackModel::isAudioTrack() const
{
    return m_track->get_int("kdenlive:audio_track");
}
