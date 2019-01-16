#include "test_utils.hpp"

using namespace fakeit;
std::default_random_engine g(42);
Mlt::Profile profile_model;

TEST_CASE("Basic creation/deletion of a track", "[TrackModel]")
{
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
    TimelineItemModel tim(new Mlt::Profile(), undoStack);
    Mock<TimelineItemModel> timMock(tim);
    TimelineItemModel &tt = timMock.get();
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    Fake(Method(timMock, adjustAssetRange));

    // This is faked to allow to count calls
    Fake(Method(timMock, _resetView));

    int id1 = TrackModel::construct(timeline);
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 1);
    REQUIRE(timeline->getTrackPosition(id1) == 0);
    // In the current implementation, when a track is added/removed, the model is notified with _resetView
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET(timMock);

    int id2 = TrackModel::construct(timeline);
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 2);
    REQUIRE(timeline->getTrackPosition(id2) == 1);
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET(timMock);

    int id3 = TrackModel::construct(timeline);
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 3);
    REQUIRE(timeline->getTrackPosition(id3) == 2);
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET(timMock);

    int id4;
    REQUIRE(timeline->requestTrackInsertion(1, id4));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 4);
    REQUIRE(timeline->getTrackPosition(id1) == 0);
    REQUIRE(timeline->getTrackPosition(id4) == 1);
    REQUIRE(timeline->getTrackPosition(id2) == 2);
    REQUIRE(timeline->getTrackPosition(id3) == 3);
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET(timMock);

    // Test deletion
    REQUIRE(timeline->requestTrackDeletion(id3));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 3);
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET(timMock);

    REQUIRE(timeline->requestTrackDeletion(id1));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 2);
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET(timMock);

    REQUIRE(timeline->requestTrackDeletion(id4));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 1);
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET(timMock);

    REQUIRE(timeline->requestTrackDeletion(id2));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 0);
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET(timMock);

    SECTION("Delete a track with groups")
    {
        int tid1, tid2;
        REQUIRE(timeline->requestTrackInsertion(-1, tid1));
        REQUIRE(timeline->requestTrackInsertion(-1, tid2));
        REQUIRE(timeline->checkConsistency());

        QString binId = createProducer(profile_model, "red", binModel);
        int length = 20;
        int cid1, cid2, cid3, cid4;
        REQUIRE(timeline->requestClipInsertion(binId, tid1, 2, cid1));
        REQUIRE(timeline->requestClipInsertion(binId, tid2, 0, cid2));
        REQUIRE(timeline->requestClipInsertion(binId, tid2, length, cid3));
        REQUIRE(timeline->requestClipInsertion(binId, tid2, 2 * length, cid4));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipsCount() == 4);
        REQUIRE(timeline->getTracksCount() == 2);

        auto g1 = std::unordered_set<int>({cid1, cid3});
        auto g2 = std::unordered_set<int>({cid2, cid4});
        auto g3 = std::unordered_set<int>({cid1, cid4});
        REQUIRE(timeline->requestClipsGroup(g1));
        REQUIRE(timeline->requestClipsGroup(g2));
        REQUIRE(timeline->requestClipsGroup(g3));
        REQUIRE(timeline->checkConsistency());

        REQUIRE(timeline->requestTrackDeletion(tid1));
        REQUIRE(timeline->getClipsCount() == 3);
        REQUIRE(timeline->getTracksCount() == 1);
        REQUIRE(timeline->checkConsistency());
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
}

TEST_CASE("Basic creation/deletion of a clip", "[ClipModel]")
{

    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), guideModel, undoStack);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    QString binId = createProducer(profile_model, "red", binModel);
    QString binId2 = createProducer(profile_model, "green", binModel);

    REQUIRE(timeline->getClipsCount() == 0);
    int id1 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    REQUIRE(timeline->getClipsCount() == 1);
    REQUIRE(timeline->checkConsistency());

    int id2 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);
    REQUIRE(timeline->getClipsCount() == 2);
    REQUIRE(timeline->checkConsistency());

    int id3 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    REQUIRE(timeline->getClipsCount() == 3);
    REQUIRE(timeline->checkConsistency());

    // Test deletion
    REQUIRE(timeline->requestItemDeletion(id2));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getClipsCount() == 2);
    REQUIRE(timeline->requestItemDeletion(id3));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getClipsCount() == 1);
    REQUIRE(timeline->requestItemDeletion(id1));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getClipsCount() == 0);
    binModel->clean();
    pCore->m_projectManager = nullptr;
}

