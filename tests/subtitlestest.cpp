/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Eric Jiang
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"

#include "definitions.h"
#define private public
#define protected public
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"

using namespace fakeit;

TEST_CASE("Read subtitle file", "[Subtitles]")
{
    // Create timeline
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack
    KdenliveDoc document(undoStack);
    Mock<KdenliveDoc> docMock(document);
    KdenliveDoc &mockedDoc = docMock.get();

    // We mock the project class so that the undoStack function returns our undoStack, and our mocked document
    Mock<ProjectManager> pmMock;
    When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
    When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
    When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
    ProjectManager &mocked = pmMock.get();
    pCore->m_projectManager = &mocked;

    mocked.m_project = &mockedDoc;
    QDateTime documentDate = QDateTime::currentDateTime();
    mocked.updateTimeline(0, false, QString(), QString(), documentDate, 0);
    auto timeline = mockedDoc.getTimeline(mockedDoc.uuid());
    mocked.m_activeTimelineModel = timeline;
    mocked.testSetActiveDocument(&mockedDoc, timeline);
    QString documentId = QString::number(QDateTime::currentMSecsSinceEpoch());
    mockedDoc.setDocumentProperty(QStringLiteral("documentid"), documentId);

    // Initialize subtitle model
    std::shared_ptr<SubtitleModel> subtitleModel = timeline->createSubtitleModel();

    SECTION("Load a subtitle file")
    {
        QString subtitleFile = sourcesPath + "/dataset/01.srt";
        bool ok;
        QByteArray guessedEncoding = SubtitleModel::guessFileEncoding(subtitleFile, &ok);
        CHECK(guessedEncoding == "UTF-8");
        subtitleModel->importSubtitle(subtitleFile, 0, false, 30.00, 30.00, guessedEncoding);
        // Ensure the 3 dialogues are loaded
        REQUIRE(subtitleModel->rowCount() == 3);
        QList<SubtitledTime> allSubs = subtitleModel->getAllSubtitles();
        QList<GenTime> sTime;
        QList<GenTime> controleTime;
        controleTime << GenTime(140, 25) << GenTime(265, 25) << GenTime(503, 25) << GenTime(628, 25) << GenTime(628, 25) << GenTime(875, 25);
        QStringList subtitlesText;
        QStringList control = {QStringLiteral("J'hésite à vérifier"), QStringLiteral("Ce test de sous-titres"), QStringLiteral("!! Quand même !!")};
        for (const auto &s : qAsConst(allSubs)) {
            subtitlesText << s.subtitle();
            sTime << s.start();
            sTime << s.end();
        }
        // Ensure the texts are read correctly
        REQUIRE(subtitlesText == control);
        // Ensure timeing is correct
        REQUIRE(sTime == controleTime);
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);
    }

    // TODO: qt6 fix
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    SECTION("Load a non-UTF-8 subtitle")
    {
        QString subtitleFile = sourcesPath + "/dataset/01-iso-8859-1.srt";
        bool ok;
        QByteArray guessedEncoding = SubtitleModel::guessFileEncoding(subtitleFile, &ok);
        qDebug() << "Guessed encoding: " << guessedEncoding;
        subtitleModel->importSubtitle(subtitleFile, 0, false, 30.00, 30.00, guessedEncoding);
        // Ensure the 3 dialogues are loaded
        REQUIRE(subtitleModel->rowCount() == 3);
        QList<SubtitledTime> allSubs = subtitleModel->getAllSubtitles();
        QStringList subtitlesText;
        QStringList control = {QStringLiteral("J'hésite à vérifier"), QStringLiteral("Ce test de sous-titres"), QStringLiteral("!! Quand même !!")};
        for (const auto &s : qAsConst(allSubs)) {
            subtitlesText << s.subtitle();
        }
        // Ensure that non-ASCII characters are read correctly
        CHECK(subtitlesText == control);
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);
    }
