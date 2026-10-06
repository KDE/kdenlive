// SPDX-FileCopyrightText: 2026 klg . <faction-frail-22@proton.me>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QGraphicsTextItem>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>

// Rich text: merge ONLY the property the user changed.
namespace TitlerRichText {
// Rich text: return the character format represented by the caret/selection.
inline QTextCharFormat activeFormat(const QGraphicsTextItem *item)
{
    QTextCursor cursor = item->textCursor();

    if (cursor.hasSelection()) {
        const int start = cursor.selectionStart();
        cursor.clearSelection();
        cursor.setPosition(start);

        if (start < item->document()->characterCount() - 1) {
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        }
    }

    return cursor.charFormat();
}

inline bool selectionHasMixedCharacterFormat(const QGraphicsTextItem *item)
{
    // Rich text: inspect intersecting runs, not UTF-16 units.
    const QTextCursor selected = item->textCursor();
    if (!selected.hasSelection()) return false;
    QFont referenceFont;
    QBrush referenceBrush;
    bool haveReference = false;
    for (QTextBlock block = item->document()->findBlock(selected.selectionStart());
         block.isValid() && block.position() < selected.selectionEnd(); block = block.next()) {
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto fragment = it.fragment();
            if (!fragment.isValid() || fragment.position() >= selected.selectionEnd()
                || fragment.position() + fragment.length() <= selected.selectionStart()) continue;
            const auto format = fragment.charFormat();
            const QFont font = format.font().resolve(item->document()->defaultFont());
            const QBrush brush = format.foreground().style() == Qt::NoBrush
                ? QBrush(item->defaultTextColor()) : format.foreground();
            if (!haveReference) {
                referenceFont = font;
                referenceBrush = brush;
                haveReference = true;
            } else if (referenceFont != font || referenceBrush != brush) {
                return true;
            }
        }
    }
    return false;
}

inline void apply(QGraphicsTextItem *item, const QTextCharFormat &delta)
{
    const QTextCursor saved = item->textCursor();
    QTextCursor cursor = saved;
    const bool objectMode = !cursor.hasSelection() &&
        !item->textInteractionFlags().testFlag(Qt::TextEditable);
    if (objectMode) {
        cursor.select(QTextCursor::Document);
    }
    cursor.beginEditBlock();
    cursor.mergeCharFormat(delta);
    cursor.endEditBlock();
    // Keep both selection endpoints; in caret mode keep the insertion format.
    item->setTextCursor(objectMode ? saved : cursor);
    item->update();
}
}
