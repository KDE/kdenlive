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
#include <QGraphicsItemAnimation>
#include <QTimeLine>

#include <KUndoStack>

#include "kdenlivedoc.h"
#include "clipitem.h"

class CustomTrackView : public QGraphicsView
{
  Q_OBJECT
  
  public:
    CustomTrackView(KdenliveDoc *doc, QGraphicsScene * projectscene, QWidget *parent=0);
    virtual void mousePressEvent ( QMouseEvent * event );
    virtual void mouseReleaseEvent ( QMouseEvent * event );
    virtual void mouseMoveEvent ( QMouseEvent * event );
    void addTrack();
    void removeTrack();
    int cursorPos();
    void initView();
    void moveClip ( const QPointF &startPos, const QPointF &endPos );
    void resizeClip ( const QPointF &startPos, const QPointF &endPos, bool resizeClipStart );
    void addClip ( QDomElement xml, int clipId, int track, int startpos, const QRectF &rect, int duration);
    void deleteClip ( int track, int startpos, const QRectF &rect );
    void setDuration(int duration);
    void setScale(double scaleFactor);
    void deleteClip(int clipId);
    void slotAddEffect(QMap <QString, QString> filter);
    void addEffect(int track, GenTime pos, QString tag, QMap <QString, QString> args);
    void deleteEffect(int track, GenTime pos, QString tag);

  public slots:
    void setCursorPos(int pos, bool seek = true);

  protected:
    virtual void drawBackground ( QPainter * painter, const QRectF & rect );
    //virtual void drawForeground ( QPainter * painter, const QRectF & rect );
    virtual void dragEnterEvent ( QDragEnterEvent * event );
    virtual void dragMoveEvent(QDragMoveEvent * event);
    virtual void dragLeaveEvent ( QDragLeaveEvent * event );
    virtual void dropEvent ( QDropEvent * event );
    virtual void wheelEvent ( QWheelEvent * e );
    virtual QStringList mimeTypes() const;
    virtual Qt::DropActions supportedDropActions () const;
    virtual void resizeEvent ( QResizeEvent * event );

  private:
    int m_tracksCount;
    int m_projectDuration;
    int m_cursorPos;
    ClipItem *m_dropItem;
    KdenliveDoc *m_document;
    void addItem(DocClipBase *clip, QPoint pos);
    QGraphicsLineItem *m_cursorLine;
    QPointF m_startPos;
    OPERATIONTYPE m_operationMode;
    OPERATIONTYPE m_moveOpMode;
    ClipItem *m_dragItem;
    KUndoStack *m_commandStack;
    QGraphicsItem *m_visualTip;
    QGraphicsItemAnimation *m_animation;
    QTimeLine *m_animationTimer;
    QColor m_tipColor;
    double m_scale;
    int m_clickPoint;
    QList <int> m_snapPoints;
    void updateSnapPoints(ClipItem *selected);
    double getSnapPointForPos(double pos);

  signals:
    void cursorMoved(int);
    void zoomIn();
    void zoomOut();
    void mousePosition(int);

};

#endif
