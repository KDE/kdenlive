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
#include <mlt++/MltFactory.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltRepository.h>

TEST_CASE("Regression") {
    Mlt::Profile profile;
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(undoStack);
    TimelineModel::next_id = 0;
    undoStack->undo();
    undoStack->redo();
    undoStack->redo();
    undoStack->undo();
    std::shared_ptr<Mlt::Producer> producer0 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer0 ->set("length", 20);
    producer0 ->set("out", 19);
    ClipModel::construct(timeline, producer0 );
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->requestClipMove(0,1 ,0 ));
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->requestClipResize(0,16 ,false ));
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE_FALSE(timeline->requestClipResize(0,0 ,false ));
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    TrackModel::construct(timeline);
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(1)->checkConsistency());
}

TEST_CASE("Regression2") {
    Mlt::Profile profile;
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineModel> timeline = TimelineItemModel::construct(undoStack);
    TimelineModel::next_id = 0;
    int dummy_id;
    undoStack->undo();
    undoStack->undo();
    undoStack->redo();
    TrackModel::construct(timeline); 
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    {
        std::shared_ptr<Mlt::Producer> producer0 = std::make_shared<Mlt::Producer>(profile, "color", "red");
        producer0->set("length", 20);
        producer0->set("out", 19);
        bool ok = timeline->requestClipInsertion(producer0,0 ,10, dummy_id );
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    TrackModel::construct(timeline); 
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    {
        std::shared_ptr<Mlt::Producer> producer1 = std::make_shared<Mlt::Producer>(profile, "color", "red");
        producer1->set("length", 20);
        producer1->set("out", 19);
        bool ok = timeline->requestClipInsertion(producer1,2 ,10, dummy_id );
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(1,0 ,10 ); 
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    {
        bool ok = timeline->requestClipResize(3,0 ,false );
        REQUIRE_FALSE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    TrackModel::construct(timeline); 
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    std::shared_ptr<Mlt::Producer> producer2 = std::make_shared<Mlt::Producer>(profile, "color", "red");
    producer2->set("length", 20);
    producer2->set("out", 19);
    ClipModel::construct(timeline, producer2 );
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    TrackModel::construct(timeline); 
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        bool ok = timeline->requestClipResize(3,15 ,true );
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        bool ok = timeline->requestClipMove(3,0 ,0 ); 
        REQUIRE_FALSE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        bool ok = timeline->requestClipResize(3,16 ,false );
        REQUIRE_FALSE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        bool ok = timeline->requestClipResize(3,16 ,true );
        REQUIRE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    {
        std::shared_ptr<Mlt::Producer> producer3 = std::make_shared<Mlt::Producer>(profile, "color", "red");
        producer3->set("length", 20);
        producer3->set("out", 19);
        bool ok = timeline->requestClipInsertion(producer3,0 ,1, dummy_id );
        REQUIRE_FALSE(ok);
    }
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->undo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->redo();
    REQUIRE(timeline->getTrackById(0)->checkConsistency());
    REQUIRE(timeline->getTrackById(2)->checkConsistency());
    REQUIRE(timeline->getTrackById(4)->checkConsistency());
    REQUIRE(timeline->getTrackById(6)->checkConsistency());
    undoStack->redo();
}
