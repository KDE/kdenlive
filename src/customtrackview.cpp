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

#include <QMouseEvent>
#include <QStylePainter>
#include <QGraphicsItem>
#include <QDomDocument>
#include <QScrollBar>

#include <KDebug>
#include <KLocale>
#include <KUrl>

#include "customtrackview.h"
#include "clipitem.h"
#include "definitions.h"
#include "moveclipcommand.h"
#include "resizeclipcommand.h"
#include "addtimelineclipcommand.h"

CustomTrackView::CustomTrackView(KUndoStack *commandStack, QGraphicsScene * scene, QWidget *parent)
    : QGraphicsView(scene, parent), m_commandStack(commandStack), m_tracksCount(0), m_cursorPos(0), m_dropItem(NULL), m_cursorLine(NULL), m_operationMode(NONE), m_startPos(QPointF()), m_dragItem(NULL), m_visualTip(NULL), m_moveOpMode(NONE), m_animation(NULL)
{
  setMouseTracking(true);
  setAcceptDrops(true);
  m_animationTimer = new QTimeLine(800);
  m_animationTimer->setFrameRange(0, 5);
  m_animationTimer->setUpdateInterval(100);
  m_animationTimer->setLoopCount(0);
  m_tipColor = QColor(230, 50, 0, 150);
}

void CustomTrackView::initView()
{
  m_cursorLine = scene()->addLine(0, 0, 0, height());
  m_cursorLine->setZValue(1000);
}

// virtual
void CustomTrackView::resizeEvent ( QResizeEvent * event )
{
  if (m_cursorLine) m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), height());
}

// virtual
void CustomTrackView::wheelEvent ( QWheelEvent * e ) 
{
  if (e->modifiers() == Qt::ControlModifier) {
    if (e->delta() > 0) emit zoomIn();
    else emit zoomOut();
  }
  else {
    if (e->delta() > 0) horizontalScrollBar()->setValue (horizontalScrollBar()->value() + horizontalScrollBar()->singleStep ());
    else  horizontalScrollBar()->setValue (horizontalScrollBar()->value() - horizontalScrollBar()->singleStep ());
  }
}


