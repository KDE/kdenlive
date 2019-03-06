#include "bin/model/markerlistmodel.hpp"
#include "catch.hpp"
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic push
#include "fakeit.hpp"
#include <iostream>
#include <unordered_set>
#define private public
#define protected public
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "project/projectmanager.h"
#include "test_utils.hpp"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/model/trackmodel.hpp"
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>

Mlt::Profile profile_group;

TEST_CASE("Functional test of the group hierarchy", "[GroupsModel]")
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
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(&profile_group, guideModel, undoStack);

    GroupsModel groups(timeline);
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };

    for (int i = 0; i < 10; i++) {
        groups.createGroupItem(i);
    }

    SECTION("Test Basic Creation")
    {
        for (int i = 0; i < 10; i++) {
            REQUIRE(groups.getRootId(i) == i);
            REQUIRE(groups.isLeaf(i));
            REQUIRE(groups.getLeaves(i).size() == 1);
            REQUIRE(groups.getSubtree(i).size() == 1);
        }
    }

    groups.setGroup(0, 1);
    groups.setGroup(1, 2);
    groups.setGroup(3, 2);
    groups.setGroup(9, 3);
    groups.setGroup(6, 3);
    groups.setGroup(4, 3);
    groups.setGroup(7, 3);
    groups.setGroup(8, 5);

    SECTION("Test leaf nodes")
    {
        std::unordered_set<int> nodes = {1, 2, 3, 5};
        for (int i = 0; i < 10; i++) {
            REQUIRE(groups.isLeaf(i) != (nodes.count(i) > 0));
            if (nodes.count(i) == 0) {
                REQUIRE(groups.getSubtree(i) == std::unordered_set<int>({i}));
                REQUIRE(groups.getLeaves(i) == std::unordered_set<int>({i}));
            }
        }
    }

    SECTION("Test leaves retrieving")
    {
        REQUIRE(groups.getLeaves(2) == std::unordered_set<int>({0, 4, 6, 7, 9}));
        REQUIRE(groups.getLeaves(3) == std::unordered_set<int>({4, 6, 7, 9}));
        REQUIRE(groups.getLeaves(1) == std::unordered_set<int>({0}));
        REQUIRE(groups.getLeaves(5) == std::unordered_set<int>({8}));
    }

    SECTION("Test subtree retrieving")
    {
        REQUIRE(groups.getSubtree(2) == std::unordered_set<int>({0, 1, 2, 3, 4, 6, 7, 9}));
        REQUIRE(groups.getSubtree(3) == std::unordered_set<int>({3, 4, 6, 7, 9}));
        REQUIRE(groups.getSubtree(5) == std::unordered_set<int>({5, 8}));
    }

    SECTION("Test root retieving")
    {
        std::set<int> first_tree = {0, 1, 2, 3, 4, 6, 7, 9};
        for (int n : first_tree) {
            CAPTURE(n);
            REQUIRE(groups.getRootId(n) == 2);
        }
        std::unordered_set<int> second_tree = {5, 8};
        for (int n : second_tree) {
            REQUIRE(groups.getRootId(n) == 5);
        }
    }

    groups.setGroup(3, 8);
    SECTION("Test leaf nodes 2")
    {
        std::unordered_set<int> nodes = {1, 2, 3, 5, 8};
        for (int i = 0; i < 10; i++) {
            REQUIRE(groups.isLeaf(i) != (nodes.count(i) > 0));
            if (nodes.count(i) == 0) {
                REQUIRE(groups.getSubtree(i) == std::unordered_set<int>({i}));
                REQUIRE(groups.getLeaves(i) == std::unordered_set<int>({i}));
            }
        }
    }

    SECTION("Test leaves retrieving 2")
    {
        REQUIRE(groups.getLeaves(1) == std::unordered_set<int>({0}));
        REQUIRE(groups.getLeaves(2) == std::unordered_set<int>({0}));
        REQUIRE(groups.getLeaves(3) == std::unordered_set<int>({4, 6, 7, 9}));
        REQUIRE(groups.getLeaves(5) == std::unordered_set<int>({4, 6, 7, 9}));
        REQUIRE(groups.getLeaves(8) == std::unordered_set<int>({4, 6, 7, 9}));
    }

    SECTION("Test subtree retrieving 2")
    {
        REQUIRE(groups.getSubtree(2) == std::unordered_set<int>({0, 1, 2}));
        REQUIRE(groups.getSubtree(3) == std::unordered_set<int>({3, 4, 6, 7, 9}));
        REQUIRE(groups.getSubtree(5) == std::unordered_set<int>({5, 8, 3, 4, 6, 7, 9}));
    }

    SECTION("Test root retieving 2")
    {
        std::set<int> first_tree = {0, 1, 2};
        for (int n : first_tree) {
            CAPTURE(n);
            REQUIRE(groups.getRootId(n) == 2);
        }
        std::unordered_set<int> second_tree = {5, 8, 3, 4, 6, 7, 9};
        for (int n : second_tree) {
            REQUIRE(groups.getRootId(n) == 5);
        }
    }

    groups.setGroup(5, 2);
    SECTION("Test leaf nodes 3")
    {
        std::unordered_set<int> nodes = {1, 2, 3, 5, 8};
        for (int i = 0; i < 10; i++) {
            REQUIRE(groups.isLeaf(i) != (nodes.count(i) > 0));
            if (nodes.count(i) == 0) {
                REQUIRE(groups.getSubtree(i) == std::unordered_set<int>({i}));
                REQUIRE(groups.getLeaves(i) == std::unordered_set<int>({i}));
            }
        }
    }

    SECTION("Test leaves retrieving 3")
    {
        REQUIRE(groups.getLeaves(1) == std::unordered_set<int>({0}));
        REQUIRE(groups.getLeaves(2) == std::unordered_set<int>({0, 4, 6, 7, 9}));
        REQUIRE(groups.getLeaves(3) == std::unordered_set<int>({4, 6, 7, 9}));
        REQUIRE(groups.getLeaves(5) == std::unordered_set<int>({4, 6, 7, 9}));
        REQUIRE(groups.getLeaves(8) == std::unordered_set<int>({4, 6, 7, 9}));
    }

    SECTION("Test subtree retrieving 3")
    {
        REQUIRE(groups.getSubtree(2) == std::unordered_set<int>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
        REQUIRE(groups.getSubtree(3) == std::unordered_set<int>({3, 4, 6, 7, 9}));
        REQUIRE(groups.getSubtree(5) == std::unordered_set<int>({5, 8, 3, 4, 6, 7, 9}));
    }

    SECTION("Test root retieving 3")
    {
        for (int i = 0; i < 10; i++) {
            CAPTURE(i);
            REQUIRE(groups.getRootId(i) == 2);
        }
    }

    groups.destructGroupItem(8, false, undo, redo);
    SECTION("Test leaf nodes 4")
    {
        std::unordered_set<int> nodes = {1, 2, 3};
        for (int i = 0; i < 10; i++) {
            if (i == 8) continue;
            REQUIRE(groups.isLeaf(i) != (nodes.count(i) > 0));
            if (nodes.count(i) == 0) {
                REQUIRE(groups.getSubtree(i) == std::unordered_set<int>({i}));
                REQUIRE(groups.getLeaves(i) == std::unordered_set<int>({i}));
            }
        }
    }

    SECTION("Test leaves retrieving 4")
    {
        REQUIRE(groups.getLeaves(1) == std::unordered_set<int>({0}));
        REQUIRE(groups.getLeaves(2) == std::unordered_set<int>({0, 5}));
        REQUIRE(groups.getLeaves(3) == std::unordered_set<int>({4, 6, 7, 9}));
    }

    SECTION("Test subtree retrieving 4")
    {
        REQUIRE(groups.getSubtree(2) == std::unordered_set<int>({0, 1, 2, 5}));
        REQUIRE(groups.getSubtree(3) == std::unordered_set<int>({3, 4, 6, 7, 9}));
        REQUIRE(groups.getSubtree(5) == std::unordered_set<int>({5}));
    }

    SECTION("Test root retieving 4")
    {
        std::set<int> first_tree = {0, 1, 2, 5};
        for (int n : first_tree) {
            CAPTURE(n);
            REQUIRE(groups.getRootId(n) == 2);
        }
        std::unordered_set<int> second_tree = {3, 4, 6, 7, 9};
        for (int n : second_tree) {
            CAPTURE(n);
            REQUIRE(groups.getRootId(n) == 3);
        }
    }
    Logger::print_trace();
}

