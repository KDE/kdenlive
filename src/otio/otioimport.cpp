/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "otioimport.h"

#include "bin/clipcreator.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "mainwindow.h"
#include "project/projectmanager.h"

#include <KLocalizedString>

#include <QFileDialog>

#include <opentimelineio/clip.h>
#include <opentimelineio/externalReference.h>

OtioImport::OtioImport(QObject *parent)
    : QObject(parent)
{
}

void OtioImport::slotImport()
{
    // Get the file name.
    const QString importFile = QFileDialog::getOpenFileName(pCore->window(), i18n("OpenTimelineIO Import"), pCore->currentDoc()->projectDataFolder(),
                                                            QStringLiteral("%1 (*.otio)").arg(i18n("OpenTimelineIO Project")));
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

    // Import the media and timeline.
    importMedia(otioTimeline);
    importTimeline(otioTimeline, timeline);

    // TODO: Is this the correct way to add the timeline?
    pCore->projectManager()->current()->addTimeline(uuid, timeline);

    // TODO: Add the bin clip?
    QString binId;
    QDomElement domElement;
    domElement.setAttribute("type", ClipType::Timeline);
    pCore->projectItemModel()->requestAddBinClip(binId, domElement, pCore->projectItemModel()->getRootFolder()->clipId());

    // TODO: Set the marker model?
    std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(binId);
    timeline->setMarkerModel(clip->markerModel());

    // TODO: Open the timeline to the main window?
    pCore->window()->openTimeline(uuid, -1, QFileInfo(importFile).baseName(), timeline);
}

void OtioImport::importMedia(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> &otioTimeline)
{
    // Find all of the OTIO media references and add them
    // to the bin.
    m_otioExternalReferencesToBinIds.clear();
    for (const auto &otioClip : otioTimeline->find_clips()) {
        if (auto otioExternalReference = dynamic_cast<OTIO_NS::ExternalReference *>(otioClip->media_reference())) {
            const QString url = QString::fromStdString(otioExternalReference->target_url());
            const auto i = m_otioExternalReferencesToBinIds.find(url);
            if (i == m_otioExternalReferencesToBinIds.end()) {
                Fun undo = []() { return true; };
                Fun redo = []() { return true; };
                const QString clipId =
                    ClipCreator::createClipFromFile(url, pCore->projectItemModel()->getRootFolder()->clipId(), pCore->projectItemModel(), undo, redo);
                m_otioExternalReferencesToBinIds[url] = clipId;
            }
        }
    }
}

void OtioImport::importTimeline(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> &otioTimeline, std::shared_ptr<TimelineItemModel> timeline)
{
    // Import the tracks.
    //
    // TODO: Tracks are in reverse order?
    auto otioChildren = otioTimeline->tracks()->children();
    for (auto i = otioChildren.rbegin(); i != otioChildren.rend(); ++i) {
        if (auto otioTrack = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Track>(*i)) {
            int trackId = 0;
            timeline->requestTrackInsertion(-1, trackId, QString::fromStdString(otioTrack->name()), OTIO_NS::Track::Kind::audio == otioTrack->kind());
            importTrack(otioTrack, timeline, trackId);
        }
    }
}

void OtioImport::importTrack(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> &otioTrack, std::shared_ptr<TimelineItemModel> timeline, int trackId)
{
    auto otioChildren = otioTrack->children();
    for (auto i = otioChildren.rbegin(); i != otioChildren.rend(); ++i) {
        if (auto otioClip = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Clip>(*i)) {
            importClip(otioClip, timeline, trackId);
        }
    }
}

void OtioImport::importClip(const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip> &otioClip, std::shared_ptr<TimelineItemModel> timeline, int trackId)
{
    if (auto otioExternalReference = dynamic_cast<OTIO_NS::ExternalReference *>(otioClip->media_reference())) {
        const QString url = QString::fromStdString(otioExternalReference->target_url());
        const auto i = m_otioExternalReferencesToBinIds.find(url);
        if (i != m_otioExternalReferencesToBinIds.end()) {
            // TODO: This gets stuck at:
            //
            // QReadWriteLock::lockForRead(class QReadWriteLock * const this) (/usr/include/x86_64-linux-gnu/qt6/QtCore/qreadwritelock.h:68)
            // QReadLocker::relock(class QReadLocker * const this) (/usr/include/x86_64-linux-gnu/qt6/QtCore/qreadwritelock.h:115)
            // QReadLocker::QReadLocker(class QReadLocker * const this, class QReadWriteLock * areadWriteLock)
            // (/usr/include/x86_64-linux-gnu/qt6/QtCore/qreadwritelock.h:134) ClipController::getProducerIntProperty(const class ClipController * const this,
            // const class QString & name) (kdenlive/src/mltcontroller/clipcontroller.cpp:595) TimelineModel::requestClipInsertion(class TimelineModel * const
            // this, const class QString & binClipId, int trackId, int position, int & id, bool logUndo, bool refreshView, bool useTargets, Fun & undo, Fun &
            // redo, const QVector & allowedTracks) (kdenlive/src/timeline2/model/timelinemodel.cpp:2120) TimelineModel::requestClipInsertion(class
            // TimelineModel * const this, const class QString & binClipId, int trackId, int position, int & id, bool logUndo, bool refreshView, bool
            // useTargets) (kdenlive/src/timeline2/model/timelinemodel.cpp:1844) OtioImport::importClip(class OtioImport * const this, const struct
            // opentimelineio::v1_0::SerializableObject::Retainer<opentimelineio::v1_0::Clip> & otioClip, class std::shared_ptr<TimelineItemModel> timeline, int
            // trackId) (kdenlive/src/otio/otioimport.cpp:133)
            int position = 0;
            int clipId = 0;
            timeline->requestClipInsertion(i.value(), trackId, position, clipId, false, false);
        }
    }
}
