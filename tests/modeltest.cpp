#include "catch.hpp"
#include <memory>
#include <random>
#include <iostream>
#include "doc/docundostack.hpp"

#include <mlt++/MltProducer.h>
#include <mlt++/MltRepository.h>
#include <mlt++/MltFactory.h>
#include <mlt++/MltProfile.h>
#define private public
#define protected public
#include "timeline2/model/trackmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/model/compositionmodel.hpp"

std::default_random_engine g(42);

Mlt::Profile profile_model;

TEST_CASE("Basic creation/deletion of a track", "[TrackModel]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);

    int id1 = TrackModel::construct(timeline);
    REQUIRE(timeline->getTracksCount() == 1);
    REQUIRE(timeline->getTrackPosition(id1) == 0);

    int id2 = TrackModel::construct(timeline);
    REQUIRE(timeline->getTracksCount() == 2);
    REQUIRE(timeline->getTrackPosition(id2) == 1);

    int id3 = TrackModel::construct(timeline);
    REQUIRE(timeline->getTracksCount() == 3);
    REQUIRE(timeline->getTrackPosition(id3) == 2);

    int id4;
    REQUIRE(timeline->requestTrackInsertion(1, id4));
    REQUIRE(timeline->getTracksCount() == 4);
    REQUIRE(timeline->getTrackPosition(id1) == 0);
    REQUIRE(timeline->getTrackPosition(id4) == 1);
    REQUIRE(timeline->getTrackPosition(id2) == 2);
    REQUIRE(timeline->getTrackPosition(id3) == 3);

    // Test deletion
    REQUIRE(timeline->requestTrackDeletion(id3));
    REQUIRE(timeline->getTracksCount() == 3);

    REQUIRE(timeline->requestTrackDeletion(id1));
    REQUIRE(timeline->getTracksCount() == 2);

    REQUIRE(timeline->requestTrackDeletion(id4));
    REQUIRE(timeline->getTracksCount() == 1);

    REQUIRE(timeline->requestTrackDeletion(id2));
    REQUIRE(timeline->getTracksCount() == 0);

    SECTION("Delete a track with groups") {
        int tid1, tid2;
        REQUIRE(timeline->requestTrackInsertion(-1, tid1));
        REQUIRE(timeline->requestTrackInsertion(-1, tid2));
        std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile_model, "color", "red");
        producer->set("length", 20);
        producer->set("out", 19);
        int length = producer->get_playtime();
        int cid1,cid2,cid3,cid4;
        REQUIRE(timeline->requestClipInsertion(producer, tid1, 2, cid1));
        REQUIRE(timeline->requestClipInsertion(producer, tid2, 0, cid2));
        REQUIRE(timeline->requestClipInsertion(producer, tid2, length, cid3));
        REQUIRE(timeline->requestClipInsertion(producer, tid2, 2*length, cid4));
        REQUIRE(timeline->getClipsCount() == 4);
        REQUIRE(timeline->getTracksCount() == 2);

        auto g1 = std::unordered_set<int>({cid1, cid3});
        auto g2 = std::unordered_set<int>({cid2, cid4});
        auto g3 = std::unordered_set<int>({cid1, cid4});
        REQUIRE(timeline->requestClipsGroup(g1));
        REQUIRE(timeline->requestClipsGroup(g2));
        REQUIRE(timeline->requestClipsGroup(g3));

        REQUIRE(timeline->requestTrackDeletion(tid1));
        REQUIRE(timeline->getClipsCount() == 3);
        REQUIRE(timeline->getTracksCount() == 1);

    }
}



TEST_CASE("Basic creation/deletion of a clip", "[ClipModel]")
{

    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);


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
    REQUIRE(timeline->requestItemDeletion(id2));
    REQUIRE(timeline->getClipsCount() == 2);
    REQUIRE(timeline->requestItemDeletion(id3));
    REQUIRE(timeline->getClipsCount() == 1);
    REQUIRE(timeline->requestItemDeletion(id1));
    REQUIRE(timeline->getClipsCount() == 0);

}

