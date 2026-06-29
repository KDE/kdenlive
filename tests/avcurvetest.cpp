/*
    SPDX-FileCopyrightText: 2026 Yash Bavadiya <krbavadiya11@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"
// test specific headers
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"

#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "effects/effectsrepository.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"

TEST_CASE("AvCurve per-channel", "[AvCurve]")
{
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    KdenliveDoc document(undoStack);
    pCore->projectManager()->testSetDocument(&document);
    QDateTime documentDate = QDateTime::currentDateTime();
    KdenliveTests::updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->testSetActiveTimeline(timeline);

    int tid1;
    REQUIRE(timeline->requestTrackInsertion(-1, tid1));

    QString binId = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel);
    int cid1;
    REQUIRE(timeline->requestClipInsertion(binId, tid1, 100, cid1));

    // Load avfilter_curves.xml so tests use the Kdenlive XML definition (not MLT fallback metadata).
    // sourcesPath points to the tests/ dir; the XML lives one level up in data/effects/avfilter/.
    EffectsRepository::get()->reloadCustom(QFileInfo(sourcesPath + QStringLiteral("/../data/effects/avfilter/avfilter_curves.xml")).absoluteFilePath());

    // avfilter.curves must be available
    REQUIRE(EffectsRepository::get()->exists(QStringLiteral("avfilter.curves")));

    auto clipModel = timeline->getClipEffectStackModel(cid1);
    REQUIRE(clipModel->appendEffect(QStringLiteral("avfilter.curves")));

    std::shared_ptr<AssetParameterModel> model = clipModel->getAssetModelById(QStringLiteral("avfilter.curves"));
    REQUIRE(model != nullptr);

    SECTION("Effect has four av_curve parameters")
    {
        // av.master, av.r, av.g, av.b
        REQUIRE(model->getParamIndexFromName(QStringLiteral("av.master")).isValid());
        REQUIRE(model->getParamIndexFromName(QStringLiteral("av.r")).isValid());
        REQUIRE(model->getParamIndexFromName(QStringLiteral("av.g")).isValid());
        REQUIRE(model->getParamIndexFromName(QStringLiteral("av.b")).isValid());

        QModelIndex ix = model->getParamIndexFromName(QStringLiteral("av.master"));
        REQUIRE(model->data(ix, AssetParameterModel::TypeRole).value<ParamType>() == ParamType::AvCurve);
    }

    SECTION("KisCubicCurve format converts to avfilter space-separated format")
    {
        QModelIndex ix = model->getParamIndexFromName(QStringLiteral("av.master"));
        // set a simple 3-point curve in KisCubicCurve format
        model->setParameter(QStringLiteral("av.master"), QStringLiteral("0/0;0.5/0.75;1/1"), true, ix);

        // the stored value should retain the KisCubicCurve format
        const QString stored = model->data(ix, AssetParameterModel::ValueRole).toString();
        REQUIRE(stored == QStringLiteral("0/0;0.5/0.75;1/1"));

        // the MLT asset should have received space-separated avfilter format
        const QString mltVal = model->getParam(QStringLiteral("av.master"));
        REQUIRE(mltVal == QStringLiteral("0/0 0.5/0.75 1/1"));
    }

    SECTION("Points too close on x-axis are snapped, not rejected")
    {
        QModelIndex ix = model->getParamIndexFromName(QStringLiteral("av.master"));
        // 0.0001 is far closer than the ~0.00266 avfilter minimum; it should be snapped
        model->setParameter(QStringLiteral("av.master"), QStringLiteral("0/0;0.0001/0.5;1/1"), true, ix);

        // the curve must still have been sent to MLT (not silently dropped)
        const QString mltVal = model->getParam(QStringLiteral("av.master"));
        REQUIRE(!mltVal.isEmpty());

        // the snapped x must be >= 0.004 (safe margin above avfilter threshold 1/255)
        const QStringList pts = mltVal.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        REQUIRE(pts.size() == 3);
        const double snappedX = pts.at(1).section(QLatin1Char('/'), 0, 0).toDouble();
        REQUIRE(snappedX >= 0.004);
    }

    SECTION("Per-channel independence: setting av.r does not affect av.g")
    {
        QModelIndex ixR = model->getParamIndexFromName(QStringLiteral("av.r"));
        QModelIndex ixG = model->getParamIndexFromName(QStringLiteral("av.g"));

        const QString gBefore = model->data(ixG, AssetParameterModel::ValueRole).toString();

        model->setParameter(QStringLiteral("av.r"), QStringLiteral("0/0;0.3/0.6;1/1"), true, ixR);

        const QString rAfter = model->data(ixR, AssetParameterModel::ValueRole).toString();
        const QString gAfter = model->data(ixG, AssetParameterModel::ValueRole).toString();

        REQUIRE(rAfter == QStringLiteral("0/0;0.3/0.6;1/1"));
        REQUIRE(gAfter == gBefore);
    }

    SECTION("Default empty curve serializes to empty MLT value")
    {
        QModelIndex ix = model->getParamIndexFromName(QStringLiteral("av.b"));
        // default value in XML is ""
        const QString stored = model->data(ix, AssetParameterModel::ValueRole).toString();
        REQUIRE(stored.isEmpty());
    }

    timeline.reset();
    pCore->projectManager()->closeCurrentDocument(false, false);
}