TEST_CASE("Clip manipulation", "[ClipModel]")
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

    Fake(Method(timMock, adjustAssetRange));

    // This is faked to allow to count calls
    Fake(Method(timMock, _resetView));
    Fake(Method(timMock, _beginInsertRows));
    Fake(Method(timMock, _beginRemoveRows));
    Fake(Method(timMock, _endInsertRows));
    Fake(Method(timMock, _endRemoveRows));

    QString binId = createProducer(profile_model, "red", binModel);
    QString binId2 = createProducer(profile_model, "blue", binModel);
    QString binId3 = createProducer(profile_model, "green", binModel);

    int cid1 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);
    int tid3 = TrackModel::construct(timeline);
    int cid2 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);
    int cid3 = ClipModel::construct(timeline, binId3, -1, PlaylistState::VideoOnly);
    int cid4 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);

    Verify(Method(timMock, _resetView)).Exactly(3_Times);
    RESET(timMock);

    // for testing purposes, we make sure the clip will behave as regular clips
    // (ie their size is fixed, we cannot resize them past their original size)
    timeline->m_allClips[cid1]->m_endlessResize = false;
    timeline->m_allClips[cid2]->m_endlessResize = false;
    timeline->m_allClips[cid3]->m_endlessResize = false;
    timeline->m_allClips[cid4]->m_endlessResize = false;

    SECTION("Insert a clip in a track and change track")
    {
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);

        REQUIRE(timeline->getClipTrackId(cid1) == -1);
        REQUIRE(timeline->getClipPosition(cid1) == -1);

        int pos = 10;
        REQUIRE(timeline->requestClipMove(cid1, tid1, pos));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == pos);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);
        // Check that the model was correctly notified
        CHECK_INSERT(Once);

        pos = 1;
        REQUIRE(timeline->requestClipMove(cid1, tid2, pos));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == pos);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
        CHECK_MOVE(Once);

        // Check conflicts
        int pos2 = binModel->getClipByBinID(binId)->frameDuration();
        REQUIRE(timeline->requestClipMove(cid2, tid1, pos2));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == pos2);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        CHECK_INSERT(Once);

        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, pos2 + 2));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == pos);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == pos2);
        CHECK_MOVE(Once);

        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, pos2 - 2));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == pos);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == pos2);
        CHECK_MOVE(Once);

        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == pos2);
        CHECK_MOVE(Once);
    }

    int length = binModel->getClipByBinID(binId)->frameDuration();
    SECTION("Insert consecutive clips")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        CHECK_INSERT(Once);

        REQUIRE(timeline->requestClipMove(cid2, tid1, length));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == length);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        CHECK_INSERT(Once);
    }

    SECTION("Resize orphan clip")
    {
        REQUIRE(timeline->getClipPlaytime(cid2) == length);
        REQUIRE(timeline->requestItemResize(cid2, 5, true) == 5);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(binModel->getClipByBinID(binId)->frameDuration() == length);
        auto inOut = std::pair<int, int>{0, 4};
        REQUIRE(timeline->m_allClips[cid2]->getInOut() == inOut);
        REQUIRE(timeline->getClipPlaytime(cid2) == 5);
        REQUIRE(timeline->requestItemResize(cid2, 10, false) == -1);
        REQUIRE(timeline->requestItemResize(cid2, length + 1, true) == -1);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipPlaytime(cid2) == 5);
        REQUIRE(timeline->getClipPlaytime(cid2) == 5);
        REQUIRE(timeline->requestItemResize(cid2, 2, false) == 2);
        REQUIRE(timeline->checkConsistency());
        inOut = std::pair<int, int>{3, 4};
        REQUIRE(timeline->m_allClips[cid2]->getInOut() == inOut);
        REQUIRE(timeline->getClipPlaytime(cid2) == 2);
        REQUIRE(timeline->requestItemResize(cid2, length, true) == -1);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipPlaytime(cid2) == 2);
        CAPTURE(timeline->m_allClips[cid2]->m_producer->get_in());
        REQUIRE(timeline->requestItemResize(cid2, length - 2, true) == -1);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->requestItemResize(cid2, length - 3, true) == length - 3);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipPlaytime(cid2) == length - 3);
    }

    SECTION("Resize inserted clips")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->checkConsistency());
        CHECK_INSERT(Once);

        REQUIRE(timeline->requestItemResize(cid1, 5, true) == 5);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipPlaytime(cid1) == 5);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        CHECK_RESIZE(Once);

        REQUIRE(timeline->requestClipMove(cid2, tid1, 5));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(binModel->getClipByBinID(binId)->frameDuration() == length);
        CHECK_INSERT(Once);

        REQUIRE(timeline->requestItemResize(cid1, 6, true) == -1);
        REQUIRE(timeline->requestItemResize(cid1, 6, false) == -1);
        REQUIRE(timeline->checkConsistency());
        NO_OTHERS();

        REQUIRE(timeline->requestItemResize(cid2, length - 5, false) == length - 5);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipPosition(cid2) == 10);
        CHECK_RESIZE(Once);

        REQUIRE(timeline->requestItemResize(cid1, 10, true) == 10);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        CHECK_RESIZE(Once);
    }

    SECTION("Change track of resized clips")
    {
        // // REQUIRE(timeline->allowClipMove(cid2, tid1, 5));
        REQUIRE(timeline->requestClipMove(cid2, tid1, 5));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);

        // // REQUIRE(timeline->allowClipMove(cid1, tid2, 10));
        REQUIRE(timeline->requestClipMove(cid1, tid2, 10));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);

        REQUIRE(timeline->requestItemResize(cid1, 5, false) == 5);
        REQUIRE(timeline->checkConsistency());

        // // REQUIRE(timeline->allowClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);
    }

    SECTION("Clip Move")
    {
        REQUIRE(timeline->requestClipMove(cid2, tid1, 5));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == 5);

        REQUIRE(timeline->requestClipMove(cid1, tid1, 5 + length));
        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5 + length);
            REQUIRE(timeline->getClipPosition(cid2) == 5);
        };
        state();

        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, 3 + length));
        state();

        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, 0));
        state();

        REQUIRE(timeline->requestClipMove(cid2, tid1, 0));
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5 + length);
            REQUIRE(timeline->getClipPosition(cid2) == 0);
        };
        state2();

        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, 0));
        state2();

        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, length - 5));
        state2();

        REQUIRE(timeline->requestClipMove(cid1, tid1, length));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length);
        REQUIRE(timeline->getClipPosition(cid2) == 0);

        REQUIRE(timeline->requestItemResize(cid2, length - 5, true) == length - 5);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length);
        REQUIRE(timeline->getClipPosition(cid2) == 0);

        REQUIRE(timeline->requestClipMove(cid1, tid1, length - 5));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length - 5);
        REQUIRE(timeline->getClipPosition(cid2) == 0);

        REQUIRE(timeline->requestItemResize(cid2, length - 10, false) == length - 10);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length - 5);
        REQUIRE(timeline->getClipPosition(cid2) == 5);

        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length - 5);
        REQUIRE(timeline->getClipPosition(cid2) == 5);

        REQUIRE(timeline->requestClipMove(cid2, tid1, 0));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length - 5);
        REQUIRE(timeline->getClipPosition(cid2) == 0);
    }

    SECTION("Move and resize")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestItemResize(cid1, length - 2, false) == length - 2);
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPlaytime(cid1) == length - 2);
        };
        state();

        // try to resize past the left end
        REQUIRE(timeline->requestItemResize(cid1, length, false) == -1);
        state();

        REQUIRE(timeline->requestItemResize(cid1, length - 4, true) == length - 4);
        REQUIRE(timeline->requestClipMove(cid2, tid1, length - 4 + 1));
        REQUIRE(timeline->requestItemResize(cid2, length - 2, false) == length - 2);
        REQUIRE(timeline->requestClipMove(cid2, tid1, length - 4 + 1));
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPlaytime(cid1) == length - 4);
            REQUIRE(timeline->getClipPosition(cid2) == length - 4 + 1);
            REQUIRE(timeline->getClipPlaytime(cid2) == length - 2);
        };
        state2();

        // the gap between the two clips is 1 frame, we try to resize them by 2 frames
        REQUIRE(timeline->requestItemResize(cid1, length - 2, true) == -1);
        state2();
        REQUIRE(timeline->requestItemResize(cid2, length, false) == -1);
        state2();

        REQUIRE(timeline->requestClipMove(cid2, tid1, length - 4));
        auto state3 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPlaytime(cid1) == length - 4);
            REQUIRE(timeline->getClipPosition(cid2) == length - 4);
            REQUIRE(timeline->getClipPlaytime(cid2) == length - 2);
        };
        state3();

        // Now the gap is 0 frames, the resize should still fail
        REQUIRE(timeline->requestItemResize(cid1, length - 2, true) == -1);
        state3();
        REQUIRE(timeline->requestItemResize(cid2, length, false) == -1);
        state3();

        // We move cid1 out of the way
        REQUIRE(timeline->requestClipMove(cid1, tid2, 0));
        // now resize should work
        REQUIRE(timeline->requestItemResize(cid1, length - 2, true) == length - 2);
        REQUIRE(timeline->requestItemResize(cid2, length, false) == length);
        REQUIRE(timeline->checkConsistency());
    }

    SECTION("Group move")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid2, tid1, length + 3));
        REQUIRE(timeline->requestClipMove(cid3, tid1, 2 * length + 5));
        REQUIRE(timeline->requestClipMove(cid4, tid2, 4));

        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipTrackId(cid3) == tid1);
        REQUIRE(timeline->getClipTrackId(cid4) == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(timeline->getClipPosition(cid2) == length + 3);
        REQUIRE(timeline->getClipPosition(cid3) == 2 * length + 5);
        REQUIRE(timeline->getClipPosition(cid4) == 4);

        // check that move is possible without groups
        REQUIRE(timeline->requestClipMove(cid3, tid1, 2 * length + 3));
        REQUIRE(timeline->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline->checkConsistency());
        // check that move is possible without groups
        REQUIRE(timeline->requestClipMove(cid4, tid2, 9));
        REQUIRE(timeline->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline->checkConsistency());

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipTrackId(cid4) == tid2);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == length + 3);
            REQUIRE(timeline->getClipPosition(cid3) == 2 * length + 5);
            REQUIRE(timeline->getClipPosition(cid4) == 4);
        };
        state();

        // grouping
        REQUIRE(timeline->requestClipsGroup({cid1, cid3}));
        REQUIRE(timeline->requestClipsGroup({cid1, cid4}));

        // move left is now forbidden, because clip1 is at position 0
        REQUIRE_FALSE(timeline->requestClipMove(cid3, tid1, 2 * length + 3));
        state();

        // this move is impossible, because clip1 runs into clip2
        REQUIRE_FALSE(timeline->requestClipMove(cid4, tid2, 9));
        state();

        // this move is possible
        REQUIRE(timeline->requestClipMove(cid3, tid1, 2 * length + 8));
        auto state1 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid3) == 0);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipTrackId(cid4) == tid2);
            REQUIRE(timeline->getClipPosition(cid1) == 3);
            REQUIRE(timeline->getClipPosition(cid2) == length + 3);
            REQUIRE(timeline->getClipPosition(cid3) == 2 * length + 8);
            REQUIRE(timeline->getClipPosition(cid4) == 7);
        };
        state1();

        // this move is possible
        REQUIRE(timeline->requestClipMove(cid1, tid2, 8));
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 2);
            REQUIRE(timeline->getTrackClipsCount(tid3) == 1);
            REQUIRE(timeline->getClipTrackId(cid1) == tid2);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipTrackId(cid3) == tid2);
            REQUIRE(timeline->getClipTrackId(cid4) == tid3);
            REQUIRE(timeline->getClipPosition(cid1) == 8);
            REQUIRE(timeline->getClipPosition(cid2) == length + 3);
            REQUIRE(timeline->getClipPosition(cid3) == 2 * length + 5 + 8);
            REQUIRE(timeline->getClipPosition(cid4) == 4 + 8);
        };
        state2();

        undoStack->undo();
        state1();

        undoStack->redo();
        state2();

        REQUIRE(timeline->requestClipMove(cid1, tid1, 3));
        state1();
    }

    SECTION("Group move consecutive clips")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 7));
        REQUIRE(timeline->requestClipMove(cid2, tid1, 7 + length));
        REQUIRE(timeline->requestClipMove(cid3, tid1, 7 + 2 * length));
        REQUIRE(timeline->requestClipMove(cid4, tid1, 7 + 3 * length));
        REQUIRE(timeline->requestClipsGroup({cid1, cid2, cid3, cid4}));

        auto state = [&](int tid, int start) {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid) == 4);
            int i = 0;
            for (int cid : std::vector<int>({cid1, cid2, cid3, cid4})) {
                REQUIRE(timeline->getClipTrackId(cid) == tid);
                REQUIRE(timeline->getClipPosition(cid) == start + i * length);
                REQUIRE(timeline->getClipPlaytime(cid) == length);
                i++;
            }
        };
        state(tid1, 7);

        auto check_undo = [&](int target, int tid, int oldTid) {
            state(tid, target);
            undoStack->undo();
            state(oldTid, 7);
            undoStack->redo();
            state(tid, target);
            undoStack->undo();
            state(oldTid, 7);
        };

        REQUIRE(timeline->requestClipMove(cid1, tid1, 6));
        qDebug() << "state1";
        state(tid1, 6);
        undoStack->undo();
        state(tid1, 7);
        undoStack->redo();
        state(tid1, 6);
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        qDebug() << "state2";
        state(tid1, 0);
        undoStack->undo();
        state(tid1, 6);
        undoStack->redo();
        state(tid1, 0);
        undoStack->undo();
        state(tid1, 6);
        undoStack->undo();
        state(tid1, 7);

        REQUIRE(timeline->requestClipMove(cid3, tid1, 1 + 2 * length));
        qDebug() << "state3";
        check_undo(1, tid1, tid1);

        REQUIRE(timeline->requestClipMove(cid4, tid1, 4 + 3 * length));
        qDebug() << "state4";
        check_undo(4, tid1, tid1);

        REQUIRE(timeline->requestClipMove(cid4, tid1, 11 + 3 * length));
        qDebug() << "state5";
        check_undo(11, tid1, tid1);

        REQUIRE(timeline->requestClipMove(cid2, tid1, 13 + length));
        qDebug() << "state6";
        check_undo(13, tid1, tid1);

        REQUIRE(timeline->requestClipMove(cid1, tid1, 20));
        qDebug() << "state7";
        check_undo(20, tid1, tid1);

        REQUIRE(timeline->requestClipMove(cid4, tid1, 7 + 4 * length));
        qDebug() << "state8";
        check_undo(length + 7, tid1, tid1);

        REQUIRE(timeline->requestClipMove(cid2, tid1, 7 + 2 * length));
        qDebug() << "state9";
        check_undo(length + 7, tid1, tid1);

        REQUIRE(timeline->requestClipMove(cid1, tid1, 7 + length));
        qDebug() << "state10";
        check_undo(length + 7, tid1, tid1);

        REQUIRE(timeline->requestClipMove(cid2, tid2, 8 + length));
        qDebug() << "state11";
        check_undo(8, tid2, tid1);
    }

    SECTION("Group move to unavailable track")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 10));
        REQUIRE(timeline->requestClipMove(cid2, tid2, 12));
        REQUIRE(timeline->requestClipsGroup({cid1, cid2}));
        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid2);
            REQUIRE(timeline->getClipPosition(cid1) == 10);
            REQUIRE(timeline->getClipPosition(cid2) == 12);
        };
        state();

        REQUIRE_FALSE(timeline->requestClipMove(cid2, tid1, 10));
        state();
        REQUIRE_FALSE(timeline->requestClipMove(cid2, tid1, 100));
        state();
        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid3, 100));
        state();
    }

    SECTION("Group move with non-consecutive track ids")
    {
        int tid5 = TrackModel::construct(timeline);
        int cid6 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
        int tid6 = TrackModel::construct(timeline);
        REQUIRE(tid5 + 1 != tid6);

        REQUIRE(timeline->requestClipMove(cid1, tid5, 10));
        REQUIRE(timeline->requestClipMove(cid2, tid5, length + 10));
        REQUIRE(timeline->requestClipsGroup({cid1, cid2}));
        auto state = [&](int t) {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(t) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == t);
            REQUIRE(timeline->getClipTrackId(cid2) == t);
            REQUIRE(timeline->getClipPosition(cid1) == 10);
            REQUIRE(timeline->getClipPosition(cid2) == 10 + length);
        };
        state(tid5);
        REQUIRE(timeline->requestClipMove(cid1, tid6, 10));
        state(tid6);
    }

    SECTION("Movement of AV groups")
    {
        int tid6b = TrackModel::construct(timeline, -1, -1, QString(), true);
        int tid6 = TrackModel::construct(timeline, -1, -1, QString(), true);
        int tid5 = TrackModel::construct(timeline);
        int tid5b = TrackModel::construct(timeline);

        QString binId3 = createProducerWithSound(profile_model, binModel);

        int cid6 = -1;
        REQUIRE(timeline->requestClipInsertion(binId3, tid5, 3, cid6, true, true, false));
        int cid7 = timeline->m_groups->getSplitPartner(cid6);

        auto state = [&](int pos) {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid5) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid6) == 1);
            REQUIRE(timeline->getClipTrackId(cid6) == tid5);
            REQUIRE(timeline->getClipTrackId(cid7) == tid6);
            REQUIRE(timeline->getClipPosition(cid6) == pos);
            REQUIRE(timeline->getClipPosition(cid7) == pos);
            REQUIRE(timeline->getClipPtr(cid6)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid7)->clipState() == PlaylistState::AudioOnly);
        };
        state(3);

        // simple translation on the right
        REQUIRE(timeline->requestClipMove(cid6, tid5, 10, true, true, true));

        state(10);
        undoStack->undo();
        state(3);
        undoStack->redo();
        state(10);

        // simple translation on the left, moving the audio clip this time
        REQUIRE(timeline->requestClipMove(cid7, tid6, 1, true, true, true));
        state(1);
        undoStack->undo();
        state(10);
        undoStack->redo();
        state(1);

        // change track, moving video
        REQUIRE(timeline->requestClipMove(cid6, tid5b, 7, true, true, true));
        auto state2 = [&](int pos) {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid5b) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid6b) == 1);
            REQUIRE(timeline->getClipTrackId(cid6) == tid5b);
            REQUIRE(timeline->getClipTrackId(cid7) == tid6b);
            REQUIRE(timeline->getClipPosition(cid6) == pos);
            REQUIRE(timeline->getClipPosition(cid7) == pos);
            REQUIRE(timeline->getClipPtr(cid6)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid7)->clipState() == PlaylistState::AudioOnly);
        };
        state2(7);
        undoStack->undo();
        state(1);
        undoStack->redo();
        state2(7);

        // change track, moving audio
        REQUIRE(timeline->requestClipMove(cid7, tid6b, 2, true, true, true));
        state2(2);
        undoStack->undo();
        state2(7);
        undoStack->redo();
        state2(2);

        undoStack->undo();
        undoStack->undo();
        state(1);

        // Switching audio and video, going to the extra track
        REQUIRE(timeline->requestClipMove(cid7, tid5b, 2, true, true, true) == 0);
        // This test is invalid. AV clips cannot be switched between audio and video clips anymore
        /*auto state3 = [&](int pos) {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid5b) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid6b) == 1);
            REQUIRE(timeline->getClipTrackId(cid6) == tid6b);
            REQUIRE(timeline->getClipTrackId(cid7) == tid5b);
            REQUIRE(timeline->getClipPosition(cid6) == pos);
            REQUIRE(timeline->getClipPosition(cid7) == pos);
            REQUIRE(timeline->getClipPtr(cid7)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid6)->clipState() == PlaylistState::AudioOnly);
        };
        state3(2);
        undoStack->undo();
        state(1);
        undoStack->redo();
        state3(2);
        undoStack->undo();
        state(1);*/

        // Switching audio and video, switching tracks in place
        REQUIRE(timeline->requestClipMove(cid6, tid6, 1, true, true, true) == 0);
        /*auto state4 = [&](int pos) {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid5) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid6) == 1);
            REQUIRE(timeline->getClipTrackId(cid6) == tid6);
            REQUIRE(timeline->getClipTrackId(cid7) == tid5);
            REQUIRE(timeline->getClipPosition(cid6) == pos);
            REQUIRE(timeline->getClipPosition(cid7) == pos);
            REQUIRE(timeline->getClipPtr(cid7)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid6)->clipState() == PlaylistState::AudioOnly);
        };
        state4(1);
        undoStack->undo();
        state(1);
        undoStack->redo();
        state4(1);*/
    }

    SECTION("Clip copy")
    {
        int cid6 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
        int l = timeline->getClipPlaytime(cid6);
        REQUIRE(timeline->requestItemResize(cid6, l - 3, true, true, -1) == l - 3);
        REQUIRE(timeline->requestItemResize(cid6, l - 7, false, true, -1) == l - 7);

        int newId;

        std::function<bool(void)> undo = []() { return true; };
        std::function<bool(void)> redo = []() { return true; };
        REQUIRE(TimelineFunctions::copyClip(timeline, cid6, newId, PlaylistState::VideoOnly, undo, redo));
        REQUIRE(timeline->m_allClips[cid6]->binId() == timeline->m_allClips[newId]->binId());
        // TODO check effects
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
}

