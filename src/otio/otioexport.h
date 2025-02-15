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
 * Notes about exporting OpenTimelineIO files:
 *
 * Guides are converted to OTIO markers on the timeline stack.
 *
 * Clip markers are converted to OTIO markers. Note that clip markers work
 * differently between kdenlive and OTIO; in kdenlive they are shared
 * between each instance of the clip, in OTIO the they are unique to each
 * instance of the clip.
 *
 * The kdenlive marker types are stored as OTIO metadata for round-tripping
 * files. This allows the guides and clip markers to be properly recreated
 * when importing an OTIO file that was created with kdenlive. The metadata
 * is stored under the key "kdenlive" to keep it separate from metadata of
 * other applications.
 */
class OtioExport : public QObject
{
    Q_OBJECT

public:
    OtioExport(QObject *parent = nullptr);

    /** @brief Export an OTIO file. */
    void exportFile(const QString &fileName);

public Q_SLOTS:
    /** @brief Open a file dialog and export an OTIO file. */
    void slotExport();

private:
    void exportTimeline(const std::shared_ptr<TimelineItemModel> &, const QString &path);
    void exportTrack(const std::shared_ptr<TimelineItemModel> &, int trackId, const std::shared_ptr<TrackModel> &,
                     OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> &);
    void exportClip(const std::shared_ptr<TimelineItemModel> &, int clipId, OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> &);
    void exportMarker(const CommentedTime &, const OTIO_NS::TimeRange &, const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Item> &);

    double projectFps() const;
};