TEST_CASE("Interface test of the group hierarchy", "[GroupsModel]")
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
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(&profile_group, guideModel, undoStack);
    GroupsModel groups(timeline);

    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };

    for (int i = 0; i < 10; i++) {
        groups.createGroupItem(i);
        // the following call shouldn't do anything, but we test that behaviour too.
        groups.ungroupItem(i, undo, redo);
        REQUIRE(groups.getRootId(i) == i);
        REQUIRE(groups.isLeaf(i));
        REQUIRE(groups.getLeaves(i).size() == 1);
        REQUIRE(groups.getSubtree(i).size() == 1);
        REQUIRE(groups.checkConsistency(false));
    }

    auto g1 = std::unordered_set<int>({4, 6, 7, 9});
    int gid1 = groups.groupItems(g1, undo, redo);

    SECTION("One single group")
    {
        for (int i = 0; i < 10; i++) {
            if (g1.count(i) > 0) {
                REQUIRE(groups.getRootId(i) == gid1);
            } else {
                REQUIRE(groups.getRootId(i) == i);
            }
            REQUIRE(groups.getSubtree(i) == std::unordered_set<int>({i}));
            REQUIRE(groups.getLeaves(i) == std::unordered_set<int>({i}));
        }
        REQUIRE(groups.getLeaves(gid1) == g1);
        auto g1b = g1;
        g1b.insert(gid1);
        REQUIRE(groups.getSubtree(gid1) == g1b);
        REQUIRE(groups.checkConsistency(false));
    }
    SECTION("Twice the same group")
    {
        int old_gid1 = gid1;
        gid1 = groups.groupItems(g1, undo, redo); // recreate the same group (will create a parent with the old group as only element)
        for (int i = 0; i < 10; i++) {
            if (g1.count(i) > 0) {
                REQUIRE(groups.getRootId(i) == gid1);
            } else {
                REQUIRE(groups.getRootId(i) == i);
            }
            REQUIRE(groups.getSubtree(i) == std::unordered_set<int>({i}));
            REQUIRE(groups.getLeaves(i) == std::unordered_set<int>({i}));
        }
        REQUIRE(groups.getLeaves(gid1) == g1);
        REQUIRE(groups.getLeaves(old_gid1) == g1);
        auto g1b = g1;
        g1b.insert(old_gid1);
        REQUIRE(groups.getSubtree(old_gid1) == g1b);
        g1b.insert(gid1);
        REQUIRE(groups.getSubtree(gid1) == g1b);
        REQUIRE(groups.checkConsistency(false));
    }

    auto g2 = std::unordered_set<int>({3, 5, 7});
    int gid2 = groups.groupItems(g2, undo, redo);
    auto all_g2 = g2;
    all_g2.insert(4);
    all_g2.insert(6);
    all_g2.insert(9);

    SECTION("Heterogeneous group")
    {
        for (int i = 0; i < 10; i++) {
            CAPTURE(i);
            if (all_g2.count(i) > 0) {
                REQUIRE(groups.getRootId(i) == gid2);
            } else {
                REQUIRE(groups.getRootId(i) == i);
            }
            REQUIRE(groups.getSubtree(i) == std::unordered_set<int>({i}));
            REQUIRE(groups.getLeaves(i) == std::unordered_set<int>({i}));
        }
        REQUIRE(groups.getLeaves(gid1) == g1);
        REQUIRE(groups.getLeaves(gid2) == all_g2);
        REQUIRE(groups.checkConsistency(false));
    }

    auto g3 = std::unordered_set<int>({0, 1});
    int gid3 = groups.groupItems(g3, undo, redo);

    auto g4 = std::unordered_set<int>({0, 4});
    int gid4 = groups.groupItems(g4, undo, redo);
    auto all_g4 = all_g2;
    for (int i : g3)
        all_g4.insert(i);

    SECTION("Group of group")
    {
        for (int i = 0; i < 10; i++) {
            CAPTURE(i);
            if (all_g4.count(i) > 0) {
                REQUIRE(groups.getRootId(i) == gid4);
            } else {
                REQUIRE(groups.getRootId(i) == i);
            }
            REQUIRE(groups.getSubtree(i) == std::unordered_set<int>({i}));
            REQUIRE(groups.getLeaves(i) == std::unordered_set<int>({i}));
        }
        REQUIRE(groups.getLeaves(gid1) == g1);
        REQUIRE(groups.getLeaves(gid2) == all_g2);
        REQUIRE(groups.getLeaves(gid3) == g3);
        REQUIRE(groups.getLeaves(gid4) == all_g4);
        REQUIRE(groups.checkConsistency(false));
    }

    // the following should delete g4
    groups.ungroupItem(3, undo, redo);

    SECTION("Ungroup")
    {
        for (int i = 0; i < 10; i++) {
            CAPTURE(i);
            if (all_g2.count(i) > 0) {
                REQUIRE(groups.getRootId(i) == gid2);
            } else if (g3.count(i) > 0) {
                REQUIRE(groups.getRootId(i) == gid3);
            } else {
                REQUIRE(groups.getRootId(i) == i);
            }
            REQUIRE(groups.getSubtree(i) == std::unordered_set<int>({i}));
            REQUIRE(groups.getLeaves(i) == std::unordered_set<int>({i}));
        }
        REQUIRE(groups.getLeaves(gid1) == g1);
        REQUIRE(groups.checkConsistency(false));
        REQUIRE(groups.getLeaves(gid2) == all_g2);
        REQUIRE(groups.getLeaves(gid3) == g3);
    }
}