TEST_CASE("Check id unicity", "[ClipModel]")
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

    QString binId = createProducer(profile_model, "red", binModel);

    std::vector<int> track_ids;
    std::unordered_set<int> all_ids;

    std::bernoulli_distribution coin(0.5);

    const int nbr = 20;

    for (int i = 0; i < nbr; i++) {
        if (coin(g)) {
            int tid = TrackModel::construct(timeline);
            REQUIRE(all_ids.count(tid) == 0);
            all_ids.insert(tid);
            track_ids.push_back(tid);
            REQUIRE(timeline->getTracksCount() == track_ids.size());
        } else {
            int cid = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
            REQUIRE(all_ids.count(cid) == 0);
            all_ids.insert(cid);
            REQUIRE(timeline->getClipsCount() == all_ids.size() - track_ids.size());
        }
    }

    REQUIRE(timeline->checkConsistency());
    REQUIRE(all_ids.size() == nbr);
    REQUIRE(all_ids.size() != track_ids.size());
    binModel->clean();
    pCore->m_projectManager = nullptr;
}

TEST_CASE("Undo and Redo", "[ClipModel]")
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

    QString binId = createProducer(profile_model, "red", binModel);
    QString binId2 = createProducer(profile_model, "blue", binModel);

    int cid1 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);
    int cid2 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);

    timeline->m_allClips[cid1]->m_endlessResize = false;
    timeline->m_allClips[cid2]->m_endlessResize = false;

    int length = 20;
    int nclips = timeline->m_allClips.size();

    SECTION("requestCreateClip")
    {
        // an invalid clip id shouln't get created
        {
            int temp;
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            REQUIRE_FALSE(timeline->requestClipCreation("impossible bin id", temp, PlaylistState::VideoOnly, 1., undo, redo));
        }

        auto state0 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->m_allClips.size() == nclips);
        };
        state0();

        QString binId3 = createProducer(profile_model, "green", binModel);
        int cid3;
        {
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            REQUIRE(timeline->requestClipCreation(binId3, cid3, PlaylistState::VideoOnly, 1., undo, redo));
            pCore->pushUndo(undo, redo, QString());
        }

        auto state1 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->m_allClips.size() == nclips + 1);
            REQUIRE(timeline->getClipPlaytime(cid3) == length);
            REQUIRE(timeline->getClipTrackId(cid3) == -1);
        };
        state1();

        QString binId4 = binId3 + "/1/10";
        int cid4;
        {
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            REQUIRE(timeline->requestClipCreation(binId4, cid4, PlaylistState::VideoOnly, 1., undo, redo));
            pCore->pushUndo(undo, redo, QString());
        }

        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->m_allClips.size() == nclips + 2);
            REQUIRE(timeline->getClipPlaytime(cid4) == 10);
            REQUIRE(timeline->getClipTrackId(cid4) == -1);
            auto inOut = std::pair<int, int>({1, 10});
            REQUIRE(timeline->m_allClips.at(cid4)->getInOut() == inOut);
            REQUIRE(timeline->getClipPlaytime(cid3) == length);
            REQUIRE(timeline->getClipTrackId(cid3) == -1);
        };
        state2();
        undoStack->undo();
        state1();
        undoStack->undo();
        state0();
        undoStack->redo();
        state1();
        undoStack->redo();
        state2();
    }

    SECTION("requestInsertClip")
    {
        auto state0 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->m_allClips.size() == nclips);
        };
        state0();

        QString binId3 = createProducer(profile_model, "green", binModel);
        int cid3;
        REQUIRE(timeline->requestClipInsertion(binId3, tid1, 12, cid3, true));

        auto state1 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->m_allClips.size() == nclips + 1);
            REQUIRE(timeline->getClipPlaytime(cid3) == length);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipPosition(cid3) == 12);
        };
        state1();

        QString binId4 = binId3 + "/1/10";
        int cid4;
        REQUIRE(timeline->requestClipInsertion(binId4, tid2, 17, cid4, true));

        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->m_allClips.size() == nclips + 2);
            REQUIRE(timeline->getClipPlaytime(cid4) == 10);
            REQUIRE(timeline->getClipTrackId(cid4) == tid2);
            REQUIRE(timeline->getClipPosition(cid4) == 17);
            auto inOut = std::pair<int, int>({1, 10});
            REQUIRE(timeline->m_allClips.at(cid4)->getInOut() == inOut);
            REQUIRE(timeline->getClipPlaytime(cid3) == length);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipPosition(cid3) == 12);
        };
        state2();
        undoStack->undo();
        state1();
        undoStack->undo();
        state0();
        undoStack->redo();
        state1();
        undoStack->redo();
        state2();
    }
    int init_index = undoStack->index();

    SECTION("Basic move undo")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);
        CHECK_INSERT(Once);

        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(undoStack->index() == init_index + 2);
        // Move on same track does not trigger insert/remove row
        CHECK_MOVE(0);

        undoStack->undo();
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);
        CHECK_MOVE(0);

        undoStack->redo();
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(undoStack->index() == init_index + 2);
        CHECK_MOVE(0);

        undoStack->undo();
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);
        CHECK_MOVE(0);

        REQUIRE(timeline->requestClipMove(cid1, tid1, 2 * length));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 2 * length);
        REQUIRE(undoStack->index() == init_index + 2);
        CHECK_MOVE(0);

        undoStack->undo();
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);
        CHECK_MOVE(0);

        undoStack->redo();
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 2 * length);
        REQUIRE(undoStack->index() == init_index + 2);
        CHECK_MOVE(0);

        undoStack->undo();
        CHECK_MOVE(0);
        undoStack->undo();
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
        REQUIRE(timeline->getClipTrackId(cid1) == -1);
        REQUIRE(undoStack->index() == init_index);
        CHECK_REMOVE(Once);
    }

    SECTION("Basic resize orphan clip undo")
    {
        REQUIRE(timeline->getClipPlaytime(cid2) == length);

        REQUIRE(timeline->requestItemResize(cid2, length - 5, true) == length - 5);
        REQUIRE(undoStack->index() == init_index + 1);
        REQUIRE(timeline->getClipPlaytime(cid2) == length - 5);

        REQUIRE(timeline->requestItemResize(cid2, length - 10, false) == length - 10);
        REQUIRE(undoStack->index() == init_index + 2);
        REQUIRE(timeline->getClipPlaytime(cid2) == length - 10);

        REQUIRE(timeline->requestItemResize(cid2, length, false) == -1);
        REQUIRE(undoStack->index() == init_index + 2);
        REQUIRE(timeline->getClipPlaytime(cid2) == length - 10);

        undoStack->undo();
        REQUIRE(undoStack->index() == init_index + 1);
        REQUIRE(timeline->getClipPlaytime(cid2) == length - 5);

        undoStack->redo();
        REQUIRE(undoStack->index() == init_index + 2);
        REQUIRE(timeline->getClipPlaytime(cid2) == length - 10);

        undoStack->undo();
        REQUIRE(undoStack->index() == init_index + 1);
        REQUIRE(timeline->getClipPlaytime(cid2) == length - 5);

        undoStack->undo();
        REQUIRE(undoStack->index() == init_index);
        REQUIRE(timeline->getClipPlaytime(cid2) == length);
    }
    SECTION("Basic resize inserted clip undo")
    {
        REQUIRE(timeline->getClipPlaytime(cid2) == length);

        auto check = [&](int pos, int l) {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipPlaytime(cid2) == l);
            REQUIRE(timeline->getClipPosition(cid2) == pos);
        };
        REQUIRE(timeline->requestClipMove(cid2, tid1, 5));
        INFO("Test 1");
        check(5, length);
        REQUIRE(undoStack->index() == init_index + 1);

        REQUIRE(timeline->requestItemResize(cid2, length - 5, true) == length - 5);
        INFO("Test 2");
        check(5, length - 5);
        REQUIRE(undoStack->index() == init_index + 2);

        REQUIRE(timeline->requestItemResize(cid2, length - 10, false) == length - 10);
        INFO("Test 3");
        check(10, length - 10);
        REQUIRE(undoStack->index() == init_index + 3);

        REQUIRE(timeline->requestItemResize(cid2, length, false) == -1);
        INFO("Test 4");
        check(10, length - 10);
        REQUIRE(undoStack->index() == init_index + 3);

        undoStack->undo();
        INFO("Test 5");
        check(5, length - 5);
        REQUIRE(undoStack->index() == init_index + 2);

        undoStack->redo();
        INFO("Test 6");
        check(10, length - 10);
        REQUIRE(undoStack->index() == init_index + 3);

        undoStack->undo();
        INFO("Test 7");
        check(5, length - 5);
        REQUIRE(undoStack->index() == init_index + 2);

        undoStack->undo();
        INFO("Test 8");
        check(5, length);
        REQUIRE(undoStack->index() == init_index + 1);
    }
    SECTION("Clip Insertion Undo")
    {
        QString binId3 = createProducer(profile_model, "red", binModel);

        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        auto state1 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(undoStack->index() == init_index + 1);
        };
        state1();

        int cid3;
        REQUIRE_FALSE(timeline->requestClipInsertion(binId3, tid1, 5, cid3));
        state1();

        REQUIRE_FALSE(timeline->requestClipInsertion(binId3, tid1, 6, cid3));
        state1();

        REQUIRE(timeline->requestClipInsertion(binId3, tid1, 5 + length, cid3));
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getClipPosition(cid3) == 5 + length);
            REQUIRE(timeline->m_allClips[cid3]->isValid());
            REQUIRE(undoStack->index() == init_index + 2);
        };
        state2();

        REQUIRE(timeline->requestClipMove(cid3, tid1, 10 + length));
        auto state3 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getClipPosition(cid3) == 10 + length);
            REQUIRE(undoStack->index() == init_index + 3);
        };
        state3();

        REQUIRE(timeline->requestItemResize(cid3, 1, true) == 1);
        auto state4 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getClipPlaytime(cid3) == 1);
            REQUIRE(timeline->getClipPosition(cid3) == 10 + length);
            REQUIRE(undoStack->index() == init_index + 4);
        };
        state4();

        undoStack->undo();
        state3();

        undoStack->undo();
        state2();

        undoStack->undo();
        state1();

        undoStack->redo();
        state2();

        undoStack->redo();
        state3();

        undoStack->redo();
        state4();

        undoStack->undo();
        state3();

        undoStack->undo();
        state2();

        undoStack->undo();
        state1();
    }

    SECTION("Clip Deletion undo")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        auto state1 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(undoStack->index() == init_index + 1);
        };
        state1();

        int nbClips = timeline->getClipsCount();
        REQUIRE(timeline->requestItemDeletion(cid1));
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
            REQUIRE(timeline->getClipsCount() == nbClips - 1);
            REQUIRE(undoStack->index() == init_index + 2);
        };
        state2();

        undoStack->undo();
        state1();

        undoStack->redo();
        state2();

        undoStack->undo();
        state1();
    }

    SECTION("Track insertion undo")
    {
        int nb_tracks = timeline->getTracksCount();
        std::map<int, int> orig_trackPositions, final_trackPositions;
        for (const auto &it : timeline->m_iteratorTable) {
            int track = it.first;
            int pos = timeline->getTrackPosition(track);
            orig_trackPositions[track] = pos;
            if (pos >= 1) pos++;
            final_trackPositions[track] = pos;
        }
        auto checkPositions = [&](const std::map<int, int> &pos) {
            for (const auto &p : pos) {
                REQUIRE(timeline->getTrackPosition(p.first) == p.second);
            }
        };
        checkPositions(orig_trackPositions);
        int new_tid;
        REQUIRE(timeline->requestTrackInsertion(1, new_tid));
        checkPositions(final_trackPositions);

        undoStack->undo();
        checkPositions(orig_trackPositions);

        undoStack->redo();
        checkPositions(final_trackPositions);

        undoStack->undo();
        checkPositions(orig_trackPositions);
    }

    SECTION("Track deletion undo")
    {
        int nb_clips = timeline->getClipsCount();
        int nb_tracks = timeline->getTracksCount();
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        auto state1 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(undoStack->index() == init_index + 1);
            REQUIRE(timeline->getClipsCount() == nb_clips);
            REQUIRE(timeline->getTracksCount() == nb_tracks);
        };
        state1();

        REQUIRE(timeline->requestTrackDeletion(tid1));
        REQUIRE(timeline->getClipsCount() == nb_clips - 1);
        REQUIRE(timeline->getTracksCount() == nb_tracks - 1);

        undoStack->undo();
        state1();

        undoStack->redo();
        REQUIRE(timeline->getClipsCount() == nb_clips - 1);
        REQUIRE(timeline->getTracksCount() == nb_tracks - 1);

        undoStack->undo();
        state1();
    }

    int clipCount = timeline->m_allClips.size();
    SECTION("Clip creation and resize")
    {
        int cid6;
        auto state0 = [&]() {
            REQUIRE(timeline->m_allClips.size() == clipCount);
            REQUIRE(timeline->checkConsistency());
        };
        state0();

        {
            std::function<bool(void)> undo = []() { return true; };
            std::function<bool(void)> redo = []() { return true; };
            REQUIRE(timeline->requestClipCreation(binId, cid6, PlaylistState::VideoOnly, 1., undo, redo));
            pCore->pushUndo(undo, redo, QString());
        }
        int l = timeline->getClipPlaytime(cid6);

        auto state1 = [&]() {
            REQUIRE(timeline->m_allClips.size() == clipCount + 1);
            REQUIRE(timeline->isClip(cid6));
            REQUIRE(timeline->getClipTrackId(cid6) == -1);
            REQUIRE(timeline->getClipPlaytime(cid6) == l);
        };
        state1();

        {
            std::function<bool(void)> undo = []() { return true; };
            std::function<bool(void)> redo = []() { return true; };
            REQUIRE(timeline->requestItemResize(cid6, l - 5, true, true, undo, redo, false));
            pCore->pushUndo(undo, redo, QString());
        }
        auto state2 = [&]() {
            REQUIRE(timeline->m_allClips.size() == clipCount + 1);
            REQUIRE(timeline->isClip(cid6));
            REQUIRE(timeline->getClipTrackId(cid6) == -1);
            REQUIRE(timeline->getClipPlaytime(cid6) == l - 5);
        };
        state2();

        {
            std::function<bool(void)> undo = []() { return true; };
            std::function<bool(void)> redo = []() { return true; };
            REQUIRE(timeline->requestClipMove(cid6, tid1, 7, true, true, undo, redo));
            pCore->pushUndo(undo, redo, QString());
        }
        auto state3 = [&]() {
            REQUIRE(timeline->m_allClips.size() == clipCount + 1);
            REQUIRE(timeline->isClip(cid6));
            REQUIRE(timeline->getClipTrackId(cid6) == tid1);
            REQUIRE(timeline->getClipPosition(cid6) == 7);
            REQUIRE(timeline->getClipPlaytime(cid6) == l - 5);
        };
        state3();

        {
            std::function<bool(void)> undo = []() { return true; };
            std::function<bool(void)> redo = []() { return true; };
            REQUIRE(timeline->requestItemResize(cid6, l - 6, false, true, undo, redo, false));
            pCore->pushUndo(undo, redo, QString());
        }
        auto state4 = [&]() {
            REQUIRE(timeline->m_allClips.size() == clipCount + 1);
            REQUIRE(timeline->isClip(cid6));
            REQUIRE(timeline->getClipTrackId(cid6) == tid1);
            REQUIRE(timeline->getClipPosition(cid6) == 8);
            REQUIRE(timeline->getClipPlaytime(cid6) == l - 6);
        };
        state4();

        undoStack->undo();
        state3();
        undoStack->undo();
        state2();
        undoStack->undo();
        state1();
        undoStack->undo();
        state0();
        undoStack->redo();
        state1();
        undoStack->redo();
        state2();
        undoStack->redo();
        state3();
        undoStack->redo();
        state4();
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
}

