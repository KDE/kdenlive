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

using namespace fakeit;

TEST_CASE("Show/hide tracks", "[HideTracks]")
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

    // Audio tracks
    /*int tid1 =*/timeline->getTrackIndexFromPosition(0);
    int tid2 = timeline->getTrackIndexFromPosition(1);
    // Video tracks
    int tid3 = timeline->getTrackIndexFromPosition(2);
    int tid4 = timeline->getTrackIndexFromPosition(3);

    // Create clip with audio (100 frames long)
    QString binId = createProducerWithSound(pCore->getProjectProfile(), binModel, 100);
    // Create color clip (50 frames long)
    QString binId2 = createProducer(pCore->getProjectProfile(), "red", binModel, 50, false);

    // Setup insert stream data
    QMap<int, QString> audioInfo;
    audioInfo.insert(1, QStringLiteral("stream1"));
    timeline->m_binAudioTargets = audioInfo;

    int cid1;
    int cid2;

    // Create AV clip 1
    REQUIRE(timeline->requestClipInsertion(binId, tid3, 10, cid1));
    cid2 = timeline->getClipSplitPartner(cid1);

    // Remove video part
    timeline->m_groups->ungroupItem(cid2, undo, redo);
    timeline->requestItemDeletion(cid1);

    SECTION("Disable, enable audio track and check length")
    {
        REQUIRE(timeline->duration() == 110);
        timeline->setTrackProperty(tid2, QStringLiteral("hide"), "3");
        REQUIRE(timeline->duration() == 0);
        timeline->setTrackProperty(tid2, QStringLiteral("hide"), "1");
        REQUIRE(timeline->duration() == 110);
    }

    SECTION("Disable, add clip, enable audio track and check length")
    {
        REQUIRE(timeline->duration() == 110);
        timeline->setTrackProperty(tid2, QStringLiteral("hide"), "3");
        REQUIRE(timeline->duration() == 0);
        REQUIRE(timeline->requestClipInsertion(binId2, tid4, 10, cid1));
        REQUIRE(timeline->duration() == 60);
        timeline->setTrackProperty(tid2, QStringLiteral("hide"), "1");
        REQUIRE(timeline->duration() == 110);
        timeline->setTrackProperty(tid4, QStringLiteral("hide"), "3");
        REQUIRE(timeline->duration() == 110);
        timeline->setTrackProperty(tid4, QStringLiteral("hide"), "2");
        REQUIRE(timeline->duration() == 110);
        timeline->setTrackProperty(tid2, QStringLiteral("hide"), "3");
        REQUIRE(timeline->duration() == 60);
    }

    pCore->projectManager()->closeCurrentDocument(false, false);
}
