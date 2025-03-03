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
#include "otioutil.h"
#include "project/projectmanager.h"
#include "timeline2/model/clipmodel.hpp"

#include <KLocalizedString>
#include <KMessageBox>

#include <QApplication>
#include <QFileDialog>

#include <opentimelineio/externalReference.h>
#include <opentimelineio/gap.h>
#include <opentimelineio/marker.h>
#include <opentimelineio/stack.h>
#include <opentimelineio/transition.h>

OtioExport::OtioExport(QObject *parent)
    : QObject(parent)
{
}

void OtioExport::exportFile(const QString &fileName)
{
    exportTimeline(pCore->currentDoc()->getTimeline(pCore->currentTimelineId()), fileName);
}

void OtioExport::slotExport()
{
    const QString fileName = QFileDialog::getSaveFileName(pCore->window(), i18n("OpenTimelineIO Export"), pCore->currentDoc()->projectDataFolder(),
                                                          QStringLiteral("%1 (*.otio)").arg(i18n("OpenTimelineIO Project")));
    if (fileName.isNull()) {
        return;
    }
    exportFile(fileName);
}

void OtioExport::exportTimeline(const std::shared_ptr<TimelineItemModel> &timeline, const QString &fileName)
{
    // Create the OTIO timeline.
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTimeline(new OTIO_NS::Timeline);

    // Export guides as OTIO markers.
    const double fps = projectFps();
    for (const CommentedTime &marker : timeline->getGuideModel()->getAllMarkers()) {
        const int position = marker.time().frames(fps);
        exportMarker(marker, OTIO_NS::TimeRange(OTIO_NS::RationalTime(position, fps), OTIO_NS::RationalTime(1, fps)), otioTimeline->tracks());
    }

    // Export the tracks.
    const int tracksCount = timeline->getTracksCount();
    for (int index = tracksCount - 1; index >= 0; --index) {
        const int trackId = timeline->getTrackIndexFromPosition(index);
        std::shared_ptr<TrackModel> track = timeline->getTrackById(trackId);
        exportTrack(timeline, trackId, track, otioTimeline);
    }

    // Write the OTIO timeline to disk.
    OTIO_NS::ErrorStatus otioError;
    bool r = otioTimeline->to_json_file(fileName.toStdString());
    if (!r || OTIO_NS::is_error(otioError)) {
        if (pCore->window()) {
            KMessageBox::error(qApp->activeWindow(), QString::fromStdString(otioError.details), i18n("Error exporting OpenTimelineIO file"));
        } else {
            qWarning() << "Error exporting OpenTimelineIO file:" << QString::fromStdString(otioError.details);
        }
        return;
    }
}

void OtioExport::exportTrack(const std::shared_ptr<TimelineItemModel> &timeline, int trackId, const std::shared_ptr<TrackModel> &track,
                             OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> &otioTimeline)
{
    // Create the OTIO track.
    const double fps = projectFps();
    std::string trackKind = OTIO_NS::Track::Kind::video;
    if (timeline->isAudioTrack(trackId)) {
        trackKind = OTIO_NS::Track::Kind::audio;
    } else if (timeline->isSubtitleTrack(trackId)) {
        trackKind = "Subtitle";
    }
    const QString trackName = track->getProperty(QStringLiteral("kdenlive:track_name")).toString();
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> otioTrack(new OTIO_NS::Track(
        trackName.toStdString(), OTIO_NS::TimeRange(OTIO_NS::RationalTime(0.0, fps), OTIO_NS::RationalTime(track->trackDuration(), fps)), trackKind));
    otioTimeline->tracks()->append_child(otioTrack);

    // Order clips by their position. If there are any mixes, we adjust the
    // clip in and out points to remove overlaps.
    std::vector<ClipData> orderedClips;
    for (int i = 0; i < track->getClipsCount(); ++i) {
        const int clipId = track->getClipByRow(i);
        int clipPos = timeline->getClipPosition(clipId);
        std::pair<int, int> clipInOut = timeline->getClipInOut(clipId);
        const auto mixInfo = track->getMixInfo(clipId);
        if (mixInfo.first.firstClipId != -1 && mixInfo.first.secondClipId != -1) {
            auto clipModel = timeline->getClipPtr(clipId);
            const int offset = clipModel->getMixDuration() - clipModel->getMixCutPosition();
            clipPos += offset;
            clipInOut.first += offset;
        }
        if (mixInfo.second.firstClipId != -1 && mixInfo.second.secondClipId != -1) {
            auto clipModel = timeline->getClipPtr(mixInfo.second.secondClipId);
            clipInOut.second -= clipModel->getMixCutPosition();
        }
        orderedClips.push_back({clipId, clipPos, clipInOut.first, clipInOut.second});
    }
    std::sort(orderedClips.begin(), orderedClips.end(), [](const ClipData &a, const ClipData &b) { return a.pos < b.pos; });

    // Export clips, gaps, and transitions.
    for (const auto &clip : orderedClips) {

        // OTIO explicitly represents gaps.
        const int blankSize = track->getBlankSizeNearClip(clip.id, false);
        if (blankSize > 0) {
            OTIO_NS::SerializableObject::Retainer<OTIO_NS::Gap> otioGap(
                new OTIO_NS::Gap(OTIO_NS::TimeRange(OTIO_NS::RationalTime(0.0, fps), OTIO_NS::RationalTime(blankSize, fps))));
            otioTrack->append_child(otioGap);
        }

        // Convert mixes to OTIO transitions.
        const auto mixInfo = track->getMixInfo(clip.id);
        if (mixInfo.first.firstClipId != -1 && mixInfo.first.secondClipId != -1) {
            auto clipModel = timeline->getClipPtr(clip.id);
            OTIO_NS::SerializableObject::Retainer<OTIO_NS::Transition> otioTransition(new OTIO_NS::Transition(
                "Transition", OTIO_NS::Transition::Type::SMPTE_Dissolve,
                OTIO_NS::RationalTime(clipModel->getMixDuration() - clipModel->getMixCutPosition(), fps), OTIO_NS::RationalTime(mixInfo.first.mixOffset, fps)));
            otioTrack->append_child(otioTransition);
        }

        exportClip(timeline, clip, otioTrack);
    }
}

