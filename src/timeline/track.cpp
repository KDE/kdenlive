/*
 * Kdenlive timeline track handling MLT playlist
 * Copyright 2015 Kdenlive team <kdenlive@kde.org>
 * Author: Vincent Pinon <vpinon@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include "track.h"
#include "clip.h"
#include <QtGlobal>
#include <QDebug>
#include <math.h>

Track::Track(Mlt::Playlist &playlist, qreal fps) :
    m_playlist(playlist),
    m_fps(fps)
{
}

Track::~Track()
{
}

// members access

Mlt::Playlist & Track::playlist()
{
    return m_playlist;
}

void Track::setPlaylist(Mlt::Playlist &playlist)
{
    m_playlist = playlist;
}

qreal Track::fps()
{
    return m_fps;
}

int Track::frame(qreal t)
{
    return round(t * m_fps);
}

qreal Track::length() {
    return m_playlist.get_playtime() / m_fps;
}

void Track::setFps(qreal fps)
{
    m_fps = fps;
}

// basic clip operations

// TODO: remove this method ?
bool Track::add(qreal t, Mlt::Producer *parent, bool duplicate, int mode)
{
    Mlt::Producer *cut = duplicate ? clipProducer(parent, PlaylistState::Original) :  new Mlt::Producer(parent);
    m_playlist.lock();
    bool result = doAdd(t, cut, mode);
    m_playlist.unlock();
    delete cut;
    return result;
}

bool Track::add(qreal t, Mlt::Producer *parent, qreal tcut, qreal dtcut, PlaylistState::ClipState state, bool duplicate, int mode)
{
    Mlt::Producer *cut;
    if (duplicate && state != PlaylistState::VideoOnly) {
        Mlt::Producer *newProd = clipProducer(parent, state);
        cut = newProd->cut(frame(tcut), frame(dtcut) - 1);
        delete newProd;
    }
    else {
        cut = parent->cut(frame(tcut), frame(dtcut) - 1);
    }
    Clip(*cut).addEffects(*parent);
    m_playlist.lock();
    bool result = doAdd(t, cut, mode);
    m_playlist.unlock();
    delete cut;
    return result;
}

bool Track::doAdd(qreal t, Mlt::Producer *cut, int mode)
{
    int pos = frame(t);
    int len = cut->get_out() - cut->get_in() + 1;
    if (pos < m_playlist.get_playtime() && mode > 0) {
        if (mode == 1) {
            m_playlist.remove_region(pos, len);
        } else if (mode == 2) {
            m_playlist.split_at(pos);
        }
        m_playlist.insert_blank(m_playlist.get_clip_index_at(pos), len);
    }
    m_playlist.consolidate_blanks();
    if (m_playlist.insert_at(frame(t), cut, 1) == m_playlist.count() - 1) {
        emit newTrackDuration(m_playlist.get_playtime());
    }
    return true;
}

bool Track::move(qreal start, qreal end, int mode)
{
    int pos = frame(start);
    m_playlist.lock();
    int clipIndex = m_playlist.get_clip_index_at(pos);
    Mlt::Producer *clipProducer = m_playlist.replace_with_blank(clipIndex);
    if (!clipProducer || clipProducer->is_blank()) {
        qDebug() << "// Cannot get clip at index: "<<clipIndex<<" / "<< start;
        m_playlist.unlock();
        return false;
    }
    m_playlist.consolidate_blanks();
    bool result = doAdd(end, clipProducer, mode);
    m_playlist.unlock();
    delete clipProducer;
    return result;
}

bool Track::del(qreal t)
{
    m_playlist.lock();
    Mlt::Producer *clip = m_playlist.replace_with_blank(m_playlist.get_clip_index_at(frame(t)));
    if (clip)
        delete clip;
    else {
        qWarning("Error deleting clip at %f", t);
        m_playlist.unlock();
        return false;
    }
    m_playlist.consolidate_blanks();
    m_playlist.unlock();
    return true;
}

bool Track::del(qreal t, qreal dt)
{
    m_playlist.lock();
    m_playlist.insert_blank(m_playlist.remove_region(frame(t), frame(dt) + 1), frame(dt));
    m_playlist.consolidate_blanks();
    m_playlist.unlock();
    return true;
}

bool Track::resize(qreal t, qreal dt, bool end)
{
    m_playlist.lock();
    int index = m_playlist.get_clip_index_at(frame(t));
    int length = frame(dt);

    Mlt::Producer *clip = m_playlist.get_clip(index);
    if (clip == NULL || clip->is_blank()) {
        qWarning("Can't resize clip at %f", t);
        return false;
    }

    int in = clip->get_in();
    int out = clip->get_out();
    if (end) out += length; else in += length;

    //image or color clips are not bounded
    if (in < 0) {out -= in; in = 0;}
    if (clip->get_length() < out + 1) {
        clip->parent().set("length", out + 2);
        clip->parent().set("out", out + 1);
        clip->set("length", out + 2);
    }

    delete clip;

    if (m_playlist.resize_clip(index, in, out)) {
        qWarning("MLT resize failed : clip %d from %d to %d", index, in, out);
        m_playlist.unlock();
        return false;
    }

    //adjust adjacent blank
    if (end) {
        ++index;
        if (index > m_playlist.count() - 1) {
            m_playlist.unlock();
            // this is the last clip in track, check tracks length to adjust black track and project duration
            emit newTrackDuration(m_playlist.get_playtime());
            return true;
        }
        length = -length;
    }
    if (length > 0) { // reducing clip
        m_playlist.insert_blank(index, length - 1);
    } else {
        if (!end) --index;
        if (!m_playlist.is_blank(index)) {
            qWarning("Resizing over non-blank clip %d!", index);
        }
        out = m_playlist.clip_length(index) + length - 1;
        if (out >= 0) {
            if (m_playlist.resize_clip(index, 0, out)) {
                qWarning("Error resizing blank %d", index);
            }
        } else {
            if (m_playlist.remove(index)) {
                qWarning("Error removing blank %d", index);
            }
        }
    }
    m_playlist.consolidate_blanks();
    m_playlist.unlock();
    return true;
}

bool Track::cut(qreal t)
{
    int pos = frame(t);
    m_playlist.lock();
    int index = m_playlist.get_clip_index_at(pos);
    if (m_playlist.is_blank(index)) {
        return false;
    }
    if (m_playlist.split(index, pos - m_playlist.clip_start(index) - 1)) {
        qWarning("MLT split failed");
        m_playlist.unlock();
        return false;
    }
    m_playlist.unlock();
    Clip(*m_playlist.get_clip(index + 1)).addEffects(*m_playlist.get_clip(index));
    return true;
}

bool Track::needsDuplicate(const QString &service) const
{
    return (service.contains("avformat") || service.contains("consumer") || service.contains("xml"));
}

bool Track::replaceAll(const QString &id, Mlt::Producer *original, Mlt::Producer *videoOnlyProducer)
{
    int startindex = 0;
    bool found = false;
    QString idForAudioTrack;
    QString idForVideoTrack;
    QString service = original->parent().get("mlt_service");
    QString idForTrack = original->parent().get("id");
    if (needsDuplicate(service)) {
        // We have to use the track clip duplication functions, because of audio glitches in MLT's multitrack
        idForAudioTrack = idForTrack + QLatin1Char('_') + m_playlist.get("id") + "_audio";
        idForVideoTrack = idForTrack + QLatin1Char('_') + m_playlist.get("id") + "_video";
        idForTrack.append(QLatin1Char('_') + m_playlist.get("id"));
    }
    Mlt::Producer *trackProducer = NULL;
    Mlt::Producer *audioTrackProducer = NULL;

    for (int i = 0; i < m_playlist.count(); i++) {
        if (m_playlist.is_blank(i)) continue;
        Mlt::Producer *p = m_playlist.get_clip(i);
        QString current = p->parent().get("id");
        Mlt::Producer *cut = NULL;
        if (idForAudioTrack.isEmpty()) {
            if (current == idForTrack) {
                // No duplication required
                cut = original->cut(p->get_in(), p->get_out());
            }
            else continue;
        }
        else if (current == idForTrack) {
            // Use duplicate
            if (trackProducer == NULL) {
                trackProducer = Clip(*original).clone();
                trackProducer->set("id", idForTrack.toUtf8().constData());
            }
            cut = trackProducer->cut(p->get_in(), p->get_out());
        }
        else if (current == idForAudioTrack) {
            if (audioTrackProducer == NULL) {
                audioTrackProducer = clipProducer(original, PlaylistState::AudioOnly, true);
            }
            cut = audioTrackProducer->cut(p->get_in(), p->get_out());
        }
        else if (current == idForVideoTrack) {
            cut = videoOnlyProducer->cut(p->get_in(), p->get_out());
        }
        if (cut) {
            // We do not re-add effects here, since we cutted the effects are taken from original
            //Clip(*cut).addEffects(*p);
            m_playlist.remove(i);
            m_playlist.insert(*cut, i);
            m_playlist.consolidate_blanks();
            found = true;
            delete cut;
        }
        delete p;
    }
    return found;
}

//TODO: cut: checkSlowMotionProducer
bool Track::replace(qreal t, Mlt::Producer *prod, PlaylistState::ClipState state) {
    m_playlist.lock();
    int index = m_playlist.get_clip_index_at(frame(t));
    Mlt::Producer *cut;
    Mlt::Producer *orig = m_playlist.replace_with_blank(index);
    if (state != PlaylistState::VideoOnly) {
        // Get track duplicate
        Mlt::Producer *copyProd = clipProducer(prod, state);
        cut = copyProd->cut(orig->get_in(), orig->get_out());
        delete copyProd;
    } else {
        cut = prod->cut(orig->get_in(), orig->get_out());
    }
    Clip(*cut).addEffects(*orig);
    delete orig;
    bool ok = !m_playlist.insert_at(frame(t), cut, 1);
    delete cut;
    m_playlist.unlock();
    return ok;
}

Mlt::Producer *Track::find(const QByteArray &name, const QByteArray &value, int startindex) {
    for (int i = 0; i < m_playlist.count(); i++) {
        if (m_playlist.is_blank(i)) continue;
        Mlt::Producer *p = m_playlist.get_clip(i);
        if (value == p->parent().get(name.constData())) return p;
        else delete p;
    }
    return NULL;
}

Mlt::Producer *Track::clipProducer(Mlt::Producer *parent, PlaylistState::ClipState state, bool forceCreation) {
    QString service = parent->parent().get("mlt_service");
    QString originalId = parent->parent().get("id");
    if (!needsDuplicate(service) || state == PlaylistState::VideoOnly || originalId.endsWith("_video")) {
        // Don't clone producer for track if it has no audio
        return new Mlt::Producer(*parent);
    }
    originalId = originalId.section("_", 0, 0);
    QString idForTrack = originalId + QLatin1Char('_') + m_playlist.get("id");
    if (state == PlaylistState::AudioOnly) {
        idForTrack.append("_audio");
    } else if (state == PlaylistState::VideoOnly) {
        idForTrack.append("_video");
    }
    Mlt::Producer *prod = NULL;
    if (!forceCreation) prod = find("id", idForTrack.toUtf8().constData());
    if (prod) {
        *prod = prod->parent();
        return prod;
    }
    prod = Clip(parent->parent()).clone();
    prod->set("id", idForTrack.toUtf8().constData());
    if (state == PlaylistState::AudioOnly) {
        prod->set("video_index", -1);
    } else if (state == PlaylistState::VideoOnly) {
        prod->set("audio_index", -1);
    }
    return prod;
}
