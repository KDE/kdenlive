// SPDX-FileCopyrightText: 2026 klg . <faction-frail-22@proton.me>
// SPDX-License-Identifier: MIT
#pragma once

#include <QAbstractTextDocumentLayout>
#include <QBrush>
#include <QCryptographicHash>
#include <QDomDocument>
#include <QGlyphRun>
#include <QLinearGradient>
#include <QRawFont>
#include <QRectF>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextFragment>
#include <QTextLayout>
#include <QVector>
#include <QtMath>

// Qt HTML does not serialize gradient brushes. Keep the gradient
// recipe as a custom QTextCharFormat property and persist only the ranges that
// use it. Positions and lengths are QTextCursor UTF-16 units.
namespace TitlerGradientV1 {
constexpr int Property = QTextFormat::UserProperty + 1;

inline QString textHash(const QTextDocument *text)
{
    return QString::fromLatin1(QCryptographicHash::hash(text->toPlainText().toUtf8(), QCryptographicHash::Sha256).toHex());
}

inline QLinearGradient gradientFromString(const QString &data, int width, int height)
{
    const QStringList values = data.split(QLatin1Char(';'));
    QLinearGradient gradient;
    if (values.size() < 5) {
        return gradient;
    }
    gradient.setColorAt(values.at(2).toDouble() / 100.0, QColor(values.at(0)));
    gradient.setColorAt(values.at(3).toDouble() / 100.0, QColor(values.at(1)));
    const double angle = values.at(4).toDouble();
    if (angle <= 90.0) {
        gradient.setStart(0, 0);
        gradient.setFinalStop(width * qCos(qDegreesToRadians(angle)), height * qSin(qDegreesToRadians(angle)));
    } else {
        gradient.setStart(width, 0);
        gradient.setFinalStop(width + width * qCos(qDegreesToRadians(angle)), height * qSin(qDegreesToRadians(angle)));
    }
    return gradient;
}

inline QDomElement save(QDomDocument &xml, const QTextDocument *text)
{
    QDomElement result = xml.createElement(QStringLiteral("richtext-gradients"));
    result.setAttribute(QStringLiteral("version"), 1);
    result.setAttribute(QStringLiteral("units"), QStringLiteral("utf16"));
    result.setAttribute(QStringLiteral("characters"), text->characterCount() - 1);
    result.setAttribute(QStringLiteral("text-sha256"), textHash(text));
    for (QTextBlock block = text->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) continue;
            const QString data = fragment.charFormat().property(Property).toString();
            if (data.isEmpty()) continue;
            QDomElement run = xml.createElement(QStringLiteral("run"));
            run.setAttribute(QStringLiteral("start"), fragment.position());
            run.setAttribute(QStringLiteral("length"), fragment.length());
            run.setAttribute(QStringLiteral("data"), data);
            result.appendChild(run);
        }
    }
    return result;
}

inline bool restore(const QDomElement &content, QTextDocument *text)
{
    const QDomElement data = content.firstChildElement(QStringLiteral("richtext-gradients"));
    if (data.isNull()) return true;
    bool sizeOk = false;
    const int total = text->characterCount() - 1;
    const int savedSize = data.attribute(QStringLiteral("characters")).toInt(&sizeOk);
    if (data.attribute(QStringLiteral("version")) != QLatin1String("1") || data.attribute(QStringLiteral("units")) != QLatin1String("utf16") || !sizeOk ||
        savedSize != total || data.attribute(QStringLiteral("text-sha256")) != textHash(text) ||
        !data.nextSiblingElement(QStringLiteral("richtext-gradients")).isNull()) {
        return false;
    }
    struct Run
    {
        int start;
        int length;
        QString data;
    };
    QVector<Run> runs;
    const QString plain = text->toPlainText();
    if (plain.size() != total) return false;
    const auto splitsSurrogate = [&plain, total](int position) {
        return position > 0 && position < total && plain.at(position - 1).isHighSurrogate() && plain.at(position).isLowSurrogate();
    };
    int previousEnd = 0;
    for (QDomElement e = data.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        bool a = false, b = false;
        const Run run{e.attribute(QStringLiteral("start")).toInt(&a), e.attribute(QStringLiteral("length")).toInt(&b), e.attribute(QStringLiteral("data"))};
        if (e.tagName() != QLatin1String("run") || !a || !b || run.data.isEmpty() || run.start < previousEnd || run.length <= 0 || run.length > total ||
            run.start > total - run.length || splitsSurrogate(run.start) || splitsSurrogate(run.start + run.length)) {
            return false;
        }
        previousEnd = run.start + run.length;
        runs.append(run);
    }
    QTextCursor cursor(text);
    cursor.beginEditBlock();
    for (const Run &run : runs) {
        cursor.setPosition(run.start);
        cursor.setPosition(run.start + run.length, QTextCursor::KeepAnchor);
        QTextCharFormat delta;
        delta.setProperty(Property, run.data);
        cursor.mergeCharFormat(delta);
    }
    cursor.endEditBlock();
    return true;
}

