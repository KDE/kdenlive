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
#include "addeffectcommand.h"
#include "editeffectcommand.h"

CustomTrackView::CustomTrackView(KdenliveDoc *doc, QGraphicsScene * projectscene, QWidget *parent)
    : QGraphicsView(projectscene, parent), m_tracksCount(0), m_cursorPos(0), m_dropItem(NULL), m_cursorLine(NULL), m_operationMode(NONE), m_startPos(QPointF()), m_dragItem(NULL), m_visualTip(NULL), m_moveOpMode(NONE), m_animation(NULL), m_projectDuration(0), m_scale(1.0), m_clickPoint(0), m_document(doc)
{
  if (doc) m_commandStack = doc->commandStack();
  else m_commandStack == NULL;
  setMouseTracking(true);
  setAcceptDrops(true);
  m_animationTimer = new QTimeLine(800);
  m_animationTimer->setFrameRange(0, 5);
  m_animationTimer->setUpdateInterval(100);
  m_animationTimer->setLoopCount(0);
  m_tipColor = QColor(230, 50, 0, 150);
  setContentsMargins(0, 0, 0, 0);
  if (projectscene) {
    m_cursorLine = projectscene->addLine(0, 0, 0, 50);
    m_cursorLine->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIgnoresTransformations);
    m_cursorLine->setZValue(1000);
  }
}

void CustomTrackView::initView()
{

}

