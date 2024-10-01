/*  This file is part of Kdenlive. See www.kdenlive.org.
    SPDX-FileCopyrightText: 2024 Chengkun Chen <serix2004@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ktextedit.h"
#include "qcompleter.h"
#include "qstringlistmodel.h"

class ASSTagHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    ASSTagHighlighter(QTextDocument *parent = nullptr)
        : QSyntaxHighlighter(parent)
    {
    }

protected:
    void highlightBlock(const QString &text) override;
};

/**
 * @class SubtitleTextEdit: Subtitle edit widget
 * @brief A widget for editing subtitles with completion and syntax highlighting.
 * @author Chengkun Chen
 */
class SubtitleTextEdit : public KTextEdit
{
    Q_OBJECT
public:
    SubtitleTextEdit(QWidget *parent = nullptr);
    ~SubtitleTextEdit();

    /** @brief Return a cursor with a selection of the nearest tag override block */
    QTextCursor selectOverrideBlock(const QTextCursor &cursor) const;
    /** @brief Return the tagname under the current text cursor if any */
    QString tagUnderCursor() const;

private Q_SLOTS:
    void insertCompletion(const QString &completion);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    QCompleter *completer = nullptr;

Q_SIGNALS:
    void triggerUpdate();
};
