/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QFileInfo>
#include <QMap>
#include <QObject>

#include <opentimelineio/clip.h>
#include <opentimelineio/marker.h>
#include <opentimelineio/timeline.h>
#include <opentimelineio/track.h>

#include <unordered_set>

class GenTime;
class MarkerListModel;
class TimelineItemModel;

/** @brief This struct provides data associated with importing OpenTimelineIO
 * files.
 */
struct OtioImportData
{
    QFileInfo otioFile;
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTimeline;
    std::shared_ptr<TimelineItemModel> timeline;
    std::unordered_set<int> defaultTracks;
    QMap<QString, QString> otioExternalRefToBinId;
    QMap<QString, QString> binIdToTimecode;
    int waitingBinIds = 0;
};

/** @brief This class provides support for importing OpenTimelineIO files.
 * See the comments in otioexport.h for notes about the import/export process.
 */
class OtioImport : public QObject
{
    Q_OBJECT

public:
    OtioImport(QObject *parent);

public Q_SLOTS:
    void slotImport();

private:
    void importTimeline(const std::shared_ptr<OtioImportData> &);
    void importTrack(const std::shared_ptr<OtioImportData> &, const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> &, int trackId);
    void importClip(const std::shared_ptr<OtioImportData> &, const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip> &, int trackId);
    void importMarker(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Marker> &, const GenTime &, const std::shared_ptr<MarkerListModel> &);

    static QString resolveFile(const QString &, const QFileInfo &timelineFileInfo);
};
