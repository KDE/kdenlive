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
 *
 * Notes about importing OpenTimelineIO files:
 *
 * OTIO files do not contain information about rendering, so we get the
 * render resolution from the first video clip.
 *
 * OTIO markers on the timeline stack are converted to kdenlive guides.
 *
 * OTIO markers on clips are converted to kdenlive clip markers. Note that
 * clip markers work differently between kdenlive and OTIO; in kdenlive they
 * are shared between each instance of the clip, in OTIO the they are unique
 * to each instance of the clip.
 *
 * If the OTIO marker metadata does not contain the kdenlive marker type,
 * we pick the kdenlive guide/marker with the closest color to the OTIO
 * marker.
 */
class OtioImport : public QObject
{
    Q_OBJECT

public:
    OtioImport(QObject *parent = nullptr);

    /** @brief Import an OTIO file. */
    void importFile(const QString &fileName, bool newDocument = true);

public Q_SLOTS:
    /** @brief Open a file dialog and import the chosen OTIO file. */
    void slotImport();

private:
    void importTimeline(const std::shared_ptr<OtioImportData> &);
    void importTrack(const std::shared_ptr<OtioImportData> &, const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> &, int trackId);
    int importClip(const std::shared_ptr<OtioImportData> &, const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip> &, int trackId);
    void importMarker(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Marker> &, const GenTime &, const std::shared_ptr<MarkerListModel> &);

    static QString resolveFile(const QString &, const QFileInfo &timelineFileInfo);
};