// virtual 
void CustomTrackView::mouseMoveEvent ( QMouseEvent * event )
{
  int pos = event->x();
  /*if (event->modifiers() == Qt::ControlModifier)
    setDragMode(QGraphicsView::ScrollHandDrag);
  else if (event->modifiers() == Qt::ShiftModifier) 
    setDragMode(QGraphicsView::RubberBandDrag);
  else*/ {

      if (event->button() == Qt::LeftButton) {
	// a button was pressed, delete visual tips
	if (m_animation) delete m_animation;
	m_animation = NULL;
	if (m_visualTip) delete m_visualTip;
	m_visualTip = NULL;
	QGraphicsView::mouseMoveEvent(event);
	return;
      }

    QList<QGraphicsItem *> itemList = items( event->pos());
    int i = 0;
    QGraphicsItem *item = NULL;
    for (int i = 0; i < itemList.count(); i++) {
      if (itemList.at(i)->type() == 70000) {
	item = itemList.at(i);
	break;
      }
    }
    if (item) {
      ClipItem *clip = (ClipItem*) item;
      double size = mapToScene(QPoint(8, 0)).x();
      OPERATIONTYPE opMode = clip->operationMode(mapToScene(event->pos()));
      if (opMode == m_moveOpMode) {
	QGraphicsView::mouseMoveEvent(event);
	return;
      }
      else {
      if (m_visualTip) {
	if (m_animation) delete m_animation;
	m_animation = NULL;
	delete m_visualTip;
	m_visualTip = NULL;
      }
      }
      m_moveOpMode = opMode;
      if (opMode == MOVE) {
	setCursor(Qt::OpenHandCursor);
      }
      else if (opMode == RESIZESTART) {
	kDebug()<<"********  RESIZE CLIP START; WIDTH: "<<size;
	if (m_visualTip == NULL) {
	  m_visualTip = new QGraphicsRectItem(clip->rect().x(), clip->rect().y(), size, clip->rect().height());
	  ((QGraphicsRectItem*) m_visualTip)->setBrush(m_tipColor);
	  ((QGraphicsRectItem*) m_visualTip)->setPen(QPen(Qt::transparent));
	  m_visualTip->setZValue (100);
	  m_animation = new QGraphicsItemAnimation;
	  m_animation->setItem(m_visualTip);
	  m_animation->setTimeLine(m_animationTimer);
	  m_visualTip->setPos(0, 0);
	  double scale = 2.0;
	  m_animation->setScaleAt(.5, scale, 1);
	  m_animation->setPosAt(.5, QPointF(clip->rect().x() - clip->rect().x() * scale, 0));
	  scale = 1.0;
	  m_animation->setScaleAt(1, scale, 1);
	  m_animation->setPosAt(1, QPointF(clip->rect().x() - clip->rect().x() * scale, 0));
	  scene()->addItem(m_visualTip);
	  m_animationTimer->start();
	}
	setCursor(Qt::SizeHorCursor);
      }
      else if (opMode == RESIZEEND) {
	if (m_visualTip == NULL) {
	  m_visualTip = new QGraphicsRectItem(clip->rect().x() + clip->rect().width() - size, clip->rect().y(), size, clip->rect().height());
	  ((QGraphicsRectItem*) m_visualTip)->setBrush(m_tipColor);
	  ((QGraphicsRectItem*) m_visualTip)->setPen(QPen(Qt::transparent));
	  m_visualTip->setZValue (100);
	  m_animation = new QGraphicsItemAnimation;
	  m_animation->setItem(m_visualTip);
	  m_animation->setTimeLine(m_animationTimer);
	  m_visualTip->setPos(0, 0);
	  double scale = 2.0;
	  m_animation->setScaleAt(.5, scale, 1);
	  m_animation->setPosAt(.5, QPointF(clip->rect().x() - clip->rect().x() * scale - clip->rect().width(), 0));
	  scale = 1.0;
	  m_animation->setScaleAt(1, scale, 1);
	  m_animation->setPosAt(1, QPointF(clip->rect().x() - clip->rect().x() * scale, 0));
	  scene()->addItem(m_visualTip);
	  m_animationTimer->start();
	}
	setCursor(Qt::SizeHorCursor);
      }
      else if (opMode == FADEIN) {
	if (m_visualTip == NULL) {
	  m_visualTip = new QGraphicsEllipseItem(clip->rect().x() - size, clip->rect().y() - 8, size * 2, 16);
	  ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
	  ((QGraphicsEllipseItem*) m_visualTip)->setPen(QPen(Qt::transparent));
	  m_visualTip->setZValue (100);
	  m_animation = new QGraphicsItemAnimation;
	  m_animation->setItem(m_visualTip);
	  m_animation->setTimeLine(m_animationTimer);
	  m_visualTip->setPos(0, 0);
	  double scale = 2.0;
	  m_animation->setScaleAt(.5, scale, scale);
	  m_animation->setPosAt(.5, QPointF(clip->rect().x() - clip->rect().x() * scale, clip->rect().y() - clip->rect().y() * scale));
	  scale = 1.0;
	  m_animation->setScaleAt(1, scale, scale);
	  m_animation->setPosAt(1, QPointF(clip->rect().x() - clip->rect().x() * scale, clip->rect().y() - clip->rect().y() * scale));
	  scene()->addItem(m_visualTip);
	  m_animationTimer->start();
	}
	setCursor(Qt::PointingHandCursor);
      }
      else if (opMode == FADEOUT) {
	if (m_visualTip == NULL) {
	  m_visualTip = new QGraphicsEllipseItem(clip->rect().x() + clip->rect().width() - size, clip->rect().y() - 8, size*2, 16);
	  ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
	  ((QGraphicsEllipseItem*) m_visualTip)->setPen(QPen(Qt::transparent));
	  m_visualTip->setZValue (100);
	  m_animation = new QGraphicsItemAnimation;
	  m_animation->setItem(m_visualTip);
	  m_animation->setTimeLine(m_animationTimer);
	  m_visualTip->setPos(0, 0);
	  double scale = 2.0;
	  m_animation->setScaleAt(.5, scale, scale);	  
	  m_animation->setPosAt(.5, QPointF(clip->rect().x() - clip->rect().x() * scale - clip->rect().width(), clip->rect().y() - clip->rect().y() * scale));
	  scale = 1.0;
	  m_animation->setScaleAt(1, scale, scale);
	  m_animation->setPosAt(1, QPointF(clip->rect().x() - clip->rect().x() * scale, clip->rect().y() - clip->rect().y() * scale));
	  scene()->addItem(m_visualTip);
	  m_animationTimer->start();
	}
	setCursor(Qt::PointingHandCursor);
      }
    }
    else {
      m_moveOpMode = NONE;
      if (m_visualTip) {
	if (m_animation) delete m_animation;
	m_animation = NULL;
	delete m_visualTip;
	m_visualTip = NULL;
      }
      setCursor(Qt::ArrowCursor);
    }
  }
  QGraphicsView::mouseMoveEvent(event);
}

