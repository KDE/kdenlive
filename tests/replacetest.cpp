/*
    SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"
// test specific headers
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include <QString>
#include <cmath>
#include <iostream>
#include <tuple>
#include <unordered_set>

#include "core.h"
#include "definitions.h"

using namespace fakeit;

TEST_CASE("Replace Clip", "[ReplaceClip]")
{
    // Ensure our core profile is correct
    REQUIRE(pCore->timecode().fps() == 25.);
    REQUIRE(pCore->getDurationFromString(KdenliveSettings::mix_duration()) == 25);

    // Create timeline
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack
    KdenliveDoc document(undoStack);

    pCore->projectManager()->m_project = &document;
    QDateTime documentDate = QDateTime::currentDateTime();
    pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->testSetActiveDocument(&document, timeline);

    // Create tracks
    /*int tid1 =*/ timeline->getTrackIndexFromPosition(0);
    /*int tid3 =*/ timeline->getTrackIndexFromPosition(1);
    int tid2 = timeline->getTrackIndexFromPosition(2);
    /*int tid4 =*/ timeline->getTrackIndexFromPosition(3);

    // Create clips with audio
    QString binId = createProducerWithSound(pCore->getProjectProfile(), binModel, 100);
    QString binId2 = createProducerWithSound(pCore->getProjectProfile(), binModel, 100);
    QString binId3 = createProducerWithSound(pCore->getProjectProfile(), binModel, 15);

    // Setup insert stream data
    QMap<int, QString> audioInfo;
    audioInfo.insert(1, QStringLiteral("stream1"));
    timeline->m_binAudioTargets = audioInfo;

    int cid1;
    int cid2;
    int cid3;
    int audio1;
    int audio2;
    int audio3;
    // Create AV clip 1 at 100, duration 10 from first Bin clip
    REQUIRE(timeline->requestClipInsertion(binId, tid2, 100, cid1));
    REQUIRE(timeline->requestItemResize(cid1, 10, true, true));
    audio1 = timeline->getClipSplitPartner(cid1);

    // Create AV clip 2 at 110, duration 10 from second Bin clip
    REQUIRE(timeline->requestClipInsertion(binId2, tid2, 110, cid2));
    REQUIRE(timeline->requestItemResize(cid2, 10, true, true));
    REQUIRE(timeline->requestClipMove(cid2, tid2, 110));
    audio2 = timeline->getClipSplitPartner(cid2);

    // Create AV clip 3 at 200, duration 20 from first Bin clip
    REQUIRE(timeline->requestClipInsertion(binId, tid2, 200, cid3));
    REQUIRE(timeline->requestItemResize(cid3, 20, true, true));
    audio3 = timeline->getClipSplitPartner(cid3);

    auto state0 = [&]() {
        REQUIRE(timeline->getClipsCount() == 6);
        REQUIRE(timeline->getClipPlaytime(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid1) == 100);
        REQUIRE(timeline->getClipPlaytime(cid2) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 110);
        REQUIRE(timeline->getClipPosition(cid3) == 200);
        REQUIRE(timeline->getClipPlaytime(cid3) == 20);
        REQUIRE(timeline->getClipPlaytime(audio1) == 10);
        REQUIRE(timeline->getClipPosition(audio1) == 100);
        REQUIRE(timeline->getClipPlaytime(audio2) == 10);
        REQUIRE(timeline->getClipPosition(audio2) == 110);
        REQUIRE(timeline->getClipPosition(audio3) == 200);
        REQUIRE(timeline->getClipPlaytime(audio3) == 20);
        REQUIRE(timeline->m_allClips[cid1]->binId() == binId);
        REQUIRE(timeline->m_allClips[audio1]->binId() == binId);
        REQUIRE(timeline->m_allClips[cid2]->binId() == binId2);
        REQUIRE(timeline->m_allClips[audio2]->binId() == binId2);
        REQUIRE(timeline->m_allClips[cid3]->binId() == binId);
        REQUIRE(timeline->m_allClips[audio3]->binId() == binId);
    };

    auto state1 = [&]() {
        REQUIRE(timeline->getClipsCount() == 6);
        REQUIRE(timeline->getClipPlaytime(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid1) == 100);
        REQUIRE(timeline->getClipPlaytime(cid2) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 110);
        REQUIRE(timeline->getClipPosition(cid3) == 200);
        REQUIRE(timeline->getClipPlaytime(cid3) == 20);
        REQUIRE(timeline->getClipPlaytime(audio1) == 10);
        REQUIRE(timeline->getClipPosition(audio1) == 100);
        REQUIRE(timeline->getClipPlaytime(audio2) == 10);
        REQUIRE(timeline->getClipPosition(audio2) == 110);
        REQUIRE(timeline->getClipPosition(audio3) == 200);
        REQUIRE(timeline->getClipPlaytime(audio3) == 20);
        REQUIRE(timeline->m_allClips[cid1]->binId() == binId);
        REQUIRE(timeline->m_allClips[audio1]->binId() == binId2);
        REQUIRE(timeline->m_allClips[cid2]->binId() == binId2);
        REQUIRE(timeline->m_allClips[audio2]->binId() == binId2);
        REQUIRE(timeline->m_allClips[cid3]->binId() == binId);
        REQUIRE(timeline->m_allClips[audio3]->binId() == binId2);
    };

    auto state2 = [&]() {
        REQUIRE(timeline->getClipsCount() == 6);
        REQUIRE(timeline->getClipPlaytime(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid1) == 100);
        REQUIRE(timeline->getClipPlaytime(cid2) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 110);
        REQUIRE(timeline->getClipPosition(cid3) == 200);
        REQUIRE(timeline->getClipPlaytime(cid3) == 20);
        REQUIRE(timeline->getClipPlaytime(audio1) == 10);
        REQUIRE(timeline->getClipPosition(audio1) == 100);
        REQUIRE(timeline->getClipPlaytime(audio2) == 10);
        REQUIRE(timeline->getClipPosition(audio2) == 110);
        REQUIRE(timeline->getClipPosition(audio3) == 200);
        REQUIRE(timeline->getClipPlaytime(audio3) == 20);
        REQUIRE(timeline->m_allClips[cid1]->binId() == binId2);
        REQUIRE(timeline->m_allClips[audio1]->binId() == binId);
        REQUIRE(timeline->m_allClips[cid2]->binId() == binId2);
        REQUIRE(timeline->m_allClips[audio2]->binId() == binId2);
        REQUIRE(timeline->m_allClips[cid3]->binId() == binId2);
        REQUIRE(timeline->m_allClips[audio3]->binId() == binId);
    };

    auto state3 = [&]() {
        REQUIRE(timeline->getClipsCount() == 6);
        REQUIRE(timeline->getClipPlaytime(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid1) == 100);
        REQUIRE(timeline->getClipPlaytime(cid2) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 110);
        REQUIRE(timeline->getClipPosition(cid3) == 200);
        REQUIRE(timeline->getClipPlaytime(cid3) == 20);
        REQUIRE(timeline->getClipPlaytime(audio1) == 10);
        REQUIRE(timeline->getClipPosition(audio1) == 100);
        REQUIRE(timeline->getClipPlaytime(audio2) == 10);
        REQUIRE(timeline->getClipPosition(audio2) == 110);
        REQUIRE(timeline->getClipPosition(audio3) == 200);
        REQUIRE(timeline->getClipPlaytime(audio3) == 20);
        REQUIRE(timeline->m_allClips[cid1]->binId() == binId2);
        REQUIRE(timeline->m_allClips[audio1]->binId() == binId2);
        REQUIRE(timeline->m_allClips[cid2]->binId() == binId2);
        REQUIRE(timeline->m_allClips[audio2]->binId() == binId2);
        REQUIRE(timeline->m_allClips[cid3]->binId() == binId2);
        REQUIRE(timeline->m_allClips[audio3]->binId() == binId2);
    };
    auto state4 = [&]() {
        REQUIRE(timeline->getClipsCount() == 6);
        REQUIRE(timeline->getClipPlaytime(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid1) == 100);
        REQUIRE(timeline->getClipPlaytime(cid2) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 110);
        REQUIRE(timeline->getClipPosition(cid3) == 200);
        REQUIRE(timeline->getClipPlaytime(cid3) == 20);
        REQUIRE(timeline->getClipPlaytime(audio1) == 10);
        REQUIRE(timeline->getClipPosition(audio1) == 100);
        REQUIRE(timeline->getClipPlaytime(audio2) == 10);
        REQUIRE(timeline->getClipPosition(audio2) == 110);
        REQUIRE(timeline->getClipPosition(audio3) == 200);
        REQUIRE(timeline->getClipPlaytime(audio3) == 20);
        REQUIRE(timeline->m_allClips[cid1]->binId() == binId);
        REQUIRE(timeline->m_allClips[audio1]->binId() == binId3);
        REQUIRE(timeline->m_allClips[cid2]->binId() == binId2);
        REQUIRE(timeline->m_allClips[audio2]->binId() == binId2);
        REQUIRE(timeline->m_allClips[cid3]->binId() == binId);
        REQUIRE(timeline->m_allClips[audio3]->binId() == binId);
    };

    SECTION("Replace audio from Bin Clip 1 with Bin Clip 2")
    {
        state0();
        pCore->projectManager()->replaceTimelineInstances(binId, binId2, true, false);
        state1();
        undoStack->undo();
        state0();
        undoStack->redo();
        state1();
        undoStack->undo();
        state0();
    }
    SECTION("Replace video from Bin Clip 1 with Bin Clip 2")
    {
        state0();
        pCore->projectManager()->replaceTimelineInstances(binId, binId2, false, true);
        state2();
        undoStack->undo();
        state0();
        undoStack->redo();
        state2();
        undoStack->undo();
        state0();
    }
    SECTION("Replace video and audio from Bin Clip 1 with Bin Clip 2")
    {
        state0();
        pCore->projectManager()->replaceTimelineInstances(binId, binId2, true, true);
        state3();
        undoStack->undo();
        state0();
        undoStack->redo();
        state3();
        undoStack->undo();
        state0();
    }

    SECTION("Replace audio from Bin Clip 1 with shorter Bin Clip 3")
    {
        state0();
        pCore->projectManager()->replaceTimelineInstances(binId, binId3, true, false);
        state4();
        undoStack->undo();
        state0();
        undoStack->redo();
        state4();
        undoStack->undo();
        state0();
    }
    timeline.reset();
    pCore->projectManager()->closeCurrentDocument(false, false);
}
