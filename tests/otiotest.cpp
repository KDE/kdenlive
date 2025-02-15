/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2025 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"

#include "otio/otioexport.h"
#include "otio/otioimport.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"

#include <opentimelineio/externalReference.h>

#include <QDir>
#include <QTemporaryDir>

using namespace fakeit;

TEST_CASE("Export/import tracks and clips", "[OTIO]")
{
    SECTION("Single clip")
    {
        // Create the test document and timeline.
        auto binModel = pCore->projectItemModel();
        std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
        KdenliveDoc document(undoStack);
        pCore->projectManager()->testSetDocument(&document);
        QDateTime documentDate = QDateTime::currentDateTime();
        KdenliveTests::updateTimeline(false, QString(), QString(), documentDate, 0);
        auto timeline = document.getTimeline(document.uuid());
        pCore->projectManager()->testSetActiveTimeline(timeline);

        // Create an OTIO timeline.
        /*OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTimeline(
            new OTIO_NS::Timeline);
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Track> otioTrack(
            new OTIO_NS::Track);
        otioTimeline->tracks()->append_child(otioTrack);
        QString path = sourcesPath + "/dataset/red.mp4";
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::ExternalReference> otioMediaReference(
            new OTIO_NS::ExternalReference(path.toStdString()));
        OTIO_NS::TimeRange sourceRange(
            OTIO_NS::RationalTime(0.0, 25.0),
            OTIO_NS::RationalTime(5.0, 25.0));
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip> otioClip(
            new OTIO_NS::Clip(
                "red",
                otioMediaReference,
                sourceRange));
        otioTrack->append_child(otioClip);
        // TODO: Replace with QTemporaryDir.
        QDir dir = QDir::temp();
        QString otioImportFileName = dir.filePath("import_single_clip.otio");
        REQUIRE(otioTimeline->to_json_file(otioImportFileName.toStdString()));*/

        // Import the OTIO timeline.
        OtioImport otioImport;
        QString path = sourcesPath + "/dataset/test-clips.otio";
        otioImport.importFile(path, false);

        // Export to an OTIO timeline.
        OtioExport otioExport;
        // TODO: Replace with QTemporaryDir.
        QDir dir = QDir::temp();
        path = dir.filePath("test-clips.otio");
        otioExport.exportFile(path);

        // TODO: Compare timelines.

        pCore->projectManager()->closeCurrentDocument(false, false);
    }
}
