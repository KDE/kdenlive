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

#include <QTemporaryDir>

#include <opentimelineio/externalReference.h>
#include <opentimelineio/gap.h>
#include <opentimelineio/generatorReference.h>
#include <opentimelineio/transition.h>

using namespace fakeit;

TEST_CASE("Export/import", "[OTIO]")
{
    SECTION("Multiple tracks and clips")
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
        QString inputPath = sourcesPath + "/dataset/test-tracks-clips.otio";
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
        QTemporaryDir dir;
        QString outputPath = dir.filePath("test-tracks-clips-export.otio");
        otioExport.exportFile(outputPath);

        // Compare the original test OTIO timeline with the new one.
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioNewTimeline(
            dynamic_cast<OTIO_NS::Timeline *>(OTIO_NS::Timeline::from_json_file(outputPath.toStdString())));
        REQUIRE(otioTestTimeline->video_tracks().size() == otioNewTimeline->video_tracks().size());
        REQUIRE(otioTestTimeline->audio_tracks().size() == otioNewTimeline->audio_tracks().size());
        REQUIRE(otioClipCount == static_cast<int>(otioNewTimeline->find_clips().size()));
        REQUIRE(otioTestTimeline->find_children<OTIO_NS::Gap>().size() == otioNewTimeline->find_children<OTIO_NS::Gap>().size());

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

    SECTION("Guides and markers")
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
        QString inputPath = sourcesPath + "/dataset/test-guides-markers.otio";
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
        QTemporaryDir dir;
        QString outputPath = dir.filePath("test-guides-markers-export.otio");
        otioExport.exportFile(outputPath);

        // Compare the original test OTIO timeline with the new one.
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioNewTimeline(
            dynamic_cast<OTIO_NS::Timeline *>(OTIO_NS::Timeline::from_json_file(outputPath.toStdString())));

        // Compare the OTIO stack markers (kdenlive guides).
        auto otioTestMarkers = otioTestTimeline->tracks()->markers();
        auto otioNewMarkers = otioNewTimeline->tracks()->markers();
        REQUIRE(otioTestMarkers.size() == otioNewMarkers.size());

        for (auto otioTestMarkerIt = otioTestMarkers.begin(), otioNewMarkerIt = otioNewMarkers.begin(); otioTestMarkerIt != otioTestMarkers.end();
             ++otioTestMarkerIt, ++otioNewMarkerIt) {
            REQUIRE((*otioTestMarkerIt)->name() == (*otioNewMarkerIt)->name());
            REQUIRE((*otioTestMarkerIt)->color() == (*otioNewMarkerIt)->color());
            REQUIRE((*otioTestMarkerIt)->marked_range() == (*otioNewMarkerIt)->marked_range());
            REQUIRE((*otioTestMarkerIt)->comment() == (*otioNewMarkerIt)->comment());
        }

        // Compare the OTIO clip markers.
        for (auto otioTestTrackIt = otioTestTimeline->tracks()->children().begin(), otioNewTrackIt = otioNewTimeline->tracks()->children().begin();
             otioTestTrackIt != otioTestTimeline->tracks()->children().end(); ++otioTestTrackIt, ++otioNewTrackIt) {

            auto otioTestTrack = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Track>(*otioTestTrackIt);
            auto otioNewTrack = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Track>(*otioNewTrackIt);
            for (auto otioTestItemIt = otioTestTrack->children().begin(), otioNewItemIt = otioNewTrack->children().begin();
                 otioTestItemIt != otioTestTrack->children().end(); ++otioTestItemIt, ++otioNewItemIt) {

                if (auto otioTestClip = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Clip>(*otioTestItemIt)) {

                    auto otioNewClip = OTIO_NS::dynamic_retainer_cast<OTIO_NS::Clip>(*otioNewItemIt);
                    auto otioTestMarkers = otioTestClip->markers();
                    auto otioNewMarkers = otioNewClip->markers();
                    REQUIRE(otioTestMarkers.size() == otioNewMarkers.size());

                    for (auto otioTestMarkerIt = otioTestMarkers.begin(), otioNewMarkerIt = otioNewMarkers.begin(); otioTestMarkerIt != otioTestMarkers.end();
                         ++otioTestMarkerIt, ++otioNewMarkerIt) {
                        REQUIRE((*otioTestMarkerIt)->name() == (*otioNewMarkerIt)->name());
                        REQUIRE((*otioTestMarkerIt)->color() == (*otioNewMarkerIt)->color());
                        REQUIRE((*otioTestMarkerIt)->marked_range() == (*otioNewMarkerIt)->marked_range());
                        REQUIRE((*otioTestMarkerIt)->comment() == (*otioNewMarkerIt)->comment());
                    }
                }
            }
        }

        pCore->projectManager()->closeCurrentDocument(false, false);
    }

    SECTION("Color Clips")
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
        QString inputPath = sourcesPath + "/dataset/test-color-clips.otio";
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTestTimeline(
            dynamic_cast<OTIO_NS::Timeline *>(OTIO_NS::Timeline::from_json_file(inputPath.toStdString())));
        const auto otioClips = otioTestTimeline->find_clips();
        const int otioClipCount = otioClips.size();

        // Import the OTIO test timeline.
        OtioImport otioImport;
        otioImport.importFile(inputPath, false);
        while (timeline->getClipsCount() < otioClipCount) {
            qApp->processEvents();
        }

        // Export a new OTIO timeline.
        OtioExport otioExport;
        QTemporaryDir dir;
        QString outputPath = dir.filePath("test-color-clips-export.otio");
        otioExport.exportFile(outputPath);

        // Compare the original test OTIO timeline with the new one.
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioNewTimeline(
            dynamic_cast<OTIO_NS::Timeline *>(OTIO_NS::Timeline::from_json_file(outputPath.toStdString())));
        const auto otioNewClips = otioNewTimeline->find_clips();
        REQUIRE(otioNewClips.size() == otioClips.size());
        for (const auto &otioClip : otioClips) {
            auto otioRef = dynamic_cast<OTIO_NS::GeneratorReference *>(otioClip->media_reference());
            REQUIRE(otioRef != nullptr);
            const std::string name = otioRef->name();
            auto i = std::find_if(otioNewClips.begin(), otioNewClips.end(), [name](const OTIO_NS::SerializableObject::Retainer<OTIO_NS::Clip> &otioClip) {
                auto otioRef = dynamic_cast<OTIO_NS::GeneratorReference *>(otioClip->media_reference());
                REQUIRE(otioRef != nullptr);
                return name == otioRef->name();
            });
            REQUIRE(i != otioNewClips.end());
        }
        pCore->projectManager()->closeCurrentDocument(false, false);
    }

    SECTION("Transitions")
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
        QString inputPath = sourcesPath + "/dataset/test-transitions.otio";
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTestTimeline(
            dynamic_cast<OTIO_NS::Timeline *>(OTIO_NS::Timeline::from_json_file(inputPath.toStdString())));
        const auto otioClips = otioTestTimeline->find_clips();
        const int otioClipCount = otioClips.size();

        // Import the OTIO test timeline.
        OtioImport otioImport;
        otioImport.importFile(inputPath, false);
        while (timeline->getClipsCount() < otioClipCount) {
            qApp->processEvents();
        }

        // Export a new OTIO timeline.
        OtioExport otioExport;
        QTemporaryDir dir;
        QString outputPath = dir.filePath("test-transitions-export.otio");
        otioExport.exportFile(outputPath);

        // Compare the original test OTIO timeline with the new one.
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioNewTimeline(
            dynamic_cast<OTIO_NS::Timeline *>(OTIO_NS::Timeline::from_json_file(outputPath.toStdString())));
        auto otioTestTransitions = otioTestTimeline->find_children<OTIO_NS::Transition>();
        REQUIRE(otioTestTransitions.size() > 0);
        auto otioNewTransitions = otioNewTimeline->find_children<OTIO_NS::Transition>();
        REQUIRE(otioNewTransitions.size() == otioTestTransitions.size());
        for (auto otioTestTransitionIt = otioTestTransitions.begin(), otioNewTransitionIt = otioNewTransitions.begin();
             otioTestTransitionIt != otioTestTransitions.end(); ++otioTestTransitionIt, ++otioNewTransitionIt) {
            REQUIRE((*otioNewTransitionIt)->name() == (*otioTestTransitionIt)->name());
            REQUIRE((*otioNewTransitionIt)->transition_type() == (*otioTestTransitionIt)->transition_type());
            REQUIRE((*otioNewTransitionIt)->in_offset() == (*otioTestTransitionIt)->in_offset());
            REQUIRE((*otioNewTransitionIt)->out_offset() == (*otioTestTransitionIt)->out_offset());
        }

        pCore->projectManager()->closeCurrentDocument(false, false);
    }

    SECTION("Missing Media")
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
        QString inputPath = sourcesPath + "/dataset/test-missing-media.otio";
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioTestTimeline(
            dynamic_cast<OTIO_NS::Timeline *>(OTIO_NS::Timeline::from_json_file(inputPath.toStdString())));
        const auto otioClips = otioTestTimeline->find_clips();
        const int otioClipCount = otioClips.size();

        // Import the OTIO test timeline.
        OtioImport otioImport;
        otioImport.importFile(inputPath, false);
        while (0 == timeline->getClipsCount()) {
            qApp->processEvents();
        }

        // Export a new OTIO timeline.
        OtioExport otioExport;
        QTemporaryDir dir;
        QString outputPath = dir.filePath("test-missing-media-export.otio");
        otioExport.exportFile(outputPath);

        // Compare the original test OTIO timeline with the new one.
        OTIO_NS::SerializableObject::Retainer<OTIO_NS::Timeline> otioNewTimeline(
            dynamic_cast<OTIO_NS::Timeline *>(OTIO_NS::Timeline::from_json_file(outputPath.toStdString())));
        REQUIRE(otioClipCount > 0);
        const auto otioNewClips = otioNewTimeline->find_clips();
        REQUIRE(static_cast<int>(otioNewClips.size()) < otioClipCount);

        pCore->projectManager()->closeCurrentDocument(false, false);
    }
}
