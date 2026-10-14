/*
    SPDX-FileCopyrightText: 2022 Eric Jiang
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "test_utils.hpp"
// test specific headers
#include "scopes/colorscopes/colorconstants.h"
#include "scopes/colorscopes/vectorscopegenerator.h"
#include "scopes/colorscopes/waveformgenerator.h"
#include "scopes/colorscopes/rgbparadegenerator.h"
#include "scopes/colorscopes/histogramgenerator.h"

// test for a bug where pixels were assumed to be RGB which was not true on
// Windows, resulting in red and blue switched. BUG: 453149
// Multiple scopes are affected, including vectorscope and waveform
TEST_CASE("Colorscope RGB/BGR handling")
{
    // create a fully red image
    QImage inputImage(480, 320, QImage::Format_RGB32);
    inputImage.fill(Qt::red);
    QImage bgrInputImage = inputImage.convertToFormat(QImage::Format_BGR30);

    QSize scopeSize{256, 256};
    qreal scalingFactor = 1.0;

    SECTION("Vectorscope handles both RGB and BGR")
    {
        VectorscopeGenerator vectorscope{};
        QImage rgbScope = vectorscope.calculateVectorscope(scopeSize, scalingFactor, inputImage, 1, VectorscopeGenerator::PaintMode::PaintMode_Green2,
                                                           VectorscopeGenerator::ColorSpace::ColorSpace_YUV, false, 7);
        QImage bgrScope = vectorscope.calculateVectorscope(scopeSize, scalingFactor, bgrInputImage, 1, VectorscopeGenerator::PaintMode::PaintMode_Green2,
                                                           VectorscopeGenerator::ColorSpace::ColorSpace_YUV, false, 7);

        // both of these should be equivalent, since the vectorscope should
        // handle different pixel formats
        CHECK(rgbScope == bgrScope);
    }

    SECTION("Waveform handles both RGB and BGR")
    {
        WaveformGenerator waveform{};
        QImage rgbScope =
            waveform.calculateWaveform(scopeSize, scalingFactor, inputImage, WaveformGenerator::PaintMode::PaintMode_Yellow, false, ITURec::Rec_709, 3);
        QImage bgrScope =
            waveform.calculateWaveform(scopeSize, scalingFactor, bgrInputImage, WaveformGenerator::PaintMode::PaintMode_Yellow, false, ITURec::Rec_709, 3);

        CHECK(rgbScope == bgrScope);
    }

    SECTION("RGB Parade handles both RGB and BGR")
    {
        RGBParadeGenerator rgb{};
        QImage rgbScope = rgb.calculateRGBParade(scopeSize, scalingFactor, inputImage, RGBParadeGenerator::PaintMode::PaintMode_RGB, false, false, 3);
        QImage bgrScope = rgb.calculateRGBParade(scopeSize, scalingFactor, bgrInputImage, RGBParadeGenerator::PaintMode::PaintMode_RGB, false, false, 3);

        CHECK(rgbScope == bgrScope);
    }

    SECTION("Histogram handles both RGB and BGR")
    {
        const auto ALL_COMPONENTS = HistogramGenerator::Components::ComponentR |
            HistogramGenerator::Components::ComponentG |
            HistogramGenerator::Components::ComponentB;

        HistogramGenerator hist{};
        QImage rgbScope = hist.calculateHistogram(scopeSize, scalingFactor, inputImage, ALL_COMPONENTS, ITURec::Rec_709, false, false, 3);
        QImage bgrScope = hist.calculateHistogram(scopeSize, scalingFactor, bgrInputImage, ALL_COMPONENTS, ITURec::Rec_709, false, false, 3);

        CHECK(rgbScope == bgrScope);
    }
}

// The scope widgets can be resized to only a few pixels, e.g. in a narrow dock.
// The scope area sizes must not underflow then.
TEST_CASE("Colorscopes handle sizes smaller than their margins")
{
    QImage inputImage(480, 320, QImage::Format_RGB32);
    inputImage.fill(Qt::red);
    qreal scalingFactor = 1.0;

    SECTION("RGB Parade")
    {
        RGBParadeGenerator rgb{};
        for (const QSize &size : {QSize(20, 200), QSize(40, 200), QSize(53, 200), QSize(200, 30), QSize(200, 44)}) {
            CAPTURE(size);
            QImage scope = rgb.calculateRGBParade(size, scalingFactor, inputImage, RGBParadeGenerator::PaintMode::PaintMode_RGB, true, true, 1);
            CHECK(scope.size().boundedTo(size) == scope.size());
        }
        // The smallest size that still has room for the parade
        QImage scope = rgb.calculateRGBParade(QSize(54, 45), scalingFactor, inputImage, RGBParadeGenerator::PaintMode::PaintMode_RGB, true, true, 1);
        CHECK(scope.size() == QSize(54, 45));
    }

    SECTION("Waveform")
    {
        WaveformGenerator waveform{};
        for (const QSize &size : {QSize(1, 100), QSize(4, 100), QSize(100, 1), QSize(100, 4)}) {
            CAPTURE(size);
            QImage scope =
                waveform.calculateWaveform(size, scalingFactor, inputImage, WaveformGenerator::PaintMode::PaintMode_Yellow, true, ITURec::Rec_709, 1);
            CHECK(scope.size().boundedTo(size) == scope.size());
        }
        QImage scope =
            waveform.calculateWaveform(QSize(5, 5), scalingFactor, inputImage, WaveformGenerator::PaintMode::PaintMode_Yellow, true, ITURec::Rec_709, 1);
        CHECK(scope.size() == QSize(5, 5));
    }
}