TEST_CASE("Snapping", "[Snapping]")
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

    QString binId = createProducer(profile_model, "red", binModel, 50);
    QString binId2 = createProducer(profile_model, "blue", binModel);

    int tid1 = TrackModel::construct(timeline);
    int cid1 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int tid2 = TrackModel::construct(timeline);
    int cid2 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);
    int cid3 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);

    timeline->m_allClips[cid1]->m_endlessResize = false;
    timeline->m_allClips[cid2]->m_endlessResize = false;
    timeline->m_allClips[cid3]->m_endlessResize = false;

    int length = timeline->getClipPlaytime(cid1);
    int length2 = timeline->getClipPlaytime(cid2);
    SECTION("getBlankSizeNearClip")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));

        // before
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, false) == 0);
        // after
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, true) == INT_MAX);
        REQUIRE(timeline->requestClipMove(cid1, tid1, 10));
        // before
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, false) == 10);
        // after
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, true) == INT_MAX);
        REQUIRE(timeline->requestClipMove(cid2, tid1, 25 + length));
        // before
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, false) == 10);
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid2, false) == 15);
        // after
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, true) == 15);
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid2, true) == INT_MAX);

        REQUIRE(timeline->requestClipMove(cid2, tid1, 10 + length));
        // before
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, false) == 10);
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid2, false) == 0);
        // after
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, true) == 0);
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid2, true) == INT_MAX);
    }
    SECTION("Snap move to a single clip")
    {
        int beg = 30;
        // in the absence of other clips, a valid move shouldn't be modified
        for (int snap = -1; snap <= 5; ++snap) {
            REQUIRE(timeline->suggestClipMove(cid2, tid2, beg, snap) == beg);
            REQUIRE(timeline->suggestClipMove(cid2, tid2, beg + length, snap) == beg + length);
            REQUIRE(timeline->checkConsistency());
        }

        // We add a clip in first track to create snap points
        REQUIRE(timeline->requestClipMove(cid1, tid1, beg));

        // Now a clip in second track should snap to beginning
        auto check_snap = [&](int pos, int perturb, int snap) {
            if (snap >= perturb) {
                REQUIRE(timeline->suggestClipMove(cid2, tid2, pos + perturb, snap) == pos);
                REQUIRE(timeline->suggestClipMove(cid2, tid2, pos - perturb, snap) == pos);
            } else {
                REQUIRE(timeline->suggestClipMove(cid2, tid2, pos + perturb, snap) == pos + perturb);
                REQUIRE(timeline->suggestClipMove(cid2, tid2, pos - perturb, snap) == pos - perturb);
            }
        };
        for (int snap = -1; snap <= 5; ++snap) {
            for (int perturb = 0; perturb <= 6; ++perturb) {
                // snap to beginning
                check_snap(beg, perturb, snap);
                check_snap(beg + length, perturb, snap);
                // snap to end
                check_snap(beg - length2, perturb, snap);
                check_snap(beg + length - length2, perturb, snap);
                REQUIRE(timeline->checkConsistency());
            }
        }

        // Same test, but now clip is moved in position 0 first
        REQUIRE(timeline->requestClipMove(cid2, tid2, 0));
        for (int snap = -1; snap <= 5; ++snap) {
            for (int perturb = 0; perturb <= 6; ++perturb) {
                // snap to beginning
                check_snap(beg, perturb, snap);
                check_snap(beg + length, perturb, snap);
                // snap to end
                check_snap(beg - length2, perturb, snap);
                check_snap(beg + length - length2, perturb, snap);
                REQUIRE(timeline->checkConsistency());
            }
        }
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
}