// virtual
void CustomTrackView::resizeEvent ( QResizeEvent * event )
{
  QGraphicsView::resizeEvent(event);
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
  emit mousePosition(mapToScene(event->pos()).x() / m_scale);
  /*if (event->modifiers() == Qt::ControlModifier)
    setDragMode(QGraphicsView::ScrollHandDrag);
  else if (event->modifiers() == Qt::ShiftModifier) 
    setDragMode(QGraphicsView::RubberBandDrag);
  else*/ {

      if (m_dragItem) { //event->button() == Qt::LeftButton) {
	// a button was pressed, delete visual tips
	if (m_operationMode == MOVE) {
	  double snappedPos = getSnapPointForPos(mapToScene(event->pos()).x() - m_clickPoint);
	  double moveX = snappedPos; //mapToScene(event->pos()).x();
	  //kDebug()<<"///////  MOVE CLIP, EVENT Y: "<<event->scenePos().y()<<", SCENE HEIGHT: "<<scene()->sceneRect().height();
	  int moveTrack = (int)  mapToScene(event->pos()).y() / 50;
	  int currentTrack = m_dragItem->track();

	  if (moveTrack > m_tracksCount - 1) moveTrack = m_tracksCount - 1;
	  else if (moveTrack < 0) moveTrack = 0;

	  int offset = moveTrack - currentTrack;
	  if (offset != 0) offset = 50 * offset;
	  m_dragItem->moveTo(moveX / m_scale, m_scale, offset, moveTrack);
	}
	else if (m_operationMode == RESIZESTART) {
	  int pos = mapToScene(event->pos()).x();
	  m_dragItem->resizeStart(pos / m_scale, m_scale);
	}
	else if (m_operationMode == RESIZEEND) {
	  int pos = mapToScene(event->pos()).x();
	  m_dragItem->resizeEnd(pos / m_scale, m_scale);
	}
	else if (m_operationMode == FADEIN) {
	  int pos = mapToScene(event->pos()).x() / m_scale;
	  m_dragItem->setFadeIn(pos - m_dragItem->startPos(), m_scale);
	}
	else if (m_operationMode == FADEOUT) {
	  int pos = mapToScene(event->pos()).x() / m_scale;
	  m_dragItem->setFadeOut(m_dragItem->endPos() - pos, m_scale);
	}

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
      OPERATIONTYPE opMode = clip->operationMode(mapToScene(event->pos()), m_scale);
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
	  m_visualTip = new QGraphicsEllipseItem(clip->rect().x() + clip->fadeIn() * m_scale - size, clip->rect().y() - 8, size * 2, 16);
	  ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
	  ((QGraphicsEllipseItem*) m_visualTip)->setPen(QPen(Qt::transparent));
	  m_visualTip->setZValue (100);
	  m_animation = new QGraphicsItemAnimation;
	  m_animation->setItem(m_visualTip);
	  m_animation->setTimeLine(m_animationTimer);
	  m_visualTip->setPos(0, 0);
	  double scale = 2.0;
	  m_animation->setScaleAt(.5, scale, scale);
	  m_animation->setPosAt(.5, QPointF(clip->rect().x() - clip->rect().x() * scale -  clip->fadeIn() * m_scale, clip->rect().y() - clip->rect().y() * scale));
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
	  m_visualTip = new QGraphicsEllipseItem(clip->rect().x() + clip->rect().width() - clip->fadeOut() * m_scale - size, clip->rect().y() - 8, size*2, 16);
	  ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
	  ((QGraphicsEllipseItem*) m_visualTip)->setPen(QPen(Qt::transparent));
	  m_visualTip->setZValue (100);
	  m_animation = new QGraphicsItemAnimation;
	  m_animation->setItem(m_visualTip);
	  m_animation->setTimeLine(m_animationTimer);
	  m_visualTip->setPos(0, 0);
	  double scale = 2.0;
	  m_animation->setScaleAt(.5, scale, scale);	  
	  m_animation->setPosAt(.5, QPointF(clip->rect().x() - clip->rect().x() * scale - clip->rect().width() + clip->fadeOut() * m_scale, clip->rect().y() - clip->rect().y() * scale));
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
  kDebug()<<"-- TIMELINE MSE PRESSED";
  int pos = event->x();
  if (event->modifiers() == Qt::ControlModifier) {
    setDragMode(QGraphicsView::ScrollHandDrag);
    QGraphicsView::mousePressEvent(event);
    return;
  }
  else if (event->modifiers() == Qt::ShiftModifier) {
    setDragMode(QGraphicsView::RubberBandDrag);
    QGraphicsView::mousePressEvent(event);
    return;
  }
  else {
    bool collision = false;
    QList<QGraphicsItem *> collisionList = items(event->pos());
    for (int i = 0; i < collisionList.size(); ++i) {
      QGraphicsItem *item = collisionList.at(i);
      if (item->type() == 70000) {
	// select item
	if (!item->isSelected()) {
	  QList<QGraphicsItem *> itemList = items();
	  for (int i = 0; i < itemList.count(); i++)
	    itemList.at(i)->setSelected(false);
	  item->setSelected(true);
	  update();
	}
	m_dragItem = (ClipItem *) item;
	emit clipItemSelected(m_dragItem);
	m_clickPoint = mapToScene(event->pos()).x() - m_dragItem->startPos() * m_scale;
	m_operationMode = m_dragItem->operationMode(item->mapFromScene(mapToScene(event->pos())), m_scale);
	if (m_operationMode == MOVE || m_operationMode == RESIZESTART) 
	  m_startPos = QPointF(m_dragItem->startPos(), m_dragItem->track());
	else if (m_operationMode == RESIZEEND) 
	  m_startPos = QPointF(m_dragItem->endPos(), m_dragItem->track());
	kDebug()<<"//////// ITEM CLICKED: "<<m_startPos;
	collision = true;
	break;
      }
    }
    if (!collision) {
      kDebug()<<"//////// NO ITEM FOUND ON CLICK";
      m_dragItem = NULL;
      setCursor(Qt::ArrowCursor);
      QList<QGraphicsItem *> itemList = items();
      for (int i = 0; i < itemList.count(); i++)
	itemList.at(i)->setSelected(false);
      emit clipItemSelected(NULL);
      setCursorPos((int) mapToScene(event->x(), 0).x());
      emit cursorMoved(cursorPos());
    }
  }
  updateSnapPoints(m_dragItem);
  //kDebug()<<pos;
  //QGraphicsView::mousePressEvent(event);
}

void CustomTrackView::dragEnterEvent( QDragEnterEvent * event )
{
  if (event->mimeData()->hasFormat("kdenlive/producerslist")) {
    kDebug()<<"///////////////  DRAG ENTERED, TEXT: "<<event->mimeData()->data("kdenlive/producerslist");
    QStringList ids = QString(event->mimeData()->data("kdenlive/producerslist")).split(";");
    //TODO: drop of several clips
    for (int i = 0; i < ids.size(); ++i) {
    }
    DocClipBase *clip = m_document->getBaseClip(ids.at(0).toInt());
    if (clip == NULL) kDebug()<<" WARNING))))))))) CLIP NOT FOUND : "<<ids.at(0).toInt();
    addItem(clip, event->pos());
    event->acceptProposedAction();
  }
  else QGraphicsView::dragEnterEvent(event);
}

void CustomTrackView::slotRefreshEffects(ClipItem *clip)
{
  int track = m_tracksCount - clip->track();
  GenTime pos = GenTime(clip->startPos(), m_document->fps());
  m_document->renderer()->mltRemoveEffect(track, pos, "-1", false);
  for (int i = 0; i < clip->effectsCount(); i++) {
    m_document->renderer()->mltAddEffect(track, pos, clip->getEffectArgs(clip->effectAt(i)), false);
  }
  m_document->renderer()->doRefresh();
}

void CustomTrackView::addEffect(int track, GenTime pos, QDomElement effect)
{
  ClipItem *clip = getClipItemAt(pos.frames(m_document->fps()) + 1, m_tracksCount - track);
  if (clip){ 
    QMap <QString, QString> effectParams = clip->addEffect(effect);
    m_document->renderer()->mltAddEffect(track, pos, effectParams);
    emit clipItemSelected(clip);
  }
}

void CustomTrackView::deleteEffect(int track, GenTime pos, QDomElement effect)
{
  QString index = effect.attribute("kdenlive_ix");
  m_document->renderer()->mltRemoveEffect(track, pos, index);  
  ClipItem *clip = getClipItemAt(pos.frames(m_document->fps()) + 1, m_tracksCount - track);
	if (clip){
		clip->deleteEffect(index);
		emit clipItemSelected(clip);
	}
}

void CustomTrackView::slotAddEffect(QDomElement effect, GenTime pos, int track)
{
  QList<QGraphicsItem *> itemList;
  if (track == -1)
    itemList = items();
  else {
    ClipItem *clip = getClipItemAt(pos.frames(m_document->fps()) + 1, track);
    if (clip) itemList.append(clip);
    else kDebug()<<"------   wrning, clip eff not found";
  }
  kDebug()<<"// REQUESTING EFFECT ON CLIP: "<<pos.frames(25)<<", TRK: "<<track;
  for (int i = 0; i < itemList.count(); i++) {
    if (itemList.at(i)->type() == 70000 && (itemList.at(i)->isSelected() || track != -1)) {
      ClipItem *item = (ClipItem *)itemList.at(i);
      // the kdenlive_ix int is used to identify an effect in mlt's playlist, should
      // not be changed
      effect.setAttribute("kdenlive_ix", QString::number(item->effectsCounter()));
      AddEffectCommand *command = new AddEffectCommand(this, m_tracksCount - item->track(),GenTime(item->startPos(), m_document->fps()), effect, true);
      m_commandStack->push(command);    
    }
  }
}

void CustomTrackView::slotDeleteEffect(ClipItem *clip, QDomElement effect)
{
  AddEffectCommand *command = new AddEffectCommand(this, m_tracksCount - clip->track(), GenTime(clip->startPos(), m_document->fps()), effect, false);
  m_commandStack->push(command);
}

void CustomTrackView::updateEffect(int track, GenTime pos, QDomElement effect)
{
  ClipItem *clip = getClipItemAt(pos.frames(m_document->fps()) + 1, m_tracksCount - track);
  if (clip){
    QMap <QString, QString> effectParams = clip->getEffectArgs(effect);
    m_document->renderer()->mltEditEffect(m_tracksCount - clip->track(), GenTime(clip->startPos(), m_document->fps()), effectParams);
  }
}


void CustomTrackView::slotUpdateClipEffect(ClipItem *clip, QDomElement oldeffect, QDomElement effect)
{
  EditEffectCommand *command = new EditEffectCommand(this, m_tracksCount - clip->track(), GenTime(clip->startPos(), m_document->fps()), oldeffect, effect, true);
  m_commandStack->push(command);
}


void CustomTrackView::addItem(DocClipBase *clip, QPoint pos)
{
  int in =0;
  int out = clip->duration().frames(m_document->fps());
  //kdDebug()<<"- - - -CREATING CLIP, duration = "<<out<<", URL: "<<clip->fileURL();
  int trackTop = ((int) mapToScene(pos).y()/50) * 50 + 1;
  m_dropItem = new ClipItem(clip, ((int) mapToScene(pos).y()/50), in, QRectF(mapToScene(pos).x() * m_scale, trackTop, out * m_scale, 49), out);
  scene()->addItem(m_dropItem);
}


void CustomTrackView::dragMoveEvent(QDragMoveEvent * event) {
  event->setDropAction(Qt::IgnoreAction);
  if (m_dropItem) {
    int track = (int) mapToScene(event->pos()).y()/50; //) * (m_scale * 50) + m_scale;
     kDebug()<<"+++++++++++++   DRAG MOVE, : "<<mapToScene(event->pos()).x()<<", SCAL: "<<m_scale;
    m_dropItem->moveTo(mapToScene(event->pos()).x() / m_scale, m_scale, (track - m_dropItem->track()) * 50, track);
  }
       //if (item) {
  event->setDropAction(Qt::MoveAction);
  if (event->mimeData()->hasFormat("kdenlive/producerslist")) {
    event->acceptProposedAction();
  }
  else QGraphicsView::dragMoveEvent(event);
        //}
}

void CustomTrackView::dragLeaveEvent ( QDragLeaveEvent * event ) {
  if (m_dropItem) {
    delete m_dropItem;
    m_dropItem = NULL;
  }
  else QGraphicsView::dragLeaveEvent(event);
}

void CustomTrackView::dropEvent ( QDropEvent * event ) {
  if (m_dropItem) {
    AddTimelineClipCommand *command = new AddTimelineClipCommand(this, m_dropItem->xml(), m_dropItem->clipProducer(), m_dropItem->track(), m_dropItem->startPos(), m_dropItem->rect(), m_dropItem->duration(), false, false);
    m_commandStack->push(command);
    m_dropItem->baseClip()->addReference();
    m_document->updateClip(m_dropItem->baseClip()->getId());
    kDebug()<<"IIIIIIIIIIIIIIIIIIIIIIII TRAX CNT: "<<m_tracksCount<<", DROP: "<<m_dropItem->track();
    m_document->renderer()->mltInsertClip(m_tracksCount - m_dropItem->track(), GenTime(m_dropItem->startPos(), m_document->fps()), m_dropItem->xml());
  }
  else QGraphicsView::dropEvent(event);  
  m_dropItem = NULL;
}


QStringList CustomTrackView::mimeTypes () const
{
    QStringList qstrList;
    // list of accepted mime types for drop
    qstrList.append("text/plain");
    qstrList.append("kdenlive/producerslist");
    return qstrList;
}

Qt::DropActions CustomTrackView::supportedDropActions () const
{
    // returns what actions are supported when dropping
    return Qt::MoveAction;
}

void CustomTrackView::setDuration(int duration)
{
  kDebug()<<"/////////////  PRO DUR: "<<duration<<", height: "<<50 * m_tracksCount;
  m_projectDuration = duration;
  scene()->setSceneRect(0, 0, (m_projectDuration + 500) * m_scale, scene()->sceneRect().height()); //50 * m_tracksCount);
}


void CustomTrackView::addTrack ()
{
  m_tracksCount++;
  m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), 50 * m_tracksCount);
  //setSceneRect(0, 0, sceneRect().width(), 50 * m_tracksCount);
  //verticalScrollBar()->setMaximum(50 * m_tracksCount); 
  //setFixedHeight(50 * m_tracksCount);
}

