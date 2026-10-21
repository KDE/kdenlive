// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include "richtextgradient.h"
#include "richtextoutline.h"

#include <QImage>
#include <QLibraryInfo>
#include <QPainter>
#include <QPainterPath>
#include <QTextBoundaryFinder>
#include <QVersionNumber>

// Shadows are independent character-format overrides of the legacy item style.
// Offsets use document coordinates, so no selection/region identifier is needed.
namespace TitlerShadowV1 {
enum Property { Enabled = QTextFormat::UserProperty + 5, Color, Blur, OffsetX, OffsetY };
constexpr int MaximumBlur = 200;
constexpr int MaximumOffset = 10000;

struct Style
{
    bool enabled{false};
    QColor color{0, 0, 0, 100};
    int blur{0};
    int x{0};
    int y{0};
    bool operator==(const Style &other) const
    {
        return enabled == other.enabled && color == other.color && blur == other.blur
               && x == other.x && y == other.y;
    }
};

inline bool valid(int property, const QVariant &value)
{
    if (property == Color)
        return value.value<QColor>().isValid();
    bool ok = false;
    const int integer = value.toInt(&ok);
    if (!ok)
        return false;
    if (property == Enabled)
        return integer == 0 || integer == 1;
    if (property == Blur)
        return integer >= 0 && integer <= MaximumBlur;
    return (property == OffsetX || property == OffsetY) && qAbs(qint64(integer)) <= MaximumOffset;
}

inline Style effective(const QTextCharFormat &format, Style fallback)
{
    fallback.blur = qBound(0, fallback.blur, MaximumBlur);
    fallback.x = qBound(-MaximumOffset, fallback.x, MaximumOffset);
    fallback.y = qBound(-MaximumOffset, fallback.y, MaximumOffset);
    if (!fallback.color.isValid())
        fallback.color = QColor(0, 0, 0, 100);
    for (int property = Enabled; property <= OffsetY; ++property) {
        if (!format.hasProperty(property) || !valid(property, format.property(property)))
            continue;
        const auto value = format.property(property);
        switch (property) {
        case Enabled:
            fallback.enabled = value.toBool();
            break;
        case Color:
            fallback.color = value.value<QColor>();
            break;
        case Blur:
            fallback.blur = value.toInt();
            break;
        case OffsetX:
            fallback.x = value.toInt();
            break;
        case OffsetY:
            fallback.y = value.toInt();
            break;
        }
    }
    return fallback;
}

inline bool hasOverrides(const QTextDocument *document)
{
    for (auto block = document->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (!fragment.isValid())
                continue;
            for (int property = Enabled; property <= OffsetY; ++property) {
                if (fragment.charFormat().hasProperty(property))
                    return true;
            }
        }
    }
    return false;
}

struct SelectionState
{
    Style style;
    bool mixed{false};
};

inline SelectionState selectionState(const QTextCursor &cursor, const Style &fallback)
{
    SelectionState result{effective(cursor.charFormat(), fallback), false};
    if (!cursor.hasSelection())
        return result;
    bool first = true;
    for (auto block = cursor.document()->findBlock(cursor.selectionStart());
         block.isValid() && block.position() < cursor.selectionEnd();
         block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (!fragment.isValid() || fragment.position() >= cursor.selectionEnd()
                || fragment.position() + fragment.length() <= cursor.selectionStart())
                continue;
            const auto style = effective(fragment.charFormat(), fallback);
            if (first) {
                result.style = style;
                first = false;
            } else {
                result.mixed |= !(result.style == style);
            }
        }
    }
    return result;
}

