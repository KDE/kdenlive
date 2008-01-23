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
#include <QGraphicsScene>
#include <KDebug>


#include "clipitem.h"

ClipItem::ClipItem(int clipType, QString name, int producer, int maxDuration, const QRectF & rect)
    : QGraphicsRectItem(rect), m_resizeMode(NONE), m_grabPoint(0), m_clipType(clipType), m_clipName(name), m_producer(producer), m_cropStart(0), m_cropDuration(maxDuration), m_maxDuration(maxDuration), m_maxTrack(0)
{
  setToolTip(name);
  //setCursor(Qt::SizeHorCursor);
  setFlags(QGraphicsItem::ItemClipsToShape | QGraphicsItem::ItemClipsChildrenToShape | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
  m_label = new LabelItem( name, this);
  QRectF textRect = m_label->boundingRect();
  m_textWidth = textRect.width();
  m_label->setPos(rect.x() + rect.width()/2 - m_textWidth/2, rect.y() + rect.height() / 2 - textRect.height()/2);
  setBrush(QColor(100, 100, 150));
}

int ClipItem::type () const
{
  return 70000;
}

int ClipItem::clipType()
{
  return m_clipType;
}

QString ClipItem::clipName()
{
  return m_clipName;
}

int ClipItem::clipProducer()
{
  return m_producer;
}

int ClipItem::maxDuration()
{
  return m_maxDuration;
}

// virtual 
 void ClipItem::paint(QPainter *painter,
                           const QStyleOptionGraphicsItem *option,
                           QWidget *widget)
 {

    //painter->setClipRect( option->exposedRect );
    /*painter->setRenderHint(QPainter::TextAntialiasing);
    painter->fillRect(rect(), QColor(200, 50, 50, 150));
    QPointF pos = option->matrix.map(QPointF(1.0, 1.0));
    //painter->setPen(QPen(Qt::black, 1.0 / option->levelOfDetail));
    painter->setPen(QPen(Qt::black, 1.0 / pos.x()));
    double scale = 1.0 / pos.x();
    //QPointF size = option->matrix.map(QPointF(1, 1));
    //double off = painter->pen().width();
    QRectF br = boundingRect();
    QRectF recta(rect().x(), rect().y(), scale,rect().height());
    painter->drawRect(recta);
    //painter->drawLine(rect().x() + 1, rect().y(), rect().x() + 1, rect().y() + rect().height());
    painter->drawLine(rect().x() + rect().width(), rect().y(), rect().x() + rect().width(), rect().y() + rect().height());
    painter->setPen(QPen(Qt::black, 1.0 / pos.y()));
    painter->drawLine(rect().x(), rect().y(), rect().x() + rect().width(), rect().y());
    painter->drawLine(rect().x(), rect().y() + rect().height(), rect().x() + rect().width(), rect().y() + rect().height());*/

    QGraphicsRectItem::paint(painter, option, widget);
    //QPen pen(Qt::green, 1.0 / size.x() + 0.5);
    //painter->setPen(pen);
    //painter->drawLine(rect().x(), rect().y(), rect().x() + rect().width(), rect().y());*/
    //kDebug()<<"ITEM REPAINT RECT: "<<boundingRect().width();
    //painter->drawText(rect(), Qt::AlignCenter, m_name);
    // painter->drawRect(boundingRect());
     //painter->drawRoundRect(-10, -10, 20, 20);
 }


OPERATIONTYPE ClipItem::operationMode(QPointF pos)
{
    if (abs(pos.x() - rect().x()) < 6) {
      if (abs(pos.y() - rect().y()) < 6) return FADEIN;
      return RESIZESTART;
    }
    else if (abs(pos.x() - (rect().x() + rect().width())) < 6) {
      if (abs(pos.y() - rect().y()) < 6) return FADEOUT;
      return RESIZEEND;
    }
    return MOVE;
}

// virtual
 void ClipItem::mousePressEvent ( QGraphicsSceneMouseEvent * event ) 
 {
    m_resizeMode = operationMode(event->pos());
    if (m_resizeMode == MOVE) {
      m_maxTrack = scene()->sceneRect().height();
      m_grabPoint = (int) (event->pos().x() - rect().x());
    }
    QGraphicsRectItem::mousePressEvent(event);
 }

// virtual
 void ClipItem::mouseReleaseEvent ( QGraphicsSceneMouseEvent * event ) 
 {
    m_resizeMode = NONE;
    QGraphicsRectItem::mouseReleaseEvent(event);
 }

 void ClipItem::moveTo(double x, double offset)
 {
  double origX = rect().x();
  double origY = rect().y();
    setRect(x, origY + offset, rect().width(), rect().height());

    QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisionList.size(); ++i) {
      QGraphicsItem *item = collisionList.at(i);
      if (item->type() == 70000)
      {
	if (offset == 0)
	{
	  QRectF other = ((QGraphicsRectItem *)item)->rect();
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
    double moveX = (int) event->scenePos().x();
    double originalX = rect().x();
    double originalWidth = rect().width();
    if (m_resizeMode == RESIZESTART) {
      if (m_cropStart - (originalX - moveX) < 0) moveX = originalX - m_cropStart;
      if (originalX + rect().width() - moveX < 1) moveX = originalX + rect().width() + 2;
      m_cropStart -= originalX - moveX;
      kDebug()<<"MOVE CLIP START TO: "<<event->scenePos()<<", CROP: "<<m_cropStart;
      setRect(moveX, rect().y(), originalX + rect().width() - moveX, rect().height());

      QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
      for (int i = 0; i < collisionList.size(); ++i) {
	QGraphicsItem *item = collisionList.at(i);
	  if (item->type() == 70000)
	  {
	    QRectF other = ((QGraphicsRectItem *)item)->rect();
	    int newStart = other.x() + other.width();
	    setRect(newStart, rect().y(), rect().x() + rect().width() - newStart, rect().height());
	    moveX = newStart;
	    break;
	  }
      }
      QList <QGraphicsItem *> childrenList = children();
      for (int i = 0; i < childrenList.size(); ++i) {
	childrenList.at(i)->moveBy((moveX - originalX) / 2 , 0);
      }
      return;
    }
    if (m_resizeMode == RESIZEEND) {
      int newWidth = moveX - originalX;
      if (newWidth < 1) newWidth = 2;
      if (newWidth > m_maxDuration) newWidth = m_maxDuration;
      setRect(originalX, rect().y(), newWidth, rect().height());

      QList <QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
      for (int i = 0; i < collisionList.size(); ++i) {
	QGraphicsItem *item = collisionList.at(i);
	  if (item->type() == 70000)
	  {
	    QRectF other = ((QGraphicsRectItem *)item)->rect();
	    newWidth = other.x() - rect().x();
	    setRect(rect().x(), rect().y(), newWidth, rect().height());
	    break;
	  }
      }

      QList <QGraphicsItem *> childrenList = children();
      for (int i = 0; i < childrenList.size(); ++i) {
	childrenList.at(i)->moveBy((newWidth - originalWidth) / 2 , 0);
      }
      return;
    }
    if (m_resizeMode == MOVE) {
      kDebug()<<"///////  MOVE CLIP, EVENT Y: "<<event->scenePos().y()<<", SCENE HEIGHT: "<<scene()->sceneRect().height();
      int moveTrack = (int) event->scenePos().y() / 50;
      int currentTrack = (int) rect().y() / 50;
      int offset = moveTrack - currentTrack;
      if (event->scenePos().y() >= m_maxTrack || event->scenePos().y() < 0) {
	offset = 0;
	kDebug()<<"%%%%%%%%%%%%%%%%%%%%%%%   MAX HEIGHT OVERLOOK";
      }
      if (offset != 0) offset = 50 * offset;
      moveTo(moveX - m_grabPoint, offset);
    }
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
