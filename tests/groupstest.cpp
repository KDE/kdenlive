#include "catch.hpp"
#include <unordered_set>
#include <iostream>
#define private public
#define protected public
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/model/trackmodel.hpp"
#include "doc/docundostack.hpp"
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>

TEST_CASE("Functional test of the group hierarchy", "[GroupsModel]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(undoStack);
    GroupsModel groups(timeline);
    std::function<bool (void)> undo = [](){return true;};
    std::function<bool (void)> redo = [](){return true;};

    for (int i = 0; i < 10; i++) {
        groups.createGroupItem(i);
    }

    SECTION("Test Basic Creation") {
        for (int i = 0; i < 10; i++) {
            REQUIRE(groups.getRootId(i) == i);
            REQUIRE(groups.isLeaf(i));
            REQUIRE(groups.getLeaves(i).size() == 1);
            REQUIRE(groups.getSubtree(i).size() == 1);
        }
    }

    groups.setGroup(0,1);
    groups.setGroup(1,2);
    groups.setGroup(3,2);
    groups.setGroup(9,3);
    groups.setGroup(6,3);
    groups.setGroup(4,3);
    groups.setGroup(7,3);
    groups.setGroup(8,5);

    SECTION("Test leaf nodes"){
        std::unordered_set<int> nodes = {1,2,3,5};
        for (int i = 0; i < 10; i++) {
            REQUIRE(groups.isLeaf(i) != (nodes.count(i) > 0));
            if (nodes.count(i) == 0) {
                REQUIRE(groups.getSubtree(i) == std::unordered_set<int>({i}));
                REQUIRE(groups.getLeaves(i) == std::unordered_set<int>({i}));
            }
        }
    }

    SECTION("Test leaves retrieving"){
        REQUIRE(groups.getLeaves(2) == std::unordered_set<int>({0,4,6,7,9}));
        REQUIRE(groups.getLeaves(3) == std::unordered_set<int>({4,6,7,9}));
        REQUIRE(groups.getLeaves(1) == std::unordered_set<int>({0}));
        REQUIRE(groups.getLeaves(5) == std::unordered_set<int>({8}));
    }

    SECTION("Test subtree retrieving"){
        REQUIRE(groups.getSubtree(2) == std::unordered_set<int>({0,1,2,3,4,6,7,9}));
        REQUIRE(groups.getSubtree(3) == std::unordered_set<int>({3,4,6,7,9}));
        REQUIRE(groups.getSubtree(5) == std::unordered_set<int>({5,8}));
    }

    SECTION("Test root retieving"){
        std::set<int> first_tree = {0,1,2,3,4,6,7,9};
        for (int n : first_tree) {
            CAPTURE(n);
            REQUIRE(groups.getRootId(n) == 2);
        }
        std::unordered_set<int> second_tree = {5,8};
        for (int n : second_tree) {
            REQUIRE(groups.getRootId(n) == 5);
        }
    }

    groups.setGroup(3,8);
    SECTION("Test leaf nodes 2"){
        std::unordered_set<int> nodes = {1,2,3,5,8};
        for (int i = 0; i < 10; i++) {
            REQUIRE(groups.isLeaf(i) != (nodes.count(i) > 0));
            if (nodes.count(i) == 0) {
                REQUIRE(groups.getSubtree(i) == std::unordered_set<int>({i}));
                REQUIRE(groups.getLeaves(i) == std::unordered_set<int>({i}));
            }
        }
    }

    SECTION("Test leaves retrieving 2"){
        REQUIRE(groups.getLeaves(1) == std::unordered_set<int>({0}));
        REQUIRE(groups.getLeaves(2) == std::unordered_set<int>({0}));
        REQUIRE(groups.getLeaves(3) == std::unordered_set<int>({4,6,7,9}));
        REQUIRE(groups.getLeaves(5) == std::unordered_set<int>({4,6,7,9}));
        REQUIRE(groups.getLeaves(8) == std::unordered_set<int>({4,6,7,9}));
    }

    SECTION("Test subtree retrieving 2"){
        REQUIRE(groups.getSubtree(2) == std::unordered_set<int>({0,1,2}));
        REQUIRE(groups.getSubtree(3) == std::unordered_set<int>({3,4,6,7,9}));
        REQUIRE(groups.getSubtree(5) == std::unordered_set<int>({5,8,3,4,6,7,9}));
    }

    SECTION("Test root retieving 2"){
        std::set<int> first_tree = {0,1,2};
        for (int n : first_tree) {
            CAPTURE(n);
            REQUIRE(groups.getRootId(n) == 2);
        }
        std::unordered_set<int> second_tree = {5,8,3,4,6,7,9};
        for (int n : second_tree) {
            REQUIRE(groups.getRootId(n) == 5);
        }
    }

    groups.setGroup(5,2);
    SECTION("Test leaf nodes 3"){
        std::unordered_set<int> nodes = {1,2,3,5,8};
        for (int i = 0; i < 10; i++) {
            REQUIRE(groups.isLeaf(i) != (nodes.count(i) > 0));
            if (nodes.count(i) == 0) {
                REQUIRE(groups.getSubtree(i) == std::unordered_set<int>({i}));
                REQUIRE(groups.getLeaves(i) == std::unordered_set<int>({i}));
            }
        }
    }

    SECTION("Test leaves retrieving 3"){
        REQUIRE(groups.getLeaves(1) == std::unordered_set<int>({0}));
        REQUIRE(groups.getLeaves(2) == std::unordered_set<int>({0,4,6,7,9}));
        REQUIRE(groups.getLeaves(3) == std::unordered_set<int>({4,6,7,9}));
        REQUIRE(groups.getLeaves(5) == std::unordered_set<int>({4,6,7,9}));
        REQUIRE(groups.getLeaves(8) == std::unordered_set<int>({4,6,7,9}));
    }

    SECTION("Test subtree retrieving 3"){
        REQUIRE(groups.getSubtree(2) == std::unordered_set<int>({0,1,2,3,4,5,6,7,8,9}));
        REQUIRE(groups.getSubtree(3) == std::unordered_set<int>({3,4,6,7,9}));
        REQUIRE(groups.getSubtree(5) == std::unordered_set<int>({5,8,3,4,6,7,9}));
    }

    SECTION("Test root retieving 3"){
        for (int i = 0; i < 10; i++) {
            CAPTURE(i);
            REQUIRE(groups.getRootId(i) == 2);
        }
    }

    groups.destructGroupItem(8, false, undo, redo);
    SECTION("Test leaf nodes 4"){
        std::unordered_set<int> nodes = {1,2,3};
        for (int i = 0; i < 10; i++) {
            if (i == 8) continue;
            REQUIRE(groups.isLeaf(i) != (nodes.count(i) > 0));
            if (nodes.count(i) == 0) {
                REQUIRE(groups.getSubtree(i) == std::unordered_set<int>({i}));
                REQUIRE(groups.getLeaves(i) == std::unordered_set<int>({i}));
            }
        }
    }

    SECTION("Test leaves retrieving 4"){
        REQUIRE(groups.getLeaves(1) == std::unordered_set<int>({0}));
        REQUIRE(groups.getLeaves(2) == std::unordered_set<int>({0,5}));
        REQUIRE(groups.getLeaves(3) == std::unordered_set<int>({4,6,7,9}));
    }

    SECTION("Test subtree retrieving 4"){
        REQUIRE(groups.getSubtree(2) == std::unordered_set<int>({0,1,2,5}));
        REQUIRE(groups.getSubtree(3) == std::unordered_set<int>({3,4,6,7,9}));
        REQUIRE(groups.getSubtree(5) == std::unordered_set<int>({5}));
    }

    SECTION("Test root retieving 4"){
        std::set<int> first_tree = {0,1,2,5};
        for (int n : first_tree) {
            CAPTURE(n);
            REQUIRE(groups.getRootId(n) == 2);
        }
        std::unordered_set<int> second_tree = {3,4,6,7,9};
        for (int n : second_tree) {
            CAPTURE(n);
            REQUIRE(groups.getRootId(n) == 3);
        }
    }

}

