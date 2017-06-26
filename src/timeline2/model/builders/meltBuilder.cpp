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

#include "meltBuilder.hpp"
#include "../clipmodel.hpp"
#include "../timelineitemmodel.hpp"
#include "../timelinemodel.hpp"
#include "../trackmodel.hpp"
#include "../undohelper.hpp"
#include "bin/bin.h"
#include "core.h"
#include "mltcontroller/bincontroller.h"
#include <QDebug>
#include <QSet>
#include <mlt++/MltPlaylist.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltTransition.h>

bool constructTrackFromMelt(const std::shared_ptr<TimelineItemModel> &timeline, int tid, Mlt::Tractor &track, Fun &undo, Fun &redo);
bool constructTrackFromMelt(const std::shared_ptr<TimelineItemModel> &timeline, int tid, Mlt::Playlist &track, Fun &undo, Fun &redo);

bool constructTimelineFromMelt(const std::shared_ptr<TimelineItemModel> &timeline, Mlt::Tractor tractor)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    // First, we destruct the previous tracks
    timeline->requestReset(undo, redo);

    pCore->binController()->loadBinPlaylist(&tractor, timeline->tractor());

    const QStringList ids = pCore->binController()->getClipIds();
    for (const QString &id : ids) {
        if (id == QLatin1String("black")) {
            continue;
        }
        // pass basic info, the others (folder, etc) will be taken from the producer itself
        requestClipInfo info;
        info.imageHeight = 0;
        info.clipId = id;
        info.replaceProducer = true;
        std::shared_ptr<Mlt::Producer> prod(pCore->binController()->getController(id).get()->originalProducer());
        pCore->bin()->slotProducerReady(info, prod);
    }
    QSet<QString> reserved_names{QLatin1String("playlistmain"), QLatin1String("timeline_preview"), QLatin1String("overlay_track"),
                                 QLatin1String("black_track")};

    bool ok = true;
    qDebug() << "//////////////////////\nTrying to construct" << tractor.count() << "tracks.\n////////////////////////////////7";
    for (int i = 0; i < tractor.count() && ok; i++) {
        std::unique_ptr<Mlt::Producer> track(tractor.track(i));
        QString playlist_name = track->get("id");
        if (reserved_names.contains(playlist_name)) {
            continue;
        }
        switch (track->type()) {
        case producer_type:
            // TODO check that it is the black track, and otherwise log an error
            qDebug() << "SUSPICIOUS: we weren't expecting a producer when parsing the timeline";
            break;
        case tractor_type: {
            // that is a double track
            int tid;
            ok = timeline->requestTrackInsertion(-1, tid, undo, redo);
            Mlt::Tractor local_tractor(*track);
            ok = ok && constructTrackFromMelt(timeline, tid, local_tractor, undo, redo);
            break;
        }
        case playlist_type: {
            // that is a single track
            qDebug() << "Adding track: " << track->get("id");
            int tid;
            ok = timeline->requestTrackInsertion(-1, tid, undo, redo);
            timeline->setTrackProperty(tid, "kdenlive:trackheight", "100");
            Mlt::Playlist local_playlist(*track);
            ok = ok && constructTrackFromMelt(timeline, tid, local_playlist, undo, redo);
            QString trackName = local_playlist.get("kdenlive:track_name");
            if (!trackName.isEmpty()) {
                timeline->setTrackProperty(tid, QStringLiteral("kdenlive:track_name"), trackName.toUtf8().constData());
            }
            int audioTrack = local_playlist.get_int("kdenlive:audio_track");
            if (audioTrack == 1) {
                // This is an audio track
                timeline->setTrackProperty(tid, QStringLiteral(""), QStringLiteral("1"));
            }
            break;
        }
        default:
            qDebug() << "ERROR: Unexpected item in the timeline";
        }
    }

    // Loading compositions
    QScopedPointer<Mlt::Service> service(tractor.producer());
    while ((service != nullptr) && service->is_valid()) {
        if (service->type() == transition_type) {
            Mlt::Transition t((mlt_transition)service->get_service());
            int compoId;
            QString id(t.get("mlt_service"));
            qDebug() << "////////// BUILD TRANS ON TK: " << t.get_b_track() << "id: " << id;
            // ok = timeline->requestCompositionInsertion(id, t.get_b_track(), t.get_in(), compoId, undo, redo);
            if (!ok) {
                qDebug() << "ERROR : failed to insert composition in track " << t.get_b_track() << ", position" << t.get_in();
                break;
            }
            qDebug() << "Inserted composition in track " << t.get_b_track() << ", position" << t.get_in();
        }
        service.reset(service->producer());
    }

    if (!ok) {
        // TODO log error
        undo();
        return false;
    }
    return true;
}

bool constructTrackFromMelt(const std::shared_ptr<TimelineItemModel> &timeline, int tid, Mlt::Tractor &track, Fun &undo, Fun &redo)
{
    if (track.count() != 2) {
        // we expect a tractor with two tracks (a "fake" track)
        qDebug() << "ERROR : wrong number of subtracks";
        return false;
    }
    for (int i = 0; i < track.count(); i++) {
        std::unique_ptr<Mlt::Producer> sub_track(track.track(i));
        if (sub_track->type() != playlist_type) {
            qDebug() << "ERROR : SubTracks must be MLT::Playlist";
            return false;
        }
        Mlt::Playlist playlist(*sub_track);
        constructTrackFromMelt(timeline, tid, playlist, undo, redo);
        if (i == 0) {
            // Pass track properties
            int height = track.get_int("kdenlive:trackheight");
            timeline->setTrackProperty(tid, "kdenlive:trackheight", height == 0 ? "100" : QString::number(height));
            QString trackName = track.get("kdenlive:track_name");
            if (!trackName.isEmpty()) {
                timeline->setTrackProperty(tid, QStringLiteral("kdenlive:track_name"), trackName.toUtf8().constData());
            }
            if (track.get_int("kdenlive:audio_track") == 1) {
                // This is an audio track
                timeline->setTrackProperty(tid, QStringLiteral("kdenlive:audio_track"), QStringLiteral("1"));
            }
        }
    }
    return true;
}

bool constructTrackFromMelt(const std::shared_ptr<TimelineItemModel> &timeline, int tid, Mlt::Playlist &track, Fun &undo, Fun &redo)
{
    for (int i = 0; i < track.count(); i++) {
        if (track.is_blank(i)) {
            continue;
        }
        std::shared_ptr<Mlt::Producer> clip(track.get_clip(i));
        int position = track.clip_start(i);
        switch (clip->type()) {
        case unknown_type:
        case producer_type: {
            QString binId = clip->get("kdenlive:id");
            bool ok = false;
            if (pCore->bin()->getBinClip(binId)) {
                int cid = ClipModel::construct(timeline, binId, clip);
                ok = timeline->requestClipMove(cid, tid, position, true, undo, redo);
            } else {
                qDebug() << "// Cannot find bin clip: " << binId;
            }
            if (!ok) {
                qDebug() << "ERROR : failed to insert clip in track" << tid << "position" << position;
                return false;
            }
            qDebug() << "Inserted clip in track" << tid << "at " << position;

            break;
        }
        case tractor_type: {
            // TODO This is a nested timeline
            qDebug() << "NOT_IMPLEMENTED: code for parsing nested timeline is not there yet.";
            break;
        }
        default:
            qDebug() << "ERROR : unexpected object found on playlist";
            return false;
            break;
        }
    }
    return true;
}
