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
