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
    // Setup insert stream data
    QMap <int, QString>audioInfo;
    audioInfo.insert(1,QStringLiteral("stream1"));
    timeline->m_binAudioTargets = audioInfo;

    // Create clip
    int cid1;
    int cid2;
    REQUIRE(timeline->requestClipInsertion(binId, tid2, 100, cid1));
    
    // Resize clip
    REQUIRE(timeline->requestItemResize(cid1, 10, true, true));
    REQUIRE(timeline->getClipPlaytime(cid1) == 10);
    REQUIRE(timeline->getClipPosition(cid1) == 100);
    REQUIRE(timeline->requestClipInsertion(binId, tid2, 110, cid2));

    // Resize clip
    REQUIRE(timeline->requestItemResize(cid2, 10, false, true));
    REQUIRE(timeline->getClipPlaytime(cid2) == 10);
    REQUIRE(timeline->requestClipMove(cid2, tid2, 110));

    SECTION("Create and delete mix")
    {
        REQUIRE(timeline->getClipPosition(cid2) == 110);
        REQUIRE(timeline->mixClip(cid2));
        REQUIRE(timeline->getClipPlaytime(cid1) > 10);
        REQUIRE(timeline->getClipPosition(cid2) != 110);
    }

    Logger::print_trace();
}
