#include "catch.hpp"
#include <memory>
#include <random>
#include <iostream>
#define private public
#define protected public
#include "timeline2/model/trackmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/clipmodel.hpp"
#include "doc/docundostack.hpp"

#include <mlt++/MltProducer.h>
#include <mlt++/MltRepository.h>
#include <mlt++/MltFactory.h>
#include <mlt++/MltProfile.h>

std::default_random_engine g(42);

Mlt::Profile profile_model;

TEST_CASE("Basic creation/deletion of a track", "[TrackModel]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(undoStack);

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

    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(undoStack);


    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile_model, "color", "red");
    std::shared_ptr<Mlt::Producer> producer2 = std::make_shared<Mlt::Producer>(profile_model, "color", "green");
    producer->set("length", 20);
    producer->set("out", 19);
    producer2->set("length", 20);
    producer2->set("out", 19);

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
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(undoStack);


    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile_model, "color", "red");
    std::shared_ptr<Mlt::Producer> producer2 = std::make_shared<Mlt::Producer>(profile_model, "color", "blue");
    std::shared_ptr<Mlt::Producer> producer3 = std::make_shared<Mlt::Producer>(profile_model, "color", "green");
    std::shared_ptr<Mlt::Producer> producer4 = std::make_shared<Mlt::Producer>(profile_model, "color", "yellow");
    producer->set("length", 20);
    producer->set("out", 19);
    producer2->set("length", 20);
    producer2->set("out", 19);
    producer3->set("length", 20);
    producer3->set("out", 19);
    producer4->set("length", 20);
    producer4->set("out", 19);

    REQUIRE(producer->is_valid());
    REQUIRE(producer2->is_valid());
    REQUIRE(producer3->is_valid());
    REQUIRE(producer4->is_valid());
    int cid1 = ClipModel::construct(timeline, producer);
    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);
    int tid3 = TrackModel::construct(timeline);
    int cid2 = ClipModel::construct(timeline, producer2);
    int cid3 = ClipModel::construct(timeline, producer2);
    int cid4 = ClipModel::construct(timeline, producer2);

    SECTION("Insert a clip in a track and change track") {
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);

        REQUIRE(timeline->getClipTrackId(cid1) == -1);
        REQUIRE(timeline->getClipPosition(cid1) == -1);

        int pos = 10;
        //real insert
        // // REQUIRE(timeline->allowClipMove(cid1, tid1, pos));
        REQUIRE(timeline->requestClipMove(cid1, tid1, pos));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == pos);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);

        pos = 1;
        //real
        // // REQUIRE(timeline->allowClipMove(cid1, tid2, pos));
        REQUIRE(timeline->requestClipMove(cid1, tid2, pos));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == pos);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 0);


        // Check conflicts
        int pos2 = producer->get_playtime();
        // // REQUIRE(timeline->allowClipMove(cid2, tid1, pos2));
        REQUIRE(timeline->requestClipMove(cid2, tid1, pos2));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == pos2);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);

        // // REQUIRE_FALSE(timeline->allowClipMove(cid1, tid1, pos2 + 2));
        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, pos2 + 2));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == pos);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == pos2);

        // // REQUIRE_FALSE(timeline->allowClipMove(cid1, tid1, pos2 - 2));
        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, pos2 - 2));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == pos);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == pos2);

        // // REQUIRE(timeline->allowClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
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
        // // REQUIRE(timeline->allowClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);

        // // REQUIRE(timeline->allowClipMove(cid2, tid1, length));
        REQUIRE(timeline->requestClipMove(cid2, tid1, length));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == length);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
    }

    SECTION("Resize orphan clip"){
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == length);
        REQUIRE(timeline->requestClipResize(cid2, 5, true));
        REQUIRE(producer->get_playtime() == length);
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == 5);
        REQUIRE_FALSE(timeline->requestClipResize(cid2, 10, false));
        REQUIRE_FALSE(timeline->requestClipResize(cid2, length + 1, true));
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == 5);
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == 5);
        REQUIRE(timeline->requestClipResize(cid2, 2, false));
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == 2);
        REQUIRE_FALSE(timeline->requestClipResize(cid2, length, true));
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == 2);
        CAPTURE(timeline->m_allClips[cid2]->m_producer->get_in());
        REQUIRE_FALSE(timeline->requestClipResize(cid2, length - 2, true));
        REQUIRE(timeline->requestClipResize(cid2, length - 3, true));
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == length - 3);
    }

    SECTION("Resize inserted clips"){
        // // REQUIRE(timeline->allowClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());

        REQUIRE(timeline->requestClipResize(cid1, 5, true));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->m_allClips[cid1]->getPlaytime() == 5);
        REQUIRE(timeline->getClipPosition(cid1) == 0);

        // // REQUIRE(timeline->allowClipMove(cid2, tid1, 5));
        REQUIRE(timeline->requestClipMove(cid2, tid1, 5));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(producer->get_playtime() == length);

        REQUIRE_FALSE(timeline->requestClipResize(cid1, 6, true));
        REQUIRE_FALSE(timeline->requestClipResize(cid1, 6, false));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());

        REQUIRE(timeline->requestClipResize(cid2, length - 5, false));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getClipPosition(cid2) == 10);

        REQUIRE(timeline->requestClipResize(cid1, 10, true));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
    }

    SECTION("Change track of resized clips"){
        // // REQUIRE(timeline->allowClipMove(cid2, tid1, 5));
        REQUIRE(timeline->requestClipMove(cid2, tid1, 5));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);

        // // REQUIRE(timeline->allowClipMove(cid1, tid2, 10));
        REQUIRE(timeline->requestClipMove(cid1, tid2, 10));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid2) == 1);

        REQUIRE(timeline->requestClipResize(cid1, 5, false));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());

        // // REQUIRE(timeline->allowClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->getTrackClipsCount(tid2) == 0);
    }

    SECTION("Clip Move"){
        // // REQUIRE(timeline->allowClipMove(cid2, tid1, 5));
        REQUIRE(timeline->requestClipMove(cid2, tid1, 5));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == 5);

        // // REQUIRE(timeline->allowClipMove(cid1, tid1, 5 + length));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5 + length));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5 + length);
        REQUIRE(timeline->getClipPosition(cid2) == 5);

        // // REQUIRE_FALSE(timeline->allowClipMove(cid1, tid1, 3 + length));
        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, 3 + length));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5 + length);
        REQUIRE(timeline->getClipPosition(cid2) == 5);

        // REQUIRE_FALSE(timeline->allowClipMove(cid1, tid1, 0));
        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5 + length);
        REQUIRE(timeline->getClipPosition(cid2) == 5);

        // REQUIRE(timeline->allowClipMove(cid2, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid2, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5 + length);
        REQUIRE(timeline->getClipPosition(cid2) == 0);

        // REQUIRE_FALSE(timeline->allowClipMove(cid1, tid1, 0));
        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5 + length);
        REQUIRE(timeline->getClipPosition(cid2) == 0);

        // REQUIRE_FALSE(timeline->allowClipMove(cid1, tid1, length - 5));
        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, length - 5));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5 + length);
        REQUIRE(timeline->getClipPosition(cid2) == 0);

        // REQUIRE(timeline->allowClipMove(cid1, tid1, length));
        REQUIRE(timeline->requestClipMove(cid1, tid1, length));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length);
        REQUIRE(timeline->getClipPosition(cid2) == 0);

        REQUIRE(timeline->requestClipResize(cid2, length - 5, true));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length);
        REQUIRE(timeline->getClipPosition(cid2) == 0);

        // REQUIRE(timeline->allowClipMove(cid1, tid1, length - 5));
        REQUIRE(timeline->requestClipMove(cid1, tid1, length - 5));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length - 5);
        REQUIRE(timeline->getClipPosition(cid2) == 0);

        REQUIRE(timeline->requestClipResize(cid2, length - 10, false));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length - 5);
        REQUIRE(timeline->getClipPosition(cid2) == 5);

        // REQUIRE_FALSE(timeline->allowClipMove(cid1, tid1, 0));
        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length - 5);
        REQUIRE(timeline->getClipPosition(cid2) == 5);

        // REQUIRE(timeline->allowClipMove(cid2, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid2, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length - 5);
        REQUIRE(timeline->getClipPosition(cid2) == 0);
    }

    SECTION ("Group move") {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid2, tid1, length + 3));
        REQUIRE(timeline->requestClipMove(cid3, tid1, 2*length + 5));
        REQUIRE(timeline->requestClipMove(cid4, tid2, 4));

        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid3]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid4]->getCurrentTrackId() == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(timeline->getClipPosition(cid2) == length + 3);
        REQUIRE(timeline->getClipPosition(cid3) == 2*length + 5);
        REQUIRE(timeline->getClipPosition(cid4) == 4);

        //check that move is possible without groups
        REQUIRE(timeline->requestClipMove(cid3, tid1, 2*length + 3));
        undoStack->undo();
        //check that move is possible without groups
        REQUIRE(timeline->requestClipMove(cid4, tid2, 9));
        undoStack->undo();

        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid3]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid4]->getCurrentTrackId() == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(timeline->getClipPosition(cid2) == length + 3);
        REQUIRE(timeline->getClipPosition(cid3) == 2*length + 5);
        REQUIRE(timeline->getClipPosition(cid4) == 4);

        //grouping
        REQUIRE(timeline->requestGroupClips({cid1, cid3, cid4}));

        //move left is now forbidden, because clip1 is at position 0
        REQUIRE_FALSE(timeline->requestClipMove(cid3, tid1, 2*length + 3));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid3]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid4]->getCurrentTrackId() == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(timeline->getClipPosition(cid2) == length + 3);
        REQUIRE(timeline->getClipPosition(cid3) == 2*length + 5);
        REQUIRE(timeline->getClipPosition(cid4) == 4);
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());

        //this move is impossible, because clip1 runs into clip2
        REQUIRE_FALSE(timeline->requestClipMove(cid4, tid2, 9));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid3]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->m_allClips[cid4]->getCurrentTrackId() == tid2);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(timeline->getClipPosition(cid2) == length + 3);
        REQUIRE(timeline->getClipPosition(cid3) == 2*length + 5);
        REQUIRE(timeline->getClipPosition(cid4) == 4);

        //this move is possible
        REQUIRE(timeline->requestClipMove(cid3, tid1, 2*length + 8));
        auto state1 = [&](){
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid3) == 0);
            REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
            REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
            REQUIRE(timeline->m_allClips[cid3]->getCurrentTrackId() == tid1);
            REQUIRE(timeline->m_allClips[cid4]->getCurrentTrackId() == tid2);
            REQUIRE(timeline->getClipPosition(cid1) == 3);
            REQUIRE(timeline->getClipPosition(cid2) == length + 3);
            REQUIRE(timeline->getClipPosition(cid3) == 2*length + 8);
            REQUIRE(timeline->getClipPosition(cid4) == 7);
        };
        state1();

        //this move is possible
        REQUIRE(timeline->requestClipMove(cid1, tid2, 8));
        auto state2 = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
            REQUIRE(timeline->getTrackById(tid3)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 2);
            REQUIRE(timeline->getTrackClipsCount(tid3) == 1);
            REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid2);
            REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
            REQUIRE(timeline->m_allClips[cid3]->getCurrentTrackId() == tid2);
            REQUIRE(timeline->m_allClips[cid4]->getCurrentTrackId() == tid3);
            REQUIRE(timeline->getClipPosition(cid1) == 8);
            REQUIRE(timeline->getClipPosition(cid2) == length + 3);
            REQUIRE(timeline->getClipPosition(cid3) == 2*length + 5 + 8);
            REQUIRE(timeline->getClipPosition(cid4) == 4 + 8);
        };
        state2();

        undoStack->undo();
        state1();

        undoStack->redo();
        state2();

        REQUIRE(timeline->requestClipMove(cid1, tid1, 3));
        state1();

    }

    SECTION("Group move to unavailable track") {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 10));
        REQUIRE(timeline->requestClipMove(cid2, tid2, 12));
        REQUIRE(timeline->requestGroupClips({cid1, cid2}));
        auto state = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
            REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
            REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid2);
            REQUIRE(timeline->getClipPosition(cid1) == 10);
            REQUIRE(timeline->getClipPosition(cid2) == 12);
        };
        state();

        REQUIRE_FALSE(timeline->requestClipMove(cid2, tid1, 10));
        state();
        REQUIRE_FALSE(timeline->requestClipMove(cid2, tid1, 100));
        state();
        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid3, 100));
        state();
    }

    SECTION("Group move with non-consecutive track ids") {
        int tid5 = TrackModel::construct(timeline);
        int cid6 = ClipModel::construct(timeline, producer);
        int tid6 = TrackModel::construct(timeline);
        REQUIRE(tid5 + 1 != tid6);

        REQUIRE(timeline->requestClipMove(cid1, tid5, 10));
        REQUIRE(timeline->requestClipMove(cid2, tid5, length + 10));
        REQUIRE(timeline->requestGroupClips({cid1, cid2}));
        auto state = [&](int t) {
            REQUIRE(timeline->getTrackById(t)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(t) == 2);
            REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == t);
            REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == t);
            REQUIRE(timeline->getClipPosition(cid1) == 10);
            REQUIRE(timeline->getClipPosition(cid2) == 10 + length);
        };
        state(tid5);
        REQUIRE(timeline->requestClipMove(cid1, tid6, 10));
        state(tid6);

    }
}