void CustomTrackView::removeTrack ()
{
  m_tracksCount--;
  m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), 50 * m_tracksCount);
}

void CustomTrackView::deleteClip(int clipId)
{
  QList<QGraphicsItem *> itemList = items();
  for (int i = 0; i < itemList.count(); i++) {
    if (itemList.at(i)->type() == 70000) {
      ClipItem *item = (ClipItem *)itemList.at(i);
      if (item->clipProducer() == clipId) {
	AddTimelineClipCommand *command = new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), item->track(), item->startPos(), item->rect(), item->duration(), true, true);
	m_commandStack->push(command);
	//delete item;
      }
    }
  }
}

void CustomTrackView::setCursorPos(int pos, bool seek)
{
  m_cursorPos = pos;
  m_cursorLine->setPos(pos, 0);
  int frame = mapToScene(QPoint(pos, 0)).x() / m_scale;
  if (seek) m_document->renderer()->seek(GenTime(frame, m_document->fps()));
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
  if (m_operationMode == MOVE && m_startPos.x() != m_dragItem->startPos()) {
    // move clip
    MoveClipCommand *command = new MoveClipCommand(this, m_startPos, QPointF(m_dragItem->startPos(), m_dragItem->track()), false);
    m_commandStack->push(command);
    m_document->renderer()->mltMoveClip(m_tracksCount - m_startPos.y(), m_tracksCount - m_dragItem->track(), m_startPos.x(), m_dragItem->startPos());
  }
  else if (m_operationMode == RESIZESTART) {
    // resize start
    ResizeClipCommand *command = new ResizeClipCommand(this, m_startPos, QPointF(m_dragItem->startPos(), m_dragItem->track()), true, false);

    m_document->renderer()->mltResizeClipStart(m_tracksCount - m_dragItem->track(), GenTime(m_dragItem->endPos(), m_document->fps()), GenTime(m_dragItem->startPos(), m_document->fps()), GenTime(m_startPos.x(), m_document->fps()), GenTime(m_dragItem->cropStart(), m_document->fps()), GenTime(m_dragItem->cropStart(), m_document->fps()) + GenTime(m_dragItem->endPos(), m_document->fps()) - GenTime(m_dragItem->startPos(), m_document->fps()));
    m_commandStack->push(command);
    m_document->renderer()->doRefresh();
  }
  else if (m_operationMode == RESIZEEND) {
    // resize end
    ResizeClipCommand *command = new ResizeClipCommand(this, m_startPos, QPointF(m_dragItem->endPos(), m_dragItem->track()), false, false);

    m_document->renderer()->mltResizeClipEnd(m_tracksCount - m_dragItem->track(), GenTime(m_dragItem->startPos(), m_document->fps()), GenTime(m_dragItem->cropStart(), m_document->fps()), GenTime(m_dragItem->cropStart(), m_document->fps()) + GenTime(m_dragItem->endPos(), m_document->fps()) - GenTime(m_dragItem->startPos(), m_document->fps()));
    m_commandStack->push(command);
    m_document->renderer()->doRefresh();
  }
  m_operationMode = NONE;
  m_dragItem = NULL; 
}

