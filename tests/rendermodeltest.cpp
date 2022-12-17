/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "renderpresets/renderpresetmodel.hpp"
#include "test_utils.hpp"

TEST_CASE("Basic tests of the render preset model", "[RenderPresets]")
{

    SECTION("Test parameter parsing")
    {
        std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(QStringLiteral("Constant"), QStringLiteral("Test"),
                                                                       QStringLiteral("one=first two=second value three=third four=4"), QStringLiteral("mp4"),
                                                                       QString(), QString(), QString(), QString(), QString(), false));
        // test hasParam
        CHECK(model->hasParam("one"));
        CHECK(model->hasParam("two"));
        CHECK(model->hasParam("three"));
        CHECK(model->hasParam("four"));
        CHECK_FALSE(model->hasParam("fifth"));
        CHECK_FALSE(model->hasParam("first"));
        CHECK_FALSE(model->hasParam("second"));
        CHECK_FALSE(model->hasParam("value"));
        CHECK_FALSE(model->hasParam("third"));
        CHECK_FALSE(model->hasParam("4"));
        CHECK_FALSE(model->hasParam("e"));

        // test getParam
        CHECK(model->getParam("one") == QStringLiteral("first"));
        CHECK(model->getParam("two") == QStringLiteral("second value"));
        CHECK(model->getParam("three") == QStringLiteral("third"));
        CHECK(model->getParam("four") == QStringLiteral("4"));
    }

    SECTION("Test unknown rate control")
    {
        std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(QStringLiteral("Constant"), QStringLiteral("Test"), QStringLiteral(""),
                                                                       QStringLiteral("mp4"), QString(), QString(), QString(), QString(), QString(), false));
        CHECK(model->videoRateControl() == RenderPresetModel::RateControl::Unknown);
    }

    /*SECTION("Test constant rate control")
    {
        std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(QStringLiteral("Constant"), QStringLiteral("Test"), QStringLiteral(""),
    QStringLiteral("mp4"), QString(), QString(), QString(), QString(), QString(), false)); CHECK(model->videoRateControl() ==
    RenderPresetModel::RateControl::Constant);
    }

    SECTION("Test average rate control")
    {
        std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(QStringLiteral("Constant"), QStringLiteral("Test"), QStringLiteral(""),
    QStringLiteral("mp4"), QString(), QString(), QString(), QString(), QString(), false)); CHECK(model->videoRateControl() ==
    RenderPresetModel::RateControl::Average);
    }

    SECTION("Test quality rate control")
    {
        std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(QStringLiteral("Constant"), QStringLiteral("Test"), QStringLiteral(""),
    QStringLiteral("mp4"), QString(), QString(), QString(), QString(), QString(), false)); CHECK(model->videoRateControl() ==
    RenderPresetModel::RateControl::Quality);
    }*/
}
