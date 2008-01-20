/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#ifndef CUSTOMTRACKVIEW_H
#define CUSTOMTRACKVIEW_H

#include <QGraphicsView>

#include "clipitem.h"

class CustomTrackView : public QGraphicsView
{
  Q_OBJECT
  
  public:
    CustomTrackView(QGraphicsScene * scene, QWidget *parent=0);
    virtual void mousePressEvent ( QMouseEvent * event );
    virtual void mouseReleaseEvent ( QMouseEvent * event );
    virtual void mouseMoveEvent ( QMouseEvent * event );
    void addTrack();
    void removeTrack();
    void setCursorPos(int pos);
    int cursorPos();

  protected:
    virtual void drawBackground ( QPainter * painter, const QRectF & rect );
    virtual void drawForeground ( QPainter * painter, const QRectF & rect );
    virtual void dragEnterEvent ( QDragEnterEvent * event );
    virtual void dragMoveEvent(QDragMoveEvent * event);
    virtual void dragLeaveEvent ( QDragLeaveEvent * event );
    virtual void dropEvent ( QDropEvent * event );
    virtual QStringList mimeTypes() const;
    virtual Qt::DropActions supportedDropActions () const;

  private:
    int m_tracksCount;
    int m_cursorPos;
    ClipItem *m_dropItem;
    void addItem(QString producer, QPoint pos);

  signals:
    void cursorMoved(int);

};

#endif
