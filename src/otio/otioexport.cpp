/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "otioexport.h"

#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "mainwindow.h"
#include "project/projectmanager.h"

#include <KLocalizedString>

#include <QFileDialog>

#include <opentimelineio/externalReference.h>
#include <opentimelineio/stack.h>

OtioExport::OtioExport(QObject *parent)
    : QObject(parent)
{
}

void OtioExport::slotExportProject()
{
    const QString path = QFileDialog::getSaveFileName(pCore->window(), i18n("OpenTimelineIO Export"), pCore->currentDoc()->projectDataFolder(), i18n("*.otio"));
    if (path.isNull()) {
        return;
    }

    exportTimeline(pCore->currentDoc()->getTimeline(pCore->currentTimelineId()), path);
}

void OtioExport::exportTimeline(const std::shared_ptr<TimelineItemModel> &timeline, const QString &path)
{
    // Create the OTIO timeline.
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTimeline(new OTIO_NS::Timeline);

    // Export the tracks.
    //
    // TODO: Tracks are in reverse order?
    for (int track = timeline->rowCount() - 1; track >= 0; --track) {
        const QModelIndex trackModelIndex = timeline->index(track);
        exportTrack(timeline, trackModelIndex, otioTimeline);
    }

    // Write the OTIO timeline to disk.
    otioTimeline->to_json_file(path.toStdString());
}

void OtioExport::exportTrack(const std::shared_ptr<TimelineItemModel> &timeline, const QModelIndex &trackModelIndex,
                             OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> &otioTimeline)
{
    const int trackId = trackModelIndex.internalId();
    // qDebug() << "track:" << timeline->getTrackFullName(trackId);

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
    for (int clip = 0; clip < timeline->rowCount(trackModelIndex); ++clip) {
        const QModelIndex clipModelIndex = timeline->index(clip, 0, trackModelIndex);
        const int clipId = clipModelIndex.internalId();
        clipPositionToId[timeline->getItemPosition(clipId)] = clipId;
    }

    // Export the clips.
    for (int clipId : clipPositionToId) {
        exportClip(timeline, clipId, otioTrack);
    }
}

void OtioExport::exportClip(const std::shared_ptr<TimelineItemModel> &timeline, int clipId, OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> &otioTrack)
{
    // qDebug() << "clip:" << timeline->getClipName(clipId);

    // Create the OTIO media reference.
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::MediaReference> otioMediaReference;
    const QString clipBinId = timeline->getClipBinId(clipId);
    if (std::shared_ptr<ProjectClip> projectClip = pCore->projectItemModel()->getClipByBinID(clipBinId)) {
        if (projectClip->hasUrl()) {
            // qDebug() << "    url:" << projectClip->url();
            const double mediaFps = projectClip->getOriginalFps();
            otioMediaReference = new OTIO_NS::ExternalReference(
                projectClip->url().toStdString(),
                OTIO_NS::TimeRange(OTIO_NS::RationalTime(0, mediaFps), OTIO_NS::RationalTime(projectClip->frameDuration(), mediaFps)));
        }
    }

    // Create the OTIO clip.
    const std::pair<int, int> clipInOut = timeline->getClipInOut(clipId);
    // qDebug() << "    range:" << clipInOut.first << clipInOut.second;
    const double fps = projectFps();
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip> otioClip(
        new OTIO_NS::Clip(timeline->getClipName(clipId).toStdString(), otioMediaReference.value,
                          OTIO_NS::TimeRange(OTIO_NS::RationalTime(clipInOut.first, fps), OTIO_NS::RationalTime(clipInOut.second - clipInOut.first + 1, fps))));
    otioTrack->append_child(otioClip);
}

double OtioExport::projectFps() const
{
    const std::pair<int, int> fps = pCore->getProjectFpsInfo();
    return fps.second > 0 ? (fps.first / static_cast<double>(fps.second)) : 24.0;
}
