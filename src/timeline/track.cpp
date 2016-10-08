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
#include "headertrack.h"
#include "kdenlivesettings.h"
#include "clip.h"
#include "effectmanager.h"

#include <QtGlobal>
#include <QDebug>
#include <math.h>

Track::Track(int index, const QList<QAction *> &actions, Mlt::Playlist &playlist, TrackType type, int height, QWidget *parent) :
    effectsList(EffectsList(true)),
    type(type),
    trackHeader(NULL),
    m_index(index),
    m_playlist(playlist)
{
    QString playlist_name = playlist.get("id");
    if (playlist_name != "black_track") {
        trackHeader = new HeaderTrack(info(), actions, this, height, parent);
    }
}

Track::~Track()
{
    if (trackHeader) trackHeader->deleteLater();
}

// members access

Mlt::Playlist & Track::playlist()
{
    return m_playlist;
}

qreal Track::fps()
{
    return m_playlist.get_fps();
}

int Track::frame(qreal t)
{
    return round(t * fps());
}

qreal Track::length() {
    return m_playlist.get_playtime() / fps();
}

// basic clip operations
bool Track::add(qreal t, Mlt::Producer *parent, qreal tcut, qreal dtcut, PlaylistState::ClipState state, bool duplicate, TimelineMode::EditMode mode)
{
    Mlt::Producer *cut = NULL;
    if (parent == NULL || !parent->is_valid()) {
        return false;
    }
    if (state == PlaylistState::Disabled) {
        QScopedPointer<Mlt::Producer> prodCopy(Clip(*parent).clone());
        prodCopy->set("video_index", -1);
        prodCopy->set("audio_index", -1);
        prodCopy->set("kdenlive:binid", parent->get("id"));
        cut = prodCopy->cut(frame(tcut), frame(dtcut) - 1);
    } else if (duplicate && state != PlaylistState::VideoOnly) {
        QScopedPointer<Mlt::Producer> newProd(clipProducer(parent, state));
        cut = newProd->cut(frame(tcut), frame(dtcut) - 1);
    }
    else {
        cut = parent->cut(frame(tcut), frame(dtcut) - 1);
    }
    if (parent->is_cut()) {
        Clip(*cut).addEffects(*parent);
    }
    m_playlist.lock();
    bool result = doAdd(t, cut, mode);
    m_playlist.unlock();
    delete cut;
    return result;
}

bool Track::doAdd(qreal t, Mlt::Producer *cut, TimelineMode::EditMode mode)
{
    int pos = frame(t);
    if (pos < m_playlist.get_playtime() && mode == TimelineMode::InsertEdit) {
        m_playlist.split_at(pos);
    }
    m_playlist.consolidate_blanks();
    if (m_playlist.insert_at(pos, cut, 1) == m_playlist.count() - 1) {
        emit newTrackDuration(m_playlist.get_playtime());
    }
    return true;
}

bool Track::move(qreal start, qreal end, TimelineMode::EditMode mode)
{
    int pos = frame(start);
    m_playlist.lock();
    int clipIndex = m_playlist.get_clip_index_at(pos);
    bool durationChanged = false;
    if (clipIndex == m_playlist.count() - 1) {
	durationChanged = true;
    }
    QScopedPointer <Mlt::Producer> clipProducer(m_playlist.replace_with_blank(clipIndex));
    if (!clipProducer || clipProducer->is_blank()) {
        qDebug() << "// Cannot get clip at index: "<<clipIndex<<" / "<< start;
        m_playlist.unlock();
        return false;
    }
    m_playlist.consolidate_blanks();
    if (end >= m_playlist.get_playtime()) {
	// Clip is inserted at the end of track, duration change event handled in doAdd()
	durationChanged = false;
    }
    bool result = doAdd(end, clipProducer.data(), mode);
    m_playlist.unlock();
    if (durationChanged) {
	emit newTrackDuration(m_playlist.get_playtime());
    }
    return result;
}

bool Track::isLastClip(qreal t)
{
    int clipIndex = m_playlist.get_clip_index_at(frame(t));
    if (clipIndex >= m_playlist.count() - 1) {
	return true;
    }
    return false;
}

