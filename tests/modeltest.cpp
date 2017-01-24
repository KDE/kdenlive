#include "catch.hpp"
#include <memory>
#include <random>
#define private public
#define protected public
#include "timeline2/model/trackmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/model/clipmodel.hpp"

#include <mlt++/MltProducer.h>
#include <mlt++/MltFactory.h>
#include <mlt++/MltProfile.h>

std::default_random_engine g(42);


TEST_CASE("Basic creation/deletion of a track", "[TrackModel]")
{
    std::shared_ptr<TimelineModel> timeline = TimelineModel::construct();

    int id1 = TrackModel::construct(timeline);
    REQUIRE(timeline->getTracksCount() == 1);

    int id2 = TrackModel::construct(timeline);
    REQUIRE(timeline->getTracksCount() == 2);

    int id3 = TrackModel::construct(timeline);
    REQUIRE(timeline->getTracksCount() == 3);

    // Test deletion
    timeline->deleteTrackById(id1);
    REQUIRE(timeline->getTracksCount() == 2);

    timeline->deleteTrackById(id2);
    REQUIRE(timeline->getTracksCount() == 1);

    timeline->deleteTrackById(id3);
    REQUIRE(timeline->getTracksCount() == 0);
}



TEST_CASE("Basic creation/deletion of a clip", "[ClipModel]")
{
    std::shared_ptr<TimelineModel> timeline = TimelineModel::construct();

    Mlt::Factory::init( NULL );
    Mlt::Profile profile;

    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile, "test");
    std::shared_ptr<Mlt::Producer> producer2 = std::make_shared<Mlt::Producer>(profile, "test2");

    REQUIRE(timeline->getClipsCount() == 0);
    int id1 = ClipModel::construct(timeline, producer);
    REQUIRE(timeline->getClipsCount() == 1);

    int id2 = ClipModel::construct(timeline, producer2);
    REQUIRE(timeline->getClipsCount() == 2);

    int id3 = ClipModel::construct(timeline, producer);
    REQUIRE(timeline->getClipsCount() == 3);

    // Test deletion
    timeline->deleteClipById(id2);
    REQUIRE(timeline->getClipsCount() == 2);
    timeline->deleteClipById(id3);
    REQUIRE(timeline->getClipsCount() == 1);
    timeline->deleteClipById(id1);
    REQUIRE(timeline->getClipsCount() == 0);
}

