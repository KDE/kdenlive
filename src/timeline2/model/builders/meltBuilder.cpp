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
#include "bin/projectitemmodel.h"
#include "core.h"
#include "kdenlivesettings.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <QDebug>
#include <QProgressDialog>
#include <QSet>
#include <mlt++/MltPlaylist.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltFilter.h>
#include <mlt++/MltTransition.h>
#include <QApplication>

static QStringList m_errorMessage;

bool constructTrackFromMelt(const std::shared_ptr<TimelineItemModel> &timeline, int tid, Mlt::Tractor &track,
                            const std::unordered_map<QString, QString> &binIdCorresp, Fun &undo, Fun &redo, bool audioTrack, QString originalDecimalPoint, QProgressDialog *progressDialog = nullptr);
bool constructTrackFromMelt(const std::shared_ptr<TimelineItemModel> &timeline, int tid, Mlt::Playlist &track,
                            const std::unordered_map<QString, QString> &binIdCorresp, Fun &undo, Fun &redo, bool audioTrack, QString originalDecimalPoint, QProgressDialog *progressDialog = nullptr);

bool constructTimelineFromMelt(const std::shared_ptr<TimelineItemModel> &timeline, Mlt::Tractor tractor, QProgressDialog *progressDialog, QString originalDecimalPoint)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    // First, we destruct the previous tracks
    timeline->requestReset(undo, redo);
    m_errorMessage.clear();
    std::unordered_map<QString, QString> binIdCorresp;
    QStringList expandedFolders;
    pCore->projectItemModel()->loadBinPlaylist(&tractor, timeline->tractor(), binIdCorresp, expandedFolders, progressDialog);
    pCore->bin()->checkMissingProxies();
    QStringList foldersToExpand;
    // Find updated ids for expanded folders
    for (const QString &folderId : expandedFolders) {
        if (binIdCorresp.count(folderId) > 0) {
            foldersToExpand << binIdCorresp.at(folderId);
        }
    }
    pCore->bin()->loadFolderState(foldersToExpand);

    QSet<QString> reserved_names{QLatin1String("playlistmain"), QLatin1String("timeline_preview"), QLatin1String("timeline_overlay"),
                                 QLatin1String("black_track")};
    bool ok = true;
    qDebug() << "//////////////////////\nTrying to construct" << tractor.count() << "tracks.\n////////////////////////////////";

    // Import master track effects
    std::shared_ptr<Mlt::Service> serv = std::make_shared<Mlt::Service>(tractor.get_service());
    timeline->importMasterEffects(serv);

    QList <int> videoTracksIndexes;
    QList <int> lockedTracksIndexes;
    // Black track index
    videoTracksIndexes << 0;
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
            bool audioTrack = track->get_int("kdenlive:audio_track") == 1;
            if (!audioTrack) {
                videoTracksIndexes << i;
            }
            ok = timeline->requestTrackInsertion(-1, tid, QString(), audioTrack, undo, redo, false);
            if (track->get_int("kdenlive:locked_track") > 0) {
                lockedTracksIndexes << tid;
            }
            Mlt::Tractor local_tractor(*track);
            ok = ok && constructTrackFromMelt(timeline, tid, local_tractor, binIdCorresp, undo, redo, audioTrack, originalDecimalPoint, progressDialog);
            timeline->setTrackProperty(tid, QStringLiteral("kdenlive:thumbs_format"), track->get("kdenlive:thumbs_format"));
            timeline->setTrackProperty(tid, QStringLiteral("kdenlive:audio_rec"), track->get("kdenlive:audio_rec"));
            timeline->setTrackProperty(tid, QStringLiteral("kdenlive:timeline_active"), track->get("kdenlive:timeline_active"));
            break;
        }
        case playlist_type: {
            // that is a single track
            qDebug() << "Adding track: " << track->get("id");
            int tid;
            Mlt::Playlist local_playlist(*track);
            const QString trackName = local_playlist.get("kdenlive:track_name");
            bool audioTrack = local_playlist.get_int("kdenlive:audio_track") == 1;
            if (!audioTrack) {
                videoTracksIndexes << i;
            }
            ok = timeline->requestTrackInsertion(-1, tid, trackName, audioTrack, undo, redo, false);
            int muteState = track->get_int("hide");
            if (muteState > 0 && (!audioTrack || (audioTrack && muteState != 1))) {
                timeline->setTrackProperty(tid, QStringLiteral("hide"), QString::number(muteState));
            }

            ok = ok && constructTrackFromMelt(timeline, tid, local_playlist, binIdCorresp, undo, redo, audioTrack, originalDecimalPoint, progressDialog);
            if (local_playlist.get_int("kdenlive:locked_track") > 0) {
                lockedTracksIndexes << tid;
            }
            timeline->setTrackProperty(tid, QStringLiteral("kdenlive:thumbs_format"), local_playlist.get("kdenlive:thumbs_format"));
            timeline->setTrackProperty(tid, QStringLiteral("kdenlive:audio_rec"), track->get("kdenlive:audio_rec"));
            timeline->setTrackProperty(tid, QStringLiteral("kdenlive:timeline_active"), track->get("kdenlive:timeline_active"));
            break;
        }
        default:
            qDebug() << "ERROR: Unexpected item in the timeline";
        }
    }
    timeline->_resetView();

    // Loading compositions
    QScopedPointer<Mlt::Service> service(tractor.producer());
    QList<Mlt::Transition *> compositions;
    while ((service != nullptr) && service->is_valid()) {
        if (service->type() == transition_type) {
            Mlt::Transition t((mlt_transition)service->get_service());
            QString id(t.get("kdenlive_id"));
            QString internal(t.get("internal_added"));
            if (internal.isEmpty()) {
                compositions << new Mlt::Transition(t);
                if (id.isEmpty()) {
                    qDebug() << "// Warning, this should not happen, transition without id: " << t.get("id") << " = " << t.get("mlt_service");
                    t.set("kdenlive_id", t.get("mlt_service"));
                }
            }
        }
        service.reset(service->producer());
    }
    // Sort compositions and insert
    bool compositionOk = true;
    while (!compositions.isEmpty()) {
        QScopedPointer<Mlt::Transition> t(compositions.takeFirst());
        QString id(t->get("kdenlive_id"));
        int compoId;
        int aTrack = t->get_a_track();
        if (aTrack > tractor.count()) {
            m_errorMessage << i18n("Invalid composition %1 found on track %2 at %3, compositing with track %4.", t->get("id"), t->get_b_track(),
                                       t->get_in(), t->get_a_track());
            continue;
        }
        if (t->get_int("force_track") == 0) {
            // This is an automatic composition, check that we composite with lower track or warn
            int pos = videoTracksIndexes.indexOf(t->get_b_track());
            if (pos > 0 && videoTracksIndexes.at(pos - 1) != aTrack) {
                t->set("force_track", 1);
                m_errorMessage << i18n("Incorrect composition %1 found on track %2 at %3, compositing with track %4 was set to forced track.", t->get("id"), t->get_b_track(),
                                    t->get_in(), t->get_a_track());
            }
        }
        auto transProps = std::make_unique<Mlt::Properties>(t->get_properties());
        compositionOk = timeline->requestCompositionInsertion(id, timeline->getTrackIndexFromPosition(t->get_b_track() - 1), t->get_a_track(), t->get_in(), t->get_length(), std::move(transProps), compoId, undo, redo, false, originalDecimalPoint);
        if (!compositionOk) {
            qDebug() << "ERROR : failed to insert composition in track " << t->get_b_track() << ", position" << t->get_in() << ", ID: " << id
                         << ", MLT ID: " << t->get("id");
            // timeline->requestItemDeletion(compoId, false);
            m_errorMessage << i18n("Invalid composition %1 found on track %2 at %3.", t->get("id"), t->get_b_track(), t->get_in());
            continue;
        }
        qDebug() << "Inserted composition in track " << t->get_b_track() << ", position" << t->get_in() << "/" << t->get_out();
    }

    // build internal track compositing
    timeline->buildTrackCompositing();

    // load locked state as last step
    for (int tid : qAsConst(lockedTracksIndexes)) {
        timeline->setTrackLockedState(tid, true);
    }

    if (!ok) {
        // TODO log error
        // Don't abort loading because of failed composition
        undo();
        return false;
    }
    if (!m_errorMessage.isEmpty()) {
        KMessageBox::sorry(qApp->activeWindow(), m_errorMessage.join("\n"), i18n("Problems found in your project file"));
    }
    return true;
}

