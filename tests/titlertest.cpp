#include "test_utils.hpp"

#include "titler/graphicsscenerectmove.h"

TEST_CASE("Title text left alignment", "[Titler]")
{
    QScopedPointer<MyTextItem> txt(new MyTextItem("Hello, world!", nullptr));
    txt->setAlignment(Qt::AlignLeft);
    QRectF origBB = txt->boundingRect();
    qreal origX = txt->x();
    txt->document()->setPlainText("Hello, longer string!");
    QRectF newBB = txt->boundingRect();
    qreal newX = txt->x();

    // make sure the left and right edges of the title are still aligned properly
    CHECK(newBB.width() > 0);
    CHECK(origBB.topRight().x() < newBB.topRight().x());
    CHECK(newX == Approx(origX));
}

TEST_CASE("Title text right alignment", "[Titler]")
{
    QScopedPointer<MyTextItem> txt(new MyTextItem("Hello, world!", nullptr));
    txt->setAlignment(Qt::AlignRight);
    QRectF origBB = txt->boundingRect();
    // origX is the left edge of the txt object
    qreal origX = txt->x();
    // origRightX is the right edge of the txt object
    qreal origRightX = origBB.width() + origX;
    txt->document()->setPlainText("Hello, longer string!");
    QRectF newBB = txt->boundingRect();
    qreal newX = txt->x();
    qreal newRightX = newBB.width() + newX;

    CHECK(newBB.width() > 0);
    CHECK(origRightX == Approx(newRightX));
    CHECK(origX > newX);
}

TEST_CASE("Title text center alignment", "[Titler]")
{
    QScopedPointer<MyTextItem> txt(new MyTextItem("short", nullptr));
    txt->setAlignment(Qt::AlignHCenter);
    QRectF origBB = txt->boundingRect();
    qreal origX = txt->x();
    qreal origRightX = origBB.width() + origX;
    qreal origCenter = (origX + origRightX) / 2;
    txt->document()->setPlainText("longer string");
    QRectF newBB = txt->boundingRect();
    qreal newX = txt->x();
    qreal newRightX = newBB.width() + newX;
    qreal newCenter = (newX + newRightX) / 2;

    CHECK(origCenter == Approx(newCenter));
    CHECK(newRightX > origRightX);
    CHECK(newX < origX);
}
