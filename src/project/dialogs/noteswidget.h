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

#include <QTextEdit>

/**
 * @class NotesWidget
 * @brief A small text editor to create project notes.
 * @author Jean-Baptiste Mardelle
 */

class NotesWidget : public QTextEdit
{
    Q_OBJECT
public:
    explicit NotesWidget(QWidget *parent = nullptr);
    ~NotesWidget() override;
    /** @brief insert current timeline timecode and focus widget to allow entering quick note */
    void addProjectNote();

protected:
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void insertFromMimeData(const QMimeData *source) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

public slots:
    void createMarkers();
    void assignProjectNote();

private:
    QAction *m_markerAction;
    void createMarker(QStringList anchors);
    QPair <QStringList, QList <QPoint> > getSelectedAnchors();

signals:
    void insertNotesTimecode();
    void seekProject(int);
    void reAssign(QStringList anchors, QList <QPoint> points);
};

#endif
