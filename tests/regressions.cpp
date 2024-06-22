/*
    SPDX-FileCopyrightText: 2017-2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2017-2019 Nicolas Carion <french.ebook.lover@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"
// test specific headers
#include "doc/kdenlivedoc.h"

TEST_CASE("Regression")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack
    KdenliveDoc document(undoStack);
    Mock<KdenliveDoc> docMock(document);
    KdenliveDoc &mockedDoc = docMock.get();
    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
    When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
    When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
    pCore->projectManager()->testSetDocument(&mockedDoc);

    // We also mock timeline object to spy few functions and mock others
    std::shared_ptr<TimelineItemModel> tim = KdenliveTests::createTimelineModel(mockedDoc.uuid(), undoStack);
    Mock<TimelineItemModel> timMock(*tim.get());
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    KdenliveTests::finishTimelineConstruct(timeline);
    pCore->projectManager()->testSetActiveTimeline(timeline);

    RESET(timMock);
    KdenliveTests::resetNextId();
    undoStack->undo();
    undoStack->redo();
    undoStack->redo();
    undoStack->undo();
    QString binId0 = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel);
    int c = ClipModel::construct(timeline, binId0, -1, PlaylistState::VideoOnly);
    KdenliveTests::makeFiniteClipEnd(timeline, c);

    TrackModel::construct(timeline);
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(timeline->requestClipMove(0, 1, 0));
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(timeline->requestItemResize(0, 16, false));
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    undoStack->undo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    undoStack->redo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    undoStack->undo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    undoStack->redo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    undoStack->undo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    undoStack->redo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(timeline->requestItemResize(0, 0, false) == -1);
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    undoStack->undo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    undoStack->redo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    pCore->taskManager.slotCancelJobs();
    mockedDoc.closing = true;
    timeline.reset();
    binModel->clean();
}

TEST_CASE("Regression2")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack
    KdenliveDoc document(undoStack);
    Mock<KdenliveDoc> docMock(document);
    KdenliveDoc &mockedDoc = docMock.get();
    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
    When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
    When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
    pCore->projectManager()->testSetDocument(&mockedDoc);
    // We also mock timeline object to spy few functions and mock others
    std::shared_ptr<TimelineItemModel> tim = KdenliveTests::createTimelineModel(mockedDoc.uuid(), undoStack);
    Mock<TimelineItemModel> timMock(*tim.get());
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    KdenliveTests::finishTimelineConstruct(timeline);

    KdenliveTests::resetNextId();
    int dummy_id;
    undoStack->undo();
    undoStack->undo();
    undoStack->redo();
    TrackModel::construct(timeline);
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    undoStack->undo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    {
        QString binId0 = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel);
        bool ok = timeline->requestClipInsertion(binId0, 0, 10, dummy_id);
        KdenliveTests::makeFiniteClipEnd(timeline, dummy_id);
        REQUIRE(ok);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    undoStack->undo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    undoStack->redo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    {
        QString binId0 = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel);
        bool ok = timeline->requestClipInsertion(binId0, 2, 10, dummy_id);
        KdenliveTests::makeFiniteClipEnd(timeline, 3);
        REQUIRE(ok);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(1, 0, 10);
        REQUIRE(ok);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    undoStack->undo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    undoStack->redo();
    KdenliveTests::makeFiniteClipEnd(timeline, 3);
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    {
        REQUIRE(timeline->requestItemResize(3, 0, false) == -1);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 4)->checkConsistency());
    {
        QString binId0 = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel);
        int c = ClipModel::construct(timeline, binId0, -1, PlaylistState::VideoOnly);
        KdenliveTests::makeFiniteClipEnd(timeline, c);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 4)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 4)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 6)->checkConsistency());
    {
        REQUIRE(timeline->requestItemResize(3, 15, true) > -1);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 4)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 6)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(3, 0, 0);
        REQUIRE_FALSE(ok);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 4)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 6)->checkConsistency());
    {
        REQUIRE(timeline->requestItemResize(3, 16, false) == -1);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 4)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 6)->checkConsistency());
    {
        REQUIRE(timeline->requestItemResize(3, 16, true) > -1);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 4)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 6)->checkConsistency());
    undoStack->undo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 4)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 6)->checkConsistency());
    undoStack->undo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 4)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 6)->checkConsistency());
    {
        QString binId0 = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel);
        bool ok = timeline->requestClipInsertion(binId0, 0, 1, dummy_id);
        REQUIRE_FALSE(ok);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 4)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 6)->checkConsistency());
    undoStack->undo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 4)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 6)->checkConsistency());
    undoStack->redo();
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 0)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 4)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 6)->checkConsistency());
    undoStack->redo();
    pCore->taskManager.slotCancelJobs();
    mockedDoc.closing = true;
    timeline.reset();
    binModel->clean();
}

/*
TEST_CASE("Regression 3")
{
    Mlt::Profile profile;
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);
    KdenliveTests::resetNextId();
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
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 3)->checkConsistency());
    std::shared_ptr<Mlt::Producer> producer3 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer3->set("length", 20);
    producer3->set("out", 19);
    ClipModel::construct(timeline, producer3 );
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 3)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 3)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 7)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 3)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 7)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 8)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 3)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 7)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 8)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 9)->checkConsistency());
    std::shared_ptr<Mlt::Producer> producer4 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer4->set("length", 20);
    producer4->set("out", 19);
    ClipModel::construct(timeline, producer4 );
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 3)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 7)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 8)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 9)->checkConsistency());
    std::shared_ptr<Mlt::Producer> producer5 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer5->set("length", 20);
    producer5->set("out", 19);
    ClipModel::construct(timeline, producer5 );
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 3)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 7)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 8)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 9)->checkConsistency());
    std::shared_ptr<Mlt::Producer> producer6 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer6->set("length", 20);
    producer6->set("out", 19);
    ClipModel::construct(timeline, producer6 );
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 3)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 7)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 8)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 9)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(0,1 ,10 );
        REQUIRE(ok);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 3)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 7)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 8)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 9)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(4,2 ,12 );
        REQUIRE(ok);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 3)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 7)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 8)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 9)->checkConsistency());
    {
        auto group = {4, 0};
        bool ok = timeline->requestClipsGroup(group);
        REQUIRE(ok);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 3)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 7)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 8)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 9)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(4,1 ,10 );
        REQUIRE_FALSE(ok);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 3)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 7)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 8)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 9)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(4,1 ,100 );
        REQUIRE_FALSE(ok);
    }
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 1)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 2)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 3)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 7)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 8)->checkConsistency());
    REQUIRE(KdenliveTests::getTrackById_const(timeline, 9)->checkConsistency());
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
    KdenliveTests::resetNextId();
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
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveTests::resetNextId();
    {
        KdenliveDoc document(undoStack);
        Mock<KdenliveDoc> docMock(document);
        KdenliveDoc &mockedDoc = docMock.get();
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
        pCore->projectManager()->testSetDocument(&mockedDoc);

        // We also mock timeline object to spy few functions and mock others
        std::shared_ptr<TimelineItemModel> tim = KdenliveTests::createTimelineModel(mockedDoc.uuid(), undoStack);
        Mock<TimelineItemModel> timMock_0(*tim.get());
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        KdenliveTests::finishTimelineConstruct(timeline_0);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);

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
        KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel, 20, true);
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
        pCore->taskManager.slotCancelJobs();
        mockedDoc.closing = true;
        timeline_0.reset();
        binModel->clean();
    }
}

TEST_CASE("FuzzBug2")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveTests::resetNextId();
    {
        KdenliveDoc document(undoStack);
        Mock<KdenliveDoc> docMock(document);
        KdenliveDoc &mockedDoc = docMock.get();
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
        pCore->projectManager()->testSetDocument(&mockedDoc);

        // We also mock timeline object to spy few functions and mock others
        std::shared_ptr<TimelineItemModel> tim = KdenliveTests::createTimelineModel(mockedDoc.uuid(), undoStack);
        Mock<TimelineItemModel> timMock_0(*tim.get());
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        KdenliveTests::finishTimelineConstruct(timeline_0);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);

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
        KdenliveTests::createProducer(pCore->getProjectProfile(), "d", binModel, 0, true);
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
            // Grouping 2 audio clips as AVSplit, this is now possible with multiple streams
            // int res = timeline_0->requestClipsGroup({3, 2}, true, GroupType::AVSplit);
            // REQUIRE(res == -1);
        }
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        pCore->taskManager.slotCancelJobs();
        mockedDoc.closing = true;
        timeline_0.reset();
        binModel->clean();
    }
}

TEST_CASE("FuzzBug3")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveTests::resetNextId();
    {
        KdenliveDoc document(undoStack);
        Mock<KdenliveDoc> docMock(document);
        KdenliveDoc &mockedDoc = docMock.get();
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
        pCore->projectManager()->testSetDocument(&mockedDoc);

        // We also mock timeline object to spy few functions and mock others
        std::shared_ptr<TimelineItemModel> tim = KdenliveTests::createTimelineModel(mockedDoc.uuid(), undoStack);
        Mock<TimelineItemModel> timMock_0(*tim.get());
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        KdenliveTests::finishTimelineConstruct(timeline_0);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);

        Fake(Method(timMock_0, adjustAssetRange));

        KdenliveTests::createProducerWithSound(pCore->getProjectProfile(), binModel);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        TrackModel::construct(timeline_0, -1, 0, "0", true);
        // Setup timeline audio drop info
        QMap<int, QString> audioInfo;
        audioInfo.insert(1, QStringLiteral("stream1"));
        KdenliveTests::setAudioTargets(timeline_0, audioInfo);
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
        pCore->taskManager.slotCancelJobs();
        mockedDoc.closing = true;
        timeline_0.reset();
        binModel->clean();
    }
}

TEST_CASE("FuzzBug4")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveTests::resetNextId();
    {
        KdenliveDoc document(undoStack);
        Mock<KdenliveDoc> docMock(document);
        KdenliveDoc &mockedDoc = docMock.get();
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
        pCore->projectManager()->testSetDocument(&mockedDoc);

        // We also mock timeline object to spy few functions and mock others
        std::shared_ptr<TimelineItemModel> tim = KdenliveTests::createTimelineModel(mockedDoc.uuid(), undoStack);
        Mock<TimelineItemModel> timMock_0(*tim.get());
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        KdenliveTests::finishTimelineConstruct(timeline_0);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);

        Fake(Method(timMock_0, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel, 2, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        KdenliveTests::createProducer(pCore->getProjectProfile(), "blue", binModel, 20, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        KdenliveTests::createProducer(pCore->getProjectProfile(), "gseen", binModel, 20, true);
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
        KdenliveTests::createProducerWithSound(pCore->getProjectProfile(), binModel);
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
        pCore->taskManager.slotCancelJobs();
        mockedDoc.closing = true;
        timeline_0.reset();
        binModel->clean();
    }
}

TEST_CASE("FuzzBug5")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveTests::resetNextId();
    {
        KdenliveDoc document(undoStack);
        Mock<KdenliveDoc> docMock(document);
        KdenliveDoc &mockedDoc = docMock.get();
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
        pCore->projectManager()->testSetDocument(&mockedDoc);

        // We also mock timeline object to spy few functions and mock others
        std::shared_ptr<TimelineItemModel> tim = KdenliveTests::createTimelineModel(mockedDoc.uuid(), undoStack);
        Mock<TimelineItemModel> timMock_0(*tim.get());
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        KdenliveTests::finishTimelineConstruct(timeline_0);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);

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
        QUuid uuid2 = QUuid::createUuid();
        std::shared_ptr<TimelineItemModel> tim_1 = KdenliveTests::createTimelineModel(uuid2, undoStack);
        Mock<TimelineItemModel> timMock_1(*tim_1.get());
        auto timeline_1 = std::shared_ptr<TimelineItemModel>(&timMock_1.get(), [](...) {});
        KdenliveTests::finishTimelineConstruct(timeline_1);
        Fake(Method(timMock_1, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        TrackModel::construct(timeline_0, -1, 0, "$", false);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        TrackModel::construct(timeline_1, -1, 0, "$", true);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        KdenliveTests::createProducerWithSound(pCore->getProjectProfile(), binModel);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        TrackModel::construct(timeline_1, -1, -1, "$", false);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        TrackModel::construct(timeline_1, -1, -1, "$", true);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();

        // Setup timeline audio drop info
        QMap<int, QString> audioInfo;
        audioInfo.insert(1, QStringLiteral("stream1"));
        KdenliveTests::setAudioTargets(timeline_1, audioInfo);
        KdenliveTests::setVideoTargets(timeline_1, 5);
        QMap<int, int> audioTarget;
        audioTarget.insert(6, 1);
        KdenliveTests::setAudioTimelineTargets(timeline_1, audioTarget);

        /*timeline_1->setTrackProperty(4, "kdenlive:timeline_active", QStringLiteral("1"));
        timeline_1->setTrackProperty(5, "kdenlive:timeline_active", QStringLiteral("1"));
        timeline_1->setTrackProperty(6, "kdenlive:timeline_active", QStringLiteral("1"));*/

        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_1->requestClipInsertion("2", 5, 0, dummy_3, true, false, true);
            qDebug() << "==== INSERTED FIRST CLIP DURATION: " << timeline_1->getClipPlaytime(dummy_3);
            REQUIRE(res == true);
        }
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_1->requestClipInsertion("2", 6, 20, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        {
            bool res = timeline_1->requestGroupMove(10, 9, 0, 0, true, false, false);
            REQUIRE(res == false);
        }
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        pCore->taskManager.slotCancelJobs();
        undoStack->clear();
        mockedDoc.closing = true;
        binModel->clean();
        timeline_0.reset();
        timeline_1.reset();
    }
}

