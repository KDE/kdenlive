/*
    SPDX-FileCopyrightText: 2024 Étienne André <eti.andre@gmail.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "catch.hpp"
#include "test_utils.hpp"

#include "jobs/audiolevels/audiolevelstask.h"
#include "jobs/audiolevels/generators.h"

void computePeaksTestHelper(const QVector<int16_t> &input, const QVector<int16_t> &expectedOutput, const size_t channels)
{
    QVector<int16_t> output(expectedOutput.size());
    REQUIRE(input.size() % channels == 0);
    REQUIRE(output.size() % channels == 0);
    computePeaks(input.constData(), output.data(), channels, input.size() / channels, output.size() / channels);
    REQUIRE(output == expectedOutput);
}

void dummyClbk(const int progress, const QVector<int16_t> &levels)
{
    REQUIRE(progress <= 100);
    REQUIRE(progress >= 0);
    Q_UNUSED(levels);
}

void REQUIRE_SILENCE(const QVector<int16_t> &x)
{
    for (const auto val : x) {
        REQUIRE(val == 0);
    }
}

TEST_CASE("computePeaks single channel no-op")
{
    const QVector<int16_t> input = {1, 2, 3, 4, 5};
    const QVector<int16_t> expectedOutput = {1, 2, 3, 4, 5};
    computePeaksTestHelper(input, expectedOutput, 1);
}

TEST_CASE("computePeaks single channel, integer ratio")
{
    const QVector<int16_t> input = {1, 2, 3, 4, 5, 6};
    const QVector<int16_t> expectedOutput = {2, 4, 6};
    computePeaksTestHelper(input, expectedOutput, 1);
}

TEST_CASE("computePeaks single channel, non-integer ratio")
{
    const QVector<int16_t> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    const QVector<int16_t> expectedOutput = {1, 2, 3, 5, 6, 7, 8, 10};
    computePeaksTestHelper(input, expectedOutput, 1);
}

TEST_CASE("computePeaks single channel, inverse integer ratio")
{
    const QVector<int16_t> input = {1, 2, 3};
    const QVector<int16_t> expectedOutput = {1, 1, 1, 2, 2, 2, 3, 3, 3};
    computePeaksTestHelper(input, expectedOutput, 1);
}

TEST_CASE("computePeaks single channel, inverse non-integer ratio")
{
    const QVector<int16_t> input = {1, 2};
    const QVector<int16_t> expectedOutput = {1, 1, 1, 1, 2, 2, 2};
    computePeaksTestHelper(input, expectedOutput, 1);
}

TEST_CASE("computePeaks multi channel no-op")
{
    const QVector<int16_t> input = {1, 2, 3, 4, 5, 6};
    const QVector<int16_t> expectedOutput = {1, 2, 3, 4, 5, 6};
    computePeaksTestHelper(input, expectedOutput, 2);
}

TEST_CASE("computePeaks multi channel, integer ratio")
{
    const QVector<int16_t> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    const QVector<int16_t> expectedOutput = {3, 4, 7, 8, 11, 12};
    computePeaksTestHelper(input, expectedOutput, 2);
}

TEST_CASE("computePeaks multi channel, non-integer ratio")
{
    const QVector<int16_t> input = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    const QVector<int16_t> expectedOutput = {1, 2, 5, 6, 9, 10};
    computePeaksTestHelper(input, expectedOutput, 2);
}

TEST_CASE("computePeaks multi channel, inverse integer ratio")
{
    const QVector<int16_t> input = {1, 2, 3, 4, 5, 6};
    const QVector<int16_t> expectedOutput = {1, 2, 1, 2, 3, 4, 3, 4, 5, 6, 5, 6};
    computePeaksTestHelper(input, expectedOutput, 2);
}

TEST_CASE("computePeaks multi channel, inverse non-integer ratio")
{
    const QVector<int16_t> input = {1, 2, 3, 4};
    const QVector<int16_t> expectedOutput = {1, 2, 1, 2, 1, 2, 3, 4, 3, 4};
    computePeaksTestHelper(input, expectedOutput, 2);
}

TEST_CASE("computePeaks large input")
{
    QVector<int16_t> input;
    for (int i = 0; i < 100000; ++i) {
        input.push_back(i % 10);
    }
    const QVector<int16_t> expectedOutput(10000, 9);

    computePeaksTestHelper(input, expectedOutput, 1);
}

TEST_CASE("generateLibav bad stream index")
{
    const auto output = generateLibav(9999, sourcesPath + "/dataset/mono.flac", 10, 30, &dummyClbk, 0);
    REQUIRE(output.isEmpty());
}

TEST_CASE("generateLibav bad file path")
{
    const auto output = generateLibav(0, "i-do-not-exist.mp3", 10, 30, &dummyClbk, 0);
    REQUIRE(output.isEmpty());
}

TEST_CASE("generateMLT bad file path")
{
    const auto output = generateMLT(0, "avformat", "i-do-not-exist.mp3", 1, &dummyClbk, 0);
    REQUIRE(output.isEmpty());
}

TEST_CASE("generateLibav correct output length")
{
    const auto output = generateLibav(0, sourcesPath + "/dataset/mono.flac", 30, 30, &dummyClbk, 0);
    REQUIRE(output.size() == 30 * AUDIOLEVELS_POINTS_PER_FRAME);
}

TEST_CASE("generateMLT correct output length")
{
    const auto profileFps = pCore->getCurrentFps();
    const auto output = generateMLT(0, "avformat", sourcesPath + "/dataset/mono.flac", 1, &dummyClbk, 0);
    REQUIRE(output.size() == profileFps * AUDIOLEVELS_POINTS_PER_FRAME);
}

TEST_CASE("generateLibav canceled")
{
    const auto output = generateLibav(0, sourcesPath + "/dataset/mono.flac", 10, 30, &dummyClbk, 1);
    REQUIRE(output.isEmpty());
}

TEST_CASE("generateMLT canceled")
{
    const auto output = generateMLT(0, "avformat", sourcesPath + "/dataset/mono.flac", 1, &dummyClbk, 1);
    REQUIRE(output.isEmpty());
}

TEST_CASE("generateLibav not enough frames requested")
{
    const auto output = generateLibav(0, sourcesPath + "/dataset/mono.flac", 1, 30, &dummyClbk, 0);
    REQUIRE(output.isEmpty());
}

TEST_CASE("both methods on mono audio")
{
    const auto profileFps = pCore->getCurrentFps();
    const auto a = generateMLT(0, "avformat", sourcesPath + "/dataset/mono.flac", 1, &dummyClbk, 0);
    const auto lengthInFrames = a.size() / 1 / AUDIOLEVELS_POINTS_PER_FRAME;
    const auto b = generateLibav(0, sourcesPath + "/dataset/mono.flac", lengthInFrames, profileFps, &dummyClbk, 0);
    REQUIRE(!a.isEmpty());
    REQUIRE(a.size() % AUDIOLEVELS_POINTS_PER_FRAME == 0);
    REQUIRE(a == b);
}

TEST_CASE("both methods on stereo audio")
{
    const auto profileFps = pCore->getCurrentFps();
    const auto a = generateMLT(0, "avformat", sourcesPath + "/dataset/stereo.flac", 2, &dummyClbk, 0);
    const auto lengthInFrames = a.size() / 2 / AUDIOLEVELS_POINTS_PER_FRAME;
    const auto b = generateLibav(0, sourcesPath + "/dataset/stereo.flac", lengthInFrames, profileFps, &dummyClbk, 0);
    REQUIRE(!a.isEmpty());
    REQUIRE(a.size() % AUDIOLEVELS_POINTS_PER_FRAME == 0);
    REQUIRE(a.size() % 2 == 0);
    REQUIRE(a == b);
}

TEST_CASE("both methods on multiple audio streams")
{
    const auto profileFps = pCore->getCurrentFps();
    SECTION("Stream 0: mono")
    {
        const auto a = generateMLT(0, "avformat", sourcesPath + "/dataset/lots_of_audio_streams.mkv", 1, &dummyClbk, 0);
        const auto lengthInFrames = a.size() / 1 / AUDIOLEVELS_POINTS_PER_FRAME;
        const auto b = generateLibav(0, sourcesPath + "/dataset/lots_of_audio_streams.mkv", lengthInFrames, profileFps, &dummyClbk, 0);
        REQUIRE(!a.isEmpty());
        REQUIRE(a.size() % AUDIOLEVELS_POINTS_PER_FRAME == 0);
        REQUIRE(a == b);
    }
    SECTION("Stream 1: stereo")
    {
        const auto a = generateMLT(1, "avformat", sourcesPath + "/dataset/lots_of_audio_streams.mkv", 2, &dummyClbk, 0);
        const auto lengthInFrames = a.size() / 2 / AUDIOLEVELS_POINTS_PER_FRAME;
        const auto b = generateLibav(1, sourcesPath + "/dataset/lots_of_audio_streams.mkv", lengthInFrames, profileFps, &dummyClbk, 0);
        REQUIRE(!a.isEmpty());
        REQUIRE(a.size() % AUDIOLEVELS_POINTS_PER_FRAME == 0);
        REQUIRE(a.size() % 2 == 0);
        REQUIRE(a == b);
    }
    SECTION("Stream 2: surround")
    {
        const auto a = generateMLT(2, "avformat", sourcesPath + "/dataset/lots_of_audio_streams.mkv", 6, &dummyClbk, 0);
        const auto lengthInFrames = a.size() / 6 / AUDIOLEVELS_POINTS_PER_FRAME;
        const auto b = generateLibav(2, sourcesPath + "/dataset/lots_of_audio_streams.mkv", lengthInFrames, profileFps, &dummyClbk, 0);
        REQUIRE(!a.isEmpty());
        REQUIRE(a.size() % AUDIOLEVELS_POINTS_PER_FRAME == 0);
        REQUIRE(a.size() % 6 == 0);
        REQUIRE(a == b);
    }
}

TEST_CASE("(de)serialize audio levels")
{
    const auto input = QVector<int16_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto tmp = QTemporaryFile();
    REQUIRE(tmp.open());
    AudioLevelsTask::saveLevelsToCache(tmp.fileName(), input);
    const auto deserialized = AudioLevelsTask::getLevelsFromCache(tmp.fileName());
    REQUIRE(deserialized == input);
}