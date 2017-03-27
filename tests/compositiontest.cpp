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
#include "transitions/transitionsrepository.hpp"

QString aCompo;
TEST_CASE("Basic creation/deletion of a composition", "[CompositionModel]")
{
    // Check whether repo works
    QVector<QPair<QString, QString> > transitions = TransitionsRepository::get()->getNames();
    REQUIRE(!transitions.isEmpty());

    // Look for a compo
    for (const auto &trans : transitions) {
        if (TransitionsRepository::get()->isComposition(trans.first)) {
            aCompo = trans.first;
            break;
        }
    }

    REQUIRE(!aCompo.isEmpty());

    // Check construction from repo
    std::unique_ptr<Mlt::Transition> mlt_transition(TransitionsRepository::get()->getTransition(aCompo));

    REQUIRE(mlt_transition->is_valid());

    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);

    REQUIRE(timeline->getCompositionsCount() == 0);
    int id1 = CompositionModel::construct(timeline, aCompo);
    REQUIRE(timeline->getCompositionsCount() == 1);

    int id2 = CompositionModel::construct(timeline, aCompo);
    REQUIRE(timeline->getCompositionsCount() == 2);

    int id3 = CompositionModel::construct(timeline, aCompo);
    REQUIRE(timeline->getCompositionsCount() == 3);

    // Test deletion
    REQUIRE(timeline->requestItemDeletion(id2));
    REQUIRE(timeline->getCompositionsCount() == 2);
    REQUIRE(timeline->requestItemDeletion(id3));
    REQUIRE(timeline->getCompositionsCount() == 1);
    REQUIRE(timeline->requestItemDeletion(id1));
    REQUIRE(timeline->getCompositionsCount() == 0);
}

TEST_CASE("Composition manipulation", "[CompositionModel]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<TimelineItemModel> timeline = TimelineItemModel::construct(new Mlt::Profile(), undoStack);

    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);
    int tid3 = TrackModel::construct(timeline);

    SECTION("Insert a composition in a track and change track") {
        REQUIRE(timeline->getTrackById(tid1)->checkConsistency());
        REQUIRE(timeline->getTrackById(tid2)->checkConsistency());
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 0);
        REQUIRE(timeline->getTrackCompositionsCount(tid2) == 0);

        int cid1 = CompositionModel::construct(timeline, aCompo);
        REQUIRE(timeline->getCompositionTrackId(cid1) == -1);
        REQUIRE(timeline->getCompositionPosition(cid1) == -1);

        int pos = 10;
        //real insert
        // // REQUIRE(timeline->allowClipMove(cid1, tid1, pos));
        REQUIRE(timeline->requestCompositionMove(cid1, tid1, pos));
        REQUIRE(timeline->getCompositionTrackId(cid1) == tid1);
        REQUIRE(timeline->getCompositionPosition(cid1) == pos);
        REQUIRE(timeline->getTrackCompositionsCount(tid1) == 1);
        REQUIRE(timeline->getTrackCompositionsCount(tid2) == 0);
    }
    /*
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
    */
}