TEST_CASE("Clip manipulation", "[ClipModel]")
{
    std::shared_ptr<TimelineModel> timeline = TimelineModel::construct();

    Mlt::Factory::init( NULL );
    Mlt::Profile profile;

    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile, "../tests/small.mp4");
    REQUIRE(producer->is_valid());
    int cid1 = ClipModel::construct(timeline, producer);
    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);
    int cid2 = ClipModel::construct(timeline, producer);

    SECTION("Insert a clip in a track and change track") {
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);

        REQUIRE(timeline->getClipTrackId(cid1) == -1);
        REQUIRE(timeline->getClipPosition(cid1) == -1);

        int pos = 10;
        //dry
        REQUIRE(timeline->requestClipChangeTrack(cid1, tid1, pos,true));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);

        REQUIRE(timeline->getClipTrackId(cid1) == -1);
        REQUIRE(timeline->getClipPosition(cid1) == -1);

        //real insert
        REQUIRE(timeline->requestClipChangeTrack(cid1, tid1, pos));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == pos);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);

        pos = 1;
        //dry
        REQUIRE(timeline->requestClipChangeTrack(cid1, tid2, pos,true));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 10);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);
        //real
        REQUIRE(timeline->requestClipChangeTrack(cid1, tid2, pos));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == pos);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 0);


        // Check conflicts
        int pos2 = producer->get_playtime();
        REQUIRE(timeline->requestClipChangeTrack(cid2, tid1, pos2));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == pos2);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);

        REQUIRE_FALSE(timeline->requestClipChangeTrack(cid1, tid1, pos2 + 2, true));
        REQUIRE_FALSE(timeline->requestClipChangeTrack(cid1, tid1, pos2 + 2));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == pos);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == pos2);

        REQUIRE_FALSE(timeline->requestClipChangeTrack(cid1, tid1, pos2 - 2, true));
        REQUIRE_FALSE(timeline->requestClipChangeTrack(cid1, tid1, pos2 - 2));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == pos);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == pos2);

        REQUIRE(timeline->requestClipChangeTrack(cid1, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == pos2);
    }

    int length = producer->get_playtime();
    SECTION("Insert consecutive clips") {
        REQUIRE(timeline->requestClipChangeTrack(cid1, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);

        REQUIRE(timeline->requestClipChangeTrack(cid2, tid1, length));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == length);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
    }

    SECTION("Resize orphan clip"){
        REQUIRE(timeline->m_allClips[cid2]->slotRequestResize(5, true, true));
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == length);
        REQUIRE(timeline->m_allClips[cid2]->slotRequestResize(5, true));
        REQUIRE(producer->get_playtime() == length);
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == 5);
        REQUIRE_FALSE(timeline->m_allClips[cid2]->slotRequestResize(10, false));
        REQUIRE_FALSE(timeline->m_allClips[cid2]->slotRequestResize(length + 1, true));
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == 5);
        REQUIRE_FALSE(timeline->m_allClips[cid2]->slotRequestResize(length + 1, true, true));
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == 5);
        REQUIRE(timeline->m_allClips[cid2]->slotRequestResize(2, false));
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == 2);
        REQUIRE_FALSE(timeline->m_allClips[cid2]->slotRequestResize(length, true));
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == 2);
        CAPTURE(timeline->m_allClips[cid2]->m_producer->get_in());
        REQUIRE_FALSE(timeline->m_allClips[cid2]->slotRequestResize(length - 2, true));
        REQUIRE(timeline->m_allClips[cid2]->slotRequestResize(length - 3, true));
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == length - 3);
    }

    SECTION("Resize inserted clips"){
        REQUIRE(timeline->requestClipChangeTrack(cid1, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());

        REQUIRE(timeline->m_allClips[cid1]->slotRequestResize(5, true));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->m_allClips[cid1]->getPlaytime() == 5);
        REQUIRE(timeline->getClipPosition(cid1) == 0);

        REQUIRE(timeline->requestClipChangeTrack(cid2, tid1, 5));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(producer->get_playtime() == length);

        REQUIRE_FALSE(timeline->m_allClips[cid1]->slotRequestResize(6, true));
        REQUIRE_FALSE(timeline->m_allClips[cid1]->slotRequestResize(6, false));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());

        REQUIRE(timeline->m_allClips[cid2]->slotRequestResize(length - 5, false));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getClipPosition(cid2) == 10);

        REQUIRE(timeline->m_allClips[cid1]->slotRequestResize(10, true));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
    }

    SECTION("Change track of resized clips"){
        REQUIRE(timeline->requestClipChangeTrack(cid2, tid1, 5));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);

        REQUIRE(timeline->requestClipChangeTrack(cid1, tid2, 10));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);

        REQUIRE(timeline->m_allClips[cid1]->slotRequestResize(5, false));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());

        REQUIRE(timeline->requestClipChangeTrack(cid1, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);
    }
}

TEST_CASE("Check id unicity", "[ClipModel]")
{
    std::shared_ptr<TimelineModel> timeline = TimelineModel::construct();

    Mlt::Factory::init( NULL );
    Mlt::Profile profile;

    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile, "../tests/small.mp4");
    REQUIRE(producer->is_valid());

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
            int cid = ClipModel::construct(timeline, producer);
            REQUIRE(all_ids.count(cid) == 0);
            all_ids.insert(cid);
            REQUIRE(timeline->getClipsCount() == all_ids.size() - track_ids.size());
        }
    }

    REQUIRE(all_ids.size() == nbr);
    REQUIRE(all_ids.size() != track_ids.size());
}