TEST_CASE("Clip manipulation", "[ClipModel]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);


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
        REQUIRE(timeline->getClipPlaytime(cid2) == length);
        REQUIRE(timeline->requestClipResize(cid2, 5, true));
        REQUIRE(producer->get_playtime() == length);
        auto inOut = std::pair<int,int>{0,4};
        REQUIRE(timeline->m_allClips[cid2]->getInOut() == inOut );
        REQUIRE(timeline->getClipPlaytime(cid2) == 5);
        REQUIRE_FALSE(timeline->requestClipResize(cid2, 10, false));
        REQUIRE_FALSE(timeline->requestClipResize(cid2, length + 1, true));
        REQUIRE(timeline->getClipPlaytime(cid2) == 5);
        REQUIRE(timeline->getClipPlaytime(cid2) == 5);
        REQUIRE(timeline->requestClipResize(cid2, 2, false));
        inOut = std::pair<int,int>{3,4};
        REQUIRE(timeline->m_allClips[cid2]->getInOut() == inOut );
        REQUIRE(timeline->getClipPlaytime(cid2) == 2);
        REQUIRE_FALSE(timeline->requestClipResize(cid2, length, true));
        REQUIRE(timeline->getClipPlaytime(cid2) == 2);
        CAPTURE(timeline->m_allClips[cid2]->m_producer->get_in());
        REQUIRE_FALSE(timeline->requestClipResize(cid2, length - 2, true));
        REQUIRE(timeline->requestClipResize(cid2, length - 3, true));
        REQUIRE(timeline->getClipPlaytime(cid2) == length - 3);
    }

    SECTION("Resize inserted clips"){
        // // REQUIRE(timeline->allowClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());

        REQUIRE(timeline->requestClipResize(cid1, 5, true));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getClipPlaytime(cid1) == 5);
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
        REQUIRE(timeline->requestClipMove(cid2, tid1, 5));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid2) == 5);

        REQUIRE(timeline->requestClipMove(cid1, tid1, 5 + length));
        auto state = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5 + length);
            REQUIRE(timeline->getClipPosition(cid2) == 5);
        };
        state();

        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, 3 + length));
        state();

        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, 0));
        state();

        REQUIRE(timeline->requestClipMove(cid2, tid1, 0));
        auto state2 = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5 + length);
            REQUIRE(timeline->getClipPosition(cid2) == 0);
        };
        state2();

        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, 0));
        state2();

        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, length - 5));
        state2();

        REQUIRE(timeline->requestClipMove(cid1, tid1, length));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length);
        REQUIRE(timeline->getClipPosition(cid2) == 0);

        REQUIRE(timeline->requestClipResize(cid2, length - 5, true));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length);
        REQUIRE(timeline->getClipPosition(cid2) == 0);

        // REQUIRE(timeline->allowClipMove(cid1, tid1, length - 5));
        REQUIRE(timeline->requestClipMove(cid1, tid1, length - 5));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length - 5);
        REQUIRE(timeline->getClipPosition(cid2) == 0);

        REQUIRE(timeline->requestClipResize(cid2, length - 10, false));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length - 5);
        REQUIRE(timeline->getClipPosition(cid2) == 5);

        REQUIRE_FALSE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length - 5);
        REQUIRE(timeline->getClipPosition(cid2) == 5);

        REQUIRE(timeline->requestClipMove(cid2, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == length - 5);
        REQUIRE(timeline->getClipPosition(cid2) == 0);
    }

    SECTION ("Move and resize") {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipResize(cid1, length - 2, false));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        auto state = [&](){
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPlaytime(cid1) == length - 2);
        };
        state();

        //try to resize past the left end
        REQUIRE_FALSE(timeline->requestClipResize(cid1, length, false));
        state();

        REQUIRE(timeline->requestClipResize(cid1, length - 4, true));
        REQUIRE(timeline->requestClipMove(cid2, tid1, length - 4 + 1));
        REQUIRE(timeline->requestClipResize(cid2, length - 2, false));
        REQUIRE(timeline->requestClipMove(cid2, tid1, length - 4 + 1));
        auto state2 = [&](){
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPlaytime(cid1) == length - 4);
            REQUIRE(timeline->getClipPosition(cid2) == length - 4 + 1);
            REQUIRE(timeline->getClipPlaytime(cid2) == length - 2);
        };
        state2();

        //the gap between the two clips is 1 frame, we try to resize them by 2 frames
        REQUIRE_FALSE(timeline->requestClipResize(cid1, length - 2, true));
        state2();
        REQUIRE_FALSE(timeline->requestClipResize(cid2, length, false));
        state2();

        REQUIRE(timeline->requestClipMove(cid2, tid1, length - 4));
        auto state3 = [&](){
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPlaytime(cid1) == length - 4);
            REQUIRE(timeline->getClipPosition(cid2) == length - 4);
            REQUIRE(timeline->getClipPlaytime(cid2) == length - 2);
        };
        state3();

        //Now the gap is 0 frames, the resize should still fail
        REQUIRE_FALSE(timeline->requestClipResize(cid1, length - 2, true));
        state3();
        REQUIRE_FALSE(timeline->requestClipResize(cid2, length, false));
        state3();

        //We move cid1 out of the way
        REQUIRE(timeline->requestClipMove(cid1, tid2, 0));
        //now resize should work
        REQUIRE(timeline->requestClipResize(cid1, length - 2, true));
        REQUIRE(timeline->requestClipResize(cid2, length, false));

    }

    SECTION ("Group move") {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid2, tid1, length + 3));
        REQUIRE(timeline->requestClipMove(cid3, tid1, 2*length + 5));
        REQUIRE(timeline->requestClipMove(cid4, tid2, 4));

        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipTrackId(cid2) == tid1);
        REQUIRE(timeline->getClipTrackId(cid3) == tid1);
        REQUIRE(timeline->getClipTrackId(cid4) == tid2);
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

        auto state = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipTrackId(cid4) == tid2);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == length + 3);
            REQUIRE(timeline->getClipPosition(cid3) == 2*length + 5);
            REQUIRE(timeline->getClipPosition(cid4) == 4);
        };
        state();

        //grouping
        REQUIRE(timeline->requestClipsGroup({cid1, cid3}));
        REQUIRE(timeline->requestClipsGroup({cid1, cid4}));

        //move left is now forbidden, because clip1 is at position 0
        REQUIRE_FALSE(timeline->requestClipMove(cid3, tid1, 2*length + 3));
        state();

        //this move is impossible, because clip1 runs into clip2
        REQUIRE_FALSE(timeline->requestClipMove(cid4, tid2, 9));
        state();

        //this move is possible
        REQUIRE(timeline->requestClipMove(cid3, tid1, 2*length + 8));
        auto state1 = [&](){
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 3);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid3) == 0);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipTrackId(cid4) == tid2);
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
            REQUIRE(timeline->getClipTrackId(cid1) == tid2);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipTrackId(cid3) == tid2);
            REQUIRE(timeline->getClipTrackId(cid4) == tid3);
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
        REQUIRE(timeline->requestClipsGroup({cid1, cid2}));
        auto state = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getTrackClipsCount(tid2) == 1);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid2);
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
        REQUIRE(timeline->requestClipsGroup({cid1, cid2}));
        auto state = [&](int t) {
            REQUIRE(timeline->getTrackById(t)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(t) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == t);
            REQUIRE(timeline->getClipTrackId(cid2) == t);
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
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);

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
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);

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
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);

        // REQUIRE(timeline->allowClipMove(cid1, tid1, 0));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 0));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(undoStack->index() == init_index + 2);

        undoStack->undo();
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);

        undoStack->redo();
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 0);
        REQUIRE(undoStack->index() == init_index + 2);

        undoStack->undo();
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);

        // REQUIRE(timeline->allowClipMove(cid1, tid1, 2*length));
        REQUIRE(timeline->requestClipMove(cid1, tid1, 2*length));
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 2*length);
        REQUIRE(undoStack->index() == init_index + 2);

        undoStack->undo();
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 5);
        REQUIRE(undoStack->index() == init_index + 1);

        undoStack->redo();
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        REQUIRE(timeline->getClipTrackId(cid1) == tid1);
        REQUIRE(timeline->getClipPosition(cid1) == 2*length);
        REQUIRE(undoStack->index() == init_index + 2);

        undoStack->undo();
        undoStack->undo();
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
        REQUIRE(timeline->getClipTrackId(cid1) == -1);
        REQUIRE(undoStack->index() == init_index);
    }

    SECTION("Basic resize orphan clip undo") {
        REQUIRE(timeline->getClipPlaytime(cid2) == length);

        REQUIRE(timeline->requestClipResize(cid2, length - 5, true));
        REQUIRE(undoStack->index() == init_index + 1);
        REQUIRE(timeline->getClipPlaytime(cid2) ==  length - 5);

        REQUIRE(timeline->requestClipResize(cid2, length - 10, false));
        REQUIRE(undoStack->index() == init_index + 2);
        REQUIRE(timeline->getClipPlaytime(cid2) ==  length - 10);

        REQUIRE_FALSE(timeline->requestClipResize(cid2, length, false));
        REQUIRE(undoStack->index() == init_index + 2);
        REQUIRE(timeline->getClipPlaytime(cid2) ==  length - 10);

        undoStack->undo();
        REQUIRE(undoStack->index() == init_index + 1);
        REQUIRE(timeline->getClipPlaytime(cid2) ==  length - 5);

        undoStack->redo();
        REQUIRE(undoStack->index() == init_index + 2);
        REQUIRE(timeline->getClipPlaytime(cid2) ==  length - 10);

        undoStack->undo();
        REQUIRE(undoStack->index() == init_index + 1);
        REQUIRE(timeline->getClipPlaytime(cid2) ==  length - 5);

        undoStack->undo();
        REQUIRE(undoStack->index() == init_index);
        REQUIRE(timeline->getClipPlaytime(cid2) ==  length);
    }
    SECTION("Basic resize inserted clip undo") {
        REQUIRE(timeline->getClipPlaytime(cid2) == length);

        auto check = [&](int pos, int l) {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipPlaytime(cid2) == l);
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
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(undoStack->index() == init_index + 1);
        };
        state1();

        int cid3;
        REQUIRE_FALSE(timeline->requestClipInsertion(prod, tid1, 5, cid3));
        state1();

        REQUIRE_FALSE(timeline->requestClipInsertion(prod, tid1, 6, cid3));
        state1();

        REQUIRE(timeline->requestClipInsertion(prod, tid1, 5 + length, cid3));
        auto state2 = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getClipPosition(cid3) == 5 + length);
            REQUIRE(timeline->m_allClips[cid3]->isValid());
            REQUIRE(undoStack->index() == init_index + 2);
        };
        state2();

        REQUIRE(timeline->requestClipMove(cid3, tid1, 10 + length));
        auto state3 = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getClipPosition(cid3) == 10 + length);
            REQUIRE(undoStack->index() == init_index + 3);
        };
        state3();

        REQUIRE(timeline->requestClipResize(cid3, 1, true));
        auto state4 = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid3) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(timeline->getClipPlaytime(cid3) == 1);
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


    SECTION("Clip Deletion undo") {
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        auto state1 = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(undoStack->index() == init_index + 1);
        };
        state1();

        int nbClips = timeline->getClipsCount();
        REQUIRE(timeline->requestItemDeletion(cid1));
        auto state2 = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
            REQUIRE(timeline->getClipsCount() == nbClips - 1);
            REQUIRE(undoStack->index() == init_index + 2);
        };
        state2();

        undoStack->undo();
        state1();

        undoStack->redo();
        state2();

        undoStack->undo();
        state1();
    }

    SECTION("Track insertion undo") {
        int nb_tracks = timeline->getTracksCount();
        std::map<int, int> orig_trackPositions, final_trackPositions;
        for (const auto& it : timeline->m_iteratorTable) {
            int track = it.first;
            int pos = timeline->getTrackPosition(track);
            orig_trackPositions[track] = pos;
            if (pos >= 1) pos++;
            final_trackPositions[track] = pos;
        }
        auto checkPositions = [&](const std::map<int,int>& pos) {
            for (const auto& p : pos ) {
                REQUIRE(timeline->getTrackPosition(p.first) == p.second);
            }
        };
        checkPositions(orig_trackPositions);
        int new_tid;
        REQUIRE(timeline->requestTrackInsertion(1, new_tid));
        checkPositions(final_trackPositions);

        undoStack->undo();
        checkPositions(orig_trackPositions);

        undoStack->redo();
        checkPositions(final_trackPositions);

        undoStack->undo();
        checkPositions(orig_trackPositions);
    }


    SECTION("Track deletion undo") {
        int nb_clips = timeline->getClipsCount();
        int nb_tracks = timeline->getTracksCount();
        REQUIRE(timeline->requestClipMove(cid1, tid1, 5));
        auto state1 = [&]() {
            REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 5);
            REQUIRE(undoStack->index() == init_index + 1);
            REQUIRE(timeline->getClipsCount() == nb_clips);
            REQUIRE(timeline->getTracksCount() == nb_tracks);
        };
        state1();

        REQUIRE(timeline->requestTrackDeletion(tid1));
        REQUIRE(timeline->getClipsCount() == nb_clips - 1);
        REQUIRE(timeline->getTracksCount() == nb_tracks - 1);

        undoStack->undo();
        state1();

        undoStack->redo();
        REQUIRE(timeline->getClipsCount() == nb_clips - 1);
        REQUIRE(timeline->getTracksCount() == nb_tracks - 1);

        undoStack->undo();
        state1();
    }
}