void CustomTrackView::deleteClip (int track, int startpos, const QRectF &rect )
{
  ClipItem *item = getClipItemAt(startpos, track);
  if (!item) {
    kDebug()<<"----------------  ERROR, CANNOT find clip to move at: "<<rect.x();
    return;
  }
  item->baseClip()->removeReference();
  m_document->updateClip(item->baseClip()->getId());
  delete item;
  m_document->renderer()->mltRemoveClip(m_tracksCount - track, GenTime(startpos, m_document->fps()));
  m_document->renderer()->doRefresh();
}

void CustomTrackView::addClip ( QDomElement xml, int clipId, int track, int startpos, const QRectF &rect, int duration )
{
  QRect r(startpos * m_scale, 50 * track, duration * m_scale, 49); 
  DocClipBase *baseclip = m_document->clipManager()->getClipById(clipId);
  ClipItem *item = new ClipItem(baseclip, track, startpos, r, duration);
  scene()->addItem(item);
  baseclip->addReference();
  m_document->updateClip(baseclip->getId());
  m_document->renderer()->mltInsertClip(m_tracksCount - track, GenTime(startpos, m_document->fps()), xml);
  m_document->renderer()->doRefresh();
}

ClipItem *CustomTrackView::getClipItemAt(int pos, int track)
{
  return (ClipItem *) scene()->itemAt(pos * m_scale, track * 50 + 25);
}

