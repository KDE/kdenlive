#include "doc/kdenlivedoc.h"
#include "test_utils.hpp"

using namespace fakeit;
Mlt::Profile profile_trimming;

TEST_CASE("Simple trimming operations", "[Trimming]")
{
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
    TimelineItemModel tim(&profile_trimming, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET(timMock)

    QString binId = createProducer(profile_trimming, "red", binModel);
    QString binId2 = createProducer(profile_trimming, "blue", binModel);
    QString binId3 = createProducerWithSound(profile_trimming, binModel);

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

    SECTION("Clip cutting")
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

        // require cut position outside the clip. Should return true and nothing is done
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid2, 0));
        state();
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid2, 5 * l));
        state();
        // cut on edges doesn't do anything either
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid2, l));
        state();
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid2, l + l - 5));
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

    SECTION("Cut and resize")
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

    SECTION("Clip cutting 2")
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

        // These functions will return true but do nothing
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid2, 0));
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid2, 5 * l));
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid2, l));
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid2, 2 * l));
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
        timeline->requestSetSelection(selection);

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

            REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>({audio1, audio3, audio2}));
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
            REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>({audio1, splitted1, audio2, audio3, splitted2, splitted3}));

            int g1 = timeline->m_groups->getDirectAncestor(audio1);
            int g2 = timeline->m_groups->getDirectAncestor(audio2);
            int g3 = timeline->m_groups->getDirectAncestor(audio3);
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
    SECTION("Cut should preserve AV groups")
    {
        QString binId3 = createProducerWithSound(profile_trimming, binModel);
        
        int tid6 = TrackModel::construct(timeline, -1, -1, QString(), true);
        int tid5 = TrackModel::construct(timeline);
    
        // Setup timeline audio drop info
        QMap <int, QString>audioInfo;
        audioInfo.insert(1,QStringLiteral("stream1"));
        timeline->m_binAudioTargets = audioInfo;
        timeline->m_videoTarget = tid5;

        int cid6 = -1;
        int pos = 3;
        REQUIRE(timeline->requestClipInsertion(binId3, tid5, pos, cid6, true, true, false));
        int cid7 = timeline->m_groups->getSplitPartner(cid6);
        int l = timeline->getClipPlaytime(cid6);
        REQUIRE(l >= 10);

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid5) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid6) == 1);
            REQUIRE(timeline->getClipTrackId(cid6) == tid5);
            REQUIRE(timeline->getClipTrackId(cid7) == tid6);
            REQUIRE(timeline->getClipPosition(cid6) == pos);
            REQUIRE(timeline->getClipPosition(cid7) == pos);
            REQUIRE(timeline->getClipPtr(cid6)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid7)->clipState() == PlaylistState::AudioOnly);

            // we check that the av group was correctly created
            REQUIRE(timeline->getGroupElements(cid6) == std::unordered_set<int>({cid6, cid7}));
            int g1 = timeline->m_groups->getDirectAncestor(cid6);
            REQUIRE(timeline->m_groups->getDirectChildren(g1) == std::unordered_set<int>({cid6, cid7}));
            REQUIRE(timeline->m_groups->getType(g1) == GroupType::AVSplit);
        };
        state();

        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid6, pos + 4));

        int cid8 = timeline->getClipByPosition(tid5, pos + 5);
        int cid9 = timeline->getClipByPosition(tid6, pos + 5);
        REQUIRE(cid8 >= 0);
        REQUIRE(cid9 >= 0);

        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid5) == 2);
            REQUIRE(timeline->getTrackClipsCount(tid6) == 2);
            REQUIRE(timeline->getClipTrackId(cid6) == tid5);
            REQUIRE(timeline->getClipTrackId(cid7) == tid6);
            REQUIRE(timeline->getClipTrackId(cid8) == tid5);
            REQUIRE(timeline->getClipTrackId(cid9) == tid6);

            REQUIRE(timeline->getClipPosition(cid6) == pos);
            REQUIRE(timeline->getClipPosition(cid7) == pos);
            REQUIRE(timeline->getClipPosition(cid8) == pos + 4);
            REQUIRE(timeline->getClipPosition(cid9) == pos + 4);

            REQUIRE(timeline->getClipPtr(cid6)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid7)->clipState() == PlaylistState::AudioOnly);
            REQUIRE(timeline->getClipPtr(cid8)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid9)->clipState() == PlaylistState::AudioOnly);

            // original AV group
            REQUIRE(timeline->getGroupElements(cid6) == std::unordered_set<int>({cid6, cid7}));
            int g1 = timeline->m_groups->getDirectAncestor(cid6);
            REQUIRE(timeline->m_groups->getDirectChildren(g1) == std::unordered_set<int>({cid6, cid7}));
            REQUIRE(timeline->m_groups->getType(g1) == GroupType::AVSplit);

            // new AV group
            REQUIRE(timeline->getGroupElements(cid8) == std::unordered_set<int>({cid8, cid9}));
            int g2 = timeline->m_groups->getDirectAncestor(cid8);
            REQUIRE(timeline->m_groups->getDirectChildren(g2) == std::unordered_set<int>({cid8, cid9}));
            REQUIRE(timeline->m_groups->getType(g2) == GroupType::AVSplit);
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