bool constructTrackFromMelt(const std::shared_ptr<TimelineItemModel> &timeline, int tid, Mlt::Tractor &track,
                            const std::unordered_map<QString, QString> &binIdCorresp, Fun &undo, Fun &redo, bool audioTrack, QString originalDecimalPoint, QProgressDialog *progressDialog)
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
        constructTrackFromMelt(timeline, tid, playlist, binIdCorresp, undo, redo, audioTrack, originalDecimalPoint, progressDialog);
        if (i == 0) {
            // Pass track properties
            int height = track.get_int("kdenlive:trackheight");
            timeline->setTrackProperty(tid, "kdenlive:trackheight", height == 0 ? "100" : QString::number(height));
            timeline->setTrackProperty(tid, "kdenlive:collapsed", QString::number(track.get_int("kdenlive:collapsed")));
            QString trackName = track.get("kdenlive:track_name");
            if (!trackName.isEmpty()) {
                timeline->setTrackProperty(tid, QStringLiteral("kdenlive:track_name"), trackName.toUtf8().constData());
            }
            if (audioTrack) {
                // This is an audio track
                timeline->setTrackProperty(tid, QStringLiteral("kdenlive:audio_track"), QStringLiteral("1"));
                timeline->setTrackProperty(tid, QStringLiteral("hide"), QStringLiteral("1"));
            } else {
                // video track, hide audio
                timeline->setTrackProperty(tid, QStringLiteral("hide"), QStringLiteral("2"));
            }
            int muteState = playlist.get_int("hide");
            if (muteState > 0 && (!audioTrack || (audioTrack && muteState != 1))) {
                timeline->setTrackProperty(tid, QStringLiteral("hide"), QString::number(muteState));
            }
        }
    }
    std::shared_ptr<Mlt::Service> serv = std::make_shared<Mlt::Service>(track.get_service());
    timeline->importTrackEffects(tid, serv);
    return true;
}

