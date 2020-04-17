#include "test_utils.hpp"

Mlt::Profile reg_profile;
TEST_CASE("Regression")
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
    TimelineItemModel tim(&reg_profile, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET(timMock);
    TimelineModel::next_id = 0;
    undoStack->undo();
    undoStack->redo();
    undoStack->redo();
    undoStack->undo();
    QString binId0 = createProducer(reg_profile, "red", binModel);
    int c = ClipModel::construct(timeline, binId0, -1, PlaylistState::VideoOnly);
    timeline->m_allClips[c]->m_endlessResize = false;
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->requestClipMove(0, 1, 0));
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->requestItemResize(0, 16, false));
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->requestItemResize(0, 0, false) == -1);
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    binModel->clean();
    pCore->m_projectManager = nullptr;
    Logger::print_trace();
}

TEST_CASE("Regression2")
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
    TimelineItemModel tim(&reg_profile, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET(timMock);
    TimelineModel::next_id = 0;
    int dummy_id;
    undoStack->undo();
    undoStack->undo();
    undoStack->redo();
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    {
        QString binId0 = createProducer(reg_profile, "red", binModel);
        bool ok = timeline->requestClipInsertion(binId0, 0, 10, dummy_id);
        timeline->m_allClips[dummy_id]->m_endlessResize = false;
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    {
        QString binId0 = createProducer(reg_profile, "red", binModel);
        bool ok = timeline->requestClipInsertion(binId0, 2, 10, dummy_id);
        timeline->m_allClips[3]->m_endlessResize = false;
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(1, 0, 10);
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    undoStack->redo();
    timeline->m_allClips[3]->m_endlessResize = false;
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    {
        REQUIRE(timeline->requestItemResize(3, 0, false) == -1);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    {
        QString binId0 = createProducer(reg_profile, "red", binModel);
        int c = ClipModel::construct(timeline, binId0, -1, PlaylistState::VideoOnly);
        timeline->m_allClips[c]->m_endlessResize = false;
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        REQUIRE(timeline->requestItemResize(3, 15, true) > -1);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(3, 0, 0);
        REQUIRE_FALSE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        REQUIRE(timeline->requestItemResize(3, 16, false) == -1);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        REQUIRE(timeline->requestItemResize(3, 16, true) > -1);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        QString binId0 = createProducer(reg_profile, "red", binModel);
        bool ok = timeline->requestClipInsertion(binId0, 0, 1, dummy_id);
        REQUIRE_FALSE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->redo();
    binModel->clean();
    pCore->m_projectManager = nullptr;
    Logger::print_trace();
}

/*
TEST_CASE("Regression 3")
{
    Mlt::Profile profile;
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);
    TimelineModel::next_id = 0;
    int dummy_id;
    std::shared_ptr<Mlt::Producer> producer0 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer0->set("length", 20);
    producer0->set("out", 19);
    ClipModel::construct(timeline, producer0 );
    {
        bool ok = timeline->requestTrackInsertion(-1, dummy_id);
        REQUIRE(ok);
    }
    TrackModel::construct(timeline);
    TrackModel::construct(timeline);
    std::shared_ptr<Mlt::Producer> producer1 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer1->set("length", 20);
    producer1->set("out", 19);
    ClipModel::construct(timeline, producer1 );
    std::shared_ptr<Mlt::Producer> producer2 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer2->set("length", 20);
    producer2->set("out", 19);
    ClipModel::construct(timeline, producer2 );
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    std::shared_ptr<Mlt::Producer> producer3 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer3->set("length", 20);
    producer3->set("out", 19);
    ClipModel::construct(timeline, producer3 );
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    std::shared_ptr<Mlt::Producer> producer4 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer4->set("length", 20);
    producer4->set("out", 19);
    ClipModel::construct(timeline, producer4 );
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    std::shared_ptr<Mlt::Producer> producer5 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer5->set("length", 20);
    producer5->set("out", 19);
    ClipModel::construct(timeline, producer5 );
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    std::shared_ptr<Mlt::Producer> producer6 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer6->set("length", 20);
    producer6->set("out", 19);
    ClipModel::construct(timeline, producer6 );
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(0,1 ,10 );
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(4,2 ,12 );
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    {
        auto group = {4, 0};
        bool ok = timeline->requestClipsGroup(group);
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(4,1 ,10 );
        REQUIRE_FALSE(ok);
    }
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(4,1 ,100 );
        REQUIRE_FALSE(ok);
    }
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(3)->checkConsistency());
    REQUIRE(timeline->getTrackById(7)->checkConsistency());
    REQUIRE(timeline->getTrackById(8)->checkConsistency());
    REQUIRE(timeline->getTrackById(9)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(0,3 ,100 );
        REQUIRE(ok);
    }
    std::shared_ptr<Mlt::Producer> producer7 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer7->set("length", 20);
    producer7->set("out", 19);
    ClipModel::construct(timeline, producer7 );
    {
        bool ok = timeline->requestTrackInsertion(-1, dummy_id);
        REQUIRE(ok);
    }
    undoStack->undo();
    {
        bool ok = timeline->requestClipMove(0,1 ,5 );
        REQUIRE(ok);
    }
    {
        bool ok = timeline->requestTrackDeletion(1);
        REQUIRE(ok);
    }
}

TEST_CASE("Regression 4")
{
    Mlt::Profile profile;
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);
    TimelineModel::next_id = 0;
    int dummy_id;
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    timeline->requestTrackInsertion(-1, dummy_id );
    {
        std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile, "color", "red");
        producer->set("length", 62);
        producer->set("out", 61);
        timeline->requestClipInsertion(producer,10 ,453, dummy_id );
    }
    timeline->requestClipMove(11,10 ,453, true, true );
    {
        std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile, "color", "red");
        producer->set("length", 62);
        producer->set("out", 61);
        timeline->requestClipInsertion(producer,9 ,590, dummy_id );
    }
    timeline->requestItemResize(11,62 ,true, false, true );
    timeline->requestItemResize(11,62 ,true, true, true );
    timeline->requestClipMove(11,10 ,507, true, true );
    timeline->requestClipMove(12,10 ,583, false, false );
    timeline->requestClipMove(12,9 ,521, true, true );
}
*/

TEST_CASE("FuzzBug1")
{
    Logger::clear();
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);
    TimelineModel::next_id = 0;
    {
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        ProjectManager &mocked = pmMock.get();
        pCore->m_projectManager = &mocked;
        TimelineItemModel tim_0(&reg_profile, undoStack);
        Mock<TimelineItemModel> timMock_0(tim_0);
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline_0, guideModel);
        Fake(Method(timMock_0, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        TrackModel::construct(timeline_0, -1, -1, "", false);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        TrackModel::construct(timeline_0, -1, -1, "$$$", false);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            bool res = timeline_0->requestTrackDeletion(2);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            bool res = timeline_0->requestTrackDeletion(1);
            // Cannot delete last track
            REQUIRE(res == false);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int dummy_1;
            bool res = timeline_0->requestTrackInsertion(-1, dummy_1, "", false);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int dummy_1;
            bool res = timeline_0->requestTrackInsertion(-1, dummy_1, "", false);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        createProducer(reg_profile, "red", binModel, 20, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 3, 0, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 3, 20, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 3, 40, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int res = timeline_0->requestClipsGroup({5, 7}, true, GroupType::Normal);
            REQUIRE(res == 8);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int res = timeline_0->requestClipsGroup({6, 7}, true, GroupType::Normal);
            REQUIRE(res == 9);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int res = timeline_0->requestClipsGroup({6, 7}, false, GroupType::Normal);
            REQUIRE(res == 9);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
    }
    pCore->m_projectManager = nullptr;
    Logger::print_trace();
}

TEST_CASE("FuzzBug2")
{
    Logger::clear();
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);
    TimelineModel::next_id = 0;
    {
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        ProjectManager &mocked = pmMock.get();
        pCore->m_projectManager = &mocked;
        TimelineItemModel tim_0(&reg_profile, undoStack);
        Mock<TimelineItemModel> timMock_0(tim_0);
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline_0, guideModel);
        Fake(Method(timMock_0, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int dummy_1;
            bool res = timeline_0->requestTrackInsertion(-1, dummy_1, "$", false);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        createProducer(reg_profile, "d", binModel, 0, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 1, 0, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 1, 30, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 1, 60, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int res = timeline_0->requestClipsGroup({3, 2}, true, GroupType::AVSplit);
            REQUIRE(res == -1);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
    }
    pCore->m_projectManager = nullptr;
    Logger::print_trace();
}

TEST_CASE("FuzzBug3")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);
    TimelineModel::next_id = 0;
    {
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        ProjectManager &mocked = pmMock.get();
        pCore->m_projectManager = &mocked;
        TimelineItemModel tim_0(&reg_profile, undoStack);
        Mock<TimelineItemModel> timMock_0(tim_0);
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline_0, guideModel);
        Fake(Method(timMock_0, adjustAssetRange));

        createProducerWithSound(reg_profile, binModel);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        TrackModel::construct(timeline_0, -1, 0, "0", true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 1, 0, dummy_3, false, false, false);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
    }
    pCore->m_projectManager = nullptr;
}

TEST_CASE("FuzzBug4")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);
    TimelineModel::next_id = 0;
    {
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        ProjectManager &mocked = pmMock.get();
        pCore->m_projectManager = &mocked;
        TimelineItemModel tim_0(&reg_profile, undoStack);
        Mock<TimelineItemModel> timMock_0(tim_0);
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline_0, guideModel);
        Fake(Method(timMock_0, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        createProducer(reg_profile, "red", binModel, 2, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        createProducer(reg_profile, "blue", binModel, 20, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        createProducer(reg_profile, "gseen", binModel, 20, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        TrackModel::construct(timeline_0, -1, -1, "", false);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        createProducerWithSound(reg_profile, binModel);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("5", 1, 3, dummy_3, true, true, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            timeline_0->requestGroupDeletion(2, false);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
    }
    pCore->m_projectManager = nullptr;
}

TEST_CASE("FuzzBug5")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);
    TimelineModel::next_id = 0;
    {
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        ProjectManager &mocked = pmMock.get();
        pCore->m_projectManager = &mocked;
        TimelineItemModel tim_0(&reg_profile, undoStack);
        Mock<TimelineItemModel> timMock_0(tim_0);
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline_0, guideModel);
        Fake(Method(timMock_0, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        TrackModel::construct(timeline_0, -1, 0, "", false);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        TimelineItemModel tim_1(&reg_profile, undoStack);
        Mock<TimelineItemModel> timMock_1(tim_1);
        auto timeline_1 = std::shared_ptr<TimelineItemModel>(&timMock_1.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline_1, guideModel);
        Fake(Method(timMock_1, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        TrackModel::construct(timeline_0, -1, 0, "$", false);
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        TrackModel::construct(timeline_1, -1, 0, "$", true);
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        createProducerWithSound(reg_profile, binModel);
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        TrackModel::construct(timeline_1, -1, -1, "$", false);
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        TrackModel::construct(timeline_1, -1, -1, "$", true);
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_1->requestClipInsertion("2", 5, 0, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_1->requestClipInsertion("2", 6, 20, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        {
            bool res = timeline_1->requestGroupMove(10, 9, 0, 0, true, false, false);
            REQUIRE(res == false);
        }
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
    }
    pCore->m_projectManager = nullptr;
}

TEST_CASE("FuzzBug6")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);
    TimelineModel::next_id = 0;
    {
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        ProjectManager &mocked = pmMock.get();
        pCore->m_projectManager = &mocked;
        TimelineItemModel tim_0(&reg_profile, undoStack);
        Mock<TimelineItemModel> timMock_0(tim_0);
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline_0, guideModel);
        Fake(Method(timMock_0, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        TrackModel::construct(timeline_0, -1, 0, "$", false);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        TimelineItemModel tim_1(&reg_profile, undoStack);
        Mock<TimelineItemModel> timMock_1(tim_1);
        auto timeline_1 = std::shared_ptr<TimelineItemModel>(&timMock_1.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline_1, guideModel);
        Fake(Method(timMock_1, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        createProducer(reg_profile, "b", binModel, 20, true);
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 1, -1, dummy_3, false, false, false);
            REQUIRE(res == false);
        }
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
    }
    pCore->m_projectManager = nullptr;
}

TEST_CASE("FuzzBug7")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);
    TimelineModel::next_id = 0;
    {
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        ProjectManager &mocked = pmMock.get();
        pCore->m_projectManager = &mocked;
        TimelineItemModel tim_0(&reg_profile, undoStack);
        Mock<TimelineItemModel> timMock_0(tim_0);
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline_0, guideModel);
        Fake(Method(timMock_0, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        TrackModel::construct(timeline_0, -1, -1, "0", false);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        TimelineItemModel tim_1(&reg_profile, undoStack);
        Mock<TimelineItemModel> timMock_1(tim_1);
        auto timeline_1 = std::shared_ptr<TimelineItemModel>(&timMock_1.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline_1, guideModel);
        Fake(Method(timMock_1, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        createProducer(reg_profile, "r5", binModel, 2, true);
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 1, 0, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 1, 20, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 1, 40, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        {
            int res = timeline_0->requestClipsGroup({4, 3}, true, GroupType::Selection);
            REQUIRE(res == -1);
        }
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        {
            int res = timeline_0->requestClipsGroup({5, 3}, true, GroupType::Normal);
            REQUIRE(res == 6);
        }
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        REQUIRE(timeline_1->checkConsistency());
    }
    pCore->m_projectManager = nullptr;
}

TEST_CASE("FuzzBug8")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);
    TimelineModel::next_id = 0;
    {
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        ProjectManager &mocked = pmMock.get();
        pCore->m_projectManager = &mocked;
        TimelineItemModel tim_0(&reg_profile, undoStack);
        Mock<TimelineItemModel> timMock_0(tim_0);
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline_0, guideModel);
        Fake(Method(timMock_0, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int dummy_1;
            bool res = timeline_0->requestTrackInsertion(-1, dummy_1, "", false);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        createProducer(reg_profile, "red20", binModel, 1, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 1, 40, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int res = timeline_0->requestClipsGroup({2}, true, GroupType::Leaf);
            REQUIRE(res == -1);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
    }
    pCore->m_projectManager = nullptr;
}

TEST_CASE("FuzzBug9")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);
    TimelineModel::next_id = 0;
    {
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        ProjectManager &mocked = pmMock.get();
        pCore->m_projectManager = &mocked;
        TimelineItemModel tim_0(&reg_profile, undoStack);
        Mock<TimelineItemModel> timMock_0(tim_0);
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline_0, guideModel);
        Fake(Method(timMock_0, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        createProducer(reg_profile, "60", binModel, 1, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        TrackModel::construct(timeline_0, -1, -1, "", false);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        ClipModel::construct(timeline_0, "2", 2, PlaylistState::Disabled, 1);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            timeline_0->suggestClipMove(2, 1, -34, 0, 0);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
    }
    pCore->m_projectManager = nullptr;
}

TEST_CASE("FuzzBug10")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);
    TimelineModel::next_id = 0;
    {
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        ProjectManager &mocked = pmMock.get();
        pCore->m_projectManager = &mocked;
        TimelineItemModel tim_0(&reg_profile, undoStack);
        Mock<TimelineItemModel> timMock_0(tim_0);
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline_0, guideModel);
        Fake(Method(timMock_0, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        createProducer(reg_profile, "red", binModel, 50, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        ClipModel::construct(timeline_0, "2", 1, PlaylistState::VideoOnly, -1);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            timeline_0->requestItemResize(1, 12, false, true, 1, false);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
    }
    pCore->m_projectManager = nullptr;
}

TEST_CASE("FuzzBug11")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);
    TimelineModel::next_id = 0;
    {
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        ProjectManager &mocked = pmMock.get();
        pCore->m_projectManager = &mocked;
        TimelineItemModel tim_0(&reg_profile, undoStack);
        Mock<TimelineItemModel> timMock_0(tim_0);
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline_0, guideModel);
        Fake(Method(timMock_0, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        createProducer(reg_profile, "red", binModel, 20, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        createProducer(reg_profile, "blue", binModel, 0, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        createProducer(reg_profile, "green", binModel, 20, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        TrackModel::construct(timeline_0, -1, -1, "$", true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        TrackModel::construct(timeline_0, -1, -1, "", false);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        createProducerWithSound(reg_profile, binModel);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("5", 2, 3, dummy_3, true, true, false);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        {
            timeline_0->requestSetSelection({4});
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
    }
    pCore->m_projectManager = nullptr;
}