TEST_CASE("Advanced trimming operations", "[Trimming]")
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

    QString binId = createProducer(profile_model, "red", binModel);
    QString binId2 = createProducer(profile_model, "blue", binModel);
    QString binId3 = createProducerWithSound(profile_model, binModel);

    int cid1 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);
    int tid3 = TrackModel::construct(timeline);
    // Add an audio track
    int tid4 = TrackModel::construct(timeline, -1, -1, QString(), true);
    int cid2 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);
    int cid3 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int cid4 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int cid5 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int cid6 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int cid7 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);

    int audio1 = ClipModel::construct(timeline, binId3, -1, PlaylistState::VideoOnly);
    int audio2 = ClipModel::construct(timeline, binId3, -1, PlaylistState::VideoOnly);
    int audio3 = ClipModel::construct(timeline, binId3, -1, PlaylistState::VideoOnly);

    timeline->m_allClips[cid1]->m_endlessResize = false;
    timeline->m_allClips[cid2]->m_endlessResize = false;
    timeline->m_allClips[cid3]->m_endlessResize = false;
    timeline->m_allClips[cid4]->m_endlessResize = false;
    timeline->m_allClips[cid5]->m_endlessResize = false;
    timeline->m_allClips[cid6]->m_endlessResize = false;
    timeline->m_allClips[cid7]->m_endlessResize = false;

    SECTION("Clip splitting")
    {
        // Trivial split
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        int l = timeline->getClipPlaytime(cid2);
        REQUIRE(timeline->requestItemResize(cid2, l - 3, true) == l - 3);
        REQUIRE(timeline->requestItemResize(cid2, l - 5, false) == l - 5);
        REQUIRE(timeline->requestClipMove(cid2, tid1, l));
        REQUIRE(timeline->requestClipMove(cid3, tid1, l + l - 5));
        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid1) == l);
            REQUIRE(timeline->getClipPlaytime(cid2) == l - 5);
            REQUIRE(timeline->getClipPlaytime(cid3) == l);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == l);
            REQUIRE(timeline->getClipPosition(cid3) == l + l - 5);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 2);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l - 4);
        };
        state();

        REQUIRE_FALSE(TimelineFunctions::requestClipCut(timeline, cid2, 0));
        REQUIRE_FALSE(TimelineFunctions::requestClipCut(timeline, cid2, 5 * l));
        REQUIRE_FALSE(TimelineFunctions::requestClipCut(timeline, cid2, l));
        REQUIRE_FALSE(TimelineFunctions::requestClipCut(timeline, cid2, l + l - 5));
        state();

        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid2, l + 4));
        int splitted = timeline->getClipByPosition(tid1, l + 5);
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid1) == l);
            REQUIRE(timeline->getClipPlaytime(cid2) == 4);
            REQUIRE(timeline->getClipPlaytime(splitted) == l - 9);
            REQUIRE(timeline->getClipPlaytime(cid3) == l);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == l);
            REQUIRE(timeline->getClipPosition(splitted) == l + 4);
            REQUIRE(timeline->getClipPosition(cid3) == l + l - 5);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 2);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == 5);
            REQUIRE(timeline->getClipPtr(splitted)->getIn() == 6);
            REQUIRE(timeline->getClipPtr(splitted)->getOut() == l - 4);
        };
        state2();

        undoStack->undo();
        state();

        undoStack->redo();
        state2();
    }

    SECTION("Split and resize")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        int l = timeline->getClipPlaytime(cid1);
        timeline->m_allClips[cid1]->m_endlessResize = false;

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipPlaytime(cid1) == l);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
        };
        state();

        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid1, 9));
        int splitted = timeline->getClipByPosition(tid1, 10);
        timeline->m_allClips[splitted]->m_endlessResize = false;
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(splitted) == tid1);
            REQUIRE(timeline->getClipPlaytime(cid1) == 4);
            REQUIRE(timeline->getClipPlaytime(splitted) == l - 4);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getClipPosition(splitted) == 9);
        };
        state2();

        REQUIRE(timeline->requestClipMove(splitted, tid2, 9, true, true));
        REQUIRE(timeline->requestItemResize(splitted, l - 3, true, true) == -1);
        REQUIRE(timeline->requestItemResize(splitted, l, false, true) == l);
        REQUIRE(timeline->requestItemResize(cid1, 5, false, true) == -1);
        REQUIRE(timeline->requestItemResize(cid1, l, true, true) == l);
        auto state3 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(splitted) == tid2);
            REQUIRE(timeline->getClipPlaytime(cid1) == l);
            REQUIRE(timeline->getClipPlaytime(splitted) == l);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getClipPosition(splitted) == 5);
        };
        state3();

        undoStack->undo();
        undoStack->undo();
        undoStack->undo();
        state2();
        undoStack->undo();
        state();
        undoStack->redo();
        state2();
        undoStack->redo();
        undoStack->redo();
        undoStack->redo();
        state3();
    }

    SECTION("Clip splitting 2")
    {
        // More complex group structure split split
        int l = timeline->getClipPlaytime(cid2);
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid2, tid1, l));
        REQUIRE(timeline->requestClipMove(cid3, tid1, 2 * l));
        REQUIRE(timeline->requestClipMove(cid4, tid2, 0));
        REQUIRE(timeline->requestClipMove(cid5, tid2, l));
        REQUIRE(timeline->requestClipMove(cid6, tid2, 2 * l));
        REQUIRE(timeline->requestClipMove(cid7, tid1, 200));
        int gid1 = timeline->requestClipsGroup(std::unordered_set<int>({cid1, cid4}), true, GroupType::Normal);
        int gid2 = timeline->requestClipsGroup(std::unordered_set<int>({cid2, cid5}), true, GroupType::Normal);
        int gid3 = timeline->requestClipsGroup(std::unordered_set<int>({cid3, cid6}), true, GroupType::Normal);
        int gid4 = timeline->requestClipsGroup(std::unordered_set<int>({cid1, cid2, cid3, cid4, cid5, cid6, cid7}), true, GroupType::Normal);
        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            int p = 0;
            for (int c : std::vector<int>({cid1, cid2, cid3})) {
                REQUIRE(timeline->getClipPlaytime(c) == l);
                REQUIRE(timeline->getClipTrackId(c) == tid1);
                REQUIRE(timeline->getClipPosition(c) == p);
                p += l;
            }
            p = 0;
            for (int c : std::vector<int>({cid4, cid5, cid6})) {
                REQUIRE(timeline->getClipPlaytime(c) == l);
                REQUIRE(timeline->getClipTrackId(c) == tid2);
                REQUIRE(timeline->getClipPosition(c) == p);
                p += l;
            }
            REQUIRE(timeline->getClipPosition(cid7) == 200);
            REQUIRE(timeline->getClipTrackId(cid7) == tid1);
            REQUIRE(timeline->m_groups->getDirectChildren(gid1) == std::unordered_set<int>({cid1, cid4}));
            REQUIRE(timeline->m_groups->getDirectChildren(gid2) == std::unordered_set<int>({cid2, cid5}));
            REQUIRE(timeline->m_groups->getDirectChildren(gid3) == std::unordered_set<int>({cid3, cid6}));
            REQUIRE(timeline->m_groups->getDirectChildren(gid4) == std::unordered_set<int>({gid1, gid2, gid3, cid7}));
            REQUIRE(timeline->getGroupElements(cid1) == std::unordered_set<int>({cid1, cid2, cid3, cid4, cid5, cid6, cid7}));
        };
        state();

        REQUIRE_FALSE(TimelineFunctions::requestClipCut(timeline, cid2, 0));
        REQUIRE_FALSE(TimelineFunctions::requestClipCut(timeline, cid2, 5 * l));
        REQUIRE_FALSE(TimelineFunctions::requestClipCut(timeline, cid2, l));
        REQUIRE_FALSE(TimelineFunctions::requestClipCut(timeline, cid2, 2 * l));
        state();

        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid2, l + 4));
        int splitted = timeline->getClipByPosition(tid1, l + 5);
        int splitted2 = timeline->getClipByPosition(tid2, l + 5);
        REQUIRE(splitted != splitted2);
        auto check_groups = [&]() {
            REQUIRE(timeline->m_groups->getDirectChildren(gid2) == std::unordered_set<int>({splitted, splitted2}));
            REQUIRE(timeline->m_groups->getDirectChildren(gid3) == std::unordered_set<int>({cid3, cid6}));
            REQUIRE(timeline->m_groups->getDirectChildren(gid4) == std::unordered_set<int>({gid2, gid3, cid7}));
            REQUIRE(timeline->getGroupElements(cid3) == std::unordered_set<int>({splitted, splitted2, cid3, cid6, cid7}));

            int g1b = timeline->m_groups->m_upLink[cid1];
            int g2b = timeline->m_groups->m_upLink[cid2];
            int g4b = timeline->m_groups->getRootId(cid1);
            REQUIRE(timeline->m_groups->getDirectChildren(g1b) == std::unordered_set<int>({cid1, cid4}));
            REQUIRE(timeline->m_groups->getDirectChildren(g2b) == std::unordered_set<int>({cid2, cid5}));
            REQUIRE(timeline->m_groups->getDirectChildren(g4b) == std::unordered_set<int>({g1b, g2b}));
            REQUIRE(timeline->getGroupElements(cid1) == std::unordered_set<int>({cid1, cid2, cid4, cid5}));
        };
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            int p = 0;
            for (int c : std::vector<int>({cid1, cid2, cid3})) {
                REQUIRE(timeline->getClipPlaytime(c) == (c == cid2 ? 4 : l));
                REQUIRE(timeline->getClipTrackId(c) == tid1);
                REQUIRE(timeline->getClipPosition(c) == p);
                p += l;
            }
            p = 0;
            for (int c : std::vector<int>({cid4, cid5, cid6})) {
                REQUIRE(timeline->getClipPlaytime(c) == (c == cid5 ? 4 : l));
                REQUIRE(timeline->getClipTrackId(c) == tid2);
                REQUIRE(timeline->getClipPosition(c) == p);
                p += l;
            }
            REQUIRE(timeline->getClipPosition(cid7) == 200);
            REQUIRE(timeline->getClipTrackId(cid7) == tid1);
            REQUIRE(timeline->getClipPosition(splitted) == l + 4);
            REQUIRE(timeline->getClipPlaytime(splitted) == l - 4);
            REQUIRE(timeline->getClipTrackId(splitted) == tid1);
            REQUIRE(timeline->getClipPosition(splitted2) == l + 4);
            REQUIRE(timeline->getClipPlaytime(splitted2) == l - 4);
            REQUIRE(timeline->getClipTrackId(splitted2) == tid2);
            check_groups();
        };
        state2();

        REQUIRE(timeline->requestClipMove(splitted, tid1, l + 4 + 10, true, true));
        REQUIRE(timeline->requestClipMove(cid1, tid2, 10, true, true));
        auto state3 = [&]() {
            REQUIRE(timeline->checkConsistency());
            int p = 0;
            for (int c : std::vector<int>({cid1, cid2, cid3})) {
                REQUIRE(timeline->getClipPlaytime(c) == (c == cid2 ? 4 : l));
                REQUIRE(timeline->getClipTrackId(c) == (c == cid3 ? tid1 : tid2));
                REQUIRE(timeline->getClipPosition(c) == p + 10);
                p += l;
            }
            p = 0;
            for (int c : std::vector<int>({cid4, cid5, cid6})) {
                REQUIRE(timeline->getClipPlaytime(c) == (c == cid5 ? 4 : l));
                REQUIRE(timeline->getClipTrackId(c) == (c == cid6 ? tid2 : tid3));
                REQUIRE(timeline->getClipPosition(c) == p + 10);
                p += l;
            }
            REQUIRE(timeline->getClipPosition(cid7) == 210);
            REQUIRE(timeline->getClipTrackId(cid7) == tid1);
            REQUIRE(timeline->getClipPosition(splitted) == l + 4 + 10);
            REQUIRE(timeline->getClipPlaytime(splitted) == l - 4);
            REQUIRE(timeline->getClipTrackId(splitted) == tid1);
            REQUIRE(timeline->getClipPosition(splitted2) == l + 4 + 10);
            REQUIRE(timeline->getClipPlaytime(splitted2) == l - 4);
            REQUIRE(timeline->getClipTrackId(splitted2) == tid2);
            check_groups();
        };
        state3();

        undoStack->undo();
        undoStack->undo();
        state2();
        undoStack->undo();
        state();
        undoStack->redo();
        state2();
        undoStack->redo();
        undoStack->redo();
        state3();
    }

    SECTION("Simple audio split")
    {
        int l = timeline->getClipPlaytime(audio1);
        REQUIRE(timeline->requestClipMove(audio1, tid1, 3));

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(audio1) == l);
            REQUIRE(timeline->getClipPosition(audio1) == 3);
            REQUIRE(timeline->getClipTrackId(audio1) == tid1);
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 0);

            REQUIRE(timeline->getGroupElements(audio1) == std::unordered_set<int>({audio1}));
        };
        state();

        REQUIRE(TimelineFunctions::requestSplitAudio(timeline, audio1, tid4));
        int splitted1 = timeline->getClipByPosition(tid4, 3);
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(audio1) == l);
            REQUIRE(timeline->getClipPosition(audio1) == 3);
            REQUIRE(timeline->getClipPlaytime(splitted1) == l);
            REQUIRE(timeline->getClipPosition(splitted1) == 3);
            REQUIRE(timeline->getClipTrackId(audio1) == tid1);
            REQUIRE(timeline->getClipTrackId(splitted1) == tid4);
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid4) == 1);

            REQUIRE(timeline->getGroupElements(audio1) == std::unordered_set<int>({audio1, splitted1}));

            int g1 = timeline->m_groups->getDirectAncestor(audio1);
            REQUIRE(timeline->m_groups->getDirectChildren(g1) == std::unordered_set<int>({audio1, splitted1}));
            REQUIRE(timeline->m_groups->getType(g1) == GroupType::AVSplit);

        };
        state2();

        undoStack->undo();
        state();
        undoStack->redo();
        state2();
        undoStack->undo();
        state();
        undoStack->redo();
        state2();

        // We also make sure that clips that are audio only cannot be further splitted
        REQUIRE(timeline->requestClipMove(cid1, tid1, l + 30));
        // This is a color clip, shouldn't be splittable
        REQUIRE_FALSE(TimelineFunctions::requestSplitAudio(timeline, cid1, tid2));
        // Check we cannot split audio on a video track
        REQUIRE_FALSE(TimelineFunctions::requestSplitAudio(timeline, audio1, tid2));
    }
    SECTION("Split audio on a selection")
    {

        int l = timeline->getClipPlaytime(audio2);
        REQUIRE(timeline->requestClipMove(audio1, tid1, 0));
        REQUIRE(timeline->requestClipMove(audio2, tid1, l));
        REQUIRE(timeline->requestClipMove(audio3, tid1, 2 * l));

        std::unordered_set<int> selection{audio1, audio3, audio2};
        REQUIRE(timeline->requestClipsGroup(selection, false, GroupType::Selection));

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(audio1) == l);
            REQUIRE(timeline->getClipPlaytime(audio2) == l);
            REQUIRE(timeline->getClipPlaytime(audio3) == l);
            REQUIRE(timeline->getClipPosition(audio1) == 0);
            REQUIRE(timeline->getClipPosition(audio2) == l);
            REQUIRE(timeline->getClipPosition(audio3) == l + l);
            REQUIRE(timeline->getClipTrackId(audio1) == tid1);
            REQUIRE(timeline->getClipTrackId(audio2) == tid1);
            REQUIRE(timeline->getClipTrackId(audio3) == tid1);
            REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 0);

            REQUIRE(timeline->getGroupElements(audio1) == std::unordered_set<int>({audio1, audio2, audio3}));

            int sel = timeline->m_temporarySelectionGroup;
            // check that selection is preserved
            REQUIRE(sel != -1);
            REQUIRE(timeline->m_groups->getType(sel) == GroupType::Selection);
        };
        state();

        REQUIRE(TimelineFunctions::requestSplitAudio(timeline, audio1, tid4));
        int splitted1 = timeline->getClipByPosition(tid4, 0);
        int splitted2 = timeline->getClipByPosition(tid4, l);
        int splitted3 = timeline->getClipByPosition(tid4, 2 * l);
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(audio1) == l);
            REQUIRE(timeline->getClipPlaytime(audio2) == l);
            REQUIRE(timeline->getClipPlaytime(audio3) == l);
            REQUIRE(timeline->getClipPosition(audio1) == 0);
            REQUIRE(timeline->getClipPosition(audio2) == l);
            REQUIRE(timeline->getClipPosition(audio3) == l + l);
            REQUIRE(timeline->getClipPlaytime(splitted1) == l);
            REQUIRE(timeline->getClipPlaytime(splitted2) == l);
            REQUIRE(timeline->getClipPlaytime(splitted3) == l);
            REQUIRE(timeline->getClipPosition(splitted1) == 0);
            REQUIRE(timeline->getClipPosition(splitted2) == l);
            REQUIRE(timeline->getClipPosition(splitted3) == l + l);
            REQUIRE(timeline->getClipTrackId(audio1) == tid1);
            REQUIRE(timeline->getClipTrackId(audio2) == tid1);
            REQUIRE(timeline->getClipTrackId(audio3) == tid1);
            REQUIRE(timeline->getClipTrackId(splitted1) == tid4);
            REQUIRE(timeline->getClipTrackId(splitted2) == tid4);
            REQUIRE(timeline->getClipTrackId(splitted3) == tid4);
            REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
            REQUIRE(timeline->getTrackClipsCount(tid4) == 3);

            REQUIRE(timeline->getGroupElements(audio1) == std::unordered_set<int>({audio1, splitted1, audio2, audio3, splitted2, splitted3}));

            int sel = timeline->m_temporarySelectionGroup;
            // check that selection is preserved
            REQUIRE(sel != -1);
            REQUIRE(timeline->m_groups->getType(sel) == GroupType::Selection);

            REQUIRE(timeline->m_groups->getRootId(audio1) == sel);
            REQUIRE(timeline->m_groups->getDirectChildren(sel).size() == 3);
            REQUIRE(timeline->m_groups->getLeaves(sel).size() == 6);

            int g1 = timeline->m_groups->getDirectAncestor(audio1);
            int g2 = timeline->m_groups->getDirectAncestor(audio2);
            int g3 = timeline->m_groups->getDirectAncestor(audio3);
            REQUIRE(timeline->m_groups->getDirectChildren(sel) == std::unordered_set<int>({g1, g2, g3}));
            REQUIRE(timeline->m_groups->getDirectChildren(g1) == std::unordered_set<int>({audio1, splitted1}));
            REQUIRE(timeline->m_groups->getDirectChildren(g2) == std::unordered_set<int>({audio2, splitted2}));
            REQUIRE(timeline->m_groups->getDirectChildren(g3) == std::unordered_set<int>({audio3, splitted3}));
            REQUIRE(timeline->m_groups->getType(g1) == GroupType::AVSplit);
            REQUIRE(timeline->m_groups->getType(g2) == GroupType::AVSplit);
            REQUIRE(timeline->m_groups->getType(g3) == GroupType::AVSplit);

        };
        state2();

        undoStack->undo();
        state();
        undoStack->redo();
        state2();
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
}