namespace {

// This function tries to recover the state of the producer (audio or video or both)
PlaylistState::ClipState inferState(const std::shared_ptr<Mlt::Producer> &prod, bool audioTrack)
{
    auto getProperty = [prod](const QString &name) {
        if (prod->parent().is_valid()) {
            return QString::fromUtf8(prod->parent().get(name.toUtf8().constData()));
        }
        return QString::fromUtf8(prod->get(name.toUtf8().constData()));
    };
    auto getIntProperty = [prod](const QString &name) {
        if (prod->parent().is_valid()) {
            return prod->parent().get_int(name.toUtf8().constData());
        }
        return prod->get_int(name.toUtf8().constData());
    };
    QString service = getProperty("mlt_service");
    std::pair<bool, bool> VidAud{true, true};
    VidAud.first = getIntProperty("set.test_image") == 0;
    VidAud.second = getIntProperty("set.test_audio") == 0;
    if (audioTrack || ((service.contains(QStringLiteral("avformat")) && getIntProperty(QStringLiteral("video_index")) == -1))) {
        VidAud.first = false;
    }
    if (!audioTrack || ((service.contains(QStringLiteral("avformat")) && getIntProperty(QStringLiteral("audio_index")) == -1))) {
        VidAud.second = false;
    }
    return stateFromBool(VidAud);
}
} // namespace

