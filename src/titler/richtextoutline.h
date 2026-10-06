// SPDX-FileCopyrightText: 2026 klg . <faction-frail-22@proton.me>
// SPDX-License-Identifier: MIT
#pragma once

#include <QAbstractTextDocumentLayout>
#include <QColor>
#include <QCryptographicHash>
#include <QDomDocument>
#include <QGlyphRun>
#include <QPainter>
#include <QPainterPath>
#include <QRawFont>
#include <QTextLayout>
#include <QPen>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QVector>
#include <QtMath>

namespace TitlerOutline {
constexpr int WidthProperty = QTextFormat::UserProperty + 2;
constexpr int ColorProperty = QTextFormat::UserProperty + 3;
constexpr qreal MaximumWidth = 200;

inline qreal width(const QPen &pen)
{
    return pen.style() == Qt::NoPen ? 0 : pen.widthF();
}

inline QPen effectivePen(const QTextCharFormat &format, const QPen &fallback)
{
    qreal value = width(fallback);
    if (format.hasProperty(WidthProperty)) {
        bool ok = false;
        const qreal requested = format.property(WidthProperty).toDouble(&ok);
        if (ok && qIsFinite(requested) && requested >= 0 && requested <= MaximumWidth) {
            value = requested;
        }
    }
    QColor color = fallback.color();
    if (format.hasProperty(ColorProperty)) {
        const QColor requested = format.property(ColorProperty).value<QColor>();
        if (requested.isValid()) {
            color = requested;
        }
    }
    return QPen(color, value, value > 0 ? Qt::SolidLine : Qt::NoPen, Qt::RoundCap, Qt::RoundJoin);
}

inline bool hasOverrides(const QTextDocument *document)
{
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto format = it.fragment().charFormat();
            if (format.hasProperty(WidthProperty) || format.hasProperty(ColorProperty)) {
                return true;
            }
        }
    }
    return false;
}

inline qreal margin(const QTextDocument *document, const QPen &fallback)
{
    qreal result = 0;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (fragment.isValid()) {
                result = qMax(result, width(effectivePen(fragment.charFormat(), fallback)) / 2);
            }
        }
    }
    return document->isEmpty() ? width(fallback) / 2 : result;
}

struct SelectionState {
    QPen pen;
    bool mixedWidth{false};
    bool mixedColor{false};
};

inline SelectionState selectionState(const QTextCursor &cursor, const QPen &fallback)
{
    SelectionState state;
    state.pen = effectivePen(cursor.charFormat(), fallback);
    if (!cursor.hasSelection()) {
        return state;
    }
    bool first = true;
    for (QTextBlock block = cursor.document()->findBlock(cursor.selectionStart());
         block.isValid() && block.position() < cursor.selectionEnd(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (!fragment.isValid() || fragment.position() >= cursor.selectionEnd()
                || fragment.position() + fragment.length() <= cursor.selectionStart()) {
                continue;
            }
            const QPen pen = effectivePen(fragment.charFormat(), fallback);
            if (first) {
                state.pen = pen;
                first = false;
            } else {
                state.mixedWidth |= width(pen) != width(state.pen);
                state.mixedColor |= pen.color() != state.pen.color();
            }
        }
    }
    return state;
}

inline QString textHash(const QTextDocument *document)
{
    return QString::fromLatin1(QCryptographicHash::hash(document->toPlainText().toUtf8(), QCryptographicHash::Sha256).toHex());
}

