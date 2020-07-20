#include "test_utils.hpp"

using namespace fakeit;
std::default_random_engine g(42);
Mlt::Profile profile_model;

TEST_CASE("Basic creation/deletion of a track", "[TrackModel]")
{
    Logger::clear();
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
    TimelineItemModel tim(&profile_model, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    Fake(Method(timMock, adjustAssetRange));

    // This is faked to allow to count calls

    int id1, id2, id3;
    REQUIRE(timeline->requestTrackInsertion(-1, id1));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 1);
    REQUIRE(timeline->getTrackPosition(id1) == 0);
    RESET(timMock);

    REQUIRE(timeline->requestTrackInsertion(-1, id2));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 2);
    REQUIRE(timeline->getTrackPosition(id2) == 1);
    RESET(timMock);

    REQUIRE(timeline->requestTrackInsertion(-1, id3));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 3);
    REQUIRE(timeline->getTrackPosition(id3) == 2);
    RESET(timMock);

    int id4;
    REQUIRE(timeline->requestTrackInsertion(1, id4));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 4);
    REQUIRE(timeline->getTrackPosition(id1) == 0);
    REQUIRE(timeline->getTrackPosition(id4) == 1);
    REQUIRE(timeline->getTrackPosition(id2) == 2);
    REQUIRE(timeline->getTrackPosition(id3) == 3);
    RESET(timMock);

    // Test deletion
    REQUIRE(timeline->requestTrackDeletion(id3));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 3);
    RESET(timMock);

    REQUIRE(timeline->requestTrackDeletion(id1));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 2);
    RESET(timMock);

    REQUIRE(timeline->requestTrackDeletion(id4));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 1);
    RESET(timMock);

    // We are not allowed to delete the last track
    REQUIRE_FALSE(timeline->requestTrackDeletion(id2));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 1);
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
        REQUIRE(timeline->getTracksCount() == 3);

        auto g1 = std::unordered_set<int>({cid1, cid3});
        auto g2 = std::unordered_set<int>({cid2, cid4});
        auto g3 = std::unordered_set<int>({cid1, cid4});
        REQUIRE(timeline->requestClipsGroup(g1));
        REQUIRE(timeline->requestClipsGroup(g2));
        REQUIRE(timeline->requestClipsGroup(g3));
        REQUIRE(timeline->checkConsistency());

        REQUIRE(timeline->requestTrackDeletion(tid1));
        REQUIRE(timeline->getClipsCount() == 3);
        REQUIRE(timeline->getTracksCount() == 2);
        REQUIRE(timeline->checkConsistency());
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
    Logger::print_trace();
}

TEST_CASE("Basic creation/deletion of a clip", "[ClipModel]")
{
    Logger::clear();

    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(&profile_model, guideModel, undoStack);

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
    Logger::print_trace();
}