TEST_CASE("Orphan groups deletion", "[GroupsModel]")
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
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(&profile_group, guideModel, undoStack);
    GroupsModel groups(timeline);
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };

    for (int i = 0; i < 4; i++) {
        groups.createGroupItem(i);
    }

    auto g1 = std::unordered_set<int>({0, 1});
    int gid1 = groups.groupItems(g1, undo, redo);

    auto g2 = std::unordered_set<int>({2, 3});
    int gid2 = groups.groupItems(g2, undo, redo);
    Q_UNUSED(gid2);

    auto g3 = std::unordered_set<int>({0, 3});
    int gid3 = groups.groupItems(g3, undo, redo);

    REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({0, 1, 2, 3}));
    REQUIRE(groups.checkConsistency(false));

    groups.destructGroupItem(0, true, undo, redo);

    REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({1, 2, 3}));
    REQUIRE(groups.checkConsistency(false));

    SECTION("Normal deletion")
    {
        groups.destructGroupItem(1, false, undo, redo);

        REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({gid1, 2, 3}));
        REQUIRE(groups.checkConsistency(false));

        groups.destructGroupItem(gid1, true, undo, redo);

        REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({2, 3}));
        REQUIRE(groups.checkConsistency(false));
    }

    SECTION("Cascade deletion")
    {
        groups.destructGroupItem(1, true, undo, redo);

        REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({2, 3}));
        REQUIRE(groups.checkConsistency(false));

        groups.destructGroupItem(2, true, undo, redo);

        REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({3}));
        REQUIRE(groups.checkConsistency(false));

        REQUIRE(groups.m_downLink.count(gid3) > 0);
        groups.destructGroupItem(3, true, undo, redo);
        REQUIRE(groups.m_downLink.count(gid3) == 0);
        REQUIRE(groups.m_downLink.size() == 0);
        REQUIRE(groups.checkConsistency(false));
    }
}