void OtioExport::exportClip(const std::shared_ptr<TimelineItemModel> &timeline, const ClipData &clip,
                            OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> &otioTrack)
{
    // Create the OTIO media reference.
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::MediaReference> otioMediaReference;
    const QString clipBinId = timeline->getClipBinId(clip.id);
    if (std::shared_ptr<ProjectClip> projectClip = pCore->projectItemModel()->getClipByBinID(clipBinId)) {
        if (projectClip->hasUrl()) {
            const double mediaFps = projectClip->getOriginalFps();
            otioMediaReference = new OTIO_NS::ExternalReference(
                projectClip->url().toStdString(),
                OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, mediaFps), OTIO_NS::RationalTime(projectClip->frameDuration(), mediaFps)));
        }
    }

    // Create the OTIO clip.
    auto clipModel = timeline->getClipPtr(clip.id);
    const double fps = projectFps();
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip> otioClip(
        new OTIO_NS::Clip(timeline->getClipName(clip.id).toStdString(), otioMediaReference.value,
                          OTIO_NS::TimeRange(OTIO_NS::RationalTime(clip.in, fps), OTIO_NS::RationalTime(clip.out - clip.in + 1, fps))));
    otioTrack->append_child(otioClip);

    // Create the OTIO markers.
    if (std::shared_ptr<ClipModel> clipModel = timeline->getClipPtr(clip.id)) {
        if (std::shared_ptr<MarkerListModel> markerModel = clipModel->getMarkerModel()) {
            for (const CommentedTime &marker : markerModel->getAllMarkers()) {

                const int position = marker.time().frames(fps) - clip.in;
                exportMarker(marker, OTIO_NS::TimeRange(OTIO_NS::RationalTime(position, fps), OTIO_NS::RationalTime(1, fps)),
                             OTIO_NS::dynamic_retainer_cast<OTIO_NS::Item>(otioClip));
            }
        }
    }
}

void OtioExport::exportMarker(const CommentedTime &marker, const OTIO_NS::TimeRange &otioRange,
                              const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Item> &otioItem)
{
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::Marker> otioMarker(new OTIO_NS::Marker);
    otioMarker->set_comment(marker.comment().toStdString());
    otioMarker->set_marked_range(otioRange);
    otioMarker->set_color(toOtioMarkerColor(marker.markerType()));
    OTIO_NS::AnyDictionary otioMetadata;
    otioMetadata["type"] = static_cast<int64_t>(marker.markerType());
    otioMarker->metadata()["kdenlive"] = otioMetadata;
    otioItem->markers().push_back(otioMarker);
}

double OtioExport::projectFps() const
{
    const std::pair<int, int> fps = pCore->getProjectFpsInfo();
    return fps.second > 0 ? (fps.first / static_cast<double>(fps.second)) : 24.0;
}
