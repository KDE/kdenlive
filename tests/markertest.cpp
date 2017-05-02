#include "catch.hpp"

#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic push
#include "fakeit.hpp"
#include <QColor>
#include <QString>
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

void checkMarkerList(std::shared_ptr<MarkerListModel> model, std::vector<Marker>& list)
{
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

TEST_CASE("Marker model", "[MarkerListModel]")
{
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    std::shared_ptr<MarkerListModel> model = std::make_shared<MarkerListModel>(undoStack, nullptr);

    std::vector<Marker> list;

    // Here we do some trickery to enable testing.
    // We mock the project class so that the getGuideModel function returns this model

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, getGuideModel)).AlwaysReturn(model);

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    SECTION("Basic Manipulation") {
        checkMarkerList(model, list);

        list.push_back(Marker(1.3, QLatin1String("test marker"), 3));
        model->addMarker(GenTime(1.3), QLatin1String("test marker"), 3);
        checkMarkerList(model, list);
    }
}