TEST_CASE("FuzzBug6")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveTests::resetNextId();
    {
        KdenliveDoc document(undoStack);
        Mock<KdenliveDoc> docMock(document);
        KdenliveDoc &mockedDoc = docMock.get();
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
        pCore->projectManager()->testSetDocument(&mockedDoc);

        // We also mock timeline object to spy few functions and mock others
        std::shared_ptr<TimelineItemModel> tim = KdenliveTests::createTimelineModel(mockedDoc.uuid(), undoStack);
        Mock<TimelineItemModel> timMock_0(*tim.get());
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        KdenliveTests::finishTimelineConstruct(timeline_0);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);

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
        QUuid uuid2 = QUuid::createUuid();
        std::shared_ptr<TimelineItemModel> tim_1 = KdenliveTests::createTimelineModel(uuid2, undoStack);
        Mock<TimelineItemModel> timMock_1(*tim_1.get());
        auto timeline_1 = std::shared_ptr<TimelineItemModel>(&timMock_1.get(), [](...) {});
        KdenliveTests::finishTimelineConstruct(timeline_1);
        Fake(Method(timMock_1, adjustAssetRange));
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        KdenliveTests::createProducer(pCore->getProjectProfile(), "b", binModel, 20, true);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 1, -1, dummy_3, false, false, false);
            REQUIRE(res == false);
        }
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        pCore->taskManager.slotCancelJobs();
        mockedDoc.closing = true;
        timeline_0.reset();
        timeline_1.reset();
        binModel->clean();
    }
}

