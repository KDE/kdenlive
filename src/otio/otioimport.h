/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QMap>
#include <QObject>

#include <opentimelineio/clip.h>
#include <opentimelineio/timeline.h>
#include <opentimelineio/track.h>

class TimelineItemModel;

class OtioImport : public QObject
{
    Q_OBJECT

public:
    OtioImport(QObject *parent);

public Q_SLOTS:
    void slotImport();

private:
    void importMedia(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> &);
    void importTimeline(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> &, std::shared_ptr<TimelineItemModel>);
    void importTrack(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> &, std::shared_ptr<TimelineItemModel>, int trackId);
    void importClip(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip> &, std::shared_ptr<TimelineItemModel>, int trackId);

    QMap<QString, QString> m_otioExternalReferencesToBinIds;
};
