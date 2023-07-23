/*
    SPDX-FileCopyrightText: 2018-2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2017-2019 Nicolas Carion <french.ebook.lover@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "test_utils.hpp"
// test specific includes
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include <memory>

using namespace fakeit;

bool test_model_equality(const std::shared_ptr<KeyframeModel> &m1, const std::shared_ptr<KeyframeModel> &m2)
{
    // we cheat a bit by simply comparing the underlying map
    qDebug() << "Equality test" << m1->m_keyframeList.size() << m2->m_keyframeList.size();
    QList<QVariant> model1;
    QList<QVariant> model2;
    for (const auto &m : m1->m_keyframeList) {
        model1 << m.first.frames(25) << (int)m.second.first << m.second.second;
    }
    for (const auto &m : m2->m_keyframeList) {
        model2 << m.first.frames(25) << (int)m.second.first << m.second.second;
    }
    return model1 == model2;
}

bool check_anim_identity(const std::shared_ptr<KeyframeModel> &m)
{
    auto m2 = std::make_shared<KeyframeModel>(m->m_model, m->m_index, m->m_undoStack);
    m2->parseAnimProperty(m->getAnimProperty());
    return test_model_equality(m, m2);
}

TEST_CASE("Keyframe model", "[KeyframeModel]")
{
    // Create timeline
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack
    KdenliveDoc document(undoStack);

    pCore->projectManager()->m_project = &document;
    QDateTime documentDate = QDateTime::currentDateTime();
    pCore->projectManager()->updateTimeline(0, false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->m_activeTimelineModel = timeline;
    pCore->projectManager()->testSetActiveDocument(&document, timeline);

    const QString binId = createProducer(pCore->getProjectProfile(), "red", binModel, 100, false);
    std::shared_ptr<ProjectClip> clip = binModel->getClipByBinID(binId);
    auto effectstack = clip->m_effectStack;

    effectstack->appendEffect(QStringLiteral("audiobalance"));
    REQUIRE(effectstack->checkConsistency());
    REQUIRE(effectstack->rowCount() == 1);
    auto effect = std::dynamic_pointer_cast<EffectItemModel>(effectstack->getEffectStackRow(0));
    effect->prepareKeyframes();
    qDebug() << effect->getAssetId() << effect->getAllParameters();

    REQUIRE(effect->rowCount() == 1);
    QModelIndex index = effect->index(0, 0);

    auto model = std::make_shared<KeyframeModel>(effect, index, undoStack);

    SECTION("Add/remove + undo")
    {
        auto state0 = [&]() {
            REQUIRE(model->rowCount() == 1);
            REQUIRE(check_anim_identity(model));
        };
        state0();

        REQUIRE(model->addKeyframe(GenTime(1.1), KeyframeType::Linear, 42));
        auto state1 = [&]() {
            REQUIRE(model->rowCount() == 2);
            REQUIRE(check_anim_identity(model));
            REQUIRE(model->hasKeyframe(GenTime(1.1)));
            bool ok;
            auto k = model->getKeyframe(GenTime(1.1), &ok);
            REQUIRE(ok);
            auto k0 = model->getKeyframe(GenTime(0), &ok);
            REQUIRE(ok);
            auto k1 = model->getClosestKeyframe(GenTime(0.655555), &ok);
            REQUIRE(ok);
            REQUIRE(k1 == k);
            auto k2 = model->getNextKeyframe(GenTime(0.5), &ok);
            REQUIRE(ok);
            REQUIRE(k2 == k);
            auto k3 = model->getPrevKeyframe(GenTime(0.5), &ok);
            REQUIRE(ok);
            REQUIRE(k3 == k0);
            auto k4 = model->getPrevKeyframe(GenTime(10), &ok);
            REQUIRE(ok);
            REQUIRE(k4 == k);
            model->getNextKeyframe(GenTime(10), &ok);
            REQUIRE_FALSE(ok);
        };
        state1();

        undoStack->undo();
        state0();
        undoStack->redo();
        state1();

        REQUIRE(model->addKeyframe(GenTime(12.6), KeyframeType::Discrete, 33));
        auto state2 = [&]() {
            REQUIRE(model->rowCount() == 3);
            REQUIRE(check_anim_identity(model));
            REQUIRE(model->hasKeyframe(GenTime(1.1)));
            REQUIRE(model->hasKeyframe(GenTime(12.6)));
            bool ok;
            auto k = model->getKeyframe(GenTime(1.1), &ok);
            REQUIRE(ok);
            auto k0 = model->getKeyframe(GenTime(0), &ok);
            REQUIRE(ok);
            auto kk = model->getKeyframe(GenTime(12.6), &ok);
            REQUIRE(ok);
            auto k1 = model->getClosestKeyframe(GenTime(0.655555), &ok);
            REQUIRE(ok);
            REQUIRE(k1 == k);
            auto k2 = model->getNextKeyframe(GenTime(0.5), &ok);
            REQUIRE(ok);
            REQUIRE(k2 == k);
            auto k3 = model->getPrevKeyframe(GenTime(0.5), &ok);
            REQUIRE(ok);
            REQUIRE(k3 == k0);
            auto k4 = model->getPrevKeyframe(GenTime(10), &ok);
            REQUIRE(ok);
            REQUIRE(k4 == k);
            auto k5 = model->getNextKeyframe(GenTime(10), &ok);
            REQUIRE(ok);
            REQUIRE(k5 == kk);
        };
        state2();

        undoStack->undo();
        state1();
        undoStack->undo();
        state0();
        undoStack->redo();
        state1();
        undoStack->redo();
        state2();

        REQUIRE(model->removeKeyframe(GenTime(1.1)));
        auto state3 = [&]() {
            REQUIRE(model->rowCount() == 2);
            REQUIRE(check_anim_identity(model));
            REQUIRE(model->hasKeyframe(GenTime(12.6)));
            bool ok;
            model->getKeyframe(GenTime(1.1), &ok);
            REQUIRE_FALSE(ok);
            auto k0 = model->getKeyframe(GenTime(0), &ok);
            REQUIRE(ok);
            auto kk = model->getKeyframe(GenTime(12.6), &ok);
            REQUIRE(ok);
            auto k1 = model->getClosestKeyframe(GenTime(0.655555), &ok);
            REQUIRE(ok);
            REQUIRE(k1 == k0);
            auto k2 = model->getNextKeyframe(GenTime(0.5), &ok);
            REQUIRE(ok);
            REQUIRE(k2 == kk);
            auto k3 = model->getPrevKeyframe(GenTime(0.5), &ok);
            REQUIRE(ok);
            REQUIRE(k3 == k0);
            auto k4 = model->getPrevKeyframe(GenTime(10), &ok);
            REQUIRE(ok);
            REQUIRE(k4 == k0);
            auto k5 = model->getNextKeyframe(GenTime(10), &ok);
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

        REQUIRE(model->removeAllKeyframes());
        state0();
        REQUIRE(model->removeAllKeyframes());
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
            REQUIRE(model->rowCount() == 1);
            REQUIRE(check_anim_identity(model));
        };
        state0();

        REQUIRE(model->addKeyframe(GenTime(1.1), KeyframeType::Linear, 42));
        auto state1 = [&](double pos) {
            REQUIRE(model->rowCount() == 2);
            REQUIRE(check_anim_identity(model));
            REQUIRE(model->hasKeyframe(GenTime(pos)));
            bool ok;
            auto k = model->getKeyframe(GenTime(pos), &ok);
            REQUIRE(ok);
            auto k0 = model->getKeyframe(GenTime(0), &ok);
            REQUIRE(ok);
            auto k1 = model->getClosestKeyframe(GenTime(pos + 10), &ok);
            REQUIRE(ok);
            REQUIRE(k1 == k);
            auto k2 = model->getNextKeyframe(GenTime(pos - 0.3), &ok);
            REQUIRE(ok);
            REQUIRE(k2 == k);
            auto k3 = model->getPrevKeyframe(GenTime(pos - 0.3), &ok);
            REQUIRE(ok);
            REQUIRE(k3 == k0);
            auto k4 = model->getPrevKeyframe(GenTime(pos + 0.3), &ok);
            REQUIRE(ok);
            REQUIRE(k4 == k);
            model->getNextKeyframe(GenTime(pos + 0.3), &ok);
            REQUIRE_FALSE(ok);
        };
        state1(1.1);

        REQUIRE(model->moveKeyframe(GenTime(1.1), GenTime(2.6), -1, true));
        state1(2.6);

        undoStack->undo();
        state1(1.1);
        undoStack->redo();
        state1(2.6);

        REQUIRE(model->moveKeyframe(GenTime(2.6), GenTime(6.1), -1, true));
        state1(6.1);

        undoStack->undo();
        state1(2.6);
        undoStack->undo();
        state1(1.1);
        undoStack->redo();
        state1(2.6);
        undoStack->redo();
        state1(6.1);

        REQUIRE(model->addKeyframe(GenTime(12.6), KeyframeType::Discrete, 33));
        // Moving a keyframe past another one another will move it 1 frame before or after.
        REQUIRE(model->moveKeyframe(GenTime(6.1), GenTime(14), -1, true));
        bool ok;
        // There should be no keyframe after 12.6
        model->getNextKeyframe(GenTime(12.6), &ok);
        REQUIRE_FALSE(ok);
        undoStack->undo();
        undoStack->undo();
        state1(6.1);
    }
    clip.reset();
    timeline.reset();
    pCore->projectManager()->closeCurrentDocument(false, false);
}