void CustomTrackView::moveClip ( const QPointF &startPos, const QPointF &endPos )
{
  ClipItem *item = getClipItemAt(startPos.x() + 1, startPos.y());
  if (!item) {
    kDebug()<<"----------------  ERROR, CANNOT find clip to move at: "<<startPos.x() * m_scale * FRAME_SIZE + 1<<", "<<startPos.y() * 50 + 25;
    return;
  }
  kDebug()<<"----------------  Move CLIP FROM: "<<startPos.x()<<", END:"<<endPos.x();
  item->moveTo(endPos.x(), m_scale, (endPos.y() - startPos.y()) * 50, endPos.y());
  m_document->renderer()->mltMoveClip(m_tracksCount - startPos.y(), m_tracksCount - endPos.y(), startPos.x(), endPos.x());
}

void CustomTrackView::resizeClip ( const QPointF &startPos, const QPointF &endPos, bool resizeClipStart )
{
  int offset;
  if (resizeClipStart) offset = 1;
  else offset = -1;
  ClipItem *item = getClipItemAt(startPos.x() + offset, startPos.y());
  if (!item) {
    kDebug()<<"----------------  ERROR, CANNOT find clip to resize at: "<<startPos;
    return;
  }
  qreal diff = endPos.x() - startPos.x();
  if (resizeClipStart) {
    m_document->renderer()->mltResizeClipStart(m_tracksCount - item->track(), GenTime(item->endPos(), m_document->fps()), GenTime(endPos.x(), m_document->fps()), GenTime(item->startPos(), m_document->fps()), GenTime(item->cropStart() + diff, m_document->fps()), GenTime(item->cropStart() + diff, m_document->fps()) + GenTime(item->endPos(), m_document->fps()) - GenTime(endPos.x(), m_document->fps()));
    item->resizeStart(endPos.x(), m_scale);
  }
  else {
    m_document->renderer()->mltResizeClipEnd(m_tracksCount - item->track(), GenTime(item->startPos(), m_document->fps()), GenTime(item->cropStart(), m_document->fps()), GenTime(item->cropStart(), m_document->fps()) + GenTime(endPos.x(), m_document->fps()) - GenTime(item->startPos(), m_document->fps()));
    item->resizeEnd(endPos.x(), m_scale);
  }
  m_document->renderer()->doRefresh();
}

