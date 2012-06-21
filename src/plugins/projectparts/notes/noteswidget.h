/***************************************************************************
 *   Copyright (C) 2011 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef NOTESWIDGET_H
#define NOTESWIDGET_H

#include <KTextEdit>

/**
 * @class NotesWidget
 * @brief A small text editor to create project notes.
 * @author Jean-Baptiste Mardelle
 */

class NotesWidget : public KTextEdit
{
    Q_OBJECT

public:
    NotesWidget(QWidget * parent = 0);

protected:
    void mouseMoveEvent(QMouseEvent * event);
    void mousePressEvent(QMouseEvent * event);
    
signals:
    void insertTimecode();
    void seekProject(int);

private slots:
    void fillContextMenu(QMenu *menu);

private:
    QAction *m_insertTimecodeAction;
};


#endif

