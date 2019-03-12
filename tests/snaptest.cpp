#include "catch.hpp"
#include "timeline2/model/snapmodel.hpp"
#include <iostream>
#include <unordered_set>

TEST_CASE("Snap points model test", "[SnapModel]")
{
    SnapModel snap;

    SECTION("Basic test")
    {
        // empty
        REQUIRE(snap.getClosestPoint(0) == -1);
        REQUIRE(snap.getClosestPoint(10) == -1);

        snap.addPoint(10);
        REQUIRE(snap.getClosestPoint(0) == 10);
        REQUIRE(snap.getClosestPoint(10) == 10);
        REQUIRE(snap.getClosestPoint(11) == 10);
        REQUIRE(snap.getClosestPoint(9) == 10);
        REQUIRE(snap.getClosestPoint(999) == 10);

        snap.addPoint(10);
        REQUIRE(snap.getClosestPoint(0) == 10);
        REQUIRE(snap.getClosestPoint(10) == 10);
        REQUIRE(snap.getClosestPoint(11) == 10);
        REQUIRE(snap.getClosestPoint(9) == 10);
        REQUIRE(snap.getClosestPoint(999) == 10);

        snap.addPoint(15);
        REQUIRE(snap.getClosestPoint(0) == 10);
        REQUIRE(snap.getClosestPoint(10) == 10);
        REQUIRE(snap.getClosestPoint(11) == 10);
        REQUIRE(snap.getClosestPoint(9) == 10);
        REQUIRE(snap.getClosestPoint(12) == 10);
        REQUIRE(snap.getClosestPoint(13) == 15);
        REQUIRE(snap.getClosestPoint(15) == 15);
        REQUIRE(snap.getClosestPoint(16) == 15);
        REQUIRE(snap.getClosestPoint(999) == 15);

        snap.removePoint(10);
        REQUIRE(snap.getClosestPoint(0) == 10);
        REQUIRE(snap.getClosestPoint(10) == 10);
        REQUIRE(snap.getClosestPoint(11) == 10);
        REQUIRE(snap.getClosestPoint(9) == 10);
        REQUIRE(snap.getClosestPoint(12) == 10);
        REQUIRE(snap.getClosestPoint(13) == 15);
        REQUIRE(snap.getClosestPoint(15) == 15);
        REQUIRE(snap.getClosestPoint(16) == 15);
        REQUIRE(snap.getClosestPoint(999) == 15);

        snap.removePoint(10);
        REQUIRE(snap.getClosestPoint(0) == 15);
        REQUIRE(snap.getClosestPoint(10) == 15);
        REQUIRE(snap.getClosestPoint(11) == 15);
        REQUIRE(snap.getClosestPoint(9) == 15);
        REQUIRE(snap.getClosestPoint(12) == 15);
        REQUIRE(snap.getClosestPoint(13) == 15);
        REQUIRE(snap.getClosestPoint(15) == 15);
        REQUIRE(snap.getClosestPoint(16) == 15);
        REQUIRE(snap.getClosestPoint(999) == 15);

        snap.removePoint(15);
        REQUIRE(snap.getClosestPoint(0) == -1);
        REQUIRE(snap.getClosestPoint(10) == -1);
        REQUIRE(snap.getClosestPoint(11) == -1);
        REQUIRE(snap.getClosestPoint(9) == -1);
        REQUIRE(snap.getClosestPoint(12) == -1);
        REQUIRE(snap.getClosestPoint(13) == -1);
        REQUIRE(snap.getClosestPoint(15) == -1);
        REQUIRE(snap.getClosestPoint(16) == -1);
        REQUIRE(snap.getClosestPoint(999) == -1);
    }

    SECTION("Snappoint Ignoring")
    {
        REQUIRE(snap.getClosestPoint(0) == -1);
        REQUIRE(snap.getClosestPoint(10) == -1);

        snap.addPoint(10);
        snap.addPoint(10);
        auto state = [&]() {
            REQUIRE(snap.getClosestPoint(0) == 10);
            REQUIRE(snap.getClosestPoint(10) == 10);
            REQUIRE(snap.getClosestPoint(11) == 10);
            REQUIRE(snap.getClosestPoint(9) == 10);
            REQUIRE(snap.getClosestPoint(999) == 10);
        };
        state();

        snap.ignore({10});
        state();

        snap.ignore({10});
        REQUIRE(snap.getClosestPoint(0) == -1);
        REQUIRE(snap.getClosestPoint(10) == -1);
        snap.unIgnore();
        state();

        snap.addPoint(15);
        REQUIRE(snap.getClosestPoint(0) == 10);
        REQUIRE(snap.getClosestPoint(10) == 10);
        REQUIRE(snap.getClosestPoint(11) == 10);
        REQUIRE(snap.getClosestPoint(9) == 10);
        REQUIRE(snap.getClosestPoint(12) == 10);
        REQUIRE(snap.getClosestPoint(13) == 15);
        REQUIRE(snap.getClosestPoint(15) == 15);
        REQUIRE(snap.getClosestPoint(16) == 15);
        REQUIRE(snap.getClosestPoint(999) == 15);

        snap.ignore({15});
        state();
        snap.removePoint(10);
        state();
        snap.removePoint(10);
        REQUIRE(snap.getClosestPoint(0) == -1);
        REQUIRE(snap.getClosestPoint(10) == -1);

        snap.unIgnore();
        REQUIRE(snap.getClosestPoint(0) == 15);
        REQUIRE(snap.getClosestPoint(10) == 15);
        REQUIRE(snap.getClosestPoint(11) == 15);
        REQUIRE(snap.getClosestPoint(9) == 15);
        REQUIRE(snap.getClosestPoint(999) == 15);
    }
}
