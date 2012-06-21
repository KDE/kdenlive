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

#include <KLocalizedString>
#include <KDebug>
#include <QMenu>
#include <QMouseEvent>


NotesWidget::NotesWidget(QWidget * parent) :
    KTextEdit(parent)
{
    m_insertTimecodeAction = new QAction(i18n("Insert current timecode"), this);
    connect(m_insertTimecodeAction, SIGNAL(triggered(bool)), this, SIGNAL(insertTimecode()));

    connect(this, SIGNAL(aboutToShowContextMenu(QMenu *)), this, SLOT(fillContextMenu(QMenu *)));

    setMouseTracking(true);
    setTabChangesFocus(true);
    setClickMessage(i18n("Enter your project notes here ..."));
}

void NotesWidget::fillContextMenu(QMenu *menu)
{
    menu->insertAction(menu->actions().at(0), m_insertTimecodeAction);
}

void NotesWidget::mouseMoveEvent(QMouseEvent * event)
{
    QString anchor = anchorAt(event->pos());
    if (anchor.isEmpty()) {
        viewport()->setCursor(Qt::IBeamCursor);
    } else {
        viewport()->setCursor(Qt::PointingHandCursor);
    }

    KTextEdit::mouseMoveEvent(event);
}

void NotesWidget::mousePressEvent(QMouseEvent * event)
{
    QString anchor = anchorAt(event->pos());
    if (anchor.isEmpty()) {
        KTextEdit::mousePressEvent(event);
    } else {
        emit seekProject(anchor.toInt());
        event->setAccepted(true);
    }
}
