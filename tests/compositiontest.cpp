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
