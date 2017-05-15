#include "catch.hpp"

#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic push
#include "fakeit.hpp"
#include <QColor>
#include <QDebug>
#include <QString>
#include <cmath>
#include <iostream>
#include <tuple>
#include <unordered_set>

#define private public
#define protected public
#include "bin/model/markerlistmodel.hpp"
#include "core.h"
#include "doc/docundostack.hpp"
#include "gentime.h"
#include "project/projectmanager.h"
#include "timeline2/model/snapmodel.hpp"

using namespace fakeit;

using Marker = std::tuple<GenTime, QString, int>;
double fps;

void checkMarkerList(std::shared_ptr<MarkerListModel> model, const std::vector<Marker> &l, std::shared_ptr<SnapModel> snaps)
{
    auto list = l;
    std::sort(list.begin(), list.end(), [](const Marker &a, const Marker &b) { return std::get<0>(a) < std::get<0>(b); });

    REQUIRE(model->rowCount() == (int)list.size());
    if (model->rowCount() == 0) {
        REQUIRE(snaps->getClosestPoint(0) == -1);
    }
    for (int i = 0; i < model->rowCount(); ++i) {
        REQUIRE(qAbs(std::get<0>(list[i]).seconds() - model->data(model->index(i), MarkerListModel::PosRole).toDouble()) < 1e-4);
        REQUIRE(std::get<1>(list[i]) == model->data(model->index(i), MarkerListModel::CommentRole).toString());
        REQUIRE(std::get<2>(list[i]) == model->data(model->index(i), MarkerListModel::TypeRole).toInt());
        REQUIRE(MarkerListModel::markerTypes[std::get<2>(list[i])] == model->data(model->index(i), MarkerListModel::ColorRole).value<QColor>());

        // check for marker existance
        int frame = std::get<0>(list[i]).frames(fps);
        REQUIRE(model->hasMarker(frame));

        // cheap way to check for snap
        REQUIRE(snaps->getClosestPoint(frame) == frame);
    }
}

void checkStates(std::shared_ptr<DocUndoStack> undoStack, std::shared_ptr<MarkerListModel> model, const std::vector<std::vector<Marker>> &states,
                 std::shared_ptr<SnapModel> snaps)
{
    for (size_t i = 0; i < states.size(); ++i) {
        checkMarkerList(model, states[states.size() - 1 - i], snaps);
        if (i < states.size() - 1) {
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
    fps = pCore->getCurrentFps();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    std::shared_ptr<MarkerListModel> model = std::make_shared<MarkerListModel>(undoStack, nullptr);

    std::shared_ptr<SnapModel> snaps = std::make_shared<SnapModel>();
    model->registerSnapModel(snaps);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the getGuideModel function returns this model

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, getGuideModel)).AlwaysReturn(model);

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    SECTION("Basic Manipulation")
    {
        std::vector<Marker> list;
        checkMarkerList(model, list, snaps);

        // add markers
        list.push_back(Marker(GenTime(1.3), QLatin1String("test marker"), 3));
        model->addMarker(GenTime(1.3), QLatin1String("test marker"), 3);
        checkMarkerList(model, list, snaps);
        auto state1 = list;

        checkStates(undoStack, model, {{}, state1}, snaps);

        list.push_back(Marker(GenTime(0.3), QLatin1String("test marker2"), 0));
        model->addMarker(GenTime(0.3), QLatin1String("test marker2"), 0);
        checkMarkerList(model, list, snaps);
        auto state2 = list;

        checkStates(undoStack, model, {{}, state1, state2}, snaps);

        // rename markers
        std::get<1>(list[0]) = QLatin1String("new comment");
        std::get<2>(list[0]) = 1;
        model->addMarker(GenTime(1.3), QLatin1String("new comment"), 1);
        checkMarkerList(model, list, snaps);
        auto state3 = list;
        checkStates(undoStack, model, {{}, state1, state2, state3}, snaps);

        // delete markers
        std::swap(list[0], list[1]);
        list.pop_back();
        model->removeMarker(GenTime(1.3));
        checkMarkerList(model, list, snaps);
        auto state4 = list;
        checkStates(undoStack, model, {{}, state1, state2, state3, state4}, snaps);

        list.pop_back();
        model->removeMarker(GenTime(0.3));
        checkMarkerList(model, list, snaps);
        auto state5 = list;
        checkStates(undoStack, model, {{}, state1, state2, state3, state4, state5}, snaps);
    }

    SECTION("Json identity test")
    {
        std::vector<Marker> list;
        checkMarkerList(model, list, snaps);

        // add markers
        list.push_back(Marker(GenTime(1.3), QLatin1String("test marker"), 3));
        model->addMarker(GenTime(1.3), QLatin1String("test marker"), 3);
        list.push_back(Marker(GenTime(0.3), QLatin1String("test marker2"), 0));
        model->addMarker(GenTime(0.3), QLatin1String("test marker2"), 0);
        list.push_back(Marker(GenTime(3), QLatin1String("test marker3"), 0));
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
        list.push_back(Marker(GenTime(5), QLatin1String("non conflicting"), 0));
        std::vector<Marker> otherMarkers;
        otherMarkers.push_back(Marker(GenTime(5), QLatin1String("non conflicting"), 0));
        model->addMarker(GenTime(5), QLatin1String("non conflicting"), 0);
        REQUIRE(model->importFromJson(json, false));
        checkMarkerList(model, list, snaps);
        undoStack->undo();
        checkMarkerList(model, otherMarkers, snaps);
        undoStack->redo();
        checkMarkerList(model, list, snaps);
        undoStack->undo();

        // conflicting marker
        otherMarkers.push_back(Marker(GenTime(1.3), QLatin1String("conflicting"), 1));
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
}
