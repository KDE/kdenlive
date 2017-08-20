#include "catch.hpp"
#include "doc/docundostack.hpp"
#include "bin/model/markerlistmodel.hpp"
#include <iostream>
#include <memory>
#include <random>
#include <string>

#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic push
#include "fakeit.hpp"
#include <mlt++/MltFactory.h>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltRepository.h>
#define private public
#define protected public
#include "timeline2/model/timelinemodel.hpp"
#include "project/projectmanager.h"
#include "core.h"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/model/compositionmodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/trackmodel.hpp"
#include "bin/projectitemmodel.h"
#include "bin/projectfolder.h"
#include "bin/projectclip.h"
#include "bin/clipcreator.hpp"

using namespace fakeit;
std::default_random_engine g(42);
Mlt::Profile profile_model;

#define RESET()                                                         \
    timMock.Reset();                                                    \
    Fake(Method(timMock, adjustAssetRange));                            \
    Spy(Method(timMock, _resetView));                                   \
    Spy(Method(timMock, _beginInsertRows));                             \
    Spy(Method(timMock, _beginRemoveRows));                             \
    Spy(Method(timMock, _endInsertRows));                               \
    Spy(Method(timMock, _endRemoveRows));                               \
    Spy(OverloadedMethod(timMock, notifyChange, void(const QModelIndex&, const QModelIndex&, bool, bool, bool))); \
    Spy(OverloadedMethod(timMock, notifyChange, void(const QModelIndex&, const QModelIndex&, QVector<int>)));

#define NO_OTHERS() \
    VerifyNoOtherInvocations(Method(timMock, _beginRemoveRows));  \
    VerifyNoOtherInvocations(Method(timMock, _beginInsertRows));  \
    VerifyNoOtherInvocations(Method(timMock, _endRemoveRows));    \
    VerifyNoOtherInvocations(Method(timMock, _endInsertRows));    \
    VerifyNoOtherInvocations(OverloadedMethod(timMock, notifyChange, void(const QModelIndex&, const QModelIndex&, bool, bool, bool))); \
    VerifyNoOtherInvocations(OverloadedMethod(timMock, notifyChange, void(const QModelIndex&, const QModelIndex&, QVector<int>))); \
    RESET();

#define CHECK_MOVE(times)                                         \
    Verify(Method(timMock, _beginRemoveRows) +                    \
           Method(timMock, _endRemoveRows) +                      \
           Method(timMock, _beginInsertRows) +                    \
           Method(timMock, _endInsertRows)                        \
        ).Exactly(times);                                         \
    NO_OTHERS();

#define CHECK_INSERT(times)                       \
    Verify(Method(timMock, _beginInsertRows) +    \
           Method(timMock, _endInsertRows)        \
        ).Exactly(times);                         \
    NO_OTHERS();

#define CHECK_REMOVE(times)                       \
    Verify(Method(timMock, _beginRemoveRows) +    \
           Method(timMock, _endRemoveRows)        \
        ).Exactly(times);                         \
    NO_OTHERS();

#define CHECK_RESIZE(times) \
    Verify(OverloadedMethod(timMock, notifyChange, void(const QModelIndex&, const QModelIndex&, bool, bool, bool))).Exactly(times); \
    NO_OTHERS();