TEST_CASE("FuzzBug7")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveTests::resetNextId();
    {
        KdenliveDoc document(undoStack);
        Mock<KdenliveDoc> docMock(document);
        KdenliveDoc &mockedDoc = docMock.get();
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
        pCore->projectManager()->testSetDocument(&mockedDoc);

        // We also mock timeline object to spy few functions and mock others
        std::shared_ptr<TimelineItemModel> tim = KdenliveTests::createTimelineModel(mockedDoc.uuid(), undoStack);
        Mock<TimelineItemModel> timMock_0(*tim.get());
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        KdenliveTests::finishTimelineConstruct(timeline_0);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);

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
        QUuid uuid2 = QUuid::createUuid();
        std::shared_ptr<TimelineItemModel> tim_1 = KdenliveTests::createTimelineModel(uuid2, undoStack);
        Mock<TimelineItemModel> timMock_1(*tim_1.get());
        auto timeline_1 = std::shared_ptr<TimelineItemModel>(&timMock_1.get(), [](...) {});
        KdenliveTests::finishTimelineConstruct(timeline_1);
        Fake(Method(timMock_1, adjustAssetRange));
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        KdenliveTests::createProducer(pCore->getProjectProfile(), "r5", binModel, 2, true);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->undo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        {
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 1, 0, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        undoStack->redo();
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        {
            pCore->projectManager()->testSetActiveTimeline(timeline_0);
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 1, 20, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        {
            pCore->projectManager()->testSetActiveTimeline(timeline_0);
            int dummy_3;
            bool res = timeline_0->requestClipInsertion("2", 1, 40, dummy_3, true, false, true);
            REQUIRE(res == true);
        }
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        {
            pCore->projectManager()->testSetActiveTimeline(timeline_0);
            int res = timeline_0->requestClipsGroup({4, 3}, true, GroupType::Selection);
            REQUIRE(res == -1);
        }
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        {
            pCore->projectManager()->testSetActiveTimeline(timeline_0);
            int res = timeline_0->requestClipsGroup({5, 3}, true, GroupType::Normal);
            REQUIRE(res == 6);
        }
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_0);
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        pCore->projectManager()->testSetActiveTimeline(timeline_1);
        REQUIRE(timeline_1->checkConsistency());
        pCore->taskManager.slotCancelJobs();
        mockedDoc.closing = true;
        timeline_0.reset();
        timeline_1.reset();
        binModel->clean();
    }
}

