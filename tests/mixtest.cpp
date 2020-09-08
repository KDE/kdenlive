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
    int tid2 = TrackModel::construct(timeline);
    
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
    REQUIRE(timeline->requestItemResize(cid2, 10, false, true));
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
        qDebug()<<"HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH\nCLIPS: "<<timeline->getClipPosition(cid1)<<"-"<<timeline->getClipPlaytime(cid1)<<" / CID2: "<<timeline->getClipPosition(cid2)<<"-"<<timeline->getClipPlaytime(cid2);
        REQUIRE(timeline->getClipPlaytime(cid1) > 10);
        REQUIRE(timeline->getClipPosition(cid1) == 100);
        REQUIRE(timeline->getClipPlaytime(cid2) > 10);
        REQUIRE(timeline->getClipPosition(cid2) < 110);
        REQUIRE(timeline->getTrackById_const(tid1)->mixCount() == 1);
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
    
    SECTION("Create and delete mix on AV clips")
    {
        state0();
        REQUIRE(timeline->mixClip(cid2));
        state1();
        undoStack->undo();
        state0();
        undoStack->redo();
        state1();
        undoStack->undo();
        state0();
    }
    
    SECTION("Create mix and move color clips")
    {
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
        undoStack->undo();
        state0();
    }
    
    SECTION("Create mix and move AV clips")
    {
        state0();
        REQUIRE(timeline->mixClip(cid2));
        state1();
        // Move clip inside mix zone, should resize the mix
        REQUIRE(timeline->requestClipMove(cid2, tid2, 100));
        REQUIRE(timeline->getTrackById_const(tid1)->mixCount() == 1);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 1);
        undoStack->undo();
        state1();
        // Move clip outside mix zone, should delete the mix
        REQUIRE(timeline->requestClipMove(cid2, tid2, 200));
        REQUIRE(timeline->getTrackById_const(tid1)->mixCount() == 0);
        REQUIRE(timeline->getTrackById_const(tid2)->mixCount() == 0);
        undoStack->undo();
        state1();
        undoStack->undo();
        state0();
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
    Logger::print_trace();
}
