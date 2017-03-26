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


TEST_CASE("Basic creation/deletion of a composition", "[CompositionModel]")
{
    std::unique_ptr<Mlt::Transition> mlt_transition(TransitionsRepository::get()->getTransition(QStringLiteral("wipe")));

    REQUIRE(mlt_transition->is_valid());
}
