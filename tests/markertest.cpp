#include "catch.hpp"

#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic push
#include "fakeit.hpp"
#include <QColor>
#include <QString>
#include <QDebug>
#include <cmath>
#include <iostream>
#include <tuple>
#include <unordered_set>

#define private public
#define protected public
#include "bin/model/markerlistmodel.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "core.h"
#include "doc/docundostack.hpp"
#include "gentime.h"
#include "project/projectmanager.h"

using namespace fakeit;


using Marker = std::tuple<GenTime, QString, int>;

void checkMarkerList(std::shared_ptr<MarkerListModel> model, const std::vector<Marker> &l)
{
    auto list = l;
    std::sort(list.begin(), list.end(), [](const Marker& a, const Marker& b){
            return std::get<0>(a) < std::get<0>(b);
        });

    REQUIRE(model->rowCount() == (int) list.size());
    for (int i = 0; i < model->rowCount() ; ++i ) {
        REQUIRE(qAbs(std::get<0>(list[i]).seconds() - model->data(model->index(i), MarkerListModel::PosRole).toDouble()) < 1e-4);
        REQUIRE(std::get<1>(list[i]) == model->data(model->index(i), MarkerListModel::CommentRole).toString());
        REQUIRE(std::get<2>(list[i]) == model->data(model->index(i), MarkerListModel::TypeRole).toInt());
        REQUIRE(MarkerListModel::markerTypes[std::get<2>(list[i])] == model->data(model->index(i), MarkerListModel::ColorRole).value<QColor>());
    }
}

void checkStates(std::shared_ptr<DocUndoStack> undoStack, std::shared_ptr<MarkerListModel> model, const std::vector<std::vector<Marker>> &states)
{
    for (size_t i = 0; i < states.size(); ++i) {
        checkMarkerList(model, states[states.size() - 1 - i]);
        if (i < states.size() - 1) {
            undoStack->undo();
        }
    }
    for (size_t i = 1; i < states.size(); ++i) {
        undoStack->redo();
        checkMarkerList(model, states[i]);
    }
}

TEST_CASE("Marker model", "[MarkerListModel]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    std::shared_ptr<MarkerListModel> model = std::make_shared<MarkerListModel>(undoStack, nullptr);


    // Here we do some trickery to enable testing.
    // We mock the project class so that the getGuideModel function returns this model

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, getGuideModel)).AlwaysReturn(model);

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    SECTION("Basic Manipulation") {
        std::vector<Marker> list;
        checkMarkerList(model, list);

        // add markers
        list.push_back(Marker(1.3, QLatin1String("test marker"), 3));
        model->addMarker(GenTime(1.3), QLatin1String("test marker"), 3);
        checkMarkerList(model, list);
        auto state1 = list;

        checkStates(undoStack, model, {{}, state1});

        list.push_back(Marker(0.3, QLatin1String("test marker2"), 0));
        model->addMarker(GenTime(0.3), QLatin1String("test marker2"), 0);
        checkMarkerList(model, list);
        auto state2 = list;

        checkStates(undoStack, model, {{}, state1, state2});

        // rename markers
        std::get<1>(list[0]) = QLatin1String("new comment");
        std::get<2>(list[0]) = 1;
        model->addMarker(GenTime(1.3), QLatin1String("new comment"), 1);
        checkMarkerList(model, list);
        auto state3 = list;
        checkStates(undoStack, model, {{}, state1, state2, state3});

        // delete markers
        std::swap(list[0], list[1]);
        list.pop_back();
        model->removeMarker(GenTime(1.3));
        checkMarkerList(model, list);
        auto state4 = list;
        checkStates(undoStack, model, {{}, state1, state2, state3, state4});

        list.pop_back();
        model->removeMarker(GenTime(0.3));
        checkMarkerList(model, list);
        auto state5 = list;
        checkStates(undoStack, model, {{}, state1, state2, state3, state4, state5});
    }

    SECTION("Json identity test") {
        std::vector<Marker> list;
        checkMarkerList(model, list);

        // add markers
        list.push_back(Marker(1.3, QLatin1String("test marker"), 3));
        model->addMarker(GenTime(1.3), QLatin1String("test marker"), 3);
        list.push_back(Marker(0.3, QLatin1String("test marker2"), 0));
        model->addMarker(GenTime(0.3), QLatin1String("test marker2"), 0);
        list.push_back(Marker(3, QLatin1String("test marker3"), 0));
        model->addMarker(GenTime(3), QLatin1String("test marker3"), 0);
        checkMarkerList(model, list);

        // export
        QString json = model->toJson();

        // clean
        model->removeMarker(GenTime(0.3));
        model->removeMarker(GenTime(3));
        model->removeMarker(GenTime(1.3));
        checkMarkerList(model, {});

        // Reimport
        REQUIRE(model->importFromJson(json, false));
        checkMarkerList(model, list);

        // undo/redo
        undoStack->undo();
        checkMarkerList(model, {});
        undoStack->redo();
        checkMarkerList(model, list);

        // now we try the same thing with non-empty model
        undoStack->undo();
        checkMarkerList(model, {});
        // non - conflicting marker
        list.push_back(Marker(5, QLatin1String("non conflicting"), 0));
        std::vector<Marker> otherMarkers;
        otherMarkers.push_back(Marker(5, QLatin1String("non conflicting"), 0));
        model->addMarker(GenTime(5), QLatin1String("non conflicting"), 0);
        REQUIRE(model->importFromJson(json, false));
        checkMarkerList(model, list);
        undoStack->undo();
        checkMarkerList(model, otherMarkers);
        undoStack->redo();
        checkMarkerList(model, list);
        undoStack->undo();

        // conflicting marker
        otherMarkers.push_back(Marker(1.3, QLatin1String("conflicting"), 1));
        model->addMarker(GenTime(1.3), QLatin1String("conflicting"), 1);
        checkMarkerList(model, otherMarkers);
        REQUIRE_FALSE(model->importFromJson(json, false));
        checkMarkerList(model, otherMarkers);
        REQUIRE(model->importFromJson(json, true));
        checkMarkerList(model, list);
        undoStack->undo();
        checkMarkerList(model, otherMarkers);
        undoStack->redo();
        checkMarkerList(model, list);
    }
}
