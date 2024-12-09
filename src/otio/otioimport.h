/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>

#include <opentimelineio/timeline.h>

class TimelineItemModel;

class OtioImport : public QObject
{
    Q_OBJECT

public:
    OtioImport(QObject *parent);

public Q_SLOTS:
    void slotImport();

private:
    void importTimeline(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> &, std::shared_ptr<TimelineItemModel>);
};
