/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "otioimport.h"

#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "mainwindow.h"
#include "project/projectmanager.h"

#include <KLocalizedString>

#include <QFileDialog>

OtioImport::OtioImport(QObject *parent)
    : QObject(parent)
{
}

void OtioImport::slotImport()
{
    // Get the file name.
    const QString importFile =
        QFileDialog::getOpenFileName(pCore->window(), i18n("OpenTimelineIO Import"), pCore->currentDoc()->projectDataFolder(), i18n("*.otio"));
    if (importFile.isNull() || !QFile::exists(importFile)) {
        // TODO: Error handling?
        return;
    }

    // Open the OTIO timeline.
    OTIO_NS::ErrorStatus otioError;
    OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTimeline(
        dynamic_cast<OTIO_NS::Timeline *>(OTIO_NS::Timeline::from_json_file(importFile.toStdString(), &otioError)));
    if (!otioTimeline || OTIO_NS::is_error(otioError)) {
        // TODO: Error handling?
        return;
    }

    // Create a timeline model.
    const QUuid uuid = QUuid::createUuid();
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(uuid, pCore->projectManager()->undoStack());

    // Import the timeline.
    importTimeline(otioTimeline, timeline);

    // TODO: Is this the correct way to add the timeline?
    pCore->projectManager()->current()->addTimeline(uuid, timeline);

    // TODO: Add the bin clip?
    QString binId;
    QDomElement domElement;
    domElement.setAttribute("type", ClipType::ProducerType::Timeline);
    pCore->projectItemModel()->requestAddBinClip(binId, domElement, pCore->projectItemModel()->getRootFolder()->clipId());

    // TODO: Set the marker model?
    std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(binId);
    timeline->setMarkerModel(clip->markerModel());

    // TODO: Open the timeline to the main window?
    pCore->window()->openTimeline(uuid, -1, QFileInfo(importFile).baseName(), timeline);
}

void OtioImport::importTimeline(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> &otioTimeline, std::shared_ptr<TimelineItemModel> timeline)
{
    // Import the tracks.
    //
    // TODO: Tracks are in reverse order?
    auto otioChildren = otioTimeline->tracks()->children();
    for (auto i = otioChildren.rbegin(); i != otioChildren.rend(); ++i) {
        if (auto track = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Track>(*i)) {
            int trackId = 0;
            timeline->requestTrackInsertion(-1, trackId, QString::fromStdString(track->name()), OTIO_NS::Track::Kind::audio == track->kind());
        }
    }
}