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

    auto state1 = [&](ProjectStorageType storageType) {
        const QString projectPath =
            document.url().isEmpty() ? QString() : QDir::cleanPath(QFileInfo(document.url().toLocalFile()).absolutePath() + QDir::separator());
        if (storageType != StoreInCustomFolder && (projectPath.isEmpty() || storageType == StoreInDefaultLocation)) {
            // storageType should not influence paths when project is not saved
            std::pair<QString, ProjectStorageType> tmpPath = document.projectTempFolder();
            REQUIRE(tmpPath.second == storageType);
            const QString baseCacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
            REQUIRE(tmpPath.first == baseCacheDir);
            QDir resultDir = document.getCacheDir(CacheProxy, &ok);
            REQUIRE(ok);
            REQUIRE(resultDir.absolutePath() == QDir(baseCacheDir).absoluteFilePath(QStringLiteral("proxy")));
            resultDir = document.getCacheDir(CachePreview, &ok);
            REQUIRE(ok);
            REQUIRE(resultDir.absolutePath() == QDir(baseCacheDir).absoluteFilePath(QStringLiteral("%1/preview").arg(documentId)));
            REQUIRE(document.projectCaptureFolder() == QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
            REQUIRE(KdenliveTests::folderForProjectFiles(&document) == QString());
        } else if (storageType == StoreWithProjectFile) {
            std::pair<QString, ProjectStorageType> tmpPath = document.projectTempFolder();
            REQUIRE(tmpPath.second == StoreWithProjectFile);
            REQUIRE(tmpPath.first == QDir(projectPath).absoluteFilePath(QStringLiteral("cachefiles")));
            QDir resultDir = document.getCacheDir(CacheProxy, &ok);
            REQUIRE(ok);
            REQUIRE(resultDir.absolutePath() == QDir(projectPath).absoluteFilePath(QStringLiteral("cachefiles/proxy")));
            resultDir = document.getCacheDir(CachePreview, &ok);
            REQUIRE(ok);
            REQUIRE(resultDir.absolutePath() == QDir(projectPath).absoluteFilePath(QStringLiteral("cachefiles/%1/preview").arg(documentId)));
            REQUIRE(document.projectCaptureFolder() == projectPath);
            REQUIRE(QDir::cleanPath(KdenliveTests::folderForProjectFiles(&document)) == projectPath);
        } else if (storageType == StoreInCustomFolder) {
            const QString storagePath = document.getDocumentProperty(QStringLiteral("storagefolder"));
            std::pair<QString, ProjectStorageType> tmpPath = document.projectTempFolder();
            REQUIRE(tmpPath.second == StoreInCustomFolder);
            REQUIRE(tmpPath.first == QDir(storagePath).absolutePath()); // FilePath(QStringLiteral("cachefiles")));
            QDir resultDir = document.getCacheDir(CacheProxy, &ok);
            REQUIRE(ok);
            REQUIRE(resultDir.absolutePath() == QDir(storagePath).absoluteFilePath(QStringLiteral("proxy")));
            resultDir = document.getCacheDir(CachePreview, &ok);
            REQUIRE(ok);
            REQUIRE(resultDir.absolutePath() == QDir(storagePath).absoluteFilePath(QStringLiteral("%1/preview").arg(documentId)));
            REQUIRE(document.projectCaptureFolder() == storagePath);
            REQUIRE(QDir::cleanPath(KdenliveTests::folderForProjectFiles(&document)) == storagePath);
        } else {
            qDebug() << "::::: UNHANDLED STORAGE TYPE: " << storageType;
            REQUIRE(false);
        }
    };

    SECTION("Default project paths")
    {
        // Test unsaved file first
        document.setDocumentProperty(QStringLiteral("storagetype"), QString::number(int(StoreInDefaultLocation)));
        std::pair<QString, ProjectStorageType> tmpPath = document.projectTempFolder();
        state1(StoreInDefaultLocation);

        // now check against a saved document
        document.setUrl(QUrl::fromLocalFile(QDir::temp().absoluteFilePath(QStringLiteral("test.kdenlive"))));
        state1(StoreInDefaultLocation);
    }

    SECTION("Default paths for save in project folder")
    {
        // Test unsaved file first
        document.setUrl(QUrl());
        document.setDocumentProperty(QStringLiteral("storagetype"), QString::number(int(StoreWithProjectFile)));
        state1(StoreWithProjectFile);

        // now check against a saved document
        document.setUrl(QUrl::fromLocalFile(QDir::temp().absoluteFilePath(QStringLiteral("test.kdenlive"))));
        state1(StoreWithProjectFile);
    }

    SECTION("Default paths for save in custom folder")
    {
        document.setDocumentProperty(QStringLiteral("storagefolder"), QDir::temp().absoluteFilePath(QString("storage")));
        document.setProjectFolder(QUrl::fromLocalFile(QDir::temp().absoluteFilePath(QString("storage"))));
        // Test unsaved file first
        document.setUrl(QUrl());
        document.setDocumentProperty(QStringLiteral("storagetype"), QString::number(int(StoreInCustomFolder)));
        state1(StoreInCustomFolder);

        // now check against a saved document
        document.setUrl(QUrl::fromLocalFile(QDir::temp().absoluteFilePath(QStringLiteral("test.kdenlive"))));
        state1(StoreInCustomFolder);
    }

    SECTION("Ensure projects with no storagetype defined are correctly detected")
    {
        // Unset storage type
        document.setDocumentProperty(QStringLiteral("storagetype"), QString());
        document.setProjectFolder(QUrl());
        // default paths
        document.setDocumentProperty(QStringLiteral("storagefolder"), QString());
        document.setUrl(QUrl::fromLocalFile(QDir::temp().absoluteFilePath("storage/test1.kdenlive")));
        std::pair<QString, ProjectStorageType> tmpPath = document.projectTempFolder();
        REQUIRE(tmpPath.second == StoreInDefaultLocation);
        REQUIRE(tmpPath.first == QStandardPaths::writableLocation(QStandardPaths::CacheLocation));

        // Custom folder
        document.setDocumentProperty(QStringLiteral("storagetype"), QString());
        const QString customFolder = QDir::temp().absoluteFilePath("custom");
        document.setProjectFolder(QUrl::fromLocalFile(customFolder));
        tmpPath = document.projectTempFolder();
        REQUIRE(tmpPath.second == StoreInCustomFolder);
        REQUIRE(tmpPath.first == QDir(customFolder).absolutePath());

        // Save in project folder
        document.setDocumentProperty(QStringLiteral("storagetype"), QString());
        const QString projectFolder = QDir::temp().absoluteFilePath("storage");
        document.setProjectFolder(QUrl::fromLocalFile(projectFolder));
        tmpPath = document.projectTempFolder();
        REQUIRE(tmpPath.second == StoreWithProjectFile);
        REQUIRE(tmpPath.first == QDir(projectFolder).absoluteFilePath(QStringLiteral("cachefiles")));
    }
    pCore->projectManager()->closeCurrentDocument(false, false);
}