bool Track::del(qreal t, bool checkDuration)
{
    m_playlist.lock();
    bool durationChanged = false;
    int pos = frame(t);
    int ix = m_playlist.get_clip_index_at(pos);
    if (ix == m_playlist.count() - 1) {
	durationChanged = true;
    }
    Mlt::Producer *clip = m_playlist.replace_with_blank(ix);
    if (clip) {
        delete clip;
    }
    else {
        qWarning("Error deleting clip at %d, tk: %d", pos, m_index);
        m_playlist.unlock();
        return false;
    }
    m_playlist.consolidate_blanks();
    m_playlist.unlock();
    if (durationChanged && checkDuration) {
        emit newTrackDuration(m_playlist.get_playtime());
    }
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
    int startFrame = frame(t);
    int index = m_playlist.get_clip_index_at(startFrame);
    int length = frame(dt);
    QScopedPointer<Mlt::Producer> clip(m_playlist.get_clip(index));
    if (clip == NULL || clip->is_blank()) {
        qWarning("Can't resize clip at %f", t);
	m_playlist.unlock();
        return false;
    }

    int in = clip->get_in();
    int out = clip->get_out();
    if (end) {
        // Resizing clip end
        startFrame += out - in;
        out += length;
    } else {
        // Resizing clip start
        in += length;
    }

    //image or color clips are not bounded
    if (in < 0) {out -= in; in = 0;}
    if (clip->get_length() < out + 1) {
        clip->parent().set("length", out + 2);
        clip->parent().set("out", out + 1);
        clip->set("length", out + 2);
    }

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
	qDebug()<<" - - --Warning, clip is blank at: "<<index;
        m_playlist.unlock();
        return false;
    }
    if (m_playlist.split(index, pos - m_playlist.clip_start(index) - 1)) {
        qWarning("MLT split failed");
        m_playlist.unlock();
        return false;
    }
    m_playlist.unlock();
    QScopedPointer<Mlt::Producer> clip1(m_playlist.get_clip(index + 1));
    QScopedPointer<Mlt::Producer> clip2(m_playlist.get_clip(index));
    qDebug()<<"CLIP CUT ID: "<<clip1->get("id")<<" / "<<clip1->parent().get("id");
    Clip (*clip1).addEffects(*clip2, true);
    // adjust filters in/out
    Clip (*clip2).adjustEffectsLength();
    return true;
}

bool Track::needsDuplicate(const QString &service) const
{
    return (service.contains(QStringLiteral("avformat")) || service.contains(QStringLiteral("consumer")) || service.contains(QStringLiteral("xml")));
}

void Track::lockTrack(bool locked)
{
    if (!trackHeader) return;
    setProperty(QStringLiteral("kdenlive:locked_track"), locked ? 1 : 0);
    trackHeader->setLock(locked);
}

void Track::replaceId(const QString &id)
{
    QString idForAudioTrack = id + QLatin1Char('_') + m_playlist.get("id") + "_audio";
    QString idForVideoTrack = id + "_video";
    QString idForTrack = id + QLatin1Char('_') + m_playlist.get("id");
    //TODO: slowmotion
    for (int i = 0; i < m_playlist.count(); i++) {
        if (m_playlist.is_blank(i)) continue;
        QScopedPointer<Mlt::Producer> p(m_playlist.get_clip(i));
        QString current = p->parent().get("id");
	if (current == id || current == idForTrack || current == idForAudioTrack || current == idForVideoTrack || current.startsWith("slowmotion:" + id + ":")) {
	    current.prepend("#");
	    p->parent().set("id", current.toUtf8().constData());
	}
    }
}

QList <Track::SlowmoInfo> Track::getSlowmotionInfos(const QString &id)
{
    QList <Track::SlowmoInfo> list;
    QLocale locale;
    for (int i = 0; i < m_playlist.count(); i++) {
        if (m_playlist.is_blank(i)) continue;
        QScopedPointer<Mlt::Producer> p(m_playlist.get_clip(i));
        QString current = p->parent().get("id");
	if (!current.startsWith(QLatin1String("#"))) {
	    continue;
	}
	current.remove(0, 1);
	if (current.startsWith("slowmotion:" + id + ":")) {
            Track::SlowmoInfo info;
            info.readFromString(current.section(":", 2), locale);
            list << info;
        }
    }
    return list;
}

