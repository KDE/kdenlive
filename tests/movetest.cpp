/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"
// test specific headers
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"

#include "core.h"
#include "definitions.h"

using namespace fakeit;

TEST_CASE("Cut undo/redo", "[MoveClips]")
{
    // Create timeline
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Create document
    KdenliveDoc document(undoStack);
    pCore->projectManager()->m_project = &document;
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    QDateTime documentDate = QDateTime::currentDateTime();
    pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->m_activeTimelineModel = timeline;
    pCore->projectManager()->testSetActiveDocument(&document, timeline);

    // Create a request
    int tid1 = timeline->getTrackIndexFromPosition(1);
    int tid2 = timeline->getTrackIndexFromPosition(2);
    int tid3 = timeline->getTrackIndexFromPosition(3);

    // Create clip with audio (40 frames long)
    // QString binId = createAVProducer(*pCore->getProjectProfile(), binModel);
    QString binId = createProducerWithSound(pCore->getProjectProfile(), binModel, 100);
    QString binId2 = createProducer(pCore->getProjectProfile(), "red", binModel, 100, false);

    // Setup insert stream data
    QMap<int, QString> audioInfo;
    audioInfo.insert(1, QStringLiteral("stream1"));
    timeline->m_binAudioTargets = audioInfo;

    // Create AV clip 1
    int cid1;
    int cid2;
    int cid3;
    int cid4;
    int cid5;

    REQUIRE(timeline->requestClipInsertion(binId, tid2, 100, cid1));
    cid2 = timeline->getClipSplitPartner(cid1);
    REQUIRE(timeline->requestClipInsertion(binId2, tid3, 100, cid5));

    SECTION("Ensure all clip instances on a track use the same producer")
    {
        REQUIRE(timeline->getItemTrackId(cid2) == tid1);
        REQUIRE(timeline->getItemTrackId(cid1) == tid2);
        Mlt::Producer prod1 = *(timeline->getClipPtr(cid1));
        Mlt::Producer prod2 = *(timeline->getClipPtr(cid2));
        // Clips on different tracks shoud not use the same producer
        REQUIRE(!prod1.same_clip(prod2));

        // Split clip
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid1, 110));
        cid3 = timeline->getClipByPosition(tid2, 111);
        cid4 = timeline->getClipSplitPartner(cid3);
        REQUIRE(timeline->getItemTrackId(cid4) == tid1);
        REQUIRE(timeline->getItemTrackId(cid3) == tid2);

        Mlt::Producer prod3 = *(timeline->getClipPtr(cid3));
        Mlt::Producer prod4 = *(timeline->getClipPtr(cid4));
        // Clips on different tracks shoud not use the same producer
        REQUIRE(!prod3.same_clip(prod4));
        // Clips on same track shoud use the same producer
        REQUIRE(prod1.same_clip(prod3));
        REQUIRE(prod2.same_clip(prod4));

        // Undo and redo cut, then ensure the producers are still correct
        undoStack->undo();
        undoStack->redo();

        prod3 = *(timeline->getClipPtr(cid3));
        prod4 = *(timeline->getClipPtr(cid4));
        // Clips on different tracks shoud not use the same producer
        REQUIRE(!prod3.same_clip(prod4));
        // Clips on same track shoud use the same producer
        REQUIRE(prod1.same_clip(prod3));
        REQUIRE(prod2.same_clip(prod4));
        // Undo cut
        undoStack->undo();
    }
    SECTION("Ensure selected group cut works")
    {
        // Set selection
        timeline->requestSetSelection({cid1, cid5});

        // Split clip
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid1, 110));

        // Undo and redo cut, then ensure the producers are still correct
        undoStack->undo();
        undoStack->redo();
        undoStack->undo();
    }
    pCore->projectManager()->closeCurrentDocument(false, false);
}