// virtual 
void CustomTrackView::mousePressEvent ( QMouseEvent * event )
{
  int pos = event->x();
  if (event->modifiers() == Qt::ControlModifier) 
    setDragMode(QGraphicsView::ScrollHandDrag);
  else if (event->modifiers() == Qt::ShiftModifier) 
    setDragMode(QGraphicsView::RubberBandDrag);
  else {
    QGraphicsItem * item = itemAt(event->pos());
    if (item && item->type() != 70000) item = item->parentItem();
    if (item && item->type() == 70000) {
      m_dragItem = (ClipItem *) item;
      m_operationMode = m_dragItem->operationMode(item->mapFromScene(mapToScene(event->pos())));
      if (m_operationMode == MOVE || m_operationMode == RESIZESTART) m_startPos = QPointF(m_dragItem->rect().x(), m_dragItem->rect().y());
      else if (m_operationMode == RESIZEEND) m_startPos = QPointF(m_dragItem->rect().x() + m_dragItem->rect().width(), m_dragItem->rect().y());

     kDebug()<<"//////// ITEM CLICKED: "<<m_startPos;
      /*while (item->parentItem()) 
	item = item->parentItem();

	int cursorPos = event->x();
	QRectF itemRect = item->sceneBoundingRect();
	int itemStart = mapFromScene(itemRect.x(), 0).x();
	int itemEnd = mapFromScene(itemRect.x() + itemRect.width(), 0).x();
	if (abs(itemStart - cursorPos) < 6)
	  ((ClipItem *) item )->setResizeMode(1);
	else if (abs(itemEnd - cursorPos) < 6)
	  ((ClipItem *) item )->setResizeMode(2);
    */}
    else {
      kDebug()<<"//////// NO ITEM FOUND ON CLICK";
      m_dragItem = NULL;
      setCursor(Qt::ArrowCursor);
      setCursorPos((int) mapToScene(event->x(), 0).x());
      emit cursorMoved(cursorPos());
    }
  }
  //kDebug()<<pos;
  QGraphicsView::mousePressEvent(event);
}

void CustomTrackView::dragEnterEvent ( QDragEnterEvent * event )
{
  if (event->mimeData()->hasText()) {
    QString clip = event->mimeData()->text();
    addItem(clip, event->pos());
    event->acceptProposedAction();
  }
}


