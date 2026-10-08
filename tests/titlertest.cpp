/*
    SPDX-FileCopyrightText: 2022 Eric Jiang
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "test_utils.hpp"
#include <QAbstractTextDocumentLayout>
#include <QDataStream>
#include <QGlyphRun>
#include <QRawFont>
#include <QTextLayout>
// test specific headers
#include "titler/graphicsscenerectmove.h"
#include "titler/richtextgradient.h"
#include "titler/richtextoutline.h"
#include "titler/richtextspacing.h"
#include "titler/titledocument.h"

// Alignment follows the text-layout rectangle, not its asymmetric ink/stroke envelope.
TEST_CASE("Title text left alignment", "[Titler]")
{
    QScopedPointer<MyTextItem> txt(new MyTextItem("Hello, world!", nullptr));
    txt->setAlignment(Qt::AlignLeft);
    QRectF origBB = txt->baseBoundingRect();
    qreal origX = txt->x();
    txt->document()->setPlainText("Hello, longer string!");
    QRectF newBB = txt->baseBoundingRect();
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
    QRectF origBB = txt->baseBoundingRect();
    // origX is the left edge of the txt object
    qreal origX = txt->x();
    // origRightX is the right edge of the txt object
    qreal origRightX = origBB.width() + origX;
    txt->document()->setPlainText("Hello, longer string!");
    QRectF newBB = txt->baseBoundingRect();
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
    QRectF origBB = txt->baseBoundingRect();
    qreal origX = txt->x();
    qreal origRightX = origBB.width() + origX;
    qreal origCenter = (origX + origRightX) / 2;
    txt->document()->setPlainText("longer string");
    QRectF newBB = txt->baseBoundingRect();
    qreal newX = txt->x();
    qreal newRightX = newBB.width() + newX;
    qreal newCenter = (newX + newRightX) / 2;

    CHECK(origCenter == Approx(newCenter));
    CHECK(newRightX > origRightX);
    CHECK(newX < origX);
}

// RichText: regression tests exercise the real MyTextItem callbacks.
#include "titler/richtextformat.h"
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>

namespace {
bool richTextSupported()
{
    std::unique_ptr<Mlt::Properties> metadata(pCore->getMltRepository()->metadata(mlt_service_producer_type, "kdenlivetitle"));
    return metadata && metadata->get_double("version") >= 8;
}
void richSelect(MyTextItem *item, int anchor, int position)
{
    QTextCursor cursor(item->document());
    cursor.setPosition(anchor);
    cursor.setPosition(position, QTextCursor::KeepAnchor);
    item->setTextCursor(cursor);
}
QTextCharFormat richFormat(MyTextItem *item, int position)
{
    QTextCursor cursor(item->document());
    cursor.setPosition(position);
    cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
    return cursor.charFormat();
}
} // namespace

TEST_CASE("Rich text color keeps fonts and selection", "[Titler][RichText]")
{
    if (!richTextSupported()) {
        return;
    }
    MyTextItem item(QStringLiteral("Hello amazing world"), nullptr);
    item.setTextInteractionFlags(Qt::TextEditorInteraction);
    QTextCharFormat base;
    base.setFontFamilies(QStringList{QStringLiteral("serif")});
    base.setProperty(QTextFormat::FontPixelSize, 28);
    base.setFontWeight(QFont::Normal);
    base.setForeground(QBrush(Qt::white));
    richSelect(&item, 0, 19);
    TitlerRichText::apply(&item, base);
    const auto outside = richFormat(&item, 0);

    richSelect(&item, 13, 6); // Backward selections must survive too.
    QTextCharFormat family, size, bold, italic, underline, spacing, red;
    family.setFontFamilies(QStringList{QStringLiteral("monospace")});
    size.setProperty(QTextFormat::FontPixelSize, 48);
    bold.setFontWeight(QFont::Bold);
    italic.setFontItalic(true);
    underline.setFontUnderline(true);
    spacing.setFontLetterSpacingType(QFont::AbsoluteSpacing);
    spacing.setFontLetterSpacing(3);
    red.setForeground(QBrush(Qt::red));
    for (const auto &delta : {family, size, bold, italic, underline, spacing, red}) {
        TitlerRichText::apply(&item, delta);
        REQUIRE(item.textCursor().anchor() == 13);
        REQUIRE(item.textCursor().position() == 6);
        REQUIRE(richFormat(&item, 0) == outside);
        REQUIRE(richFormat(&item, 14) == outside);
    }
    const auto styled = richFormat(&item, 8);
    REQUIRE(styled.fontFamilies().toStringList() == QStringList{QStringLiteral("monospace")});
    REQUIRE(styled.font().pixelSize() == 48);
    REQUIRE(styled.fontWeight() == QFont::Bold);
    REQUIRE(styled.fontItalic());
    REQUIRE(styled.fontUnderline());
    REQUIRE(styled.fontLetterSpacing() == 3);
    REQUIRE(styled.foreground().color() == QColor(Qt::red));

    // Changing one property across mixed runs must keep the others mixed.
    richSelect(&item, 0, 19);
    size.setProperty(QTextFormat::FontPixelSize, 36);
    TitlerRichText::apply(&item, size);
    REQUIRE(richFormat(&item, 0).fontFamilies() == outside.fontFamilies());
    REQUIRE(richFormat(&item, 8).fontFamilies() == styled.fontFamilies());
    REQUIRE(richFormat(&item, 8).foreground().color() == QColor(Qt::red));
    REQUIRE(richFormat(&item, 8).fontWeight() == QFont::Bold);

    richSelect(&item, 13, 6);
    item.setAlignment(Qt::AlignHCenter);
    REQUIRE(item.textCursor().anchor() == 13);
    REQUIRE(item.textCursor().position() == 6);
    QTextCharFormat blue;
    blue.setForeground(QBrush(Qt::blue));
    TitlerRichText::apply(&item, blue);
    item.document()->undo();
    REQUIRE(richFormat(&item, 8).foreground().color() == QColor(Qt::red));
    item.document()->redo();
    REQUIRE(richFormat(&item, 8).foreground().color() == QColor(Qt::blue));

    // A caret-only change styles new typing, not the whole object.
    richSelect(&item, 9, 9);
    TitlerRichText::apply(&item, red);
    auto cursor = item.textCursor();
    cursor.insertText(QStringLiteral("X"));
    item.setTextCursor(cursor);
    REQUIRE(item.toPlainText() == QStringLiteral("Hello amaXzing world"));
    REQUIRE(richFormat(&item, 9).foreground().color() == QColor(Qt::red));
    REQUIRE(richFormat(&item, 0).foreground().color() == QColor(Qt::white));
}

TEST_CASE("Rich text paints mixed colors outside edit mode", "[Titler][RichText]")
{
    if (!richTextSupported()) {
        return;
    }
    QGraphicsScene scene;
    auto *item = new MyTextItem(QStringLiteral("Hello world"), nullptr);
    scene.addItem(item);
    QTextCharFormat base, red;
    base.setProperty(QTextFormat::FontPixelSize, 48);
    base.setForeground(QBrush(Qt::white));
    richSelect(item, 0, 11);
    TitlerRichText::apply(item, base);
    richSelect(item, 6, 11);
    red.setForeground(QBrush(Qt::red));
    TitlerRichText::apply(item, red);
    richSelect(item, 0, 0);
    item->setTextInteractionFlags(Qt::NoTextInteraction);
    item->setSelected(false);
    QImage image(800, 200, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    scene.render(&painter, QRectF(image.rect()), scene.itemsBoundingRect().adjusted(-4, -4, 4, 4));
    painter.end();
    int reds = 0, whites = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor c = image.pixelColor(x, y);
            if (c.alpha() > 100) {
                reds += c.red() > 180 && c.green() < 80 && c.blue() < 80;
                whites += c.red() > 180 && c.green() > 180 && c.blue() > 180;
            }
        }
    }
    REQUIRE(reds > 10);
    REQUIRE(whites > 10);
}

TEST_CASE("Rich text survives title XML round trip", "[Titler][RichText]")
{
    if (!richTextSupported()) {
        return;
    }
    MyTextItem source(QStringLiteral("Hello amazing world"), nullptr);
    source.setTextInteractionFlags(Qt::TextEditorInteraction);

    QTextCharFormat base;
    base.setFontFamilies(QStringList{QStringLiteral("serif")});
    base.setProperty(QTextFormat::FontPixelSize, 28);
    base.setFontWeight(QFont::Normal);
    base.setForeground(QBrush(Qt::white));

    richSelect(&source, 0, 19);
    TitlerRichText::apply(&source, base);

    QTextCharFormat styled;
    styled.setFontFamilies(QStringList{QStringLiteral("monospace")});
    styled.setProperty(QTextFormat::FontPixelSize, 48);
    styled.setFontWeight(QFont::Bold);
    styled.setFontItalic(true);
    styled.setFontUnderline(true);
    styled.setFontLetterSpacingType(QFont::AbsoluteSpacing);
    styled.setFontLetterSpacing(3);
    styled.setForeground(QBrush(Qt::red));

    richSelect(&source, 6, 13);
    TitlerRichText::apply(&source, styled);

    QDomDocument stored = TitleDocument::xmlItem(&source, 1920, 1080);
    // RichText: serialize and parse XML, not just an in-memory DOM.
    const QString wire = stored.toString();
    REQUIRE(static_cast<bool>(stored.setContent(wire)));
    QDomElement itemElement = stored.documentElement();
    REQUIRE(itemElement.tagName() == QStringLiteral("item"));

    QDomElement content = itemElement.firstChildElement(QStringLiteral("content"));
    REQUIRE(!content.isNull());

    // Legacy fallback stays intact.
    REQUIRE(content.firstChild().nodeValue() == QStringLiteral("Hello amazing world"));

    QDomElement richText = content.firstChildElement(QStringLiteral("richtext"));
    REQUIRE(!richText.isNull());
    REQUIRE(richText.attribute(QStringLiteral("format")) == QStringLiteral("qt-html-v1"));
    REQUIRE(!richText.text().isEmpty());

    int missing = 0;
    int maxZ = 0;

    QScopedPointer<QGraphicsItem> loaded(TitleDocument::loadItemFromXml(itemElement, QString(), 1920, 1080, missing, maxZ));

    REQUIRE(!loaded.isNull());
    REQUIRE(loaded->type() == QGraphicsTextItem::Type);

    auto *text = static_cast<MyTextItem *>(loaded.data());
    REQUIRE(text->toPlainText() == QStringLiteral("Hello amazing world"));

    const QTextCharFormat outside = richFormat(text, 0);
    const QTextCharFormat inside = richFormat(text, 8);

    REQUIRE(outside.foreground().color() == QColor(Qt::white));
    REQUIRE(inside.foreground().color() == QColor(Qt::red));
    REQUIRE(inside.fontWeight() == QFont::Bold);
    REQUIRE(inside.fontItalic());
    REQUIRE(inside.fontUnderline());
    REQUIRE(inside.font().pixelSize() == 48);
    REQUIRE(inside.fontLetterSpacing() == Approx(3.0));
    REQUIRE(inside.fontLetterSpacingType() == QFont::AbsoluteSpacing);
    REQUIRE(content.attribute(QStringLiteral("font-pixel-size")).toInt() == 28);

    const QStringList insideFamilies = inside.fontFamilies().toStringList();
    REQUIRE(insideFamilies.contains(QStringLiteral("monospace")));
}

TEST_CASE("Rich text spacing handles Unicode and invalid ranges", "[Titler][RichText]")
{
    if (!richTextSupported()) {
        return;
    }
    const QString sample = QStringLiteral("A\U0001F642\nB\te\u0301");
    MyTextItem source(sample, nullptr);
    source.setTextInteractionFlags(Qt::TextEditorInteraction);
    QTextCharFormat base;
    base.setProperty(QTextFormat::FontPixelSize, 28);
    base.setFontLetterSpacingType(QFont::PercentageSpacing);
    base.setFontLetterSpacing(110);
    richSelect(&source, 0, sample.size());
    TitlerRichText::apply(&source, base);
    QTextCharFormat fraction;
    fraction.setFontLetterSpacingType(QFont::AbsoluteSpacing);
    fraction.setFontLetterSpacing(2.25);
    richSelect(&source, 1, 3); // One emoji, two UTF-16 units.
    TitlerRichText::apply(&source, fraction);
    fraction.setFontLetterSpacing(-0.5);
    const int pos = sample.indexOf(QLatin1Char('B'));
    richSelect(&source, pos, pos + 1);
    TitlerRichText::apply(&source, fraction);

    QDomDocument xml = TitleDocument::xmlItem(&source, 1920, 1080);
    const QString wire = xml.toString();
    REQUIRE(static_cast<bool>(xml.setContent(wire)));
    int missing = 0, maxZ = 0;
    QScopedPointer<QGraphicsItem> loaded(TitleDocument::loadItemFromXml(xml.documentElement(), QString(), 1920, 1080, missing, maxZ));
    REQUIRE(!loaded.isNull());
    auto *text = dynamic_cast<MyTextItem *>(loaded.data());
    REQUIRE(text != nullptr);
    REQUIRE(text->toPlainText() == sample);
    REQUIRE(richFormat(text, 0).fontLetterSpacingType() == QFont::PercentageSpacing);
    REQUIRE(richFormat(text, 0).fontLetterSpacing() == Approx(110));
    REQUIRE(richFormat(text, 1).fontLetterSpacing() == Approx(2.25));
    REQUIRE(richFormat(text, pos).fontLetterSpacing() == Approx(-0.5));

    QDomElement content = xml.documentElement().firstChildElement(QStringLiteral("content"));
    QDomElement data = content.firstChildElement(QStringLiteral("richtext-spacing"));
    REQUIRE(!data.isNull());
    data.firstChildElement(QStringLiteral("run")).setAttribute(QStringLiteral("start"), -1);
    const auto before = richFormat(text, pos);
    REQUIRE_FALSE(TitlerSpacingV1::restore(content, text->document()));
    REQUIRE(richFormat(text, pos) == before); // Reject without partial changes.
}

TEST_CASE("Rich text active inspector format follows caret and selection", "[Titler][RichText]")
{
    if (!richTextSupported()) {
        return;
    }
    MyTextItem item(QStringLiteral("Hello amazing world"), nullptr);

    QTextCharFormat base;
    base.setFontFamilies(QStringList{QStringLiteral("serif")});
    base.setProperty(QTextFormat::FontPixelSize, 28);
    base.setFontWeight(QFont::Normal);
    base.setForeground(QBrush(Qt::white));

    richSelect(&item, 0, 19);
    TitlerRichText::apply(&item, base);

    QTextCharFormat styled;
    styled.setFontFamilies(QStringList{QStringLiteral("monospace")});
    styled.setProperty(QTextFormat::FontPixelSize, 48);
    styled.setFontWeight(QFont::Bold);
    styled.setFontItalic(true);
    styled.setFontLetterSpacingType(QFont::AbsoluteSpacing);
    styled.setFontLetterSpacing(3);
    styled.setForeground(QBrush(Qt::red));

    richSelect(&item, 6, 13);
    TitlerRichText::apply(&item, styled);

    richSelect(&item, 8, 8);

    auto format = TitlerRichText::activeFormat(&item);

    REQUIRE(format.fontWeight() == QFont::Bold);
    REQUIRE(format.fontItalic());
    REQUIRE(format.foreground().color() == QColor(Qt::red));
    REQUIRE(format.fontLetterSpacing() == Approx(3.0));

    richSelect(&item, 6, 13);

    format = TitlerRichText::activeFormat(&item);

    REQUIRE(format.fontWeight() == QFont::Bold);
    REQUIRE(format.foreground().color() == QColor(Qt::red));

    REQUIRE_FALSE(TitlerRichText::selectionHasMixedCharacterFormat(&item));

    richSelect(&item, 0, 13);

    REQUIRE(TitlerRichText::selectionHasMixedCharacterFormat(&item));
}

// RichText: regression coverage for the actual rendering services.
#include <QCryptographicHash>
#include <QDir>
#include <QTextBoundaryFinder>
#include <mlt++/MltFilter.h>
#include <mlt++/MltFrame.h>

namespace {
QDomDocument makeRichTextTitle(const QString &text, int mode, int sigma = 0, bool shadow = false)
{
    MyTextItem source(text, nullptr);
    QFont defaultFont(QStringLiteral("serif"));
    defaultFont.setPixelSize(28);
    source.setFont(defaultFont);
    source.setPos(40, 40);
    source.setAlignment(Qt::AlignLeft);
    source.setData(TitleDocument::OutlineWidth, 0);
    source.updateShadow(shadow, 2, 8, 8, QColor(30, 210, 130, 128));
    source.updateTW(mode != 0, 2, mode, sigma, 37);
    QTextCharFormat base;
    base.setFont(defaultFont);
    base.setForeground(QBrush(Qt::white));
    richSelect(&source, 0, text.size());
    TitlerRichText::apply(&source, base);
    const int word = text.indexOf(QStringLiteral("amazing"));
    if (word >= 0) {
        QTextCharFormat style;
        style.setFontFamilies(QStringList{QStringLiteral("monospace")});
        style.setProperty(QTextFormat::FontPixelSize, 48);
        style.setFontWeight(QFont::Bold);
        style.setFontItalic(true);
        style.setFontUnderline(true);
        style.setForeground(QBrush(Qt::red));
        style.setFontLetterSpacingType(QFont::AbsoluteSpacing);
        style.setFontLetterSpacing(3);
        richSelect(&source, word, word + 7);
        TitlerRichText::apply(&source, style);
    }
    richSelect(&source, 0, 0);
    auto item = TitleDocument::xmlItem(&source, 1280, 720);
    QDomDocument result;
    auto root = result.createElement(QStringLiteral("kdenlivetitle"));
    root.setAttribute(QStringLiteral("width"), 1280);
    root.setAttribute(QStringLiteral("height"), 720);
    root.setAttribute(QStringLiteral("out"), 299);
    result.appendChild(root);
    root.appendChild(result.importNode(item.documentElement(), true));
    return result;
}
void setProducerTitle(Mlt::Producer &producer, const QDomDocument &title)
{
    REQUIRE(producer.is_valid());
    producer.set("xmldata", title.toByteArray().constData());
    producer.set("length", 300);
    producer.set_in_and_out(0, 299);
    producer.set("force_reload", 1);
}
QImage renderProducerFrame(Mlt::Producer &producer, int position)
{
    producer.seek(position);
    std::unique_ptr<Mlt::Frame> frame(producer.get_frame());
    REQUIRE(frame.get() != nullptr);
    REQUIRE(frame->is_valid());
    mlt_image_format format = mlt_image_rgba;
    int width = 1280, height = 720;
    auto *data = frame->get_image(format, width, height);
    REQUIRE(data != nullptr);
    REQUIRE(format == mlt_image_rgba);
    REQUIRE(width == 1280);
    REQUIRE(height == 720);
    return QImage(data, width, height, width * 4, QImage::Format_RGBA8888).copy();
}
QByteArray frameHash(const QImage &image)
{
    return QCryptographicHash::hash(QByteArray(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes()), QCryptographicHash::Sha256);
}
int countRedPixels(const QImage &image)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            auto c = image.pixelColor(x, y);
            if (c.alpha() > 100 && c.red() > 180 && c.green() < 60 && c.blue() < 60) ++count;
        }
    }
    return count;
}
} // namespace

TEST_CASE("Rich text selection scan preserves Unicode and cursor", "[Titler][RichText]")
{
    if (!richTextSupported()) {
        return;
    }
    const QString text = QString::fromUtf8("A\xCC\x88 \xF0\x9F\x8C\x88 amazing");
    MyTextItem item(text, nullptr);
    QTextCharFormat base;
    base.setForeground(QBrush(Qt::white));
    richSelect(&item, 0, text.size());
    TitlerRichText::apply(&item, base);
    REQUIRE_FALSE(TitlerRichText::selectionHasMixedCharacterFormat(&item));
    const int word = text.indexOf(QStringLiteral("amazing"));
    QTextCharFormat red;
    red.setForeground(QBrush(Qt::red));
    richSelect(&item, word, text.size());
    TitlerRichText::apply(&item, red);
    richSelect(&item, text.size(), 0);
    const auto html = item.toHtml();
    REQUIRE(TitlerRichText::selectionHasMixedCharacterFormat(&item));
    REQUIRE(item.textCursor().anchor() == text.size());
    REQUIRE(item.textCursor().position() == 0);
    REQUIRE(item.toHtml() == html);
}

TEST_CASE("Rich text colored shadow keeps premultiplied alpha", "[Titler][RichText]")
{
    if (!richTextSupported()) {
        return;
    }
    QGraphicsScene scene;
    auto *item = new MyTextItem(QStringLiteral("Test"), nullptr);
    scene.addItem(item);
    QFont font(QStringLiteral("sans-serif"));
    font.setPixelSize(32);
    item->setFont(font);
    QTextCharFormat white;
    white.setForeground(QBrush(Qt::white));
    richSelect(item, 0, 4);
    TitlerRichText::apply(item, white);
    richSelect(item, 0, 0);
    item->setPos(20, 20);
    const QString html = item->toHtml();
    item->updateShadow(true, 2, 8, 50, QColor(0, 255, 0, 96));
    QImage image(320, 200, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    scene.render(&painter, QRectF(0, 0, 320, 200), QRectF(0, 0, 320, 200));
    painter.end();
    int green = 0;
    int invalid = 0;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *row = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            invalid += qRed(row[x]) > qAlpha(row[x]) || qGreen(row[x]) > qAlpha(row[x]) || qBlue(row[x]) > qAlpha(row[x]);
            if (qAlpha(row[x]) > 10 && qAlpha(row[x]) <= 96 && qGreen(row[x]) > qRed(row[x])) ++green;
        }
    }
    REQUIRE(green > 10);
    REQUIRE(invalid == 0);
    REQUIRE(item->toHtml() == html);
}

TEST_CASE("Rich text native typewriter preserves render and seeking", "[Titler][RichTextRender]")
{
    if (!richTextSupported()) {
        return;
    }
    Mlt::Profile profile("atsc_720p_25");
    const QString text = QString::fromUtf8("A\xCC\x88 Hello amazing } \\ world\nNext line");
    const auto plain = makeRichTextTitle(text, 0);
    Mlt::Producer reference(profile, "kdenlivetitle", "");
    setProducerTitle(reference, plain);
    const auto finalImage = renderProducerFrame(reference, 240);
    REQUIRE(countRedPixels(finalImage) > 10);
    const auto finalHash = frameHash(finalImage);
    for (int mode = 1; mode <= 3; ++mode) {
        INFO("native mode " << mode);
        Mlt::Producer animated(profile, "kdenlivetitle", "");
        const auto title = makeRichTextTitle(text, mode);
        setProducerTitle(animated, title);
        const QByteArray xmlBefore(animated.get("xmldata"));
        const auto first = renderProducerFrame(animated, 0);
        REQUIRE(frameHash(first) != finalHash);
        if (mode == 1) REQUIRE(countRedPixels(first) == 0);
        REQUIRE(frameHash(renderProducerFrame(animated, 240)) == finalHash);
        REQUIRE(frameHash(renderProducerFrame(animated, 0)) == frameHash(first));
        for (int pos : {20, 2, 240, 8, 0, 120, 4, 20}) {
            const auto a = frameHash(renderProducerFrame(animated, pos));
            renderProducerFrame(animated, 240);
            REQUIRE(frameHash(renderProducerFrame(animated, pos)) == a);
        }
        REQUIRE(QByteArray(animated.get("xmldata")) == xmlBefore);
    }
    // Reloading a title after disabling animation must clear the old state.
    Mlt::Producer changed(profile, "kdenlivetitle", "");
    setProducerTitle(changed, makeRichTextTitle(text, 1));
    renderProducerFrame(changed, 0);
    setProducerTitle(changed, plain);
    REQUIRE(frameHash(renderProducerFrame(changed, 0)) == finalHash);
    REQUIRE(changed.get_int("_animated") == 0);
}

TEST_CASE("Rich text effect stack typewriter preserves rich XML", "[Titler][RichTextRender]")
{
    if (!richTextSupported()) {
        return;
    }
    Mlt::Profile profile("atsc_720p_25");
    const QString text = QStringLiteral("Hello amazing } \\ world\nSecond line");
    const auto title = makeRichTextTitle(text, 0);
    Mlt::Producer reference(profile, "kdenlivetitle", "");
    setProducerTitle(reference, title);
    const auto finalHash = frameHash(renderProducerFrame(reference, 240));
    for (int mode = 1; mode <= 3; ++mode) {
        INFO("effect stack mode " << mode);
        Mlt::Producer animated(profile, "kdenlivetitle", "");
        setProducerTitle(animated, title);
        Mlt::Filter effect(profile, "typewriter");
        REQUIRE(effect.is_valid());
        effect.set("macro_type", mode);
        effect.set("step_length", 2);
        effect.set("step_sigma", 0);
        effect.set("random_seed", 37);
        REQUIRE(animated.attach(effect) == 0);
        const QByteArray original(animated.get("xmldata"));
        const auto first = frameHash(renderProducerFrame(animated, 0));
        REQUIRE(first != finalHash);
        REQUIRE(QByteArray(animated.get("xmldata")) == original);
        REQUIRE(frameHash(renderProducerFrame(animated, 240)) == finalHash);
        REQUIRE(frameHash(renderProducerFrame(animated, 0)) == first);
        REQUIRE(QByteArray(animated.get("xmldata")) == original);
        // Timing changes must reparse from the ORIGINAL, not the last prefix.
        effect.set("step_length", 3);
        REQUIRE(frameHash(renderProducerFrame(animated, 240)) == finalHash);
        REQUIRE(QByteArray(animated.get("xmldata")) == original);
        renderProducerFrame(animated, 0);
        REQUIRE(animated.detach(effect) == 0);
        REQUIRE(frameHash(renderProducerFrame(animated, 0)) == finalHash);
    }
    // An unsupported producer must not leave the filter lock held.
    Mlt::Producer color(profile, "color", "white");
    REQUIRE(color.is_valid());
    Mlt::Filter unsupported(profile, "typewriter");
    REQUIRE(unsupported.is_valid());
    REQUIRE(color.attach(unsupported) == 0);
    REQUIRE(!renderProducerFrame(color, 0).isNull());
    REQUIRE(!renderProducerFrame(color, 1).isNull());
}

TEST_CASE("Rich text seeded timing and shadow are seek repeatable", "[Titler][RichTextRender]")
{
    if (!richTextSupported()) {
        return;
    }
    Mlt::Profile profile("atsc_720p_25");
    const auto title = makeRichTextTitle(QStringLiteral("Hello amazing world"), 1, 3, true);
    Mlt::Producer first(profile, "kdenlivetitle", ""), second(profile, "kdenlivetitle", "");
    setProducerTitle(first, title);
    setProducerTitle(second, title);
    const auto staticTitle = makeRichTextTitle(QStringLiteral("Hello amazing world"), 0, 0, true);
    Mlt::Producer reference(profile, "kdenlivetitle", "");
    setProducerTitle(reference, staticTitle);
    REQUIRE(frameHash(renderProducerFrame(first, 240)) == frameHash(renderProducerFrame(reference, 240)));
    for (int repeat = 0; repeat < 3; ++repeat) {
        for (int pos : {0, 24, 2, 240, 6, 18, 0, 9}) {
            INFO("seeded seek " << pos << " repeat " << repeat);
            REQUIRE(frameHash(renderProducerFrame(first, pos)) == frameHash(renderProducerFrame(second, pos)));
        }
    }
}

#include <QFile>
#include <QTemporaryDir>
TEST_CASE("Rich text file backed typewriter restores the source", "[Titler][RichTextRender]")
{
    if (!richTextSupported()) {
        return;
    }
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto title = makeRichTextTitle(QStringLiteral("Hello amazing world"), 0);
    const auto bytes = title.toByteArray();
    const auto filename = directory.filePath(QStringLiteral("title.kdenlivetitle"));
    QFile file(filename);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(bytes) == bytes.size());
    file.close();
    Mlt::Profile profile("atsc_720p_25");
    Mlt::Producer reference(profile, "kdenlivetitle", "");
    setProducerTitle(reference, title);
    const auto finalHash = frameHash(renderProducerFrame(reference, 240));
    Mlt::Producer animated(profile, "kdenlivetitle", filename.toUtf8().constData());
    REQUIRE(animated.is_valid());
    animated.set("length", 300);
    animated.set_in_and_out(0, 299);
    Mlt::Filter effect(profile, "typewriter");
    REQUIRE(effect.is_valid());
    effect.set("macro_type", 1);
    effect.set("step_length", 2);
    effect.set("step_sigma", 0);
    effect.set("random_seed", 37);
    REQUIRE(animated.attach(effect) == 0);
    REQUIRE(frameHash(renderProducerFrame(animated, 0)) != finalHash);
    REQUIRE(QByteArray(animated.get("_xmldata")) == bytes);
    REQUIRE(frameHash(renderProducerFrame(animated, 240)) == finalHash);
    REQUIRE(QByteArray(animated.get("_xmldata")) == bytes);
}

TEST_CASE("Rich text emits reference frames for lossless export", "[Titler][RichTextRender]")
{
    if (!richTextSupported()) {
        return;
    }
    const auto title = makeRichTextTitle(QStringLiteral("Hello amazing world"), 1);
    Mlt::Profile profile("atsc_720p_25");
    Mlt::Producer producer(profile, "kdenlivetitle", "");
    setProducerTitle(producer, title);
    const QString output = qEnvironmentVariable("RICHTEXT_TEST_ARTIFACTS");
    if (!output.isEmpty()) {
        QDir dir(output);
        REQUIRE(dir.exists());
        QFile file(dir.filePath(QStringLiteral("native.kdenlivetitle")));
        REQUIRE(file.open(QIODevice::WriteOnly));
        const auto bytes = title.toByteArray();
        REQUIRE(file.write(bytes) == bytes.size());
    }
    for (int frame : {0, 20, 39}) {
        const auto image = renderProducerFrame(producer, frame);
        REQUIRE(!image.isNull());
        if (!output.isEmpty()) {
            QFile file(QDir(output).filePath(QStringLiteral("reference-%1.rgba").arg(frame)));
            REQUIRE(file.open(QIODevice::WriteOnly));
            REQUIRE(file.write(reinterpret_cast<const char *>(image.constBits()), image.sizeInBytes()) == image.sizeInBytes());
            REQUIRE(image.save(QDir(output).filePath(QStringLiteral("reference-%1.png").arg(frame))));
        }
    }
}

TEST_CASE("Rich text selective gradient survives XML round trip", "[Titler][RichText]")
{
    if (!richTextSupported()) {
        return;
    }
    MyTextItem source(QStringLiteral("Hello amazing world"), nullptr);
    QFont font(QStringLiteral("sans-serif"));
    font.setPixelSize(32);
    source.setFont(font);
    QTextCharFormat base;
    base.setForeground(QBrush(Qt::white));
    richSelect(&source, 0, source.toPlainText().size());
    TitlerRichText::apply(&source, base);
    const QString data = QStringLiteral("#ffff0000;#ff0000ff;0;100;0");
    const auto rect = source.boundingRect();
    QTextCharFormat gradient;
    gradient.setProperty(TitlerGradientV1::Property, data);
    gradient.setForeground(QBrush(TitlerGradientV1::gradientFromString(data, int(rect.width()), int(rect.height()))));
    richSelect(&source, 6, 13);
    TitlerRichText::apply(&source, gradient);
    richSelect(&source, 0, 0);

    const QDomDocument saved = TitleDocument::xmlItem(&source, 1280, 720);
    const QDomElement content = saved.documentElement().firstChildElement(QStringLiteral("content"));
    REQUIRE(content.attribute(QStringLiteral("gradient")).isEmpty());
    const QDomElement supplement = content.firstChildElement(QStringLiteral("richtext-gradients"));
    REQUIRE_FALSE(supplement.isNull());
    const QDomElement run = supplement.firstChildElement(QStringLiteral("run"));
    REQUIRE(run.attribute(QStringLiteral("start")).toInt() == 6);
    REQUIRE(run.attribute(QStringLiteral("length")).toInt() == 7);

    int missing = 0;
    int maxZ = 0;
    QScopedPointer<QGraphicsItem> loaded(TitleDocument::loadItemFromXml(saved.documentElement(), QString(), 1280, 720, missing, maxZ));
    REQUIRE(loaded);
    auto *text = static_cast<MyTextItem *>(loaded.data());
    REQUIRE(richFormat(text, 0).property(TitlerGradientV1::Property).toString().isEmpty());
    REQUIRE(richFormat(text, 7).property(TitlerGradientV1::Property).toString() == data);
    REQUIRE(richFormat(text, 14).property(TitlerGradientV1::Property).toString().isEmpty());
    REQUIRE(richFormat(text, 0).foreground().style() == Qt::SolidPattern);
    REQUIRE(richFormat(text, 7).foreground().style() == Qt::LinearGradientPattern);
}

TEST_CASE("Rich text selective gradient renders in MLT and typewriter", "[Titler][RichText][RichTextRender]")
{
    if (!richTextSupported()) {
        return;
    }
    const auto makeTitle = [](int mode) {
        MyTextItem source(QStringLiteral("Hello amazing world"), nullptr);
        QFont font(QStringLiteral("sans-serif"));
        font.setPixelSize(40);
        source.setFont(font);
        source.setPos(40, 40);
        source.setAlignment(Qt::AlignLeft);
        source.setData(TitleDocument::OutlineWidth, 0);
        source.updateTW(mode != 0, 2, mode, 0, 37);
        QTextCharFormat base;
        base.setForeground(QBrush(Qt::white));
        richSelect(&source, 0, source.toPlainText().size());
        TitlerRichText::apply(&source, base);
        const QString data = QStringLiteral("#ffff0000;#ff0000ff;0;100;0");
        const auto rect = source.boundingRect();
        QTextCharFormat gradient;
        gradient.setProperty(TitlerGradientV1::Property, data);
        gradient.setForeground(QBrush(TitlerGradientV1::gradientFromString(data, int(rect.width()), int(rect.height()))));
        richSelect(&source, 6, 13);
        TitlerRichText::apply(&source, gradient);
        richSelect(&source, 0, 0);
        auto item = TitleDocument::xmlItem(&source, 1280, 720);
        QDomDocument result;
        auto root = result.createElement(QStringLiteral("kdenlivetitle"));
        root.setAttribute(QStringLiteral("width"), 1280);
        root.setAttribute(QStringLiteral("height"), 720);
        root.setAttribute(QStringLiteral("out"), 299);
        result.appendChild(root);
        root.appendChild(result.importNode(item.documentElement(), true));
        return result;
    };

    Mlt::Profile profile("atsc_720p_25");
    Mlt::Producer reference(profile, "kdenlivetitle", "");
    setProducerTitle(reference, makeTitle(0));
    const QImage finalImage = renderProducerFrame(reference, 240);
    int white = 0;
    int colored = 0;
    for (int y = 0; y < finalImage.height(); ++y) {
        for (int x = 0; x < finalImage.width(); ++x) {
            const QColor c = finalImage.pixelColor(x, y);
            if (c.alpha() < 80) continue;
            if (c.red() > 180 && c.green() > 180 && c.blue() > 180) ++white;
            if ((c.red() > 120 || c.blue() > 120) && c.green() < 140 && qAbs(c.red() - c.blue()) > 20) ++colored;
        }
    }
    REQUIRE(white > 20);
    REQUIRE(colored > 20);

    Mlt::Producer animated(profile, "kdenlivetitle", "");
    setProducerTitle(animated, makeTitle(1));
    REQUIRE(frameHash(renderProducerFrame(animated, 0)) != frameHash(finalImage));
    REQUIRE(frameHash(renderProducerFrame(animated, 240)) == frameHash(finalImage));
    REQUIRE(frameHash(renderProducerFrame(animated, 0)) != frameHash(finalImage));
}

namespace {
std::unique_ptr<MyTextItem> outlinedTitle(qreal width, const QString &text = QStringLiteral("Hello amazing world"))
{
    auto xml = makeRichTextTitle(text, 0);
    auto element = xml.documentElement().firstChildElement(QStringLiteral("item"));
    auto content = element.firstChildElement(QStringLiteral("content"));
    content.setAttribute(QStringLiteral("font-outline"), width);
    content.setAttribute(QStringLiteral("font-outline-color"), QStringLiteral("0,0,0,255"));
    int missing = 0, maxZ = 0;
    auto *loaded = TitleDocument::loadItemFromXml(element, QString(), 1280, 720, missing, maxZ);
    REQUIRE(loaded != nullptr);
    auto *item = dynamic_cast<MyTextItem *>(loaded);
    REQUIRE(item != nullptr);
    REQUIRE(missing == 0);
    return std::unique_ptr<MyTextItem>(item);
}

// A Qt-only copy initializes font rendering without painting the item under test.
class OutlineFontReference final : public QGraphicsTextItem
{
public:
    explicit OutlineFontReference(MyTextItem *source)
    {
        setDocument(source->document()->clone(this));
        setDefaultTextColor(source->defaultTextColor());
        setTextWidth(source->textWidth());
        setPos(source->pos());
        setTransform(source->transform());
        setTransformOriginPoint(source->transformOriginPoint());
        setRotation(source->rotation());
        setScale(source->scale());
        const qreal width = source->data(TitleDocument::OutlineWidth).toDouble();
        m_pen =
            width > 0 ? QPen(source->data(TitleDocument::OutlineColor).value<QColor>(), width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin) : QPen(Qt::NoPen);
    }

    QRectF boundingRect() const override
    {
        const qreal padding = m_pen.style() == Qt::NoPen ? 0 : m_pen.widthF() / 2;
        return QGraphicsTextItem::boundingRect().adjusted(-padding, -padding, padding, padding);
    }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        painter->save();
        painter->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        if (m_pen.style() != Qt::NoPen) {
            QAbstractTextDocumentLayout::PaintContext context;
            QAbstractTextDocumentLayout::Selection stroke;
            stroke.cursor = QTextCursor(document());
            stroke.cursor.select(QTextCursor::Document);
            stroke.format.setForeground(QBrush(Qt::transparent));
            stroke.format.setBackground(QBrush(Qt::transparent));
            stroke.format.setTextOutline(m_pen);
            stroke.format.setFontUnderline(false);
            stroke.format.setFontOverline(false);
            stroke.format.setFontStrikeOut(false);
            context.selections.append(stroke);
            document()->documentLayout()->draw(painter, context);
        }
        QGraphicsTextItem::paint(painter, option, widget);
        painter->restore();
    }

private:
    QPen m_pen;
};

QImage renderOutlinedTestItem(QGraphicsItem *item)
{
    REQUIRE(item->scene() == nullptr);
    QGraphicsScene scene;
    scene.setItemIndexMethod(QGraphicsScene::NoIndex);
    scene.addItem(item);
    QImage image(1280, 720, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    scene.render(&painter, QRectF(image.rect()), QRectF(image.rect()), Qt::IgnoreAspectRatio);
    painter.end();
    scene.removeItem(item);
    return image;
}

// Do not query glyphRuns here: the first real paint must precede glyph inspection.
QByteArray outlineStoredState(MyTextItem *item)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    const auto *doc = item->document();
    stream << doc->toHtml() << doc->defaultFont() << doc->documentMargin() << doc->textWidth();
    stream << doc->availableUndoSteps() << doc->availableRedoSteps();
    stream << item->defaultTextColor() << item->pos() << item->transform();
    stream << item->data(TitleDocument::OutlineWidth) << item->data(TitleDocument::OutlineColor) << item->data(TitleDocument::Gradient)
           << item->data(TitleDocument::LineSpacing);
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        stream << block.position() << block.text() << block.blockFormat().properties() << block.charFormat().properties();
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (fragment.isValid()) {
                stream << fragment.position() << fragment.length() << fragment.charFormat().properties();
            }
        }
    }
    REQUIRE(stream.status() == QDataStream::Ok);
    return result;
}

QImage paintOutlinedTitle(MyTextItem *item)
{
    const QByteArray before = outlineStoredState(item);
    OutlineFontReference reference(item);
    renderOutlinedTestItem(&reference);
    const QImage referenceSecond = renderOutlinedTestItem(&reference);
    REQUIRE(renderOutlinedTestItem(&reference) == referenceSecond);
    REQUIRE(outlineStoredState(item) == before);

    // Keep and check the FIRST MyTextItem image, rather than discarding it.
    const QImage first = renderOutlinedTestItem(item);
    REQUIRE(renderOutlinedTestItem(item) == first);
    REQUIRE(outlineStoredState(item) == before);
    return first;
}

QByteArray outlineLayoutState(MyTextItem *item)
{
    QByteArray result;
    QDataStream stream(&result, QIODevice::WriteOnly);
    stream << item->baseBoundingRect() << item->textWidth();
    (void)item->document()->documentLayout()->documentSize();
    for (QTextBlock block = item->document()->begin(); block.isValid(); block = block.next()) {
        const auto *layout = block.layout();
        REQUIRE(layout != nullptr);
        stream << block.position() << block.text() << layout->position() << layout->lineCount();
        for (int i = 0; i < layout->lineCount(); ++i) {
            const auto line = layout->lineAt(i);
            stream << line.textStart() << line.textLength() << line.position() << line.width() << line.ascent() << line.descent() << line.height();
        }
        const auto runs = layout->glyphRuns();
        stream << int(runs.size());
        for (const auto &run : runs) {
            const auto font = run.rawFont();
            stream << font.familyName() << font.styleName() << font.pixelSize() << int(run.flags()) << run.glyphIndexes() << run.positions();
        }
    }
    REQUIRE(stream.status() == QDataStream::Ok);
    return result;
}

void resizeOutlinedWord(MyTextItem *item, int size)
{
    const int word = item->toPlainText().indexOf(QStringLiteral("amazing"));
    REQUIRE(word >= 0);
    richSelect(item, word, word + 7);
    QTextCharFormat format;
    format.setProperty(QTextFormat::FontPixelSize, size);
    TitlerRichText::apply(item, format);
    richSelect(item, 0, 0);
}

QDomDocument outlinedTitleXml(MyTextItem *item, int mode = 0)
{
    auto xml = TitleDocument::xmlItem(item, 1280, 720);
    xml.documentElement()
        .firstChildElement(QStringLiteral("content"))
        .setAttribute(QStringLiteral("typewriter"), QStringLiteral("%1;2;%2;0;37").arg(mode != 0 ? 1 : 0).arg(mode));
    QDomDocument result;
    auto root = result.createElement(QStringLiteral("kdenlivetitle"));
    root.setAttribute(QStringLiteral("width"), 1280);
    root.setAttribute(QStringLiteral("height"), 720);
    root.setAttribute(QStringLiteral("out"), 299);
    result.appendChild(root);
    root.appendChild(result.importNode(xml.documentElement(), true));
    return result;
}

void checkOutlineFill(const QImage &fill, const QImage &outlined)
{
    REQUIRE(fill.size() == outlined.size());
    int interiors = 0, changed = 0, border = 0;
    for (int y = 1; y + 1 < fill.height(); ++y) {
        for (int x = 1; x + 1 < fill.width(); ++x) {
            const QRgb color = fill.pixel(x, y);
            if (qAlpha(color) == 255 && fill.pixel(x - 1, y) == color && fill.pixel(x + 1, y) == color && fill.pixel(x, y - 1) == color &&
                fill.pixel(x, y + 1) == color) {
                ++interiors;
                changed += outlined.pixel(x, y) != color;
            }
            border += qAlpha(color) == 0 && qAlpha(outlined.pixel(x, y)) > 0;
        }
    }
    REQUIRE(interiors > 20);
    REQUIRE(changed == 0);
    REQUIRE(border > 20);
}
} // namespace

TEST_CASE("Rich text outlines keep mixed fills after resize", "[Titler][RichTextOutline][OutlineRegression]")
{
    if (!richTextSupported()) {
        return;
    }
    auto outlined = outlinedTitle(12);
    auto fill = outlinedTitle(0);
    resizeOutlinedWord(outlined.get(), 72);
    resizeOutlinedWord(fill.get(), 72);
    outlined->setTextInteractionFlags(Qt::NoTextInteraction);
    const QImage image = paintOutlinedTitle(outlined.get());
    const QByteArray layout = outlineLayoutState(outlined.get());
    REQUIRE(countRedPixels(image) > 10);
    checkOutlineFill(paintOutlinedTitle(fill.get()), image);
    const auto html = outlined->toHtml();
    const auto cursor = outlined->textCursor();
    outlined->setTextInteractionFlags(Qt::TextEditorInteraction);
    REQUIRE(paintOutlinedTitle(outlined.get()) == image);
    REQUIRE(outlineLayoutState(outlined.get()) == layout);
    REQUIRE(outlined->toHtml() == html);
    REQUIRE(outlined->textCursor().anchor() == cursor.anchor());
    REQUIRE(outlined->textCursor().position() == cursor.position());
}

TEST_CASE("Rich text outlined resize and undo preserve layout", "[Titler][RichTextOutline]")
{
    if (!richTextSupported()) {
        return;
    }
    auto outlined = outlinedTitle(8);
    auto fill = outlinedTitle(0);
    const QImage original = paintOutlinedTitle(outlined.get());
    for (int size : {28, 96, 40, 72, 48}) {
        INFO("Font pixel size: " << size);
        resizeOutlinedWord(outlined.get(), size);
        resizeOutlinedWord(fill.get(), size);
        const QImage expected = paintOutlinedTitle(outlined.get());
        const QByteArray layout = outlineLayoutState(outlined.get());
        checkOutlineFill(paintOutlinedTitle(fill.get()), expected);
        const QRectF bounds = outlined->baseBoundingRect();
        for (int repeat = 0; repeat < 3; ++repeat) {
            outlined->doUpdateGeometry();
            REQUIRE(outlined->baseBoundingRect() == bounds);
            REQUIRE(paintOutlinedTitle(outlined.get()) == expected);
            REQUIRE(outlineLayoutState(outlined.get()) == layout);
        }
    }
    REQUIRE(paintOutlinedTitle(outlined.get()) == original);
    resizeOutlinedWord(outlined.get(), 72);
    const QImage enlarged = paintOutlinedTitle(outlined.get());
    outlined->document()->undo();
    REQUIRE(paintOutlinedTitle(outlined.get()) == original);
    outlined->document()->redo();
    REQUIRE(paintOutlinedTitle(outlined.get()) == enlarged);
}

TEST_CASE("Rich text outlined gradients survive XML and fit painting bounds", "[Titler][RichTextOutline]")
{
    if (!richTextSupported()) {
        return;
    }
    for (int width : {2, 12, 40}) {
        INFO("Outline width: " << width);
        auto source = outlinedTitle(width, QStringLiteral("A\u0308 Hello amazing world\n\u0645\u0631\u062d\u0628\u0627 \U0001f642"));
        const int word = source->toPlainText().indexOf(QStringLiteral("amazing"));
        QTextCharFormat gradient;
        gradient.setProperty(TitlerGradientV1::Property, QStringLiteral("#ffff0000;#ff0000ff;0;100;0"));
        richSelect(source.get(), word, word + 7);
        TitlerRichText::apply(source.get(), gradient);
        richSelect(source.get(), 0, 0);
        const QImage expected = paintOutlinedTitle(source.get());
        const QByteArray layout = outlineLayoutState(source.get());
        const QRectF bounds = source->boundingRect().translated(source->pos()).adjusted(-1, -1, 1, 1);
        int outside = 0;
        for (int y = 0; y < expected.height(); ++y) {
            for (int x = 0; x < expected.width(); ++x) {
                outside += qAlpha(expected.pixel(x, y)) > 0 && !bounds.contains(QPointF(x + 0.5, y + 0.5));
            }
        }
        REQUIRE(outside == 0);
        QDomDocument saved;
        REQUIRE(static_cast<bool>(saved.setContent(TitleDocument::xmlItem(source.get(), 1280, 720).toString())));
        int missing = 0, maxZ = 0;
        std::unique_ptr<QGraphicsItem> loaded(TitleDocument::loadItemFromXml(saved.documentElement(), QString(), 1280, 720, missing, maxZ));
        auto *restored = dynamic_cast<MyTextItem *>(loaded.get());
        REQUIRE(restored != nullptr);
        REQUIRE(restored->data(TitleDocument::OutlineWidth).toDouble() == Approx(width));
        REQUIRE(paintOutlinedTitle(restored) == expected);
        REQUIRE(outlineLayoutState(restored) == layout);
    }
}

TEST_CASE("Rich text outlined editor and MLT renders agree", "[Titler][RichTextOutline][RichTextRender]")
{
    if (!richTextSupported()) {
        return;
    }
    auto source = outlinedTitle(12);
    resizeOutlinedWord(source.get(), 72);
    const QImage editor = paintOutlinedTitle(source.get());
    Mlt::Profile profile("atsc_720p_25");
    Mlt::Producer producer(profile, "kdenlivetitle", "");
    setProducerTitle(producer, outlinedTitleXml(source.get()));
    const QImage exported = renderProducerFrame(producer, 240);
    REQUIRE(exported == editor);
}

TEST_CASE("Rich text outlined typewriters hide strokes and survive seeking", "[Titler][RichTextOutline][RichTextRender]")
{
    if (!richTextSupported()) {
        return;
    }
    auto source = outlinedTitle(12);
    Mlt::Profile profile("atsc_720p_25");
    Mlt::Producer reference(profile, "kdenlivetitle", "");
    const auto staticXml = outlinedTitleXml(source.get());
    setProducerTitle(reference, staticXml);
    const QImage finalImage = renderProducerFrame(reference, 240);
    REQUIRE(finalImage == paintOutlinedTitle(source.get()));
    for (int mode : {1, 2, 3}) {
        Mlt::Producer native(profile, "kdenlivetitle", "");
        setProducerTitle(native, outlinedTitleXml(source.get(), mode));
        Mlt::Producer effectProducer(profile, "kdenlivetitle", "");
        setProducerTitle(effectProducer, staticXml);
        Mlt::Filter effect(profile, "typewriter");
        REQUIRE(effect.is_valid());
        effect.set("step_length", 2);
        effect.set("step_sigma", 0);
        effect.set("macro_type", mode);
        REQUIRE(effectProducer.attach(effect) == 0);
        for (auto *producer : {&native, &effectProducer}) {
            const QByteArray xmlBefore(producer->get("xmldata"));
            const QImage first = renderProducerFrame(*producer, 0);
            if (mode == 1) {
                int leaked = 0;
                for (int y = 0; y < first.height(); ++y) {
                    for (int x = 130; x < first.width(); ++x) {
                        leaked += qAlpha(first.pixel(x, y)) > 0;
                    }
                }
                REQUIRE(leaked == 0);
                REQUIRE(first != finalImage);
            }
            REQUIRE(renderProducerFrame(*producer, 240) == finalImage);
            REQUIRE(renderProducerFrame(*producer, 0) == first);
            REQUIRE(QByteArray(producer->get("xmldata")) == xmlBefore);
        }
        REQUIRE(effectProducer.detach(effect) == 0);
        REQUIRE(renderProducerFrame(effectProducer, 0) == finalImage);
    }
}

TEST_CASE("Title XML preserves fractional text box width", "[Titler][TitleGeometryPrecision]")
{
    if (!richTextSupported()) {
        return;
    }
    MyTextItem item(QStringLiteral("Text"), nullptr);
    QFont font(QStringLiteral("serif"));
    font.setPixelSize(28);
    item.setFont(font);
    for (qreal width : {qreal(387.078125), qreal(487.8125)}) {
        item.setTextWidth(width);
        const QRectF bounds = item.baseBoundingRect();
        REQUIRE(bounds.width() == width);
        QDomDocument saved;
        REQUIRE(static_cast<bool>(saved.setContent(TitleDocument::xmlItem(&item, 1280, 720).toString())));
        const auto content = saved.documentElement().firstChildElement(QStringLiteral("content"));
        REQUIRE_FALSE(content.isNull());
        const qreal storedWidth = content.attribute(QStringLiteral("box-width")).toDouble();
        INFO("Saved box width: " << content.attribute(QStringLiteral("box-width")).toStdString());
        REQUIRE(storedWidth == width);
        REQUIRE(content.attribute(QStringLiteral("box-height")).toDouble() == bounds.height());
        REQUIRE(item.baseBoundingRect() == bounds);
    }
}

namespace {
std::unique_ptr<MyTextItem> selectiveOutlineTitle(const QString &text = QStringLiteral("AAA     BBB     CCC"))
{
    auto item = std::make_unique<MyTextItem>(text, nullptr);
    QFont font(QStringLiteral("sans-serif"));
    font.setPixelSize(40);
    item->setFont(font);
    item->setPos(40, 40);
    item->setAlignment(Qt::AlignLeft);
    item->setOutline(0, Qt::black);
    item->setTextInteractionFlags(Qt::TextEditorInteraction);
    QTextCharFormat fill;
    fill.setForeground(QBrush(Qt::white));
    richSelect(item.get(), 0, text.size());
    TitlerRichText::apply(item.get(), fill);
    richSelect(item.get(), 0, 0);
    return item;
}

QPen selectiveOutlineAt(MyTextItem *item, int position)
{
    return TitlerOutline::effectivePen(richFormat(item, position), item->defaultOutline());
}

std::unique_ptr<MyTextItem> selectiveOutlineReload(MyTextItem *source)
{
    QDomDocument saved;
    REQUIRE(static_cast<bool>(saved.setContent(TitleDocument::xmlItem(source, 1280, 720).toString())));
    int missing = 0, maxZ = 0;
    auto *loaded = TitleDocument::loadItemFromXml(saved.documentElement(), QString(), 1280, 720, missing, maxZ);
    auto *item = dynamic_cast<MyTextItem *>(loaded);
    REQUIRE(item != nullptr);
    REQUIRE(missing == 0);
    return std::unique_ptr<MyTextItem>(item);
}

int selectiveStrokePixels(const QImage &image, bool red)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.alpha() > 20) {
                count += red ? color.red() > color.green() + 80 && color.red() > color.blue() + 80
                             : color.blue() > color.green() + 80 && color.blue() > color.red() + 80;
            }
        }
    }
    return count;
}
} // namespace

TEST_CASE("Selective outlines preserve unselected formats and selection", "[Titler][SelectiveOutline]")
{
    if (!richTextSupported()) {
        return;
    }
    auto item = selectiveOutlineTitle();
    item->setOutline(4, Qt::black);
    const auto untouched = richFormat(item.get(), 0).properties();
    richSelect(item.get(), 11, 8);
    item->applyOutlineWidth(8);
    item->applyOutlineColor(QColor(255, 0, 0, 220));
    REQUIRE(item->textCursor().anchor() == 11);
    REQUIRE(item->textCursor().position() == 8);
    REQUIRE(richFormat(item.get(), 0).properties() == untouched);
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), 0)) == 4);
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), 8)) == 8);
    REQUIRE(selectiveOutlineAt(item.get(), 8).color() == QColor(255, 0, 0, 220));
    REQUIRE(richFormat(item.get(), 8).textOutline().style() == Qt::NoPen);
    REQUIRE(richFormat(item.get(), 8).foreground().color() == Qt::white);
    item->applyOutlineWidth(0);
    REQUIRE(selectiveOutlineAt(item.get(), 8).style() == Qt::NoPen);
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), 0)) == 4);
    REQUIRE(selectiveOutlineAt(item.get(), 8).color().alpha() == 220);
    item->document()->undo();
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), 8)) == 8);
    item->document()->redo();
    REQUIRE(selectiveOutlineAt(item.get(), 8).style() == Qt::NoPen);
}

TEST_CASE("Selective outline controls preserve the other mixed property", "[Titler][SelectiveOutline]")
{
    if (!richTextSupported()) {
        return;
    }
    auto item = selectiveOutlineTitle();
    richSelect(item.get(), 0, 3);
    item->applyOutlineWidth(2);
    item->applyOutlineColor(Qt::red);
    richSelect(item.get(), 8, 11);
    item->applyOutlineWidth(10);
    item->applyOutlineColor(Qt::blue);
    richSelect(item.get(), 0, 11);
    auto state = TitlerOutline::selectionState(item->textCursor(), item->defaultOutline());
    REQUIRE(state.mixedWidth);
    REQUIRE(state.mixedColor);
    item->applyOutlineColor(QColor(20, 200, 40, 96));
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), 0)) == 2);
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), 8)) == 10);
    REQUIRE(selectiveOutlineAt(item.get(), 0).color() == QColor(20, 200, 40, 96));
    REQUIRE(selectiveOutlineAt(item.get(), 8).color() == QColor(20, 200, 40, 96));
    item->document()->undo();
    item->applyOutlineWidth(6);
    REQUIRE(selectiveOutlineAt(item.get(), 0).color() == Qt::red);
    REQUIRE(selectiveOutlineAt(item.get(), 8).color() == Qt::blue);
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), 0)) == 6);
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), 8)) == 6);
    const auto before = outlineStoredState(item.get());
    (void)TitlerOutline::selectionState(item->textCursor(), item->defaultOutline());
    REQUIRE(outlineStoredState(item.get()) == before);
}

TEST_CASE("Selective outlines support caret insertion and whole object editing", "[Titler][SelectiveOutline]")
{
    if (!richTextSupported()) {
        return;
    }
    auto item = selectiveOutlineTitle();
    const int end = item->toPlainText().size();
    richSelect(item.get(), end, end);
    item->applyOutlineWidth(7);
    item->applyOutlineColor(Qt::red);
    REQUIRE_FALSE(TitlerOutline::hasOverrides(item->document()));
    QTextCursor cursor = item->textCursor();
    cursor.insertText(QStringLiteral("Z"));
    item->setTextCursor(cursor);
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), end)) == 7);
    REQUIRE(selectiveOutlineAt(item.get(), end).color() == Qt::red);
    REQUIRE(selectiveOutlineAt(item.get(), 0).style() == Qt::NoPen);
    item->setTextInteractionFlags(Qt::NoTextInteraction);
    item->applyOutlineWidth(3);
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), 0)) == 3);
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), end)) == 3);
    REQUIRE(selectiveOutlineAt(item.get(), end).color() == Qt::red);
    item->document()->undo();
    REQUIRE(selectiveOutlineAt(item.get(), 0).style() == Qt::NoPen);
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), end)) == 7);
    item->document()->redo();
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), 0)) == 3);
}

TEST_CASE("Selective outlines preserve exact alpha and fractional width in XML", "[Titler][SelectiveOutline]")
{
    if (!richTextSupported()) {
        return;
    }
    auto item = selectiveOutlineTitle(QStringLiteral("A\u0308 \U0001f642 \u0645\u0631\u062d\u0628\u0627\namazing end"));
    item->setOutline(4, Qt::black);
    const int start = item->toPlainText().indexOf(QStringLiteral("amazing"));
    richSelect(item.get(), start, start + 7);
    item->applyOutlineWidth(3.5);
    item->applyOutlineColor(QColor(255, 0, 0, 220));
    richSelect(item.get(), 0, 2);
    item->applyOutlineWidth(0);
    richSelect(item.get(), 0, 0);
    for (int cycle = 0; cycle < 8; ++cycle) {
        INFO("XML round trip: " << cycle);
        item = selectiveOutlineReload(item.get());
        REQUIRE(selectiveOutlineAt(item.get(), 0).style() == Qt::NoPen);
        REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), start)) == 3.5);
        REQUIRE(selectiveOutlineAt(item.get(), start).color() == QColor(255, 0, 0, 220));
        REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), start + 8)) == 4);
    }
    for (int alpha : {0, 1, 96, 127, 128, 219, 220, 254, 255}) {
        richSelect(item.get(), start, start + 7);
        item->applyOutlineColor(QColor(10, 20, 30, alpha));
        item = selectiveOutlineReload(item.get());
        REQUIRE(selectiveOutlineAt(item.get(), start).color() == QColor(10, 20, 30, alpha));
    }
}

TEST_CASE("Selective outline metadata rejects invalid ranges without partial application", "[Titler][SelectiveOutline]")
{
    if (!richTextSupported()) {
        return;
    }
    auto item = selectiveOutlineTitle(QStringLiteral("A\U0001f642B"));
    richSelect(item.get(), 0, 1);
    item->applyOutlineWidth(2);
    richSelect(item.get(), 1, 3);
    item->applyOutlineWidth(5);
    item->applyOutlineColor(Qt::blue);
    const auto saved = TitleDocument::xmlItem(item.get(), 1280, 720).toString();
    for (const auto &change : QVector<QPair<QString, QString>>{{QStringLiteral("start"), QStringLiteral("2")},
                                                               {QStringLiteral("start"), QStringLiteral("0")},
                                                               {QStringLiteral("length"), QStringLiteral("2147483647")},
                                                               {QStringLiteral("length"), QStringLiteral("-1")},
                                                               {QStringLiteral("width"), QStringLiteral("nan")},
                                                               {QStringLiteral("width"), QStringLiteral("201")},
                                                               {QStringLiteral("color"), QStringLiteral("#xxxxxxxx")}}) {
        INFO(change.first.toStdString() << ": " << change.second.toStdString());
        QDomDocument corrupted;
        REQUIRE(static_cast<bool>(corrupted.setContent(saved)));
        const auto content = corrupted.documentElement().firstChildElement(QStringLiteral("content"));
        auto data = content.firstChildElement(QStringLiteral("richtext-outlines"));
        auto second = data.firstChildElement(QStringLiteral("run")).nextSiblingElement(QStringLiteral("run"));
        REQUIRE_FALSE(second.isNull());
        second.setAttribute(change.first, change.second);
        QTextDocument restored;
        restored.setHtml(item->toHtml());
        const int undoBefore = restored.availableUndoSteps();
        REQUIRE_FALSE(TitlerOutline::restore(content, &restored));
        REQUIRE_FALSE(TitlerOutline::hasOverrides(&restored));
        REQUIRE(restored.availableUndoSteps() == undoBefore);
    }
    QDomDocument duplicated;
    REQUIRE(static_cast<bool>(duplicated.setContent(saved)));
    auto content = duplicated.documentElement().firstChildElement(QStringLiteral("content"));
    content.appendChild(content.firstChildElement(QStringLiteral("richtext-outlines")).cloneNode(true));
    QTextDocument restored;
    restored.setHtml(item->toHtml());
    REQUIRE_FALSE(TitlerOutline::restore(content, &restored));
    REQUIRE_FALSE(TitlerOutline::hasOverrides(&restored));
}

TEST_CASE("Selective outlines follow text edits and leave unstyled regions alone", "[Titler][SelectiveOutline]")
{
    if (!richTextSupported()) {
        return;
    }
    auto item = selectiveOutlineTitle();
    richSelect(item.get(), 8, 11);
    item->applyOutlineWidth(10);
    item->applyOutlineColor(Qt::red);
    const QFont fontBefore = richFormat(item.get(), 8).font();
    QTextCursor cursor(item->document());
    cursor.setPosition(0);
    cursor.insertText(QStringLiteral("prefix "));
    REQUIRE(selectiveOutlineAt(item.get(), 8).style() == Qt::NoPen);
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), 15)) == 10);
    REQUIRE(richFormat(item.get(), 15).font() == fontBefore);
    auto restored = selectiveOutlineReload(item.get());
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(restored.get(), 15)) == 10);
    item->document()->undo();
    REQUIRE(item->toPlainText() == QStringLiteral("AAA     BBB     CCC"));
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(item.get(), 8)) == 10);
    richSelect(item.get(), 8, 11);
    const QByteArray layout = outlineLayoutState(item.get());
    for (qreal width : {qreal(2), qreal(12), qreal(40), qreal(0)}) {
        item->applyOutlineWidth(width);
        REQUIRE(outlineLayoutState(item.get()) == layout);
        REQUIRE(item->boundingRect().contains(item->baseBoundingRect()));
    }
}

TEST_CASE("Selective outlines paint only selected strokes without changing the fill", "[Titler][SelectiveOutline]")
{
    if (!richTextSupported()) {
        return;
    }
    auto item = selectiveOutlineTitle();
    const QImage fill = renderOutlinedTestItem(item.get());
    richSelect(item.get(), 8, 11);
    item->applyOutlineWidth(8);
    item->applyOutlineColor(Qt::red);
    richSelect(item.get(), 16, 19);
    item->applyOutlineWidth(4);
    item->applyOutlineColor(Qt::blue);
    richSelect(item.get(), 0, 0);
    const auto before = outlineStoredState(item.get());
    const QImage outlined = renderOutlinedTestItem(item.get());
    REQUIRE(outlineStoredState(item.get()) == before);
    REQUIRE(selectiveStrokePixels(outlined, true) > 20);
    REQUIRE(selectiveStrokePixels(outlined, false) > 20);
    checkOutlineFill(fill, outlined);
    const QImage unstyled = outlined.copy(QRect(35, 30, 90, 80));
    REQUIRE(selectiveStrokePixels(unstyled, true) == 0);
    REQUIRE(selectiveStrokePixels(unstyled, false) == 0);
    REQUIRE(selectiveOutlineAt(item.get(), 0).style() == Qt::NoPen);
    const QRectF bounds = item->boundingRect().translated(item->pos()).adjusted(-1, -1, 1, 1);
    int outside = 0;
    for (int y = 0; y < outlined.height(); ++y) {
        for (int x = 0; x < outlined.width(); ++x) {
            outside += qAlpha(outlined.pixel(x, y)) > 0 && !bounds.contains(QPointF(x + 0.5, y + 0.5));
        }
    }
    REQUIRE(outside == 0);
    richSelect(item.get(), 8, 11);
    item->applyOutlineWidth(0);
    richSelect(item.get(), 0, 0);
    const auto removed = renderOutlinedTestItem(item.get());
    REQUIRE(selectiveStrokePixels(removed, true) == 0);
    REQUIRE(selectiveStrokePixels(removed, false) > 20);
}

TEST_CASE("Selective outlines render through MLT and typewriter seeking", "[Titler][SelectiveOutline][RichTextRender]")
{
    if (!richTextSupported()) {
        return;
    }
    auto source = selectiveOutlineTitle();
    richSelect(source.get(), 8, 11);
    source->applyOutlineWidth(8);
    source->applyOutlineColor(Qt::red);
    richSelect(source.get(), 16, 19);
    source->applyOutlineWidth(4);
    source->applyOutlineColor(Qt::blue);
    richSelect(source.get(), 0, 0);
    Mlt::Profile profile("atsc_720p_25");
    Mlt::Producer reference(profile, "kdenlivetitle", "");
    const auto staticXml = outlinedTitleXml(source.get());
    setProducerTitle(reference, staticXml);
    const QImage finalImage = renderProducerFrame(reference, 240);
    REQUIRE(selectiveStrokePixels(finalImage, true) > 20);
    REQUIRE(selectiveStrokePixels(finalImage, false) > 20);
    for (int mode : {1, 2, 3}) {
        Mlt::Producer native(profile, "kdenlivetitle", "");
        setProducerTitle(native, outlinedTitleXml(source.get(), mode));
        Mlt::Producer effectProducer(profile, "kdenlivetitle", "");
        setProducerTitle(effectProducer, staticXml);
        Mlt::Filter effect(profile, "typewriter");
        REQUIRE(effect.is_valid());
        effect.set("step_length", 2);
        effect.set("step_sigma", 0);
        effect.set("macro_type", mode);
        REQUIRE(effectProducer.attach(effect) == 0);
        for (auto *producer : {&native, &effectProducer}) {
            const QByteArray before(producer->get("xmldata"));
            const QImage first = renderProducerFrame(*producer, 0);
            if (mode == 1) {
                REQUIRE(selectiveStrokePixels(first, true) == 0);
                REQUIRE(selectiveStrokePixels(first, false) == 0);
            }
            const QImage last = renderProducerFrame(*producer, 240);
            REQUIRE(selectiveStrokePixels(last, true) > 20);
            REQUIRE(selectiveStrokePixels(last, false) > 20);
            const QImage sought = renderProducerFrame(*producer, 0);
            REQUIRE(sought.size() == first.size());
            REQUIRE((selectiveStrokePixels(sought, true) > 20) == (selectiveStrokePixels(first, true) > 20));
            REQUIRE((selectiveStrokePixels(sought, false) > 20) == (selectiveStrokePixels(first, false) > 20));
            REQUIRE(QByteArray(producer->get("xmldata")) == before);
        }
    }
}

#include <QDataStream>
#include <QGlyphRun>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QRawFont>
#include <QTextLayout>
#include <memory>

namespace OutlineBoundaryChecks {
constexpr int Padding = 128;

inline std::unique_ptr<QTextDocument> makeDocument(int start, int end, qreal width, int alpha = 255)
{
    auto document = std::make_unique<QTextDocument>();
    QFont font(QStringLiteral("sans-serif"));
    font.setPixelSize(60);
    font.setHintingPreference(QFont::PreferNoHinting);
    document->setDefaultFont(font);
    document->setDocumentMargin(0);
    document->setPlainText(QStringLiteral("ABCDEF"));
    QTextCursor cursor(document.get());
    cursor.select(QTextCursor::Document);
    QTextCharFormat fill;
    fill.setForeground(QBrush(Qt::white));
    fill.setTextOutline(QPen(Qt::NoPen));
    cursor.mergeCharFormat(fill);
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    QTextCharFormat outline;
    outline.setProperty(TitlerOutline::WidthProperty, width);
    outline.setProperty(TitlerOutline::ColorProperty, QColor(255, 0, 0, alpha));
    cursor.mergeCharFormat(outline);
    (void)document->documentLayout()->documentSize();
    return document;
}

inline QByteArray storedState(const QTextDocument *document)
{
    QByteArray bytes;
    QDataStream stream(&bytes, QIODevice::WriteOnly);
    stream << document->toHtml() << document->textWidth() << document->defaultFont() << document->documentMargin() << document->availableUndoSteps()
           << document->availableRedoSteps();
    for (auto block = document->begin(); block.isValid(); block = block.next()) {
        stream << block.position() << block.blockFormat().properties() << block.charFormat().properties();
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (fragment.isValid()) {
                stream << fragment.position() << fragment.length() << fragment.charFormat().properties();
            }
        }
    }
    return bytes;
}

inline QImage render(QTextDocument *document, int visible = -1)
{
    QImage image(900, 450, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    painter.translate(Padding, Padding);
    TitlerOutline::paint(&painter, document, QPen(Qt::NoPen), visible);
    painter.end();
    return image;
}

// Build the expected stroke area independently of fragment iteration, outline
// metadata lookup and the helper's drawing method. These Latin ranges contain
// whole glyphs; a partial ligature is not a valid input for this control.
inline QPainterPath expectedArea(QTextDocument *document, int start, int end, qreal width, bool &partial)
{
    QPainterPath glyphs;
    glyphs.setFillRule(Qt::WindingFill);
    partial = false;
    const auto block = document->begin();
    const auto *layout = block.layout();
    for (const auto &run : layout->glyphRuns(start, end - start)) {
        partial |= run.flags().testFlag(QGlyphRun::SplitLigature);
        const auto indexes = run.glyphIndexes();
        const auto positions = run.positions();
        for (int i = 0; i < indexes.size(); ++i) {
            glyphs.addPath(run.rawFont().pathForGlyph(indexes.at(i)).translated(layout->position() + positions.at(i)));
        }
    }
    QPainterPathStroker stroker;
    stroker.setWidth(width);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    return stroker.createStroke(glyphs);
}

struct Coverage
{
    int witnesses{0};
    int missing{0};
    bool partial{false};
    bool sourceUnchanged{false};
    QImage actual;
    QImage reference;
};

inline Coverage coverage(int start, int end, qreal width, int visible = -1)
{
    auto document = makeDocument(start, end, width);
    const auto before = storedState(document.get());
    Coverage result;
    const auto area = expectedArea(document.get(), start, visible < 0 ? end : qMin(end, visible), width, result.partial);
    result.actual = render(document.get(), visible);
    result.sourceUnchanged = storedState(document.get()) == before;
    result.reference = QImage(result.actual.size(), result.actual.format());
    result.reference.fill(Qt::transparent);
    {
        QPainter painter(&result.reference);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(Padding, Padding);
        painter.fillPath(area, Qt::red);
    }
    const QRect bounds = area.boundingRect().translated(Padding, Padding).toAlignedRect().intersected(result.actual.rect().adjusted(2, 2, -2, -2));
    for (int y = bounds.top(); y <= bounds.bottom(); ++y) {
        for (int x = bounds.left(); x <= bounds.right(); ++x) {
            // Test fully covered interiors, not platform-dependent edge AA.
            if (qAlpha(result.reference.pixel(x, y)) == 255 && qAlpha(result.reference.pixel(x - 2, y)) == 255 &&
                qAlpha(result.reference.pixel(x + 2, y)) == 255 && qAlpha(result.reference.pixel(x, y - 2)) == 255 &&
                qAlpha(result.reference.pixel(x, y + 2)) == 255) {
                ++result.witnesses;
                result.missing += qAlpha(result.actual.pixel(x, y)) != 255;
            }
        }
    }
    return result;
}

inline std::unique_ptr<QTextDocument> splitFormats(QTextDocument *source)
{
    std::unique_ptr<QTextDocument> result(source->clone());
    for (int i = 0; i < result->characterCount() - 1; ++i) {
        QTextCursor cursor(result.get());
        cursor.setPosition(i);
        cursor.setPosition(i + 1, QTextCursor::KeepAnchor);
        QTextCharFormat metadata;
        metadata.setProperty(QTextFormat::UserProperty + 1000, i + 1);
        cursor.mergeCharFormat(metadata);
    }
    (void)result->documentLayout()->documentSize();
    return result;
}
} // namespace OutlineBoundaryChecks

TEST_CASE("Selective outline covers glyph contours at every contiguous Latin range", "[Titler][SelectiveOutlineBoundary]")
{
    if (!richTextSupported()) {
        return;
    }
    for (int start = 0; start < 6; ++start) {
        for (int end = start + 1; end <= 6; ++end) {
            for (qreal width : {qreal(12), qreal(24)}) {
                INFO("Range: " << start << ":" << end << ", width: " << width);
                const auto result = OutlineBoundaryChecks::coverage(start, end, width);
                REQUIRE_FALSE(result.partial);
                REQUIRE(result.witnesses >= 10);
                REQUIRE(result.sourceUnchanged);
                REQUIRE(result.missing == 0);
            }
        }
    }
    const auto reveal = OutlineBoundaryChecks::coverage(0, 6, 24, 3);
    REQUIRE_FALSE(reveal.partial);
    REQUIRE(reveal.witnesses >= 10);
    REQUIRE(reveal.sourceUnchanged);
    REQUIRE(reveal.missing == 0);
}

TEST_CASE("Selective outline pixels ignore nonpainting fragment boundaries", "[Titler][SelectiveOutlineBoundary]")
{
    if (!richTextSupported()) {
        return;
    }
    for (int alpha : {255, 128}) {
        INFO("Stroke alpha: " << alpha);
        auto whole = OutlineBoundaryChecks::makeDocument(0, 6, 24, alpha);
        auto split = OutlineBoundaryChecks::splitFormats(whole.get());
        const auto beforeA = OutlineBoundaryChecks::storedState(whole.get());
        const auto beforeB = OutlineBoundaryChecks::storedState(split.get());
        const auto a = OutlineBoundaryChecks::render(whole.get());
        const auto b = OutlineBoundaryChecks::render(split.get());
        REQUIRE(a == b);
        REQUIRE(OutlineBoundaryChecks::render(whole.get()) == a);
        REQUIRE(OutlineBoundaryChecks::render(split.get()) == b);
        REQUIRE(OutlineBoundaryChecks::storedState(whole.get()) == beforeA);
        REQUIRE(OutlineBoundaryChecks::storedState(split.get()) == beforeB);
    }
}

TEST_CASE("Touching selective outlines survive XML and MLT rendering", "[Titler][SelectiveOutlineBoundary][RichTextRender]")
{
    if (!richTextSupported()) {
        return;
    }
    auto source = selectiveOutlineTitle(QStringLiteral("ABCDEF"));
    richSelect(source.get(), 1, 4);
    source->applyOutlineWidth(24);
    source->applyOutlineColor(Qt::red);
    richSelect(source.get(), 4, 6);
    source->applyOutlineWidth(6);
    source->applyOutlineColor(Qt::blue);
    richSelect(source.get(), 0, 0);
    source->setTextInteractionFlags(Qt::NoTextInteraction);
    const auto before = outlineStoredState(source.get());
    const auto editor = renderOutlinedTestItem(source.get());
    REQUIRE(outlineStoredState(source.get()) == before);
    auto restored = selectiveOutlineReload(source.get());
    REQUIRE(selectiveOutlineAt(restored.get(), 0).style() == Qt::NoPen);
    REQUIRE(selectiveOutlineAt(restored.get(), 1).color() == Qt::red);
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(restored.get(), 1)) == 24);
    REQUIRE(selectiveOutlineAt(restored.get(), 4).color() == Qt::blue);
    REQUIRE(TitlerOutline::width(selectiveOutlineAt(restored.get(), 4)) == 6);
    REQUIRE(renderOutlinedTestItem(restored.get()) == editor);
    Mlt::Profile profile("atsc_720p_25");
    Mlt::Producer producer(profile, "kdenlivetitle", "");
    setProducerTitle(producer, outlinedTitleXml(source.get()));
    REQUIRE(renderProducerFrame(producer, 240) == editor);
}

// Alignment uses the text layout, while painting bounds also contain glyph
// overhang and outline padding. Test the two responsibilities independently.
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

namespace RichTextAlignmentContract {
constexpr qreal epsilon = 0.000001;
bool same(qreal a, qreal b)
{
    return std::abs(a - b) <= epsilon;
}
QJsonArray rect(const QRectF &r)
{
    return {r.x(), r.y(), r.width(), r.height()};
}
qreal anchor(MyTextItem &item, Qt::Alignment alignment)
{
    const QRectF layout = item.baseBoundingRect();
    const qreal x = alignment.testFlag(Qt::AlignRight) ? layout.right() : alignment.testFlag(Qt::AlignHCenter) ? layout.center().x() : layout.left();
    return item.mapToParent(QPointF(x, 0)).x();
}
QJsonObject snapshot(MyTextItem &item, Qt::Alignment alignment)
{
    return {{QStringLiteral("text"), item.toPlainText()},
            {QStringLiteral("alignment"), int(alignment)},
            {QStringLiteral("layout"), rect(item.baseBoundingRect())},
            {QStringLiteral("paintBounds"), rect(item.boundingRect())},
            {QStringLiteral("position"), QJsonArray{item.x(), item.y()}},
            {QStringLiteral("layoutAnchorX"), anchor(item, alignment)},
            {QStringLiteral("oldWidthOnlyCenter"), item.x() + item.boundingRect().width() / 2}};
}
void record(const QString &name, const QJsonObject &data)
{
    // Normal upstream test execution does not require a report directory.
    const QString root = qEnvironmentVariable("RICHTEXT_ALIGNMENT_REPORT_DIR");
    if (root.isEmpty()) {
        return;
    }
    REQUIRE(QDir(root).exists());
    QFile file(QDir(root).filePath(name + QStringLiteral(".json")));
    REQUIRE(file.open(QIODevice::WriteOnly | QIODevice::NewOnly));
    const QByteArray bytes = QJsonDocument(data).toJson(QJsonDocument::Indented);
    REQUIRE(file.write(bytes) == bytes.size());
}
} // namespace RichTextAlignmentContract

TEST_CASE("Rich text center alignment distinguishes layout from ink bounds", "[Titler][AlignmentContract]")
{
    if (!richTextSupported()) {
        return;
    }
    using namespace RichTextAlignmentContract;
    MyTextItem item(QStringLiteral("short"), nullptr);
    item.setAlignment(Qt::AlignHCenter);
    const QJsonObject before = snapshot(item, Qt::AlignHCenter);
    item.document()->setPlainText(QStringLiteral("longer string"));
    const QJsonObject after = snapshot(item, Qt::AlignHCenter);
    const qreal layoutDelta = after[QStringLiteral("layoutAnchorX")].toDouble() - before[QStringLiteral("layoutAnchorX")].toDouble();
    const qreal oldDelta = after[QStringLiteral("oldWidthOnlyCenter")].toDouble() - before[QStringLiteral("oldWidthOnlyCenter")].toDouble();
    const auto beforeLayout = before[QStringLiteral("layout")].toArray();
    const auto afterLayout = after[QStringLiteral("layout")].toArray();
    const auto beforePaint = before[QStringLiteral("paintBounds")].toArray();
    const auto afterPaint = after[QStringLiteral("paintBounds")].toArray();
    // Exact decomposition of the OLD width-only measurement. This includes
    // the layout rectangle's origin instead of assuming that every rect starts at zero.
    const qreal paddingDelta = ((afterPaint[2].toDouble() - afterLayout[2].toDouble()) - (beforePaint[2].toDouble() - beforeLayout[2].toDouble())) / 2 -
                               (afterLayout[0].toDouble() - beforeLayout[0].toDouble());
    const bool stable = same(layoutDelta, 0);
    const bool explained = same(oldDelta - layoutDelta, paddingDelta);
    record(QStringLiteral("original-center-measurements"), {{QStringLiteral("before"), before},
                                                            {QStringLiteral("after"), after},
                                                            {QStringLiteral("layoutDelta"), layoutDelta},
                                                            {QStringLiteral("oldMetricDelta"), oldDelta},
                                                            {QStringLiteral("paintPaddingContribution"), paddingDelta},
                                                            {QStringLiteral("layoutStable"), stable},
                                                            {QStringLiteral("oldMetricExplained"), explained}});
    CHECK(stable);
    CHECK(explained);
    CHECK(item.boundingRect().contains(item.baseBoundingRect()));
    // Detector control: an actual one-pixel position change must be rejected.
    const qreal fixed = anchor(item, Qt::AlignHCenter);
    item.setX(item.x() + 1);
    CHECK_FALSE(same(anchor(item, Qt::AlignHCenter), fixed));
    item.setX(item.x() - 1);
    CHECK(same(anchor(item, Qt::AlignHCenter), fixed));
}

TEST_CASE("Rich text layout anchors survive text changes with ink overhang", "[Titler][AlignmentContract]")
{
    if (!richTextSupported()) {
        return;
    }
    using namespace RichTextAlignmentContract;
    QJsonArray rows;
    int failures = 0;
    const QList<QPair<QString, QString>> texts{{QStringLiteral("short"), QStringLiteral("longer string")},
                                               {QStringLiteral("fjy"), QStringLiteral("fjy es more")},
                                               {QStringLiteral("e"), QStringLiteral("es")}};
    for (const QString &family : {QStringLiteral("sans-serif"), QStringLiteral("serif")}) {
        for (int size : {40, 96}) {
            for (int outline : {0, 12}) {
                for (const Qt::Alignment alignment : {Qt::Alignment(Qt::AlignLeft), Qt::Alignment(Qt::AlignHCenter), Qt::Alignment(Qt::AlignRight)}) {
                    for (const auto &text : texts) {
                        MyTextItem item(text.first, nullptr);
                        QFont font(family);
                        font.setPixelSize(size);
                        font.setItalic(text.first == QLatin1String("fjy"));
                        item.setFont(font);
                        item.setAlignment(alignment);
                        item.setOutline(outline, QColor(Qt::white));
                        item.setPos(300, 120);
                        const auto before = snapshot(item, alignment);
                        const qreal fixed = anchor(item, alignment);
                        item.document()->setPlainText(text.second);
                        const auto longer = snapshot(item, alignment);
                        const bool grows = item.baseBoundingRect().width() > before[QStringLiteral("layout")].toArray()[2].toDouble();
                        const bool first = same(anchor(item, alignment), fixed);
                        const bool contained = item.boundingRect().contains(item.baseBoundingRect());
                        item.document()->setPlainText(text.first);
                        const auto restored = snapshot(item, alignment);
                        const bool second = same(anchor(item, alignment), fixed);
                        const bool ok = first && second && grows && contained;
                        failures += !ok;
                        rows.append(QJsonObject{{QStringLiteral("family"), family},
                                                {QStringLiteral("size"), size},
                                                {QStringLiteral("outline"), outline},
                                                {QStringLiteral("before"), before},
                                                {QStringLiteral("longer"), longer},
                                                {QStringLiteral("restored"), restored},
                                                {QStringLiteral("passed"), ok}});
                    }
                }
            }
        }
    }
    record(QStringLiteral("layout-anchor-matrix"),
           {{QStringLiteral("cases"), rows.size()}, {QStringLiteral("failures"), failures}, {QStringLiteral("rows"), rows}});
    REQUIRE(rows.size() == 72);
    REQUIRE(failures == 0);
}

TEST_CASE("Outline changes preserve the layout anchor and text width", "[Titler][AlignmentContract]")
{
    if (!richTextSupported()) {
        return;
    }
    using namespace RichTextAlignmentContract;
    QJsonArray rows;
    int failures = 0;
    for (const QString &family : {QStringLiteral("sans-serif"), QStringLiteral("serif")}) {
        for (int size : {40, 96}) {
            for (const Qt::Alignment alignment : {Qt::Alignment(Qt::AlignLeft), Qt::Alignment(Qt::AlignHCenter), Qt::Alignment(Qt::AlignRight)}) {
                MyTextItem item(QStringLiteral("fjy es"), nullptr);
                // Use the same character-format helper as the Titler font controls.
                // QGraphicsTextItem::setFont only changes the document default; it
                // does not perform MyTextItem's automatic-width update on its own.
                QTextCharFormat familyFormat;
                familyFormat.setFontFamilies(QStringList{family});
                familyFormat.setFontStyleName(QString());
                TitlerRichText::apply(&item, familyFormat);
                QTextCharFormat sizeFormat;
                sizeFormat.setProperty(QTextFormat::FontPixelSize, size);
                TitlerRichText::apply(&item, sizeFormat);
                QTextCharFormat italicFormat;
                italicFormat.setFontItalic(true);
                italicFormat.setFontStyleName(QString());
                TitlerRichText::apply(&item, italicFormat);
                item.setAlignment(alignment);
                item.setTextInteractionFlags(Qt::TextEditorInteraction);
                item.setPos(300, 120);
                const qreal fixed = anchor(item, alignment);
                const qreal width = item.baseBoundingRect().width();
                const auto before = snapshot(item, alignment);
                item.setOutline(12, QColor(Qt::white));
                bool ok = same(anchor(item, alignment), fixed) && same(item.baseBoundingRect().width(), width);
                QTextCursor cursor(item.document());
                cursor.setPosition(0);
                cursor.setPosition(1, QTextCursor::KeepAnchor);
                item.setTextCursor(cursor);
                item.applyOutlineWidth(24);
                item.applyOutlineColor(QColor(Qt::red));
                ok = ok && same(anchor(item, alignment), fixed) && same(item.baseBoundingRect().width(), width);
                const auto outlined = snapshot(item, alignment);
                item.applyOutlineWidth(0);
                item.setOutline(0, QColor(Qt::white));
                ok = ok && same(anchor(item, alignment), fixed) && same(item.baseBoundingRect().width(), width);
                ok = ok && item.toPlainText() == QStringLiteral("fjy es");
                failures += !ok;
                rows.append(QJsonObject{{QStringLiteral("family"), family},
                                        {QStringLiteral("size"), size},
                                        {QStringLiteral("before"), before},
                                        {QStringLiteral("outlined"), outlined},
                                        {QStringLiteral("after"), snapshot(item, alignment)},
                                        {QStringLiteral("passed"), ok}});
            }
        }
    }
    record(QStringLiteral("outline-anchor-matrix"),
           {{QStringLiteral("cases"), rows.size()}, {QStringLiteral("failures"), failures}, {QStringLiteral("rows"), rows}});
    REQUIRE(rows.size() == 12);
    REQUIRE(failures == 0);
}

// Check the initializer itself before any outline operation. An independent
// QTextDocument receives the same delta without MyTextItem's geometry callbacks.
#include <memory>
TEST_CASE("Titler character font edits settle layout before outline edits", "[Titler][AlignmentContract]")
{
    if (!richTextSupported()) {
        return;
    }
    using namespace RichTextAlignmentContract;
    QJsonArray rows;
    int cases = 0, failures = 0;
    for (const QString &family : {QStringLiteral("sans-serif"), QStringLiteral("serif")}) {
        for (int size : {40, 96}) {
            for (const Qt::Alignment alignment : {Qt::Alignment(Qt::AlignLeft), Qt::Alignment(Qt::AlignHCenter), Qt::Alignment(Qt::AlignRight)}) {
                for (bool selected : {false, true}) {
                    MyTextItem item(QStringLiteral("fjy es"), nullptr);
                    item.setAlignment(alignment);
                    item.setPos(300, 120);
                    if (selected) {
                        item.setTextInteractionFlags(Qt::TextEditorInteraction);
                        QTextCursor active(item.document());
                        active.setPosition(item.document()->characterCount() - 1);
                        active.setPosition(0, QTextCursor::KeepAnchor);
                        item.setTextCursor(active);
                    }
                    const qreal fixedAnchor = anchor(item, alignment);
                    const QTextCursor saved = item.textCursor();
                    QTextCharFormat familyFormat, sizeFormat, italicFormat;
                    familyFormat.setFontFamilies(QStringList{family});
                    familyFormat.setFontStyleName(QString());
                    sizeFormat.setProperty(QTextFormat::FontPixelSize, size);
                    italicFormat.setFontItalic(true);
                    italicFormat.setFontStyleName(QString());
                    const QList<QTextCharFormat> changes{familyFormat, sizeFormat, italicFormat};
                    int step = 0;
                    for (const QTextCharFormat &delta : changes) {
                        std::unique_ptr<QTextDocument> expected(item.document()->clone());
                        QTextCursor reference(expected.get());
                        reference.select(QTextCursor::Document);
                        reference.mergeCharFormat(delta);
                        expected->setTextWidth(-1);
                        const QSizeF natural = expected->documentLayout()->documentSize();
                        const auto before = snapshot(item, alignment);
                        TitlerRichText::apply(&item, delta);
                        const auto after = snapshot(item, alignment);
                        const QRectF actual = item.baseBoundingRect();
                        const QTextCursor active = item.textCursor();
                        const bool geometry = same(actual.width(), natural.width()) && same(actual.height(), natural.height());
                        const bool position = same(anchor(item, alignment), fixedAnchor);
                        const bool selection = active.anchor() == saved.anchor() && active.position() == saved.position();
                        const bool content = item.toPlainText() == QStringLiteral("fjy es");
                        const bool ok = geometry && position && selection && content;
                        failures += !ok;
                        rows.append(QJsonObject{{QStringLiteral("family"), family},
                                                {QStringLiteral("size"), size},
                                                {QStringLiteral("selected"), selected},
                                                {QStringLiteral("step"), step++},
                                                {QStringLiteral("before"), before},
                                                {QStringLiteral("after"), after},
                                                {QStringLiteral("expectedNaturalSize"), QJsonArray{natural.width(), natural.height()}},
                                                {QStringLiteral("geometryMatches"), geometry},
                                                {QStringLiteral("anchorStable"), position},
                                                {QStringLiteral("selectionPreserved"), selection},
                                                {QStringLiteral("passed"), ok}});
                    }
                    ++cases;
                }
            }
        }
    }
    record(QStringLiteral("editor-font-setup"),
           {{QStringLiteral("cases"), cases}, {QStringLiteral("steps"), rows.size()}, {QStringLiteral("failures"), failures}, {QStringLiteral("rows"), rows}});
    REQUIRE(cases == 24);
    REQUIRE(rows.size() == 72);
    REQUIRE(failures == 0);
}

// Regression: the old committed width must remain the alignment reference
// even when a newly enlarged glyph no longer fits inside that width.
TEST_CASE("Font resize undo and redo preserve the committed layout anchor", "[Titler][AlignmentContract][FontResizeAnchor]")
{
    if (!richTextSupported()) {
        return;
    }
    using namespace RichTextAlignmentContract;
    QJsonArray rows;
    int cases = 0;
    int failures = 0;
    for (const QString &family : {QStringLiteral("sans-serif"), QStringLiteral("serif")}) {
        for (const Qt::Alignment alignment : {Qt::Alignment(Qt::AlignLeft), Qt::Alignment(Qt::AlignHCenter), Qt::Alignment(Qt::AlignRight)}) {
            for (bool selected : {false, true}) {
                MyTextItem item(QStringLiteral("fjy es"), nullptr);
                QTextCharFormat initial;
                initial.setFontFamilies(QStringList{family});
                initial.setFontStyleName(QString());
                initial.setProperty(QTextFormat::FontPixelSize, 12);
                TitlerRichText::apply(&item, initial);
                item.setAlignment(alignment);
                item.setPos(300, 120);
                if (selected) {
                    item.setTextInteractionFlags(Qt::TextEditorInteraction);
                    QTextCursor cursor(item.document());
                    cursor.setPosition(item.document()->characterCount() - 1);
                    cursor.setPosition(0, QTextCursor::KeepAnchor);
                    item.setTextCursor(cursor);
                }
                const qreal fixed = anchor(item, alignment);
                const QTextCursor saved = item.textCursor();
                for (int pixels : {96, 12, 160, 12, 40, 12}) {
                    const QRectF original = item.baseBoundingRect();
                    const QString originalHtml = item.document()->toHtml();
                    const qreal committedWidth = item.textWidth();
                    QTextCharFormat delta;
                    delta.setProperty(QTextFormat::FontPixelSize, pixels);
                    std::unique_ptr<QTextDocument> reference(item.document()->clone());
                    QTextCursor expected(reference.get());
                    expected.select(QTextCursor::Document);
                    expected.mergeCharFormat(delta);
                    reference->setTextWidth(-1);
                    const QSizeF natural = reference->documentLayout()->documentSize();
                    TitlerRichText::apply(&item, delta);
                    const QJsonObject changed = snapshot(item, alignment);
                    const QString changedHtml = item.document()->toHtml();
                    const bool geometry = same(item.baseBoundingRect().width(), natural.width()) && same(item.baseBoundingRect().height(), natural.height());
                    const bool selection = saved.anchor() == item.textCursor().anchor() && saved.position() == item.textCursor().position();
                    const bool changeAnchor = same(anchor(item, alignment), fixed);
                    item.document()->undo();
                    const QJsonObject undone = snapshot(item, alignment);
                    const bool undo = same(anchor(item, alignment), fixed) && same(item.baseBoundingRect().width(), original.width()) &&
                                      same(item.baseBoundingRect().height(), original.height()) && item.document()->toHtml() == originalHtml &&
                                      item.document()->isRedoAvailable();
                    item.document()->redo();
                    const QJsonObject redone = snapshot(item, alignment);
                    const bool redo = same(anchor(item, alignment), fixed) && same(item.baseBoundingRect().width(), natural.width()) &&
                                      same(item.baseBoundingRect().height(), natural.height()) && item.document()->toHtml() == changedHtml;
                    // A no-content geometry refresh must not move the anchor a second time.
                    item.doUpdateGeometry();
                    const bool repeat = same(anchor(item, alignment), fixed) && same(item.baseBoundingRect().width(), natural.width());
                    const bool ok = geometry && selection && changeAnchor && undo && redo && repeat;
                    failures += !ok;
                    rows.append(QJsonObject{{QStringLiteral("family"), family},
                                            {QStringLiteral("alignment"), int(alignment)},
                                            {QStringLiteral("selected"), selected},
                                            {QStringLiteral("pixels"), pixels},
                                            {QStringLiteral("fixedAnchor"), fixed},
                                            {QStringLiteral("previousCommittedWidth"), committedWidth},
                                            {QStringLiteral("changed"), changed},
                                            {QStringLiteral("undone"), undone},
                                            {QStringLiteral("redone"), redone},
                                            {QStringLiteral("geometryMatches"), geometry},
                                            {QStringLiteral("selectionPreserved"), selection},
                                            {QStringLiteral("changeAnchorStable"), changeAnchor},
                                            {QStringLiteral("undoPassed"), undo},
                                            {QStringLiteral("redoPassed"), redo},
                                            {QStringLiteral("repeatPassed"), repeat},
                                            {QStringLiteral("passed"), ok}});
                }
                ++cases;
            }
        }
    }
    record(QStringLiteral("font-resize-history"),
           {{QStringLiteral("cases"), cases}, {QStringLiteral("steps"), rows.size()}, {QStringLiteral("failures"), failures}, {QStringLiteral("rows"), rows}});
    REQUIRE(cases == 12);
    REQUIRE(rows.size() == 72);
    REQUIRE(failures == 0);
}
