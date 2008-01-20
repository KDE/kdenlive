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

#include <KDebug>
#include <KUrl>

#include "customtrackview.h"
#include "clipitem.h"
#include "definitions.h"

CustomTrackView::CustomTrackView(QGraphicsScene * scene, QWidget *parent)
    : QGraphicsView(scene, parent), m_tracksCount(0), m_cursorPos(0), m_dropItem(NULL)
{
  setMouseTracking(true);
  setAcceptDrops(true);
}

// virtual 
void CustomTrackView::mouseMoveEvent ( QMouseEvent * event )
{
  int pos = event->x();
  if (event->modifiers() == Qt::ControlModifier)
    setDragMode(QGraphicsView::ScrollHandDrag);
  else if (event->modifiers() == Qt::ShiftModifier) 
    setDragMode(QGraphicsView::RubberBandDrag);
  else {
    QGraphicsItem * item = itemAt(event->pos());
    if (item) {
      while (item->parentItem()) 
	item = item->parentItem();
      int cursorPos = event->x();
      QRectF itemRect = item->sceneBoundingRect();
      int itemStart = mapFromScene(itemRect.x(), 0).x();
      int itemEnd = mapFromScene(itemRect.x() + itemRect.width(), 0).x();
      if (abs(itemStart - cursorPos) < 6 || abs(itemEnd - cursorPos) < 6)
	setCursor(Qt::SizeHorCursor);
      else setCursor(Qt::OpenHandCursor);

    }
    else {
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
    if (item) {
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
      setCursor(Qt::ArrowCursor);
      emit cursorMoved((int) mapToScene(event->x(), 0).x());
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
  m_dropItem = new ClipItem(elem.attribute("type").toInt(), clipName, elem.attribute("id").toInt(), QRectF(mapToScene(pos).x(), trackTop, out, 49));
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
  if (m_dropItem) delete m_dropItem;
  m_dropItem = NULL;
}

void CustomTrackView::dropEvent ( QDropEvent * event ) {
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
}

int CustomTrackView::cursorPos()
{
  return m_cursorPos;
}

void CustomTrackView::mouseReleaseEvent ( QMouseEvent * event )
{
  QGraphicsView::mouseReleaseEvent(event);
  setDragMode(QGraphicsView::NoDrag);
}

void CustomTrackView::drawBackground ( QPainter * painter, const QRectF & rect )  
{
  //kDebug()<<"/////  DRAWING BG: "<<rect.x()<<", width: "<<rect.width();
  painter->drawRect(rect);
    for (uint i = 0; i < m_tracksCount;i++)
    {
      painter->drawLine(rect.x(), 50 * i, rect.x() + rect.width(), 50 * i);
    }
}

void CustomTrackView::drawForeground ( QPainter * painter, const QRectF & rect )  
{
  //kDebug()<<"/////  DRAWING FB: "<<rect.x()<<", width: "<<rect.width();
  painter->drawLine(m_cursorPos, rect.y(), m_cursorPos, rect.y() + rect.height());
}

#include "customtrackview.moc"
