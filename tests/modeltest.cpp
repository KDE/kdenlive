#include "catch.hpp"
#include <memory>
#define private public
#include "timeline2/model/trackmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/model/clipmodel.hpp"

#include <mlt++/MltProducer.h>
#include <mlt++/MltFactory.h>
#include <mlt++/MltProfile.h>



TEST_CASE("Basic creation/deletion of a track", "[TrackModel]")
{
    std::shared_ptr<TimelineModel> timeline = std::make_shared<TimelineModel>();

    int id1 = TrackModel::construct(timeline);
    REQUIRE(timeline->getTracksNumber() == 1);

    int id2 = TrackModel::construct(timeline);
    REQUIRE(timeline->getTracksNumber() == 2);

    int id3 = TrackModel::construct(timeline);
    REQUIRE(timeline->getTracksNumber() == 3);

    // Test deletion
    timeline->deleteTrackById(id1);
    REQUIRE(timeline->getTracksNumber() == 2);

    timeline->deleteTrackById(id2);
    REQUIRE(timeline->getTracksNumber() == 1);

    timeline->deleteTrackById(id3);
    REQUIRE(timeline->getTracksNumber() == 0);
}



TEST_CASE("Basic creation/deletion of a clip", "[ClipModel]")
{
    std::shared_ptr<TimelineModel> timeline = std::make_shared<TimelineModel>();

    Mlt::Factory::init( NULL );
    Mlt::Profile profile;

    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile, "test");

    REQUIRE(timeline->getClipsNumber() == 0);
    int id1 = ClipModel::construct(timeline, producer);
    REQUIRE(timeline->getClipsNumber() == 1);
}
