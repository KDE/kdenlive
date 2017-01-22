#include "catch.hpp"
#include <unordered_set>
#define private public
#include "timeline2/model/groupsmodel.hpp"

TEST_CASE("Functional test of the group hierarchy", "[GroupsModel]")
{
    GroupsModel groups;

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
}
