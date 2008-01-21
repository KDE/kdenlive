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



#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <KDebug>


#include "clipitem.h"

ClipItem::ClipItem(int clipType, QString name, int producer, const QRectF & rect)
    : QGraphicsRectItem(rect), m_resizeMode(0), m_grabPoint(0), m_producer(producer)
{
  setToolTip(name);
  //setCursor(Qt::SizeHorCursor);
  setFlags(QGraphicsItem::ItemClipsToShape | QGraphicsItem::ItemClipsChildrenToShape | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
  //producer = "sjis isjisjsi sij ssao sa sao ";
  m_label = new LabelItem( name, this);
  QRectF textRect = m_label->boundingRect();
  m_textWidth = textRect.width();
  m_label->setPos(rect.x() + rect.width()/2 - m_textWidth/2, rect.y() + rect.height() / 2 - textRect.height()/2);
}

int ClipItem::type () const
{
  return 70000;
}

// virtual 
 void ClipItem::paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           QWidget *widget)
 {
    painter->setClipRect( option->exposedRect );
    painter->fillRect(rect(), QColor(200, 50, 50, 150));
    //kDebug()<<"ITEM REPAINT RECT: "<<boundingRect().width();
    //painter->drawText(rect(), Qt::AlignCenter, m_name);
    painter->drawRect(rect());
     //painter->drawRoundRect(-10, -10, 20, 20);
 }


int ClipItem::operationMode(QPointF pos)
{
    if (abs(pos.x() - rect().x()) < 6)
      return 1;
    else if (abs(pos.x() - (rect().x() + rect().width())) < 6)
      return 2;
    return 0;
}

// virtual
 void ClipItem::mousePressEvent ( QGraphicsSceneMouseEvent * event ) 
 {
    m_resizeMode = operationMode(event->pos());
    if (m_resizeMode == 0) {
      m_grabPoint = event->pos().x() - rect().x();
    }
    QGraphicsRectItem::mousePressEvent(event);
 }

// virtual
 void ClipItem::mouseReleaseEvent ( QGraphicsSceneMouseEvent * event ) 
 {
    m_resizeMode = 0;
    QGraphicsRectItem::mouseReleaseEvent(event);
 }

 void ClipItem::moveTo(double x, double offset)
 {
  double origX = rect().x();
  double origY = rect().y();
    setRect(x, origY + offset, rect().width(), rect().height());

    QList <QGraphicsItem *> collisionList = collidingItems();
    for (int i = 0; i < collisionList.size(); ++i) {
      QGraphicsItem *item = collisionList.at(i);
      if (item->type() == 70000)
      {
	if (offset == 0)
	{
	  QRectF other = ((QGraphicsRectItem *)item)->rect();
	  QPointF pos = mapFromItem(item, other.x()+other.width(), 0);
	  //mapToItem(collisionList.at(i), collisionList.at(i)->boundingRect()).boundingRect();
	  if (x < origX) {
	    kDebug()<<"COLLISION, MOVING TO------";
	    origX = other.x() + other.width(); 
	  }
	  else if (x > origX) {
	    kDebug()<<"COLLISION, MOVING TO+++";
	    origX = other.x() - rect().width(); 
	  }
	}
	setRect(origX, origY, rect().width(), rect().height());
	offset = 0;
	origX = rect().x();
	break;
      }
    }

    QList <QGraphicsItem *> childrenList = children();
    for (int i = 0; i < childrenList.size(); ++i) {
      childrenList.at(i)->moveBy(rect().x() - origX , offset);
    }
 }

// virtual
 void ClipItem::mouseMoveEvent ( QGraphicsSceneMouseEvent * event ) 
 {
    double moveX = event->scenePos().x();
    double originalX = rect().x();
    if (m_resizeMode == 1) {
      setRect(moveX, rect().y(), originalX + rect().width() - moveX, rect().height());
      QList <QGraphicsItem *> childrenList = children();
      for (int i = 0; i < childrenList.size(); ++i) {
	childrenList.at(i)->moveBy((moveX - originalX) / 2 , 0);
      }
      return;
    }
    if (m_resizeMode == 2) {
      setRect(originalX, rect().y(), moveX - originalX, rect().height());
      QList <QGraphicsItem *> childrenList = children();
      for (int i = 0; i < childrenList.size(); ++i) {
	childrenList.at(i)->moveBy(-(moveX - originalX) / 2 , 0);
      }
      return;
    }
    int moveTrack = (int) event->scenePos().y() / 50;
    int currentTrack = (int) rect().y() / 50;
    int offset = moveTrack - currentTrack;
    if (offset != 0) offset = 50 * offset;
    moveTo(moveX - m_grabPoint, offset);
    
 }


// virtual 
/*
void CustomTrackView::mousePressEvent ( QMouseEvent * event )
{
  int pos = event->x();
  if (event->modifiers() == Qt::ControlModifier) 
    setDragMode(QGraphicsView::ScrollHandDrag);
  else if (event->modifiers() == Qt::ShiftModifier) 
    setDragMode(QGraphicsView::RubberBandDrag);
  else {
    QGraphicsItem * item = itemAt(event->pos());
    if (item) {
    }
    else emit cursorMoved((int) mapToScene(event->x(), 0).x());
  }
  kDebug()<<pos;
  QGraphicsView::mousePressEvent(event);
}

void CustomTrackView::mouseReleaseEvent ( QMouseEvent * event )
{
  QGraphicsView::mouseReleaseEvent(event);
  setDragMode(QGraphicsView::NoDrag);
}
*/

#include "clipitem.moc"