TEST_CASE("Integration with timeline", "[GroupsModel]")
{
    qDebug() << "STARTING PASS";
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

    TimelineItemModel tim(&profile_group, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    TimelineItemModel tim2(&profile_group, undoStack);
    Mock<TimelineItemModel> timMock2(tim2);
    auto timeline2 = std::shared_ptr<TimelineItemModel>(&timMock2.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline2, guideModel);

    RESET(timMock);
    RESET(timMock2);

    QString binId = createProducer(profile_group, "red", binModel);
    QString binId2 = createProducerWithSound(profile_group, binModel);

    int length = binModel->getClipByBinID(binId)->frameDuration();
    GroupsModel groups(timeline);

    std::vector<int> clips;
    for (int i = 0; i < 4; i++) {
        if (i % 2 == 0) {
            clips.push_back(ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly));
        } else {
            clips.push_back(ClipModel::construct(timeline, binId, -1, PlaylistState::AudioOnly));
            timeline->m_allClips[clips.back()]->m_canBeAudio = true;
        }
    }
    std::vector<int> clips2;
    for (int i = 0; i < 4; i++) {
        if (i % 2 == 0) {
            clips2.push_back(ClipModel::construct(timeline2, binId, -1, PlaylistState::VideoOnly));
        } else {
            clips2.push_back(ClipModel::construct(timeline2, binId, -1, PlaylistState::AudioOnly));
            timeline2->m_allClips[clips2.back()]->m_canBeAudio = true;
        }
    }
    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);
    Q_UNUSED(tid2);
    int tid3 = TrackModel::construct(timeline, -1, -1, QStringLiteral("audio"), true);
    int tid1_2 = TrackModel::construct(timeline2);
    int tid2_2 = TrackModel::construct(timeline2);
    Q_UNUSED(tid2_2);
    int tid3_2 = TrackModel::construct(timeline2, -1, -1, QStringLiteral("audio2"), true);

    int init_index = undoStack->index();
    SECTION("Basic Creation and export/import from json")
    {
        auto check_roots = [&](int r1, int r2, int r3, int r4) {
            REQUIRE(timeline->m_groups->getRootId(clips[0]) == r1);
            REQUIRE(timeline->m_groups->getRootId(clips[1]) == r2);
            REQUIRE(timeline->m_groups->getRootId(clips[2]) == r3);
            REQUIRE(timeline->m_groups->getRootId(clips[3]) == r4);
        };

        // the following function is a recursive function to check the correctness of a json import
        // Basically, it takes as input a groupId in the imported (target) group hierarchy, and outputs the corresponding groupId from the original one. If no
        // match is found, it returns -1
        std::function<int(int)> rec_check;
        rec_check = [&](int gid) {
            // we first check if the gid is a leaf
            if (timeline2->m_groups->isLeaf(gid)) {
                // then it must be a clip/composition
                int found = -1;
                for (int i = 0; i < 4; i++) {
                    if (clips2[i] == gid) {
                        found = i;
                        break;
                    }
                }
                if (found != -1) {
                    return clips[found];
                } else {
                    qDebug() << "ERROR: did not find correspondence for group" << gid;
                }
            } else {
                // we find correspondances of all the children
                auto children = timeline2->m_groups->getDirectChildren(gid);
                std::unordered_set<int> corresp;
                for (int c : children) {
                    corresp.insert(rec_check(c));
                }
                if (corresp.count(-1) > 0) {
                    return -1; // something went wrong
                }
                std::unordered_set<int> parents;
                for (int c : corresp) {
                    // we find the parents of the corresponding groups in the original hierarchy
                    parents.insert(timeline->m_groups->m_upLink[c]);
                }
                // if the matching is correct, we should have found only one parent
                if (parents.size() != 1) {
                    return -1; // something went wrong
                }
                return *parents.begin();
            }
            return -1;
        };
        auto checkJsonParsing = [&]() {
            // we first destroy all groups in target timeline
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            for (int i = 0; i < 4; i++) {
                while (timeline2->m_groups->getRootId(clips2[i]) != clips2[i]) {
                    timeline2->m_groups->ungroupItem(clips2[i], undo, redo);
                }
            }
            // we do the export then import
            REQUIRE(timeline2->m_groups->fromJson(timeline->m_groups->toJson()));
            std::unordered_map<int, int> roots;
            for (int i = 0; i < 4; i++) {
                int r = timeline2->m_groups->getRootId(clips2[0]);
                if (roots.count(r) == 0) {
                    roots[r] = rec_check(r);
                    REQUIRE(roots[r] != -1);
                }
            }
            for (int i = 0; i < 4; i++) {
                int r = timeline->m_groups->getRootId(clips[0]);
                int r2 = timeline2->m_groups->getRootId(clips2[0]);
                REQUIRE(roots[r2] == r);
            }
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline2->checkConsistency());
        };
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline2->checkConsistency());
        auto g1 = std::unordered_set<int>({clips[0], clips[1]});
        int gid1, gid2, gid3;
        // this fails because clips are not inserted
        REQUIRE(timeline->requestClipsGroup(g1) == -1);
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline2->checkConsistency());

        for (int i = 0; i < 4; i++) {
            REQUIRE(timeline->requestClipMove(clips[i], (i % 2 == 0) ? tid1 : tid3, i * length));
        }
        for (int i = 0; i < 4; i++) {
            REQUIRE(timeline2->requestClipMove(clips2[i], (i % 2 == 0) ? tid1_2 : tid3_2, i * length));
        }
        REQUIRE(timeline->checkConsistency());
        REQUIRE(timeline2->checkConsistency());
        init_index = undoStack->index();
        REQUIRE(timeline->requestClipsGroup(g1, true, GroupType::Normal) > 0);
        auto state1 = [&]() {
            gid1 = timeline->m_groups->getRootId(clips[0]);
            check_roots(gid1, gid1, clips[2], clips[3]);
            REQUIRE(timeline->m_groups->getType(gid1) == GroupType::Normal);
            REQUIRE(timeline->m_groups->getSubtree(gid1) == std::unordered_set<int>({gid1, clips[0], clips[1]}));
            REQUIRE(timeline->m_groups->getLeaves(gid1) == std::unordered_set<int>({clips[0], clips[1]}));
            REQUIRE(undoStack->index() == init_index + 1);
            REQUIRE(timeline->checkConsistency());
        };
        INFO("Test 1");
        state1();
        checkJsonParsing();
        state1();

        auto g2 = std::unordered_set<int>({clips[2], clips[3]});
        REQUIRE(timeline->requestClipsGroup(g2, true, GroupType::AVSplit) > 0);
        auto state2 = [&]() {
            gid2 = timeline->m_groups->getRootId(clips[2]);
            check_roots(gid1, gid1, gid2, gid2);
            REQUIRE(timeline->m_groups->getType(gid1) == GroupType::Normal);
            REQUIRE(timeline->m_groups->getType(gid2) == GroupType::AVSplit);
            REQUIRE(timeline->m_groups->getSubtree(gid2) == std::unordered_set<int>({gid2, clips[2], clips[3]}));
            REQUIRE(timeline->m_groups->getLeaves(gid2) == std::unordered_set<int>({clips[2], clips[3]}));
            REQUIRE(timeline->m_groups->getSubtree(gid1) == std::unordered_set<int>({gid1, clips[0], clips[1]}));
            REQUIRE(timeline->m_groups->getLeaves(gid1) == std::unordered_set<int>({clips[0], clips[1]}));
            REQUIRE(undoStack->index() == init_index + 2);
            REQUIRE(timeline->checkConsistency());
        };
        INFO("Test 2");
        checkJsonParsing();
        state2();

        auto g3 = std::unordered_set<int>({clips[0], clips[3]});
        REQUIRE(timeline->requestClipsGroup(g3, true, GroupType::Normal) > 0);
        auto state3 = [&]() {
            REQUIRE(undoStack->index() == init_index + 3);
            gid3 = timeline->m_groups->getRootId(clips[0]);
            check_roots(gid3, gid3, gid3, gid3);
            REQUIRE(timeline->m_groups->getType(gid1) == GroupType::Normal);
            REQUIRE(timeline->m_groups->getType(gid2) == GroupType::AVSplit);
            REQUIRE(timeline->m_groups->getType(gid3) == GroupType::Normal);
            REQUIRE(timeline->m_groups->getSubtree(gid3) == std::unordered_set<int>({gid1, clips[0], clips[1], gid3, gid2, clips[2], clips[3]}));
            REQUIRE(timeline->m_groups->getLeaves(gid3) == std::unordered_set<int>({clips[2], clips[3], clips[0], clips[1]}));
            REQUIRE(timeline->m_groups->getSubtree(gid2) == std::unordered_set<int>({gid2, clips[2], clips[3]}));
            REQUIRE(timeline->m_groups->getLeaves(gid2) == std::unordered_set<int>({clips[2], clips[3]}));
            REQUIRE(timeline->m_groups->getSubtree(gid1) == std::unordered_set<int>({gid1, clips[0], clips[1]}));
            REQUIRE(timeline->m_groups->getLeaves(gid1) == std::unordered_set<int>({clips[0], clips[1]}));
            REQUIRE(timeline->checkConsistency());
        };

        INFO("Test 3");
        checkJsonParsing();
        state3();

        undoStack->undo();
        INFO("Test 4");
        checkJsonParsing();
        state2();
        undoStack->redo();
        INFO("Test 5");
        checkJsonParsing();
        state3();
        undoStack->undo();
        INFO("Test 6");
        checkJsonParsing();
        state2();
        undoStack->undo();
        INFO("Test 8");
        checkJsonParsing();
        state1();
        undoStack->undo();
        INFO("Test 9");
        checkJsonParsing();
        check_roots(clips[0], clips[1], clips[2], clips[3]);
        undoStack->redo();
        INFO("Test 10");
        checkJsonParsing();
        state1();
        undoStack->redo();
        INFO("Test 11");
        checkJsonParsing();
        state2();

        REQUIRE(timeline->requestClipsGroup(g3) > 0);
        checkJsonParsing();
        state3();

        undoStack->undo();
        checkJsonParsing();
        state2();

        undoStack->undo();
        checkJsonParsing();
        state1();
        undoStack->undo();
        checkJsonParsing();
        check_roots(clips[0], clips[1], clips[2], clips[3]);
    }

    SECTION("Group deletion undo")
    {
        CAPTURE(clips[0]);
        CAPTURE(clips[1]);
        CAPTURE(clips[2]);
        CAPTURE(clips[3]);
        REQUIRE(timeline->requestClipMove(clips[0], tid1, 10));
        REQUIRE(timeline->requestClipMove(clips[1], tid3, 10 + length));
        REQUIRE(timeline->requestClipMove(clips[2], tid1, 15 + 2 * length));
        REQUIRE(timeline->requestClipMove(clips[3], tid3, 50 + 3 * length));

        auto state0 = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getTrackClipsCount(tid3) == 2);
            REQUIRE(timeline->getClipsCount() == 4);
            REQUIRE(timeline->getClipTrackId(clips[0]) == tid1);
            REQUIRE(timeline->getClipTrackId(clips[2]) == tid1);
            REQUIRE(timeline->getClipTrackId(clips[1]) == tid3);
            REQUIRE(timeline->getClipTrackId(clips[3]) == tid3);
            REQUIRE(timeline->getClipPosition(clips[0]) == 10);
            REQUIRE(timeline->getClipPosition(clips[1]) == 10 + length);
            REQUIRE(timeline->getClipPosition(clips[2]) == 15 + 2 * length);
            REQUIRE(timeline->getClipPosition(clips[3]) == 50 + 3 * length);
            REQUIRE(timeline->checkConsistency());
        };

        auto state = [&](int gid1, int gid2, int gid3) {
            state0();
            REQUIRE(timeline->m_groups->getType(gid1) == GroupType::AVSplit);
            REQUIRE(timeline->m_groups->getType(gid2) == GroupType::AVSplit);
            REQUIRE(timeline->m_groups->getType(gid3) == GroupType::Normal);
            REQUIRE(timeline->checkConsistency());
        };
        state0();
        auto g1 = std::unordered_set<int>({clips[0], clips[1]});
        int gid1, gid2, gid3;
        gid1 = timeline->requestClipsGroup(g1, true, GroupType::AVSplit);
        REQUIRE(gid1 > 0);
        auto g2 = std::unordered_set<int>({clips[2], clips[3]});
        gid2 = timeline->requestClipsGroup(g2, true, GroupType::AVSplit);
        REQUIRE(gid2 > 0);
        auto g3 = std::unordered_set<int>({clips[0], clips[3]});
        gid3 = timeline->requestClipsGroup(g3, true, GroupType::Normal);
        REQUIRE(gid3 > 0);
        state(gid1, gid2, gid3);

        std::vector<int> all_clips{clips[0], clips[2], clips[1], clips[3]};
        for (int i = 0; i < 4; i++) {
            REQUIRE(timeline->requestItemDeletion(all_clips[i]));
            REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
            REQUIRE(timeline->getTrackClipsCount(tid3) == 0);
            REQUIRE(timeline->getClipsCount() == 0);
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackById(tid3)->checkConsistency());
            REQUIRE(timeline->checkConsistency());

            undoStack->undo();
            state(gid1, gid2, gid3);
            undoStack->redo();
            REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
            REQUIRE(timeline->getTrackClipsCount(tid3) == 0);
            REQUIRE(timeline->getClipsCount() == 0);
            REQUIRE(timeline->checkConsistency());
            undoStack->undo();
            state(gid1, gid2, gid3);
        }
        // we undo the three grouping operations
        undoStack->undo();
        state0();
        undoStack->undo();
        state0();
        undoStack->undo();
        state0();
    }

    SECTION("Group creation and query from timeline")
    {
        REQUIRE(timeline->requestClipMove(clips[0], tid1, 10));
        REQUIRE(timeline->requestClipMove(clips[1], tid1, 10 + length));
        REQUIRE(timeline->requestClipMove(clips[2], tid1, 15 + 2 * length));
        REQUIRE(timeline->requestClipMove(clips[3], tid1, 50 + 3 * length));
        auto state1 = [&]() {
            REQUIRE(timeline->getGroupElements(clips[2]) == std::unordered_set<int>({clips[2]}));
            REQUIRE(timeline->getGroupElements(clips[1]) == std::unordered_set<int>({clips[1]}));
            REQUIRE(timeline->getGroupElements(clips[3]) == std::unordered_set<int>({clips[3]}));
            REQUIRE(timeline->getGroupElements(clips[0]) == std::unordered_set<int>({clips[0]}));
            REQUIRE(timeline->checkConsistency());
        };
        state1();

        auto g1 = std::unordered_set<int>({clips[0], clips[3]});
        REQUIRE(timeline->requestClipsGroup(g1) > 0);
        auto state2 = [&]() {
            REQUIRE(timeline->getGroupElements(clips[0]) == g1);
            REQUIRE(timeline->getGroupElements(clips[3]) == g1);
            REQUIRE(timeline->getGroupElements(clips[2]) == std::unordered_set<int>({clips[2]}));
            REQUIRE(timeline->getGroupElements(clips[1]) == std::unordered_set<int>({clips[1]}));
            REQUIRE(timeline->checkConsistency());
        };
        state2();

        undoStack->undo();
        state1();

        undoStack->redo();
        state2();

        undoStack->undo();
        state1();

        undoStack->redo();
        state2();

        auto g2 = std::unordered_set<int>({clips[2], clips[1]});
        REQUIRE(timeline->requestClipsGroup(g2) > 0);
        auto state3 = [&]() {
            REQUIRE(timeline->getGroupElements(clips[0]) == g1);
            REQUIRE(timeline->getGroupElements(clips[3]) == g1);
            REQUIRE(timeline->getGroupElements(clips[2]) == g2);
            REQUIRE(timeline->getGroupElements(clips[1]) == g2);
            REQUIRE(timeline->checkConsistency());
        };
        state3();

        undoStack->undo();
        state2();

        undoStack->redo();
        state3();

        auto g3 = std::unordered_set<int>({clips[0], clips[1]});
        REQUIRE(timeline->requestClipsGroup(g3) > 0);
        auto all_g = std::unordered_set<int>({clips[0], clips[1], clips[2], clips[3]});
        auto state4 = [&]() {
            REQUIRE(timeline->getGroupElements(clips[0]) == all_g);
            REQUIRE(timeline->getGroupElements(clips[3]) == all_g);
            REQUIRE(timeline->getGroupElements(clips[2]) == all_g);
            REQUIRE(timeline->getGroupElements(clips[1]) == all_g);
            REQUIRE(timeline->checkConsistency());
        };
        state4();

        undoStack->undo();
        state3();

        undoStack->redo();
        state4();

        REQUIRE(timeline->requestClipUngroup(clips[0]));
        state3();

        undoStack->undo();
        state4();

        REQUIRE(timeline->requestClipUngroup(clips[1]));
        state3();

        undoStack->undo();
        state4();

        undoStack->redo();
        state3();

        REQUIRE(timeline->requestClipUngroup(clips[0]));
        REQUIRE(timeline->getGroupElements(clips[2]) == g2);
        REQUIRE(timeline->getGroupElements(clips[1]) == g2);
        REQUIRE(timeline->getGroupElements(clips[3]) == std::unordered_set<int>({clips[3]}));
        REQUIRE(timeline->getGroupElements(clips[0]) == std::unordered_set<int>({clips[0]}));

        REQUIRE(timeline->requestClipUngroup(clips[1]));
        state1();
    }
    SECTION("Ungroup multiple groups")
    {
        REQUIRE(timeline->requestClipMove(clips[0], tid1, 10));
        REQUIRE(timeline->requestClipMove(clips[1], tid1, 10 + length));
        REQUIRE(timeline->requestClipMove(clips[2], tid1, 15 + 2 * length));
        REQUIRE(timeline->requestClipMove(clips[3], tid1, 50 + 3 * length));
        auto state1 = [&]() {
            REQUIRE(timeline->getGroupElements(clips[2]) == std::unordered_set<int>({clips[2]}));
            REQUIRE(timeline->getGroupElements(clips[1]) == std::unordered_set<int>({clips[1]}));
            REQUIRE(timeline->getGroupElements(clips[3]) == std::unordered_set<int>({clips[3]}));
            REQUIRE(timeline->getGroupElements(clips[0]) == std::unordered_set<int>({clips[0]}));
            REQUIRE(timeline->checkConsistency());
        };
        state1();

        auto g1 = std::unordered_set<int>({clips[0], clips[3]});
        REQUIRE(timeline->requestClipsGroup(g1) > 0);
        auto g2 = std::unordered_set<int>({clips[1], clips[2]});
        REQUIRE(timeline->requestClipsGroup(g2) > 0);
        auto state2 = [&]() {
            REQUIRE(timeline->getGroupElements(clips[0]) == g1);
            REQUIRE(timeline->getGroupElements(clips[3]) == g1);
            REQUIRE(timeline->getGroupElements(clips[1]) == g2);
            REQUIRE(timeline->getGroupElements(clips[2]) == g2);
            REQUIRE(timeline->checkConsistency());
        };
        state2();

        // ungroup clips from same group
        REQUIRE(timeline->requestClipsUngroup({clips[0], clips[3]}));
        auto state3 = [&]() {
            REQUIRE(timeline->getGroupElements(clips[3]) == std::unordered_set<int>({clips[3]}));
            REQUIRE(timeline->getGroupElements(clips[0]) == std::unordered_set<int>({clips[0]}));
            REQUIRE(timeline->getGroupElements(clips[1]) == g2);
            REQUIRE(timeline->getGroupElements(clips[2]) == g2);
            REQUIRE(timeline->checkConsistency());
        };
        state3();

        undoStack->undo();
        state2();

        // ungroup clips from different groups
        REQUIRE(timeline->requestClipsUngroup({clips[0], clips[1]}));
        state1();

        undoStack->undo();
        state2();

        // ungroup all clips
        REQUIRE(timeline->requestClipsUngroup({clips[0], clips[1], clips[2], clips[3]}));
        state1();

        undoStack->undo();
        state2();
    }
}