TEST_CASE("Spacer operations", "[Spacer]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
    When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
    When(Method(pmMock, getGuideModel)).AlwaysReturn(guideModel);

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    // We also mock timeline object to spy few functions and mock others
    TimelineItemModel tim(&profile_trimming, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET(timMock)

    QString binId = createProducer(profile_trimming, "red", binModel);
    QString binId2 = createProducer(profile_trimming, "blue", binModel);
    QString binId3 = createProducerWithSound(profile_trimming, binModel);

    int cid1 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int tid1 = TrackModel::construct(timeline);

    // Add an audio track
    //int tid4 = TrackModel::construct(timeline, -1, -1, QString(), true);
    int cid2 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);
    int cid3 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);

    //int audio1 = ClipModel::construct(timeline, binId3, -1, PlaylistState::VideoOnly);

    timeline->m_allClips[cid1]->m_endlessResize = false;
    timeline->m_allClips[cid2]->m_endlessResize = false;
    timeline->m_allClips[cid3]->m_endlessResize = false;

    SECTION("Simple one track space insertion")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        int l = timeline->getClipPlaytime(cid1);
        int p2 = l + 10;
        REQUIRE(timeline->requestClipMove(cid2, tid1, p2));
        int p3 = l + l + 20;
        REQUIRE(timeline->requestClipMove(cid3, tid1, p3));
        auto state = [&]() {
            REQUIRE(timeline->checkConsistency(guideModel->getSnapPoints()));
            REQUIRE(timeline->getClipPlaytime(cid1) == l);
            REQUIRE(timeline->getClipPlaytime(cid2) == l);
            REQUIRE(timeline->getClipPlaytime(cid3) == l);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == p2);
            REQUIRE(timeline->getClipPosition(cid3) == p3);

            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l - 1);
        };
        state();

        int spacerIid = TimelineFunctions::requestSpacerStartOperation(timeline, tid1, l + 5);
        REQUIRE(spacerIid > -1);
        std::function<bool(void)> undo = []() { return true; };
        std::function<bool(void)> redo = []() { return true; };
        int itemPos = timeline->getItemPosition(spacerIid);
        int space = 18;
        REQUIRE(TimelineFunctions::requestSpacerEndOperation(timeline, spacerIid, itemPos, itemPos + space, tid1, false, undo, redo));
        auto state1 = [&]() {
            REQUIRE(timeline->checkConsistency(guideModel->getSnapPoints()));
            REQUIRE(timeline->getClipPlaytime(cid1) == l);
            REQUIRE(timeline->getClipPlaytime(cid2) == l);
            REQUIRE(timeline->getClipPlaytime(cid3) == l);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == p2 + space);
            REQUIRE(timeline->getClipPosition(cid3) == p3 + space);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l - 1);
        };
        state1();

        int startPos = timeline->getClipPosition(cid3) + l/2;
        spacerIid = TimelineFunctions::requestSpacerStartOperation(timeline, tid1, startPos);
        REQUIRE(spacerIid > -1);
        undo = []() { return true; };
        redo = []() { return true; };
        itemPos = timeline->getItemPosition(spacerIid);
        int space2 = 3;
        REQUIRE(TimelineFunctions::requestSpacerEndOperation(timeline, spacerIid, itemPos, itemPos + space2, tid1, false, undo, redo));
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency(guideModel->getSnapPoints()));
            REQUIRE(timeline->getClipPlaytime(cid1) == l);
            REQUIRE(timeline->getClipPlaytime(cid2) == l);
            REQUIRE(timeline->getClipPlaytime(cid3) == l);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == p2 + space);
            REQUIRE(timeline->getClipPosition(cid3) == p3 + space + space2);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l - 1);
        };
        state2();

        undoStack->undo();
        state1();
        undoStack->undo();
        state();

        undoStack->redo();
        state1();
        undoStack->redo();
        state2();
    }

    SECTION("Simple one track space insertion with guides")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        int l = timeline->getClipPlaytime(cid1);
        int p2 = l + 10;
        REQUIRE(timeline->requestClipMove(cid2, tid1, p2));
        int p3 = l + l + 20;
        REQUIRE(timeline->requestClipMove(cid3, tid1, p3));
        guideModel->addMarker(GenTime(l/2, profile_trimming.fps()), "guide1");
        guideModel->addMarker(GenTime(l+2, profile_trimming.fps()), "guide2");
        guideModel->addMarker(GenTime(l+7, profile_trimming.fps()), "guide3");
        guideModel->addMarker(GenTime(p2 + l/2, profile_trimming.fps()), "guide4");
        guideModel->addMarker(GenTime(p3, profile_trimming.fps()), "guide5");
        auto state = [&]() {
            REQUIRE(timeline->checkConsistency(guideModel->getSnapPoints()));
            REQUIRE(timeline->getClipPlaytime(cid1) == l);
            REQUIRE(timeline->getClipPlaytime(cid2) == l);
            REQUIRE(timeline->getClipPlaytime(cid3) == l);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == p2);
            REQUIRE(timeline->getClipPosition(cid3) == p3);

            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l - 1);
        };
        state();

        int spacerIid = TimelineFunctions::requestSpacerStartOperation(timeline, tid1, l + 5);
        REQUIRE(spacerIid > -1);
        std::function<bool(void)> undo = []() { return true; };
        std::function<bool(void)> redo = []() { return true; };
        int itemPos = timeline->getItemPosition(spacerIid);
        int space = 18;
        REQUIRE(TimelineFunctions::requestSpacerEndOperation(timeline, spacerIid, itemPos, itemPos + space, tid1, true, undo, redo));
        auto state1 = [&]() {
            REQUIRE(guideModel->getSnapPoints() == std::vector<int>{l/2, l+2, l+7, p2 + l/2 + space, p3 + space});
            REQUIRE(timeline->checkConsistency(guideModel->getSnapPoints()));
            REQUIRE(timeline->getClipPlaytime(cid1) == l);
            REQUIRE(timeline->getClipPlaytime(cid2) == l);
            REQUIRE(timeline->getClipPlaytime(cid3) == l);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == p2 + space);
            REQUIRE(timeline->getClipPosition(cid3) == p3 + space);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l - 1);
        };
        state1();

        int startPos = timeline->getClipPosition(cid3) + l/2;
        spacerIid = TimelineFunctions::requestSpacerStartOperation(timeline, tid1, startPos);
        REQUIRE(spacerIid > -1);
        undo = []() { return true; };
        redo = []() { return true; };
        itemPos = timeline->getItemPosition(spacerIid);
        int space2 = 3;
        REQUIRE(TimelineFunctions::requestSpacerEndOperation(timeline, spacerIid, itemPos, itemPos + space2, tid1, true, undo, redo));
        auto state2 = [&]() {
            REQUIRE(guideModel->getSnapPoints() == std::vector<int>{l/2, l+2, l+7, p2 + l/2 + space, p3 + space + space2});
            REQUIRE(timeline->checkConsistency(guideModel->getSnapPoints()));
            REQUIRE(timeline->getClipPlaytime(cid1) == l);
            REQUIRE(timeline->getClipPlaytime(cid2) == l);
            REQUIRE(timeline->getClipPlaytime(cid3) == l);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == p2 + space);
            REQUIRE(timeline->getClipPosition(cid3) == p3 + space + space2);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l - 1);
        };
        state2();

        undoStack->undo();
        state1();
        undoStack->undo();
        state();

        undoStack->redo();
        state1();
        undoStack->redo();
        state2();
    }

    SECTION("Simple one track space deletion")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        int l = timeline->getClipPlaytime(cid1);
        int p2 = l + 10;
        REQUIRE(timeline->requestClipMove(cid2, tid1, p2));
        int p3 = l + l + 10;
        REQUIRE(timeline->requestClipMove(cid3, tid1, p3));
        auto state = [&]() {
            REQUIRE(timeline->checkConsistency(guideModel->getSnapPoints()));
            REQUIRE(timeline->getClipPlaytime(cid1) == l);
            REQUIRE(timeline->getClipPlaytime(cid2) == l);
            REQUIRE(timeline->getClipPlaytime(cid3) == l);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == p2);
            REQUIRE(timeline->getClipPosition(cid3) == p3);

            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l - 1);
        };
        state();

        int spacerIid = TimelineFunctions::requestSpacerStartOperation(timeline, tid1, l + 5);
        REQUIRE(spacerIid > -1);
        std::function<bool(void)> undo = []() { return true; };
        std::function<bool(void)> redo = []() { return true; };
        int itemPos = timeline->getItemPosition(spacerIid);
        // space to remove is larger than possible (at the end only 10 frames should be removed)
        int space = 18;
        REQUIRE(TimelineFunctions::requestSpacerEndOperation(timeline, spacerIid, itemPos, itemPos - space, tid1, false, undo, redo));
        auto state1 = [&]() {
            REQUIRE(timeline->checkConsistency(guideModel->getSnapPoints()));
            REQUIRE(timeline->getClipPlaytime(cid1) == l);
            REQUIRE(timeline->getClipPlaytime(cid2) == l);
            REQUIRE(timeline->getClipPlaytime(cid3) == l);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == p2 - 10);
            REQUIRE(timeline->getClipPosition(cid3) == p3 - 10);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l - 1);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l - 1);
        };
        state1();

        int startPos = timeline->getClipPosition(cid3) + l/2;
        spacerIid = TimelineFunctions::requestSpacerStartOperation(timeline, tid1, startPos);
        REQUIRE(spacerIid > -1);
        undo = []() { return true; };
        redo = []() { return true; };
        itemPos = timeline->getItemPosition(spacerIid);
        int space2 = 3;
        REQUIRE(!TimelineFunctions::requestSpacerEndOperation(timeline, spacerIid, itemPos, itemPos - space2, tid1, false, undo, redo));
        state1();

        undoStack->undo();
        state();

        undoStack->redo();
        state1();
    }


    binModel->clean();
    pCore->m_projectManager = nullptr;
}

