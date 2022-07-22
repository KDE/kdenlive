#include "test_utils.hpp"

#include "scopes/colorscopes/vectorscopegenerator.h"

TEST_CASE("Vectorscope")
{
    // create a fully red image
    QImage inputImage(480, 320, QImage::Format_RGB32);
    inputImage.fill(Qt::red);
    VectorscopeGenerator vectorscope{};

    QSize scopeSize{256, 256};

    // test for a bug where pixels were assumed to be RGB which was not true on
    // Windows, resulting in red and blue switched. BUG: 453149
    SECTION("Vectorscope handles both RGB and BGR")
    {
        QImage rgbScope = vectorscope.calculateVectorscope(scopeSize, inputImage, 1,
            VectorscopeGenerator::PaintMode::PaintMode_Green2,
            VectorscopeGenerator::ColorSpace::ColorSpace_YUV,
            false, 7);

        QImage bgrInputImage = inputImage.convertToFormat(QImage::Format_BGR30);
        QImage bgrScope = vectorscope.calculateVectorscope(scopeSize, bgrInputImage, 1,
            VectorscopeGenerator::PaintMode::PaintMode_Green2,
            VectorscopeGenerator::ColorSpace::ColorSpace_YUV,
            false, 7);

        // both of these should be equivalent, since the vectorscope should
        // handle different pixel formats
        CHECK(rgbScope == bgrScope);
    }
}
