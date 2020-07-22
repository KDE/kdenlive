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
#include "bin/bin.h"

#include <QMenu>
#include <QMouseEvent>
#include <QMimeData>
#include <klocalizedstring.h>

NotesWidget::NotesWidget(QWidget *parent)
    : QTextEdit(parent)
{
    setMouseTracking(true);
}

NotesWidget::~NotesWidget() = default;

void NotesWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();
    if (menu) {
        QAction *a = new QAction(i18n("Insert current timecode"), menu);
        connect(a, &QAction::triggered, this, &NotesWidget::insertNotesTimecode);
        menu->insertAction(menu->actions().at(0), a);
        QPair <QStringList, QList <QPoint> > result = getSelectedAnchors();
        QStringList anchors = result.first;
        QList <QPoint> anchorPoints = result.second;
        if (anchors.isEmpty()) {
            const QString anchor = anchorAt(event->pos());
            if (!anchor.isEmpty()) {
                anchors << anchor;
            }
        }
        if (!anchors.isEmpty()) {
            a = new QAction(i18np("Create marker", "create markers", anchors.count()), menu);
            connect(a, &QAction::triggered, this, [this, anchors] () {
                createMarker(anchors);
            });
            menu->insertAction(menu->actions().at(1), a);
            if (!anchorPoints.isEmpty()) {
                a = new QAction(i18n("Assign timestamps to current Bin Clip"), menu);
                connect(a, &QAction::triggered, this, [this, anchors, anchorPoints] () {
                    emit reAssign(anchors, anchorPoints);
                });
                menu->insertAction(menu->actions().at(2), a);
            }
        }
        menu->exec(event->globalPos());
        delete menu;
    }
}

void NotesWidget::createMarker(QStringList anchors)
{
    QMap <QString, QList<int>> clipMarkers;
    QList<int> guides;
    for (const QString &anchor : anchors) {
        if (anchor.contains(QLatin1Char('#'))) {
            // That's a Bin Clip reference.
            const QString binId = anchor.section(QLatin1Char('#'), 0, 0);
            QList <int> timecodes;
            if (clipMarkers.contains(binId)) {
                timecodes = clipMarkers.value(binId);
                timecodes << anchor.section(QLatin1Char('#'), 1).toInt();
            } else {
                timecodes = {anchor.section(QLatin1Char('#'), 1).toInt()};
            }
            clipMarkers.insert(binId, timecodes);
        } else {
            // That is a guide
            guides << anchor.toInt();
        }
    }
    QMapIterator<QString, QList<int>> i(clipMarkers);
    while (i.hasNext()) {
        i.next();
        // That's a Bin Clip reference.
        pCore->bin()->addClipMarker(i.key(), i.value());
    }
    if (!clipMarkers.isEmpty()) {
        const QString &binId = clipMarkers.firstKey();
        pCore->selectBinClip(binId, clipMarkers.value(binId).constFirst(), QPoint());
    }
    if (!guides.isEmpty()) {
        pCore->addGuides(guides);
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
    if (anchor.isEmpty() || e->button() != Qt::LeftButton) {
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

QPair <QStringList, QList <QPoint> > NotesWidget::getSelectedAnchors()
{
    int startPos = textCursor().selectionStart();
    int endPos = textCursor().selectionEnd();
    QStringList anchors;
    QList <QPoint> anchorPoints;
    if (endPos > startPos) {
        textCursor().clearSelection();
        QTextCursor cur(textCursor());
        // Ensure we are at the start of current selection
        if (!cur.atBlockStart()) {
            cur.setPosition(startPos, QTextCursor::MoveAnchor);
            int pos = startPos;
            const QString an = anchorAt(cursorRect(cur).center());
            while (!cur.atBlockStart()) {
                pos--;
                cur.setPosition(pos, QTextCursor::MoveAnchor);
                if (anchorAt(cursorRect(cur).center()) == an) {
                    startPos = pos;
                } else {
                    break;
                }
            }
        }
        bool isInAnchor = false;
        QPoint anchorPoint;
        for (int p = startPos; p <= endPos; ++p) {
            cur.setPosition(p, QTextCursor::MoveAnchor);
            const QString anchor = anchorAt(cursorRect(cur).center());
            if (isInAnchor && !anchor.isEmpty() && p == endPos) {
                endPos++;
            }
            if (isInAnchor && (anchor.isEmpty() || !anchors.contains(anchor) || cur.atEnd())) {
                // End of current anchor
                anchorPoint.setY(p);
                anchorPoints.prepend(anchorPoint);
                isInAnchor = false;
            }
            if (!anchor.isEmpty() && !anchors.contains(anchor)) {
                anchors.prepend(anchor);
                if (!isInAnchor) {
                    isInAnchor = true;
                    anchorPoint.setX(p);
                }
            }
        }
    }
    return {anchors, anchorPoints};
}

void NotesWidget::assignProjectNote()
{
    QPair <QStringList, QList <QPoint> > result = getSelectedAnchors();
    QStringList anchors = result.first;
    QList <QPoint> anchorPoints = result.second;
    if (!anchors.isEmpty()) {
        emit reAssign(anchors, anchorPoints);
    } else {
        pCore->displayMessage(i18n("Select some timecodes to reassign"), InformationMessage);
    }
}

void NotesWidget::createMarkers()
{
    QPair <QStringList, QList <QPoint> > result = getSelectedAnchors();
    QStringList anchors = result.first;
    if (!anchors.isEmpty()) {
        createMarker(anchors);
    } else {
        pCore->displayMessage(i18n("Select some timecodes to create markers"), InformationMessage);
    }
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
    for (const QString &w : qAsConst(words)) {
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
