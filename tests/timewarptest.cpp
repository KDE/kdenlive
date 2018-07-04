#include "test_utils.hpp"

using namespace fakeit;
Mlt::Profile profile_timewarp;

TEST_CASE("Test of timewarping", "[Timewarp]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    // We also mock timeline object to spy few functions and mock others
    TimelineItemModel tim(new Mlt::Profile(), undoStack);
    Mock<TimelineItemModel> timMock(tim);
    TimelineItemModel &tt = timMock.get();
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET(timMock);

    QString binId = createProducer(profile_timewarp, "red", binModel);
    QString binId2 = createProducer(profile_timewarp, "blue", binModel);
    QString binId3 = createProducerWithSound(profile_timewarp, binModel);

    int cid1 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);
    int cid2 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);
    int cid3 = ClipModel::construct(timeline, binId3, -1, PlaylistState::VideoOnly);

    timeline->m_allClips[cid1]->m_endlessResize = false;
    timeline->m_allClips[cid2]->m_endlessResize = false;
    timeline->m_allClips[cid3]->m_endlessResize = false;

    SECTION("Timewarping orphan clip")
    {
        int originalDuration = timeline->getClipPlaytime(cid3);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid3) == -1);
        REQUIRE(timeline->getClipSpeed(cid3) == 1.);

        std::function<bool(void)> undo = []() { return true; };
        std::function<bool(void)> redo = []() { return true; };

        REQUIRE(timeline->requestClipTimeWarp(cid3, 0.1, undo, redo));

        CHECK_UPDATE(TimelineModel::SpeedRole);
        REQUIRE(timeline->getClipSpeed(cid3) == 0.1);
        INFO(timeline->m_allClips[cid3]->getIn());
        INFO(timeline->m_allClips[cid3]->getOut());
        REQUIRE(timeline->getClipPlaytime(cid3) == originalDuration / 0.1);

        undo();

        CHECK_UPDATE(TimelineModel::SpeedRole);
        REQUIRE(timeline->getClipSpeed(cid3) == 1.);
        REQUIRE(timeline->getClipPlaytime(cid3) == originalDuration);

        redo();

        CHECK_UPDATE(TimelineModel::SpeedRole);
        REQUIRE(timeline->getClipSpeed(cid3) == 0.1);
        REQUIRE(timeline->getClipPlaytime(cid3) == originalDuration / 0.1);

        std::function<bool(void)> undo2 = []() { return true; };
        std::function<bool(void)> redo2 = []() { return true; };
        REQUIRE(timeline->requestClipTimeWarp(cid3, 1.2, undo2, redo2));

        CHECK_UPDATE(TimelineModel::SpeedRole);
        REQUIRE(timeline->getClipSpeed(cid3) == 1.2);
        REQUIRE(timeline->getClipPlaytime(cid3) == int(originalDuration / 1.2));

        undo2();
        CHECK_UPDATE(TimelineModel::SpeedRole);
        REQUIRE(timeline->getClipSpeed(cid3) == 0.1);
        REQUIRE(timeline->getClipPlaytime(cid3) == originalDuration / 0.1);

        undo();
        CHECK_UPDATE(TimelineModel::SpeedRole);
        REQUIRE(timeline->getClipSpeed(cid3) == 1.);
        REQUIRE(timeline->getClipPlaytime(cid3) == originalDuration);

        // Finally, we test that setting a very high speed isn't possible.
        // Specifically, it must be impossible to make the clip shorter than one frame
        int curLength = timeline->getClipPlaytime(cid3);

        // This is the limit, should work
        REQUIRE(timeline->requestClipTimeWarp(cid3, double(curLength), undo2, redo2));

        CHECK_UPDATE(TimelineModel::SpeedRole);
        REQUIRE(timeline->getClipSpeed(cid3) == double(curLength));
        REQUIRE(timeline->getClipPlaytime(cid3) == 1);

        // This is the higher than the limit, should not work
        REQUIRE_FALSE(timeline->requestClipTimeWarp(cid3, double(curLength) + 0.1, undo2, redo2));
    }
}