QString createProducer(std::string color, std::shared_ptr<ProjectItemModel> binModel)
{
    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile_model, "color", color.c_str());
    producer->set("length", 20);
    producer->set("out", 19);

    REQUIRE(producer->is_valid());

    QString binId = QString::number(binModel->getFreeClipId());
    auto binClip = ProjectClip::construct(binId, QIcon(), binModel, producer);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    REQUIRE(binModel->addItem(binClip, binModel->getRootFolder()->clipId(), undo, redo));

    return binId;
}

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
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(),[](...){});
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
    RESET();

    int id2 = TrackModel::construct(timeline);
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 2);
    REQUIRE(timeline->getTrackPosition(id2) == 1);
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET();

    int id3 = TrackModel::construct(timeline);
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 3);
    REQUIRE(timeline->getTrackPosition(id3) == 2);
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET();

    int id4;
    REQUIRE(timeline->requestTrackInsertion(1, id4));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 4);
    REQUIRE(timeline->getTrackPosition(id1) == 0);
    REQUIRE(timeline->getTrackPosition(id4) == 1);
    REQUIRE(timeline->getTrackPosition(id2) == 2);
    REQUIRE(timeline->getTrackPosition(id3) == 3);
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET();

    // Test deletion
    REQUIRE(timeline->requestTrackDeletion(id3));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 3);
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET();

    REQUIRE(timeline->requestTrackDeletion(id1));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 2);
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET();

    REQUIRE(timeline->requestTrackDeletion(id4));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 1);
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET();

    REQUIRE(timeline->requestTrackDeletion(id2));
    REQUIRE(timeline->checkConsistency());
    REQUIRE(timeline->getTracksCount() == 0);
    Verify(Method(timMock, _resetView)).Exactly(Once);
    RESET();

    SECTION("Delete a track with groups") {
        int tid1, tid2;
        REQUIRE(timeline->requestTrackInsertion(-1, tid1));
        REQUIRE(timeline->requestTrackInsertion(-1, tid2));
        REQUIRE(timeline->checkConsistency());

        QString binId = createProducer("red", binModel);
        int length = 20;
        int cid1,cid2,cid3,cid4;
        REQUIRE(timeline->requestClipInsertion(binId, tid1, 2, cid1));
        REQUIRE(timeline->requestClipInsertion(binId, tid2, 0, cid2));
        REQUIRE(timeline->requestClipInsertion(binId, tid2, length, cid3));
        REQUIRE(timeline->requestClipInsertion(binId, tid2, 2*length, cid4));
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


    QString binId = createProducer("red", binModel);
    QString binId2 = createProducer("green", binModel);

    REQUIRE(timeline->getClipsCount() == 0);
    int id1 = ClipModel::construct(timeline, binId);
    REQUIRE(timeline->getClipsCount() == 1);
    REQUIRE(timeline->checkConsistency());

    int id2 = ClipModel::construct(timeline, binId2);
    REQUIRE(timeline->getClipsCount() == 2);
    REQUIRE(timeline->checkConsistency());

    int id3 = ClipModel::construct(timeline, binId);
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
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(),[](...){});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    Fake(Method(timMock, adjustAssetRange));

    // This is faked to allow to count calls
    Fake(Method(timMock, _resetView));
    Fake(Method(timMock, _beginInsertRows));
    Fake(Method(timMock, _beginRemoveRows));
    Fake(Method(timMock, _endInsertRows));
    Fake(Method(timMock, _endRemoveRows));

    QString binId = createProducer("red", binModel);
    QString binId2 = createProducer("blue", binModel);
    QString binId3 = createProducer("green", binModel);

    int cid1 = ClipModel::construct(timeline, binId);
    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);
    int tid3 = TrackModel::construct(timeline);
    int cid2 = ClipModel::construct(timeline, binId2);
    int cid3 = ClipModel::construct(timeline, binId3);
    int cid4 = ClipModel::construct(timeline, binId2);

    Verify(Method(timMock, _resetView)).Exactly(3_Times);
    RESET();

    // for testing purposes, we make sure the clip will behave as regular clips
    // (ie their size is fixed, we cannot resize them past their original size)
    timeline->m_allClips[cid1]->m_endlessResize = false;
    timeline->m_allClips[cid2]->m_endlessResize = false;
    timeline->m_allClips[cid3]->m_endlessResize = false;
    timeline->m_allClips[cid4]->m_endlessResize = false;

    SECTION("Insert a clip in a track and change track") {
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
    SECTION("Insert consecutive clips") {
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

    SECTION("Resize orphan clip"){
        REQUIRE(timeline->getClipPlaytime(cid2) == length);
        REQUIRE(timeline->requestItemResize(cid2, 5, true));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(binModel->getClipByBinID(binId)->frameDuration() == length);
        auto inOut = std::pair<int,int>{0,4};
        REQUIRE(timeline->m_allClips[cid2]->getInOut() == inOut );
        REQUIRE(timeline->getClipPlaytime(cid2) == 5);
        REQUIRE_FALSE(timeline->requestItemResize(cid2, 10, false));
        REQUIRE_FALSE(timeline->requestItemResize(cid2, length + 1, true));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipPlaytime(cid2) == 5);
        REQUIRE(timeline->getClipPlaytime(cid2) == 5);
        REQUIRE(timeline->requestItemResize(cid2, 2, false));
        REQUIRE(timeline->checkConsistency());
        inOut = std::pair<int,int>{3,4};
        REQUIRE(timeline->m_allClips[cid2]->getInOut() == inOut );
        REQUIRE(timeline->getClipPlaytime(cid2) == 2);
        REQUIRE_FALSE(timeline->requestItemResize(cid2, length, true));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipPlaytime(cid2) == 2);
        CAPTURE(timeline->m_allClips[cid2]->m_producer->get_in());
        REQUIRE_FALSE(timeline->requestItemResize(cid2, length - 2, true));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->requestItemResize(cid2, length - 3, true));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipPlaytime(cid2) == length - 3);
    }

    SECTION("Resize inserted clips"){
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->checkConsistency());
        CHECK_INSERT(Once);

        REQUIRE(timeline->requestItemResize(cid1, 5, true));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipPlaytime(cid1) == 5);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        CHECK_RESIZE(Once);

        REQUIRE(timeline->requestClipMove(cid2, tid1, 5));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(binModel->getClipByBinID(binId)->frameDuration() == length);
        CHECK_INSERT(Once);

        REQUIRE_FALSE(timeline->requestItemResize(cid1, 6, true));
        REQUIRE_FALSE(timeline->requestItemResize(cid1, 6, false));
        REQUIRE(timeline->checkConsistency());
        NO_OTHERS();

        REQUIRE(timeline->requestItemResize(cid2, length - 5, false));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getClipPosition(cid2) == 10);
        CHECK_RESIZE(Once);

        REQUIRE(timeline->requestItemResize(cid1, 10, true));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        CHECK_RESIZE(Once);
    }

    SECTION("Change track of resized clips"){
        // // REQUIRE(timeline->allowClipMove(cid2, tid1, 5));
        REQUIRE(timeline->requestClipMove(cid2, tid1, 5));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);

        // // REQUIRE(timeline->allowClipMove(cid1, tid2, 10));
        REQUIRE(timeline->requestClipMove(cid1, tid2, 10));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);

        REQUIRE(timeline->requestItemResize(cid1, 5, false));
        REQUIRE(timeline->checkConsistency());

        // // REQUIRE(timeline->allowClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);
    }

    SECTION("Clip Move"){
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

        REQUIRE(timeline->requestItemResize(cid2, length - 5, true));
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

        REQUIRE(timeline->requestItemResize(cid2, length - 10, false));
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

    SECTION ("Move and resize") {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestItemResize(cid1, length - 2, false));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        auto state = [&](){
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPlaytime(cid1) == length - 2);
        };
        state();

        //try to resize past the left end
        REQUIRE_FALSE(timeline->requestItemResize(cid1, length, false));
        state();

        REQUIRE(timeline->requestItemResize(cid1, length - 4, true));
        REQUIRE(timeline->requestClipMove(cid2, tid1, length - 4 + 1));
        REQUIRE(timeline->requestItemResize(cid2, length - 2, false));
        REQUIRE(timeline->requestClipMove(cid2, tid1, length - 4 + 1));
        auto state2 = [&](){
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

        //the gap between the two clips is 1 frame, we try to resize them by 2 frames
        REQUIRE_FALSE(timeline->requestItemResize(cid1, length - 2, true));
        state2();
        REQUIRE_FALSE(timeline->requestItemResize(cid2, length, false));
        state2();

        REQUIRE(timeline->requestClipMove(cid2, tid1, length - 4));
        auto state3 = [&](){
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

        //Now the gap is 0 frames, the resize should still fail
        REQUIRE_FALSE(timeline->requestItemResize(cid1, length - 2, true));
        state3();
        REQUIRE_FALSE(timeline->requestItemResize(cid2, length, false));
        state3();

        //We move cid1 out of the way
        REQUIRE(timeline->requestClipMove(cid1, tid2, 0));
        //now resize should work
        REQUIRE(timeline->requestItemResize(cid1, length - 2, true));
        REQUIRE(timeline->requestItemResize(cid2, length, false));
        REQUIRE(timeline->checkConsistency());

    }

    SECTION ("Group move") {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid2, tid1, length + 3));
        REQUIRE(timeline->requestClipMove(cid3, tid1, 2*length + 5));
        REQUIRE(timeline->requestClipMove(cid4, tid2, 4));

        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipTrackId(cid3) == tid1);
        REQUIRE(timeline->getClipTrackId(cid4) == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(timeline->getClipPosition(cid2) == length + 3);
        REQUIRE(timeline->getClipPosition(cid3) == 2*length + 5);
        REQUIRE(timeline->getClipPosition(cid4) == 4);

        //check that move is possible without groups
        REQUIRE(timeline->requestClipMove(cid3, tid1, 2*length + 3));
        REQUIRE(timeline->checkConsistency());
        undoStack->undo();
        REQUIRE(timeline->checkConsistency());
        //check that move is possible without groups
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
            REQUIRE(timeline->getClipPosition(cid3) == 2*length + 5);
            REQUIRE(timeline->getClipPosition(cid4) == 4);
        };
        state();

        //grouping
        REQUIRE(timeline->requestClipsGroup({cid1, cid3}));
        REQUIRE(timeline->requestClipsGroup({cid1, cid4}));

        //move left is now forbidden, because clip1 is at position 0
        REQUIRE_FALSE(timeline->requestClipMove(cid3, tid1, 2*length + 3));
        state();

        //this move is impossible, because clip1 runs into clip2
        REQUIRE_FALSE(timeline->requestClipMove(cid4, tid2, 9));
        state();

        //this move is possible
        REQUIRE(timeline->requestClipMove(cid3, tid1, 2*length + 8));
        auto state1 = [&](){
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
            REQUIRE(timeline->getClipPosition(cid3) == 2*length + 8);
            REQUIRE(timeline->getClipPosition(cid4) == 7);
        };
        state1();

        //this move is possible
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
            REQUIRE(timeline->getClipPosition(cid3) == 2*length + 5 + 8);
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

    SECTION("Group move to unavailable track") {
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

    SECTION("Group move with non-consecutive track ids") {
        int tid5 = TrackModel::construct(timeline);
        int cid6 = ClipModel::construct(timeline, binId);
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
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(),[](...){});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET();

    QString binId = createProducer("red", binModel);

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
            int cid = ClipModel::construct(timeline, binId);
            REQUIRE(all_ids.count(cid) == 0);
            all_ids.insert(cid);
            REQUIRE(timeline->getClipsCount() == all_ids.size() - track_ids.size());
        }
    }

    REQUIRE(timeline->checkConsistency());
    REQUIRE(all_ids.size() == nbr);
    REQUIRE(all_ids.size() != track_ids.size());
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
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(),[](...){});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET();

    QString binId = createProducer("red", binModel);
    QString binId2 = createProducer("blue", binModel);

    int cid1 = ClipModel::construct(timeline, binId);
    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);
    int cid2 = ClipModel::construct(timeline, binId2);

    timeline->m_allClips[cid1]->m_endlessResize = false;
    timeline->m_allClips[cid2]->m_endlessResize = false;

    int length = 20;

    int init_index = undoStack->index();

    SECTION("Basic move undo") {
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
        CHECK_MOVE(Once);

        undoStack->undo();
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);
        CHECK_MOVE(Once);

        undoStack->redo();
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(undoStack->index() == init_index + 2);
        CHECK_MOVE(Once);

        undoStack->undo();
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);
        CHECK_MOVE(Once);

        REQUIRE(timeline->requestClipMove(cid1, tid1, 2*length));
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 2*length);
        REQUIRE(undoStack->index() == init_index + 2);
        CHECK_MOVE(Once);

        undoStack->undo();
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);
        CHECK_MOVE(Once);

        undoStack->redo();
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 2*length);
        REQUIRE(undoStack->index() == init_index + 2);
        CHECK_MOVE(Once);

        undoStack->undo();
        CHECK_MOVE(Once);
        undoStack->undo();
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
        REQUIRE(timeline->getClipTrackId(cid1) == -1);
        REQUIRE(undoStack->index() == init_index);
        CHECK_REMOVE(Once);
    }

    SECTION("Basic resize orphan clip undo") {
        REQUIRE(timeline->getClipPlaytime(cid2) == length);

        REQUIRE(timeline->requestItemResize(cid2, length - 5, true));
        REQUIRE(undoStack->index() == init_index + 1);
        REQUIRE(timeline->getClipPlaytime(cid2) ==  length - 5);

        REQUIRE(timeline->requestItemResize(cid2, length - 10, false));
        REQUIRE(undoStack->index() == init_index + 2);
        REQUIRE(timeline->getClipPlaytime(cid2) ==  length - 10);

        REQUIRE_FALSE(timeline->requestItemResize(cid2, length, false));
        REQUIRE(undoStack->index() == init_index + 2);
        REQUIRE(timeline->getClipPlaytime(cid2) ==  length - 10);

        undoStack->undo();
        REQUIRE(undoStack->index() == init_index + 1);
        REQUIRE(timeline->getClipPlaytime(cid2) ==  length - 5);

        undoStack->redo();
        REQUIRE(undoStack->index() == init_index + 2);
        REQUIRE(timeline->getClipPlaytime(cid2) ==  length - 10);

        undoStack->undo();
        REQUIRE(undoStack->index() == init_index + 1);
        REQUIRE(timeline->getClipPlaytime(cid2) ==  length - 5);

        undoStack->undo();
        REQUIRE(undoStack->index() == init_index);
        REQUIRE(timeline->getClipPlaytime(cid2) ==  length);
    }
    SECTION("Basic resize inserted clip undo") {
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

        REQUIRE(timeline->requestItemResize(cid2, length - 5, true));
        INFO("Test 2");
        check(5, length - 5);
        REQUIRE(undoStack->index() == init_index + 2);

        REQUIRE(timeline->requestItemResize(cid2, length - 10, false));
        INFO("Test 3");
        check(10, length - 10);
        REQUIRE(undoStack->index() == init_index + 3);

        REQUIRE_FALSE(timeline->requestItemResize(cid2, length, false));
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
    SECTION("Clip Insertion Undo") {
        QString binId3 = createProducer("red", binModel);

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

        REQUIRE(timeline->requestItemResize(cid3, 1, true));
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


    SECTION("Clip Deletion undo") {
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

    SECTION("Track insertion undo") {
        int nb_tracks = timeline->getTracksCount();
        std::map<int, int> orig_trackPositions, final_trackPositions;
        for (const auto& it : timeline->m_iteratorTable) {
            int track = it.first;
            int pos = timeline->getTrackPosition(track);
            orig_trackPositions[track] = pos;
            if (pos >= 1) pos++;
            final_trackPositions[track] = pos;
        }
        auto checkPositions = [&](const std::map<int,int>& pos) {
            for (const auto& p : pos ) {
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


    SECTION("Track deletion undo") {
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
}
/*
TEST_CASE("Snapping", "[Snapping]") {
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);


    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile_model, "color", "red");
    std::shared_ptr<Mlt::Producer> producer2 = std::make_shared<Mlt::Producer>(profile_model, "color", "blue");
    producer->set("length", 20);
    producer->set("out", 19);
    producer2->set("length", 20);
    producer2->set("out", 19);

    REQUIRE(producer->is_valid());
    REQUIRE(producer2->is_valid());
    int tid1 = TrackModel::construct(timeline);
    int cid1 = ClipModel::construct(timeline, producer);
    int tid2 = TrackModel::construct(timeline);
    int cid2 = ClipModel::construct(timeline, producer2);

    int length = timeline->getClipPlaytime(cid1);
    SECTION("getBlankSizeNearClip") {
        REQUIRE(timeline->requestClipMove(cid1,tid1,0));

        //before
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, false) == 0);
        //after
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, true) == INT_MAX);
        REQUIRE(timeline->requestClipMove(cid1,tid1,10));
        //before
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, false) == 10);
        //after
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, true) == INT_MAX);
        REQUIRE(timeline->requestClipMove(cid2,tid1,25 + length));
        //before
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, false) == 10);
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid2, false) == 15);
        //after
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, true) == 15);
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid2, true) == INT_MAX);

        REQUIRE(timeline->requestClipMove(cid2,tid1,10 + length));
        //before
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, false) == 10);
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid2, false) == 0);
        //after
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, true) == 0);
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid2, true) == INT_MAX);
    }
}
*/
