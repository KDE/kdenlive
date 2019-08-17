#include "catch.hpp"
#include "doc/docundostack.hpp"
#include "test_utils.hpp"

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
    Logger::clear();
    // Create timeline
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    // We also mock timeline object to spy few functions and mock others
    TimelineItemModel tim(&profile_effects, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    Fake(Method(timMock, adjustAssetRange));

    // Create a request
    int tid1;
    REQUIRE(timeline->requestTrackInsertion(-1, tid1));

    // Create clip
    QString binId = createProducer(profile_effects, "red", binModel);
    int cid1;
    REQUIRE(timeline->requestClipInsertion(binId, tid1, 100, cid1));
    std::shared_ptr<ProjectClip> clip = binModel->getClipByBinID(binId);

    auto model = clip->m_effectStack;

    REQUIRE(model->checkConsistency());
    REQUIRE(model->rowCount() == 0);

    // Check whether repo works
    QVector<QPair<QString, QString>> effects = EffectsRepository::get()->getNames();
    REQUIRE(!effects.isEmpty());

    anEffect = QStringLiteral("sepia"); // effects.first().first;

    REQUIRE(!anEffect.isEmpty());

    SECTION("Create and delete effects")
    {
        REQUIRE(model->appendEffect(anEffect));
        REQUIRE(model->checkConsistency());
        REQUIRE(model->rowCount() == 1);

        REQUIRE(model->appendEffect(anEffect));
        REQUIRE(model->checkConsistency());
        REQUIRE(model->rowCount() == 2);

        undoStack->undo();
        REQUIRE(model->checkConsistency());
        REQUIRE(model->rowCount() == 1);
    }

    SECTION("Create cut with fade in")
    {
        auto clipModel = timeline->getClipPtr(cid1)->m_effectStack;
        REQUIRE(clipModel->rowCount() == 0);
        clipModel->appendEffect("fade_from_black");
        REQUIRE(clipModel->checkConsistency());
        REQUIRE(clipModel->rowCount() == 1);

        int l = timeline->getClipPlaytime(cid1);
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid1, 100 + l - 10));
        int splitted = timeline->getClipByPosition(tid1, 100 + l - 9);
        auto splitModel = timeline->getClipPtr(splitted)->m_effectStack;
        REQUIRE(clipModel->rowCount() == 1);
        REQUIRE(splitModel->rowCount() == 0);
    }

    SECTION("Create cut with fade out")
    {
        auto clipModel = timeline->getClipPtr(cid1)->m_effectStack;
        REQUIRE(clipModel->rowCount() == 0);
        clipModel->appendEffect("fade_to_black");
        REQUIRE(clipModel->checkConsistency());
        REQUIRE(clipModel->rowCount() == 1);

        int l = timeline->getClipPlaytime(cid1);
        REQUIRE(TimelineFunctions::requestClipCut(timeline, cid1, 100 + l - 10));
        int splitted = timeline->getClipByPosition(tid1, 100 + l - 9);
        auto splitModel = timeline->getClipPtr(splitted)->m_effectStack;
        REQUIRE(clipModel->rowCount() == 0);
        REQUIRE(splitModel->rowCount() == 1);
    }
    Logger::print_trace();
}