// Preserve exact RGBA values; Qt HTML rounds fractional alpha on a round trip.
// The properties also keep strokes separate from the document's fill pass.
inline QDomElement save(QDomDocument &xml, const QTextDocument *document)
{
    if (!hasOverrides(document)) {
        return {};
    }
    QDomElement result = xml.createElement(QStringLiteral("richtext-outlines"));
    result.setAttribute(QStringLiteral("version"), 1);
    result.setAttribute(QStringLiteral("units"), QStringLiteral("utf16"));
    result.setAttribute(QStringLiteral("characters"), document->characterCount() - 1);
    result.setAttribute(QStringLiteral("text-sha256"), textHash(document));
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            const auto format = fragment.charFormat();
            if (!fragment.isValid() || (!format.hasProperty(WidthProperty) && !format.hasProperty(ColorProperty))) {
                continue;
            }
            QDomElement run = xml.createElement(QStringLiteral("run"));
            run.setAttribute(QStringLiteral("start"), fragment.position());
            run.setAttribute(QStringLiteral("length"), fragment.length());
            if (format.hasProperty(WidthProperty)) {
                run.setAttribute(QStringLiteral("width"), QString::number(format.property(WidthProperty).toDouble(), 'g', 17));
            }
            if (format.hasProperty(ColorProperty)) {
                run.setAttribute(QStringLiteral("color"), format.property(ColorProperty).value<QColor>().name(QColor::HexArgb));
            }
            result.appendChild(run);
        }
    }
    return result;
}

inline bool restore(const QDomElement &content, QTextDocument *document)
{
    const QDomElement data = content.firstChildElement(QStringLiteral("richtext-outlines"));
    if (data.isNull()) {
        return true;
    }
    bool sizeOk = false;
    const int total = document->characterCount() - 1;
    const int savedSize = data.attribute(QStringLiteral("characters")).toInt(&sizeOk);
    if (data.attribute(QStringLiteral("version")) != QLatin1String("1")
        || data.attribute(QStringLiteral("units")) != QLatin1String("utf16")
        || !sizeOk || savedSize != total || data.attribute(QStringLiteral("text-sha256")) != textHash(document)
        || !data.nextSiblingElement(QStringLiteral("richtext-outlines")).isNull()) {
        return false;
    }
    const QString plain = document->toPlainText();
    if (plain.size() != total) {
        return false;
    }
    const auto splitsSurrogate = [&plain, total](int position) {
        return position > 0 && position < total && plain.at(position - 1).isHighSurrogate() && plain.at(position).isLowSurrogate();
    };
    struct Run {
        int start;
        int length;
        QTextCharFormat format;
    };
    QVector<Run> runs;
    int previousEnd = 0;
    for (QDomElement element = data.firstChildElement(); !element.isNull(); element = element.nextSiblingElement()) {
        bool startOk = false, lengthOk = false;
        const int start = element.attribute(QStringLiteral("start")).toInt(&startOk);
        const int length = element.attribute(QStringLiteral("length")).toInt(&lengthOk);
        if (element.tagName() != QLatin1String("run") || !startOk || !lengthOk || start < previousEnd
            || length <= 0 || length > total || start > total - length || splitsSurrogate(start) || splitsSurrogate(start + length)
            || (!element.hasAttribute(QStringLiteral("width")) && !element.hasAttribute(QStringLiteral("color")))
            || !element.firstChildElement().isNull()) {
            return false;
        }
        QTextCharFormat format;
        if (element.hasAttribute(QStringLiteral("width"))) {
            bool ok = false;
            const qreal value = element.attribute(QStringLiteral("width")).toDouble(&ok);
            if (!ok || !qIsFinite(value) || value < 0 || value > MaximumWidth) {
                return false;
            }
            format.setProperty(WidthProperty, value);
        }
        if (element.hasAttribute(QStringLiteral("color"))) {
            const QString text = element.attribute(QStringLiteral("color"));
            if (text.size() != 9 || text.at(0) != QLatin1Char('#')) {
                return false;
            }
            for (int i = 1; i < text.size(); ++i) {
                const QChar ch = text.at(i).toLower();
                if (!((ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) || (ch >= QLatin1Char('a') && ch <= QLatin1Char('f')))) {
                    return false;
                }
            }
            bool ok = false;
            const uint rgba = text.mid(1).toUInt(&ok, 16);
            if (!ok) {
                return false;
            }
            format.setProperty(ColorProperty, QColor::fromRgba(rgba));
        }
        runs.append({start, length, format});
        previousEnd = start + length;
    }
    // Reject malformed metadata before applying any part of it.
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

// Full-layout ink extents, separate from the advance/layout rectangle used by
// alignment and serialization. Cached by the item, never recomputed in paint().
inline QRectF inkBounds(QTextDocument *document)
{
    (void)document->documentLayout()->documentSize();
    QRectF bounds;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        const QTextLayout *layout = block.layout();
        if (!layout || block.text().isEmpty()) {
            continue;
        }
        for (const QGlyphRun &run : layout->glyphRuns(0, int(block.text().size()))) {
            const QRawFont font = run.rawFont();
            if (!font.isValid()) {
                continue;
            }
            const auto glyphs = run.glyphIndexes();
            const auto positions = run.positions();
            for (qsizetype i = 0; i < qMin(glyphs.size(), positions.size()); ++i) {
                const QRectF glyph = font.boundingRect(glyphs.at(i));
                if (!glyph.isEmpty()) {
                    bounds = bounds.united(glyph.translated(layout->position() + positions.at(i)));
                }
            }
        }
    }
    // Raster bounds are rounded independently of logical layout coordinates.
    // Preserve a one-pixel fringe without modifying spacing or wrap width.
    return bounds.isEmpty() ? bounds : bounds.adjusted(-1, -1, 1, 1);
}

