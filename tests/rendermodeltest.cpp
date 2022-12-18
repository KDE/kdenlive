/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "renderpresets/renderpresetmodel.hpp"
#include "test_utils.hpp"

TEST_CASE("Basic tests of the render preset model", "[RenderPresets]")
{

    SECTION("Test getters")
    {
        std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(QStringLiteral("GettersTest"), QStringLiteral("Test"), QString(), QStringLiteral("mp4"),
                                                                       QString(), QString(), QString(), QString(), QString(), false));
        CHECK(model->name() == QStringLiteral("GettersTest"));
        CHECK(model->groupName() == QStringLiteral("Test"));
        CHECK(model->extension() == QStringLiteral("mp4"));
        // we did not set the format parameter, test if it was automatically
        // set based on the extension
        CHECK(model->getParam("f") == QStringLiteral("mp4"));
    }

    SECTION("Test parameter parsing")
    {
        std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(QStringLiteral("ParamTest"), QStringLiteral("Test"),
                                                                       QStringLiteral("one=first two=second value three=third four=4"), QString(), QString(),
                                                                       QString(), QString(), QString(), QString(), false));
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

        CHECK(model->params().toString().length() == QString("one=first two=second value three=third four=4").length());
        CHECK(model->params().size() == 4);
        CHECK(model->params().contains("one"));
        CHECK(model->params().contains("two"));
        CHECK(model->params().contains("three"));
        CHECK(model->params().contains("four"));

        // test removing a normal parameter in the middle
        CHECK(model->params({QStringLiteral("three")}).toString().length() == QString("one=first two=second value four=4").length());
        CHECK(model->params({QStringLiteral("three")}).size() == 3);
        CHECK_FALSE(model->params({QStringLiteral("three")}).contains("three"));

        // test removing a parameter at the first position
        CHECK(model->params({QStringLiteral("one")}).toString().length() == QString("two=second value three=third four=4").length());
        CHECK(model->params({QStringLiteral("one")}).size() == 3);
        CHECK_FALSE(model->params({QStringLiteral("one")}).contains("one"));

        // test removing a parameter at the last position
        CHECK(model->params({QStringLiteral("four")}).toString().length() == QString("one=first two=second value three=third").length());
        CHECK(model->params({QStringLiteral("four")}).size() == 3);
        CHECK_FALSE(model->params({QStringLiteral("four")}).contains("four"));

        // test removing a non-existing parameter
        CHECK(model->params({QStringLiteral("fifth")}).toString().length() == QString("one=first two=second value three=third four=4").length());
        CHECK(model->params({QStringLiteral("fifth")}).size() == 4);
        CHECK_FALSE(model->params({QStringLiteral("fifth")}).contains("fifth"));

        // test removing a handing a value should do nothing
        CHECK(model->params({QStringLiteral("first")}).toString().length() == QString("one=first two=second value three=third four=4").length());
        CHECK(model->params({QStringLiteral("first")}).size() == 4);
        CHECK_FALSE(model->params({QStringLiteral("first")}).contains("first"));

        // test removing multiple parameters
        CHECK(model->params({QStringLiteral("one"), QStringLiteral("three")}).toString().length() == QString("two=second value four=4").length());
        CHECK(model->params({QStringLiteral("one"), QStringLiteral("three")}).size() == 2);
        CHECK_FALSE(model->params({QStringLiteral("one"), QStringLiteral("three")}).contains("one"));
        CHECK_FALSE(model->params({QStringLiteral("one"), QStringLiteral("three")}).contains("three"));

        // test removing a parameter with space in value
        CHECK(model->params({QStringLiteral("two")}).toString().length() == QString("one=first three=third four=4").length());
        CHECK(model->params({QStringLiteral("two")}).size() == 3);
        CHECK_FALSE(model->params({QStringLiteral("two")}).contains("two"));
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

    SECTION("Test error")
    {
        std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(QStringLiteral("ErrorTest"), QStringLiteral("Test"),
                                                                       QStringLiteral("acodec=doesnotexist"), QStringLiteral("mp4"), QString(), QString(),
                                                                       QString(), QString(), QString(), false));

        // we have a unknown audio codec, the error message should not be empty
        CHECK_FALSE(model->error().isEmpty());
    }

    SECTION("Test replace placeholders")
    {
        std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(QStringLiteral("ParamTest"), QStringLiteral("Test"),
                                                                       QStringLiteral("one=%hello two=this%isaphval value three=%isaph four=4"), QString(),
                                                                       QString(), QString(), QString(), QString(), QString(), false));

        RenderPresetParams params = model->params();
        CHECK_FALSE(params.toString().contains(QStringLiteral("one=world")));
        CHECK_FALSE(params.toString().contains(QStringLiteral("two=thismynewval value")));
        CHECK_FALSE(params.toString().contains(QStringLiteral("three=mynew")));

        params.replacePlaceholder(QStringLiteral("%hello"), QStringLiteral("world"));
        CHECK(params.toString().contains(QStringLiteral("one=world")));

        params.replacePlaceholder(QStringLiteral("%isaph"), QStringLiteral("mynew"));
        CHECK(params.toString().contains(QStringLiteral("two=thismynewval value")));
        CHECK(params.toString().contains(QStringLiteral("three=mynew")));
    }
}