#endif

    SECTION("Load ASS file with commas")
    {
        QString subtitleFile = sourcesPath + "/dataset/subs-with-commas.ass";
        bool ok;
        QByteArray guessedEncoding = SubtitleModel::guessFileEncoding(subtitleFile, &ok);
        qDebug() << "Guessed encoding: " << guessedEncoding;
        subtitleModel->importSubtitle(subtitleFile, 0, false, 30.00, 30.00, guessedEncoding);
        // Ensure all 2 lines are loaded
        REQUIRE(subtitleModel->rowCount() == 2);
        QList<SubtitledTime> allSubs = subtitleModel->getAllSubtitles();
        QStringList subtitlesText;
        QStringList control = {QStringLiteral("Line with one comma, second part."), QStringLiteral("Line with two commas, second part, third part.")};
        for (const auto &s : qAsConst(allSubs)) {
            subtitlesText << s.subtitle();
        }
        // Ensure that non-ASCII characters are read correctly
        CHECK(subtitlesText == control);
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);
    }

    SECTION("Load a broken subtitle file")
    {
        QString subtitleFile = sourcesPath + "/dataset/02.srt";
        subtitleModel->importSubtitle(subtitleFile);
        // Ensure the 2 dialogues are loaded
        REQUIRE(subtitleModel->rowCount() == 2);
        QList<SubtitledTime> allSubs = subtitleModel->getAllSubtitles();
        QList<GenTime> sTime;
        QList<GenTime> controleTime;
        controleTime << GenTime(140, 25) << GenTime(265, 25) << GenTime(628, 25) << GenTime(875, 25);
        QStringList subtitlesText;
        QStringList control = {QStringLiteral("J'hésite à vérifier"), QStringLiteral("!! Quand même !!")};
        for (const auto &s : allSubs) {
            subtitlesText << s.subtitle();
            sTime << s.start();
            sTime << s.end();
        }
        // Ensure the texts are read correctly
        REQUIRE(subtitlesText == control);
        // Ensure timeing is correct
        REQUIRE(sTime == controleTime);
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);
    }

    SECTION("Preserve multiple spaces in subtitles")
    {
        QString subtitleFile = sourcesPath + "/dataset/multiple-spaces.srt";
        subtitleModel->importSubtitle(subtitleFile);
        const QList<SubtitledTime> allSubs = subtitleModel->getAllSubtitles();
        CHECK(allSubs.at(0).subtitle().toStdString() == "three   spaces");
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);
    }

    SECTION("Load SBV subtitle file")
    {
        QString subtitleFile = sourcesPath + "/dataset/01.sbv";
        subtitleModel->importSubtitle(subtitleFile);
        // Ensure the 3 dialogues are loaded
        REQUIRE(subtitleModel->rowCount() == 3);
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);
    }

    SECTION("Load VTT subtitle file")
    {
        QString subtitleFile = sourcesPath + "/dataset/01.vtt";
        subtitleModel->importSubtitle(subtitleFile);
        // Ensure the 2 dialogues are loaded
        REQUIRE(subtitleModel->rowCount() == 2);
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);
    }

    SECTION("Load SRT subtitle file with two dots")
    {
        QString subtitleFile = sourcesPath + "/dataset/subs-with-two-dots.srt";
        subtitleModel->importSubtitle(subtitleFile);
        // Ensure the 2 dialogues are loaded
        REQUIRE(subtitleModel->rowCount() == 2);
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);
    }

    SECTION("Ensure 2 subtitles cannot be place at same frame position")
    {
        // In our current implementation, having 2 subtitles at same start time is not allowed
        int subId = TimelineModel::getNextId();
        int subId2 = TimelineModel::getNextId();
        int subId3 = TimelineModel::getNextId();
        double fps = pCore->getCurrentFps();
        REQUIRE(subtitleModel->addSubtitle(subId, GenTime(50, fps), GenTime(70, fps), QStringLiteral("Hello"), false, false));
        REQUIRE(subtitleModel->addSubtitle(subId2, GenTime(50, fps), GenTime(90, fps), QStringLiteral("Hello2"), false, false) == false);
        REQUIRE(subtitleModel->addSubtitle(subId3, GenTime(100, fps), GenTime(140, fps), QStringLiteral("Second"), false, false));
        REQUIRE(subtitleModel->rowCount() == 2);
        REQUIRE(subtitleModel->moveSubtitle(subId, GenTime(100, fps), false, false) == false);
        REQUIRE(subtitleModel->moveSubtitle(subId, GenTime(300, fps), false, false));
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);
    }

    binModel->clean();
    pCore->m_projectManager = nullptr;
}
