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
