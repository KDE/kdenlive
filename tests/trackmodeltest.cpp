#include "catch.hpp"
#include <memory>
#define private public
#include "timeline2/model/trackmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/model/clipmodel.hpp"




TEST_CASE("Basic creation of a track", "[TrackModel]")
{
    std::shared_ptr<TimelineModel> timeline = std::make_shared<TimelineModel>();

    TrackModel::construct(timeline);

    REQUIRE(timeline->getTrackNumber() == 1);

    TrackModel::construct(timeline);

    REQUIRE(timeline->getTrackNumber() == 2);
}


