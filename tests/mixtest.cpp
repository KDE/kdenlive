/*
    SPDX-FileCopyrightText: 2020-2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"
// test specific headers
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include <QString>
#include <unordered_set>

#include "core.h"

using namespace fakeit;

TEST_CASE("Simple Mix", "[SameTrackMix]")
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

    pCore->projectManager()->testSetDocument(&document);
    QDateTime documentDate = QDateTime::currentDateTime();
    KdenliveTests::updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->testSetActiveTimeline(timeline);

    // Create a request
    int tid1 = timeline->getTrackIndexFromPosition(0);
    int tid3 = timeline->getTrackIndexFromPosition(1);
    int tid2 = timeline->getTrackIndexFromPosition(2);
    int tid4 = timeline->getTrackIndexFromPosition(3);

    // Create clip with audio
    QString binId = KdenliveTests::createProducerWithSound(pCore->getProjectProfile(), binModel, 100);

    // Create video clip
    QString binId2 = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel, 50, false);
    // Setup insert stream data
    QMap<int, QString> audioInfo;
    audioInfo.insert(1, QStringLiteral("stream1"));
    KdenliveTests::setAudioTargets(timeline, audioInfo);

    // Create AV clip 1
    int cid1;
    int cid2;
    int cid3;
    int cid4;
    int cid5;
    int audio1;
    int audio2;
    int audio5;
    REQUIRE(timeline->requestClipInsertion(binId, tid2, 100, cid1));
    REQUIRE(timeline->requestItemResize(cid1, 10, true, true));
    audio1 = timeline->getClipSplitPartner(cid1);

    // Create AV clip 2
    REQUIRE(timeline->requestClipInsertion(binId, tid2, 110, cid2));
    REQUIRE(timeline->requestItemResize(cid2, 10, true, true));
    REQUIRE(timeline->requestClipMove(cid2, tid2, 110));
    audio2 = timeline->getClipSplitPartner(cid2);

    // Create color clip 1
    REQUIRE(timeline->requestClipInsertion(binId2, tid2, 500, cid3));
    REQUIRE(timeline->requestItemResize(cid3, 20, true, true));
    REQUIRE(timeline->requestClipInsertion(binId2, tid2, 520, cid4));
    REQUIRE(timeline->requestItemResize(cid4, 20, true, true));
    int mixDuration = pCore->getDurationFromString(KdenliveSettings::mix_duration());

    auto state0 = [&]() {
        REQUIRE(timeline->getClipsCount() == 6);
        REQUIRE(timeline->getClipPlaytime(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid1) == 100);
        REQUIRE(timeline->getClipPlaytime(cid2) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 110);
        REQUIRE(timeline->getClipPosition(cid3) == 500);
        REQUIRE(timeline->getClipPlaytime(cid3) == 20);
        REQUIRE(timeline->getClipPosition(cid4) == 520);
        REQUIRE(timeline->getClipPlaytime(cid4) == 20);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid1)->mixCount() == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
    };

    auto state0b = [&]() {
        REQUIRE(timeline->getClipsCount() == 8);
        REQUIRE(timeline->getClipPlaytime(cid1) == 10);
        REQUIRE(timeline->getClipPosition(cid1) == 100);
        REQUIRE(timeline->getClipPlaytime(cid2) == 10);
        REQUIRE(timeline->getClipPosition(cid2) == 110);
        REQUIRE(timeline->getClipPlaytime(cid5) == 10);
        REQUIRE(timeline->getClipPosition(cid5) == 120);
        REQUIRE(timeline->getClipPosition(cid3) == 500);
        REQUIRE(timeline->getClipPlaytime(cid3) == 20);
        REQUIRE(timeline->getClipPosition(cid4) == 520);
        REQUIRE(timeline->getClipPlaytime(cid4) == 20);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid1)->mixCount() == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
    };

    auto state1 = [&]() {
        REQUIRE(timeline->getClipsCount() == 6);
        REQUIRE(timeline->getClipPlaytime(cid1) > 10);
        REQUIRE(timeline->getClipPosition(cid1) == 100);
        REQUIRE(timeline->getClipPlaytime(cid2) > 10);
        REQUIRE(timeline->getClipPosition(cid2) < 110);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixCount() == 1);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
    };

    auto state1b = [&]() {
        REQUIRE(timeline->getClipsCount() == 8);
        REQUIRE(timeline->getClipPlaytime(cid1) > 10);
        REQUIRE(timeline->getClipPosition(cid1) == 100);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid1) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(audio1) == 0);
        REQUIRE(timeline->getClipPlaytime(cid2) > 10);
        REQUIRE(timeline->getClipPosition(cid2) < 110);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid2) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(audio2) == 1);
        REQUIRE(timeline->getClipPlaytime(cid5) == 10);
        REQUIRE(timeline->getClipPosition(cid5) == 120);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(audio5) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixCount() == 1);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
    };

    auto state3 = [&, mixDuration]() {
        REQUIRE(timeline->getClipsCount() == 6);
        REQUIRE(timeline->getClipPlaytime(cid1) > 30);
        REQUIRE(timeline->getClipPosition(cid1) == 100);
        REQUIRE(timeline->getClipPlaytime(cid2) > 30);
        REQUIRE(timeline->getClipPosition(cid2) < 130);
        REQUIRE(timeline->getMixDuration(cid2) == mixDuration);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixCount() == 1);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
    };

    auto state2 = [&]() {
        REQUIRE(timeline->getClipsCount() == 6);
        REQUIRE(timeline->getClipPlaytime(cid3) == 32);
        REQUIRE(timeline->getClipPosition(cid3) == 500);
        REQUIRE(timeline->getClipPlaytime(cid4) == 33);
        REQUIRE(timeline->getClipPosition(cid4) == 507);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid1)->mixCount() == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
    };

    SECTION("Create and delete mix on color clips")
    {
        // Cid3 at 500, length 20
        // Cid4 at 520, length 20
        state0();
        REQUIRE(timeline->mixClip(cid4));
        state2();
        // Automatic lookup follows the cut; explicit playlists expose both overlapping clips.
        CHECK(timeline->getClipByPosition(tid2, 519) == cid3);
        CHECK(timeline->getClipByPosition(tid2, 520) == cid4);
        CHECK(timeline->getClipByPosition(tid2, 520, 0) == cid3);
        CHECK(timeline->getClipByPosition(tid2, 519, 1) == cid4);
        undoStack->undo();
        state0();
        undoStack->redo();
        state2();
        undoStack->undo();
        state0();
    }

    SECTION("Create mix and cut on color clips")
    {
        // Cid3 at 500, length 20
        // Cid4 at 520, length 20
        state0();
        REQUIRE(timeline->mixClip(cid4));
        state2();
        // Cut a clip with mix end only
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid3, 505));
        // Get newly created cut clip
        int clone = timeline->getClipByPosition(tid2, 506, 0);
        // Ensure each clip is on the correct playlist
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(clone) == 0);
        undoStack->undo();
        state2();

        // Cut a clip with mix start only
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid4, 535));
        // Get newly created cut clip
        clone = timeline->getClipByPosition(tid2, 536);
        // Ensure each clip is on the correct playlist
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(clone) == 0);
        undoStack->undo();
        state2();
        undoStack->undo();
        state0();
        undoStack->redo();
        state2();
        undoStack->undo();
        state0();
    }

    SECTION("Add mix and resize last clip in playlist")
    {
        // Cid3 at 500, length 20
        // Cid4 at 520, length 20
        state0();
        REQUIRE(timeline->mixClip(cid4));
        state2();
        // Resize clip 4
        REQUIRE(timeline->requestItemResize(cid4, 60, true, true));
        REQUIRE(timeline->getClipPlaytime(cid4) == 60);
        undoStack->undo();
        REQUIRE(timeline->getClipPlaytime(cid4) == 33);
        undoStack->redo();
        REQUIRE(timeline->getClipPlaytime(cid4) == 60);
        undoStack->undo();

        undoStack->undo();
        state0();
        undoStack->redo();
        state2();
        undoStack->undo();
        state0();
    }

    SECTION("Create mix on color clips and move main (right side) clip")
    {
        // CID 3 length=20, pos=500, CID4 length=20, pos=520
        // Default mix duration = 25 frames (12 before / 13 after)
        state0();
        REQUIRE(timeline->mixClip(cid4));
        state2();

        // Move right clip to the left, should fail
        REQUIRE(timeline->requestClipMove(cid4, tid2, 506) == false);

        // Move clip inside mix zone, should delete the mix
        REQUIRE(timeline->requestClipMove(cid4, tid2, 509));
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
        undoStack->undo();
        state2();
        // Move clip outside mix zone, should delete the mix and move it back to playlist 0
        REQUIRE(timeline->requestClipMove(cid4, tid2, 600));
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
        undoStack->undo();
        state2();
        // Move clip to another track, should delete mix
        REQUIRE(timeline->requestClipMove(cid4, tid4, 600));
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid4)->mixCount() == 0);
        undoStack->undo();
        state2();
        undoStack->undo();
        state0();
    }

    SECTION("Create mix on color clip and move left side clip")
    {
        state0();
        REQUIRE(timeline->mixClip(cid4));
        state2();

        // Move left clip to the right, should silently fail
        REQUIRE(timeline->requestClipMove(cid3, tid2, 502, true, true, false) == false);
        REQUIRE(timeline->getClipPosition(cid3) == 500);

        // Move clip inside mix zone, should delete the mix
        REQUIRE(timeline->requestClipMove(cid3, tid2, 499));
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
        undoStack->undo();
        state2();
        // Move clip outside mix zone, should delete the mix
        REQUIRE(timeline->requestClipMove(cid3, tid2, 450));
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
        undoStack->undo();
        state2();
        // Move clip to another track, should delete mix
        REQUIRE(timeline->requestClipMove(cid3, tid4, 600));
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid4)->mixCount() == 0);
        undoStack->undo();
        state2();
        undoStack->undo();
        state0();
    }

    SECTION("Create mix on color clips and move some to another track")
    {
        state0();
        // insert third color clip
        cid5 = -1;
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 540, cid5));
        REQUIRE(timeline->requestItemResize(cid5, 20, true, true));
        REQUIRE(timeline->getClipPosition(cid5) == 540);
        audio5 = timeline->getClipSplitPartner(cid5);

        // CID 3 length=20, pos=500, CID4 length=20, pos=520, CID5 length=20, pos=540
        // Default mix duration = 25 frames (12 before / 13 after)

        REQUIRE(timeline->mixClip(cid3));
        REQUIRE(timeline->mixClip(cid4));
        REQUIRE(timeline->getClipPosition(cid5) < 540);
        undoStack->undo();
        REQUIRE(timeline->getClipPosition(cid5) == 540);
        undoStack->redo();
        REQUIRE(timeline->getClipPosition(cid5) < 540);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid4)->mixCount() == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 2);

        // Move middle clip to another track, should delete the mixes
        REQUIRE(timeline->requestClipMove(cid4, tid4, 500));
        REQUIRE(timeline->getClipPosition(cid5) == 540);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid4)->mixCount() == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);

        // Undo track move
        undoStack->undo();
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid4)->mixCount() == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 2);

        // Undo mixes
        undoStack->undo();
        undoStack->undo();
        // undo 3rd clip resize & insert
        undoStack->undo();
        undoStack->undo();
        state0();
    }

    SECTION("Resize a mix around its edit point")
    {
        REQUIRE(timeline->mixClip(cid4));
        auto track = KdenliveTests::getTrackById_const(timeline, tid2);
        const int undoIndex = undoStack->index();
        for (const auto &durations : {std::pair{7, 3}, {0, 10}, {10, 0}, {18, 17}}) {
            CAPTURE(durations.first, durations.second);
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            REQUIRE(track->requestResizeMix(cid4, durations, undo, redo));
            auto resizedState = [&]() {
                REQUIRE(timeline->checkConsistency());
                REQUIRE(undoStack->index() == undoIndex);
                REQUIRE(timeline->getClipPosition(cid3) == 500);
                REQUIRE(timeline->getClipPlaytime(cid3) == 20 + durations.second);
                REQUIRE(timeline->getClipPosition(cid4) == 520 - durations.first);
                REQUIRE(timeline->getClipPlaytime(cid4) == 20 + durations.first);
                REQUIRE(timeline->getMixDuration(cid4) == durations.first + durations.second);
                REQUIRE(KdenliveTests::getClipPtr(timeline, cid4)->getMixCutPosition() == durations.second);
                REQUIRE(timeline->getClipByPosition(tid2, 519) == cid3);
                REQUIRE(timeline->getClipByPosition(tid2, 520) == cid4);
            };
            resizedState();
            REQUIRE(undo());
            state2();
            REQUIRE(timeline->checkConsistency());
            REQUIRE(redo());
            resizedState();
            REQUIRE(undo());
            state2();
        }
        for (const auto &durations : {std::pair{-1, 5}, {5, -1}, {0, 0}, {21, 1}, {1, 21}}) {
            CAPTURE(durations.first, durations.second);
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            REQUIRE_FALSE(track->requestResizeMix(cid4, durations, undo, redo));
            state2();
            REQUIRE(undo());
            REQUIRE(redo());
            state2();
            REQUIRE(timeline->checkConsistency());
        }
    }

    SECTION("Mix resize alignment preserves the edit point and forms one undo command")
    {
        REQUIRE(timeline->mixClip(cid4));
        struct Resize
        {
            int duration;
            MixAlignment alignment;
            int leftFrames;
            int before;
            int after;
        };
        const std::vector<Resize> requests = {
            {10, MixAlignment::AlignLeft, -1, 0, 10},     {10, MixAlignment::AlignRight, -1, 10, 0}, {11, MixAlignment::AlignCenter, -1, 5, 6},
            {10, MixAlignment::AlignNone, 3, 3, 7},       {10, MixAlignment::AlignNone, -1, 6, 4},   {24, MixAlignment::AlignNone, -1, 12, 12},
            {26, MixAlignment::AlignNone, -1, 14, 12},    {100, MixAlignment::AlignLeft, -1, 0, 20}, {100, MixAlignment::AlignRight, -1, 20, 0},
            {100, MixAlignment::AlignCenter, -1, 20, 20},
        };
        for (const auto &request : requests) {
            CAPTURE(request.duration, request.before, request.after);
            const int undoIndex = undoStack->index();
            timeline->requestResizeMix(cid4, request.duration, request.alignment, request.leftFrames);
            auto resizedState = [&]() {
                REQUIRE(timeline->checkConsistency());
                REQUIRE(undoStack->index() == undoIndex + 1);
                REQUIRE(timeline->getClipPosition(cid3) == 500);
                REQUIRE(timeline->getClipPlaytime(cid3) == 20 + request.after);
                REQUIRE(timeline->getClipPosition(cid4) == 520 - request.before);
                REQUIRE(timeline->getClipPlaytime(cid4) == 20 + request.before);
                REQUIRE(timeline->getMixDuration(cid4) == request.before + request.after);
                REQUIRE(KdenliveTests::getClipPtr(timeline, cid4)->getMixCutPosition() == request.after);
            };
            resizedState();
            undoStack->undo();
            state2();
            REQUIRE(timeline->checkConsistency());
            undoStack->redo();
            resizedState();
            undoStack->undo();
            state2();
        }
        const int undoIndex = undoStack->index();
        timeline->requestResizeMix(cid4, 0, MixAlignment::AlignNone);
        timeline->requestResizeMix(cid4, 25, MixAlignment::AlignNone);
        REQUIRE(undoStack->index() == undoIndex);
        REQUIRE(timeline->checkConsistency());
        state2();
    }

    SECTION("Mix resize respects available source footage")
    {
        const QString limitedBin = KdenliveTests::createProducer(pCore->getProjectProfile(), "blue", binModel, 50);
        int first;
        int second;
        REQUIRE(timeline->requestClipInsertion(limitedBin + "/20/39", tid2, 600, first));
        REQUIRE(timeline->requestClipInsertion(limitedBin + "/5/24", tid2, 620, second));
        REQUIRE(timeline->mixClip(second));
        auto track = KdenliveTests::getTrackById_const(timeline, tid2);
        const int undoIndex = undoStack->index();
        auto originalState = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(undoStack->index() == undoIndex);
            REQUIRE(timeline->getClipPosition(first) == 600);
            REQUIRE(timeline->getClipPlaytime(first) == 30);
            REQUIRE(timeline->getClipPosition(second) == 615);
            REQUIRE(timeline->getClipPlaytime(second) == 25);
            REQUIRE(timeline->getMixDuration(second) == 15);
            REQUIRE(KdenliveTests::getClipPtr(timeline, second)->getMixCutPosition() == 10);
        };
        originalState();
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        // The outgoing clip can shrink, but the incoming clip cannot extend
        // before frame zero of its source. Failure must restore both clips.
        REQUIRE_FALSE(track->requestResizeMix(second, {6, 2}, undo, redo));
        originalState();
        REQUIRE(undo());
        REQUIRE(redo());
        originalState();
        timeline->requestResizeMix(second, 20, MixAlignment::AlignCenter);
        originalState();
        timeline->requestResizeMix(second, 6, MixAlignment::AlignCenter);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipPlaytime(first) == 23);
        REQUIRE(timeline->getClipPosition(second) == 617);
        REQUIRE(timeline->getMixDuration(second) == 6);
        timeline->requestResizeMix(second, 40, MixAlignment::AlignNone, 20);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipPlaytime(first) == 30);
        REQUIRE(timeline->getClipPosition(second) == 615);
        REQUIRE(timeline->getMixDuration(second) == 15);
        REQUIRE(undoStack->index() == undoIndex + 2);
        undoStack->undo();
        REQUIRE(timeline->getMixDuration(second) == 6);
        undoStack->undo();
        originalState();
    }

    SECTION("Failed mix resize restores a participant already resized")
    {
        REQUIRE(timeline->requestItemResize(cid4, 80, true) == 80);
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 600, cid5));
        REQUIRE(timeline->requestItemResize(cid5, 80, true) == 80);
        REQUIRE(timeline->mixClip(cid3));
        REQUIRE(timeline->mixClip(cid4));
        auto track = KdenliveTests::getTrackById_const(timeline, tid2);
        const int undoIndex = undoStack->index();
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        // The middle clip can shrink, but extending the last clip to 510 would
        // overlap the first clip in the same playlist (which ends at 532).
        REQUIRE_FALSE(track->requestResizeMix(cid5, {90, 3}, undo, redo));
        auto unchangedState = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(undoStack->index() == undoIndex);
            REQUIRE(timeline->getClipPosition(cid3) == 500);
            REQUIRE(timeline->getClipPlaytime(cid3) == 32);
            REQUIRE(timeline->getClipPosition(cid4) == 507);
            REQUIRE(timeline->getClipPlaytime(cid4) == 105);
            REQUIRE(timeline->getClipPosition(cid5) == 587);
            REQUIRE(timeline->getClipPlaytime(cid5) == 93);
            REQUIRE(timeline->getMixDuration(cid4) == 25);
            REQUIRE(timeline->getMixDuration(cid5) == 25);
            REQUIRE(KdenliveTests::getClipPtr(timeline, cid4)->getMixCutPosition() == 12);
            REQUIRE(KdenliveTests::getClipPtr(timeline, cid5)->getMixCutPosition() == 12);
        };
        unchangedState();
        REQUIRE(undo());
        REQUIRE(redo());
        unchangedState();
        timeline->requestResizeMix(cid5, 93, MixAlignment::AlignNone, 90);
        unchangedState();
    }

    SECTION("Cut inside a mix shortens it to the cut")
    {
        REQUIRE(timeline->mixClip(cid4));
        state2();
        const int oldCut = KdenliveTests::getClipPtr(timeline, cid4)->getMixCutPosition();
        // Exercise both sides of the original edit point and one-frame remaining mixes.
        for (bool startMix : {false, true}) {
            for (int cut : {startMix ? 532 : 507, 508, 515, 520, 525, 531}) {
                CAPTURE(startMix, cut);
                const int undoIndex = undoStack->index();
                REQUIRE(TimelineFunctions::requestClipCut(timeline, startMix ? cid4 : cid3, cut));
                const int clone = timeline->getClipByPosition(tid2, cut, 0);
                REQUIRE(clone > -1);
                REQUIRE(clone != cid3);
                REQUIRE(clone != cid4);
                auto cutState = [&]() {
                    REQUIRE(timeline->checkConsistency());
                    REQUIRE(undoStack->index() == undoIndex + 1);
                    REQUIRE(timeline->getClipsCount() == 7);
                    REQUIRE(timeline->getClipPosition(cid3) == 500);
                    REQUIRE(timeline->getClipPlaytime(cid3) == cut - 500);
                    REQUIRE(timeline->getClipPosition(cid4) == (startMix ? 507 : cut));
                    REQUIRE(timeline->getClipPlaytime(cid4) == (startMix ? cut - 507 : 540 - cut));
                    REQUIRE(timeline->getClipPosition(clone) == cut);
                    REQUIRE(timeline->getClipPlaytime(clone) == (startMix ? 540 : 532) - cut);
                    REQUIRE(timeline->getClipSubPlaylistIndex(clone) == 0);
                    REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
                    REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
                    const int mixDuration = startMix ? cut - 507 : 532 - cut;
                    REQUIRE(timeline->getMixDuration(cid4) == mixDuration);
                    REQUIRE(KdenliveTests::getClipPtr(timeline, cid4)->getMixCutPosition() ==
                            qBound(startMix ? 1 : 0, oldCut - (startMix ? 532 - cut : 0), mixDuration - (startMix ? 0 : 1)));
                };
                cutState();
                undoStack->undo();
                state2();
                REQUIRE(timeline->checkConsistency());
                REQUIRE(KdenliveTests::getClipPtr(timeline, cid4)->getMixCutPosition() == oldCut);
                undoStack->redo();
                cutState();
                // Later deletion and its undo must still find the correct clips and mix.
                REQUIRE(timeline->requestItemDeletion(clone));
                REQUIRE(timeline->checkConsistency());
                undoStack->undo();
                cutState();
                REQUIRE(timeline->requestClipMove(clone, tid4, 600));
                REQUIRE(timeline->checkConsistency());
                undoStack->undo();
                cutState();
                undoStack->undo();
                state2();
                REQUIRE(timeline->checkConsistency());
            }
        }
    }

    SECTION("Cut inside chained mixes preserves the other mix")
    {
        REQUIRE(timeline->requestItemResize(cid4, 80, true) == 80);
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 600, cid5));
        REQUIRE(timeline->requestItemResize(cid5, 80, true) == 80);
        REQUIRE(timeline->mixClip(cid3));
        REQUIRE(timeline->mixClip(cid4));
        auto mixedState = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPosition(cid3) == 500);
            REQUIRE(timeline->getClipPlaytime(cid3) == 32);
            REQUIRE(timeline->getClipPosition(cid4) == 507);
            REQUIRE(timeline->getClipPlaytime(cid4) == 105);
            REQUIRE(timeline->getClipPosition(cid5) == 587);
            REQUIRE(timeline->getClipPlaytime(cid5) == 93);
            REQUIRE(timeline->getMixDuration(cid4) == 25);
            REQUIRE(timeline->getMixDuration(cid5) == 25);
        };
        mixedState();
        for (auto target : {std::make_pair(cid4, 515), std::make_pair(cid4, 600), std::make_pair(cid5, 600)}) {
            CAPTURE(target.first, target.second);
            REQUIRE(TimelineFunctions::requestClipCut(timeline, target.first, target.second));
            int clone = timeline->getClipByPosition(tid2, target.second, target.first == cid4 ? 1 : 0);
            REQUIRE(clone > -1);
            REQUIRE(clone != target.first);
            auto cutState = [&]() {
                REQUIRE(timeline->checkConsistency());
                REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 2);
                REQUIRE(timeline->getClipPosition(clone) == target.second);
                REQUIRE(timeline->getClipPlaytime(clone) == (target.first == cid4 ? 612 : 680) - target.second);
                REQUIRE(timeline->getMixDuration(cid4) == (target.second == 515 ? 8 : 25));
                REQUIRE(timeline->getMixDuration(cid5) == (target.second == 515 ? 25 : target.first == cid4 ? 12 : 13));
            };
            cutState();
            undoStack->undo();
            mixedState();
            undoStack->redo();
            cutState();
            REQUIRE(timeline->requestItemDeletion(clone));
            REQUIRE(timeline->checkConsistency());
            undoStack->undo();
            cutState();
            undoStack->undo();
            mixedState();
        }
    }

    SECTION("Cut inside an AV mix keeps audio and video aligned")
    {
        REQUIRE(timeline->mixClip(cid2));
        const int mixStart = timeline->getClipPosition(cid2);
        const int mixEnd = timeline->getClipPosition(cid1) + timeline->getClipPlaytime(cid1);
        const int cut = (mixStart + mixEnd) / 2;
        REQUIRE(cut > mixStart);
        REQUIRE(cut < mixEnd);
        for (bool startMix : {false, true}) {
            REQUIRE(TimelineFunctions::requestClipCut(timeline, startMix ? cid2 : cid1, cut));
            auto cutState = [&]() {
                REQUIRE(timeline->checkConsistency());
                REQUIRE(timeline->getClipPosition(cid1) == timeline->getClipPosition(audio1));
                REQUIRE(timeline->getClipPlaytime(cid1) == timeline->getClipPlaytime(audio1));
                REQUIRE(timeline->getClipPosition(cid2) == timeline->getClipPosition(audio2));
                REQUIRE(timeline->getClipPlaytime(cid2) == timeline->getClipPlaytime(audio2));
                REQUIRE(timeline->getMixDuration(cid2) == (startMix ? cut - mixStart : mixEnd - cut));
                REQUIRE(timeline->getMixDuration(audio2) == timeline->getMixDuration(cid2));
                int clone = timeline->getClipByPosition(tid2, cut, 0);
                REQUIRE(clone > -1);
                int audioClone = timeline->getClipSplitPartner(clone);
                REQUIRE(audioClone > -1);
                REQUIRE(timeline->getClipPosition(audioClone) == cut);
                REQUIRE(timeline->getClipPlaytime(audioClone) == timeline->getClipPlaytime(clone));
            };
            cutState();
            undoStack->undo();
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getMixDuration(cid2) == mixEnd - mixStart);
            REQUIRE(timeline->getMixDuration(audio2) == mixEnd - mixStart);
            undoStack->redo();
            cutState();
            undoStack->undo();
            REQUIRE(timeline->checkConsistency());
        }
    }

    SECTION("Cutting grouped mix partners does not create an empty fragment")
    {
        REQUIRE(timeline->mixClip(cid4));
        REQUIRE(timeline->requestClipsGroup({cid3, cid4}));
        for (int target : {cid3, cid4}) {
            REQUIRE(TimelineFunctions::requestClipCut(timeline, target, 525));
            auto cutState = [&]() {
                REQUIRE(timeline->checkConsistency());
                REQUIRE(timeline->getClipsCount() == 7);
                REQUIRE(timeline->getClipPlaytime(cid3) == 25);
                REQUIRE(timeline->getClipPosition(cid4) == 525);
                REQUIRE(timeline->getClipPlaytime(cid4) == 15);
                REQUIRE(timeline->getMixDuration(cid4) == 7);
                int clone = timeline->getClipByPosition(tid2, 525, 0);
                REQUIRE(clone > -1);
                REQUIRE(clone != cid3);
                REQUIRE(timeline->getClipPlaytime(clone) == 7);
                REQUIRE(timeline->getGroupElements(clone) == std::unordered_set<int>{clone, cid4});
            };
            cutState();
            undoStack->undo();
            state2();
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getGroupElements(cid3) == std::unordered_set<int>{cid3, cid4});
            undoStack->redo();
            cutState();
            undoStack->undo();
        }
    }

    SECTION("Mix at start of a clip at frame 0 has no previous clip")
    {
        state0();
        int cid6 = -1;
        REQUIRE(timeline->requestClipInsertion(binId2, tid4, 0, cid6));
        // Clip at frame 0 with nothing after it: mix falls back to the clip start, where there is no previous clip.
        // The clip used to be mixed with itself, crashing on later undo
        REQUIRE_FALSE(timeline->mixClip(cid6));
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid4)->mixCount() == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
        undoStack->undo();
        state0();
    }

    SECTION("Check collisions with mixed clips during direct clip move")
    {
        state0();
        cid5 = -1;
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 540, cid5));
        REQUIRE(timeline->requestItemResize(cid5, 20, true, true));
        REQUIRE(timeline->mixClip(cid4, QStringLiteral("luma"), -1));
        auto mixed = [&]() {
            REQUIRE(timeline->getClipPosition(cid3) == 500);
            REQUIRE(timeline->getClipPlaytime(cid3) == 32);
            REQUIRE(timeline->getClipPosition(cid4) == 507);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
            REQUIRE(timeline->getClipPosition(cid5) == 540);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        };
        mixed();
        // cid3 ends at 532 on playlist 0, cid4 covers 507-540 on playlist 1
        REQUIRE_FALSE(timeline->requestClipMove(cid5, tid2, 532));
        REQUIRE(timeline->getClipPosition(cid5) == 540);
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid4, 538));
        undoStack->undo();
        mixed();
        // undo mix, resize and insert
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        state0();
    }

    SECTION("Create mix on color clips and group move")
    {
        state0();
        REQUIRE(timeline->mixClip(cid4));
        state2();
        // Move clip inside mix zone, should resize the mix
        auto g1 = std::unordered_set<int>({cid3, cid4});
        REQUIRE(timeline->requestClipsGroup(g1));
        // Move clip to another track, should delete mix
        REQUIRE(timeline->requestClipMove(cid4, tid4, 600));
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid4)->mixCount() == 1);
        undoStack->undo();
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid4)->mixCount() == 0);
        state2();
        // Move on same track
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        REQUIRE(timeline->requestClipMove(cid4, tid3, 800));
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        undoStack->undo();
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        state2();
        // Undo group
        undoStack->undo();
        // Undo mix
        undoStack->undo();
        state0();
    }

    SECTION("Create and delete mix on AV clips")
    {
        state0();
        // Clips are cid1 at 100, cid2 at 110
        REQUIRE(timeline->requestItemResize(cid2, 30, true, true) == 30);
        REQUIRE(timeline->requestItemResize(cid2, 10, false, true) == 10);
        REQUIRE(timeline->requestClipMove(cid2, tid2, 110));
        REQUIRE(timeline->mixClip(cid1));
        state1();
        undoStack->undo();
        state0();
        undoStack->redo();
        state1();
        undoStack->undo();
        state0();
    }

    SECTION("Create mix and move AV clips")
    {
        // CID 1 length=10, pos=100, CID2 length=10, pos=110
        // Default mix duration = 25 frames (12 before / 13 after)
        // Resize CID2 so that it has some space to expand left
        REQUIRE(timeline->requestItemResize(cid2, 30, true, true) == 30);
        REQUIRE(timeline->requestItemResize(cid2, 10, false, true) == 10);
        REQUIRE(timeline->requestClipMove(cid2, tid2, 110));
        // Resize clip, should resize the mix
        state0();
        REQUIRE(timeline->mixClip(cid2));
        state1();
        // Resize right clip, should resize the mix
        REQUIRE(timeline->requestItemResize(cid2, 15, false, true) == 15);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixCount() == 1);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        undoStack->undo();
        state1();
        // Resize left clip, should resize the mix
        REQUIRE(timeline->requestItemResize(cid1, 18, true, true) == 18);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixCount() == 1);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        undoStack->undo();
        state1();
        // Move clip outside mix zone, should delete the mix
        REQUIRE(timeline->requestClipMove(cid2, tid2, 200));
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixCount() == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
        undoStack->undo();
        state1();
        // Undo mix
        undoStack->undo();
        state0();
    }

    SECTION("Create chained mixes on AV clips")
    {
        // CID 1 length=10, pos=100, CID2 length=10, pos=110
        // Default mix duration = 25 frames (12 before / 13 after)
        // Resize CID2 so that it has some space to expand left
        REQUIRE(timeline->requestItemResize(cid2, 30, true, true) == 30);
        REQUIRE(timeline->requestItemResize(cid2, 10, false, true) == 10);
        REQUIRE(timeline->requestClipMove(cid2, tid2, 110));
        state0();

        // Create a third AV clip and make some space
        cid5 = -1;
        REQUIRE(timeline->requestClipInsertion(binId, tid2, 120, cid5));
        REQUIRE(timeline->requestItemResize(cid5, 30, true, true) == 30);
        REQUIRE(timeline->requestItemResize(cid5, 10, false, true) == 10);
        REQUIRE(timeline->requestClipMove(cid5, tid2, 120));
        audio5 = timeline->getClipSplitPartner(cid5);

        state0b();

        // CID 1 length=10, pos=100, CID2 length=20, pos=130, CID5 length=20, pos=130

        // Create mix between cid1 and cid2
        REQUIRE(timeline->getClipSubPlaylistIndex(cid2) == 0);
        REQUIRE(timeline->mixClip(cid1));
        state1b();
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid2) == false);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixIsReversed(audio2) == false);

        // Create mix between cid2 and cid5
        REQUIRE(timeline->mixClip(cid2));
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid2) == false);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid5) == true);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixIsReversed(audio2) == false);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixIsReversed(audio5) == true);
        // Undo cid5 mix
        undoStack->undo();

        state1b();
        // Undo cid2 mix
        undoStack->undo();
        // Undo cid5
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();

        state0();
    }

    SECTION("Create mix on color clip and test resize")
    {
        state0();
        // CID 3 length=20, pos=500, CID4 length=20, pos=520
        // Default mix duration = 25 frames (12 before / 13 after)
        REQUIRE(timeline->mixClip(cid3));
        state2();
        // Resize left clip, should resize the mix
        REQUIRE(timeline->requestItemResize(cid3, 24, true, true) == 24);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        undoStack->undo();
        state2();
        // Resize left clip outside mix zone, should delete the mix
        REQUIRE(timeline->requestItemResize(cid3, 4, true, true) == 20);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
        undoStack->undo();
        state2();
        // Resize right clip, should resize the mix
        REQUIRE(timeline->requestItemResize(cid4, 16, false, true) == 16);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        undoStack->undo();
        state2();
        // Resize right clip outside mix zone, should delete the mix
        REQUIRE(timeline->requestItemResize(cid4, 4, false, true) == 20);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
        undoStack->undo();
        state2();
        // Resize right clip before left clip, should limit the resize to left clip position
        REQUIRE(timeline->requestItemResize(cid4, 50, false, true) == 40);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        undoStack->undo();
        state2();
        // Resize left clip past right clip, should limit the resize to left clip position
        REQUIRE(timeline->requestItemResize(cid3, 100, true, true) == 40);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        undoStack->undo();
        state2();

        // Before mix: CID 3 length=20, pos=500, CID4 length=20, pos=520
        // Default mix duration = 25 frames (12 before / 13 after)

        // Resize left clip before right clip start, then right clip outside left clip, should delete the mix
        REQUIRE(timeline->requestItemResize(cid3, 20, true, true) == 20);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->requestItemResize(cid4, 20, false, true) == 20);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
        undoStack->undo();
        undoStack->undo();
        state2();

        // Resize right clip after left clip end, then left clip outside right clip, should delete the mix
        REQUIRE(timeline->requestItemResize(cid4, 20, false, true) == 20);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->requestItemResize(cid3, 20, true, true) == 20);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
        undoStack->undo();
        undoStack->undo();
        state2();

        undoStack->undo();
        state0();
    }

    SECTION("Create mix on AV clips and resize")
    {
        state0();
        // CID 1 length=10, pos=100, CID2 length=10, pos=110
        REQUIRE(timeline->getClipPlaytime(cid1) == 10);
        REQUIRE(timeline->getClipPlaytime(cid2) == 10);
        // Resize clip in to have some space for mix
        REQUIRE(timeline->requestItemResize(cid2, 90, true, true) == 90);
        REQUIRE(timeline->requestItemResize(cid2, 30, false, true) == 30);
        REQUIRE(timeline->requestClipMove(cid2, tid2, 130));
        REQUIRE(timeline->requestItemResize(cid1, 30, true, true) == 30);

        REQUIRE(timeline->mixClip(cid1));
        state3();
        // CID 1 length=30, pos=100, CID2 length=30, pos=130
        // Default mix duration = 25 frames (12 before / 13 after)
        // Resize left clip, should resize the mix
        REQUIRE(timeline->requestItemResize(cid1, 35, true, true) == 35);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixCount() == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid1) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid2) == 1);
        undoStack->undo();
        state3();
        // Resize left clip outside mix zone, should delete the mix
        REQUIRE(timeline->requestItemResize(cid1, 10, true, true) == 30);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixCount() == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid1) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid2) == 0);
        undoStack->undo();
        state3();
        // Resize right clip, should resize the mix
        REQUIRE(timeline->requestItemResize(cid2, 25, false, true) == 25);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixCount() == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid1) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid2) == 1);
        undoStack->undo();
        state3();
        // Resize right clip outside mix zone, should delete the mix
        REQUIRE(timeline->requestItemResize(cid2, 4, false, true) == 30);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixCount() == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid1) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid2) == 0);
        undoStack->undo();
        state3();
        // Resize right clip before left clip, should limit to left clip position
        REQUIRE(timeline->requestItemResize(cid2, 80, false, true) == 60);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixCount() == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid1) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid2) == 1);
        undoStack->undo();
        state3();
        // Resize left clip after right clip, should limit to right clip duration
        REQUIRE(timeline->requestItemResize(cid1, 80, true, true) == 60);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->mixCount() == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid1) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid2) == 1);
        undoStack->undo();
        state3();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        state0();
    }

    SECTION("Test chained mixes on color clips")
    {
        // Add 2 more color clips
        cid5 = -1;
        int cid6;
        int cid7;
        state0();
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 540, cid5));
        REQUIRE(timeline->requestItemResize(cid5, 20, true, true));
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 560, cid6));
        REQUIRE(timeline->requestItemResize(cid6, 40, true, true));
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 600, cid7));
        REQUIRE(timeline->requestItemResize(cid7, 20, true, true));
        audio5 = timeline->getClipSplitPartner(cid5);

        // Cid3 pos=500, duration=20
        // Cid4 pos=520, duration=20
        // Cid5 pos=540, duration=20
        // Cid6 pos=560, duration=40
        // Cid7 pos=600, duration=20

        // Mix 3 and 4
        REQUIRE(timeline->mixClip(cid3));
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);

        // Mix 6 and 7
        REQUIRE(timeline->mixClip(cid6));
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 1);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 2);

        // Mix 5 and 6
        REQUIRE(timeline->mixClip(cid5));
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 3);

        // Undo mix 5 and 6
        undoStack->undo();
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 1);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 2);

        // Undo mix 6 and 7
        undoStack->undo();
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);

        // Undo mix 3 and 4
        undoStack->undo();

        // Undo insert/resize ops
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();

        state0();
    }

    SECTION("Test chained mixes and clip resize")
    {
        // Add 3 more color clips
        cid5 = -1;
        int cid6;
        int cid7;
        state0();
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 540, cid5));
        REQUIRE(timeline->requestItemResize(cid5, 20, true, true));
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 560, cid6));
        REQUIRE(timeline->requestItemResize(cid6, 40, true, true));
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 600, cid7));
        REQUIRE(timeline->requestItemResize(cid7, 20, true, true));

        // Cid3 pos=500, duration=20
        // Cid4 pos=520, duration=20
        // Cid5 pos=540, duration=20
        // Cid6 pos=560, duration=40
        // Cid7 pos=600, duration=20

        auto mix0 = [&]() {
            REQUIRE(timeline->getClipsCount() == 9);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 0);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
        };

        auto mix1 = [&]() {
            REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 1);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 2);
        };

        // Mix 4 and 5
        REQUIRE(timeline->mixClip(cid4));
        mix0();

        // Mix 6 and 7
        REQUIRE(timeline->mixClip(cid6));
        mix1();

        // Test resize, should fail
        int clipSize = timeline->getClipPlaytime(cid5);
        REQUIRE(timeline->requestItemResize(cid5, 38, true, true) == clipSize);

        clipSize = timeline->getClipPlaytime(cid6);
        REQUIRE(timeline->requestItemResize(cid6, 60, false, true) == clipSize);

        // Undo second mix
        undoStack->undo();
        // Undo first mix
        undoStack->undo();

        // Undo insert/resize ops
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        state0();
    }

    SECTION("Test grouped mixes and clip resize")
    {
        // Add 3 more color clips
        cid5 = -1;
        int cid6;
        int cid7;
        state0();
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 540, cid5));
        REQUIRE(timeline->requestItemResize(cid5, 20, true, true));
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 560, cid6));
        REQUIRE(timeline->requestItemResize(cid6, 40, true, true));
        REQUIRE(timeline->requestClipInsertion(binId2, tid4, 520, cid7));
        REQUIRE(timeline->requestItemResize(cid7, 20, true, true));

        // Cid3 pos=500, duration=20
        // Cid4 pos=520, duration=20
        // Cid5 pos=540, duration=20
        // Cid6 pos=560, duration=40
        // Cid7 pos=520, duration=20 on tid4

        auto mix1 = [&]() {
            REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 0);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 2);
        };

        // Mix cid3 & cid4
        REQUIRE(timeline->mixClip(cid3));
        REQUIRE(timeline->getClipPlaytime(cid4) == 33);
        // Group cid4 and cid7
        auto g1 = std::unordered_set<int>({cid4, cid7});
        REQUIRE(timeline->requestClipsGroup(g1));
        // Try to resize the grouped cid7, should not be allowed
        REQUIRE(timeline->requestItemResize(cid7, 23, true, true) == -1);
        // Mix cid5 & cid6
        REQUIRE(timeline->mixClip(cid6));
        mix1();
        // Try to resize the grouped cid7, should not be allowed
        REQUIRE(timeline->requestItemResize(cid7, 23, true, true) == -1);

        // Undo second mix
        undoStack->undo();
        // Undo group
        undoStack->undo();
        // Undo first mix
        undoStack->undo();

        // Undo insert/resize ops
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        state0();
    }

    SECTION("Test chained mixes and check mix direction")
    {
        // Add 3 more color clips
        cid5 = -1;
        int cid6;
        int cid7;
        state0();
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 540, cid5));
        REQUIRE(timeline->requestItemResize(cid5, 20, true, true));
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 560, cid6));
        REQUIRE(timeline->requestItemResize(cid6, 40, true, true));
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 600, cid7));
        REQUIRE(timeline->requestItemResize(cid7, 20, true, true));

        // Cid3 pos=500, duration=20
        // Cid4 pos=520, duration=20
        // Cid5 pos=540, duration=20
        // Cid6 pos=560, duration=40
        // Cid7 pos=600, duration=20

        auto mix0 = [&]() {
            REQUIRE(timeline->getClipsCount() == 9);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 0);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid5) == false);
        };

        auto mix1 = [&]() {
            REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 0);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 2);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid5) == false);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid6) == true);
        };

        auto mix2 = [&]() {
            REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 1);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 3);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid5) == false);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid6) == true);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid7) == false);
        };

        auto mix3 = [&]() {
            REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 0);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 4);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid4) == false);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid5) == true);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid6) == false);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid7) == true);
        };

        // Mix 4 and 5
        REQUIRE(timeline->mixClip(cid4));
        mix0();

        // Mix 5 and 6
        REQUIRE(timeline->mixClip(cid5));
        mix1();

        // Mix 6 and 7
        REQUIRE(timeline->mixClip(cid6));
        mix2();

        // Mix 3 and 4, this will revert all subsequent mixes
        REQUIRE(timeline->mixClip(cid3));
        mix3();

        // Undo mix 3 and 4
        undoStack->undo();
        mix2();

        // Now switch mixes to Slide type
        timeline->switchComposition(cid7, QStringLiteral("slide"));
        timeline->switchComposition(cid6, QStringLiteral("slide"));
        timeline->switchComposition(cid5, QStringLiteral("slide"));
        mix2();

        // Mix 3 and 4, this will revert all subsequent mixes
        REQUIRE(timeline->mixClip(cid3));
        mix3();

        // Undo mix 3 and 4
        undoStack->undo();
        mix2();

        // Now switch mixes to Wipe type
        timeline->switchComposition(cid7, QStringLiteral("wipe"));
        timeline->switchComposition(cid6, QStringLiteral("wipe"));
        timeline->switchComposition(cid5, QStringLiteral("wipe"));
        mix2();

        // Mix 3 and 4, this will revert all subsequent mixes
        REQUIRE(timeline->mixClip(cid3));
        mix3();

        // Undo mix 3 and 4
        undoStack->undo();
        mix2();

        // Undo Wipe mix switch on cid5
        undoStack->undo();
        // Undo mix switch on cid6
        undoStack->undo();
        // Undo mix switch on cid7
        undoStack->undo();
        mix2();

        // Undo Slide mix switch on cid5
        undoStack->undo();
        // Undo mix switch on cid6
        undoStack->undo();
        // Undo mix switch on cid7
        undoStack->undo();
        mix2();

        // Undo mix 6 and 7
        undoStack->undo();
        mix1();
        // Undo mix 5 and 6
        undoStack->undo();
        mix0();
        // Undo mix 4 and 5
        undoStack->undo();

        // Undo insert/resize ops
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();

        state0();
    }

    SECTION("Test mixes track move and direction")
    {
        // Add 3 more av clips
        cid5 = -1;
        int cid6;
        int cid7;
        state0();
        REQUIRE(timeline->requestClipInsertion(binId, tid2, 540, cid5));
        REQUIRE(timeline->requestItemResize(cid5, 20, true, true));
        REQUIRE(timeline->requestClipInsertion(binId, tid2, 560, cid6));
        REQUIRE(timeline->requestItemResize(cid6, 40, true, true));
        REQUIRE(timeline->requestClipInsertion(binId, tid2, 600, cid7));
        REQUIRE(timeline->requestItemResize(cid7, 20, true, true));
        audio5 = timeline->getClipSplitPartner(cid5);

        // Cid3 pos=500, duration=20
        // Cid4 pos=520, duration=20
        // Cid5 pos=540, duration=20
        // Cid6 pos=560, duration=40
        // Cid7 pos=600, duration=20

        auto mix0 = [&]() {
            REQUIRE(timeline->getClipsCount() == 12);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 0);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid5) == false);
        };

        auto mix1 = [&]() {
            REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 0);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 2);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid5) == false);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid6) == true);
        };

        auto mix2 = [&]() {
            REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 1);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 3);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid5) == false);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid6) == true);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid7) == false);
        };

        auto mix3 = [&]() {
            REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 0);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 4);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid4) == false);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid5) == true);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid6) == false);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixIsReversed(cid7) == true);
        };

        auto mix3b = [&]() {
            REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 1);
            REQUIRE(timeline->getClipSubPlaylistIndex(cid7) == 0);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 0);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid4)->mixCount() == 4);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid4)->mixIsReversed(cid4) == false);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid4)->mixIsReversed(cid5) == true);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid4)->mixIsReversed(cid6) == false);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid4)->mixIsReversed(cid7) == true);
        };

        // Mix 4 and 5
        REQUIRE(timeline->mixClip(cid4));
        mix0();

        // Mix 5 and 6
        REQUIRE(timeline->mixClip(cid5));
        mix1();

        // Mix 6 and 7
        REQUIRE(timeline->mixClip(cid6));
        mix2();

        // Mix 3 and 4, this will revert all subsequent mixes
        REQUIRE(timeline->mixClip(cid3));
        mix3();

        // Move everything to another track
        auto g1 = std::unordered_set<int>({cid3, cid4, cid5, cid6, cid7});
        REQUIRE(timeline->requestClipsGroup(g1));

        REQUIRE(timeline->requestClipMove(cid3, tid4, 600));
        // Now clips 3, 4, 5, 6 and 7 will be on top video track. Check mixes have the correct direction
        mix3b();
        // Undo track move
        undoStack->undo();
        mix3();
        // Redo track move
        undoStack->redo();
        mix3b();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();

        // Undo insert/resize ops
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();

        state0();
    }
    SECTION("Test chained mixes and cuts")
    {
        // Add 2 more color clips
        cid5 = -1;
        int cid6;
        state0();
        REQUIRE(timeline->requestItemResize(cid4, 80, true, true));
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 600, cid5));
        REQUIRE(timeline->requestItemResize(cid5, 80, true, true));
        REQUIRE(timeline->requestClipInsertion(binId2, tid2, 680, cid6));
        REQUIRE(timeline->requestItemResize(cid6, 80, true, true));
        audio5 = timeline->getClipSplitPartner(cid5);

        // Cid3 pos=500, duration=20
        // Cid4 pos=520, duration=80
        // Cid5 pos=600, duration=80
        // Cid6 pos=680, duration=80

        // Mix 3 and 4
        REQUIRE(timeline->mixClip(cid3));
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);

        // Mix 4 and 5
        REQUIRE(timeline->mixClip(cid4));
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 2);

        // Mix 5 and 6
        REQUIRE(timeline->mixClip(cid5));
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 1);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 3);

        // Now try to cut mixes and check they are in the correct subplaylist

        // Cut a clip with mix start only
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid3, 505));
        // Get newly created cut clip
        int clone = timeline->getClipByPosition(tid2, 506, 0);
        // Ensure each clip is on the correct playlist
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(clone) == 0);
        // Undo cid3 cut
        undoStack->undo();
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        undoStack->redo();
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        undoStack->undo();

        // Cut a clip with mix at start and end
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid4, 540));
        // Get newly created cut clip
        clone = timeline->getClipByPosition(tid2, 540, 1);
        // Ensure each clip is on the correct playlist
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(clone) == 1);
        // Undo cid3 cut
        undoStack->undo();
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        undoStack->redo();
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(clone) == 1);
        undoStack->undo();

        // Cut a clip with mix at end
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid6, 710));
        // Get newly created cut clip
        clone = timeline->getClipByPosition(tid2, 710, 0);
        // Ensure each clip is on the correct playlist
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(clone) == 0);
        undoStack->undo();
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        undoStack->redo();
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(clone) == 0);
        undoStack->undo();

        // Undo mix 5 and 6
        undoStack->undo();
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 2);

        // Undo mix 4 and 5
        undoStack->undo();
        REQUIRE(timeline->getClipSubPlaylistIndex(cid3) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid4) == 1);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid5) == 0);
        REQUIRE(timeline->getClipSubPlaylistIndex(cid6) == 0);
        REQUIRE(KdenliveTests::getTrackById_const(timeline, tid2)->mixCount() == 1);

        // Undo mix 3 and 4
        undoStack->undo();

        // Undo insert/resize ops
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        state0();
    }
    timeline.reset();
    pCore->projectManager()->closeCurrentDocument(false, false);
}