inline QDomElement save(QDomDocument &xml, const QTextDocument *document)
{
    if (!hasOverrides(document))
        return {};
    auto result = xml.createElement(QStringLiteral("richtext-shadows"));
    result.setAttribute(QStringLiteral("version"), 1);
    result.setAttribute(QStringLiteral("units"), QStringLiteral("utf16"));
    result.setAttribute(QStringLiteral("characters"), document->characterCount() - 1);
    result.setAttribute(QStringLiteral("text-sha256"), TitlerGradientV1::textHash(document));
    const char *names[] = {"enabled", "color", "blur", "x", "y"};
    for (auto block = document->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (!fragment.isValid())
                continue;
            auto run = xml.createElement(QStringLiteral("run"));
            bool any = false;
            for (int property = Enabled; property <= OffsetY; ++property) {
                const auto format = fragment.charFormat();
                if (!format.hasProperty(property) || !valid(property, format.property(property)))
                    continue;
                const auto value = format.property(property);
                run.setAttribute(QLatin1String(names[property - Enabled]),
                                 property == Color ? value.value<QColor>().name(QColor::HexArgb)
                                                   : QString::number(value.toInt()));
                any = true;
            }
            if (any) {
                run.setAttribute(QStringLiteral("start"), fragment.position());
                run.setAttribute(QStringLiteral("length"), fragment.length());
                result.appendChild(run);
            }
        }
    }
    return result;
}