TEST_CASE("Snapping", "[Snapping]") {
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);


    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile_model, "color", "red");
    std::shared_ptr<Mlt::Producer> producer2 = std::make_shared<Mlt::Producer>(profile_model, "color", "blue");
    producer->set("length", 20);
    producer->set("out", 19);
    producer2->set("length", 20);
    producer2->set("out", 19);

    REQUIRE(producer->is_valid());
    REQUIRE(producer2->is_valid());
    int tid1 = TrackModel::construct(timeline);
    int cid1 = ClipModel::construct(timeline, producer);
    int tid2 = TrackModel::construct(timeline);
    int cid2 = ClipModel::construct(timeline, producer2);

    int length = timeline->getClipPlaytime(cid1);
    SECTION("getBlankSizeNearClip") {
        REQUIRE(timeline->requestClipMove(cid1,tid1,0));

        //before
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, false) == 0);
        //after
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, true) == INT_MAX);
        REQUIRE(timeline->requestClipMove(cid1,tid1,10));
        //before
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, false) == 10);
        //after
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, true) == INT_MAX);
        REQUIRE(timeline->requestClipMove(cid2,tid1,25 + length));
        //before
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, false) == 10);
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid2, false) == 15);
        //after
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, true) == 15);
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid2, true) == INT_MAX);

        REQUIRE(timeline->requestClipMove(cid2,tid1,10 + length));
        //before
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, false) == 10);
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid2, false) == 0);
        //after
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid1, true) == 0);
        REQUIRE(timeline->getTrackById(tid1)->getBlankSizeNearClip(cid2, true) == INT_MAX);
    }
}
