#include "catch.hpp"
#include "doc/docundostack.hpp"

#include <QColor>
#include <QDebug>
#include <QString>
#include <cmath>
#include <iostream>
#include <tuple>
#include <unordered_set>

#include "definitions.h"
#define private public
#define protected public
#include "core.h"
#include "effects/effectsrepository.hpp"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"

Mlt::Profile profile_effects;
QString anEffect;
TEST_CASE("Effects stack", "[Effects]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(profile_effects, "color", "red");
    auto model = EffectStackModel::construct(producer, {ObjectType::TimelineClip, 0}, undoStack);

    REQUIRE(model->checkConsistency());
    REQUIRE(model->rowCount() == 0);

    // Check whether repo works
    QVector<QPair<QString, QString>> effects = EffectsRepository::get()->getNames();
    REQUIRE(!effects.isEmpty());

    anEffect = effects.first().first;

    REQUIRE(!anEffect.isEmpty());

    SECTION("Create and delete effects")
    {
        model->appendEffect(anEffect);
        REQUIRE(model->checkConsistency());
        REQUIRE(model->rowCount() == 1);

        model->appendEffect(anEffect);
        REQUIRE(model->checkConsistency());
        REQUIRE(model->rowCount() == 2);

        undoStack->undo();
        REQUIRE(model->checkConsistency());
        REQUIRE(model->rowCount() == 1);
    }
}
