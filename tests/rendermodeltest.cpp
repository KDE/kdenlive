/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "test_utils.hpp"
// test specific headers
#include "doc/kdenlivedoc.h"
#include "render/renderrequest.h"
#include "renderpresets/renderpresetmodel.hpp"
#include "renderpresets/renderpresetrepository.hpp"

TEST_CASE("Basic tests of the render preset model", "[RenderPresets]")
{

    KdenliveTests::initRenderRepository();

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

        CHECK(model->params().toString().length() == QStringLiteral("one=first two=second value three=third four=4").length());
        CHECK(model->params().size() == 4);
        CHECK(model->params().contains("one"));
        CHECK(model->params().contains("two"));
        CHECK(model->params().contains("three"));
        CHECK(model->params().contains("four"));

        // test removing a normal parameter in the middle
        CHECK(model->params({QStringLiteral("three")}).toString().length() == QStringLiteral("one=first two=second value four=4").length());
        CHECK(model->params({QStringLiteral("three")}).size() == 3);
        CHECK_FALSE(model->params({QStringLiteral("three")}).contains("three"));

        // test removing a parameter at the first position
        CHECK(model->params({QStringLiteral("one")}).toString().length() == QStringLiteral("two=second value three=third four=4").length());
        CHECK(model->params({QStringLiteral("one")}).size() == 3);
        CHECK_FALSE(model->params({QStringLiteral("one")}).contains("one"));

        // test removing a parameter at the last position
        CHECK(model->params({QStringLiteral("four")}).toString().length() == QStringLiteral("one=first two=second value three=third").length());
        CHECK(model->params({QStringLiteral("four")}).size() == 3);
        CHECK_FALSE(model->params({QStringLiteral("four")}).contains("four"));

        // test removing a non-existing parameter
        CHECK(model->params({QStringLiteral("fifth")}).toString().length() == QStringLiteral("one=first two=second value three=third four=4").length());
        CHECK(model->params({QStringLiteral("fifth")}).size() == 4);
        CHECK_FALSE(model->params({QStringLiteral("fifth")}).contains("fifth"));

        // test removing a handing a value should do nothing
        CHECK(model->params({QStringLiteral("first")}).toString().length() == QStringLiteral("one=first two=second value three=third four=4").length());
        CHECK(model->params({QStringLiteral("first")}).size() == 4);
        CHECK_FALSE(model->params({QStringLiteral("first")}).contains("first"));

        // test removing multiple parameters
        CHECK(model->params({QStringLiteral("one"), QStringLiteral("three")}).toString().length() == QStringLiteral("two=second value four=4").length());
        CHECK(model->params({QStringLiteral("one"), QStringLiteral("three")}).size() == 2);
        CHECK_FALSE(model->params({QStringLiteral("one"), QStringLiteral("three")}).contains("one"));
        CHECK_FALSE(model->params({QStringLiteral("one"), QStringLiteral("three")}).contains("three"));

        // test removing a parameter with space in value
        CHECK(model->params({QStringLiteral("two")}).toString().length() == QStringLiteral("one=first three=third four=4").length());
        CHECK(model->params({QStringLiteral("two")}).size() == 3);
        CHECK_FALSE(model->params({QStringLiteral("two")}).contains("two"));
    }

    SECTION("Test profile with no parameters")
    {
        std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(QStringLiteral("EmptyParamTest"), QStringLiteral("Test"), QString(), QString(),
                                                                       QString(), QString(), QString(), QString(), QString(), false));

        // check there are really no parameters
        CHECK(model->params().toString().length() == 0);
        CHECK(model->params().size() == 0);
    }

    SECTION("Test unknown rate control")
    {
        std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(QStringLiteral("UnknownRate"), QStringLiteral("Test"), QString(), QStringLiteral("mp4"),
                                                                       QString(), QString(), QString(), QString(), QString(), false));
        CHECK(model->params().videoRateControl() == RenderPresetParams::RateControl::Unknown);
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

    SECTION("Test alpha detection placeholders")
    {
        std::unique_ptr<RenderPresetModel> model(new RenderPresetModel(QStringLiteral("AlphaTest"), QStringLiteral("Test"), QStringLiteral(""), QString(),
                                                                       QString(), QString(), QString(), QString(), QString(), false));
        CHECK_FALSE(model->params().hasAlpha());

        model.reset(new RenderPresetModel(QStringLiteral("AlphaTest"), QStringLiteral("Test"), QStringLiteral("pix_fmt=rgba"), QString(), QString(), QString(),
                                          QString(), QString(), QString(), false));
        CHECK(model->params().hasAlpha());
    }

    SECTION("Test x265 param")
    {
        std::unique_ptr<RenderPresetModel> model(
            new RenderPresetModel(QStringLiteral("AlphaTest"), QStringLiteral("Test"),
                                  QStringLiteral("properties=x265-medium f=mp4 vcodec=libx265 crf=%quality acodec=aac ab=%audiobitrate+'k'"), QString(),
                                  QString(), QString(), QString(), QString(), QString(), false));

        RenderPresetParams params = model->params();

        // we did not calulate the x265 params yet, so we expect to not have them
        CHECK_FALSE(params.contains(QStringLiteral("x265-params")));

        params.refreshX265Params();
        // now that we calculated them we expect them to exist
        CHECK(params.contains(QStringLiteral("x265-params")));

        params.insert(QStringLiteral("vcodec"), QStringLiteral("libx264"));
        params.refreshX265Params();
        // for a non-x265 codec they should not be there
        CHECK_FALSE(params.contains(QStringLiteral("x265-params")));

        params.insert(QStringLiteral("vcodec"), QStringLiteral("libx265"));
        params.refreshX265Params();
        // we set x265 codec again so the params should be back
        CHECK(params.contains(QStringLiteral("x265-params")));
    }
}

