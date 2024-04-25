/*
    SPDX-FileCopyrightText: 2017-2019 Nicolas Carion <french.ebook.lover@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"
// test specific headers
#include "assets/model/assetcommand.hpp"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include <cmath>
#include <iostream>
#include <tuple>
#include <unordered_set>

#include "core.h"
#include "definitions.h"
#include "effects/effectsrepository.hpp"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"

QString anEffect;
TEST_CASE("Effects groups", "[Effects]")
{
    // Create timeline
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack
    KdenliveDoc document(undoStack);

    pCore->projectManager()->m_project = &document;
    QDateTime documentDate = QDateTime::currentDateTime();
    pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->m_activeTimelineModel = timeline;
    pCore->projectManager()->testSetActiveDocument(&document, timeline);

    // Create a request
    int tid1;
    REQUIRE(timeline->requestTrackInsertion(-1, tid1));

    // Create clip
    QString binId = createProducer(pCore->getProjectProfile(), "red", binModel);
    int cid1;
    int cid2;
    int cid3;
    REQUIRE(timeline->requestClipInsertion(binId, tid1, 100, cid1));
    REQUIRE(timeline->requestClipInsertion(binId, tid1, 120, cid2));
    REQUIRE(timeline->requestClipInsertion(binId, tid1, 140, cid3));
    timeline->requestClipsGroup({cid1, cid2, cid3});
    std::shared_ptr<ProjectClip> clip = binModel->getClipByBinID(binId);
    auto model = clip->m_effectStack;

    REQUIRE(model->checkConsistency());
    REQUIRE(model->rowCount() == 0);

    // Check whether repo works
    QVector<QPair<QString, QString>> effects = EffectsRepository::get()->getNames();
    REQUIRE(!effects.isEmpty());

    anEffect = QStringLiteral("sepia"); // effects.first().first;

    REQUIRE(!anEffect.isEmpty());

    auto state = [&](int count) {
        auto clipModel1 = timeline->getClipPtr(cid1)->m_effectStack;
        REQUIRE(clipModel1->rowCount() == count);
        auto clipModel2 = timeline->getClipPtr(cid2)->m_effectStack;
        REQUIRE(clipModel2->rowCount() == count);
        auto clipModel3 = timeline->getClipPtr(cid3)->m_effectStack;
        REQUIRE(clipModel3->rowCount() == count);
    };
    auto effectState = [&](int cid, const QString &paramName, const QString value) {
        auto clipModel1 = timeline->getClipPtr(cid)->m_effectStack;
        std::shared_ptr<AssetParameterModel> model = clipModel1->getAssetModelById(anEffect);
        REQUIRE(model->getParamFromName(paramName) == value);
    };
    SECTION("Add an effect to grouped clips")
    {
        timeline->addClipEffect(cid1, anEffect);
        state(1);
        undoStack->undo();
        state(0);
        undoStack->redo();
        state(1);
        undoStack->undo();
        state(0);
    }
    SECTION("Add an effect to grouped clips, edit param")
    {
        timeline->addClipEffect(cid1, anEffect);
        state(1);
        KdenliveSettings::setApplyEffectParamsToGroup(true);
        KdenliveSettings::setApplyEffectParamsToGroupWithSameValue(false);
        // Apply aan effect param change that should apply to all 3 clips
        auto clipModel1 = timeline->getClipPtr(cid1)->m_effectStack;
        std::shared_ptr<AssetParameterModel> model = clipModel1->getAssetModelById(anEffect);
        QModelIndex ix = model->getParamIndexFromName(QStringLiteral("u"));
        auto *command = new AssetCommand(model, ix, QStringLiteral("140"));
        pCore->groupAssetCommand(model->getOwnerId(), model->getAssetId(), ix, QString::number(75), QString::number(140), command);
        pCore->pushUndo(command);
        // Check that the param change was done on all effects in the group
        effectState(cid1, QStringLiteral("u"), QStringLiteral("140"));
        effectState(cid2, QStringLiteral("u"), QStringLiteral("140"));
        effectState(cid3, QStringLiteral("u"), QStringLiteral("140"));
        undoStack->undo();
        effectState(cid1, QStringLiteral("u"), QStringLiteral("75"));
        effectState(cid2, QStringLiteral("u"), QStringLiteral("75"));
        effectState(cid3, QStringLiteral("u"), QStringLiteral("75"));
        undoStack->redo();
        effectState(cid1, QStringLiteral("u"), QStringLiteral("140"));
        effectState(cid2, QStringLiteral("u"), QStringLiteral("140"));
        effectState(cid3, QStringLiteral("u"), QStringLiteral("140"));
        KdenliveSettings::setApplyEffectParamsToGroup(false);

        // Modify only effect on cid2
        auto clipModel2 = timeline->getClipPtr(cid2)->m_effectStack;
        std::shared_ptr<AssetParameterModel> model2 = clipModel2->getAssetModelById(anEffect);
        QModelIndex ix2 = model2->getParamIndexFromName(QStringLiteral("u"));
        auto *command2 = new AssetCommand(model2, ix2, QStringLiteral("60"));
        pCore->pushUndo(command2);
        effectState(cid1, QStringLiteral("u"), QStringLiteral("140"));
        effectState(cid2, QStringLiteral("u"), QStringLiteral("60"));
        effectState(cid3, QStringLiteral("u"), QStringLiteral("140"));

        // Now apply command to group again, cid2 should be left untouched
        KdenliveSettings::setApplyEffectParamsToGroup(true);
        KdenliveSettings::setApplyEffectParamsToGroupWithSameValue(true);
        auto *command3 = new AssetCommand(model, ix, QStringLiteral("180"));
        pCore->groupAssetCommand(model->getOwnerId(), model->getAssetId(), ix, QString::number(140), QString::number(180), command3);
        pCore->pushUndo(command3);
        effectState(cid1, QStringLiteral("u"), QStringLiteral("180"));
        effectState(cid2, QStringLiteral("u"), QStringLiteral("60"));
        effectState(cid3, QStringLiteral("u"), QStringLiteral("180"));
    }
    timeline.reset();
    clip.reset();
    pCore->projectManager()->closeCurrentDocument(false, false);
}