TEST_CASE("Complex Functions", "[GroupsModel]")
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

    TimelineItemModel tim(&profile_group, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    RESET(timMock);

    GroupsModel groups(timeline);

    SECTION("MergeSingleGroups")
    {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        REQUIRE(groups.m_upLink.size() == 0);

        for (int i = 0; i < 6; i++) {
            groups.createGroupItem(i);
        }
        groups.setGroup(0, 3);
        groups.setGroup(2, 4);
        groups.setGroup(3, 1);
        groups.setGroup(4, 1);
        groups.setGroup(5, 0);

        auto test_tree = [&]() {
            REQUIRE(groups.getSubtree(1) == std::unordered_set<int>({0, 1, 2, 3, 4, 5}));
            REQUIRE(groups.getDirectChildren(0) == std::unordered_set<int>({5}));
            REQUIRE(groups.getDirectChildren(1) == std::unordered_set<int>({3, 4}));
            REQUIRE(groups.getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(3) == std::unordered_set<int>({0}));
            REQUIRE(groups.getDirectChildren(4) == std::unordered_set<int>({2}));
            REQUIRE(groups.getDirectChildren(5) == std::unordered_set<int>({}));
            REQUIRE(groups.checkConsistency(false));
        };
        test_tree();

        REQUIRE(groups.mergeSingleGroups(1, undo, redo));
        auto test_tree2 = [&]() {
            REQUIRE(groups.getSubtree(1) == std::unordered_set<int>({1, 2, 5}));
            REQUIRE(groups.getDirectChildren(1) == std::unordered_set<int>({2, 5}));
            REQUIRE(groups.getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(5) == std::unordered_set<int>({}));
            REQUIRE(groups.checkConsistency());
        };
        test_tree2();

        undo();
        test_tree();

        redo();
        test_tree2();
    }

    SECTION("MergeSingleGroups2")
    {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        REQUIRE(groups.m_upLink.size() == 0);

        for (int i = 0; i < 3; i++) {
            groups.createGroupItem(i);
        }
        groups.setGroup(1, 0);
        groups.setGroup(2, 1);

        auto test_tree = [&]() {
            REQUIRE(groups.getSubtree(0) == std::unordered_set<int>({0, 1, 2}));
            REQUIRE(groups.getDirectChildren(0) == std::unordered_set<int>({1}));
            REQUIRE(groups.getDirectChildren(1) == std::unordered_set<int>({2}));
            REQUIRE(groups.getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups.checkConsistency(false));
        };
        test_tree();

        REQUIRE(groups.mergeSingleGroups(0, undo, redo));
        auto test_tree2 = [&]() {
            REQUIRE(groups.getSubtree(2) == std::unordered_set<int>({2}));
            REQUIRE(groups.getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups.getRootId(2) == 2);
            REQUIRE(groups.checkConsistency());
        };
        test_tree2();

        undo();
        test_tree();

        redo();
        test_tree2();
    }

    SECTION("MergeSingleGroups3")
    {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        REQUIRE(groups.m_upLink.size() == 0);

        for (int i = 0; i < 6; i++) {
            groups.createGroupItem(i);
        }
        groups.setGroup(0, 2);
        groups.setGroup(1, 0);
        groups.setGroup(3, 1);
        groups.setGroup(4, 1);
        groups.setGroup(5, 4);

        auto test_tree = [&]() {
            for (int i = 0; i < 6; i++) {
                REQUIRE(groups.getRootId(i) == 2);
            }
            REQUIRE(groups.getSubtree(2) == std::unordered_set<int>({0, 1, 2, 3, 4, 5}));
            REQUIRE(groups.getDirectChildren(0) == std::unordered_set<int>({1}));
            REQUIRE(groups.getDirectChildren(1) == std::unordered_set<int>({4, 3}));
            REQUIRE(groups.getDirectChildren(2) == std::unordered_set<int>({0}));
            REQUIRE(groups.getDirectChildren(3) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(4) == std::unordered_set<int>({5}));
            REQUIRE(groups.getDirectChildren(5) == std::unordered_set<int>({}));
            REQUIRE(groups.checkConsistency(false));
        };
        test_tree();

        REQUIRE(groups.mergeSingleGroups(2, undo, redo));
        auto test_tree2 = [&]() {
            REQUIRE(groups.getRootId(1) == 1);
            REQUIRE(groups.getRootId(3) == 1);
            REQUIRE(groups.getRootId(5) == 1);
            REQUIRE(groups.getSubtree(1) == std::unordered_set<int>({1, 3, 5}));
            REQUIRE(groups.getDirectChildren(1) == std::unordered_set<int>({3, 5}));
            REQUIRE(groups.getDirectChildren(3) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(5) == std::unordered_set<int>({}));
            REQUIRE(groups.checkConsistency());
        };
        test_tree2();

        undo();
        test_tree();

        redo();
        test_tree2();
    }
    SECTION("Split leaf")
    {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        REQUIRE(groups.m_upLink.size() == 0);

        // This is a dummy split criterion
        auto criterion = [](int a) { return a % 2 == 0; };
        auto criterion2 = [](int a) { return a % 2 != 0; };

        // We create a leaf
        groups.createGroupItem(1);
        auto test_leaf = [&]() {
            REQUIRE(groups.getRootId(1) == 1);
            REQUIRE(groups.isLeaf(1));
            REQUIRE(groups.m_upLink.size() == 1);
            REQUIRE(groups.checkConsistency());
        };
        test_leaf();

        REQUIRE(groups.split(1, criterion, undo, redo));
        test_leaf();
        undo();
        test_leaf();
        redo();
        REQUIRE(groups.split(1, criterion2, undo, redo));
        test_leaf();
        undo();
        test_leaf();
        redo();
    }
    SECTION("Simple split Tree")
    {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        REQUIRE(groups.m_upLink.size() == 0);

        // This is a dummy split criterion
        auto criterion = [](int a) { return a % 2 == 0; };

        // We create a very simple tree
        for (int i = 0; i < 3; i++) {
            groups.createGroupItem(i);
        }
        groups.setGroup(1, 0);
        groups.setGroup(2, 0);

        auto test_tree = [&]() {
            REQUIRE(groups.getRootId(0) == 0);
            REQUIRE(groups.getRootId(1) == 0);
            REQUIRE(groups.getRootId(2) == 0);
            REQUIRE(groups.getSubtree(0) == std::unordered_set<int>({0, 1, 2}));
            REQUIRE(groups.getDirectChildren(0) == std::unordered_set<int>({1, 2}));
            REQUIRE(groups.getDirectChildren(1) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups.checkConsistency());
        };
        test_tree();

        REQUIRE(groups.split(0, criterion, undo, redo));
        auto test_tree2 = [&]() {
            REQUIRE(groups.getRootId(1) == 1);
            REQUIRE(groups.getRootId(2) == 2);
            REQUIRE(groups.getSubtree(2) == std::unordered_set<int>({2}));
            REQUIRE(groups.getSubtree(1) == std::unordered_set<int>({1}));
            REQUIRE(groups.getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(1) == std::unordered_set<int>({}));
            REQUIRE(groups.checkConsistency());
        };
        test_tree2();

        undo();
        test_tree();

        redo();
        test_tree2();
    }

    SECTION("complex split Tree")
    {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        REQUIRE(groups.m_upLink.size() == 0);

        // This is a dummy split criterion
        auto criterion = [](int a) { return a % 2 != 0; };

        for (int i = 0; i < 9; i++) {
            groups.createGroupItem(i);
        }
        groups.setGroup(0, 3);
        groups.setGroup(1, 0);
        groups.setGroup(3, 2);
        groups.setGroup(4, 3);
        groups.setGroup(5, 8);
        groups.setGroup(6, 0);
        groups.setGroup(7, 8);
        groups.setGroup(8, 2);

        auto test_tree = [&]() {
            for (int i = 0; i < 9; i++) {
                REQUIRE(groups.getRootId(i) == 2);
            }
            REQUIRE(groups.getSubtree(2) == std::unordered_set<int>({0, 1, 2, 3, 4, 5, 6, 7, 8}));
            REQUIRE(groups.getDirectChildren(0) == std::unordered_set<int>({1, 6}));
            REQUIRE(groups.getDirectChildren(1) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(2) == std::unordered_set<int>({3, 8}));
            REQUIRE(groups.getDirectChildren(3) == std::unordered_set<int>({0, 4}));
            REQUIRE(groups.getDirectChildren(4) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(5) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(6) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(7) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(8) == std::unordered_set<int>({5, 7}));
            REQUIRE(groups.checkConsistency());
        };
        test_tree();

        REQUIRE(groups.split(2, criterion, undo, redo));
        auto test_tree2 = [&]() {
            REQUIRE(groups.getRootId(6) == 3);
            REQUIRE(groups.getRootId(3) == 3);
            REQUIRE(groups.getRootId(4) == 3);
            REQUIRE(groups.getSubtree(3) == std::unordered_set<int>({3, 4, 6}));
            REQUIRE(groups.getDirectChildren(6) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(4) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(3) == std::unordered_set<int>({6, 4}));
            // new tree
            int newRoot = groups.getRootId(1);
            REQUIRE(groups.getRootId(1) == newRoot);
            REQUIRE(groups.getRootId(5) == newRoot);
            REQUIRE(groups.getRootId(7) == newRoot);
            int other = -1;
            REQUIRE(groups.getDirectChildren(newRoot).size() == 2);
            for (int c : groups.getDirectChildren(newRoot))
                if (c != 1) other = c;
            REQUIRE(other != -1);
            REQUIRE(groups.getSubtree(newRoot) == std::unordered_set<int>({1, 5, 7, newRoot, other}));
            REQUIRE(groups.getDirectChildren(1) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(5) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(7) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(newRoot) == std::unordered_set<int>({1, other}));
            REQUIRE(groups.getDirectChildren(other) == std::unordered_set<int>({5, 7}));
            REQUIRE(groups.checkConsistency());
        };
        test_tree2();

        undo();
        test_tree();

        redo();
        test_tree2();
    }
    SECTION("Splitting preserves group type")
    {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        REQUIRE(groups.m_upLink.size() == 0);

        // This is a dummy split criterion
        auto criterion = [](int a) { return a % 2 == 0; };

        // We create a very simple tree
        for (int i = 0; i <= 6; i++) {
            groups.createGroupItem(i);
        }
        groups.setGroup(0, 4);
        groups.setGroup(2, 4);
        groups.setGroup(1, 5);
        groups.setGroup(3, 5);

        groups.setGroup(4, 6);
        groups.setGroup(5, 6);

        groups.setType(4, GroupType::AVSplit);
        groups.setType(5, GroupType::AVSplit);
        groups.setType(6, GroupType::Normal);

        auto test_tree = [&]() {
            REQUIRE(groups.m_upLink.size() == 7);
            for (int i = 0; i <= 6; i++) {
                REQUIRE(groups.getRootId(i) == 6);
            }
            REQUIRE(groups.getSubtree(6) == std::unordered_set<int>({0, 1, 2, 3, 4, 5, 6}));
            REQUIRE(groups.getDirectChildren(0) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(1) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(3) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(4) == std::unordered_set<int>({0, 2}));
            REQUIRE(groups.getDirectChildren(5) == std::unordered_set<int>({1, 3}));
            REQUIRE(groups.getDirectChildren(6) == std::unordered_set<int>({4, 5}));
            REQUIRE(groups.getType(4) == GroupType::AVSplit);
            REQUIRE(groups.getType(5) == GroupType::AVSplit);
            REQUIRE(groups.getType(6) == GroupType::Normal);
            REQUIRE(groups.checkConsistency());
        };
        test_tree();
        qDebug() << " done testing";

        REQUIRE(groups.split(6, criterion, undo, redo));
        qDebug() << " done splitting";
        auto test_tree2 = [&]() {
            // REQUIRE(groups.m_upLink.size() == 6);
            int r1 = groups.getRootId(0);
            int r2 = groups.getRootId(1);
            bool ok = r1 == 4 || r2 == 5;
            REQUIRE(ok);
            REQUIRE(groups.getRootId(2) == r1);
            REQUIRE(groups.getRootId(3) == r2);
            REQUIRE(groups.getSubtree(r1) == std::unordered_set<int>({r1, 0, 2}));
            REQUIRE(groups.getSubtree(r2) == std::unordered_set<int>({r2, 1, 3}));
            REQUIRE(groups.getDirectChildren(0) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(1) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(3) == std::unordered_set<int>({}));
            REQUIRE(groups.getDirectChildren(r1) == std::unordered_set<int>({0, 2}));
            REQUIRE(groups.getDirectChildren(r2) == std::unordered_set<int>({1, 3}));
            REQUIRE(groups.getType(r1) == GroupType::AVSplit);
            REQUIRE(groups.getType(r2) == GroupType::AVSplit);
            REQUIRE(groups.checkConsistency());
        };
        test_tree2();

        undo();
        test_tree();

        redo();
        test_tree2();
        undo();
        test_tree();
        redo();
        test_tree2();
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
}