TEST_CASE("Insert/delete", "[Trimming2]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
    When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
    When(Method(pmMock, getGuideModel)).AlwaysReturn(guideModel);

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    // We also mock timeline object to spy few functions and mock others
    TimelineItemModel tim(&profile_trimming, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET(timMock);

    QString binId = createProducerWithSound(profile_trimming, binModel);

    TrackModel::construct(timeline, -1, -1, QString(), true);
    int tid2 = TrackModel::construct(timeline, -1, -1, QString(), true);
    int tid1 = TrackModel::construct(timeline);
    TrackModel::construct(timeline);
    
    // Setup timeline audio drop info
    QMap <int, QString>audioInfo;
    audioInfo.insert(1,QStringLiteral("stream1"));
    timeline->m_binAudioTargets = audioInfo;
    timeline->m_videoTarget = tid1;

    SECTION("Remove Space should preserve groups")
    {

        int cid1 = -1;
        REQUIRE(timeline->requestClipInsertion(binId, tid1, 3, cid1, true, true, false));
        int cid2 = timeline->m_groups->getSplitPartner(cid1);

        auto state = [&](int pos) {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid2);
            REQUIRE(timeline->getClipPosition(cid1) == pos);
            REQUIRE(timeline->getClipPosition(cid2) == pos);
            REQUIRE(timeline->getClipPtr(cid1)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid2)->clipState() == PlaylistState::AudioOnly);
            // we check that the av group was correctly created
            REQUIRE(timeline->getGroupElements(cid1) == std::unordered_set<int>({cid1, cid2}));
            int g1 = timeline->m_groups->getDirectAncestor(cid1);
            REQUIRE(timeline->m_groups->getDirectChildren(g1) == std::unordered_set<int>({cid1, cid2}));
            REQUIRE(timeline->m_groups->getType(g1) == GroupType::AVSplit);
        };
        state(3);

        REQUIRE(TimelineFunctions::requestDeleteBlankAt(timeline, tid1, 1, true));
        state(0);

        undoStack->undo();
        state(3);
        undoStack->redo();
        state(0);
    }

    SECTION("Insert zone should preserve groups")
    {
        int cid1 = -1;
        REQUIRE(timeline->requestClipInsertion(binId, tid1, 3, cid1, true, true, false));
        int cid2 = timeline->m_groups->getSplitPartner(cid1);

        int l = timeline->getClipPlaytime(cid2);
        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid2);
            REQUIRE(timeline->getClipPosition(cid1) == 3);
            REQUIRE(timeline->getClipPosition(cid2) == 3);
            REQUIRE(timeline->getClipPtr(cid1)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid2)->clipState() == PlaylistState::AudioOnly);
            // we check that the av group was correctly created
            REQUIRE(timeline->getGroupElements(cid1) == std::unordered_set<int>({cid1, cid2}));
            int g1 = timeline->m_groups->getDirectAncestor(cid1);
            REQUIRE(timeline->m_groups->getDirectChildren(g1) == std::unordered_set<int>({cid1, cid2}));
            REQUIRE(timeline->m_groups->getType(g1) == GroupType::AVSplit);
        };
        state();

        timeline->m_audioTarget.insert(tid2, 0);
        timeline->m_videoTarget = tid1;
        // Make tracks active
        timeline->setTrackProperty(tid1, QStringLiteral("kdenlive:timeline_active"), QStringLiteral("1"));
        timeline->setTrackProperty(tid2, QStringLiteral("kdenlive:timeline_active"), QStringLiteral("1"));
        REQUIRE(TimelineFunctions::insertZone(timeline, {tid1, tid2}, binId, 3 + 2, {l / 4, 3 * l / 4}, false));
        timeline->m_audioTarget.clear();
        timeline->m_videoTarget = -1;
        int small_length = 3 * l / 4 - l / 4;
        int cid3 = timeline->getClipByPosition(tid1, 3 + 2);
        int cid4 = timeline->getClipByPosition(tid2, 3 + 2);
        int cid5 = timeline->getClipByPosition(tid1, 3 + 2 + small_length);
        int cid6 = timeline->getClipByPosition(tid2, 3 + 2 + small_length);

        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 3);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid2);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipTrackId(cid4) == tid2);
            REQUIRE(timeline->getClipTrackId(cid5) == tid1);
            REQUIRE(timeline->getClipTrackId(cid6) == tid2);
            REQUIRE(timeline->getClipPosition(cid1) == 3);
            REQUIRE(timeline->getClipPosition(cid2) == 3);
            REQUIRE(timeline->getClipPosition(cid3) == 3 + 2);
            REQUIRE(timeline->getClipPosition(cid4) == 3 + 2);
            REQUIRE(timeline->getClipPosition(cid5) == 3 + 2 + small_length);
            REQUIRE(timeline->getClipPosition(cid6) == 3 + 2 + small_length);
            REQUIRE(timeline->getClipPlaytime(cid1) + timeline->getClipPlaytime(cid5) == l);
            REQUIRE(timeline->getClipPlaytime(cid1) == 2);
            REQUIRE(timeline->getClipPlaytime(cid3) == small_length);
            REQUIRE(timeline->getClipPtr(cid1)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid2)->clipState() == PlaylistState::AudioOnly);
            REQUIRE(timeline->getClipPtr(cid3)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid4)->clipState() == PlaylistState::AudioOnly);
            REQUIRE(timeline->getClipPtr(cid5)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid6)->clipState() == PlaylistState::AudioOnly);
            // we check that the av group was correctly created
            REQUIRE(timeline->getGroupElements(cid1) == std::unordered_set<int>({cid1, cid2}));
            int g1 = timeline->m_groups->getDirectAncestor(cid1);
            REQUIRE(timeline->m_groups->getDirectChildren(g1) == std::unordered_set<int>({cid1, cid2}));
            REQUIRE(timeline->m_groups->getType(g1) == GroupType::AVSplit);

            REQUIRE(timeline->getGroupElements(cid3) == std::unordered_set<int>({cid3, cid4}));
            int g2 = timeline->m_groups->getDirectAncestor(cid3);
            REQUIRE(timeline->m_groups->getDirectChildren(g2) == std::unordered_set<int>({cid3, cid4}));
            REQUIRE(timeline->m_groups->getType(g2) == GroupType::AVSplit);

            int g3 = timeline->m_groups->getDirectAncestor(cid5);
            REQUIRE(timeline->m_groups->getDirectChildren(g3) == std::unordered_set<int>({cid5, cid6}));
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