QList <ItemInfo> Track::replaceAll(const QString &id, Mlt::Producer *original, Mlt::Producer *videoOnlyProducer, QMap <QString, Mlt::Producer *> newSlowMos)
{
    QString idForAudioTrack;
    QString idForVideoTrack;
    QString service = original->parent().get("mlt_service");
    QString idForTrack = original->parent().get("id");
    QLocale locale;
    int tkState = state();
    if (needsDuplicate(service)) {
        // We have to use the track clip duplication functions, because of audio glitches in MLT's multitrack
        idForAudioTrack = idForTrack + QLatin1Char('_') + m_playlist.get("id") + "_audio";
        idForVideoTrack = idForTrack + "_video";
        idForTrack.append(QLatin1Char('_') + m_playlist.get("id"));
    }
    Mlt::Producer *trackProducer = NULL;
    Mlt::Producer *audioTrackProducer = NULL;
    QList <ItemInfo> replaced;
    Mlt::ClipInfo *info = new Mlt::ClipInfo();
    for (int i = 0; i < m_playlist.count(); i++) {
        if (m_playlist.is_blank(i)) continue;
        QScopedPointer<Mlt::Producer> p(m_playlist.get_clip(i));
        QString current = p->parent().get("id");
        if (current == id) {
            if (tkState & 1 || (QString(p->parent().get("mlt_service")).contains(QStringLiteral("avformat")) && p->parent().get_int("video_index") == -1)) {
                // Video is hidden, nothing visible
                continue;
            }
            // master clip used, only notify for update
            m_playlist.clip_info(i, info);
            ItemInfo cInfo;
            cInfo.startPos = GenTime(info->start, fps());
            cInfo.endPos = GenTime(info->start + info->frame_count, fps());
            cInfo.track = m_index;
            replaced << cInfo;
            continue;
        }
	if (!current.startsWith(QLatin1String("#"))) {
	    continue;
	}
	current.remove(0, 1);
        Mlt::Producer *cut = NULL;
	if (current.startsWith("slowmotion:" + id + ":")) {
	      // Slowmotion producer, just update resource
	      Mlt::Producer *slowProd = newSlowMos.value(current.section(QStringLiteral(":"), 2));
	      if (!slowProd || !slowProd->is_valid()) {
		    qDebug()<<"/// WARNING, couldn't find replacement slowmo for "<<id;
		    continue;
	      }
	      cut = slowProd->cut(p->get_in(), p->get_out());
	}
        if (!cut && idForAudioTrack.isEmpty()) {
            if (current == idForTrack) {
                // No duplication required
                cut = original->cut(p->get_in(), p->get_out());
            }
            else {
		continue;
	    }
        }
        if (!cut && p->parent().get_int("audio_index") == -1 && current == id) {
	        // No audio - no duplication required
                cut = original->cut(p->get_in(), p->get_out());
	}
        else if (!cut && current == idForTrack) {
            // Use duplicate
            if (trackProducer == NULL) {
                trackProducer = Clip(*original).clone();
                trackProducer->set("id", idForTrack.toUtf8().constData());
            }
            cut = trackProducer->cut(p->get_in(), p->get_out());
        }
        else if (!cut && current == idForAudioTrack) {
            if (audioTrackProducer == NULL) {
                audioTrackProducer = clipProducer(original, PlaylistState::AudioOnly, true);
            }
            cut = audioTrackProducer->cut(p->get_in(), p->get_out());
        }
        else if (!cut && current == idForVideoTrack) {
            cut = videoOnlyProducer->cut(p->get_in(), p->get_out());
        }
        if (cut) {
            Clip(*cut).addEffects(*p);
            m_playlist.clip_info(i, info);
            m_playlist.remove(i);
            m_playlist.insert(*cut, i);
            m_playlist.consolidate_blanks();
            delete cut;
            if (tkState & 1 || (QString(p->parent().get("mlt_service")).contains(QStringLiteral("avformat")) && p->parent().get_int("video_index") == -1)) {
                // Video is hidden for this track, nothing visible
                continue;
            }
            ItemInfo cInfo;
            cInfo.startPos = GenTime(info->start, fps());
            cInfo.endPos = GenTime(info->start + info->frame_count, fps());
            cInfo.track = m_index;
            replaced << cInfo;
        }
    }
    delete trackProducer;
    delete info;
    return replaced;
}