void CustomTrackView::addItem(QString producer, QPoint pos)
{
  QDomDocument doc;
  doc.setContent(producer);
  QDomElement elem = doc.documentElement();
  int in = elem.attribute("in", 0).toInt();
  int out = elem.attribute("duration", 0).toInt();
  if (out == 0) out = elem.attribute("out", 0).toInt() - in;
  kDebug()<<"ADDING CLIP: "<<producer<<", OUT: "<<out<<", POS: "<<mapToScene(pos);
  int trackTop = ((int) mapToScene(pos).y()/50) * 50 + 1;
  QString clipName = elem.attribute("name");
  if (clipName.isEmpty()) clipName = KUrl(elem.attribute("resource")).fileName();
  m_dropItem = new ClipItem(elem.attribute("type").toInt(), clipName, elem.attribute("id").toInt(), out, QRectF(mapToScene(pos).x(), trackTop, out, 49));
  scene()->addItem(m_dropItem);
}


void CustomTrackView::dragMoveEvent(QDragMoveEvent * event) {
  event->setDropAction(Qt::IgnoreAction);
  if (m_dropItem) {
    int trackTop = ((int) mapToScene(event->pos()).y()/50) * 50 + 1;
    m_dropItem->moveTo(mapToScene(event->pos()).x(), trackTop - m_dropItem->rect().y());
  }
        //if (item) {
                event->setDropAction(Qt::MoveAction);
                if (event->mimeData()->hasText()) {
                        event->acceptProposedAction();
                }
        //}
}

void CustomTrackView::dragLeaveEvent ( QDragLeaveEvent * event ) {
  if (m_dropItem) {
    delete m_dropItem;
    m_dropItem = NULL;
  }
}

void CustomTrackView::dropEvent ( QDropEvent * event ) {
  if (m_dropItem) {
    AddTimelineClipCommand *command = new AddTimelineClipCommand(this, m_dropItem->clipType(), m_dropItem->clipName(), m_dropItem->clipProducer(), m_dropItem->maxDuration(), m_dropItem->rect(), false);
    m_commandStack->push(command);
  }
  m_dropItem = NULL;
}


QStringList CustomTrackView::mimeTypes () const
{
    QStringList qstrList;
    // list of accepted mime types for drop
    qstrList.append("text/plain");
    return qstrList;
}

Qt::DropActions CustomTrackView::supportedDropActions () const
{
    // returns what actions are supported when dropping
    return Qt::MoveAction;
}

void CustomTrackView::addTrack ()
{
  m_tracksCount++;
}

void CustomTrackView::removeTrack ()
{
  m_tracksCount--;
}

void CustomTrackView::setCursorPos(int pos)
{
  m_cursorPos = pos;
  m_cursorLine->setPos(pos, 0);
}

int CustomTrackView::cursorPos()
{
  return m_cursorPos;
}

void CustomTrackView::mouseReleaseEvent ( QMouseEvent * event )
{
  QGraphicsView::mouseReleaseEvent(event);
  setDragMode(QGraphicsView::NoDrag);
  if (m_dragItem == NULL) return;
  //kDebug()<<"/// MOVING CLIP: "<<m_startPos<<", END: "<<QPoint(m_dragItem->rect().x(),m_dragItem->rect().y());
  if (m_operationMode == MOVE) {
    // move clip
    MoveClipCommand *command = new MoveClipCommand(this, m_startPos, QPointF(m_dragItem->rect().x(), m_dragItem->rect().y()), false);
    m_commandStack->push(command);
  }
  else if (m_operationMode == RESIZESTART) {
    // resize start
    ResizeClipCommand *command = new ResizeClipCommand(this, m_startPos, QPointF(m_dragItem->rect().x(), m_dragItem->rect().y()), true, false);
    m_commandStack->push(command);
  }
  else if (m_operationMode == RESIZEEND) {
    // resize end
    ResizeClipCommand *command = new ResizeClipCommand(this, m_startPos, QPointF(m_dragItem->rect().x() + m_dragItem->rect().width(), m_dragItem->rect().y()), false, false);
    m_commandStack->push(command);
  }
}

