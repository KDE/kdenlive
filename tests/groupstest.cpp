#include "catch.hpp"
#include <unordered_set>
#include <iostream>
#define private public
#define protected public
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"

TEST_CASE("Functional test of the group hierarchy", "[GroupsModel]")
{
    std::shared_ptr<TimelineModel> timeline = TimelineModel::construct();
    GroupsModel groups(timeline);

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

    groups.destructGroupItem(8);
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
    std::shared_ptr<TimelineModel> timeline = TimelineModel::construct();
    GroupsModel groups(timeline);

    for (int i = 0; i < 10; i++) {
        groups.createGroupItem(i);
        //the following call shouldn't do anything, but we test that behaviour too.
        groups.ungroupItem(i);
        REQUIRE(groups.getRootId(i) == i);
        REQUIRE(groups.isLeaf(i));
        REQUIRE(groups.getLeaves(i).size() == 1);
        REQUIRE(groups.getSubtree(i).size() == 1);
    }

    auto g1 = std::unordered_set<int>({4,6,7,9});
    int gid1 = groups.groupItems(g1);

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
    }

    auto g2 = std::unordered_set<int>({3,5,7});
    int gid2 = groups.groupItems(g2);
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
    int gid3 = groups.groupItems(g3);

    auto g4 = std::unordered_set<int>({0,4});
    int gid4 = groups.groupItems(g4);
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
    groups.ungroupItem(3);

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
    std::shared_ptr<TimelineModel> timeline = TimelineModel::construct();
    GroupsModel groups(timeline);

    for (int i = 0; i < 4; i++) {
        groups.createGroupItem(i);
    }

    auto g1 = std::unordered_set<int>({0,1});
    int gid1 = groups.groupItems(g1);

    auto g2 = std::unordered_set<int>({2,3});
    int gid2 = groups.groupItems(g2);

    auto g3 = std::unordered_set<int>({0,3});
    int gid3 = groups.groupItems(g3);

    REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({0,1,2,3}));

    groups.destructGroupItem(0,true);

    REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({1,2,3}));

    SECTION("Normal deletion") {
        groups.destructGroupItem(1,false);

        REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({gid1,2,3}));

        groups.destructGroupItem(gid1,true);

        REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({2,3}));
    }

    SECTION("Cascade deletion") {
        groups.destructGroupItem(1,true);

        REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({2,3}));

        groups.destructGroupItem(2,true);

        REQUIRE(groups.getLeaves(gid3) == std::unordered_set<int>({3}));

        REQUIRE(groups.m_downLink.count(gid3) > 0);
        groups.destructGroupItem(3,true);
        REQUIRE(groups.m_downLink.count(gid3) == 0);
        REQUIRE(groups.m_downLink.size() == 0);
    }

}
