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


#ifndef CLIPITEM_H
#define CLIPITEM_H

#include <QGraphicsRectItem>
#include <QDomElement>
#include <QGraphicsSceneMouseEvent>

#include "definitions.h"
#include "kthumb.h"


class ClipItem : public QObject, public QGraphicsRectItem
{
  Q_OBJECT

  public:
    ClipItem(QDomElement xml, int track, int startpos, const QRectF & rect, int duration = -1);
    virtual ~ ClipItem();
    virtual void paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           QWidget *widget);
    virtual int type () const;
    void moveTo(int x, double scale, double offset, int newTrack);
    void resizeStart(int posx, double scale);
    void resizeEnd(int posx, double scale);
    OPERATIONTYPE operationMode(QPointF pos);
    int clipProducer();
    int clipType();
    QString clipName();
    int maxDuration();
    int track();
    void setTrack(int track);
    int startPos();
    int endPos();
    int duration();
    QDomElement xml() const;

  protected:
    virtual void mouseMoveEvent ( QGraphicsSceneMouseEvent * event );
    virtual void mouseReleaseEvent ( QGraphicsSceneMouseEvent * event );
    virtual void mousePressEvent ( QGraphicsSceneMouseEvent * event );

  private:
    QDomElement m_xml;
    int m_textWidth;
    OPERATIONTYPE m_resizeMode;
    int m_grabPoint;
    int m_producer;
    CLIPTYPE m_clipType;
    QString m_clipName;
    int m_maxDuration;
    int m_cropStart;
    int m_cropDuration;
    int m_maxTrack;
    int m_track;
    int m_startPos;
    QPixmap m_startPix;
    QPixmap m_endPix;
    KThumb *m_thumbProd;
    QTimer *startThumbTimer;
    QTimer *endThumbTimer;

  private slots:
    void slotThumbReady(int frame, QPixmap pix);
    void slotFetchThumbs();
    void slotGetStartThumb();
    void slotGetEndThumb();

  signals:
    void getThumb(int, int, int, int);

};

#endif