TEST_CASE("Copy/paste", "[CP]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);

    // Here we do some trickery to enable testing.

    // we mock a doc to stub the getDocumentProperty

    Mock<KdenliveDoc> docMock;
    When(Method(docMock, getDocumentProperty)).AlwaysDo([](const QString &name, const QString &defaultValue) {
        Q_UNUSED(name) Q_UNUSED(defaultValue)
        qDebug() << "Intercepted call";
        return QStringLiteral("dummyId");
    });

    KdenliveDoc &mockedDoc = docMock.get();

    // We mock the project class so that the undoStack function returns our undoStack, and our mocked document
    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
    When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
    When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    // We also mock timeline object to spy few functions and mock others
    TimelineItemModel tim(&profile_trimming, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET(timMock)

    QString binId = createProducerWithSound(profile_trimming, binModel);
    QString binId2 = createProducer(profile_trimming, "red", binModel);

    int tid2b = TrackModel::construct(timeline, -1, -1, QString(), true);
    int tid2 = TrackModel::construct(timeline, -1, -1, QString(), true);
    int tid1 = TrackModel::construct(timeline);
    int tid1b = TrackModel::construct(timeline);
    
    // Setup timeline audio drop info
    QMap <int, QString>audioInfo;
    audioInfo.insert(1,QStringLiteral("stream1"));
    timeline->m_binAudioTargets = audioInfo;
    timeline->m_videoTarget = tid1;

    SECTION("Simple copy paste of one clip")
    {

        int cid1 = -1;
        REQUIRE(timeline->requestClipInsertion(binId2, tid1, 3, cid1, true, true, false));
        int l = timeline->getClipPlaytime(cid1);
        int cid2 = -1;
        REQUIRE(timeline->requestClipInsertion(binId2, tid1, 3 + l, cid2, true, true, false));

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 3);
            REQUIRE(timeline->getClipPosition(cid2) == 3 + l);
        };
        state();

        QString cpy_str = TimelineFunctions::copyClips(timeline, {cid1});

        // Try to paste in invalid positions
        REQUIRE_FALSE(TimelineFunctions::pasteClips(timeline, cpy_str, tid1, 0));
        state();
        REQUIRE_FALSE(TimelineFunctions::pasteClips(timeline, cpy_str, tid1, 4));
        state();
        REQUIRE_FALSE(TimelineFunctions::pasteClips(timeline, cpy_str, tid1, 4 + l));
        state();
        // Paste in audio track
        REQUIRE_FALSE(TimelineFunctions::pasteClips(timeline, cpy_str, tid2, 0));
        state();
        REQUIRE_FALSE(TimelineFunctions::pasteClips(timeline, cpy_str, tid2b, 0));
        state();

        // Paste after the last clip
        REQUIRE(TimelineFunctions::pasteClips(timeline, cpy_str, tid1, 3 + 2 * l));
        int cid3 = timeline->getTrackById(tid1)->getClipByPosition(3 + 2 * l + 1);
        REQUIRE(cid3 != -1);
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 3);
            REQUIRE(timeline->getClipPosition(cid2) == 3 + l);
            REQUIRE(timeline->getClipPosition(cid3) == 3 + 2 * l);
        };
        state2();

        undoStack->undo();
        state();
        undoStack->redo();
        state2();

        // Paste in different track
        REQUIRE(TimelineFunctions::pasteClips(timeline, cpy_str, tid1b, 0));
        int cid4 = timeline->getTrackById(tid1b)->getClipByPosition(0);
        REQUIRE(cid4 != -1);
        auto state3 = [&]() {
            state2();
            REQUIRE(timeline->getTrackClipsCount(tid1b) == 1);
            REQUIRE(timeline->getClipTrackId(cid4) == tid1b);
            REQUIRE(timeline->getClipPosition(cid4) == 0);
        };
        state3();

        undoStack->undo();
        state2();
        undoStack->undo();
        state();
        undoStack->redo();
        state2();
        undoStack->redo();
        state3();
    }

    SECTION("Copy paste groups")
    {

        auto state0 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 0);
            REQUIRE(timeline->getTrackClipsCount(tid1b) == 0);
            REQUIRE(timeline->getTrackClipsCount(tid2b) == 0);
        };
        state0();
        int cid1 = -1;
        REQUIRE(timeline->requestClipInsertion(binId, tid1, 3, cid1, true, true, false));
        int l = timeline->getClipPlaytime(cid1);
        int cid2 = timeline->m_groups->getSplitPartner(cid1);

        auto state = [&](int count1, int count2) {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == count1);
            REQUIRE(timeline->getTrackClipsCount(tid2) == count1);
            REQUIRE(timeline->getTrackClipsCount(tid1b) == count2);
            REQUIRE(timeline->getTrackClipsCount(tid2b) == count2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid2);
            REQUIRE(timeline->getClipPosition(cid1) == 3);
            REQUIRE(timeline->getClipPosition(cid2) == 3);
            REQUIRE(timeline->getClipPtr(cid1)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid2)->clipState() == PlaylistState::AudioOnly);
            // we check that the av group was correctly created
            REQUIRE(timeline->getGroupElements(cid1) == std::unordered_set<int>({cid1, cid2}));
            int g1 = timeline->m_groups->getDirectAncestor(cid1);
            REQUIRE(timeline->m_groups->getDirectChildren(g1) == std::unordered_set<int>({cid1, cid2}));
            REQUIRE(timeline->m_groups->getType(g1) == GroupType::AVSplit);
        };
        state(1, 0);

        QString cpy_str = TimelineFunctions::copyClips(timeline, {cid1});

        REQUIRE_FALSE(TimelineFunctions::pasteClips(timeline, cpy_str, tid1, 0));
        state(1, 0);
        REQUIRE_FALSE(TimelineFunctions::pasteClips(timeline, cpy_str, tid1, 4));
        state(1, 0);

        // potentially annoying selection
        REQUIRE(timeline->requestSetSelection({cid1}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid1, cid2});

        // paste on same track, after clip
        REQUIRE(TimelineFunctions::pasteClips(timeline, cpy_str, tid1, 3 + 2 * l));
        int cid3 = timeline->getTrackById(tid1)->getClipByPosition(3 + 2 * l + 1);
        REQUIRE(cid3 != -1);
        int cid4 = timeline->m_groups->getSplitPartner(cid3);
        auto state2 = [&](int count1, int count2) {
            state(count1, count2);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipTrackId(cid4) == tid2);
            REQUIRE(timeline->getClipPosition(cid3) == 3 + 2 * l);
            REQUIRE(timeline->getClipPosition(cid4) == 3 + 2 * l);
            REQUIRE(timeline->getClipPtr(cid3)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid4)->clipState() == PlaylistState::AudioOnly);
            // we check that the av group was correctly created
            REQUIRE(timeline->getGroupElements(cid3) == std::unordered_set<int>({cid3, cid4}));
            int g1 = timeline->m_groups->getDirectAncestor(cid3);
            REQUIRE(timeline->m_groups->getDirectChildren(g1) == std::unordered_set<int>({cid3, cid4}));
            REQUIRE(timeline->m_groups->getType(g1) == GroupType::AVSplit);
        };
        state2(2, 0);

        // potentially annoying selection
        REQUIRE(timeline->requestSetSelection({cid1}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid1, cid2});
        undoStack->undo();
        REQUIRE(timeline->requestClearSelection());
        state(1, 0);

        // potentially annoying selection
        REQUIRE(timeline->requestSetSelection({cid1}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid1, cid2});
        undoStack->redo();
        REQUIRE(timeline->requestClearSelection());
        state2(2, 0);

        // another potentially annoying selection
        REQUIRE(timeline->requestSetSelection({cid1, cid3}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid1, cid2, cid3, cid4});
        undoStack->undo();
        REQUIRE(timeline->requestClearSelection());
        state(1, 0);
        REQUIRE(timeline->requestSetSelection({cid1}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid1, cid2});
        undoStack->redo();
        REQUIRE(timeline->requestClearSelection());
        state2(2, 0);

        // now, we copy the old clips as well as those we just pasted. Let's do it using a selection
        REQUIRE(timeline->requestSetSelection({cid1, cid3}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid1, cid2, cid3, cid4});
        // since everything is selected, everything should be copied here
        cpy_str = TimelineFunctions::copyClips(timeline, {cid1});

        REQUIRE_FALSE(TimelineFunctions::pasteClips(timeline, cpy_str, tid1, 0));
        state2(2, 0);

        // parasitic selection
        REQUIRE(timeline->requestSetSelection({cid1, cid3}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid1, cid2, cid3, cid4});

        // We paste on the other track
        REQUIRE(TimelineFunctions::pasteClips(timeline, cpy_str, tid1b, 0));
        qDebug() << "track tid1 count" << timeline->getTrackClipsCount(tid1);
        qDebug() << "track tid2 count" << timeline->getTrackClipsCount(tid2);
        qDebug() << "track tid1b count" << timeline->getTrackClipsCount(tid1b);
        qDebug() << "track tid2b count" << timeline->getTrackClipsCount(tid2b);
        int cid5 = timeline->getTrackById(tid1b)->getClipByPosition(0);
        REQUIRE(cid5 != -1);
        int cid6 = timeline->m_groups->getSplitPartner(cid5);
        REQUIRE(cid6 != -1);
        int cid7 = timeline->getTrackById(tid1b)->getClipByPosition(2 * l + 1);
        REQUIRE(cid7 != -1);
        int cid8 = timeline->m_groups->getSplitPartner(cid7);
        REQUIRE(cid8 != -1);
        auto state3 = [&](int count1, int count2) {
            state2(count1, count2);
            REQUIRE(timeline->getClipTrackId(cid5) == tid1b);
            REQUIRE(timeline->getClipTrackId(cid7) == tid1b);
            REQUIRE(timeline->getClipTrackId(cid6) == tid2b);
            REQUIRE(timeline->getClipTrackId(cid8) == tid2b);
            REQUIRE(timeline->getClipPosition(cid5) == 0);
            REQUIRE(timeline->getClipPosition(cid6) == 0);
            REQUIRE(timeline->getClipPosition(cid7) == 2 * l);
            REQUIRE(timeline->getClipPosition(cid8) == 2 * l);
            REQUIRE(timeline->getClipPtr(cid5)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid6)->clipState() == PlaylistState::AudioOnly);
            REQUIRE(timeline->getClipPtr(cid7)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid8)->clipState() == PlaylistState::AudioOnly);
            // we check that the av group was correctly created
            REQUIRE(timeline->getGroupElements(cid5) == std::unordered_set<int>({cid5, cid6}));
            int g1 = timeline->m_groups->getDirectAncestor(cid5);
            REQUIRE(timeline->m_groups->getDirectChildren(g1) == std::unordered_set<int>({cid5, cid6}));
            REQUIRE(timeline->m_groups->getType(g1) == GroupType::AVSplit);

            REQUIRE(timeline->getGroupElements(cid7) == std::unordered_set<int>({cid7, cid8}));
            int g2 = timeline->m_groups->getDirectAncestor(cid7);
            REQUIRE(timeline->m_groups->getDirectChildren(g2) == std::unordered_set<int>({cid7, cid8}));
            REQUIRE(timeline->m_groups->getType(g2) == GroupType::AVSplit);
        };
        state3(2, 2);

        // parasitic selection
        REQUIRE(timeline->requestSetSelection({cid1, cid3}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid1, cid2, cid3, cid4});
        undoStack->undo();
        REQUIRE(timeline->requestClearSelection());
        state2(2, 0);
        REQUIRE(timeline->requestSetSelection({cid1, cid3}));
        REQUIRE(timeline->getCurrentSelection() == std::unordered_set<int>{cid1, cid2, cid3, cid4});
        undoStack->redo();
        REQUIRE(timeline->requestClearSelection());
        state3(2, 2);
    }

    SECTION("Paste when tracks get deleted")
    {
        auto state0 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 0);
            REQUIRE(timeline->getTrackClipsCount(tid1b) == 0);
            REQUIRE(timeline->getTrackClipsCount(tid2b) == 0);
        };
        state0();
        int cid1 = -1;
        REQUIRE(timeline->requestClipInsertion(binId, tid1, 3, cid1, true, true, false));
        timeline->getClipPlaytime(cid1);
        int cid2 = timeline->m_groups->getSplitPartner(cid1);

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid1b) == 0);
            REQUIRE(timeline->getTrackClipsCount(tid2b) == 0);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid2);
            REQUIRE(timeline->getClipPosition(cid1) == 3);
            REQUIRE(timeline->getClipPosition(cid2) == 3);
            REQUIRE(timeline->getClipPtr(cid1)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid2)->clipState() == PlaylistState::AudioOnly);
            // we check that the av group was correctly created
            REQUIRE(timeline->getGroupElements(cid1) == std::unordered_set<int>({cid1, cid2}));
            int g1 = timeline->m_groups->getDirectAncestor(cid1);
            REQUIRE(timeline->m_groups->getDirectChildren(g1) == std::unordered_set<int>({cid1, cid2}));
            REQUIRE(timeline->m_groups->getType(g1) == GroupType::AVSplit);
        };
        state();

        QString cpy_str = TimelineFunctions::copyClips(timeline, {cid1});

        // we delete origin of the copy, paste should still be possible
        REQUIRE(timeline->requestItemDeletion(cid1));
        state0();

        REQUIRE(TimelineFunctions::pasteClips(timeline, cpy_str, tid1, 0));
        int cid3 = timeline->getTrackById(tid1)->getClipByPosition(0);
        REQUIRE(cid3 != -1);
        int cid4 = timeline->m_groups->getSplitPartner(cid3);
        auto state2 = [&](int audio) {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getTrackClipsCount(audio) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid1b) == 0);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipTrackId(cid4) == audio);
            REQUIRE(timeline->getClipPosition(cid3) == 0);
            REQUIRE(timeline->getClipPosition(cid4) == 0);
            REQUIRE(timeline->getClipPtr(cid3)->clipState() == PlaylistState::VideoOnly);
            REQUIRE(timeline->getClipPtr(cid4)->clipState() == PlaylistState::AudioOnly);
            // we check that the av group was correctly created
            REQUIRE(timeline->getGroupElements(cid3) == std::unordered_set<int>({cid3, cid4}));
            int g1 = timeline->m_groups->getDirectAncestor(cid3);
            REQUIRE(timeline->m_groups->getDirectChildren(g1) == std::unordered_set<int>({cid3, cid4}));
            REQUIRE(timeline->m_groups->getType(g1) == GroupType::AVSplit);
        };
        state2(tid2);

        undoStack->undo();
        state0();
        undoStack->redo();
        state2(tid2);
        undoStack->undo();
        state0();

        // now, we remove all audio tracks, making paste impossible
        REQUIRE(timeline->requestTrackDeletion(tid2));
        REQUIRE(timeline->requestTrackDeletion(tid2b));
        REQUIRE_FALSE(TimelineFunctions::pasteClips(timeline, cpy_str, tid1, 0));

        // undo one deletion
        undoStack->undo();
        // now, tid2b should be a valid audio track
        REQUIRE(TimelineFunctions::pasteClips(timeline, cpy_str, tid1, 0));
        cid3 = timeline->getTrackById(tid1)->getClipByPosition(0);
        REQUIRE(cid3 != -1);
        cid4 = timeline->m_groups->getSplitPartner(cid3);
        state2(tid2b);
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
}