//TODO: cut: checkSlowMotionProducer
bool Track::replace(qreal t, Mlt::Producer *prod, PlaylistState::ClipState state, PlaylistState::ClipState originalState) {
    m_playlist.lock();
    int index = m_playlist.get_clip_index_at(frame(t));
    Mlt::Producer *cut;
    QScopedPointer <Mlt::Producer> orig(m_playlist.replace_with_blank(index));
    QString service = prod->get("mlt_service");
    if (state == PlaylistState::Disabled) {
        QScopedPointer<Mlt::Producer> prodCopy(Clip(*prod).clone());
        // Reset id to let MLT give a new one
        prodCopy->set("id", (char*)NULL);
        prodCopy->set("video_index", -1);
        prodCopy->set("audio_index", -1);
        prodCopy->set("kdenlive:binid", prod->get("id"));
        prodCopy->set("kdenlive:clipstate", (int) originalState);
        cut = prodCopy->cut(orig->get_in(), orig->get_out());
    } else if (state != PlaylistState::VideoOnly && service != QLatin1String("timewarp")) {
        // Get track duplicate
        Mlt::Producer *copyProd = clipProducer(prod, state);
        cut = copyProd->cut(orig->get_in(), orig->get_out());
        delete copyProd;
    } else {
        cut = prod->cut(orig->get_in(), orig->get_out());
    }
    Clip(*cut).addEffects(*orig);
    bool ok = m_playlist.insert_at(frame(t), cut, 1) >= 0;
    delete cut;
    m_playlist.unlock();
    return ok;
}

void Track::updateEffects(const QString &id, Mlt::Producer *original)
{
    QString idForAudioTrack;
    QString idForVideoTrack;
    QString service = original->parent().get("mlt_service");
    QString idForTrack = original->parent().get("id");
    if (needsDuplicate(service)) {
        // We have to use the track clip duplication functions, because of audio glitches in MLT's multitrack
        idForAudioTrack = idForTrack + QLatin1Char('_') + m_playlist.get("id") + "_audio";
        idForVideoTrack = idForTrack + "_video";
        idForTrack.append(QLatin1Char('_') + m_playlist.get("id"));
    }

    for (int i = 0; i < m_playlist.count(); i++) {
        if (m_playlist.is_blank(i)) continue;
        QScopedPointer<Mlt::Producer> p(m_playlist.get_clip(i));
	Mlt::Producer origin = p->parent();
        QString current = origin.get("id");
	if (current.startsWith(QLatin1String("slowmotion:"))) {
            if (current.section(QStringLiteral(":"), 1, 1) == id) {
		Clip(origin).replaceEffects(*original);
            }
	}
	else if (current == id) {
            // we are directly using original producer, no need to update effects
            continue;
	}
	else if (current.section(QStringLiteral("_"), 0, 0) == id) {
            Clip(origin).replaceEffects(*original);
	}
    }
}

/*Mlt::Producer &Track::find(const QByteArray &name, const QByteArray &value, int startindex) {
    for (int i = startindex; i < m_playlist.count(); i++) {
        if (m_playlist.is_blank(i)) continue;
        QScopedPointer<Mlt::Producer> p(m_playlist.get_clip(i));
        if (value == p->parent().get(name.constData())) {
            return p->parent();
        }
    }
    return Mlt::0;
}*/

Mlt::Producer *Track::clipProducer(Mlt::Producer *parent, PlaylistState::ClipState state, bool forceCreation) {
    QString service = parent->parent().get("mlt_service");
    QString originalId = parent->parent().get("id");
    if (!needsDuplicate(service) || state == PlaylistState::VideoOnly || originalId.endsWith(QLatin1String("_video"))) {
        // Don't clone producer for track if it has no audio
        return new Mlt::Producer(*parent);
    }
    originalId = originalId.section(QStringLiteral("_"), 0, 0);
    QString idForTrack = originalId + QLatin1Char('_') + m_playlist.get("id");
    if (state == PlaylistState::AudioOnly) {
        idForTrack.append("_audio");
    }
    if (!forceCreation) {
        for (int i = 0; i < m_playlist.count(); i++) {
            if (m_playlist.is_blank(i)) continue;
            QScopedPointer<Mlt::Producer> p(m_playlist.get_clip(i));
            if (QString(p->parent().get("id")) == idForTrack) {
                return new Mlt::Producer(p->parent());
            }
        }
    }
    Mlt::Producer *prod = Clip(parent->parent()).clone();
    prod->set("id", idForTrack.toUtf8().constData());
    if (state == PlaylistState::AudioOnly) {
        prod->set("video_index", -1);
    }
    return prod;
}

bool Track::hasAudio() 
{
    for (int i = 0; i < m_playlist.count(); i++) {
        if (m_playlist.is_blank(i)) continue;
        QScopedPointer<Mlt::Producer> p(m_playlist.get_clip(i));
        QString service = p->get("mlt_service");
        if (service == QLatin1String("xml") || service == QLatin1String("consumer") || p->get_int("audio_index") > -1) {
            return true;
        }
    }
    return false;
}