TEST_CASE("FuzzBug8")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveTests::resetNextId();
    {
        KdenliveDoc document(undoStack);
        Mock<KdenliveDoc> docMock(document);
        KdenliveDoc &mockedDoc = docMock.get();
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
        pCore->projectManager()->testSetDocument(&mockedDoc);

        // We also mock timeline object to spy few functions and mock others
        std::shared_ptr<TimelineItemModel> tim = KdenliveTests::createTimelineModel(mockedDoc.uuid(), undoStack);
        Mock<TimelineItemModel> timMock_0(*tim.get());
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        KdenliveTests::finishTimelineConstruct(timeline_0);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);

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
        KdenliveTests::createProducer(pCore->getProjectProfile(), "red20", binModel, 1, true);
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
        pCore->taskManager.slotCancelJobs();
        mockedDoc.closing = true;
        timeline_0.reset();
        binModel->clean();
    }
}

TEST_CASE("FuzzBug9")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveTests::resetNextId();
    {
        KdenliveDoc document(undoStack);
        Mock<KdenliveDoc> docMock(document);
        KdenliveDoc &mockedDoc = docMock.get();
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
        pCore->projectManager()->testSetDocument(&mockedDoc);

        // We also mock timeline object to spy few functions and mock others
        std::shared_ptr<TimelineItemModel> tim = KdenliveTests::createTimelineModel(mockedDoc.uuid(), undoStack);
        Mock<TimelineItemModel> timMock_0(*tim.get());
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        KdenliveTests::finishTimelineConstruct(timeline_0);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);

        Fake(Method(timMock_0, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        KdenliveTests::createProducer(pCore->getProjectProfile(), "60", binModel, 1, true);
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
        pCore->taskManager.slotCancelJobs();
        mockedDoc.closing = true;
        timeline_0.reset();
        binModel->clean();
    }
}

