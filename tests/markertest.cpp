#include "test_utils.hpp"

#include "kdenlivesettings.h"
#define private public
#define protected public
#include "bin/model/markerlistmodel.hpp"
#include "timeline2/model/snapmodel.hpp"

using Marker = std::tuple<GenTime, QString, int>;
double fps;

void checkMarkerList(const std::shared_ptr<MarkerListModel> &model, const std::vector<Marker> &l, const std::shared_ptr<SnapModel> &snaps)
{
    auto list = l;
    std::sort(list.begin(), list.end(), [](const Marker &a, const Marker &b) { return std::get<0>(a) < std::get<0>(b); });

    REQUIRE(model->rowCount() == (int)list.size());
    if (model->rowCount() == 0) {
        REQUIRE(snaps->getClosestPoint(0) == -1);
    }
    for (int i = 0; i < model->rowCount(); ++i) {
        REQUIRE(qAbs(std::get<0>(list[i]).seconds() - model->data(model->index(i), MarkerListModel::PosRole).toDouble()) < 0.9 / fps);
        REQUIRE(std::get<1>(list[i]) == model->data(model->index(i), MarkerListModel::CommentRole).toString());
        REQUIRE(std::get<2>(list[i]) == model->data(model->index(i), MarkerListModel::TypeRole).toInt());
        REQUIRE(MarkerListModel::markerTypes[std::get<2>(list[i])] == model->data(model->index(i), MarkerListModel::ColorRole).value<QColor>());

        // check for marker existence
        int frame = std::get<0>(list[i]).frames(fps);
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
    fps = pCore->getCurrentFps();
    GenTime::setFps(fps);
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
    pCore->m_projectManager = nullptr;
}
