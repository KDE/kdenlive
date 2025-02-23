/*
    SPDX-FileCopyrightText: 2023 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "catch.hpp"
#include "test_utils.hpp"
// test specific headers
#include "utils/gentime.h"
#include "utils/qstringutils.h"
#include "utils/timecode.h"

TEST_CASE("Testing for different utils", "[Utils]")
{

    SECTION("Append to filename")
    {
        QString filename("myfile.mp4");
        QString appendix("-hello");
        QString newName = QStringUtils::appendToFilename(filename, appendix);
        qDebug() << newName;
        REQUIRE(newName == QStringLiteral("myfile-hello.mp4"));
    }

    SECTION("Unique names should really be unique")
    {
        QStringList names;
        for (int i = 0; i < 1000; i++) {
            names << QStringUtils::getUniqueName(names, QStringLiteral("my923test.string-1"));
        }

        REQUIRE(names.removeDuplicates() == 0);
    }
}

TEST_CASE("Testing for GenTime", "[GenTime]")
{
    SECTION("Create from frames and fps should work")
    {
        GenTime time(11.5);

        REQUIRE(time.seconds() == 11.5);
        REQUIRE(time.ms() == 11500);
        REQUIRE(time.frames(30) == 345);
    }

    SECTION("Create from seconds should work")
    {
        GenTime time(460, 12.5);

        REQUIRE(time.seconds() == 36.8);
        REQUIRE(time.ms() == 36800);
        REQUIRE(time.frames(30) == 1104);
    }

    SECTION("Calculations should work")
    {
        GenTime time_a(100);
        GenTime time_b(20);
        GenTime res;

        REQUIRE(time_a.seconds() == 100);
        REQUIRE(time_b.seconds() == 20);

        res = time_a * 2;
        REQUIRE(res.seconds() == 200);

        res = time_a / 2;
        REQUIRE(res.seconds() == 50);

        res = time_a - time_b;
        REQUIRE(res.seconds() == 80);

        res -= time_b;
        REQUIRE(res.seconds() == 60);

        res = time_a + time_b;
        REQUIRE(res.seconds() == 120);

        res = -time_a;
        REQUIRE(res.seconds() == -100);
    }
}

TEST_CASE("Testing for timecodes", "[Timecodes]")
{
    SECTION("Static frames to TC: should work for positive values")
    {
        // without frames
        QString tc = Timecode::getStringTimecode(273080, 25);
        REQUIRE(tc == QStringLiteral("03:02:03"));

        // with showFrames = true
        tc = Timecode::getStringTimecode(273080, 25, true);
        REQUIRE(tc == QStringLiteral("03:02:03.05"));
    }

    SECTION("Static frames to TC: should work for negative values")
    {
        // without frames
        QString tc = Timecode::getStringTimecode(-273080, 25);
        REQUIRE(tc == QStringLiteral("-03:02:03"));

        // with showFrames = true
        tc = Timecode::getStringTimecode(-273080, 25, true);
        REQUIRE(tc == QStringLiteral("-03:02:03.05"));
    }

    SECTION("Static frames to TC: should work for fps larger 100")
    {
        QString tc = Timecode::getStringTimecode(1310820, 120, true);
        REQUIRE(tc == QStringLiteral("03:02:03.060"));
    }

    SECTION("Frames to TC: should work")
    {
        Timecode tc;

        QString string = tc.getTimecodeFromFrames(273080);
        REQUIRE(string == QStringLiteral("03:02:03:05"));
    }

    SECTION("TC to Frames: should work for positive values")
    {
        Timecode tc;

        // without frames
        int frames = tc.getFrameCount(QStringLiteral("03:02:03"));
        REQUIRE(frames == 273075);

        // with frames
        frames = tc.getFrameCount(QStringLiteral("03:02:03.05"));
        REQUIRE(frames == 273080);
    }

    SECTION("TC to Frames: should work for negative values")
    {
        Timecode tc;

        // without frames
        int frames = tc.getFrameCount(QStringLiteral("-03:02:03"));
        REQUIRE(frames == -273075);

        // with frames
        frames = tc.getFrameCount(QStringLiteral("-03:02:03.05"));
        REQUIRE(frames == -273080);
    }

    SECTION("Scaling timecode should work")
    {
        // same fps should change nothing
        QString res = Timecode::scaleTimecode(QStringLiteral("01:02:03:10"), 30, 30);
        REQUIRE(res == QStringLiteral("01:02:03:10"));

        // target fps higher than source
        res = Timecode::scaleTimecode(QStringLiteral("01:02:03:10"), 30, 60);
        REQUIRE(res == QStringLiteral("01:02:03:20"));

        // target fps lower than source
        res = Timecode::scaleTimecode(QStringLiteral("01:02:03:10"), 30, 15);
        REQUIRE(res == QStringLiteral("01:02:03:05"));
    }
}