void Track::setProperty(const QString &name, const QString &value)
{
    m_playlist.set(name.toUtf8().constData(), value.toUtf8().constData());
}

void Track::setProperty(const QString &name, int value)
{
    m_playlist.set(name.toUtf8().constData(), value);
}

const QString Track::getProperty(const QString &name)
{
    return QString(m_playlist.get(name.toUtf8().constData()));
}

int Track::getIntProperty(const QString &name)
{
    return m_playlist.get_int(name.toUtf8().constData());
}

TrackInfo Track::info()
{
    TrackInfo info;
    info.trackName = m_playlist.get("kdenlive:track_name");
    info.isLocked= m_playlist.get_int("kdenlive:locked_track");
    int currentState = m_playlist.parent().get_int("hide");
    info.isMute = currentState & 2;
    info.isBlind = currentState & 1;
    info.type = type;
    info.effectsList = effectsList;
    info.duration = m_playlist.get_length();
    return info;
}

void Track::setInfo(TrackInfo info)
{
    if (!trackHeader) return;
    m_playlist.set("kdenlive:track_name", info.trackName.toUtf8().constData());
    m_playlist.set("kdenlive:locked_track", info.isLocked ? 1 : 0);
    int state = 0;
    if (info.isMute) {
        if (info.isBlind) state = 3;
        else state = 2;
    }
    else if (info.isBlind) state = 1;
    m_playlist.parent().set("hide", state);
    type = info.type;
    trackHeader->updateStatus(info);
}

int Track::state()
{
    return m_playlist.parent().get_int("hide");
}

void Track::setState(int state)
{
    m_playlist.parent().set("hide", state);
}

int Track::getBlankLength(int pos, bool fromBlankStart)
{
    int clipIndex = m_playlist.get_clip_index_at(pos);
    if (clipIndex == m_playlist.count()) {
        // We are after the end of the playlist
        return -1;
    }
    if (!m_playlist.is_blank(clipIndex)) return 0;
    if (fromBlankStart) return m_playlist.clip_length(clipIndex);
    return m_playlist.clip_length(clipIndex) + m_playlist.clip_start(clipIndex) - pos;
}

void Track::updateClipProperties(const QString &id, QMap <QString, QString> properties)
{
    QString idForTrack = id + QLatin1Char('_') + m_playlist.get("id");
    QString idForVideoTrack = id + "_video";
    QString idForAudioTrack = idForTrack + "_audio";
    // slowmotion producers are updated in renderer

    for (int i = 0; i < m_playlist.count(); i++) {
        if (m_playlist.is_blank(i)) continue;
        QScopedPointer<Mlt::Producer> p(m_playlist.get_clip(i));
        QString current = p->parent().get("id");
        QStringList processed;
        if (!processed.contains(current) && (current == idForTrack || current == idForAudioTrack || current == idForVideoTrack)) {
            QMapIterator<QString, QString> i(properties);
            while (i.hasNext()) {
                i.next();
                p->parent().set(i.key().toUtf8().constData(), i.value().toUtf8().constData());
            }
            processed << current;
        }
    }
}

Mlt::Producer *Track::buildSlowMoProducer(Mlt::Properties passProps, const QString &url, const QString &id, Track::SlowmoInfo info)
{
    QLocale locale;
    Mlt::Producer *prod = new Mlt::Producer(*m_playlist.profile(), 0, ("timewarp:" + url).toUtf8().constData());
    if (!prod->is_valid()) {
	qDebug()<<"++++ FAILED TO CREATE SLOWMO PROD";
	return NULL;
    }
    QString producerid = "slowmotion:" + id + ':' + info.toString(locale);
    prod->set("id", producerid.toUtf8().constData());

    // copy producer props
    for (int i = 0; i < passProps.count(); i++) {
	prod->set(passProps.get_name(i), passProps.get(i)); 
    }
    // set clip state
    switch ((int) info.state) {
        case PlaylistState::VideoOnly:
            prod->set("audio_index", -1);
            break;
        case PlaylistState::AudioOnly:
            prod->set("video_index", -1);
            break;
        default:
            break;
    }
    QString slowmoId = info.toString(locale);
    slowmoId.append(prod->get("warp_resource"));
    emit storeSlowMotion(slowmoId, prod);
    return prod;
}