double CustomTrackView::getSnapPointForPos(double pos)
{
  for (int i = 0; i < m_snapPoints.size(); ++i) {
    //kDebug()<<"SNAP POINT: "<<m_snapPoints.at(i);
    if (abs(pos - m_snapPoints.at(i) * m_scale) < 6 * m_scale) {
      //kDebug()<<" FOUND SNAP POINT AT: "<<m_snapPoints.at(i)<<", current pos: "<<pos / m_scale;
      return m_snapPoints.at(i) * m_scale + 0.5;
    }
    if (m_snapPoints.at(i) > pos) break;
  }
  return pos;
}

void CustomTrackView::updateSnapPoints(ClipItem *selected)
{
  m_snapPoints.clear();
  int offset = 0;
  if (selected) offset = selected->duration();
  QList<QGraphicsItem *> itemList = items();
  for (int i = 0; i < itemList.count(); i++) {
    if (itemList.at(i)->type() == 70000 && itemList.at(i) != selected) {
      ClipItem *item = (ClipItem *)itemList.at(i);
      int start = item->startPos();
      int fadein = item->fadeIn() + start;
      int end = item->endPos();
      int fadeout = end - item->fadeOut();
      m_snapPoints.append(start);
      if (fadein != start) m_snapPoints.append(fadein);
      m_snapPoints.append(end);
      if (fadeout != end) m_snapPoints.append(fadeout);
      if (offset != 0) {
	m_snapPoints.append(start - offset);
	if (fadein != start) m_snapPoints.append(fadein - offset);
	m_snapPoints.append(end - offset);
	if (fadeout != end) m_snapPoints.append(fadeout - offset);
      }
    }
  }
  kDebug()<<" GOT SNAPPOINTS TOTAL: "<<m_snapPoints.count();
  qSort(m_snapPoints);
  for (int i = 0; i < m_snapPoints.size(); ++i)
    kDebug()<<"SNAP POINT: "<<m_snapPoints.at(i);
}