TEST_CASE("Clip manipulation", "[ClipModel]")
{
    Logger::clear();
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
    TimelineItemModel tim(&profile_model, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    Fake(Method(timMock, adjustAssetRange));

    // This is faked to allow to count calls
    Fake(Method(timMock, _beginInsertRows));
    Fake(Method(timMock, _beginRemoveRows));
    Fake(Method(timMock, _endInsertRows));
    Fake(Method(timMock, _endRemoveRows));

    QString binId = createProducer(profile_model, "red", binModel);
    QString binId2 = createProducer(profile_model, "blue", binModel);
    QString binId3 = createProducer(profile_model, "green", binModel);
    QString binId_unlimited = createProducer(profile_model, "green", binModel, 20, false);

    int cid1 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int tid1, tid2, tid3;
    REQUIRE(timeline->requestTrackInsertion(-1, tid1));
    REQUIRE(timeline->requestTrackInsertion(-1, tid2));
    REQUIRE(timeline->requestTrackInsertion(-1, tid3));
    int cid2 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);
    int cid3 = ClipModel::construct(timeline, binId3, -1, PlaylistState::VideoOnly);
    int cid4 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);
    int cid5 = ClipModel::construct(timeline, binId_unlimited, -1, PlaylistState::VideoOnly);

    RESET(timMock);

    SECTION("Endless clips can be resized both sides")
    {

        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);
        int l = timeline->getClipPlaytime(cid5);

        // try resizing uninserted clip
        REQUIRE(timeline->requestItemResize(cid5, l + 2, false) == l + 2);
        REQUIRE(timeline->getClipPlaytime(cid5) == l + 2);
        undoStack->undo();
        REQUIRE(timeline->getClipPlaytime(cid5) == l);
        undoStack->redo();
        REQUIRE(timeline->getClipPlaytime(cid5) == l + 2);
        undoStack->undo();
        REQUIRE(timeline->getClipPlaytime(cid5) == l);

        REQUIRE(timeline->requestItemResize(cid5, 3 * l, true) == 3 * l);
        REQUIRE(timeline->getClipPlaytime(cid5) == 3 * l);
        undoStack->undo();
        REQUIRE(timeline->getClipPlaytime(cid5) == l);
        undoStack->redo();
        REQUIRE(timeline->getClipPlaytime(cid5) == 3 * l);
        undoStack->undo();
        REQUIRE(timeline->getClipPlaytime(cid5) == l);

        // try resizing inserted clip
        int pos = 10;
        REQUIRE(timeline->requestClipMove(cid5, tid1, pos));

        auto state = [&](int s, int p) {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipTrackId(cid5) == tid1);
            REQUIRE(timeline->getClipPosition(cid5) == p);
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 0);
            REQUIRE(timeline->getClipPlaytime(cid5) == s);
        };
        state(l, pos);

        // too big
        REQUIRE(timeline->requestItemResize(cid5, l + pos + 2, false) == -1);

        REQUIRE(timeline->requestItemResize(cid5, l + 2, false) == l + 2);
        state(l + 2, pos - 2);
        undoStack->undo();
        state(l, pos);
        undoStack->redo();
        state(l + 2, pos - 2);
        undoStack->undo();
        state(l, pos);

        REQUIRE(timeline->requestItemResize(cid5, 3 * l, true) == 3 * l);
        state(3 * l, pos);
        undoStack->undo();
        state(l, pos);
        undoStack->redo();
        state(3 * l, pos);
        undoStack->undo();
        state(l, pos);
    }

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

    SECTION("Group and selection")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid2, tid1, length + 3));
        REQUIRE(timeline->requestClipMove(cid3, tid1, 2 * length + 5));
        auto pos_state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == length + 3);
            REQUIRE(timeline->getClipPosition(cid3) == 2 * length + 5);
        };
        auto state0 = [&]() {
            pos_state();
            REQUIRE_FALSE(timeline->m_groups->isInGroup(cid1));
            REQUIRE_FALSE(timeline->m_groups->isInGroup(cid2));
            REQUIRE_FALSE(timeline->m_groups->isInGroup(cid3));
        };
        state0();

        REQUIRE(timeline->requestClipsGroup({cid1, cid2}));
        auto state = [&]() {
            pos_state();
            REQUIRE_FALSE(timeline->m_groups->isInGroup(cid3));
            REQUIRE(timeline->m_groups->isInGroup(cid1));
            int gid = timeline->m_groups->getRootId(cid1);
            REQUIRE(timeline->m_groups->getLeaves(gid) == std::unordered_set<int>{cid1, cid2});
        };
        state();

        // undo/redo should work fine
        undoStack->undo();
        state0();
        undoStack->redo();
        state();

        // Tricky case, we do a non-trivial selection before undoing
        REQUIRE(timeline->requestSetSelection({cid1, cid3}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid1, cid2, cid3});
        undoStack->undo();
        state0();
        REQUIRE(timeline->requestSetSelection({cid1, cid3}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid1, cid3});
        undoStack->redo();
        state();

        // same thing, but when ungrouping manually
        REQUIRE(timeline->requestSetSelection({cid1, cid3}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid1, cid2, cid3});
        REQUIRE(timeline->requestClipUngroup(cid1));
        state0();

        // normal undo/redo
        undoStack->undo();
        state();
        undoStack->redo();
        state0();

        // undo/redo mixed with selections
        REQUIRE(timeline->requestSetSelection({cid1, cid3}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid1, cid3});
        undoStack->undo();
        state();
        REQUIRE(timeline->requestSetSelection({cid1, cid3}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid1, cid2, cid3});
        undoStack->redo();
        state0();
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
        };
        state();

        // Moving clips on an unavailable track will do a same track move
        REQUIRE(timeline->requestClipMove(cid2, tid1, 10));
        REQUIRE(timeline->getClipPosition(cid1) == 8);
        REQUIRE(timeline->getClipPosition(cid2) == 10);
        state();
        REQUIRE(timeline->requestClipMove(cid2, tid1, 100));
        REQUIRE(timeline->getClipPosition(cid1) == 98);
        REQUIRE(timeline->getClipPosition(cid2) == 100);
        state();
        REQUIRE(timeline->requestClipMove(cid1, tid3, 100));
        REQUIRE(timeline->getClipPosition(cid1) == 100);
        REQUIRE(timeline->getClipPosition(cid2) == 102);
        state();
    }

    SECTION("Group move with non-consecutive track ids")
    {
        int tid5 = TrackModel::construct(timeline);
        int cid6 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
        Q_UNUSED(cid6);
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

    SECTION("Creation and movement of AV groups")
    {
        int tid6b = TrackModel::construct(timeline, -1, -1, QString(), true);
        int tid6 = TrackModel::construct(timeline, -1, -1, QString(), true);
        int tid5 = TrackModel::construct(timeline);
        int tid5b = TrackModel::construct(timeline);
        auto state0 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid5) == 0);
            REQUIRE(timeline->getTrackClipsCount(tid6) == 0);
        };
        state0();
        QString binId3 = createProducerWithSound(profile_model, binModel);

        int cid6 = -1;
        REQUIRE(timeline->requestClipInsertion(binId3, tid5, 3, cid6, true, true, false));
        int cid7 = timeline->m_groups->getSplitPartner(cid6);

        auto check_group = [&]() {
            // we check that the av group was correctly created
            REQUIRE(timeline->getGroupElements(cid6) == std::unordered_set<int>({cid6, cid7}));
            int g1 = timeline->m_groups->getDirectAncestor(cid6);
            REQUIRE(timeline->m_groups->getDirectChildren(g1) == std::unordered_set<int>({cid6, cid7}));
            REQUIRE(timeline->m_groups->getType(g1) == GroupType::AVSplit);
        };

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
            check_group();
        };
        state(3);
        undoStack->undo();
        state0();
        undoStack->redo();
        state(3);

        // test deletion + undo after selection
        REQUIRE(timeline->requestSetSelection({cid6}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid6, cid7});

        REQUIRE(timeline->requestItemDeletion(cid6, true));
        state0();
        undoStack->undo();
        state(3);
        undoStack->redo();
        state0();
        undoStack->undo();
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
            check_group();
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
    }

    SECTION("Clip clone")
    {
        int cid6 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
        int l = timeline->getClipPlaytime(cid6);
        REQUIRE(timeline->requestItemResize(cid6, l - 3, true, true, -1) == l - 3);
        REQUIRE(timeline->requestItemResize(cid6, l - 7, false, true, -1) == l - 7);

        int newId;

        std::function<bool(void)> undo = []() { return true; };
        std::function<bool(void)> redo = []() { return true; };
        REQUIRE(TimelineFunctions::cloneClip(timeline, cid6, newId, PlaylistState::VideoOnly, undo, redo));
        REQUIRE(timeline->m_allClips[cid6]->binId() == timeline->m_allClips[newId]->binId());
        // TODO check effects
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
    Logger::print_trace();
}

