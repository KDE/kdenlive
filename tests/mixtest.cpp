#include "catch.hpp"
#include "doc/docundostack.hpp"
#include "test_utils.hpp"

#include <QString>
#include <cmath>
#include <iostream>
#include <tuple>
#include <unordered_set>

#include "definitions.h"
#define private public
#define protected public
#include "core.h"

using namespace fakeit;
Mlt::Profile profile_mix;

TEST_CASE("Simple Mix", "[SameTrackMix]")
{
    Logger::clear();
    // Create timeline
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    // We also mock timeline object to spy few functions and mock others
    TimelineItemModel tim(&profile_mix, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

        // Create a request
    int tid1 = TrackModel::construct(timeline, -1, -1, QString(), true);
    int tid3 = TrackModel::construct(timeline, -1, -1, QString(), true);
    int tid2 = TrackModel::construct(timeline);
    int tid4 = TrackModel::construct(timeline);
    
    // Create clip with audio
    QString binId = createProducerWithSound(profile_mix, binModel, 50);
    
    // Create clip with audio
    QString binId2 = createProducer(profile_mix, "red", binModel, 50, false);
    // Setup insert stream data
    QMap <int, QString>audioInfo;
    audioInfo.insert(1,QStringLiteral("stream1"));
    timeline->m_binAudioTargets = audioInfo;

    // Create AV clip 1
    int cid1;
    int cid2;
    int cid3;
    int cid4;
    REQUIRE(timeline->requestClipInsertion(binId, tid2, 100, cid1));
    REQUIRE(timeline->requestItemResize(cid1, 10, true, true));
    
    // Create AV clip 2
    REQUIRE(timeline->requestClipInsertion(binId, tid2, 110, cid2));
    REQUIRE(timeline->requestItemResize(cid2, 10, true, true));
    REQUIRE(timeline->requestClipMove(cid2, tid2, 110));
    
    // Create color clip 1
    REQUIRE(timeline->requestClipInsertion(binId2, tid2, 500, cid3));
    REQUIRE(timeline->requestItemResize(cid3, 20, true, true));
    REQUIRE(timeline->requestClipInsertion(binId2, tid2, 520, cid4));
    REQUIRE(timeline->requestItemResize(cid4, 20, true, true));
    
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
        REQUIRE(timeline->m_allClips[cid4]->getSubPlaylistIndex() == 0);
        REQUIRE(timeline->getTrackById_const(tid1)->mixCount() == 0);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 0);
    };
    
    auto state1 = [&]() {
        REQUIRE(timeline->getClipsCount() == 6);
        REQUIRE(timeline->getClipPlaytime(cid1) > 10);
        REQUIRE(timeline->getClipPosition(cid1) == 100);
        REQUIRE(timeline->getClipPlaytime(cid2) > 10);
        REQUIRE(timeline->getClipPosition(cid2) < 110);
        REQUIRE(timeline->getTrackById_const(tid3)->mixCount() == 1);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
    };
    
    auto state3 = [&]() {
        REQUIRE(timeline->getClipsCount() == 6);
        REQUIRE(timeline->getClipPlaytime(cid1) > 30);
        REQUIRE(timeline->getClipPosition(cid1) == 100);
        REQUIRE(timeline->getClipPlaytime(cid2) > 30);
        REQUIRE(timeline->getClipPosition(cid2) < 130);
        REQUIRE(timeline->getTrackById_const(tid3)->mixCount() == 1);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
    };
    
    auto state2 = [&]() {
        REQUIRE(timeline->getClipsCount() == 6);
        REQUIRE(timeline->getClipPlaytime(cid3) > 20);
        REQUIRE(timeline->getClipPosition(cid3) == 500);
        REQUIRE(timeline->getClipPlaytime(cid4) > 20);
        REQUIRE(timeline->getClipPosition(cid4) < 520);
        REQUIRE(timeline->m_allClips[cid4]->getSubPlaylistIndex() == 1);
        REQUIRE(timeline->getTrackById_const(tid1)->mixCount() == 0);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
    };

    SECTION("Create and delete mix on color clips")
    {
        state0();
        REQUIRE(timeline->mixClip(cid4));
        state2();
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
        // Move clip inside mix zone, should resize the mix
        REQUIRE(timeline->requestClipMove(cid4, tid2, 506));
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
        undoStack->undo();
        state2();
        // Move clip outside mix zone, should delete the mix and move it back to playlist 0
        REQUIRE(timeline->requestClipMove(cid4, tid2, 600));
        REQUIRE(timeline->m_allClips[cid4]->getSubPlaylistIndex() == 0);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 0);
        undoStack->undo();
        state2();
        // Move clip to another track, should delete mix
        REQUIRE(timeline->requestClipMove(cid4, tid4, 600));
        REQUIRE(timeline->m_allClips[cid4]->getSubPlaylistIndex() == 0);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 0);
        REQUIRE(timeline->getTrackById_const(tid4)->mixCount() == 0);
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
        // Move clip inside mix zone, should resize the mix
        REQUIRE(timeline->requestClipMove(cid3, tid2, 502));
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
        undoStack->undo();
        state2();
        // Move clip outside mix zone, should delete the mix
        REQUIRE(timeline->requestClipMove(cid3, tid2, 450));
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 0);
        undoStack->undo();
        state2();
        // Move clip to another track, should delete mix
        REQUIRE(timeline->requestClipMove(cid3, tid4, 600));
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 0);
        REQUIRE(timeline->getTrackById_const(tid4)->mixCount() == 0);
        undoStack->undo();
        state2();
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
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 0);
        REQUIRE(timeline->getTrackById_const(tid4)->mixCount() == 1);
        undoStack->undo();
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
        REQUIRE(timeline->getTrackById_const(tid4)->mixCount() == 0);
        state2();
        // Move on same track
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
        REQUIRE(timeline->requestClipMove(cid4, tid3, 800));
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
        undoStack->undo();
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
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
        REQUIRE(timeline->requestItemResize(cid2, 30, true, true) == 30);
        REQUIRE(timeline->requestItemResize(cid2, 10, false, true) == 10);
        REQUIRE(timeline->requestClipMove(cid2, tid2, 110));
        REQUIRE(timeline->mixClip(cid2));
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
        //state1();
        // Move clip inside mix zone, should resize the mix
        REQUIRE(timeline->requestClipMove(cid2, tid2, 101));
        REQUIRE(timeline->getTrackById_const(tid3)->mixCount() == 1);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
        undoStack->undo();
        state1();
        // Move clip outside mix zone, should delete the mix
        REQUIRE(timeline->requestClipMove(cid2, tid2, 200));
        REQUIRE(timeline->getTrackById_const(tid3)->mixCount() == 0);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 0);
        undoStack->undo();
        state1();
        // Undo mix
        undoStack->undo();
        state0();
    }
    
    SECTION("Create mix on color clip and resize")
    {
        state0();
        REQUIRE(timeline->mixClip(cid4));
        state2();
        // CID 3 length=20, pos=500, CID4 length=20, pos=520
        // Default mix duration = 25 frames (12 before / 13 after)
        // Resize left clip, should resize the mix
        REQUIRE(timeline->requestItemResize(cid3, 16, true, true) == 16);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
        REQUIRE(timeline->m_allClips[cid3]->getSubPlaylistIndex() == 0);
        REQUIRE(timeline->m_allClips[cid4]->getSubPlaylistIndex() == 1);
        undoStack->undo();
        state2();
        // Resize left clip outside mix zone, should delete the mix
        REQUIRE(timeline->requestItemResize(cid3, 4, true, true) == 4);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 0);
        REQUIRE(timeline->m_allClips[cid3]->getSubPlaylistIndex() == 0);
        REQUIRE(timeline->m_allClips[cid4]->getSubPlaylistIndex() == 0);
        undoStack->undo();
        state2();
        // Resize right clip, should resize the mix
        REQUIRE(timeline->requestItemResize(cid4, 16, false, true) == 16);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
        REQUIRE(timeline->m_allClips[cid3]->getSubPlaylistIndex() == 0);
        REQUIRE(timeline->m_allClips[cid4]->getSubPlaylistIndex() == 1);
        undoStack->undo();
        state2();
        // Resize right clip outside mix zone, should delete the mix
        REQUIRE(timeline->requestItemResize(cid4, 4, false, true) == 4);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 0);
        REQUIRE(timeline->m_allClips[cid3]->getSubPlaylistIndex() == 0);
        REQUIRE(timeline->m_allClips[cid4]->getSubPlaylistIndex() == 0);
        undoStack->undo();
        state2();
        undoStack->undo();
        state0();
    }
    
    SECTION("Create mix on AV clips and resize")
    {
        state0();
        // CID 1 length=10, pos=100, CID2 length=10, pos=110
        REQUIRE(timeline->m_allClips[cid1]->getPlaytime() == 10);
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == 10);
        REQUIRE(timeline->requestItemResize(cid2, 50, true, true) == 50);
        REQUIRE(timeline->requestItemResize(cid2, 30, false, true) == 30);
        REQUIRE(timeline->requestClipMove(cid2, tid2, 130));
        REQUIRE(timeline->requestItemResize(cid1, 30, true, true) == 30);
        
        REQUIRE(timeline->mixClip(cid2));
        state3();
        // CID 1 length=30, pos=100, CID2 length=30, pos=130
        // Default mix duration = 25 frames (12 before / 13 after)
        // Resize left clip, should resize the mix
        REQUIRE(timeline->requestItemResize(cid1, 25, true, true) == 25);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
        REQUIRE(timeline->getTrackById_const(tid3)->mixCount() == 1);
        REQUIRE(timeline->m_allClips[cid1]->getSubPlaylistIndex() == 0);
        REQUIRE(timeline->m_allClips[cid2]->getSubPlaylistIndex() == 1);
        undoStack->undo();
        state3();
        // Resize left clip outside mix zone, should delete the mix
        REQUIRE(timeline->requestItemResize(cid1, 10, true, true) == 10);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 0);
        REQUIRE(timeline->getTrackById_const(tid3)->mixCount() == 0);
        REQUIRE(timeline->m_allClips[cid1]->getSubPlaylistIndex() == 0);
        REQUIRE(timeline->m_allClips[cid2]->getSubPlaylistIndex() == 0);
        undoStack->undo();
        state3();
        // Resize right clip, should resize the mix
        REQUIRE(timeline->requestItemResize(cid2, 25, false, true) == 25);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
        REQUIRE(timeline->getTrackById_const(tid3)->mixCount() == 1);
        REQUIRE(timeline->m_allClips[cid1]->getSubPlaylistIndex() == 0);
        REQUIRE(timeline->m_allClips[cid2]->getSubPlaylistIndex() == 1);
        undoStack->undo();
        state3();
        // Resize right clip outside mix zone, should delete the mix
        REQUIRE(timeline->requestItemResize(cid2, 4, false, true) == 4);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 0);
        REQUIRE(timeline->getTrackById_const(tid3)->mixCount() == 0);
        REQUIRE(timeline->m_allClips[cid1]->getSubPlaylistIndex() == 0);
        REQUIRE(timeline->m_allClips[cid2]->getSubPlaylistIndex() == 0);
        undoStack->undo();
        state3();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        state0();
    }
    
    binModel->clean();
    pCore->m_projectManager = nullptr;
    Logger::print_trace();
}
