#include "catch.hpp"
#include "doc/docundostack.hpp"
#include "test_utils.hpp"

#include "definitions.h"
#define private public
#define protected public
#include "core.h"

using namespace fakeit;
Mlt::Profile profile_move;

TEST_CASE("Cut undo/redo", "[MoveClips]")
{
    // Create timeline
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
    When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    // We also mock timeline object to spy few functions and mock others
    TimelineItemModel tim(&profile_move, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    // Create a request
    int tid1 = TrackModel::construct(timeline, -1, -1, QString(), true);
    int tid3 = TrackModel::construct(timeline, -1, -1, QString(), true);
    int tid2 = TrackModel::construct(timeline);
    int tid4 = TrackModel::construct(timeline);

    // Create clip with audio (40 frames long)
    // QString binId = createAVProducer(profile_move, binModel);
    QString binId = createProducerWithSound(profile_move, binModel, 100);

    // Setup insert stream data
    QMap<int, QString> audioInfo;
    audioInfo.insert(1, QStringLiteral("stream1"));
    timeline->m_binAudioTargets = audioInfo;

    // Create AV clip 1
    int cid1;
    int cid2;
    int cid3;
    int cid4;

    REQUIRE(timeline->requestClipInsertion(binId, tid2, 100, cid1));
    cid2 = timeline->getClipSplitPartner(cid1);

    SECTION("Ensure all clip instances on a track use the same producer")
    {
        REQUIRE(timeline->getItemTrackId(cid2) == tid3);
        REQUIRE(timeline->getItemTrackId(cid1) == tid2);
        Mlt::Producer prod1 = *(timeline->getClipPtr(cid1));
        Mlt::Producer prod2 = *(timeline->getClipPtr(cid2));
        // Clips on different tracks shoud not use the same producer
        REQUIRE(!prod1.same_clip(prod2));

        // Split clip
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid1, 110));
        cid3 = timeline->getClipByPosition(tid2, 111);
        cid4 = timeline->getClipSplitPartner(cid3);
        REQUIRE(timeline->getItemTrackId(cid4) == tid3);
        REQUIRE(timeline->getItemTrackId(cid3) == tid2);

        Mlt::Producer prod3 = *(timeline->getClipPtr(cid3));
        Mlt::Producer prod4 = *(timeline->getClipPtr(cid4));
        // Clips on different tracks shoud not use the same producer
        REQUIRE(!prod3.same_clip(prod4));
        // Clips on same track shoud use the same producer
        REQUIRE(prod1.same_clip(prod3));
        REQUIRE(prod2.same_clip(prod4));

        // Undo and redo cut, then ensure the producers are still correct
        undoStack->undo();
        undoStack->redo();

        prod3 = *(timeline->getClipPtr(cid3));
        prod4 = *(timeline->getClipPtr(cid4));
        // Clips on different tracks shoud not use the same producer
        REQUIRE(!prod3.same_clip(prod4));
        // Clips on same track shoud use the same producer
        REQUIRE(prod1.same_clip(prod3));
        REQUIRE(prod2.same_clip(prod4));
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
}