inline bool restore(const QDomElement &content, QTextDocument *document)
{
    const auto element = content.firstChildElement(QStringLiteral("richtext-shadows"));
    if (element.isNull())
        return true;
    bool ok = false;
    const int total = document->characterCount() - 1;
    if (element.attribute(QStringLiteral("version")) != QLatin1String("1")
        || element.attribute(QStringLiteral("units")) != QLatin1String("utf16")
        || element.attribute(QStringLiteral("characters")).toInt(&ok) != total || !ok
        || element.attribute(QStringLiteral("text-sha256")) != TitlerGradientV1::textHash(document)
        || !element.nextSiblingElement(QStringLiteral("richtext-shadows")).isNull())
        return false;
    const auto plain = document->toPlainText();
    if (plain.size() != total)
        return false;
    const auto splits = [&plain, total](int position) {
        return position > 0 && position < total && plain.at(position - 1).isHighSurrogate()
               && plain.at(position).isLowSurrogate();
    };
    struct SavedRun
    {
        int start;
        int length;
        QTextCharFormat format;
    };
    QVector<SavedRun> runs;
    const char *names[] = {"enabled", "color", "blur", "x", "y"};
    int previous = 0;
    for (auto run = element.firstChildElement(); !run.isNull(); run = run.nextSiblingElement()) {
        bool startOk = false, lengthOk = false;
        const int start = run.attribute(QStringLiteral("start")).toInt(&startOk);
        const int length = run.attribute(QStringLiteral("length")).toInt(&lengthOk);
        if (run.tagName() != QLatin1String("run") || !startOk || !lengthOk || start < previous
            || length <= 0 || length > total || start > total - length || splits(start)
            || splits(start + length) || !run.firstChildElement().isNull())
            return false;
        QTextCharFormat format;
        bool any = false;
        for (int property = Enabled; property <= OffsetY; ++property) {
            const auto name = QLatin1String(names[property - Enabled]);
            if (!run.hasAttribute(name))
                continue;
            const QString text = run.attribute(name);
            QVariant value;
            if (property == Color) {
                if (text.size() != 9 || text.at(0) != QLatin1Char('#'))
                    return false;
                for (int i = 1; i < text.size(); ++i) {
                    const auto ch = text.at(i).toLower();
                    if (!((ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
                          || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f'))))
                        return false;
                }
                const uint rgba = text.mid(1).toUInt(&ok, 16);
                if (!ok)
                    return false;
                value = QColor::fromRgba(rgba);
            } else {
                const int integer = text.toInt(&ok);
                if (!ok)
                    return false;
                value = integer;
            }
            if (!valid(property, value))
                return false;
            format.setProperty(property, value);
            any = true;
        }
        if (!any)
            return false;
        runs.append({start, length, format});
        previous = start + length;
    }
    // Validate every run before modifying the document.
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    for (const auto &run : runs) {
        cursor.setPosition(run.start);
        cursor.setPosition(run.start + run.length, QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(run.format);
    }
    cursor.endEditBlock();
    return true;
}

struct Span
{
    int start;
    int length;
};
struct Group
{
    Style style;
    QVector<Span> spans;
};
struct Layer
{
    QImage image;
    QPointF origin;
};
using Layers = QVector<Layer>;

// A logical cursor at a bidi boundary can resolve to the neighboring visual
// run. Use the shaped interval, retaining cursor subdivisions for a shared
// ligature (whose whole glyph rectangle exceeds the selected characters).
inline QRectF decorationRect(const QTextLine &line,
                             int start,
                             int length,
                             const QList<QGlyphRun> &runs,
                             const QPointF &origin)
{
    qreal a = line.cursorToX(start);
    qreal z = line.cursorToX(start + length);
    QRectF visual;
    bool split = false;
    for (const auto &run : runs) {
        split |= run.flags().testFlag(QGlyphRun::SplitLigature);
        const auto bounds = run.boundingRect();
        if (!bounds.isEmpty())
            visual = visual.isEmpty() ? bounds : visual.united(bounds);
    }
    if (!split && !visual.isEmpty()) {
        a = visual.left();
        z = visual.right();
    }
    return QRectF(origin.x() + std::min(a, z),
                  origin.y() + line.y(),
                  std::abs(z - a),
                  line.height());
}

inline QRectF decorationBounds(QTextDocument *document, const QPen &outline)
{
    // Match the visible text item's source bounds: accents can extend above
    // the document rectangle, and Qt clips decoration fringes to these bounds.
    const qreal margin = TitlerOutline::margin(document, outline);
    return QRectF(QPointF(), document->size())
        .united(TitlerOutline::inkBounds(document))
        .adjusted(-margin, -margin, margin, margin);
}

struct ShadowCell
{
    int start;
    int end;
    QRectF rect;
    QRectF visibleRect;
    qreal baseline;
    int lineId;
    QTextCharFormat format;
    QRawFont physicalFont;
    QRectF lineBounds;
};

inline QVector<ShadowCell> shadowCells(QTextDocument *document,
                                       const Group &group,
                                       int visible,
                                       const QVector<TitlerGradientV1::Region> &gradients)
{
    QVector<ShadowCell> cells;
    // Collect the final line, including the hidden suffix: a later fallback
    // font must not change the underline metrics of already visible text.
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        const QTextLayout *layout = block.layout();
        if (!layout) {
            continue;
        }
        const QString text = block.text();
        QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
        int start = 0;
        for (int end = finder.toNextBoundary(); end >= 0;
             start = end, end = finder.toNextBoundary()) {
            for (const auto &part : TitlerGradientV1::layoutRanges(layout, start, end - start)) {
                const QTextLine line = layout->lineForTextPosition(part.start);
                if (!line.isValid() || end <= start) {
                    continue;
                }
                QTextCursor cursor(document);
                cursor.setPosition(block.position() + start);
                cursor.setPosition(block.position() + end, QTextCursor::KeepAnchor);
                QTextCharFormat format = cursor.charFormat();
                for (const auto &gradient : gradients) {
                    if (block.position() + start >= gradient.start
                        && block.position() + start < gradient.start + gradient.length) {
                        format.setForeground(QBrush(
                            TitlerGradientV1::gradientFromString(gradient.data, gradient.bounds)));
                        break;
                    }
                }
                bool included = false;
                for (const auto &span : group.spans)
                    included |= block.position() + start < span.start + span.length
                                && block.position() + end > span.start;
                QRawFont physicalFont;
                qreal largestAdvance = -1;
                qreal baseline = line.y() + line.ascent();
                // Decorations belong to the advancing font in the shaped
                // cluster. Zero-advance fallback marks do not gain a second
                // cell-wide decoration. No shaping or positions are replaced.
                const auto runs = line.glyphRuns(part.start, part.length);
                for (const QGlyphRun &run : runs) {
                    const QRawFont raw = run.rawFont();
                    if (!raw.isValid()) {
                        continue;
                    }
                    qreal advance = 0;
                    for (const QPointF &a : raw.advancesForGlyphIndexes(run.glyphIndexes())) {
                        advance += std::abs(a.x());
                    }
                    if (!physicalFont.isValid() || advance > largestAdvance) {
                        physicalFont = raw;
                        largestAdvance = advance;
                        if (!run.positions().isEmpty())
                            baseline = run.positions().first().y();
                    }
                }
                if (!physicalFont.isValid()) {
                    // Tabs and other advance-only cells can have no glyph run.
                    physicalFont = QRawFont::fromFont(
                        format.font().resolve(document->defaultFont()));
                }
                const QPointF origin
                    = document->documentLayout()->blockBoundingRect(block).topLeft();
                const QRectF rect = decorationRect(line, part.start, part.length, runs, origin);
                cells.append({block.position() + start,
                              block.position() + end,
                              rect,
                              included && block.position() + end <= visible ? rect : QRectF(),
                              origin.y() + baseline,
                              block.position() + line.textStart(),
                              format,
                              physicalFont,
                              line.naturalTextRect().translated(origin)});
            }
        }
    }
    std::sort(cells.begin(), cells.end(), [](const ShadowCell &a, const ShadowCell &b) {
        if (a.lineId != b.lineId) {
            return a.lineId < b.lineId;
        }
        return a.rect.left() < b.rect.left();
    });
    return cells;
}

inline void paintShadowBackgrounds(QPainter *painter, const QVector<ShadowCell> &cells)
{
    painter->save();
    for (const ShadowCell &cell : cells) {
        const QBrush brush = cell.format.background();
        if (brush.style() != Qt::NoBrush && !cell.visibleRect.isEmpty()) {
            painter->fillRect(cell.visibleRect.toAlignedRect(), brush);
        }
    }
    painter->restore();
}

template<typename Cells>
inline void paintShadowDecorations(QPainter *painter,
                                   const Cells &cells,
                                   const QFont &defaultFont,
                                   const QColor &defaultColor)
{
    struct Decoration
    {
        int lineId;
        qreal fullLeft;
        qreal fullRight;
        qreal left;
        qreal right;
        qreal y;
        QPen pen;
    };
    // QTBUG-134626 changed underline centering in Qt 6.8.4 and 6.9.1.
    // Use the loaded Qt version: an older-built binary can load newer Qt.
    static const bool underlineUsesPenWidth = [] {
        const auto version = QLibraryInfo::version();
        return version >= QVersionNumber(6, 9, 1)
               || (version >= QVersionNumber(6, 8, 4) && version < QVersionNumber(6, 9, 0));
    }();
    QVector<Decoration> underlines;
    QVector<Decoration> strikes;
    QVector<Decoration> overlines;
    painter->save();
    painter->setBrush(Qt::NoBrush);
    for (const auto &cell : cells) {
        const QTextCharFormat &format = cell.format;
        if (!format.fontUnderline() && !format.fontOverline() && !format.fontStrikeOut()) {
            continue;
        }
        const QBrush brush = format.foreground().style() == Qt::NoBrush ? QBrush(defaultColor)
                                                                        : format.foreground();
        const QFontMetricsF fallback(format.font().resolve(defaultFont), painter->device());
        const bool raw = cell.physicalFont.isValid();
        const qreal ascent = raw ? cell.physicalFont.ascent() : fallback.ascent();
        const qreal descent = raw ? cell.physicalFont.descent() : fallback.descent();
        const qreal thickness = raw ? cell.physicalFont.lineThickness() : fallback.lineWidth();
        const qreal underlinePosition = raw ? cell.physicalFont.underlinePosition()
                                            : fallback.underlinePos();
        QPen pen(brush, thickness, Qt::SolidLine, Qt::FlatCap);
        if (format.underlineColor().isValid()) {
            pen.setColor(format.underlineColor());
        }
        // Qt excludes trailing wrap whitespace from text decorations.
        // The natural line rectangle also includes its alignment offset.
        const QRectF fullRect = cell.rect.intersected(cell.lineBounds);
        if (fullRect.isEmpty())
            continue;
        const QRectF visibleRect = cell.visibleRect.intersected(fullRect);
        const bool visible = !visibleRect.isEmpty()
                             && !(pen.brush().style() == Qt::SolidPattern
                                  && pen.color().alpha() == 0);
        const qreal left = visible ? std::floor(visibleRect.left()) : 0;
        const qreal right = visible ? std::floor(visibleRect.right()) : 0;
        const qreal fullLeft = std::floor(fullRect.left());
        const qreal fullRight = std::floor(fullRect.right());
        if (format.fontUnderline()) {
            QPen underlinePen = pen;
            if (underlineUsesPenWidth && !painter->testRenderHint(QPainter::Antialiasing)) {
                underlinePen.setWidthF(std::max(qreal(1), qreal(qRound(thickness))));
            }
            // A contiguous underline has common final-layout position and
            // thickness, while each segment retains its foreground color.
            const qreal halfWidth = underlineUsesPenWidth ? underlinePen.widthF() / 2 : qreal(0.5);
            qreal offset = std::ceil(underlinePosition) + halfWidth;
            if (underlinePosition <= descent) {
                offset = std::min(offset, descent - halfWidth);
            }
            const auto style = format.underlineStyle();
            if (style >= QTextCharFormat::SingleUnderline
                && style <= QTextCharFormat::DashDotDotLine) {
                underlinePen.setStyle(static_cast<Qt::PenStyle>(style));
            }
            underlines.append(
                {cell.lineId, fullLeft, fullRight, left, right, cell.baseline + offset, underlinePen});
        }
        if (format.fontStrikeOut()) {
            strikes.append(
                {cell.lineId, fullLeft, fullRight, left, right, cell.baseline - ascent / 3, pen});
        }
        if (format.fontOverline()) {
            overlines.append(
                {cell.lineId, fullLeft, fullRight, left, right, cell.baseline - ascent, pen});
        }
    }
    for (qsizetype start = 0; start < underlines.size();) {
        qsizetype end = start + 1;
        qreal y = underlines[start].y;
        qreal width = underlines[start].pen.widthF();
        while (end < underlines.size() && underlines[end].lineId == underlines[start].lineId
               && underlines[end - 1].fullRight == underlines[end].fullLeft) {
            y = std::max(y, underlines[end].y);
            width = std::max(width, underlines[end].pen.widthF());
            ++end;
        }
        for (qsizetype i = start; i < end; ++i) {
            underlines[i].y = y;
            underlines[i].pen.setWidthF(width);
        }
        start = end;
    }
    const auto draw = [painter](const QVector<Decoration> &segments) {
        for (const auto &segment : segments) {
            if (segment.right > segment.left) {
                painter->setPen(segment.pen);
                painter->drawLine(QLineF(segment.left, segment.y, segment.right, segment.y));
            }
        }
    };
    draw(underlines);
    draw(strikes);
    draw(overlines);
    painter->restore();
}

inline QVector<Group> groups(QTextDocument *document, const Style &fallback, int visible = -1)
{
    QVector<Group> result;
    const int total = document->characterCount() - 1;
    visible = visible < 0 ? total : qBound(0, visible, total);
    for (auto block = document->begin(); block.isValid() && block.position() < visible;
         block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (!fragment.isValid() || fragment.position() >= visible)
                continue;
            const auto style = effective(fragment.charFormat(), fallback);
            if (!style.enabled || style.color.alpha() == 0)
                continue;
            int index = 0;
            while (index < result.size() && !(result.at(index).style == style))
                ++index;
            if (index == result.size())
                result.append({style, {}});
            result[index].spans.append(
                {fragment.position(), qMin(fragment.length(), visible - fragment.position())});
        }
    }
    return result;
}

// Same four-pass exponential blur as the existing LGPL MLT title painter.
inline void blurImage(QImage &image, int radius)
{
    if (radius <= 0 || image.isNull())
        return;
    const int table[] = {14, 10, 8, 6, 5, 5, 4, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2};
    const int alpha = radius > 17 ? 1 : table[radius - 1];
    const auto pass = [&](int startX, int startY, int dx, int dy, int count) {
        auto *pixel = image.scanLine(startY) + startX * 4;
        int channels[4] = {pixel[0] << 4, pixel[1] << 4, pixel[2] << 4, pixel[3] << 4};
        const int step = dy * image.bytesPerLine() + dx * 4;
        for (int i = 1; i < count; ++i) {
            pixel += step;
            for (int c = 0; c < 4; ++c)
                pixel[c] = uchar((channels[c] += ((pixel[c] << 4) - channels[c]) * alpha / 16) >> 4);
        }
    };
    for (int x = 0; x < image.width(); ++x)
        pass(x, 0, 0, 1, image.height());
    for (int y = 0; y < image.height(); ++y)
        pass(0, y, 1, 0, image.width());
    for (int x = 0; x < image.width(); ++x)
        pass(x, image.height() - 1, 0, -1, image.height());
    for (int y = 0; y < image.height(); ++y)
        pass(image.width() - 1, y, -1, 0, image.width());
}

inline QRectF sourceBounds(QTextDocument *document, const Span &span)
{
    QRectF result = TitlerGradientV1::rangeBounds(document, span.start, span.length);
    const int end = span.start + span.length;
    for (auto block = document->findBlock(span.start); block.isValid() && block.position() < end;
         block = block.next()) {
        const auto *layout = block.layout();
        if (!layout)
            continue;
        const QPointF origin = document->documentLayout()->blockBoundingRect(block).topLeft();
        const int a = qMax(span.start, block.position()) - block.position();
        const int b = qMin(end, block.position() + int(block.text().size())) - block.position();
        for (const auto &part : TitlerGradientV1::layoutRanges(layout, a, b - a)) {
            for (int i = 0; i < layout->lineCount(); ++i) {
                const auto line = layout->lineAt(i);
                const int from = qMax(part.start, line.textStart());
                const int to = qMin(part.start + part.length, line.textStart() + line.textLength());
                if (from >= to)
                    continue;
                for (const auto &run : line.glyphRuns(from, to - from)) {
                    // Metrics, in addition to ink, leave space for underlines and
                    // other decorations below a glyph's otherwise tight ink box.
                    const auto rect = run.boundingRect().translated(origin);
                    if (!rect.isEmpty())
                        result = result.isEmpty() ? rect : result.united(rect);
                    const QRectF cell(rect.left(),
                                      origin.y() + line.y(),
                                      rect.width(),
                                      line.height());
                    if (!cell.isEmpty())
                        result = result.isEmpty() ? cell : result.united(cell);
                }
            }
        }
    }
    return result;
}

inline QRectF bounds(QTextDocument *document, const Style &fallback, const QPen &outline)
{
    QRectF result;
    const qreal margin = TitlerOutline::margin(document, outline) + 2;
    document->documentLayout()->documentSize();
    for (const auto &group : groups(document, fallback)) {
        for (const auto &span : group.spans) {
            const QRectF ink = sourceBounds(document, span);
            const qreal pad = margin + 2 * group.style.blur;
            const auto rect
                = ink.adjusted(-pad, -pad, pad, pad).translated(group.style.x, group.style.y);
            result = result.isEmpty() ? rect : result.united(rect);
        }
    }
    return result;
}

inline Layers render(QTextDocument *document,
                     const Style &fallback,
                     const QPen &outline,
                     int visible = -1,
                     const QColor &defaultColor = Qt::white)
{
    Layers result;
    const auto gradients = TitlerGradientV1::regions(document);
    const auto resolved = groups(document, fallback, visible);
    document->documentLayout()->documentSize();
    const QRectF decorationClip = decorationBounds(document, outline);
    for (const auto &group : resolved) {
        QRectF source;
        for (const auto &span : group.spans) {
            const auto ink = sourceBounds(document, span);
            source = source.isEmpty() ? ink : source.united(ink);
        }
        if (source.isEmpty())
            continue;
        const int pad = qCeil(TitlerOutline::margin(document, outline)) + 2 * group.style.blur + 2;
        const QRect rect = source.toAlignedRect().adjusted(-pad, -pad, pad, pad);
        // Each style has its own image. Large offsets do not allocate a giant
        // mostly-empty union image, and hidden typewriter suffixes never paint.
        QImage image(rect.size(), QImage::Format_ARGB32_Premultiplied);
        if (image.isNull())
            continue;
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
        painter.translate(-rect.topLeft());
        const int total = document->characterCount() - 1;
        const auto cells = shadowCells(document, group, visible < 0 ? total : visible, gradients);
        for (int pass = 0; pass < 2; ++pass) {
            if (pass == 1)
                paintShadowBackgrounds(&painter, cells);
            for (const auto &span : group.spans) {
                const int end = span.start + span.length;
                for (auto block = document->findBlock(span.start);
                     block.isValid() && block.position() < end;
                     block = block.next()) {
                    const auto *layout = block.layout();
                    if (!layout)
                        continue;
                    const QPointF origin
                        = document->documentLayout()->blockBoundingRect(block).topLeft();
                    for (auto it = block.begin(); !it.atEnd(); ++it) {
                        const auto fragment = it.fragment();
                        const int a = qMax(span.start, fragment.position());
                        const int b = qMin(end, fragment.position() + fragment.length());
                        if (!fragment.isValid() || a >= b)
                            continue;
                        const auto format = fragment.charFormat();
                        QBrush brush = format.foreground().style() == Qt::NoBrush
                                           ? QBrush(defaultColor)
                                           : format.foreground();
                        for (const auto &gradient : gradients) {
                            if (a >= gradient.start && a < gradient.start + gradient.length) {
                                brush = QBrush(
                                    TitlerGradientV1::gradientFromString(gradient.data,
                                                                         gradient.bounds));
                                break;
                            }
                        }
                        const QPen stroke = TitlerOutline::effectivePen(format, outline);
                        for (const auto &part :
                             TitlerGradientV1::layoutRanges(layout, a - block.position(), b - a)) {
                            for (int lineIndex = 0; lineIndex < layout->lineCount(); ++lineIndex) {
                                const auto line = layout->lineAt(lineIndex);
                                const int from = qMax(part.start, line.textStart());
                                const int to = qMin(part.start + part.length,
                                                    line.textStart() + line.textLength());
                                if (from >= to)
                                    continue;
                                for (const auto &run : line.glyphRuns(from, to - from)) {
                                    painter.save();
                                    if (run.flags().testFlag(QGlyphRun::SplitLigature))
                                        painter.setClipRect(run.boundingRect().translated(origin),
                                                            Qt::IntersectClip);
                                    if (pass == 0 && stroke.style() != Qt::NoPen
                                        && stroke.color().alpha() != 0) {
                                        QPainterPath path;
                                        path.setFillRule(Qt::WindingFill);
                                        const auto glyphs = run.glyphIndexes();
                                        const auto positions = run.positions();
                                        for (int i = 0; i < qMin(glyphs.size(), positions.size());
                                             ++i)
                                            path.addPath(run.rawFont()
                                                             .pathForGlyph(glyphs.at(i))
                                                             .translated(origin + positions.at(i)));
                                        painter.strokePath(path, stroke);
                                    } else if (pass == 1
                                               && (brush.style() != Qt::SolidPattern
                                                   || brush.color().alpha() != 0)) {
                                        // Explicit ranges keep unselected bitmap/color glyphs out
                                        // of the mask, without recoloring or reflowing the source.
                                        painter.setPen(QPen(brush, 0));
                                        QGlyphRun glyphs = run;
                                        glyphs.setUnderline(false);
                                        glyphs.setOverline(false);
                                        glyphs.setStrikeOut(false);
                                        painter.drawGlyphRun(origin, glyphs);
                                    }
                                    painter.restore();
                                }
                            }
                        }
                    }
                }
            }
        }
        painter.setClipRect(decorationClip, Qt::IntersectClip);
        paintShadowDecorations(&painter, cells, document->defaultFont(), defaultColor);
        painter.setClipping(false);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.resetTransform();
        painter.fillRect(image.rect(), group.style.color);
        painter.end();
        blurImage(image, group.style.blur);
        result.append({image, rect.topLeft() + QPoint(group.style.x, group.style.y)});
    }
    return result;
}

inline void paint(QPainter *painter, const Layers &layers)
{
    for (const auto &layer : layers)
        painter->drawImage(layer.origin, layer.image);
}
} // namespace TitlerShadowV1