TEST_CASE("Advanced trimming operations: Slip", "[TrimmingSlip]")
{
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
    TimelineItemModel tim(&profile_trimming, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET(timMock)

    QString binId = createProducer(profile_trimming, "red", binModel);
    QString binId2 = createProducer(profile_trimming, "blue", binModel);

    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);

    int cid1 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int cid2 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);
    int cid3 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int cid4 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int cid5 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);

    timeline->m_allClips[cid1]->m_endlessResize = false;
    timeline->m_allClips[cid2]->m_endlessResize = false;
    timeline->m_allClips[cid3]->m_endlessResize = false;
    timeline->m_allClips[cid4]->m_endlessResize = false;
    timeline->m_allClips[cid5]->m_endlessResize = false;

    // sliping a fullsized clips should not to anything
    SECTION("Slip single fullsized clip")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        int l = timeline->getClipPlaytime(cid1);
        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid1) == l);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 1);
        };
        state();

        REQUIRE(timeline->requestClipSlip(cid1, 3) == 3);
        state();

        REQUIRE(timeline->requestClipSlip(cid1, -3) == -3);
        state();

        undoStack->undo();
        state();
        undoStack->undo();
        state();

        undoStack->redo();
        state();
        undoStack->redo();
        state();
    }

    // slipping a downsized clip should only change the in and out point
    SECTION("Slip single cutted clip")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        int l = timeline->getClipPlaytime(cid1);
        REQUIRE(timeline->requestItemResize(cid1, l - 5, true) == l - 5);
        REQUIRE(timeline->requestItemResize(cid1, l - 11, false) == l - 11);

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid1) == l - 11);
            REQUIRE(timeline->getClipPosition(cid1) == 5 + 6);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 6);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 6);
        };
        state();

        REQUIRE(timeline->requestClipSlip(cid1, 3) == 3);
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid1) == l - 11);
            REQUIRE(timeline->getClipPosition(cid1) == 5 + 6);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 3);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 9);
        };
        state2();

        REQUIRE(timeline->requestClipSlip(cid1, -3) == -3);
        state();

        undoStack->undo();
        state2();
        undoStack->undo();
        state();

        undoStack->redo();
        state2();
        undoStack->redo();
        state();

    }

    // if offset is bigger than the borders use the biggest possible offset without going beyond the borders
    SECTION("Slip beyond borders")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        int l = timeline->getClipPlaytime(cid1);

        REQUIRE(timeline->getClipPlaytime(cid1) == l);
        REQUIRE(timeline->requestItemResize(cid1, l - 5, true) == l - 5);
        REQUIRE(timeline->requestItemResize(cid1, l - 11, false) == l - 11);
        REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 6);


        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid1) == l - 11);
            REQUIRE(timeline->getClipPosition(cid1) == 5 + 6);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 6);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 6);
        };
        state();

        // left border
        REQUIRE(timeline->requestClipSlip(cid1, 30) == 30);
        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid1) == l - 11);
            REQUIRE(timeline->getClipPosition(cid1) == 5 + 6);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 12);
        };
        state2();

        // right border
        REQUIRE(timeline->requestClipSlip(cid1, -30) == -30);
        auto state3 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid1) == l - 11);
            REQUIRE(timeline->getClipPosition(cid1) == 5 + 6);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 11);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l - 1);
        };
        state3();

        undoStack->undo();
        state2();
        undoStack->undo();
        state();

        undoStack->redo();
        state2();
        undoStack->redo();
        state3();
    }

    // slipping one clip of a group should slip all members
    SECTION("Slip group")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        int l1 = timeline->getClipPlaytime(cid1);
        REQUIRE(timeline->requestItemResize(cid1, l1 - 5, true) == l1 - 5);
        REQUIRE(timeline->requestItemResize(cid1, l1 - 11, false) == l1 - 11);

        REQUIRE(timeline->requestClipMove(cid2, tid1, 50));
        int l2 = timeline->getClipPlaytime(cid2);
        REQUIRE(timeline->requestItemResize(cid2, l2 - 11, false) == l2 - 11);

        REQUIRE(timeline->requestClipMove(cid3, tid2, 25));
        int l3 = timeline->getClipPlaytime(cid3);
        REQUIRE(timeline->requestItemResize(cid3, l3 - 5, true) == l3 - 5);

        REQUIRE(timeline->requestClipMove(cid4, tid2, 0));
        int l4 = timeline->getClipPlaytime(cid4);
        REQUIRE(timeline->requestItemResize(cid4, l4 - 9, true) == l4 - 9);
        REQUIRE(timeline->requestItemResize(cid4, l4 - 17, false) == l4 - 17);

        REQUIRE(timeline->requestClipMove(cid5, tid2, 60));
        int l5 = timeline->getClipPlaytime(cid5);
        int gid1 = timeline->requestClipsGroup(std::unordered_set<int>({cid1, cid2, cid3, cid4, cid5}), true, GroupType::Normal);
        REQUIRE(gid1 > -1);

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid1) == l1 - 11);
            REQUIRE(timeline->getClipPosition(cid1) == 5 + 6);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 6);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l1 - 6);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);

            REQUIRE(timeline->getClipPlaytime(cid2) == l2 - 11);
            REQUIRE(timeline->getClipPosition(cid2) == 50 + 11);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 11);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l2 -1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);

            REQUIRE(timeline->getClipPlaytime(cid3) == l3 - 5);
            REQUIRE(timeline->getClipPosition(cid3) == 25);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l3 - 6);
            REQUIRE(timeline->getClipTrackId(cid3) == tid2);

            REQUIRE(timeline->getClipPlaytime(cid4) == l4 - 17);
            REQUIRE(timeline->getClipPosition(cid4) == 8);
            REQUIRE(timeline->getClipPtr(cid4)->getIn() == 8);
            REQUIRE(timeline->getClipPtr(cid4)->getOut() == l4 - 10);
            REQUIRE(timeline->getClipTrackId(cid4) == tid2);

            REQUIRE(timeline->getClipPlaytime(cid5) == l5);
            REQUIRE(timeline->getClipPosition(cid5) == 60);
            REQUIRE(timeline->getClipPtr(cid5)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid5)->getOut() == l5 - 1);
            REQUIRE(timeline->getClipTrackId(cid5) == tid2);

            REQUIRE(timeline->m_groups->getLeaves(gid1) == std::unordered_set<int>({cid1, cid2, cid3, cid4, cid5}));
        };
        state();

        REQUIRE(timeline->requestClipSlip(cid1, 3) == 3);

        auto state2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid1) == l1 - 11);
            REQUIRE(timeline->getClipPosition(cid1) == 5 + 6);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 3);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l1 - 9);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);

            REQUIRE(timeline->getClipPlaytime(cid2) == l2 - 11);
            REQUIRE(timeline->getClipPosition(cid2) == 50 + 11);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 8);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l2 - 4);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);

            REQUIRE(timeline->getClipPlaytime(cid3) == l3 - 5);
            REQUIRE(timeline->getClipPosition(cid3) == 25);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l3 - 6);
            REQUIRE(timeline->getClipTrackId(cid3) == tid2);

            REQUIRE(timeline->getClipPlaytime(cid4) == l4 - 17);
            REQUIRE(timeline->getClipPosition(cid4) == 8);
            REQUIRE(timeline->getClipPtr(cid4)->getIn() == 5);
            REQUIRE(timeline->getClipPtr(cid4)->getOut() == l4 - 13);
            REQUIRE(timeline->getClipTrackId(cid4) == tid2);

            REQUIRE(timeline->getClipPlaytime(cid5) == l5);
            REQUIRE(timeline->getClipPosition(cid5) == 60);
            REQUIRE(timeline->getClipPtr(cid5)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid5)->getOut() == l5 - 1);
            REQUIRE(timeline->getClipTrackId(cid5) == tid2);

            REQUIRE(timeline->m_groups->getLeaves(gid1) == std::unordered_set<int>({cid1, cid2, cid3, cid4, cid5}));
        };
        state2();

        REQUIRE(timeline->requestClipSlip(cid4, 30) == 30);

        auto state3 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid1) == l1 - 11);
            REQUIRE(timeline->getClipPosition(cid1) == 5 + 6);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l1 - 12);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);

            REQUIRE(timeline->getClipPlaytime(cid2) == l2 - 11);
            REQUIRE(timeline->getClipPosition(cid2) == 50 + 11);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l2 - 12);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);

            REQUIRE(timeline->getClipPlaytime(cid3) == l3 - 5);
            REQUIRE(timeline->getClipPosition(cid3) == 25);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l3 - 6);
            REQUIRE(timeline->getClipTrackId(cid3) == tid2);

            REQUIRE(timeline->getClipPlaytime(cid4) == l4 - 17);
            REQUIRE(timeline->getClipPosition(cid4) == 8);
            REQUIRE(timeline->getClipPtr(cid4)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid4)->getOut() == l4 - 18);
            REQUIRE(timeline->getClipTrackId(cid4) == tid2);

            REQUIRE(timeline->getClipPlaytime(cid5) == l5);
            REQUIRE(timeline->getClipPosition(cid5) == 60);
            REQUIRE(timeline->getClipPtr(cid5)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid5)->getOut() == l5 - 1);
            REQUIRE(timeline->getClipTrackId(cid5) == tid2);

            REQUIRE(timeline->m_groups->getLeaves(gid1) == std::unordered_set<int>({cid1, cid2, cid3, cid4, cid5}));
        };
        state3();

        undoStack->undo();
        state2();
        undoStack->undo();
        state();

        undoStack->redo();
        state2();
        undoStack->redo();
        state3();
    }

    binModel->clean();
    pCore->m_projectManager = nullptr;
}

