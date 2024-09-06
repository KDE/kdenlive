/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Eric Jiang
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"
// test specific headers
#include "core.h"
#include "definitions.h"
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
    pCore->projectManager()->testSetDocument(&document);
    QDateTime documentDate = QDateTime::currentDateTime();
    KdenliveTests::updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->testSetActiveTimeline(timeline);
    KdenliveTests::resetNextId();
    QDir dir = QDir::temp();

    QString documentId = QString::number(QDateTime::currentMSecsSinceEpoch());
    document.setDocumentProperty(QStringLiteral("documentid"), documentId);

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
        QList<std::pair<std::pair<int, GenTime>, SubtitleEvent>> allSubs = subtitleModel->getAllSubtitles();
        QList<GenTime> sTime;
        QList<GenTime> controleTime;
        controleTime << GenTime(140, 25) << GenTime(265, 25) << GenTime(503, 25) << GenTime(628, 25) << GenTime(628, 25) << GenTime(875, 25);
        QStringList subtitlesText;
        QStringList control = {QStringLiteral("J'hésite à vérifier"), QStringLiteral("Ce test de sous-titres"), QStringLiteral("!! Quand même !!")};
        for (const auto &s : std::as_const(allSubs)) {
            subtitlesText << s.second.text();
            sTime << s.first.second;
            sTime << s.second.endTime();
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
        QList<std::pair<std::pair<int, GenTime>, SubtitleEvent>> allSubs = subtitleModel->getAllSubtitles();
        QStringList subtitlesText;
        QStringList control = {QStringLiteral("J'hésite à vérifier"), QStringLiteral("Ce test de sous-titres"), QStringLiteral("!! Quand même !!")};
        for (const auto &s : std::as_const(allSubs)) {
            subtitlesText << s.second.text();
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
        QList<std::pair<std::pair<int, GenTime>, SubtitleEvent>> allSubs = subtitleModel->getAllSubtitles();
        QStringList subtitlesText;
        QStringList control = {QStringLiteral("Line with one comma, second part."), QStringLiteral("Line with two commas, second part, third part.")};
        for (const auto &s : std::as_const(allSubs)) {
            subtitlesText << s.second.text();
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
        QList<std::pair<std::pair<int, GenTime>, SubtitleEvent>> allSubs = subtitleModel->getAllSubtitles();
        QList<GenTime> sTime;
        QList<GenTime> controleTime;
        controleTime << GenTime(140, 25) << GenTime(265, 25) << GenTime(628, 25) << GenTime(875, 25);
        QStringList subtitlesText;
        QStringList control = {QStringLiteral("J'hésite à vérifier"), QStringLiteral("!! Quand même !!")};
        for (const auto &s : allSubs) {
            subtitlesText << s.second.text();
            sTime << s.first.second;
            sTime << s.second.endTime();
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
        const QList<std::pair<std::pair<int, GenTime>, SubtitleEvent>> allSubs = subtitleModel->getAllSubtitles();
        CHECK(allSubs.at(0).second.text().toStdString() == "three   spaces");
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
        int subId = KdenliveTests::getNextId();
        int subId2 = KdenliveTests::getNextId();
        int subId3 = KdenliveTests::getNextId();
        double fps = pCore->getCurrentFps();
        REQUIRE(subtitleModel->addSubtitle(subId, {0, GenTime(50, fps)},
                                           SubtitleEvent(true, GenTime(70, fps), "Default", "", 0, 0, 0, "", QStringLiteral("Hello")), false, false));
        REQUIRE(subtitleModel->addSubtitle(subId2, {0, GenTime(50, fps)},
                                           SubtitleEvent(true, GenTime(90, fps), "Default", "", 0, 0, 0, "", QStringLiteral("Hello2")), false, false) == false);
        REQUIRE(subtitleModel->addSubtitle(subId3, {0, GenTime(100, fps)},
                                           SubtitleEvent(true, GenTime(140, fps), "Default", "", 0, 0, 0, "", QStringLiteral("Second")), false, false));
        REQUIRE(subtitleModel->rowCount() == 2);
        REQUIRE(subtitleModel->moveSubtitle(subId, 0, GenTime(100, fps), false, false) == false);
        REQUIRE(subtitleModel->moveSubtitle(subId, 0, GenTime(300, fps), false, false));
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);
    }

    SECTION("Ensure we cannot cut overlapping subtitles (it would create 2 subtitles at same frame position")
    {
        // In our current implementation, having 2 subtitles at same start time is not allowed
        int subId = KdenliveTests::getNextId();
        int subId2 = KdenliveTests::getNextId();
        double fps = pCore->getCurrentFps();
        REQUIRE(subtitleModel->addSubtitle(subId, {0, GenTime(50, fps)},
                                           SubtitleEvent(true, GenTime(70, fps), "Default", "", 0, 0, 0, "", QStringLiteral("Hello")), false, false));
        REQUIRE(subtitleModel->addSubtitle(subId2, {0, GenTime(60, fps)},
                                           SubtitleEvent(true, GenTime(90, fps), "Default", "", 0, 0, 0, "", QStringLiteral("Hello2")), false, false));
        REQUIRE(subtitleModel->rowCount() == 2);
        REQUIRE(timeline->requestClipsGroup({subId, subId2}));
        REQUIRE_FALSE(TimelineFunctions::requestClipCut(timeline, subId, 65));
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);
    }

    SECTION("Read start/end time of the subtitles")
    {
        // srt
        QString subtitleFile = sourcesPath + "/dataset/01.srt";
        subtitleModel->importSubtitle(subtitleFile);
        // Ensure the 3 dialogues are loaded
        REQUIRE(subtitleModel->rowCount() == 3);
        QList<std::pair<std::pair<int, GenTime>, SubtitleEvent>> allSubs = subtitleModel->getAllSubtitles();
        REQUIRE(allSubs.at(0).first.second.ms() - 5600 < 40);
        REQUIRE(allSubs.at(0).second.endTime().ms() - 10600 < 40);
        REQUIRE(allSubs.at(1).first.second.ms() - 20120 < 40);
        REQUIRE(allSubs.at(1).second.endTime().ms() - 25120 < 40);
        REQUIRE(allSubs.at(2).first.second.ms() - 25120 < 40);
        REQUIRE(allSubs.at(2).second.endTime().ms() - 35000 < 40);
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);

        // sbv
        subtitleFile = sourcesPath + "/dataset/01.sbv";
        subtitleModel->importSubtitle(subtitleFile);
        // Ensure the 3 dialogues are loaded
        REQUIRE(subtitleModel->rowCount() == 3);
        allSubs = subtitleModel->getAllSubtitles();
        REQUIRE(allSubs.at(0).first.second.ms() - 150000 < 40);
        REQUIRE(allSubs.at(0).second.endTime().ms() - 286000 < 40);
        REQUIRE(allSubs.at(1).first.second.ms() - 342000 < 40);
        REQUIRE(allSubs.at(1).second.endTime().ms() - 353000 < 40);
        REQUIRE(allSubs.at(2).first.second.ms() - 411000 < 40);
        REQUIRE(allSubs.at(2).first.second.ms() - 415000 < 40);
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);

        // vtt
        subtitleFile = sourcesPath + "/dataset/01.vtt";
        subtitleModel->importSubtitle(subtitleFile);
        // Ensure the 2 dialogues are loaded
        REQUIRE(subtitleModel->rowCount() == 2);
        allSubs = subtitleModel->getAllSubtitles();
        REQUIRE(allSubs.at(0).first.second.ms() - 600 < 40);
        REQUIRE(allSubs.at(0).second.endTime().ms() - 2000 < 40);
        REQUIRE(allSubs.at(1).first.second.ms() - 2400 < 40);
        REQUIRE(allSubs.at(1).second.endTime().ms() - 4400 < 40);
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);

        // ASS
        subtitleFile = sourcesPath + "/dataset/subs-with-commas.ass";
        subtitleModel->importSubtitle(subtitleFile);
        // Ensure the 2 dialogues are loaded
        REQUIRE(subtitleModel->rowCount() == 2);
        allSubs = subtitleModel->getAllSubtitles();
        REQUIRE(allSubs.at(0).first.second.ms() - 580 < 40);
        REQUIRE(allSubs.at(0).second.endTime().ms() - 2980 < 40);
        REQUIRE(allSubs.at(1).first.second.ms() - 4330 < 40);
        REQUIRE(allSubs.at(1).second.endTime().ms() - 9100 < 40);
        subtitleModel->removeAllSubtitles();
        REQUIRE(subtitleModel->rowCount() == 0);
    }

    binModel->clean();
    pCore->projectManager()->closeCurrentDocument(false, false);
}