bool constructTrackFromMelt(const std::shared_ptr<TimelineItemModel> &timeline, int tid, Mlt::Playlist &track,
                            const std::unordered_map<QString, QString> &binIdCorresp, Fun &undo, Fun &redo, bool audioTrack, QString originalDecimalPoint, QProgressDialog *progressDialog)
{
    int max = track.count();
    for (int i = 0; i < max; i++) {
        if (track.is_blank(i)) {
            continue;
        }
        if (progressDialog) {
            progressDialog->setValue(progressDialog->value() + 1);
        } else {
            emit pCore->loadingMessageUpdated(QString(), 1);
        }
        std::shared_ptr<Mlt::Producer> clip(track.get_clip(i));
        int position = track.clip_start(i);
        switch (clip->type()) {
        case unknown_type:
        case producer_type: {
            qDebug() << "Looking for clip with ID " << clip->parent().get("kdenlive:id") << " and name " << clip->parent().get("kdenlive:clipname");
            QString binId;
            if (clip->parent().get_int("_kdenlive_processed") == 1) {
                // This is a bin clip, already processed no need to change id
                binId = QString(clip->parent().get("kdenlive:id"));
            } else {
                QString clipId = clip->parent().get("kdenlive:id");
                if (clipId.startsWith(QStringLiteral("slowmotion"))) {
                    clipId = clipId.section(QLatin1Char(':'), 1, 1);
                }
                if (clipId.isEmpty()) {
                    clipId = clip->get("kdenlive:id");
                }
                if (binIdCorresp.count(clipId) == 0) {
                    // Project was somehow corrupted
                    qDebug()<<"=== WARNING, CANNOT FIND CLIP WITH ID: "<<clipId<<" IN BIN PLAYLIST";
                    QStringList fixedId = pCore->projectItemModel()->getClipByUrl(QFileInfo(clip->parent().get("resource")));
                    if (!fixedId.isEmpty()) {
                        binId = fixedId.first();
                        m_errorMessage << i18n("Invalid clip %1 (%2) not found in project bin, recovered.", clip->parent().get("id"), clipId);
                    } else {
                        qWarning()<<"Warning, clip in timeline has no parent in bin: "<<clip->parent().get("id");
                        m_errorMessage << i18n("Project corrupted. Clip %1 (%2) not found in project bin.", clip->parent().get("id"), clipId);
                    }
                } else {
                    binId = binIdCorresp.at(clipId);
                }
                Q_ASSERT(!clipId.isEmpty() && !binId.isEmpty());
                clip->parent().set("kdenlive:id", binId.toUtf8().constData());
                clip->parent().set("_kdenlive_processed", 1);
            }
            bool ok = false;
            int cid = -1;
            if (pCore->bin()->getBinClip(binId)) {
                PlaylistState::ClipState st = inferState(clip, audioTrack);
                cid = ClipModel::construct(timeline, binId, clip, st, tid, originalDecimalPoint);
                ok = timeline->requestClipMove(cid, tid, position, true, true, false, true, undo, redo);
            } else {
                qDebug() << "// Cannot find bin clip: " << binId << " - " << clip->get("id");
            }
            if (!ok && cid > -1) {
                qDebug() << "ERROR : failed to insert clip in track" << tid << "position" << position;
                timeline->requestItemDeletion(cid, false);
                m_errorMessage << i18n("Invalid clip %1 found on track %2 at %3.", clip->parent().get("id"), track.get("id"), position);
                break;
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
    std::shared_ptr<Mlt::Service> serv = std::make_shared<Mlt::Service>(track.get_service());
    timeline->importTrackEffects(tid, serv);
    return true;
}