inline void paint(QPainter *painter, QTextDocument *document, const QPen &fallback, int visible = -1)
{
    const int total = document->characterCount() - 1;
    visible = visible < 0 ? total : qBound(0, visible, total);
    if (visible == 0) {
        return;
    }
    (void)document->documentLayout()->documentSize();
    struct Stroke {
        QPen pen;
        QPainterPath path;
        QRectF clip;
        bool partialLigature;
    };
    QVector<Stroke> strokes;
    for (QTextBlock block = document->begin(); block.isValid() && block.position() < visible; block = block.next()) {
        const QTextLayout *layout = block.layout();
        if (!layout) {
            continue;
        }
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (!fragment.isValid() || fragment.position() >= visible) {
                continue;
            }
            const QPen pen = effectivePen(fragment.charFormat(), fallback);
            if (pen.style() == Qt::NoPen || pen.color().alpha() == 0) {
                continue;
            }
            const int start = fragment.position() - block.position();
            const int length = qMin(fragment.length(), visible - fragment.position());
            const auto runs = layout->glyphRuns(start, length);
            for (const QGlyphRun &run : runs) {
                const QRawFont font = run.rawFont();
                const auto glyphs = run.glyphIndexes();
                const auto positions = run.positions();
                QPainterPath path;
                path.setFillRule(Qt::WindingFill);
                for (int i = 0; i < glyphs.size(); ++i) {
                    path.addPath(font.pathForGlyph(glyphs.at(i)).translated(layout->position() + positions.at(i)));
                }
                if (path.isEmpty()) {
                    continue;
                }
                const bool partial = run.flags().testFlag(QGlyphRun::SplitLigature);
                // Paint whole glyph contours without selection rectangles. Only a
                // range which actually splits a shared glyph needs Qt's clip.
                if (!partial && !strokes.isEmpty() && !strokes.last().partialLigature && strokes.last().pen == pen) {
                    strokes.last().path.addPath(path);
                } else {
                    strokes.append({pen, path, run.boundingRect().translated(layout->position()), partial});
                }
            }
        }
    }
    painter->save();
    for (const auto &stroke : strokes) {
        painter->save();
        if (stroke.partialLigature) {
            painter->setClipRect(stroke.clip, Qt::IntersectClip);
        }
        painter->strokePath(stroke.path, stroke.pen);
        painter->restore();
    }
    painter->restore();
}
}