int Track::changeClipSpeed(ItemInfo info, ItemInfo speedIndependantInfo, PlaylistState::ClipState state, double speed, int strobe, Mlt::Producer *prod, const QString &id, Mlt::Properties passProps, bool removeEffect)
{
    //TODO: invalidate preview rendering
    int newLength = 0;
    int startPos = info.startPos.frames(fps());
    int clipIndex = m_playlist.get_clip_index_at(startPos);
    int clipLength = m_playlist.clip_length(clipIndex);
    m_playlist.lock();
    QScopedPointer<Mlt::Producer> original(m_playlist.get_clip(clipIndex));
    if (original == NULL) {
        qDebug()<<"// No clip to apply effect";
        m_playlist.unlock();
        return -1;
    }
    if (!original->is_valid() || original->is_blank()) {
        // invalid clip
        qDebug()<<"// Invalid clip to apply effect";
        m_playlist.unlock();
        return -1;
    }
    Mlt::Producer clipparent = original->parent();
    if (!clipparent.is_valid() || clipparent.is_blank()) {
        // invalid clip
        qDebug()<<"// Invalid parent to apply effect";
        m_playlist.unlock();
        return -1;
    }
    QLocale locale;
    if (speed <= 0 && speed > -1) speed = 1.0;
    QString serv = clipparent.get("mlt_service");
    QString url;
    if (serv == QLatin1String("timewarp")) {
        url = QString::fromUtf8(clipparent.get("warp_resource"));
    } else {
        url = QString::fromUtf8(clipparent.get("resource"));
    }
    url.prepend(locale.toString(speed) + ':');
    Track::SlowmoInfo slowInfo;
    slowInfo.speed = speed;
    slowInfo.strobe = strobe;
    slowInfo.state = state;
    if (serv.contains(QStringLiteral("avformat"))) {
	if (speed != 1.0 || strobe > 1) {
	    if (!prod || !prod->is_valid()) {
		prod = buildSlowMoProducer(passProps, url, id, slowInfo);
                if (prod == NULL) {
                    // error, abort
                    qDebug()<<"++++ FAILED TO CREATE SLOWMO PROD";
                    m_playlist.unlock();
                    return -1;
                }
	    }
	    QScopedPointer <Mlt::Producer> clip(m_playlist.replace_with_blank(clipIndex));
	    m_playlist.consolidate_blanks(0);

	    // Check that the blank space is long enough for our new duration
	    clipIndex = m_playlist.get_clip_index_at(startPos);
	    int blankEnd = m_playlist.clip_start(clipIndex) + m_playlist.clip_length(clipIndex);
	    Mlt::Producer *cut;
	    if (clipIndex + 1 < m_playlist.count() && (startPos + clipLength / speed > blankEnd)) {
		GenTime maxLength = GenTime(blankEnd, fps()) - info.startPos;
		cut = prod->cut((int)(info.cropStart.frames(fps()) / speed), (int)(info.cropStart.frames(fps()) / speed + maxLength.frames(fps()) - 1));
	    } else cut = prod->cut((int)(info.cropStart.frames(fps()) / speed), (int)((info.cropStart.frames(fps()) + clipLength) / speed - 1));

	    // move all effects to the correct producer
	    Clip(*cut).addEffects(*clip);
	    m_playlist.insert_at(startPos, cut, 1);
	    delete cut;
	    clipIndex = m_playlist.get_clip_index_at(startPos);
	    newLength = m_playlist.clip_length(clipIndex);
	} else if (speed == 1.0 && strobe < 2) {
	    QScopedPointer <Mlt::Producer> clip(m_playlist.replace_with_blank(clipIndex));
	    m_playlist.consolidate_blanks(0);

	    // Check that the blank space is long enough for our new duration
	    clipIndex = m_playlist.get_clip_index_at(startPos);
	    int blankEnd = m_playlist.clip_start(clipIndex) + m_playlist.clip_length(clipIndex);

	    Mlt::Producer *cut;
	
	    if (!prod || !prod->is_valid()) {
		prod = buildSlowMoProducer(passProps, url, id, slowInfo);
                if (prod == NULL) {
                    // error, abort
                    qDebug()<<"++++ FAILED TO CREATE SLOWMO PROD";
                    m_playlist.unlock();
                    return -1;
                }
            }

	    int originalStart = (int)(speedIndependantInfo.cropStart.frames(fps()));
	    if (clipIndex + 1 < m_playlist.count() && (info.startPos + speedIndependantInfo.cropDuration).frames(fps()) > blankEnd) {
		GenTime maxLength = GenTime(blankEnd, fps()) - info.startPos;
		cut = prod->cut(originalStart, (int)(originalStart + maxLength.frames(fps()) - 1));
	    } else {
		cut = prod->cut(originalStart, (int)(originalStart + speedIndependantInfo.cropDuration.frames(fps())) - 1);
	    }

	    // move all effects to the correct producer
	    Clip(*cut).addEffects(*clip);
	    m_playlist.insert_at(startPos, cut, 1);
	    delete cut;
	    clipIndex = m_playlist.get_clip_index_at(startPos);
	    newLength = m_playlist.clip_length(clipIndex);
	}
    } else if (serv == QLatin1String("timewarp")) {
        if (!prod || !prod->is_valid()) {
            prod = buildSlowMoProducer(passProps, url, id, slowInfo);
            if (prod == NULL) {
                // error, abort
                qDebug()<<"++++ FAILED TO CREATE SLOWMO PROD";
                m_playlist.unlock();
                return -1;
            }
        }
        if (removeEffect) {
            prod = clipProducer(prod, state);
        }
        QScopedPointer <Mlt::Producer> clip(m_playlist.replace_with_blank(clipIndex));
        m_playlist.consolidate_blanks(0);
        int duration;
        int originalStart;
        if (speed == 1.0) {
          duration = speedIndependantInfo.cropDuration.frames(fps());
          originalStart = speedIndependantInfo.cropStart.frames(fps());
        } else {
          duration = qMax(2, qAbs((int) (speedIndependantInfo.cropDuration.frames(fps()) / speed + 0.5)));
          originalStart = (int)(speedIndependantInfo.cropStart.frames(fps()) / speed + 0.5);
        }
        //qDebug()<<"/ / /UPDATE SPEED: "<<speed<<", "<<speedIndependantInfo.cropStart.frames(fps())<<":"<<originalStart;
        // Check that the blank space is long enough for our new duration
        clipIndex = m_playlist.get_clip_index_at(startPos);

        Mlt::Producer *cut;
        int outPos = originalStart + duration - 1;
        if (clipIndex + 1 < m_playlist.count()) {
            int blankEnd = m_playlist.clip_start(clipIndex + 1) - 1;
            if (duration + startPos > blankEnd) {
                outPos = originalStart + (blankEnd - startPos);
            }
        }
        // Last clip in playlist, no duration limit
	cut = prod->cut(originalStart, outPos);
        // move all effects to the correct producer
        Clip(*cut).addEffects(*clip);
        m_playlist.insert_at(startPos, cut, 1);
        delete cut;
        clipIndex = m_playlist.get_clip_index_at(startPos);
        newLength = m_playlist.clip_length(clipIndex);
        if (removeEffect)
            delete prod;
    }
    //Do not delete prod, it is now stored in the slowmotion producers list
    m_playlist.unlock();
    if (clipIndex + 1 == m_playlist.count()) {
        // We changed the speed of last clip in playlist, check track length
        emit newTrackDuration(m_playlist.get_playtime());
    }
    return newLength;
}