void CustomTrackView::deleteClip ( const QRectF &rect )
{
  ClipItem *item = (ClipItem *) scene()->itemAt(rect.x() + 1, rect.y() + 1);
  if (!item) {
    kDebug()<<"----------------  ERROR, CANNOT find clip to move at: "<<rect.x();
    return;
  }
  delete item;
}

void CustomTrackView::addClip ( int clipType, QString clipName, int clipProducer, int maxDuration, const QRectF &rect )
{
  ClipItem *item = new ClipItem(clipType, clipName, clipProducer, maxDuration, rect);
  scene()->addItem(item);
}

void CustomTrackView::moveClip ( const QPointF &startPos, const QPointF &endPos )
{
  ClipItem *item = (ClipItem *) scene()->itemAt(startPos.x() + 1, startPos.y() + 1);
  if (!item) {
    kDebug()<<"----------------  ERROR, CANNOT find clip to move at: "<<startPos;
    return;
  }
  item->setRect(QRectF(endPos.x(), endPos.y(), item->rect().width(), item->rect().height()));
  QList <QGraphicsItem *> childrenList = item->children();
  for (int i = 0; i < childrenList.size(); ++i) {
    childrenList.at(i)->moveBy(endPos.x() - startPos.x() , endPos.y() - startPos.y());
  }
}

void CustomTrackView::resizeClip ( const QPointF &startPos, const QPointF &endPos, bool resizeClipStart )
{
  int offset;
  if (resizeClipStart) offset = 1;
  else offset = -1;
  ClipItem *item = (ClipItem *) scene()->itemAt(startPos.x() + offset, startPos.y() + 1);
  if (!item) {
    kDebug()<<"----------------  ERROR, CANNOT find clip to resize at: "<<startPos;
    return;
  }
  qreal diff = endPos.x() - startPos.x();
  if (resizeClipStart) {
    item->setRect(QRectF(endPos.x(), endPos.y(), item->rect().width() - diff, item->rect().height()));
    QList <QGraphicsItem *> childrenList = item->children();
    for (int i = 0; i < childrenList.size(); ++i) {
      childrenList.at(i)->moveBy(diff / 2 , endPos.y() - startPos.y());
    }
  }
  else {
    //kdDebug()<<"///////  RESIZE CLIP END: "<<item->rect().x()<<", "<<item->rect().width()<<", "<<startPos<<", "<<endPos;
    item->setRect(QRectF(item->rect().x(), item->rect().y(), endPos.x() - item->rect().x(), item->rect().height()));
    QList <QGraphicsItem *> childrenList = item->children();
    for (int i = 0; i < childrenList.size(); ++i) {
      childrenList.at(i)->moveBy(-diff/2, endPos.y() - startPos.y());
    }
  }
}


void CustomTrackView::drawBackground ( QPainter * painter, const QRectF & rect )  
{
  //kDebug()<<"/////  DRAWING BG: "<<rect.x()<<", width: "<<rect.width();
  painter->setPen(QColor(150, 150, 150, 255));
  painter->drawLine(rect.x(), 0, rect.x() + rect.width(), 0);
    for (uint i = 0; i < m_tracksCount;i++)
    {
      painter->drawLine(rect.x(), 50 * (i+1), rect.x() + rect.width(), 50 * (i+1));
      painter->drawText(QRectF(10, 50 * i, 100, 50 * i + 49), Qt::AlignLeft, i18n(" Track ") + QString::number(i));
    }
}
/*
void CustomTrackView::drawForeground ( QPainter * painter, const QRectF & rect )  
{
  //kDebug()<<"/////  DRAWING FB: "<<rect.x()<<", width: "<<rect.width();
  painter->fillRect(rect, QColor(50, rand() % 250,50,100));
  painter->drawLine(m_cursorPos, rect.y(), m_cursorPos, rect.y() + rect.height());
}
*/
#include "customtrackview.moc"