// A region is a maximal logical range with the same recipe. Formatting can split
// it into several QTextFragments; those fragments must share one gradient.
// Version 1 has no region identity, so adjacent applications of the same recipe
// are indistinguishable. Paragraph separators are not serialized as runs and do
// not interrupt an otherwise continuous region; unformatted text always does.
struct Region
{
    int start;
    int length;
    QString data;
    QRectF bounds;
};

struct LayoutRange
{
    int start;
    int length;
};

// QTextLayout positions include the input method's preedit insertion, while
// document/serialized positions do not. Exclude the preedit from both geometry
// and the additional gradient formats, preserving its own input-method formats.
inline QVector<LayoutRange> layoutRanges(const QTextLayout *layout, int start, int length)
{
    const int preedit = layout->preeditAreaPosition();
    const int extra = int(layout->preeditAreaText().size());
    if (extra == 0 || preedit < 0 || start + length <= preedit) {
        return {{start, length}};
    }
    if (start >= preedit) {
        return {{start + extra, length}};
    }
    return {{start, preedit - start}, {preedit + extra, start + length - preedit}};
}

inline QRectF rangeBounds(QTextDocument *text, int start, int length)
{
    const int end = start + length;
    QRectF bounds;
    QRectF whitespaceBounds;
    const auto unite = [](QRectF &target, const QRectF &part) {
        if (!part.isEmpty()) {
            target = target.isEmpty() ? part : target.united(part);
        }
    };
    for (QTextBlock block = text->findBlock(start); block.isValid() && block.position() < end; block = block.next()) {
        const QTextLayout *layout = block.layout();
        if (!layout) {
            continue;
        }
        // Unlike layout->position(), this also includes any containing frames.
        const QPointF origin = text->documentLayout()->blockBoundingRect(block).topLeft();
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            const int a = qMax(start, fragment.position());
            const int b = qMin(end, fragment.position() + fragment.length());
            if (a >= b) {
                continue;
            }
            const QTextCharFormat format = fragment.charFormat();
            for (const LayoutRange &part : layoutRanges(layout, a - block.position(), b - a)) {
                for (int i = 0; i < layout->lineCount(); ++i) {
                    const QTextLine line = layout->lineAt(i);
                    const int from = qMax(part.start, line.textStart());
                    const int to = qMin(part.start + part.length, line.textStart() + line.textLength());
                    if (from >= to) {
                        continue;
                    }
                    // Query each visual line separately: QTextLayout::glyphRuns
                    // can merge runs on different lines, losing per-line clips.
                    for (const QGlyphRun &run : line.glyphRuns(from, to - from)) {
                        const QRawFont font = run.rawFont();
                        if (!font.isValid()) {
                            continue;
                        }
                        const qreal fontHeight = font.ascent() + font.descent();
                        qreal baseline = -fontHeight * format.baselineOffset() / 100.0;
                        if (format.verticalAlignment() == QTextCharFormat::AlignSubScript) {
                            baseline += fontHeight * format.subScriptBaseline() / 100.0;
                        } else if (format.verticalAlignment() == QTextCharFormat::AlignSuperScript) {
                            baseline -= fontHeight * format.superScriptBaseline() / 100.0;
                        }
                        const QPointF offset = origin + QPointF(0, baseline);
                        unite(whitespaceBounds, run.boundingRect().translated(offset));
                        const auto glyphs = run.glyphIndexes();
                        const auto positions = run.positions();
                        for (int g = 0; g < qMin(glyphs.size(), positions.size()); ++g) {
                            QRectF ink = font.boundingRect(glyphs.at(g)).translated(positions.at(g));
                            if (run.flags().testFlag(QGlyphRun::SplitLigature)) {
                                // The range owns only part of this shared glyph.
                                // Keep vertical ink (accents/overhangs), clipping
                                // only to Qt's selected horizontal interval.
                                ink.setLeft(qMax(ink.left(), run.boundingRect().left()));
                                ink.setRight(qMin(ink.right(), run.boundingRect().right()));
                            }
                            unite(bounds, ink.translated(offset));
                        }
                    }
                }
            }
        }
    }
    // Whitespace has advance but no ink. It must not enlarge a painted region,
    // but supplies a harmless coordinate system for a whitespace-only region.
    return bounds.isEmpty() ? whitespaceBounds : bounds;
}

inline QVector<Region> regions(QTextDocument *text)
{
    QVector<Region> result;
    for (QTextBlock block = text->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            const QString data = fragment.charFormat().property(Property).toString();
            if (data.isEmpty()) {
                continue;
            }
            bool adjacent = false;
            if (!result.isEmpty() && result.last().data == data) {
                const int previousEnd = result.last().start + result.last().length;
                adjacent = previousEnd <= fragment.position();
                for (int p = previousEnd; adjacent && p < fragment.position(); ++p) {
                    adjacent = text->characterAt(p) == QChar::ParagraphSeparator;
                }
            }
            if (adjacent) {
                result.last().length = fragment.position() + fragment.length() - result.last().start;
            } else {
                result.append({fragment.position(), fragment.length(), data, {}});
            }
        }
    }
    if (!result.isEmpty()) {
        // Resolve wrapping, alignment and font metrics before measuring. Resolve
        // every region before changing any presentation/foreground formats.
        text->documentLayout()->documentSize();
        for (Region &region : result) {
            region.bounds = rangeBounds(text, region.start, region.length);
        }
    }
    return result;
}