int Track::index() const
{
    return m_index;
}


int Track::spaceLength(int pos, bool fromBlankStart)
{
    int clipIndex = m_playlist.get_clip_index_at(pos);
    if (clipIndex == m_playlist.count()) {
        // We are after the end of the playlist
        return -1;
    }
    if (!m_playlist.is_blank(clipIndex)) return 0;
    if (fromBlankStart) return m_playlist.clip_length(clipIndex);
    return m_playlist.clip_length(clipIndex) + m_playlist.clip_start(clipIndex) - pos;
}

void Track::disableEffects(bool disable)
{
    // Disable track effects
    enableTrackEffects(QList <int> (), disable, true);
    // Disable timeline clip effects
    for (int i = 0; i < m_playlist.count(); i++) {
        QScopedPointer<Mlt::Producer> original(m_playlist.get_clip(i));
        if (original == NULL || !original->is_valid() || original->is_blank()) {
            // invalid clip
            continue;
        }
        Clip(*original).disableEffects(disable);
    }
}

bool Track::addEffect(double start, EffectsParameterList params)
{
    int pos = frame(start);
    int clipIndex = m_playlist.get_clip_index_at(pos);
    int duration = m_playlist.clip_length(clipIndex);
    QScopedPointer<Mlt::Producer> clip(m_playlist.get_clip(clipIndex));
    if (!clip) {
        return false;
    }
    Mlt::Service clipService(clip->get_service());
    EffectManager effect(clipService);
    return effect.addEffect(params, duration);
}

