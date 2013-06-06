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

#include <KLocale>
#include <KDebug>
#include <QMenu>
#include <QMouseEvent>


NotesWidget::NotesWidget(QWidget * parent) :
    KTextEdit(parent)
{
    connect(this, SIGNAL(aboutToShowContextMenu(QMenu*)), this, SLOT(slotFillNotesMenu(QMenu*)));
    setMouseTracking(true);
    setTabChangesFocus(true);
    setClickMessage(i18n("Enter your project notes here ..."));
}

NotesWidget::~NotesWidget()
{
}

void NotesWidget::slotFillNotesMenu(QMenu *menu)
{
    QAction *a = new QAction(i18n("Insert current timecode"), menu);
    connect(a, SIGNAL(triggered(bool)), this, SIGNAL(insertNotesTimecode()));
    menu->insertAction(menu->actions().at(0), a);
}

void NotesWidget::mouseMoveEvent(QMouseEvent * e)
{
    const QString anchor = anchorAt(e->pos());
    if (anchor.isEmpty())
        viewport()->setCursor(Qt::IBeamCursor);
    else
        viewport()->setCursor(Qt::PointingHandCursor);
    KTextEdit::mouseMoveEvent(e);
}

void NotesWidget::mousePressEvent(QMouseEvent * e)
{
    const QString anchor = anchorAt(e->pos());
    if (anchor.isEmpty()) {
        KTextEdit::mousePressEvent(e);
        return;
    }
    emit seekProject(anchor.toInt());
    e->setAccepted(true);
}


#include "noteswidget.moc"