TEST_CASE("Check id unicity", "[ClipModel]")
{
    Logger::clear();
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
    TimelineItemModel tim(&profile_model, undoStack);
    Mock<TimelineItemModel> timMock(tim);
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
    Logger::print_trace();
}

TEST_CASE("Undo and Redo", "[ClipModel]")
{
    Logger::clear();
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
    TimelineItemModel tim(&profile_model, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET(timMock);

    QString binId = createProducer(profile_model, "red", binModel);
    QString binId2 = createProducer(profile_model, "blue", binModel);

    int cid1 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);
    int cid2 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);

    int length = 20;
    int nclips = timeline->m_allClips.size();

    SECTION("requestCreateClip")
    {
        // an invalid clip id shouldn't get created
        {
            int temp;
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            REQUIRE_FALSE(timeline->requestClipCreation("impossible bin id", temp, PlaylistState::VideoOnly, 1, 1., false, undo, redo));
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
            REQUIRE(timeline->requestClipCreation(binId3, cid3, PlaylistState::VideoOnly, 1, 1., false, undo, redo));
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
            REQUIRE(timeline->requestClipCreation(binId4, cid4, PlaylistState::VideoOnly, 1, 1., false, undo, redo));
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

    SECTION("Select then delete")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        REQUIRE(timeline->requestClipMove(cid2, tid2, 1));
        auto state1 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid2);
            REQUIRE(timeline->getClipPosition(cid2) == 1);
        };
        state1();

        REQUIRE(timeline->requestSetSelection({cid1, cid2}));
        int nbClips = timeline->getClipsCount();
        REQUIRE(timeline->requestItemDeletion(cid1));
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 0);
            REQUIRE(timeline->getClipsCount() == nbClips - 2);
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
            REQUIRE(timeline->requestClipCreation(binId, cid6, PlaylistState::VideoOnly, 1, 1., false, undo, redo));
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
            REQUIRE(timeline->requestClipMove(cid6, tid1, 7, true, true, true, true, undo, redo));
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
    Logger::print_trace();
}