TEST_CASE("Interface test of the group hierarchy", "[GroupsModel]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(undoStack);
    GroupsModel groups(timeline);

    std::function<bool (void)> undo = [](){return true;};
    std::function<bool (void)> redo = [](){return true;};

    for (int i = 0; i < 10; i++) {
        groups.createGroupItem(i);
        //the following call shouldn't do anything, but we test that behaviour too.
        groups.ungroupItem(i, undo, redo);
        REQUIRE(groups.getRootId(i) == i);
        REQUIRE(groups.isLeaf(i));
        REQUIRE(groups.getLeaves(i).size() == 1);
        REQUIRE(groups.getSubtree(i).size() == 1);
    }

    auto g1 = std::unordered_set<int>({4,6,7,9});
    int gid1 = groups.groupItems(g1, undo, redo);

    SECTION("One single group") {
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
    }
    SECTION("Twice the same group") {
        int old_gid1 = gid1;
        gid1 = groups.groupItems(g1, undo, redo); //recreate the same group (will create a parent with the old group as only element)
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
    }

    auto g2 = std::unordered_set<int>({3,5,7});
    int gid2 = groups.groupItems(g2, undo, redo);
    auto all_g2 = g2;
    all_g2.insert(4);
    all_g2.insert(6);
    all_g2.insert(9);

    SECTION("Heterogeneous group") {
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
    }

    auto g3 = std::unordered_set<int>({0,1});
    int gid3 = groups.groupItems(g3, undo, redo);

    auto g4 = std::unordered_set<int>({0,4});
    int gid4 = groups.groupItems(g4, undo, redo);
    auto all_g4 = all_g2;
    for (int i : g3) all_g4.insert(i);

    SECTION("Group of group") {
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
    }

    //the following should delete g4
    groups.ungroupItem(3, undo, redo);

    SECTION("Ungroup") {
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
        REQUIRE(groups.getLeaves(gid2) == all_g2);
        REQUIRE(groups.getLeaves(gid3) == g3);
    }
}

