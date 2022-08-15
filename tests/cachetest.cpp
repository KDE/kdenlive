/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Eric Jiang
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "doc/docundostack.hpp"
#include "test_utils.hpp"

#include <QString>
#include <cmath>
#include <iostream>
#include <tuple>
#include <unordered_set>

#include "definitions.h"
#define private public
#define protected public
#include "core.h"
#include "utils/thumbnailcache.hpp"

Mlt::Profile profile_cache;

TEST_CASE("Cache insert-remove", "[Cache]")
{
    // Create timeline
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack

    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
    When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    // We also mock timeline object to spy few functions and mock others
    TimelineItemModel tim(&profile_cache, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    // Create bin clip
    QString binId = createProducer(profile_cache, "red", binModel, 20, false);

    SECTION("Insert and remove thumbnail")
    {
        QImage img(100, 100, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::red);
        ThumbnailCache::get()->storeThumbnail(binId, 0, img, false);
        REQUIRE(ThumbnailCache::get()->checkIntegrity());
        ThumbnailCache::get()->storeThumbnail(binId, 0, img, false);
        REQUIRE(ThumbnailCache::get()->checkIntegrity());
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
}

TEST_CASE("getAudioKey() should dereference `ok` param", "ThumbnailCache") {
    // Create timeline
    auto binModel = pCore->projectItemModel();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack
    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
    When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));

    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    // We also mock timeline object to spy few functions and mock others
    TimelineItemModel tim(&profile_cache, undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    TimelineItemModel::finishConstruct(timeline, guideModel);

    // Create bin clip
    QString binId = createProducer(profile_cache, "red", binModel, 20, false);

    SECTION("Request invalid id")
    {
        // Catches a bug where, after setting *ok, the code checks
        //     if (ok) {
        // instead of
        //     if (*ok) {
        bool ok = true;
        ThumbnailCache::getAudioKey(QStringLiteral("nonexistent-key"), &ok);
        REQUIRE(ok == false);
    }
    SECTION("Request valid id")
    {
        bool ok = false;
        ThumbnailCache::getAudioKey(binId, &ok);
        REQUIRE(ok == true);
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
}
