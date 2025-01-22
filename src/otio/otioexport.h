/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>

#include <opentimelineio/clip.h>
#include <opentimelineio/timeline.h>
#include <opentimelineio/track.h>

class CommentedTime;
class TimelineItemModel;
class TrackModel;

/** @brief This class provides support for exporting OpenTimelineIO files.
 *
 * General notes for OTIO export and import:
 *
 * Guides are converted to OTIO markers on the OTIO timeline stack.
 *
 * Clip markers are converted to OTIO markers on the OTIO clips. Note that clip
 * markers work differently between kdenlive and OTIO; in kdenlive they are
 * shared between each instance of the clip, in OTIO the they are unique to
 * each instance of the clip.
 *
 * Some kdenlive data is stored as OTIO metadata for round-tripping files. For
 * example the kdenlive marker type is stored in the OTIO marker metadata so
 * that re-importing the file will properly recreate the guides and clip
 * markers. If the marker metadata is missing we try and pick the kdenlive
 * guide/marker with the closest color to the OTIO marker. The metadata is
 * stored under the key "kdenlive" to keep it separate from other application's
 * metadata.
 */
class OtioExport : public QObject
{
    Q_OBJECT

public:
    OtioExport(QObject *parent);

public Q_SLOTS:
    void slotExport();

private:
    void exportTimeline(const std::shared_ptr<TimelineItemModel> &, const QString &path);
    void exportTrack(const std::shared_ptr<TimelineItemModel> &, int trackId, const std::shared_ptr<TrackModel> &,
                     OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> &);
    void exportClip(const std::shared_ptr<TimelineItemModel> &, int clipId, OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> &);
    void exportMarker(const CommentedTime &, const OTIO_NS::TimeRange &, const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Item> &);

    double projectFps() const;
};
