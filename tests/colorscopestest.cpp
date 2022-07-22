#include "test_utils.hpp"

#include "scopes/colorscopes/colorconstants.h"
#include "scopes/colorscopes/vectorscopegenerator.h"
#include "scopes/colorscopes/waveformgenerator.h"

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

    SECTION("Vectorscope handles both RGB and BGR")
    {
        VectorscopeGenerator vectorscope{};
        QImage rgbScope = vectorscope.calculateVectorscope(scopeSize, inputImage, 1,
            VectorscopeGenerator::PaintMode::PaintMode_Green2,
            VectorscopeGenerator::ColorSpace::ColorSpace_YUV,
            false, 7);
        QImage bgrScope = vectorscope.calculateVectorscope(scopeSize, bgrInputImage, 1,
            VectorscopeGenerator::PaintMode::PaintMode_Green2,
            VectorscopeGenerator::ColorSpace::ColorSpace_YUV,
            false, 7);

        // both of these should be equivalent, since the vectorscope should
        // handle different pixel formats
        CHECK(rgbScope == bgrScope);
    }

    SECTION("Waveform handles both RGB and BGR")
    {
        WaveformGenerator waveform{};
        QImage rgbScope = waveform.calculateWaveform(scopeSize, inputImage,
            WaveformGenerator::PaintMode::PaintMode_Yellow,
            false, ITURec::Rec_709, 1);
        QImage bgrScope = waveform.calculateWaveform(scopeSize, bgrInputImage,
            WaveformGenerator::PaintMode::PaintMode_Yellow,
            false, ITURec::Rec_709, 1);

        CHECK(rgbScope == bgrScope);
    }
}