void CustomTrackView::setScale(double scaleFactor)
{
  //scale(scaleFactor, scaleFactor);
  m_scale = scaleFactor;
  kDebug()<<" HHHHHHHH  SCALING: "<<m_scale;
  QList<QGraphicsItem *> itemList = items();
  scene()->setSceneRect(0, 0, (m_projectDuration + 500) * m_scale, scene()->sceneRect().height()); //50 *

  for (int i = 0; i < itemList.count(); i++) {
      if (itemList.at(i)->type() == 70000) {
	ClipItem *clip = (ClipItem *)itemList.at(i);
	clip->setRect(clip->startPos() * m_scale, clip->rect().y(), clip->duration() * m_scale, clip->rect().height());
      }
      /*else if (itemList.at(i)->type() == 70001) {
	LabelItem *label = (LabelItem *)itemList.at(i);
	QGraphicsItem *parent = label->parentItem();
	QRectF r = label->boundingRect();
	QRectF p = parent->boundingRect();
	label->setPos(p.x() + p.width() / 2 - r.width() / 2, p.y() + p.height() / 2 - r.height() / 2);
	//label->setRect(clip->startPos() * m_scale, clip->rect().y(), clip->duration() * m_scale, clip->rect().height());
      }*/
    }
}

void CustomTrackView::drawBackground ( QPainter * painter, const QRectF & rect )  
{
  QRect rectInView;//this is the rect that is visible by the user
  if (scene()->views().size()>0){ 
    rectInView=scene()->views()[0]->viewport()->rect();
    rectInView.moveTo(scene()->views()[0]->horizontalScrollBar()->value(),scene()->views()[0]->verticalScrollBar()->value());
  }
  if (rectInView.isNull()) return;

  QColor base = palette().button().color();
  painter->setClipRect(rect);
  painter->drawLine(rectInView.left(), 0, rectInView.right(), 0);
  for (uint i = 0; i < m_tracksCount;i++)
  {
    painter->drawLine(rectInView.left(), 50 * (i+1), rectInView.right(), 50 * (i+1));
    painter->drawText(QRectF(10, 50 * i, 100, 50 * i + 49), Qt::AlignLeft, i18n(" Track ") + QString::number(i + 1));
  }
  int lowerLimit = 50 * m_tracksCount + 1;
  if (height() > lowerLimit)
  painter->fillRect(QRectF(rectInView.left(), lowerLimit, rectInView.width(), height() - lowerLimit), QBrush(base));
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
