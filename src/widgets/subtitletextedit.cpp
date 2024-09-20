/*  This file is part of Kdenlive. See www.kdenlive.org.
    SPDX-FileCopyrightText: 2024 Chengkun Chen <serix2004@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "subtitletextedit.h"
#include "qabstractitemview.h"
#include "qscrollbar.h"
#include <iostream>
#include <qapplication.h>

static const QStringList ASSTagNames = {
    "\\b",    "\\i",     "\\u",    "\\s",        "\\fn",        "\\fs",   "\\r",     "\\pos",   "\\an",   "\\k",     "\\kf",    "\\ko", "\\q",    "\\p",
    "\\pbo",  "\\fad",   "\\fade", "\\org",      "\\c",         "\\1c",   "\\2c",    "\\3c",    "\\4c",   "\\alpha", "\\1a",    "\\2a", "\\3a",   "\\4a",
    "\\clip", "\\iclip", "\\t",    "\\cliprect", "\\icliprect", "\\bord", "\\xbord", "\\ybord", "\\shad", "\\xshad", "\\yshad", "\\be", "\\blur", "\\fr",
    "\\fax",  "\\fay",   "\\fscx", "\\fscy",     "\\fsp",       "\\frx",  "\\frz",   "\\fry",   "\\frz",  "\\fr",    "\\move"};
static const int maxASSTagNameLength = 10;

void ASSTagHighlighter::highlightBlock(const QString &text)
{
    auto setFmt = [this](int pos, int len, QColor color, bool bold) {
        QTextCharFormat fmt;
        fmt.setForeground(color);
        fmt.setFontWeight(bold ? QFont::Bold : QFont::Normal);

        setFormat(pos, len, fmt);
    };
    QColor textColor = QApplication::palette().text().color();
    bool inTag = false;
    bool inArg = false;
    bool inTagName = false;
    for (int i = 0; i < text.size(); i++) {
        if (!inTag && i + 1 < text.size() && text[i] == '\\' && (text[i + 1] == '{' || text[i + 1] == '}')) {
            setFmt(i, 2, textColor, false);
            i++;
            continue;
        }

        // separators
        if (!inTag && text[i] == '{') {
            inTag = true;
            setFmt(i, 1, QApplication::palette().highlight().color(), false);
            continue;
        }

        if (inTag && text[i] == '}') {
            inTag = false;
            inTagName = false;
            setFmt(i, 1, QApplication::palette().highlight().color(), false);
            continue;
        }

        if (inTag && !inArg && text[i] == '(') {
            inArg = true;
            setFmt(i, 1, QApplication::palette().highlight().color(), false);
            continue;
        }

        if (inTag && inArg && text[i] == ')') {
            inArg = false;
            inTagName = false;
            setFmt(i, 1, QApplication::palette().highlight().color(), false);
            continue;
        }

        if (inTag && text[i] == '\\') {
            inTagName = true;
            setFmt(i, 1, Qt::red, false);
            continue;
        }

        if (inTag && inArg && !inTagName && text[i] == ',') {
            inTagName = false;
            setFmt(i, 1, Qt::red, false);
            continue;
        }

        if (inTag) {
            if (inTagName) {
                int maxLength = 0;
                QString tagText;
                for (int j = i; j < text.size() && text[j] != '(' && text[j] != ')' && text[j] != '}' && text[j] != '\\'; j++) {
                    tagText += text[j];
                    if (ASSTagNames.contains("\\" + tagText)) {
                        if (tagText.size() > maxLength) {
                            maxLength = tagText.size();
                        }
                    }
                }

                if (maxLength > 0) {
                    // tagname & arg
                    setFmt(i, maxLength, Qt::darkGreen, true);
                    if (tagText.size() > maxLength) setFmt(i + maxLength, tagText.size() - maxLength, Qt::gray, false);

                    inTagName = false;
                } else {
                    // invalid tag
                    setFmt(i, tagText.size(), Qt::gray, true);
                }

                i += tagText.size() - 1;
            } else {
                setFmt(i, 1, Qt::gray, false);
            }
        } else {
            setFmt(i, 1, textColor, false);
        }
    }
}

SubtitleTextEdit::SubtitleTextEdit(QWidget *parent)
    : KTextEdit(parent)
{
    // Highlighter
    new ASSTagHighlighter(document());

    // Completer
    completer = new QCompleter(this);
    completer->setWidget(this);
    completer->setModel(new QStringListModel(ASSTagNames, completer));
    completer->setModelSorting(QCompleter::CaseInsensitivelySortedModel);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setWrapAround(false);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setFilterMode(Qt::MatchContains);
    QObject::connect(completer, QOverload<const QString &>::of(&QCompleter::activated), this, &SubtitleTextEdit::insertCompletion);
}

SubtitleTextEdit::~SubtitleTextEdit() {}

void SubtitleTextEdit::keyPressEvent(QKeyEvent *event)
{
    if (completer && completer->popup()->isVisible()) {
        // The following keys are forwarded by the completer to the widget
        switch (event->key()) {
        case Qt::Key_Escape:
            completer->popup()->hide();
            event->accept();
            return;
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            event->ignore();
            return;
        default:
            break;
        }
    } else {
        switch (event->key()) {
        // disable newline
        case Qt::Key_Enter:
        case Qt::Key_Return:
            Q_EMIT triggerUpdate();
            event->ignore();
            return;
        default:
            break;
        }
    }

    QTextCursor tc = textCursor();
    switch (event->key()) {
    // auto complete the right brace and right parenthesis
    case Qt::Key_BraceLeft:
        // must not be commented by '\'
        // not in block, at the left of start, or at the right of end
        if (!(!tc.atBlockStart() && tc.block().text().at(tc.positionInBlock() - 1) == '\\')) {
            if (!selectOverrideBlock(tc).hasSelection() || (!tc.atBlockStart() && tc.block().text().at(tc.positionInBlock() - 1) == '}') ||
                (!tc.atBlockEnd() && tc.block().text().at(tc.positionInBlock()) == '{')) {
                tc.insertText("{}");
                tc.movePosition(QTextCursor::Left);
                setTextCursor(tc);
                return;
            }
        }
        break;
    case Qt::Key_ParenLeft:
        // in block, not at the left of start, and not at the right of end
        if (selectOverrideBlock(tc).hasSelection() && (!tc.atBlockStart() && tc.block().text().at(tc.positionInBlock() - 1) != '}') &&
            (!tc.atBlockEnd() && tc.block().text().at(tc.positionInBlock()) != '{')) {
            tc.insertText("()");
            tc.movePosition(QTextCursor::Left);
            setTextCursor(tc);
            completer->popup()->hide();
            return;
        }
        break;
    case Qt::Key_BraceRight:
        if ((selectOverrideBlock(tc).hasSelection()) && (!tc.atBlockStart() && tc.block().text().at(tc.positionInBlock() - 1) == '{') &&
            (!tc.atBlockEnd() && tc.block().text().at(tc.positionInBlock()) == '}')) {
            tc.movePosition(QTextCursor::Right);
            setTextCursor(tc);
            return;
        }
        break;
    case Qt::Key_ParenRight:
        // move right directly if it's in a empty block
        if ((selectOverrideBlock(tc).hasSelection()) && (!tc.atBlockStart() && tc.block().text().at(tc.positionInBlock() - 1) == '(') &&
            (!tc.atBlockEnd() && tc.block().text().at(tc.positionInBlock()) == ')')) {
            tc.movePosition(QTextCursor::Right);
            setTextCursor(tc);
            return;
        }
        break;
    // delete the right brace and right parenthesis if it's in a empty block
    case Qt::Key_Backspace:
        if (selectOverrideBlock(tc).hasSelection() && selectOverrideBlock(tc).selectedText().length() == 2) {
            if (((!tc.atBlockStart() && tc.block().text().at(tc.positionInBlock() - 1) == '{') &&
                 (!tc.atBlockEnd() && tc.block().text().at(tc.positionInBlock()) == '}'))) {
                tc.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
                tc.removeSelectedText();
                setTextCursor(tc);
            }
        }

        if ((!tc.atBlockStart() && tc.block().text().at(tc.positionInBlock() - 1) == '(') &&
            (!tc.atBlockEnd() && tc.block().text().at(tc.positionInBlock()) == ')')) {
            tc.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
            tc.removeSelectedText();
            setTextCursor(tc);
        }
        break;
    default:
        break;
    }

    const bool isShortcut = (event->modifiers().testFlag(Qt::ControlModifier) && event->key() == Qt::Key_E); // CTRL+E
    if (!completer || !isShortcut) // do not process the shortcut when we have a completer
        KTextEdit::keyPressEvent(event);
    const bool ctrlOrShift = event->modifiers().testFlag(Qt::ControlModifier) || event->modifiers().testFlag(Qt::ShiftModifier);
    if (!completer || (ctrlOrShift && event->text().isEmpty())) return;

    static QString eow("~!@#$%^&*_+(){}|:\"<>?,./;'[]-="); // end of word
    const bool hasModifier = (event->modifiers() != Qt::NoModifier) && !ctrlOrShift;
    QString completionPrefix = tagUnderCursor();

    if (!isShortcut && (hasModifier || event->text().isEmpty() || completionPrefix.length() == 0 || eow.contains(event->text().right(1)))) {
        completer->popup()->hide();
        return;
    }

    if (completionPrefix != completer->completionPrefix()) {
        completer->setCompletionPrefix(completionPrefix);
        completer->popup()->setCurrentIndex(completer->completionModel()->index(0, 0));
    }
    QRect cr = cursorRect();
    cr.setWidth(completer->popup()->sizeHintForColumn(0) + completer->popup()->verticalScrollBar()->sizeHint().width());
    completer->complete(cr);
}

void SubtitleTextEdit::focusOutEvent(QFocusEvent *event)
{
    KTextEdit::focusOutEvent(event);
    completer->popup()->hide();
    Q_EMIT triggerUpdate();
}

QTextCursor SubtitleTextEdit::selectOverrideBlock(const QTextCursor &cursor) const
{
    // \{ }    fail
    // \{ \}   fail
    // { \}    success
    // { }     success

    /*  Return a cursor with a selection of the nearest tag override block if
     *  (1) the cursor is inside a override block
     *  (2) the cursor is at the end of a override block
     *  (3) the cursor is at the beginning of a override block
     */

    QTextCursor newCursor = cursor;

    // check if it's on the right of '}'
    if (newCursor.positionInBlock() > 0) {
        if (newCursor.block().text().at(newCursor.positionInBlock() - 1) == '}') {
            int anchorPos = newCursor.positionInBlock();
            int offset = 0;
            for (int i = 2; i <= newCursor.positionInBlock(); i++) {
                // find the very first '{' on the left, exclude the '\{'
                if (newCursor.block().text().at(anchorPos - i) == '{' &&
                    (newCursor.positionInBlock() - i == 0 || newCursor.block().text().at(anchorPos - i - 1) != '\\')) {
                    offset = i;
                }
                if (newCursor.block().text().at(anchorPos - i) == '}') {
                    break;
                }
            }
            newCursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, offset);
            return newCursor;
        }
    }

    // check if it's on the left of '{', exclude the '\{'
    if (!newCursor.atBlockEnd()) {
        if (newCursor.block().text().at(newCursor.positionInBlock()) == '{' &&
            (newCursor.positionInBlock() == 0 || newCursor.block().text().at(newCursor.positionInBlock() - 1) != '\\')) {
            // find the very first '{' on the left
            int anchorPos = newCursor.positionInBlock();
            int offset = 0;
            for (int i = 1; i <= newCursor.positionInBlock(); i++) {
                if (newCursor.block().text().at(anchorPos - i) == '{' &&
                    (newCursor.positionInBlock() - i == 0 || newCursor.block().text().at(anchorPos - i - 1) != '\\')) {
                    offset = i;
                }
                if (newCursor.block().text().at(anchorPos - i) == '}') {
                    break;
                }
            }
            newCursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, offset);
            // select until the '}'
            anchorPos = newCursor.positionInBlock();
            offset = 0;
            for (int i = 1; i < newCursor.block().text().length() - newCursor.positionInBlock(); i++) {
                if (newCursor.block().text().at(anchorPos + i) == '}') {
                    offset = i + 1;
                    break;
                }
            }
            newCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, offset);
            return newCursor;
        }
    }

    // check if it's wrapped by '{...}'
    if (newCursor.positionInBlock() > 0 && newCursor.positionInBlock() < newCursor.block().text().length()) {
        int offset = 0;

        // find the very first '{' on the left
        int anchorPos = newCursor.positionInBlock();
        for (int i = 1; i <= newCursor.positionInBlock(); i++) {
            if (newCursor.block().text().at(anchorPos - i) == '{' && (anchorPos - i == 0 || newCursor.block().text().at(anchorPos - i - 1) != '\\')) {
                offset = i;
            }
            if (newCursor.block().text().at(anchorPos - i) == '}') {
                break;
            }
        }

        if (offset == 0) return newCursor;

        newCursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, offset);
        offset = 0;
        anchorPos = newCursor.positionInBlock();

        for (int i = 1; i < newCursor.block().text().length() - anchorPos; i++) {
            if (newCursor.block().text().at(anchorPos + i) == '}') {
                offset = i + 1;
                break;
            }
        }
        newCursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, offset);
        return newCursor;
    }

    return newCursor;
}

