
// SPDX-FileCopyrightText: 2026 klg . <faction-frail-22@proton.me>
// SPDX-License-Identifier: MIT
#pragma once
#include <QCryptographicHash>
#include <QDomDocument>
#include <QFont>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QVector>
#include <cmath>

// Keep spacing metadata as a versioned supplement to Qt's HTML representation.
// Positions and lengths use QTextCursor's UTF-16 units, not UTF-8 byte offsets.
namespace TitlerSpacingV1 {
inline QString textHash(const QTextDocument *text)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        text->toPlainText().toUtf8(), QCryptographicHash::Sha256).toHex());
}

inline QDomElement save(QDomDocument &xml, const QTextDocument *text)
{
    QDomElement result = xml.createElement(QStringLiteral("richtext-spacing"));
    result.setAttribute(QStringLiteral("version"), 1);
    result.setAttribute(QStringLiteral("units"), QStringLiteral("utf16"));
    result.setAttribute(QStringLiteral("characters"), text->characterCount() - 1);
    result.setAttribute(QStringLiteral("text-sha256"), textHash(text));
    for (QTextBlock block = text->begin(); block.isValid(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            const QFont font = fragment.charFormat().font().resolve(text->defaultFont());
            QDomElement run = xml.createElement(QStringLiteral("run"));
            run.setAttribute(QStringLiteral("start"), fragment.position());
            run.setAttribute(QStringLiteral("length"), fragment.length());
            run.setAttribute(QStringLiteral("type"), int(font.letterSpacingType()));
            run.setAttribute(QStringLiteral("value"), QString::number(font.letterSpacing(), 'g', 17));
            result.appendChild(run);
        }
    }
    return result;
}

inline bool restore(const QDomElement &content, QTextDocument *text)
{
    const QDomElement data = content.firstChildElement(QStringLiteral("richtext-spacing"));
    if (data.isNull()) {
        return true; // Older files: keep the existing HTML/legacy behavior.
    }
    bool sizeOk = false;
    const int total = text->characterCount() - 1;
    const int savedSize = data.attribute(QStringLiteral("characters")).toInt(&sizeOk);
    if (data.attribute(QStringLiteral("version")) != QLatin1String("1")
        || data.attribute(QStringLiteral("units")) != QLatin1String("utf16")
        || !sizeOk || savedSize != total
        || data.attribute(QStringLiteral("text-sha256")) != textHash(text)
        || !data.nextSiblingElement(QStringLiteral("richtext-spacing")).isNull()) {
        return false;
    }
    struct Run { int start; int length; int type; double value; };
    QVector<Run> runs;
    const QString plain = text->toPlainText();
    if (plain.size() != total) {
        return false;
    }
    const auto splitsSurrogate = [&plain, total](int position) {
        return position > 0 && position < total
            && plain.at(position - 1).isHighSurrogate()
            && plain.at(position).isLowSurrogate();
    };
    int previousEnd = 0;
    for (QDomElement e = data.firstChildElement(); !e.isNull(); e = e.nextSiblingElement()) {
        bool a = false, b = false, c = false, d = false;
        Run run{e.attribute(QStringLiteral("start")).toInt(&a),
                e.attribute(QStringLiteral("length")).toInt(&b),
                e.attribute(QStringLiteral("type")).toInt(&c),
                e.attribute(QStringLiteral("value")).toDouble(&d)};
        if (e.tagName() != QLatin1String("run") || !a || !b || !c || !d
            || run.start < previousEnd || run.length <= 0 || run.length > total
            || run.start > total - run.length || !std::isfinite(run.value)
            || (run.type != int(QFont::AbsoluteSpacing) && run.type != int(QFont::PercentageSpacing))
            || splitsSurrogate(run.start) || splitsSurrogate(run.start + run.length)) {
            return false;
        }
        previousEnd = run.start + run.length;
        runs.append(run);
    }
    // Validate the complete supplement before changing any characters.
    QTextCursor cursor(text);
    cursor.beginEditBlock();
    for (const Run &run : runs) {
        cursor.setPosition(run.start);
        cursor.setPosition(run.start + run.length, QTextCursor::KeepAnchor);
        QTextCharFormat delta;
        delta.setFontLetterSpacingType(QFont::SpacingType(run.type));
        delta.setFontLetterSpacing(run.value);
        cursor.mergeCharFormat(delta);
    }
    cursor.endEditBlock();
    return true;
}
}
