/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "otioexport.h"

#include "bin/model/markerlistmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "mainwindow.h"
#include "project/projectmanager.h"
#include "timeline2/model/clipmodel.hpp"

#include <KLocalizedString>

#include <QFileDialog>

#include <opentimelineio/externalReference.h>
#include <opentimelineio/gap.h>
#include <opentimelineio/marker.h>
#include <opentimelineio/stack.h>

OtioExport::OtioExport(QObject *parent)
    : QObject(parent)
{
}

void OtioExport::slotExport()
{
    // Get the file name.
    const QString fileName = QFileDialog::getSaveFileName(pCore->window(), i18n("OpenTimelineIO Export"), pCore->currentDoc()->projectDataFolder(),
                                                          QStringLiteral("%1 (*.otio)").arg(i18n("OpenTimelineIO Project")));
    if (fileName.isNull()) {
        return;
    }

    // Export the timeline.
    exportTimeline(pCore->currentDoc()->getTimeline(pCore->currentTimelineId()), fileName);
}

void OtioExport::exportTimeline(const std::shared_ptr<TimelineItemModel> &timeline, const QString &fileName)
{
    // Create the OTIO timeline.
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTimeline(new OTIO_NS::Timeline);

    // Export the tracks.
    const int tracksCount = timeline->getTracksCount();
    for (int index = tracksCount - 1; index >= 0; --index) {
        const int trackId = timeline->getTrackIndexFromPosition(index);
        std::shared_ptr<TrackModel> track = timeline->getTrackById(trackId);
        exportTrack(timeline, trackId, track, otioTimeline);
    }

    // Write the OTIO timeline to disk.
    otioTimeline->to_json_file(fileName.toStdString());
}

void OtioExport::exportTrack(const std::shared_ptr<TimelineItemModel> &timeline, int trackId, const std::shared_ptr<TrackModel> &track,
                             OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> &otioTimeline)
{
    // Create the OTIO track.
    std::string trackKind = OTIO_NS::Track::Kind::video;
    if (timeline->isAudioTrack(trackId)) {
        trackKind = OTIO_NS::Track::Kind::audio;
    } else if (timeline->isSubtitleTrack(trackId)) {
        trackKind = "Subtitle";
    }
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> otioTrack(
        new OTIO_NS::Track(timeline->getTrackFullName(trackId).toStdString(), std::nullopt, trackKind));
    otioTimeline->tracks()->append_child(otioTrack);

    // Sort the clips by their position in the timeline.
    QMap<int, int> clipPositionToId;
    for (int clip = 0; clip < track->getClipsCount(); ++clip) {
        const int clipId = track->getClipByRow(clip);
        clipPositionToId[timeline->getItemPosition(clipId)] = clipId;
    }

    // Export the clips.
    int position = 0;
    const double fps = projectFps();
    for (int clipId : clipPositionToId) {

        const int clipPosition = clipPositionToId.key(clipId);
        if (clipPosition != position) {
            // OTIO explicitly represents gaps.
            const int duration = clipPosition - position;
            OTIO_NS::SerializableObject::Retainer<OTIO_NS::Gap> otioGap(
                new OTIO_NS::Gap(OTIO_NS::TimeRange(OTIO_NS::RationalTime(position, fps), OTIO_NS::RationalTime(duration, fps))));
            otioTrack->append_child(otioGap);
            position += duration;
        }

        exportClip(timeline, clipId, otioTrack);
        position += timeline->getItemPlaytime(clipId);
    }
}

void OtioExport::exportClip(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> &otioTrack)
{
    // Create the OTIO media reference.
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::MediaReference> otioMediaReference;
    const QString clipBinId = timeline->getClipBinId(clipId);
    if (std::shared_ptr<ProjectClip> projectClip = pCore->projectItemModel()->getClipByBinID(clipBinId)) {
        if (projectClip->hasUrl()) {
            const double mediaFps = projectClip->getOriginalFps();
            otioMediaReference = new OTIO_NS::ExternalReference(
                projectClip->url().toStdString(),
                OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, mediaFps), OTIO_NS::RationalTime(projectClip->frameDuration(), mediaFps)));
        }
    }

    // Create the OTIO clip.
    const std::pair<int, int> clipInOut = timeline->getClipInOut(clipId);
    const double fps = projectFps();
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip> otioClip(
        new OTIO_NS::Clip(timeline->getClipName(clipId).toStdString(), otioMediaReference.value,
                          OTIO_NS::TimeRange(OTIO_NS::RationalTime(clipInOut.first, fps), OTIO_NS::RationalTime(clipInOut.second - clipInOut.first + 1, fps))));
    otioTrack->append_child(otioClip);

    // Create the markers.
    if (std::shared_ptr<ClipModel> clipModel = timeline->getClipPtr(clipId)) {
        if (std::shared_ptr<MarkerListModel> markerModel = clipModel->getMarkerModel()) {
            for (const CommentedTime &marker : markerModel->getAllMarkers()) {
                OTIO_NS::SerializableObject::Retainer<OTIO_NS::Marker> otioMarker(new OTIO_NS::Marker);
                otioMarker->set_comment(marker.comment().toStdString());
                const int position = marker.time().frames(fps) - clipInOut.first;
                otioMarker->set_marked_range(OTIO_NS::TimeRange(OTIO_NS::RationalTime(position, fps), OTIO_NS::RationalTime(1, fps)));
                auto it = pCore->markerTypes.find(marker.markerType());
                if (it != pCore->markerTypes.end()) {
                    otioMarker->set_color(it->color.name().toStdString());
                }
                otioClip->markers().push_back(otioMarker);
            }
        }
    }
}

double OtioExport::projectFps() const
{
    const std::pair<int, int> fps = pCore->getProjectFpsInfo();
    return fps.second > 0 ? (fps.first / static_cast<double>(fps.second)) : 24.0;
}
