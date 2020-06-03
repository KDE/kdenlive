/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "noteswidget.h"
#include "kdenlive_debug.h"
#include "core.h"

#include <QMenu>
#include <QMouseEvent>
#include <QMimeData>
#include <klocalizedstring.h>

NotesWidget::NotesWidget(QWidget *parent)
    : QTextEdit(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &NotesWidget::customContextMenuRequested, this, &NotesWidget::slotFillNotesMenu);
    setMouseTracking(true);
}

NotesWidget::~NotesWidget() = default;

void NotesWidget::slotFillNotesMenu(const QPoint &pos)
{
    QMenu *menu = createStandardContextMenu();
    if (menu) {
        QAction *a = new QAction(i18n("Insert current timecode"), menu);
        connect(a, &QAction::triggered, this, &NotesWidget::insertNotesTimecode);
        menu->insertAction(menu->actions().at(0), a);
        menu->exec(viewport()->mapToGlobal(pos));
    }
}

void NotesWidget::mouseMoveEvent(QMouseEvent *e)
{
    const QString anchor = anchorAt(e->pos());
    if (anchor.isEmpty()) {
        viewport()->setCursor(Qt::IBeamCursor);
    } else {
        viewport()->setCursor(Qt::PointingHandCursor);
    }
    QTextEdit::mouseMoveEvent(e);
}

void NotesWidget::mousePressEvent(QMouseEvent *e)
{
    QString anchor = anchorAt(e->pos());
    if (anchor.isEmpty()) {
        QTextEdit::mousePressEvent(e);
        return;
    }
    if (anchor.contains(QLatin1Char('#'))) {
        // That's a Bin Clip reference.
        pCore->selectBinClip(anchor.section(QLatin1Char('#'), 0, 0), anchor.section(QLatin1Char('#'), 1).toInt(), QPoint());
    } else {
        emit seekProject(anchor.toInt());
    }
    e->setAccepted(true);
}

void NotesWidget::addProjectNote()
{
    if (!textCursor().atBlockStart()) {
        textCursor().movePosition(QTextCursor::EndOfBlock);
        insertPlainText(QStringLiteral("\n"));
    }
    emit insertNotesTimecode();
}

void NotesWidget::insertFromMimeData(const QMimeData *source)
{
    QString pastedText = source->text();
    // Check for timecodes
    QStringList words = pastedText.split(QLatin1Char(' '));
    QMap <QString, QString> replacementPatterns;
    for (const QString &w : words) {
        if (w.size() > 4 && w.size() < 13 && w.count(QLatin1Char(':')) > 1) {
            // This is probably a timecode
            int frames = pCore->timecode().getFrameCount(w);
            if (frames > 0) {
                pastedText.replace(w, QStringLiteral("<a href=\"") + QString::number(frames) + QStringLiteral("\">") + w + QStringLiteral("</a> "));
            }
        }
    }
    insertHtml(pastedText);
}