QString SubtitleTextEdit::tagUnderCursor() const
{
    QTextCursor cursor = textCursor();
    QTextCursor selectCursor = selectOverrideBlock(cursor);

    if (selectCursor.hasSelection()) {
        // check if the cursor is on the right of '}' or on the left of '{'
        if ((cursor.positionInBlock() != cursor.block().text().length() && cursor.block().text().at(cursor.positionInBlock()) != '{') &&
            (cursor.positionInBlock() != 0 && cursor.block().text().at(cursor.positionInBlock() - 1) != '}')) {
            for (int i = cursor.position(); i >= 0 && cursor.position() - i + 1 < maxASSTagNameLength; i--) { // out of the block
                if (cursor.block().text().at(i) == '{') {
                    break;
                }
                cursor.setPosition(i, QTextCursor::KeepAnchor);
                if (cursor.selectedText().startsWith('\\')) {
                    return cursor.selectedText();
                }
            }
        }
    }

    return QString();
}

void SubtitleTextEdit::insertCompletion(const QString &completion)
{
    if (completer->widget() != this) return;
    QTextCursor tc = textCursor();
    int extra = completion.length() - completer->completionPrefix().length();
    if (toPlainText().at(tc.position() - 1) != '\\') {
        tc.movePosition(QTextCursor::Left);
        tc.movePosition(QTextCursor::EndOfWord);
    }
    tc.insertText(completion.right(extra));
    setTextCursor(tc);
}