TEST_CASE("FuzzBug10")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveTests::resetNextId();
    {
        KdenliveDoc document(undoStack);
        Mock<KdenliveDoc> docMock(document);
        KdenliveDoc &mockedDoc = docMock.get();
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
        pCore->projectManager()->testSetDocument(&mockedDoc);

        // We also mock timeline object to spy few functions and mock others
        std::shared_ptr<TimelineItemModel> tim = KdenliveTests::createTimelineModel(mockedDoc.uuid(), undoStack);
        Mock<TimelineItemModel> timMock_0(*tim.get());
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        KdenliveTests::finishTimelineConstruct(timeline_0);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);

        Fake(Method(timMock_0, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel, 50, true);
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
        pCore->taskManager.slotCancelJobs();
        mockedDoc.closing = true;
        timeline_0.reset();
        binModel->clean();
    }
}

TEST_CASE("FuzzBug11")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveTests::resetNextId();
    {
        KdenliveDoc document(undoStack);
        Mock<KdenliveDoc> docMock(document);
        KdenliveDoc &mockedDoc = docMock.get();
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
        pCore->projectManager()->testSetDocument(&mockedDoc);

        // We also mock timeline object to spy few functions and mock others
        std::shared_ptr<TimelineItemModel> tim = KdenliveTests::createTimelineModel(mockedDoc.uuid(), undoStack);
        Mock<TimelineItemModel> timMock_0(*tim.get());
        auto timeline_0 = std::shared_ptr<TimelineItemModel>(&timMock_0.get(), [](...) {});
        KdenliveTests::finishTimelineConstruct(timeline_0);
        pCore->projectManager()->testSetActiveTimeline(timeline_0);

        Fake(Method(timMock_0, adjustAssetRange));
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel, 20, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        KdenliveTests::createProducer(pCore->getProjectProfile(), "blue", binModel, 0, true);
        REQUIRE(timeline_0->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline_0->checkConsistency());
        undoStack->redo();
        REQUIRE(timeline_0->checkConsistency());
        KdenliveTests::createProducer(pCore->getProjectProfile(), "green", binModel, 20, true);
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
        KdenliveTests::createProducerWithSound(pCore->getProjectProfile(), binModel);

        // Setup timeline audio drop info
        QMap<int, QString> audioInfo;
        audioInfo.insert(1, QStringLiteral("stream1"));
        KdenliveTests::setAudioTargets(timeline_0, audioInfo);
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
        pCore->taskManager.slotCancelJobs();
        mockedDoc.closing = true;
        timeline_0.reset();
        binModel->clean();
    }
}