TEST_CASE("Orphan groups deletion", "[GroupsModel]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(undoStack);
    GroupsModel groups(timeline);
    std::function<bool (void)> undo = [](){return true;};
    std::function<bool (void)> redo = [](){return true;};

    for (int i = 0; i < 4; i++) {
        groups.createGroupItem(i);
    }

    auto g1 = std::unordered_set<int>({0,1});
    int gid1 = groups.groupItems(g1, undo, redo);

    auto g2 = std::unordered_set<int>({2,3});
    int gid2 = groups.groupItems(g2, undo, redo);

    auto g3 = std::unordered_set<int>({0,3});
    int gid3 = groups.groupItems(g3, undo, redo);

    REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({0,1,2,3}));

    groups.destructGroupItem(0,true, undo, redo);

    REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({1,2,3}));

    SECTION("Normal deletion") {
        groups.destructGroupItem(1,false, undo, redo);

        REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({gid1,2,3}));

        groups.destructGroupItem(gid1,true, undo, redo);

        REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({2,3}));
    }

    SECTION("Cascade deletion") {
        groups.destructGroupItem(1,true, undo, redo);

        REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({2,3}));

        groups.destructGroupItem(2,true, undo, redo);

        REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({3}));

        REQUIRE(groups.m_downLink.count(gid3) > 0);
        groups.destructGroupItem(3,true, undo, redo);
        REQUIRE(groups.m_downLink.count(gid3) == 0);
        REQUIRE(groups.m_downLink.size() == 0);
    }

}

