/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"
// test specific headers
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"

#include "core.h"
#include <QStandardPaths>

using namespace fakeit;

TEST_CASE("Project Paths", "[ProjectPaths]")
{
    // Create timeline
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Create document
    KdenliveDoc document(undoStack);
    pCore->projectManager()->testSetDocument(&document);
    const QString documentId = QString::number(QDateTime::currentMSecsSinceEpoch());
    bool ok;
    documentId.toLongLong(&ok, 10);
    REQUIRE(ok);
    document.setDocumentProperty(QStringLiteral("documentid"), documentId);

    SECTION("Default project paths")
    {
        // Test unsaved file first
        const QString baseCacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        document.setDocumentProperty(QStringLiteral("storagetype"), QString::number(int(StoreInDefaultLocation)));
        std::pair<QString, ProjectStorageType> tmpPath = document.projectTempFolder();
        REQUIRE(tmpPath.second == StoreInDefaultLocation);
        REQUIRE(tmpPath.first == baseCacheDir);
        QDir resultDir = document.getCacheDir(CacheProxy, &ok);
        REQUIRE(ok);
        REQUIRE(resultDir.absolutePath() == QDir(baseCacheDir).absoluteFilePath(QStringLiteral("proxy")));
        resultDir = document.getCacheDir(CachePreview, &ok);
        REQUIRE(ok);
        REQUIRE(resultDir.absolutePath() == QDir(baseCacheDir).absoluteFilePath(QStringLiteral("%1/preview").arg(documentId)));
        REQUIRE(document.projectCaptureFolder() == QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
        REQUIRE(KdenliveTests::folderForProjectFiles(&document) == QString());

        // now check against a saved document
        document.setUrl(QUrl::fromLocalFile(QDir::temp().absoluteFilePath(QStringLiteral("test.kdenlive"))));
        tmpPath = document.projectTempFolder();
        REQUIRE(tmpPath.second == StoreInDefaultLocation);
        REQUIRE(tmpPath.first == baseCacheDir);
        resultDir = document.getCacheDir(CacheProxy, &ok);
        REQUIRE(ok);
        REQUIRE(resultDir.absolutePath() == QDir(baseCacheDir).absoluteFilePath(QStringLiteral("proxy")));
        resultDir = document.getCacheDir(CachePreview, &ok);
        REQUIRE(ok);
        REQUIRE(resultDir.absolutePath() == QDir(baseCacheDir).absoluteFilePath(QStringLiteral("%1/preview").arg(documentId)));
        REQUIRE(document.projectCaptureFolder() == QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
        REQUIRE(KdenliveTests::folderForProjectFiles(&document) == QString());
    }

    SECTION("Default paths for save in project folder")
    {
        // Test unsaved file first
        QString baseCacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        document.setUrl(QUrl());
        document.setDocumentProperty(QStringLiteral("storagetype"), QString::number(int(StoreWithProjectFile)));
        std::pair<QString, ProjectStorageType> tmpPath = document.projectTempFolder();
        REQUIRE(tmpPath.second == StoreWithProjectFile);
        REQUIRE(tmpPath.first == baseCacheDir);
        QDir resultDir = document.getCacheDir(CacheProxy, &ok);
        REQUIRE(ok);
        REQUIRE(resultDir.absolutePath() == QDir(baseCacheDir).absoluteFilePath(QStringLiteral("proxy")));
        resultDir = document.getCacheDir(CachePreview, &ok);
        REQUIRE(ok);
        REQUIRE(resultDir.absolutePath() == QDir(baseCacheDir).absoluteFilePath(QStringLiteral("%1/preview").arg(documentId)));
        REQUIRE(document.projectCaptureFolder() == QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
        REQUIRE(KdenliveTests::folderForProjectFiles(&document) == QString());

        // now check against a saved document
        document.setUrl(QUrl::fromLocalFile(QDir::temp().absoluteFilePath(QStringLiteral("test.kdenlive"))));
        baseCacheDir = QDir::tempPath();
        tmpPath = document.projectTempFolder();
        REQUIRE(tmpPath.second == StoreWithProjectFile);
        REQUIRE(tmpPath.first == QDir(baseCacheDir).absoluteFilePath(QStringLiteral("cachefiles")));
        resultDir = document.getCacheDir(CacheProxy, &ok);
        REQUIRE(ok);
        REQUIRE(resultDir.absolutePath() == QDir(baseCacheDir).absoluteFilePath(QStringLiteral("cachefiles/proxy")));
        resultDir = document.getCacheDir(CachePreview, &ok);
        REQUIRE(ok);
        REQUIRE(resultDir.absolutePath() == QDir(baseCacheDir).absoluteFilePath(QStringLiteral("cachefiles/%1/preview").arg(documentId)));
        REQUIRE(document.projectCaptureFolder() == QDir::tempPath());
        qDebug() << "CACHE PATH COMP: " << KdenliveTests::folderForProjectFiles(&document) << " == " << baseCacheDir;
        REQUIRE(QDir::cleanPath(KdenliveTests::folderForProjectFiles(&document)) == QDir::cleanPath(baseCacheDir));
    }
    pCore->projectManager()->closeCurrentDocument(false, false);
}