bool Track::addTrackEffect(EffectsParameterList params)
{
    Mlt::Service trackService(m_playlist.get_service());
    EffectManager effect(trackService);
    int duration = m_playlist.get_playtime() - 1;
    return effect.addEffect(params, duration);
}

bool Track::editEffect(double start, EffectsParameterList params, bool replace)
{
    int pos = frame(start);
    int clipIndex = m_playlist.get_clip_index_at(pos);
    int duration = m_playlist.clip_length(clipIndex);
    QScopedPointer<Mlt::Producer> clip(m_playlist.get_clip(clipIndex));
    if (!clip) {
        return false;
    }
    Mlt::Service clipService(clip->get_service());
    EffectManager effect(clipService);
    return effect.editEffect(params, duration, replace);
}

bool Track::editTrackEffect(EffectsParameterList params, bool replace)
{
    EffectManager effect(m_playlist);
    int duration = m_playlist.get_playtime() - 1;
    return effect.editEffect(params, duration, replace);
}

bool Track::removeEffect(double start, int effectIndex, bool updateIndex)
{
    int pos = frame(start);
    int clipIndex = m_playlist.get_clip_index_at(pos);
    QScopedPointer<Mlt::Producer> clip(m_playlist.get_clip(clipIndex));
    if (!clip) {
        return false;
    }
    Mlt::Service clipService(clip->get_service());
    EffectManager effect(clipService);
    return effect.removeEffect(effectIndex, updateIndex);
}

bool Track::removeTrackEffect(int effectIndex, bool updateIndex)
{
    EffectManager effect(m_playlist);
    return effect.removeEffect(effectIndex, updateIndex);
}

bool Track::enableEffects(double start, const QList <int> &effectIndexes, bool disable)
{
    int pos = frame(start);
    int clipIndex = m_playlist.get_clip_index_at(pos);
    QScopedPointer<Mlt::Producer> clip(m_playlist.get_clip(clipIndex));
    if (!clip) {
        return false;
    }
    Mlt::Service clipService(clip->get_service());
    EffectManager effect(clipService);
    return effect.enableEffects(effectIndexes, disable);
}

bool Track::enableTrackEffects(const QList <int> &effectIndexes, bool disable, bool remember)
{
    EffectManager effect(m_playlist);
    return effect.enableEffects(effectIndexes, disable, remember);
}

bool Track::moveEffect(double start, int oldPos, int newPos)
{
    int pos = frame(start);
    int clipIndex = m_playlist.get_clip_index_at(pos);
    QScopedPointer<Mlt::Producer> clip(m_playlist.get_clip(clipIndex));
    if (!clip) {
        return false;
    }
    Mlt::Service clipService(clip->get_service());
    EffectManager effect(clipService);
    return effect.moveEffect(oldPos, newPos);
}

bool Track::moveTrackEffect(int oldPos, int newPos)
{
    EffectManager effect(m_playlist);
    return effect.moveEffect(oldPos, newPos);
}

QList <QPoint> Track::visibleClips()
{
    QList <QPoint> clips;
    int tkState = state();
    if (tkState & 1) {
        // Video is hidden for this track, nothing visible
        return clips;
    }
    QPoint current;
    Mlt::ClipInfo *info = new Mlt::ClipInfo();
    for (int i = 0; i < m_playlist.count(); i++) {
        if (m_playlist.is_blank(i)) continue;

        // get producer, check if it has video
        m_playlist.clip_info(i, info);
        Mlt::Producer *clip = info->producer;
        QString service = clip->get("mlt_service");
        if (service.contains(QStringLiteral("avformat"))) {
            // Check if it is audio only
            if (clip->get_int("video_index") == -1) {
                continue;
            }
        }
        // Found a clip
        int cStart = info->start;
        int length = info->frame_count;
        if (current.isNull()) {
            current.setX(cStart);
            current.setY(cStart + length);
        } else if (cStart - current.y() < 25) {
            current.setY(cStart + length);
        } else {
            clips << current;
            current.setX(cStart);
            current.setY(cStart + length);
        }
    }
    if (!current.isNull())
        clips << current;
    delete info;
    return clips;
}

bool Track::resize_in_out(int pos, int in, int out)
{
    int ix = m_playlist.get_clip_index_at(pos);
    m_playlist.resize_clip(ix, in, out);
    return true;
}
