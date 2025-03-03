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

#include <QDir>
#include <QTemporaryDir>

#include <opentimelineio/externalReference.h>
#include <opentimelineio/gap.h>

using namespace fakeit;

TEST_CASE("Export/import", "[OTIO]")
{
    SECTION("Multiple clips and tracks")
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

        // Read the OTIO test timeline.
        QString inputPath = sourcesPath + "/dataset/test-clips.otio";
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTestTimeline(
            dynamic_cast<OTIO_NS::Timeline *>(OTIO_NS::Timeline::from_json_file(inputPath.toStdString())));
        const int otioClipCount = otioTestTimeline->find_clips().size();

        // Import the OTIO test timeline.
        OtioImport otioImport;
        otioImport.importFile(inputPath, false);
        while (timeline->getClipsCount() < otioClipCount) {
            qApp->processEvents();
        }

        // Export a new OTIO timeline.
        OtioExport otioExport;
        // TODO: Replace with QTemporaryDir.
        QDir dir = QDir::temp();
        QString outputPath = dir.filePath("test-clips-export.otio");
        otioExport.exportFile(outputPath);

        // Compare the original test OTIO timeline with the new one.
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioNewTimeline(
            dynamic_cast<OTIO_NS::Timeline *>(OTIO_NS::Timeline::from_json_file(outputPath.toStdString())));
        REQUIRE(otioTestTimeline->video_tracks().size() == otioNewTimeline->video_tracks().size());
        REQUIRE(otioTestTimeline->audio_tracks().size() == otioNewTimeline->audio_tracks().size());
        REQUIRE(otioClipCount == static_cast<int>(otioNewTimeline->find_clips().size()));

        // Compare the tracks.
        for (auto otioTestTrackIt = otioTestTimeline->tracks()->children().begin(), otioNewTrackIt = otioNewTimeline->tracks()->children().begin();
             otioTestTrackIt != otioTestTimeline->tracks()->children().end(); ++otioTestTrackIt, ++otioNewTrackIt) {
            auto otioTestTrack = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Track>(*otioTestTrackIt);
            auto otioNewTrack = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Track>(*otioNewTrackIt);
            REQUIRE(otioTestTrack != nullptr);
            REQUIRE(otioNewTrack != nullptr);
            REQUIRE(otioTestTrack->name() == otioNewTrack->name());
            REQUIRE(otioTestTrack->kind() == otioNewTrack->kind());
            REQUIRE(otioTestTrack->children().size() == otioNewTrack->children().size());

            // Compare the items, clips, and gaps.
            for (auto otioTestItemIt = otioTestTrack->children().begin(), otioNewItemIt = otioNewTrack->children().begin();
                 otioTestItemIt != otioTestTrack->children().end(); ++otioTestItemIt, ++otioNewItemIt) {
                REQUIRE((*otioTestItemIt)->duration() == (*otioNewItemIt)->duration());
                if (auto otioTestClip = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Clip>(*otioTestItemIt)) {
                    auto otioNewClip = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Clip>(*otioNewItemIt);
                    REQUIRE(otioNewClip != nullptr);
                    REQUIRE(otioTestClip->available_range() == otioNewClip->available_range());
                    auto otioTestRef = dynamic_cast<OTIO_NS::ExternalReference *>(otioTestClip->media_reference());
                    auto otioNewRef = dynamic_cast<OTIO_NS::ExternalReference *>(otioNewClip->media_reference());
                    REQUIRE(otioTestRef != nullptr);
                    REQUIRE(otioNewRef != nullptr);
                    REQUIRE(QFileInfo(QString::fromStdString(otioTestRef->target_url())).fileName() ==
                            QFileInfo(QString::fromStdString(otioNewRef->target_url())).fileName());
                } else if (auto otioTestGap = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Gap>(*otioTestItemIt)) {
                    auto otioNewGap = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Gap>(*otioNewItemIt);
                    REQUIRE(otioNewGap != nullptr);
                    REQUIRE(otioTestGap->available_range() == otioNewGap->available_range());
                }
            }
        }

        pCore->projectManager()->closeCurrentDocument(false, false);
    }
}