TEST_CASE("Undo/redo", "[GroupsModel]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(undoStack);
    Mlt::Profile profile;

    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer->set("length", 20);
    producer->set("out", 19);

    int length = producer->get_playtime();
    GroupsModel groups(timeline);

    std::vector<int> clips;
    for (int i = 0; i < 4; i++) {
        clips.push_back(ClipModel::construct(timeline, producer));
    }

    int init_index = undoStack->index();
/*    SECTION("Basic Creation") {
        auto check_roots = [&](int r1, int r2, int r3, int r4) {
            REQUIRE(timeline->m_groups->getRootId(clips[0]) == r1);
            REQUIRE(timeline->m_groups->getRootId(clips[1]) == r2);
            REQUIRE(timeline->m_groups->getRootId(clips[2]) == r3);
            REQUIRE(timeline->m_groups->getRootId(clips[3]) == r4);
        };
        auto g1 = std::unordered_set<int>({clips[0],clips[1]});
        int gid1, gid2, gid3;
        REQUIRE(timeline->requestGroupClips(g1));
        auto state1 = [&](){
            gid1 = timeline->m_groups->getRootId(clips[0]);
            check_roots(gid1, gid1, clips[2], clips[3]);
            REQUIRE(timeline->m_groups->getSubtree(gid1) == std::unordered_set<int>({gid1, clips[0], clips[1]}));
            REQUIRE(timeline->m_groups->getLeaves(gid1) == std::unordered_set<int>({clips[0], clips[1]}));
            REQUIRE(undoStack->index() == init_index + 1);
        };
        INFO("Test 1");
        state1();

        auto g2 = std::unordered_set<int>({clips[2],clips[3]});
        REQUIRE(timeline->requestGroupClips(g2));
        auto state2 = [&](){
            gid2 = timeline->m_groups->getRootId(clips[2]);
            check_roots(gid1, gid1, gid2, gid2);
            REQUIRE(timeline->m_groups->getSubtree(gid2) == std::unordered_set<int>({gid2, clips[2], clips[3]}));
            REQUIRE(timeline->m_groups->getLeaves(gid2) == std::unordered_set<int>({clips[2], clips[3]}));
            REQUIRE(timeline->m_groups->getSubtree(gid1) == std::unordered_set<int>({gid1, clips[0], clips[1]}));
            REQUIRE(timeline->m_groups->getLeaves(gid1) == std::unordered_set<int>({clips[0], clips[1]}));
            REQUIRE(undoStack->index() == init_index + 2);
        };
        INFO("Test 2");
        state2();

        auto g3 = std::unordered_set<int>({clips[0],clips[3]});
        REQUIRE(timeline->requestGroupClips(g3));
        auto state3 = [&](){
            REQUIRE(undoStack->index() == init_index + 3);
            gid3 = timeline->m_groups->getRootId(clips[0]);
            check_roots(gid3, gid3, gid3, gid3);
            REQUIRE(timeline->m_groups->getSubtree(gid3) == std::unordered_set<int>({gid1, clips[0], clips[1], gid3, gid2, clips[2], clips[3]}));
            REQUIRE(timeline->m_groups->getLeaves(gid3) == std::unordered_set<int>({clips[2], clips[3], clips[0], clips[1]}));
            REQUIRE(timeline->m_groups->getSubtree(gid2) == std::unordered_set<int>({gid2, clips[2], clips[3]}));
            REQUIRE(timeline->m_groups->getLeaves(gid2) == std::unordered_set<int>({clips[2], clips[3]}));
            REQUIRE(timeline->m_groups->getSubtree(gid1) == std::unordered_set<int>({gid1, clips[0], clips[1]}));
            REQUIRE(timeline->m_groups->getLeaves(gid1) == std::unordered_set<int>({clips[0], clips[1]}));
        };

        INFO("Test 3");
        state3();

        undoStack->undo();
        INFO("Test 4");
        state2();
        undoStack->redo();
        INFO("Test 5");
        state3();
        undoStack->undo();
        INFO("Test 6");
        state2();
        undoStack->undo();
        INFO("Test 8");
        state1();
        undoStack->undo();
        INFO("Test 9");
        check_roots(clips[0], clips[1], clips[2], clips[3]);
        undoStack->redo();
        INFO("Test 10");
        state1();
        undoStack->redo();
        INFO("Test 11");
        state2();

        REQUIRE(timeline->requestGroupClips(g3));
        state3();

        undoStack->undo();
        state2();

        undoStack->undo();
        state1();
        undoStack->undo();
        check_roots(clips[0], clips[1], clips[2], clips[3]);

    }

*/

    SECTION("Group deletion undo") {
        int tid1 = TrackModel::construct(timeline);
        CAPTURE(clips[0]);
        CAPTURE(clips[1]);
        CAPTURE(clips[2]);
        CAPTURE(clips[3]);
        REQUIRE(timeline->requestClipMove(clips[0], tid1, 10));
        REQUIRE(timeline->requestClipMove(clips[1], tid1, 10 + length));
        REQUIRE(timeline->requestClipMove(clips[2], tid1, 15 + 2*length));
        REQUIRE(timeline->requestClipMove(clips[3], tid1, 50 + 3*length));

        auto state = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 4);
            for (int i = 0; i < 4; i++) {
                REQUIRE(timeline->getClipTrackId(clips[i]) == tid1);
            }
            REQUIRE(timeline->getClipPosition(clips[0]) == 10);
            REQUIRE(timeline->getClipPosition(clips[1]) == 10 + length);
            REQUIRE(timeline->getClipPosition(clips[2]) == 15 + 2*length);
            REQUIRE(timeline->getClipPosition(clips[3]) == 50 + 3*length);
        };
        state();
        auto g1 = std::unordered_set<int>({clips[0],clips[1]});
        int gid1, gid2, gid3;
        REQUIRE(timeline->requestGroupClips(g1));
        auto g2 = std::unordered_set<int>({clips[2],clips[3]});
        REQUIRE(timeline->requestGroupClips(g2));
        auto g3 = std::unordered_set<int>({clips[0],clips[3]});
        REQUIRE(timeline->requestGroupClips(g3));
        state();

        for (int i = 0; i < 4; i++) {
            REQUIRE(timeline->requestClipDeletion(clips[i]));
            REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
            REQUIRE(timeline->getClipsCount() == 0);
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());

            undoStack->undo();
            state();
            undoStack->redo();
            REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
            REQUIRE(timeline->getClipsCount() == 0);
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            undoStack->undo();
            state();
        }
        //we undo the three grouping operations
        undoStack->undo();
        state();
        undoStack->undo();
        state();
        undoStack->undo();
        state();
    }

}
