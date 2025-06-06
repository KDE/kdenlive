/*
    SPDX-FileCopyrightText: 2018-2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2017-2019 Nicolas Carion <french.ebook.lover@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "test_utils.hpp"
// test specific headers
#include "bin/model/markerlistmodel.hpp"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "timeline2/model/snapmodel.hpp"

using Marker = std::tuple<GenTime, QString, int>;
double fps;

void checkMarkerList(const std::shared_ptr<MarkerListModel> &model, const std::vector<Marker> &l, const std::shared_ptr<SnapModel> &snaps)
{
    auto list = l;
    // std::sort(list.begin(), list.end(), [](const Marker &a, const Marker &b) { return std::get<0>(a) < std::get<0>(b); });

    REQUIRE(model->rowCount() == (int)list.size());
    if (model->rowCount() == 0) {
        REQUIRE(snaps->getClosestPoint(0) == -1);
    }
    for (int i = 0; i < model->rowCount(); ++i) {
        Marker m;
        // Model markers and List do not necessarily use the same order
        for (size_t j = 0; j < list.size(); j++) {
            if (qAbs(std::get<0>(list[j]).seconds() - model->data(model->index(i), MarkerListModel::PosRole).toDouble()) < 0.9 / fps) {
                m = list[j];
                list.erase(std::remove(list.begin(), list.end(), m), list.end());
                break;
            }
        }
        REQUIRE(qAbs(std::get<0>(m).seconds() - model->data(model->index(i), MarkerListModel::PosRole).toDouble()) < 0.9 / fps);
        REQUIRE(std::get<1>(m) == model->data(model->index(i), MarkerListModel::CommentRole).toString());
        REQUIRE(std::get<2>(m) == model->data(model->index(i), MarkerListModel::TypeRole).toInt());
        REQUIRE(pCore->markerTypes.value(std::get<2>(m)).color == model->data(model->index(i), MarkerListModel::ColorRole).value<QColor>());

        // check for marker existence
        int frame = std::get<0>(m).frames(fps);
        REQUIRE(model->hasMarker(frame));

        // cheap way to check for snap
        REQUIRE(snaps->getClosestPoint(frame) == frame);
    }
}

void checkStates(const std::shared_ptr<DocUndoStack> &undoStack, const std::shared_ptr<MarkerListModel> &model, const std::vector<std::vector<Marker>> &states,
                 const std::shared_ptr<SnapModel> &snaps)
{
    for (size_t i = 0; i < states.size(); ++i) {
        checkMarkerList(model, states[states.size() - 1 - i], snaps);
        if (i != states.size() - 1) {
            undoStack->undo();
        }
    }
    for (size_t i = 1; i < states.size(); ++i) {
        undoStack->redo();
        checkMarkerList(model, states[i], snaps);
    }
}

TEST_CASE("Marker model", "[MarkerListModel]")
{
    auto binModel = pCore->projectItemModel();
    fps = pCore->getCurrentFps();
    GenTime::setFps(fps);
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveDoc document(undoStack);
    pCore->projectManager()->testSetDocument(&document);
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    QDateTime documentDate = QDateTime::currentDateTime();
    KdenliveTests::updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->testSetActiveTimeline(timeline);

    std::shared_ptr<MarkerListModel> model = timeline->getGuideModel();
    std::shared_ptr<SnapModel> snaps = std::make_shared<SnapModel>();
    model->registerSnapModel(snaps);

    SECTION("Basic Manipulation")
    {
        std::vector<Marker> list;
        checkMarkerList(model, list, snaps);

        // add markers
        list.emplace_back(GenTime(1.3), QLatin1String("test marker"), 3);
        REQUIRE(model->addMarker(GenTime(1.3), QLatin1String("test marker"), 3));
        checkMarkerList(model, list, snaps);
        auto state1 = list;

        checkStates(undoStack, model, {{}, state1}, snaps);

        list.emplace_back(GenTime(0.3), QLatin1String("test marker2"), 0);
        REQUIRE(model->addMarker(GenTime(0.3), QLatin1String("test marker2"), 0));
        checkMarkerList(model, list, snaps);
        auto state2 = list;

        checkStates(undoStack, model, {{}, state1, state2}, snaps);

        // delete unexisting marker shouldn't work
        REQUIRE_FALSE(model->removeMarker(GenTime(42.)));
        checkMarkerList(model, list, snaps);
        checkStates(undoStack, model, {{}, state1, state2}, snaps);

        // rename markers
        std::get<1>(list[0]) = QLatin1String("new comment");
        std::get<2>(list[0]) = 1;
        REQUIRE(model->addMarker(GenTime(1.3), QLatin1String("new comment"), 1));
        checkMarkerList(model, list, snaps);
        auto state3 = list;
        checkStates(undoStack, model, {{}, state1, state2, state3}, snaps);

        // edit marker
        GenTime oldPos = std::get<0>(list[1]);
        std::get<0>(list[1]) = GenTime(42.8);
        std::get<1>(list[1]) = QLatin1String("edited comment");
        std::get<2>(list[1]) = 3;
        REQUIRE(model->editMarker(oldPos, GenTime(42.8), QLatin1String("edited comment"), 3));
        checkMarkerList(model, list, snaps);
        auto state4 = list;
        checkStates(undoStack, model, {{}, state1, state2, state3, state4}, snaps);

        // delete markers
        std::swap(list[0], list[1]);
        list.pop_back();
        REQUIRE(model->removeMarker(GenTime(1.3)));
        checkMarkerList(model, list, snaps);
        auto state5 = list;
        checkStates(undoStack, model, {{}, state1, state2, state3, state4, state5}, snaps);
        GenTime old = std::get<0>(list.back());
        list.pop_back();
        REQUIRE(model->removeMarker(old));
        checkMarkerList(model, list, snaps);
        auto state6 = list;
        checkStates(undoStack, model, {{}, state1, state2, state3, state4, state5, state6}, snaps);

        // add some back
        list.emplace_back(GenTime(1.7), QLatin1String("test marker6"), KdenliveSettings::default_marker_type());
        REQUIRE(model->addMarker(GenTime(1.7), QLatin1String("test marker6"), -1));
        auto state7 = list;
        list.emplace_back(GenTime(2), QLatin1String("auieuansr"), 3);
        REQUIRE(model->addMarker(GenTime(2), QLatin1String("auieuansr"), 3));
        auto state8 = list;
        list.emplace_back(GenTime(0), QLatin1String("sasenust"), 1);
        REQUIRE(model->addMarker(GenTime(0), QLatin1String("sasenust"), 1));
        checkMarkerList(model, list, snaps);
        auto state9 = list;
        checkStates(undoStack, model, {{}, state1, state2, state3, state4, state5, state6, state7, state8, state9}, snaps);

        // try spurious model registration
        // std::shared_ptr<SnapInterface> spurious;
        // REQUIRE(ABORTS(&MarkerListModel::registerSnapModel, model, spurious));

        // try real model registration
        std::shared_ptr<SnapModel> other_snaps = std::make_shared<SnapModel>();
        model->registerSnapModel(other_snaps);
        checkMarkerList(model, list, other_snaps);

        // remove all
        REQUIRE(model->removeAllMarkers());
        checkMarkerList(model, {}, snaps);
        checkStates(undoStack, model, {{}, state1, state2, state3, state4, state5, state6, state7, state8, state9, {}}, snaps);
    }

    SECTION("Test Categories")
    {
        std::vector<Marker> list;
        checkMarkerList(model, list, snaps);

        // add markers
        list.emplace_back(GenTime(1.3), QLatin1String("test marker"), 3);
        REQUIRE(model->addMarker(GenTime(1.3), QLatin1String("test marker"), 3));
        REQUIRE(model->addMarker(GenTime(0.3), QLatin1String("test marker2"), 0));

        QStringList categories = model->categoriesToStringList();
        // We have 9 marker categories by default
        Q_ASSERT(categories.count() == 9);

        QStringList newCategories = categories;
        newCategories.removeFirst();

        REQUIRE(model->rowCount() == int(snaps->_snaps().size()));
        REQUIRE(model->rowCount() == 2);
        model->loadCategoriesWithUndo(newCategories, categories);
        REQUIRE(model->rowCount() == int(snaps->_snaps().size()));
        REQUIRE(model->rowCount() == 1);

        // Reset to default categories
        undoStack->undo();
        REQUIRE(model->rowCount() == int(snaps->_snaps().size()));
        REQUIRE(model->rowCount() == 2);

        REQUIRE(model->removeAllMarkers());
        checkMarkerList(model, {}, snaps);
    }

    SECTION("Json identity test")
    {
        std::vector<Marker> list;
        checkMarkerList(model, list, snaps);

        // add markers
        list.emplace_back(GenTime(1.3), QLatin1String("test marker"), 3);
        model->addMarker(GenTime(1.3), QLatin1String("test marker"), 3);
        list.emplace_back(GenTime(0.3), QLatin1String("test marker2"), 0);
        model->addMarker(GenTime(0.3), QLatin1String("test marker2"), 0);
        list.emplace_back(GenTime(3), QLatin1String("test marker3"), 0);
        model->addMarker(GenTime(3), QLatin1String("test marker3"), 0);
        checkMarkerList(model, list, snaps);

        // export
        QString json = model->toJson();

        // clean
        model->removeMarker(GenTime(0.3));
        model->removeMarker(GenTime(3));
        model->removeMarker(GenTime(1.3));
        checkMarkerList(model, {}, snaps);

        // Reimport
        REQUIRE(model->importFromJson(json, false));
        checkMarkerList(model, list, snaps);

        // undo/redo
        undoStack->undo();
        checkMarkerList(model, {}, snaps);
        undoStack->redo();
        checkMarkerList(model, list, snaps);

        // now we try the same thing with non-empty model
        undoStack->undo();
        checkMarkerList(model, {}, snaps);
        // non - conflicting marker
        list.emplace_back(GenTime(5), QLatin1String("non conflicting"), 0);
        std::vector<Marker> otherMarkers;
        otherMarkers.emplace_back(GenTime(5), QLatin1String("non conflicting"), 0);
        model->addMarker(GenTime(5), QLatin1String("non conflicting"), 0);
        REQUIRE(model->importFromJson(json, false));
        checkMarkerList(model, list, snaps);
        undoStack->undo();
        checkMarkerList(model, otherMarkers, snaps);
        undoStack->redo();
        checkMarkerList(model, list, snaps);
        undoStack->undo();

        // conflicting marker
        otherMarkers.emplace_back(GenTime(1.3), QLatin1String("conflicting"), 1);
        model->addMarker(GenTime(1.3), QLatin1String("conflicting"), 1);
        checkMarkerList(model, otherMarkers, snaps);
        REQUIRE_FALSE(model->importFromJson(json, false));
        checkMarkerList(model, otherMarkers, snaps);
        REQUIRE(model->importFromJson(json, true));
        checkMarkerList(model, list, snaps);
        undoStack->undo();
        checkMarkerList(model, otherMarkers, snaps);
        undoStack->redo();
        checkMarkerList(model, list, snaps);
    }

    SECTION("Duration-based markers")
    {
        std::vector<Marker> list;
        checkMarkerList(model, list, snaps);

        REQUIRE(model->addRangeMarker(GenTime(1.0), GenTime(2.0), QLatin1String("range marker 1"), 1));
        REQUIRE(model->addRangeMarker(GenTime(5.0), GenTime(1.5), QLatin1String("range marker 2"), 2));

        bool exists;
        auto marker1 = model->getMarker(GenTime(1.0), &exists);
        REQUIRE(exists);
        REQUIRE(marker1.hasRange());
        REQUIRE(marker1.duration().seconds() == 2.0);
        REQUIRE(marker1.endTime().seconds() == 3.0);
        REQUIRE(marker1.comment() == QLatin1String("range marker 1"));
        REQUIRE(marker1.markerType() == 1);

        auto marker2 = model->getMarker(GenTime(5.0), &exists);
        REQUIRE(exists);
        REQUIRE(marker2.hasRange());
        REQUIRE(marker2.duration().seconds() == 1.5);
        REQUIRE(marker2.endTime().seconds() == 6.5);

        // Test point marker (duration = 0)
        REQUIRE(model->addMarker(GenTime(10.0), QLatin1String("point marker"), 0));
        auto pointMarker = model->getMarker(GenTime(10.0), &exists);
        REQUIRE(exists);
        REQUIRE_FALSE(pointMarker.hasRange());
        REQUIRE(pointMarker.duration().seconds() == 0.0);
        REQUIRE(pointMarker.endTime().seconds() == 10.0);

        // Test converting point marker to range marker
        REQUIRE(model->addRangeMarker(GenTime(10.0), GenTime(3.0), QLatin1String("converted to range"), 0));
        auto convertedMarker = model->getMarker(GenTime(10.0), &exists);
        REQUIRE(exists);
        REQUIRE(convertedMarker.hasRange());
        REQUIRE(convertedMarker.duration().seconds() == 3.0);
        REQUIRE(convertedMarker.comment() == QLatin1String("converted to range"));

        // Test converting range marker to point marker
        REQUIRE(model->addMarker(GenTime(1.0), QLatin1String("converted to point"), 1));
        auto convertedToPoint = model->getMarker(GenTime(1.0), &exists);
        REQUIRE(exists);
        REQUIRE_FALSE(convertedToPoint.hasRange());
        REQUIRE(convertedToPoint.duration().seconds() == 0.0);
        REQUIRE(convertedToPoint.comment() == QLatin1String("converted to point"));

        // Test editing markers with duration
        REQUIRE(model->editMarker(GenTime(5.0), GenTime(5.0), QLatin1String("edited range"), 2, GenTime(2.5)));
        auto editedMarker = model->getMarker(GenTime(5.0), &exists);
        REQUIRE(exists);
        REQUIRE(editedMarker.hasRange());
        REQUIRE(editedMarker.duration().seconds() == 2.5);
        REQUIRE(editedMarker.comment() == QLatin1String("edited range"));

        // Test model data roles for range markers
        int rowCount = model->rowCount();
        bool foundRangeMarker = false;
        for (int i = 0; i < rowCount; ++i) {
            QModelIndex idx = model->index(i);
            double duration = model->data(idx, MarkerListModel::DurationRole).toDouble();
            if (duration > 0) {
                foundRangeMarker = true;
                double pos = model->data(idx, MarkerListModel::PosRole).toDouble();
                double endPos = model->data(idx, MarkerListModel::EndPosRole).toDouble();
                bool hasRange = model->data(idx, MarkerListModel::HasRangeRole).toBool();

                REQUIRE(hasRange);
                REQUIRE(endPos == pos + duration);
                REQUIRE(duration > 0);
            }
        }
        REQUIRE(foundRangeMarker);

        // Test JSON export/import with duration
        QString json = model->toJson();

        REQUIRE(json.contains(QLatin1String("duration")));

        REQUIRE(model->removeAllMarkers());
        checkMarkerList(model, {}, snaps);

        REQUIRE(model->importFromJson(json, false));

        // Verify imported markers have correct duration
        auto reimportedMarker = model->getMarker(GenTime(5.0), &exists);
        REQUIRE(exists);
        REQUIRE(reimportedMarker.hasRange());
        REQUIRE(qAbs(reimportedMarker.duration().seconds() - 2.5) < 0.1);

        // Test backward compatibility - import JSON without duration field
        QString legacyJson = R"([{"pos":120,"comment":"legacy marker","type":1}])";
        REQUIRE(model->removeAllMarkers());
        REQUIRE(model->importFromJson(legacyJson, false));

        auto legacyMarker = model->getMarker(GenTime(120, fps), &exists);
        REQUIRE(exists);
        REQUIRE_FALSE(legacyMarker.hasRange());
        REQUIRE(legacyMarker.duration().seconds() == 0.0);
        REQUIRE(legacyMarker.comment() == QLatin1String("legacy marker"));

        // Clean up
        REQUIRE(model->removeAllMarkers());
        checkMarkerList(model, {}, snaps);
    }
    snaps.reset();
    // undoStack->clear();
    pCore->projectManager()->closeCurrentDocument(false, false);
}