TEST_CASE("Check id unicity", "[ClipModel]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(undoStack);

    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile_model, "color", "red");
    producer->set("length", 20);
    producer->set("out", 19);

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

TEST_CASE("Undo and Redo", "[ClipModel]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(undoStack);

    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile_model, "color", "red");
    std::shared_ptr<Mlt::Producer> producer2 = std::make_shared<Mlt::Producer>(profile_model, "color", "blue");
    producer->set("length", 20);
    producer->set("out", 19);
    producer2->set("length", 20);
    producer2->set("out", 19);

    REQUIRE(producer->is_valid());
    REQUIRE(producer2->is_valid());
    int cid1 = ClipModel::construct(timeline, producer);
    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);
    int cid2 = ClipModel::construct(timeline, producer2);

    int length = producer->get_playtime();

    int init_index = undoStack->index();

    SECTION("Basic move undo") {
        // REQUIRE(timeline->allowClipMove(cid1, tid1, 5));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);

        // REQUIRE(timeline->allowClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(undoStack->index() == init_index + 2);

        undoStack->undo();
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);

        undoStack->redo();
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(undoStack->index() == init_index + 2);

        undoStack->undo();
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);

        // REQUIRE(timeline->allowClipMove(cid1, tid1, 2*length));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 2*length));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 2*length);
        REQUIRE(undoStack->index() == init_index + 2);

        undoStack->undo();
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);

        undoStack->redo();
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 2*length);
        REQUIRE(undoStack->index() == init_index + 2);

        undoStack->undo();
        undoStack->undo();
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
        REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == -1);
        REQUIRE(undoStack->index() == init_index);
    }

    SECTION("Basic resize orphan clip undo") {
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == length);

        REQUIRE(timeline->requestClipResize(cid2, length - 5, true));
        REQUIRE(undoStack->index() == init_index + 1);
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() ==  length - 5);

        REQUIRE(timeline->requestClipResize(cid2, length - 10, false));
        REQUIRE(undoStack->index() == init_index + 2);
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() ==  length - 10);

        REQUIRE_FALSE(timeline->requestClipResize(cid2, length, false));
        REQUIRE(undoStack->index() == init_index + 2);
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() ==  length - 10);

        undoStack->undo();
        REQUIRE(undoStack->index() == init_index + 1);
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() ==  length - 5);

        undoStack->redo();
        REQUIRE(undoStack->index() == init_index + 2);
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() ==  length - 10);

        undoStack->undo();
        REQUIRE(undoStack->index() == init_index + 1);
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() ==  length - 5);

        undoStack->undo();
        REQUIRE(undoStack->index() == init_index);
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() ==  length);
    }
    SECTION("Basic resize inserted clip undo") {
        REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == length);

        auto check = [&](int pos, int l) {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->m_allClips[cid2]->getCurrentTrackId() == tid1);
            REQUIRE(timeline->m_allClips[cid2]->getPlaytime() == l);
            REQUIRE(timeline->getClipPosition(cid2) == pos);
        };
        // REQUIRE(timeline->allowClipMove(cid2, tid1, 5));
        REQUIRE(timeline->requestClipMove(cid2, tid1, 5));
        INFO("Test 1");
        check(5, length);
        REQUIRE(undoStack->index() == init_index + 1);

        REQUIRE(timeline->requestClipResize(cid2, length - 5, true));
        INFO("Test 2");
        check(5, length - 5);
        REQUIRE(undoStack->index() == init_index + 2);

        REQUIRE(timeline->requestClipResize(cid2, length - 10, false));
        INFO("Test 3");
        check(10, length - 10);
        REQUIRE(undoStack->index() == init_index + 3);

        REQUIRE_FALSE(timeline->requestClipResize(cid2, length, false));
        INFO("Test 4");
        check(10, length - 10);
        REQUIRE(undoStack->index() == init_index + 3);

        undoStack->undo();
        INFO("Test 5");
        check(5, length - 5);
        REQUIRE(undoStack->index() == init_index + 2);

        undoStack->redo();
        INFO("Test 6");
        check(10, length - 10);
        REQUIRE(undoStack->index() == init_index + 3);

        undoStack->undo();
        INFO("Test 7");
        check(5, length - 5);
        REQUIRE(undoStack->index() == init_index + 2);

        undoStack->undo();
        INFO("Test 8");
        check(5, length);
        REQUIRE(undoStack->index() == init_index + 1);
    }
    SECTION("Clip Insertion Undo") {
        std::shared_ptr<Mlt::Producer> prod = std::make_shared<Mlt::Producer>(profile_model, "color", "red");
        prod->set("length", 20);
        prod->set("out", 19);

        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        auto state1 = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(undoStack->index() == init_index + 1);
        };
        state1();

        int cid3;
        REQUIRE_FALSE(timeline->requestClipInsert(prod, tid1, 5, cid3));
        state1();

        REQUIRE_FALSE(timeline->requestClipInsert(prod, tid1, 6, cid3));
        state1();

        REQUIRE(timeline->requestClipInsert(prod, tid1, 5 + length, cid3));
        auto state2 = [&]() {
            std::cout<<"consist"<<std::endl;
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
            REQUIRE(timeline->m_allClips[cid3]->getCurrentTrackId() == tid1);
            std::cout<<"pos"<<std::endl;
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getClipPosition(cid3) == 5 + length);
            REQUIRE(undoStack->index() == init_index + 2);
        };
        state2();

        REQUIRE(timeline->requestClipMove(cid3, tid1, 10 + length));
        auto state3 = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
            REQUIRE(timeline->m_allClips[cid3]->getCurrentTrackId() == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getClipPosition(cid3) == 10 + length);
            REQUIRE(undoStack->index() == init_index + 3);
        };
        state3();

        REQUIRE(timeline->requestClipMove(cid1, tid1, 10));
        auto state4 = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->m_allClips[cid1]->getCurrentTrackId() == tid1);
            REQUIRE(timeline->m_allClips[cid3]->getCurrentTrackId() == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 10);
            REQUIRE(timeline->getClipPosition(cid3) == 10 + length);
            REQUIRE(undoStack->index() == init_index + 4);
        };
        state4();

        undoStack->undo();
        state3();

        undoStack->undo();
        state2();

        undoStack->undo();
        state1();

        std::cout<<"REDOING"<<std::endl;
        undoStack->redo();
        state2();

        undoStack->redo();
        state3();

        undoStack->redo();
        state4();

        undoStack->undo();
        state3();

        undoStack->undo();
        state2();

        undoStack->undo();
        state1();
    }
}