TEST_CASE("Snapping", "[Snapping]")
{
    Logger::clear();
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
    TimelineItemModel tim(&profile_model, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET(timMock);

    QString binId = createProducer(profile_model, "red", binModel, 50);
    QString binId2 = createProducer(profile_model, "blue", binModel);

    int tid1 = TrackModel::construct(timeline);
    int cid1 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int tid2 = TrackModel::construct(timeline);
    int cid2 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);

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
            REQUIRE(timeline->suggestClipMove(cid2, tid2, beg, -1, snap).at(0) == beg);
            REQUIRE(timeline->suggestClipMove(cid2, tid2, beg + length, -1, snap).at(0) == beg + length);
            REQUIRE(timeline->checkConsistency());
        }

        // We add a clip in first track to create snap points
        REQUIRE(timeline->requestClipMove(cid1, tid1, beg));

        // Now a clip in second track should snap to beginning
        auto check_snap = [&](int pos, int perturb, int snap) {
            if (snap >= perturb) {
                REQUIRE(timeline->suggestClipMove(cid2, tid2, pos + perturb, -1, snap).at(0) == pos);
                REQUIRE(timeline->suggestClipMove(cid2, tid2, pos - perturb, -1, snap).at(0) == pos);
            } else {
                REQUIRE(timeline->suggestClipMove(cid2, tid2, pos + perturb, -1, snap).at(0) == pos + perturb);
                REQUIRE(timeline->suggestClipMove(cid2, tid2, pos - perturb, -1, snap).at(0) == pos - perturb);
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
    Logger::print_trace();
}

TEST_CASE("Operations under locked tracks", "[Locked]")
{
    Logger::clear();

    QString aCompo;
    // Look for a compo
    QVector<QPair<QString, QString>> transitions = TransitionsRepository::get()->getNames();
    for (const auto &trans : qAsConst(transitions)) {
        if (TransitionsRepository::get()->isComposition(trans.first)) {
            aCompo = trans.first;
            break;
        }
    }
    REQUIRE(!aCompo.isEmpty());

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
    TimelineItemModel tim(&profile_model, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    Fake(Method(timMock, adjustAssetRange));

    // This is faked to allow to count calls
    Fake(Method(timMock, _beginInsertRows));
    Fake(Method(timMock, _beginRemoveRows));
    Fake(Method(timMock, _endInsertRows));
    Fake(Method(timMock, _endRemoveRows));

    QString binId = createProducer(profile_model, "red", binModel);
    QString binId3 = createProducerWithSound(profile_model, binModel);

    int tid1, tid2, tid3;
    REQUIRE(timeline->requestTrackInsertion(-1, tid1));
    REQUIRE(timeline->requestTrackInsertion(-1, tid2));
    REQUIRE(timeline->requestTrackInsertion(-1, tid3));

    RESET(timMock);

    SECTION("Locked track can't receive insertion")
    {
        timeline->setTrackLockedState(tid1, true);
        REQUIRE(timeline->getTrackById(tid1)->isLocked());
        REQUIRE(timeline->getClipsCount() == 0);
        REQUIRE(timeline->checkConsistency());
        int cid1 = -1;
        REQUIRE_FALSE(timeline->requestClipInsertion(binId, tid1, 2, cid1));
        REQUIRE(timeline->getClipsCount() == 0);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(cid1 == -1);

        // now unlock and check that insertion becomes possible again
        timeline->setTrackLockedState(tid1, false);
        REQUIRE_FALSE(timeline->getTrackById(tid1)->isLocked());
        REQUIRE(timeline->getClipsCount() == 0);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->requestClipInsertion(binId, tid1, 2, cid1));
        REQUIRE(timeline->getClipsCount() == 1);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 2);
    }
    SECTION("Can't move clip on locked track")
    {
        int cid1 = -1;
        REQUIRE(timeline->requestClipInsertion(binId, tid1, 2, cid1));
        REQUIRE(timeline->getClipsCount() == 1);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 2);
        // not yet locked, move should work
        REQUIRE(timeline->requestClipMove(cid1, tid1, 4));
        REQUIRE(timeline->getClipPosition(cid1) == 4);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);

        timeline->setTrackLockedState(tid1, true);
        REQUIRE(timeline->getTrackById(tid1)->isLocked());
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipsCount() == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 4);

        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, 6));

        REQUIRE(timeline->getTrackById(tid1)->isLocked());
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipsCount() == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 4);

        // unlock, move should work again
        timeline->setTrackLockedState(tid1, false);
        REQUIRE_FALSE(timeline->getTrackById(tid1)->isLocked());
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->requestClipMove(cid1, tid1, 6));
        REQUIRE(timeline->getClipsCount() == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 6);
        REQUIRE(timeline->checkConsistency());
    }
    SECTION("Can't move composition on locked track")
    {
        int compo = CompositionModel::construct(timeline, aCompo, QString());
        timeline->setTrackLockedState(tid1, true);
        REQUIRE(timeline->getTrackById(tid1)->isLocked());
        REQUIRE(timeline->checkConsistency());

        REQUIRE(timeline->getCompositionTrackId(compo) == -1);
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 0);
        int pos = 10;
        REQUIRE_FALSE(timeline->requestCompositionMove(compo, tid1, pos));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionTrackId(compo) == -1);
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 0);

        // unlock to be able to insert
        timeline->setTrackLockedState(tid1, false);
        REQUIRE_FALSE(timeline->getTrackById(tid1)->isLocked());
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->requestCompositionMove(compo, tid1, pos));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionTrackId(compo) == tid1);
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 1);
        REQUIRE(timeline->getCompositionPosition(compo) == pos);

        // relock
        timeline->setTrackLockedState(tid1, true);
        REQUIRE(timeline->getTrackById(tid1)->isLocked());
        REQUIRE(timeline->checkConsistency());
        REQUIRE_FALSE(timeline->requestCompositionMove(compo, tid1, pos + 10));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getCompositionTrackId(compo) == tid1);
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 1);
        REQUIRE(timeline->getCompositionPosition(compo) == pos);
    }
    SECTION("Can't resize clip on locked track")
    {
        int cid1 = -1;
        REQUIRE(timeline->requestClipInsertion(binId, tid1, 2, cid1));
        REQUIRE(timeline->getClipsCount() == 1);

        auto check = [&](int l) {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 2);
            REQUIRE(timeline->getClipPlaytime(cid1) == l);
        };
        check(20);

        // not yet locked, resize should work
        REQUIRE(timeline->requestItemResize(cid1, 18, true) == 18);
        check(18);

        // lock
        timeline->setTrackLockedState(tid1, true);
        REQUIRE(timeline->getTrackById(tid1)->isLocked());
        check(18);
        REQUIRE(timeline->requestItemResize(cid1, 17, true) == -1);
        check(18);
        REQUIRE(timeline->requestItemResize(cid1, 17, false) == -1);
        check(18);
        REQUIRE(timeline->requestItemResize(cid1, 19, true) == -1);
        check(18);
        REQUIRE(timeline->requestItemResize(cid1, 19, false) == -1);
        check(18);

        // unlock, resize should work again
        timeline->setTrackLockedState(tid1, false);
        REQUIRE_FALSE(timeline->getTrackById(tid1)->isLocked());
        check(18);
        REQUIRE(timeline->requestItemResize(cid1, 17, true) == 17);
        check(17);
    }
    SECTION("Can't resize composition on locked track")
    {
        int compo = CompositionModel::construct(timeline, aCompo, QString());
        REQUIRE(timeline->requestCompositionMove(compo, tid1, 2));
        REQUIRE(timeline->requestItemResize(compo, 20, true) == 20);

        auto check = [&](int l) {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getCompositionsCount() == 1);
            REQUIRE(timeline->getCompositionTrackId(compo) == tid1);
            REQUIRE(timeline->getCompositionPosition(compo) == 2);
            REQUIRE(timeline->getCompositionPlaytime(compo) == l);
        };
        check(20);

        // not yet locked, resize should work
        REQUIRE(timeline->requestItemResize(compo, 18, true) == 18);
        check(18);

        // lock
        timeline->setTrackLockedState(tid1, true);
        REQUIRE(timeline->getTrackById(tid1)->isLocked());
        check(18);
        REQUIRE(timeline->requestItemResize(compo, 17, true) == -1);
        check(18);
        REQUIRE(timeline->requestItemResize(compo, 17, false) == -1);
        check(18);
        REQUIRE(timeline->requestItemResize(compo, 19, true) == -1);
        check(18);
        REQUIRE(timeline->requestItemResize(compo, 19, false) == -1);
        check(18);

        // unlock, resize should work again
        timeline->setTrackLockedState(tid1, false);
        REQUIRE_FALSE(timeline->getTrackById(tid1)->isLocked());
        check(18);
        REQUIRE(timeline->requestItemResize(compo, 17, true) == 17);
        check(17);
    }
    
    binModel->clean();
    pCore->m_projectManager = nullptr;
    Logger::print_trace();
}
