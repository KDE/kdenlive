/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"
// test specific includes
#include "assets/keyframes/model/dopesheetmodel.hpp"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include <QApplication>
#include <memory>

using namespace fakeit;

bool test_model_equality(const std::shared_ptr<KeyframeModel> &m1, const std::shared_ptr<KeyframeModel> &m2)
{
    // we cheat a bit by simply comparing the underlying map
    QList<QVariant> model1 = m1->testSerializeKeyframes();
    QList<QVariant> model2 = m2->testSerializeKeyframes();
    return model1 == model2;
}

bool check_anim_identity(const std::shared_ptr<KeyframeModel> &m)
{
    auto m2 = KdenliveTests::cloneModel(m);
    return test_model_equality(m, m2);
}

TEST_CASE("Dopesheet model", "[DopesheetModel]")
{
    // Create timeline
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack
    KdenliveDoc document(undoStack);

    pCore->projectManager()->testSetDocument(&document);
    QDateTime documentDate = QDateTime::currentDateTime();
    KdenliveTests::updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->testSetActiveTimeline(timeline);

    const QString binId = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel, 100, false);
    std::shared_ptr<ProjectClip> clip = binModel->getClipByBinID(binId);
    auto effectstack = clip->getEffectStack();

    effectstack->appendEffect(QStringLiteral("brightness"));
    REQUIRE(effectstack->checkConsistency());
    REQUIRE(effectstack->rowCount() == 1);
    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectstack->getEffectStackRow(0));
    effect->prepareKeyframes();

    // Load model into dopesheet
    pCore->dopeSheetModel()->registerStack(effectstack, 0);

    REQUIRE(effect->rowCount() == 1);

    const std::map<int, std::pair<DopeSheetModel::EffectParamInfo, std::shared_ptr<KeyframeModel>>> pInfo =
        KdenliveTests::getParamInfo(pCore->dopeSheetModel());
    std::shared_ptr<KeyframeModel> keyframeModel;
    std::shared_ptr<KeyframeModel> recapModel;
    QPersistentModelIndex recapIndex;
    for (auto &p : pInfo) {
        if (!p.second.first.mltId.isEmpty()) {
            keyframeModel = p.second.second;
        } else {
            // recap
            recapModel = p.second.second;
            recapIndex = pCore->dopeSheetModel()->getIndexFromId(p.first);
        }
    }
    Q_ASSERT(keyframeModel);
    Q_ASSERT(recapModel);

    SECTION("Add/remove + undo")
    {
        auto state0 = [&]() {
            // There is a timer when syncing master recap
            qApp->processEvents();
            REQUIRE(keyframeModel->rowCount() == 1);
            REQUIRE(recapModel->rowCount() == 1);
            REQUIRE(check_anim_identity(keyframeModel));
        };
        state0();

        REQUIRE(KdenliveTests::addKeyframe(keyframeModel, GenTime(28, pCore->getCurrentFps()), KeyframeType::Linear, 42));
        auto state1 = [&]() {
            // There is a timer when syncing master recap
            qApp->processEvents();
            REQUIRE(keyframeModel->rowCount() == 2);
            REQUIRE(recapModel->rowCount() == 2);
            REQUIRE(check_anim_identity(keyframeModel));
            REQUIRE(keyframeModel->hasKeyframe(GenTime(28, pCore->getCurrentFps())));
            REQUIRE(recapModel->hasKeyframe(GenTime(28, pCore->getCurrentFps())));
            bool ok;
            auto k = keyframeModel->getKeyframe(GenTime(28, pCore->getCurrentFps()), &ok);
            REQUIRE(ok);
            auto k0 = keyframeModel->getKeyframe(GenTime(0), &ok);
            REQUIRE(ok);
            auto k1 = keyframeModel->getClosestKeyframe(GenTime(0.655555), &ok);
            REQUIRE(ok);
            REQUIRE(k1 == k);
            auto k2 = keyframeModel->getNextKeyframe(GenTime(0.5), &ok);
            REQUIRE(ok);
            REQUIRE(k2 == k);
            auto k3 = keyframeModel->getPrevKeyframe(GenTime(0.5), &ok);
            REQUIRE(ok);
            REQUIRE(k3 == k0);
            auto k4 = keyframeModel->getPrevKeyframe(GenTime(10), &ok);
            REQUIRE(ok);
            REQUIRE(k4 == k);
            auto k5 = recapModel->getPrevKeyframe(GenTime(10), &ok);
            REQUIRE(ok);
            REQUIRE(k5 == k);
            keyframeModel->getNextKeyframe(GenTime(10), &ok);
            REQUIRE_FALSE(ok);
            recapModel->getNextKeyframe(GenTime(10), &ok);
            REQUIRE_FALSE(ok);
        };
        state1();

        undoStack->undo();
        state0();
        // Now test adding keyframe to recap
        REQUIRE(pCore->dopeSheetModel()->addKeyframe(recapIndex, GenTime(28, pCore->getCurrentFps()).frames(pCore->getCurrentFps())));
        state1();

        undoStack->undo();
        state0();

        undoStack->redo();
        state1();

        REQUIRE(KdenliveTests::addKeyframe(keyframeModel, GenTime(12.6), KeyframeType::Discrete, 33));
        auto state2 = [&]() {
            // There is a timer when syncing master recap
            qApp->processEvents();
            REQUIRE(keyframeModel->rowCount() == 3);
            REQUIRE(recapModel->rowCount() == 3);
            REQUIRE(check_anim_identity(keyframeModel));
            REQUIRE(keyframeModel->hasKeyframe(GenTime(28, pCore->getCurrentFps())));
            REQUIRE(keyframeModel->hasKeyframe(GenTime(12.6)));
            REQUIRE(recapModel->hasKeyframe(GenTime(28, pCore->getCurrentFps())));
            REQUIRE(recapModel->hasKeyframe(GenTime(12.6)));
            bool ok;
            auto k = keyframeModel->getKeyframe(GenTime(28, pCore->getCurrentFps()), &ok);
            REQUIRE(ok);
            recapModel->getKeyframe(GenTime(28, pCore->getCurrentFps()), &ok);
            REQUIRE(ok);
            auto k0 = keyframeModel->getKeyframe(GenTime(0), &ok);
            REQUIRE(ok);
            recapModel->getKeyframe(GenTime(0), &ok);
            REQUIRE(ok);
            auto kk = keyframeModel->getKeyframe(GenTime(12.6), &ok);
            REQUIRE(ok);
            recapModel->getKeyframe(GenTime(12.6), &ok);
            REQUIRE(ok);
            auto k1 = keyframeModel->getClosestKeyframe(GenTime(0.655555), &ok);
            REQUIRE(ok);
            REQUIRE(k1 == k);
            auto k2 = keyframeModel->getNextKeyframe(GenTime(0.5), &ok);
            REQUIRE(ok);
            REQUIRE(k2 == k);
            auto k3 = keyframeModel->getPrevKeyframe(GenTime(0.5), &ok);
            REQUIRE(ok);
            REQUIRE(k3 == k0);
            auto k4 = keyframeModel->getPrevKeyframe(GenTime(10), &ok);
            REQUIRE(ok);
            REQUIRE(k4 == k);
            auto k5 = keyframeModel->getNextKeyframe(GenTime(10), &ok);
            REQUIRE(ok);
            REQUIRE(k5 == kk);
        };
        state2();

        // Now test removing keyframe from recap
        REQUIRE(pCore->dopeSheetModel()->removeKeyframe(recapIndex, GenTime(12.6).frames(pCore->getCurrentFps())));
        state1();
        undoStack->undo();
        state2();

        undoStack->undo();
        state1();
        undoStack->undo();
        state0();
        undoStack->redo();
        state1();
        undoStack->redo();
        state2();

        REQUIRE(KdenliveTests::removeKeyframe(keyframeModel, GenTime(28, pCore->getCurrentFps())));
        auto state3 = [&]() {
            // There is a timer when syncing master recap
            qApp->processEvents();
            REQUIRE(keyframeModel->rowCount() == 2);
            REQUIRE(check_anim_identity(keyframeModel));
            REQUIRE(keyframeModel->hasKeyframe(GenTime(12.6)));
            bool ok;
            keyframeModel->getKeyframe(GenTime(28, pCore->getCurrentFps()), &ok);
            REQUIRE_FALSE(ok);
            auto k0 = keyframeModel->getKeyframe(GenTime(0), &ok);
            REQUIRE(ok);
            auto kk = keyframeModel->getKeyframe(GenTime(12.6), &ok);
            REQUIRE(ok);
            auto k1 = keyframeModel->getClosestKeyframe(GenTime(0.655555), &ok);
            REQUIRE(ok);
            REQUIRE(k1 == k0);
            auto k2 = keyframeModel->getNextKeyframe(GenTime(0.5), &ok);
            REQUIRE(ok);
            REQUIRE(k2 == kk);
            auto k3 = keyframeModel->getPrevKeyframe(GenTime(0.5), &ok);
            REQUIRE(ok);
            REQUIRE(k3 == k0);
            auto k4 = keyframeModel->getPrevKeyframe(GenTime(10), &ok);
            REQUIRE(ok);
            REQUIRE(k4 == k0);
            auto k5 = keyframeModel->getNextKeyframe(GenTime(10), &ok);
            REQUIRE(ok);
            REQUIRE(k5 == kk);
        };

        state3();

        undoStack->undo();
        state2();
        undoStack->undo();
        state1();
        undoStack->undo();
        state0();
        undoStack->redo();
        state1();
        undoStack->redo();
        state2();
        undoStack->redo();
        state3();

        REQUIRE(KdenliveTests::removeAllKeyframes(keyframeModel));
        state0();
        REQUIRE(KdenliveTests::removeAllKeyframes(keyframeModel));
        state0();
        undoStack->undo();
        state0();
        undoStack->undo();
        state3();
        undoStack->redo();
        state0();
    }

    SECTION("Move keyframes + undo")
    {
        auto state0 = [&]() {
            // There is a timer when syncing master recap
            qApp->processEvents();
            REQUIRE(keyframeModel->rowCount() == 1);
            REQUIRE(recapModel->rowCount() == 1);
            REQUIRE(check_anim_identity(keyframeModel));
        };
        state0();

        REQUIRE(KdenliveTests::addKeyframe(keyframeModel, GenTime(28, pCore->getCurrentFps()), KeyframeType::Linear, 42));
        auto state1 = [&](double pos) {
            // There is a timer when syncing master recap
            qApp->processEvents();
            REQUIRE(keyframeModel->rowCount() == 2);
            REQUIRE(recapModel->rowCount() == 2);
            REQUIRE(check_anim_identity(keyframeModel));
            REQUIRE(recapModel->hasKeyframe(GenTime(pos)));
            REQUIRE(keyframeModel->hasKeyframe(GenTime(pos)));
            bool ok;
            auto k = keyframeModel->getKeyframe(GenTime(pos), &ok);
            REQUIRE(ok);
            recapModel->getKeyframe(GenTime(pos), &ok);
            REQUIRE(ok);
            auto k0 = keyframeModel->getKeyframe(GenTime(0), &ok);
            REQUIRE(ok);
            auto k1 = keyframeModel->getClosestKeyframe(GenTime(pos + 10), &ok);
            REQUIRE(ok);
            REQUIRE(k1 == k);
            recapModel->getClosestKeyframe(GenTime(pos + 10), &ok);
            REQUIRE(ok);
            auto k2 = keyframeModel->getNextKeyframe(GenTime(pos - 0.3), &ok);
            REQUIRE(ok);
            recapModel->getNextKeyframe(GenTime(pos - 0.3), &ok);
            REQUIRE(ok);
            REQUIRE(k2 == k);
            auto k3 = keyframeModel->getPrevKeyframe(GenTime(pos - 0.3), &ok);
            REQUIRE(ok);
            recapModel->getPrevKeyframe(GenTime(pos - 0.3), &ok);
            REQUIRE(ok);
            REQUIRE(k3 == k0);
            auto k4 = keyframeModel->getPrevKeyframe(GenTime(pos + 0.3), &ok);
            REQUIRE(ok);
            recapModel->getPrevKeyframe(GenTime(pos + 0.3), &ok);
            REQUIRE(ok);
            REQUIRE(k4 == k);
            keyframeModel->getNextKeyframe(GenTime(pos + 0.3), &ok);
            REQUIRE_FALSE(ok);
            recapModel->getNextKeyframe(GenTime(pos + 0.3), &ok);
            REQUIRE_FALSE(ok);
        };
        state1(GenTime(28, pCore->getCurrentFps()).seconds());

        REQUIRE(keyframeModel->moveKeyframe(GenTime(28, pCore->getCurrentFps()), GenTime(2.6), -1, true));
        state1(2.6);

        undoStack->undo();
        state1(GenTime(28, pCore->getCurrentFps()).seconds());
        undoStack->redo();
        state1(2.6);

        REQUIRE(keyframeModel->moveKeyframe(GenTime(2.6), GenTime(6.1), -1, true));
        state1(6.1);

        undoStack->undo();
        state1(2.6);
        undoStack->undo();
        state1(GenTime(28, pCore->getCurrentFps()).seconds());
        undoStack->redo();
        state1(2.6);
        undoStack->redo();
        state1(6.1);

        REQUIRE(KdenliveTests::addKeyframe(keyframeModel, GenTime(12.6), KeyframeType::Discrete, 33));
        // Moving a keyframe past another one another will move it 1 frame before or after.
        REQUIRE(keyframeModel->moveKeyframe(GenTime(6.1), GenTime(14), -1, true));
        bool ok;
        // There should be no keyframe after 12.6
        keyframeModel->getNextKeyframe(GenTime(12.6), &ok);
        REQUIRE_FALSE(ok);
        recapModel->getNextKeyframe(GenTime(12.6), &ok);
        REQUIRE_FALSE(ok);
        undoStack->undo();
        undoStack->undo();
        state1(6.1);
    }
    clip.reset();
    timeline.reset();
    pCore->projectManager()->closeCurrentDocument(false, false);
}