TEST_CASE("Advanced trimming operations: Ripple", "[TrimmingRipple]")
{
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
    TimelineItemModel tim(&profile_trimming, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET(timMock)

    QString binId = createProducer(profile_trimming, "red", binModel);
    QString binId2 = createProducer(profile_trimming, "blue", binModel);

    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);

    int cid1 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int cid2 = ClipModel::construct(timeline, binId2, -1, PlaylistState::VideoOnly);
    int cid3 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int cid4 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int cid5 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);
    int cid6 = ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly);

    timeline->m_allClips[cid1]->m_endlessResize = false;
    timeline->m_allClips[cid2]->m_endlessResize = false;
    timeline->m_allClips[cid3]->m_endlessResize = false;
    timeline->m_allClips[cid4]->m_endlessResize = false;
    timeline->m_allClips[cid5]->m_endlessResize = false;

    // ripple resize a fullsized clip longer should not to anything
    SECTION("Ripple resize single fullsized clip (longer)")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        int l1 = timeline->getClipPlaytime(cid1);

        REQUIRE(timeline->requestClipMove(cid2, tid1, 50));
        int l2 = timeline->getClipPlaytime(cid2);

        REQUIRE(timeline->requestClipMove(cid3, tid1, 80));
        int l3 = timeline->getClipPlaytime(cid3);

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid1) == l1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l1 - 1);
            REQUIRE(timeline->getClipPlaytime(cid2) == l2);
            REQUIRE(timeline->getClipPosition(cid2) == 50);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l2 - 1);
            REQUIRE(timeline->getClipPlaytime(cid3) == l3);
            REQUIRE(timeline->getClipPosition(cid3) == 80);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l3 - 1);
        };
        state();

        REQUIRE(timeline->requestItemRippleResize(timeline, cid1, l1 + 5, true));
        state();

        REQUIRE(timeline->requestItemRippleResize(timeline, cid1, l1 + 5, false));
        state();

        REQUIRE(timeline->requestItemRippleResize(timeline, cid2, l2 + 5, true));
        REQUIRE(timeline->requestItemRippleResize(timeline, cid1, l2 + 5, false));
        state();

        REQUIRE(timeline->requestItemRippleResize(timeline, cid3, l3 + 5, true));
        REQUIRE(timeline->requestItemRippleResize(timeline, cid3, l3 + 5, false));
        state();

        undoStack->undo();
        state();
        undoStack->undo();
        state();

        undoStack->redo();
        state();
        undoStack->redo();
        state();
    }

    // ripple resize a fullsized clip shorter should resize the clip and move following clips
    SECTION("Ripple resize single fullsized clip (shorter)")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        int l1 = timeline->getClipPlaytime(cid1);

        REQUIRE(timeline->requestClipMove(cid2, tid1, 50));
        int l2 = timeline->getClipPlaytime(cid2);

        REQUIRE(timeline->requestClipMove(cid3, tid1, 80));
        int l3 = timeline->getClipPlaytime(cid3);

        auto stateA1 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid1) == l1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l1 - 1);
        };
        auto stateA2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid2) == l2);
            REQUIRE(timeline->getClipPosition(cid2) == 50);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l2 - 1);

            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid3) == l3);
            REQUIRE(timeline->getClipPosition(cid3) == 80);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l3 - 1);
        };
        stateA1();
        stateA2();

        auto stateB = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid2) == l2 - 5);
            REQUIRE(timeline->getClipPosition(cid2) == 50);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l2 - 6);

            REQUIRE(timeline->getClipPlaytime(cid3) == l3);
            REQUIRE(timeline->getClipPosition(cid3) == 75);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l3 - 1);
        };
        REQUIRE(timeline->requestItemRippleResize(timeline, cid2, l2 - 5, true) == l2 - 5);
        stateA1();
        stateB();

        auto stateC = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid2) == l2 - 8);
            REQUIRE(timeline->getClipPosition(cid2) == 50);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 3);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l2 - 6);

            REQUIRE(timeline->getClipPlaytime(cid3) == l3);
            REQUIRE(timeline->getClipPosition(cid3) == 72);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l3 - 1);
        };

        REQUIRE(timeline->requestItemRippleResize(timeline, cid2, l2 - 8, false));
        stateA1();
        stateC();

        undoStack->undo();
        stateA1();
        stateB();

        undoStack->undo();
        stateA1();
        stateA2();

        undoStack->redo();
        stateA1();
        stateB();

        undoStack->redo();
        stateA1();
        stateC();
    }

    // ripple resize a fullsized clip shorter should resize the clip and move following clips
    SECTION("Ripple resize fullsized group (shorter)")
    {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        int l1 = timeline->getClipPlaytime(cid1);

        REQUIRE(timeline->requestClipMove(cid2, tid1, 50));
        int l2 = timeline->getClipPlaytime(cid2);

        REQUIRE(timeline->requestClipMove(cid3, tid1, 80));
        int l3 = timeline->getClipPlaytime(cid3);

        REQUIRE(timeline->requestClipMove(cid4, tid2, 5));
        int l4 = timeline->getClipPlaytime(cid4);
        REQUIRE(l4 == l1);
        int gid1 = timeline->requestClipsGroup(std::unordered_set<int>({cid1, cid4}), true, GroupType::Normal);

        REQUIRE(timeline->requestClipMove(cid5, tid2, 50));
        int l5 = timeline->getClipPlaytime(cid5);
        REQUIRE(l5 == l2);
        int gid2 = timeline->requestClipsGroup(std::unordered_set<int>({cid2, cid5}), true, GroupType::Normal);

        REQUIRE(timeline->requestClipMove(cid6, tid2, 80));
        int l6 = timeline->getClipPlaytime(cid6);
        REQUIRE(l6 == l3);
        int gid3 = timeline->requestClipsGroup(std::unordered_set<int>({cid3, cid6}), true, GroupType::Normal);

        auto stateA1 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid1) == l1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getClipPtr(cid1)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid1)->getOut() == l1 - 1);

            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid4) == timeline->getClipPlaytime(cid1));
            REQUIRE(timeline->getClipPosition(cid4) == timeline->getClipPosition(cid1));
            REQUIRE(timeline->getClipPtr(cid4)->getIn() == timeline->getClipPtr(cid1)->getIn());
            REQUIRE(timeline->getClipPtr(cid4)->getOut() == timeline->getClipPtr(cid1)->getOut());
        };
        auto stateA2 = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid2) == l2);
            REQUIRE(timeline->getClipPosition(cid2) == 50);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l2 - 1);

            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid3) == l3);
            REQUIRE(timeline->getClipPosition(cid3) == 80);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l3 - 1);

            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid5) == timeline->getClipPlaytime(cid2));
            REQUIRE(timeline->getClipPosition(cid5) == timeline->getClipPosition(cid2));
            REQUIRE(timeline->getClipPtr(cid5)->getIn() == timeline->getClipPtr(cid2)->getIn());
            REQUIRE(timeline->getClipPtr(cid5)->getOut() == timeline->getClipPtr(cid2)->getOut());

            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid6) == timeline->getClipPlaytime(cid3));
            REQUIRE(timeline->getClipPosition(cid6) == timeline->getClipPosition(cid3));
            REQUIRE(timeline->getClipPtr(cid6)->getIn() == timeline->getClipPtr(cid3)->getIn());
            REQUIRE(timeline->getClipPtr(cid6)->getOut() == timeline->getClipPtr(cid3)->getOut());
        };
        stateA1();
        stateA2();

        auto stateB = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid2) == l2 - 5);
            REQUIRE(timeline->getClipPosition(cid2) == 50);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l2 - 6);

            REQUIRE(timeline->getClipPlaytime(cid3) == l3);
            REQUIRE(timeline->getClipPosition(cid3) == 75);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l3 - 1);

            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid5) == timeline->getClipPlaytime(cid2));
            REQUIRE(timeline->getClipPosition(cid5) == timeline->getClipPosition(cid2));
            REQUIRE(timeline->getClipPtr(cid5)->getIn() == timeline->getClipPtr(cid2)->getIn());
            REQUIRE(timeline->getClipPtr(cid5)->getOut() == timeline->getClipPtr(cid2)->getOut());

            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid6) == timeline->getClipPlaytime(cid3));
            REQUIRE(timeline->getClipPosition(cid6) == timeline->getClipPosition(cid3));
            REQUIRE(timeline->getClipPtr(cid6)->getIn() == timeline->getClipPtr(cid3)->getIn());
            REQUIRE(timeline->getClipPtr(cid6)->getOut() == timeline->getClipPtr(cid3)->getOut());
        };
        REQUIRE(timeline->requestItemRippleResize(timeline, cid2, l2 - 5, true) == l2 - 5);
        stateA1();
        stateB();

        auto stateC = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid2) == l2 - 8);
            REQUIRE(timeline->getClipPosition(cid2) == 50);
            REQUIRE(timeline->getClipPtr(cid2)->getIn() == 3);
            REQUIRE(timeline->getClipPtr(cid2)->getOut() == l2 - 6);

            REQUIRE(timeline->getClipPlaytime(cid3) == l3);
            REQUIRE(timeline->getClipPosition(cid3) == 72);
            REQUIRE(timeline->getClipPtr(cid3)->getIn() == 0);
            REQUIRE(timeline->getClipPtr(cid3)->getOut() == l3 - 1);

            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid5) == timeline->getClipPlaytime(cid2));
            REQUIRE(timeline->getClipPosition(cid5) == timeline->getClipPosition(cid2));
            REQUIRE(timeline->getClipPtr(cid5)->getIn() == timeline->getClipPtr(cid2)->getIn());
            REQUIRE(timeline->getClipPtr(cid5)->getOut() == timeline->getClipPtr(cid2)->getOut());

            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getClipPlaytime(cid6) == timeline->getClipPlaytime(cid3));
            REQUIRE(timeline->getClipPosition(cid6) == timeline->getClipPosition(cid3));
            REQUIRE(timeline->getClipPtr(cid6)->getIn() == timeline->getClipPtr(cid3)->getIn());
            REQUIRE(timeline->getClipPtr(cid6)->getOut() == timeline->getClipPtr(cid3)->getOut());
        };

        REQUIRE(timeline->requestItemRippleResize(timeline, cid2, l2 - 8, false));
        stateA1();
        stateC();

        undoStack->undo();
        stateA1();
        stateB();

        undoStack->undo();
        stateA1();
        stateA2();

        undoStack->redo();
        stateA1();
        stateB();

        undoStack->redo();
        stateA1();
        stateC();
    }

    binModel->clean();
    pCore->m_projectManager = nullptr;
}