inline QLinearGradient gradientFromString(const QString &data, const QRectF &bounds)
{
    // The existing unit-size recipe preserves angle/stop semantics. Scale and
    // translate in qreal coordinates rather than rounding selected glyph bounds.
    QLinearGradient gradient = gradientFromString(data, 1, 1);
    const auto point = [&bounds](const QPointF &p) { return bounds.topLeft() + QPointF(p.x() * bounds.width(), p.y() * bounds.height()); };
    gradient.setStart(point(gradient.start()));
    gradient.setFinalStop(point(gradient.finalStop()));
    return gradient;
}

// Rebuild presentation brushes without adding document-format edits to Undo.
inline void applyLayoutBrushes(QTextDocument *text, int width, int height, const QString &legacy = QString())
{
    const auto resolved = legacy.isEmpty() ? regions(text) : QVector<Region>();
    constexpr int layoutBrushProperty = QTextFormat::UserProperty + 4;
    for (QTextBlock block = text->begin(); block.isValid(); block = block.next()) {
        QTextLayout *layout = block.layout();
        if (!layout) {
            continue;
        }
        const auto previous = layout->formats();
        QList<QTextLayout::FormatRange> formats;
        for (const auto &range : previous) {
            if (!range.format.hasProperty(layoutBrushProperty)) {
                formats.append(range);
            }
        }
        const int preedit = layout->preeditAreaPosition();
        const int preeditLength = int(layout->preeditAreaText().size());
        const auto append = [&](int start, int length, const QBrush &brush) {
            const auto part = [&](int position, int count) {
                if (count <= 0) {
                    return;
                }
                QTextLayout::FormatRange range;
                range.start = position;
                range.length = count;
                range.format.setForeground(brush);
                range.format.setProperty(layoutBrushProperty, true);
                formats.append(range);
            };
            // Leave the input method's preedit formats separate from document ranges.
            if (preeditLength == 0 || preedit < 0 || start + length <= preedit) {
                part(start, length);
            } else if (start >= preedit) {
                part(start + preeditLength, length);
            } else {
                part(start, preedit - start);
                part(preedit + preeditLength, start + length - preedit);
            }
        };
        if (!legacy.isEmpty()) {
            // Keep the existing object-level gradient coordinate system.
            for (auto it = block.begin(); !it.atEnd(); ++it) {
                const QTextFragment fragment = it.fragment();
                if (fragment.isValid()) {
                    append(fragment.position() - block.position(), fragment.length(), QBrush(gradientFromString(legacy, width, height)));
                }
            }
        } else {
            const int blockEnd = block.position() + int(block.text().size());
            for (const Region &region : resolved) {
                const int start = qMax(region.start, block.position());
                const int end = qMin(region.start + region.length, blockEnd);
                if (start < end) {
                    append(start - block.position(), end - start, QBrush(gradientFromString(region.data, region.bounds)));
                }
            }
        }
        if (formats != previous) {
            layout->setFormats(formats);
            text->markContentsDirty(block.position(), block.length());
        }
    }
}

inline void applyBrushes(QTextDocument *text, int, int)
{
    // Resolve every region before foreground edits can invalidate the layout.
    // Apply per fragment as before: paragraph separators and insertion formats
    // must not be changed merely because a region spans several blocks.
    const auto resolved = regions(text);
    struct Run
    {
        int start;
        int length;
        QBrush brush;
    };
    QVector<Run> runs;
    int regionIndex = 0;
    for (QTextBlock block = text->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid() || fragment.charFormat().property(Property).toString().isEmpty()) {
                continue;
            }
            while (regionIndex < resolved.size() && resolved.at(regionIndex).start + resolved.at(regionIndex).length <= fragment.position()) {
                ++regionIndex;
            }
            if (regionIndex >= resolved.size()) {
                continue;
            }
            const Region &region = resolved.at(regionIndex);
            const QBrush brush(gradientFromString(region.data, region.bounds));
            if (fragment.charFormat().foreground() != brush) {
                runs.append({fragment.position(), fragment.length(), brush});
            }
        }
    }
    QTextCursor cursor(text);
    cursor.beginEditBlock();
    for (const Run &run : runs) {
        cursor.setPosition(run.start);
        cursor.setPosition(run.start + run.length, QTextCursor::KeepAnchor);
        QTextCharFormat delta;
        delta.setForeground(run.brush);
        cursor.mergeCharFormat(delta);
    }
    cursor.endEditBlock();
}
} // namespace TitlerGradientV1