TEST_CASE("Tests of the render functions to use guides for sections", "[RenderRequestGuides]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> markerModel(new MarkerListModel(QString(), undoStack));
    markerModel->loadCategories(KdenliveDoc::getDefaultGuideCategories(), false);

    int guideCategory = 3;

    // create some guides at different positions and in different categories
    markerModel->addMarker(GenTime(35, pCore->getCurrentFps()), QStringLiteral("test marker"), 1);
    markerModel->addMarker(GenTime(70, pCore->getCurrentFps()), QStringLiteral("test marker"), guideCategory);
    markerModel->addMarker(GenTime(25, pCore->getCurrentFps()), QStringLiteral("test marker"), guideCategory);

    // set the bounding range
    int in = 0;
    int out = 100;

    RenderRequest *r = new RenderRequest();
    KdenliveTests::setRenderRequestBounds(r, in, out);

    SECTION("Single Guide")
    {
        // Category 1 contains only one guide and hence should have 2 sections
        r->setGuideParams(markerModel, true, 1);
        CHECK(r->guideSectionsCount() == 2);
    }

    SECTION("No markers at all")
    {
        // Category 0 has no guides and should return no sections at all
        r->setGuideParams(markerModel, true, 0);
        CHECK(r->guideSectionsCount() == 0);
    }

    SECTION("Multiple Guides")
    {
        r->setGuideParams(markerModel, true, guideCategory);
        CHECK(r->guideSectionsCount() == 3);

        // Add one more marker and check this leads to one more section
        markerModel->addMarker(GenTime(51, pCore->getCurrentFps()), QStringLiteral("test marker"), guideCategory);
        CHECK(r->guideSectionsCount() == 4);

        // The size should not change if we add a guide at the start, because there should always be a section from the start.
        // Our guide should only replace the automatically generated section which in our case should only affect the name.
        markerModel->addMarker(GenTime(0, pCore->getCurrentFps()), QStringLiteral("test marker"), guideCategory);
        CHECK(r->guideSectionsCount() == 4);

        // check in and out points
        QVector<std::pair<int, int>> sections = r->getSectionsInOut();
        CHECK(sections.at(0).first == in);
        CHECK(sections.at(0).second == 24);
        CHECK(sections.at(1).first == 25);
        CHECK(sections.at(1).second == 50);
        CHECK(sections.at(2).first == 51);
        CHECK(sections.at(2).second == 69);
        CHECK(sections.at(3).first == 70);
        CHECK(sections.at(3).second == out);

        // check no section name appears twice, eventhough we have guides with the same name
        QStringList names = r->getSectionsNames();
        CHECK(names.removeDuplicates() == 0);

        // change the bounding range
        in = 10;
        out = 60;
        KdenliveTests::setRenderRequestBounds(r, in, out);

        CHECK(r->guideSectionsCount() == 3);
        sections = r->getSectionsInOut();
        // check in and out points
        CHECK(sections.at(0).first == in);
        CHECK(sections.at(0).second == 24);
        CHECK(sections.at(1).first == 25);
        CHECK(sections.at(1).second == 50);
        CHECK(sections.at(2).first == 51);
        CHECK(sections.at(2).second == out);
    }
}
