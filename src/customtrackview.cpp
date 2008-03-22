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
#include <KCursor>

#include "customtrackview.h"
#include "docclipbase.h"
#include "clipitem.h"
#include "definitions.h"
#include "moveclipcommand.h"
#include "resizeclipcommand.h"
#include "addtimelineclipcommand.h"
#include "addeffectcommand.h"
#include "editeffectcommand.h"
#include "addtransitioncommand.h"
#include "edittransitioncommand.h"
#include "kdenlivesettings.h"
#include "transition.h"
#include "clipitem.h"
#include "customtrackview.h"
#include "clipmanager.h"
#include "renderer.h"
//TODO:
// disable animation if user asked it in KDE's global settings
// http://lists.kde.org/?l=kde-commits&m=120398724717624&w=2
// needs something like below (taken from dolphin)
// #include <kglobalsettings.h>
// const bool animate = KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects;
// const int duration = animate ? 1500 : 1;

CustomTrackView::CustomTrackView(KdenliveDoc *doc, QGraphicsScene * projectscene, QWidget *parent)
        : QGraphicsView(projectscene, parent), m_cursorPos(0), m_dropItem(NULL), m_cursorLine(NULL), m_operationMode(NONE), m_startPos(QPointF()), m_dragItem(NULL), m_visualTip(NULL), m_moveOpMode(NONE), m_animation(NULL), m_projectDuration(0), m_scale(1.0), m_clickPoint(QPoint()), m_document(doc), m_autoScroll(KdenliveSettings::autoscroll()), m_tracksHeight(KdenliveSettings::trackheight()) {
    if (doc) m_commandStack = doc->commandStack();
    else m_commandStack == NULL;
    setMouseTracking(true);
    setAcceptDrops(true);
    m_animationTimer = new QTimeLine(800);
    m_animationTimer->setFrameRange(0, 5);
    m_animationTimer->setUpdateInterval(100);
    m_animationTimer->setLoopCount(0);
    m_tipColor = QColor(0, 192, 0, 200);
    QColor border = QColor(255, 255, 255, 100);
    m_tipPen.setColor(border);
    m_tipPen.setWidth(3);
    setContentsMargins(0, 0, 0, 0);
    if (projectscene) {
        m_cursorLine = projectscene->addLine(0, 0, 0, m_tracksHeight);
        m_cursorLine->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIgnoresTransformations);
        m_cursorLine->setZValue(1000);
    }
}

void CustomTrackView::setContextMenu(QMenu *timeline, QMenu *clip, QMenu *transition) {
    m_timelineContextMenu = timeline;
    m_timelineContextClipMenu = clip;
    m_timelineContextTransitionMenu = transition;
}

void CustomTrackView::checkAutoScroll() {
    m_autoScroll = KdenliveSettings::autoscroll();
}

QList <TrackInfo> CustomTrackView::tracksList() const {
    return m_tracksList;
}

void CustomTrackView::checkTrackHeight() {
    if (m_tracksHeight == KdenliveSettings::trackheight()) return;
    m_tracksHeight = KdenliveSettings::trackheight();
    emit trackHeightChanged();
    QList<QGraphicsItem *> itemList = items();
    ClipItem *item;
    Transition *transitionitem;
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            item = (ClipItem*) itemList.at(i);
            item->setRect(item->rect().x(), item->track() * m_tracksHeight, item->rect().width(), m_tracksHeight - 1);
            item->resetThumbs();
        } else if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            transitionitem = (Transition*) itemList.at(i);
            transitionitem->setRect(transitionitem->rect().x(), transitionitem->track() * m_tracksHeight + m_tracksHeight / 2, transitionitem->rect().width(), m_tracksHeight - 1);
        }
    }
    m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), m_tracksHeight * m_tracksList.count());
    setSceneRect(0, 0, sceneRect().width(), m_tracksHeight * m_tracksList.count());
    verticalScrollBar()->setMaximum(m_tracksHeight * m_tracksList.count());
    update();
}

// virtual
void CustomTrackView::resizeEvent(QResizeEvent * event) {
    QGraphicsView::resizeEvent(event);
}

// virtual
void CustomTrackView::wheelEvent(QWheelEvent * e) {
    if (e->modifiers() == Qt::ControlModifier) {
        if (e->delta() > 0) emit zoomIn();
        else emit zoomOut();
    } else {
        if (e->delta() > 0) horizontalScrollBar()->setValue(horizontalScrollBar()->value() + horizontalScrollBar()->singleStep());
        else  horizontalScrollBar()->setValue(horizontalScrollBar()->value() - horizontalScrollBar()->singleStep());
    }
}

int CustomTrackView::getPreviousVideoTrack(int track) {
    track = m_tracksList.count() - track - 1;
    int videoTracksCount = 0;
    track --;
    for (int i = track; i > -1; i--) {
        if (m_tracksList.at(i).type == VIDEOTRACK) return i + 1;
    }
    return 0;
}

// virtual
void CustomTrackView::mouseMoveEvent(QMouseEvent * event) {
    int pos = event->x();
    emit mousePosition((int)(mapToScene(event->pos()).x() / m_scale));
    /*if (event->modifiers() == Qt::ControlModifier)
      setDragMode(QGraphicsView::ScrollHandDrag);
    else if (event->modifiers() == Qt::ShiftModifier)
      setDragMode(QGraphicsView::RubberBandDrag);
    else*/
    {

        if (m_dragItem) { //event->button() == Qt::LeftButton) {
            // a button was pressed, delete visual tips
            if (m_operationMode == MOVE) {
                double snappedPos = getSnapPointForPos(mapToScene(event->pos()).x() - m_clickPoint.x());
                //kDebug() << "///////  MOVE CLIP, EVENT Y: "<<m_clickPoint.y();//<<event->scenePos().y()<<", SCENE HEIGHT: "<<scene()->sceneRect().height();
                int moveTrack = (int)  mapToScene(event->pos() + QPoint(0, (m_dragItem->type() == TRANSITIONWIDGET ? m_tracksHeight - m_clickPoint.y() : 0))).y() / m_tracksHeight;
                int currentTrack = m_dragItem->track();

                if (moveTrack > m_tracksList.count() - 1) moveTrack = m_tracksList.count() - 1;
                else if (moveTrack < 0) moveTrack = 0;

                int offset = moveTrack - currentTrack;
                if (offset != 0) offset = m_tracksHeight * offset;
                m_dragItem->moveTo((int)(snappedPos / m_scale), m_scale, offset, moveTrack);
            } else if (m_operationMode == RESIZESTART) {
                double snappedPos = getSnapPointForPos(mapToScene(event->pos()).x());
                m_dragItem->resizeStart(snappedPos / m_scale, m_scale);
            } else if (m_operationMode == RESIZEEND) {
                double snappedPos = getSnapPointForPos(mapToScene(event->pos()).x());
                m_dragItem->resizeEnd(snappedPos / m_scale, m_scale);
            } else if (m_operationMode == FADEIN) {
                int pos = mapToScene(event->pos()).x() / m_scale;
                m_dragItem->setFadeIn(pos - m_dragItem->startPos().frames(m_document->fps()), m_scale);
            } else if (m_operationMode == FADEOUT) {
                int pos = mapToScene(event->pos()).x() / m_scale;
                m_dragItem->setFadeOut(m_dragItem->endPos().frames(m_document->fps()) - pos, m_scale);
            }

            if (m_animation) delete m_animation;
            m_animation = NULL;
            if (m_visualTip) delete m_visualTip;
            m_visualTip = NULL;
            QGraphicsView::mouseMoveEvent(event);
            return;
        }

        QList<QGraphicsItem *> itemList = items(event->pos());
        QGraphicsRectItem *item = NULL;
        for (int i = 0; i < itemList.count(); i++) {
            if (itemList.at(i)->type() == AVWIDGET || itemList.at(i)->type() == TRANSITIONWIDGET) {
                item = (QGraphicsRectItem*) itemList.at(i);
                break;
            }
        }
        if (item && event->buttons() == Qt::NoButton) {
            AbstractClipItem *clip = (AbstractClipItem*) item;
            OPERATIONTYPE opMode = opMode = clip->operationMode(mapToScene(event->pos()), m_scale);
            double size = 8;

            if (opMode == m_moveOpMode) {
                QGraphicsView::mouseMoveEvent(event);
                return;
            } else {
                if (m_visualTip) {
                    if (m_animation) delete m_animation;
                    m_animation = NULL;
                    m_animationTimer->stop();
                    delete m_visualTip;
                    m_visualTip = NULL;
                }
            }
            m_moveOpMode = opMode;
            if (opMode == MOVE) {
                setCursor(Qt::OpenHandCursor);
            } else if (opMode == RESIZESTART) {
                setCursor(KCursor("left_side", Qt::SizeHorCursor));
                kDebug() << "********  RESIZE CLIP START; WIDTH: " << size;
                if (m_visualTip == NULL) {
                    QPolygon polygon;
                    polygon << QPoint(clip->rect().x(), clip->rect().y() + clip->rect().height() / 2 - size * 2);
                    polygon << QPoint(clip->rect().x() + size * 2, clip->rect().y() + clip->rect().height() / 2);
                    polygon << QPoint(clip->rect().x(), clip->rect().y() + clip->rect().height() / 2 + size * 2);
                    polygon << QPoint(clip->rect().x(), clip->rect().y() + clip->rect().height() / 2 - size * 2);

                    m_visualTip = new QGraphicsPolygonItem(polygon);
                    ((QGraphicsPolygonItem*) m_visualTip)->setBrush(m_tipColor);
                    ((QGraphicsPolygonItem*) m_visualTip)->setPen(m_tipPen);
                    m_visualTip->setZValue(100);
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
            } else if (opMode == RESIZEEND) {
                setCursor(KCursor("right_side", Qt::SizeHorCursor));
                if (m_visualTip == NULL) {
                    QPolygon polygon;
                    polygon << QPoint(clip->rect().x() + clip->rect().width(), clip->rect().y() + clip->rect().height() / 2 - size * 2);
                    polygon << QPoint(clip->rect().x() + clip->rect().width() - size * 2, clip->rect().y() + clip->rect().height() / 2);
                    polygon << QPoint(clip->rect().x() + clip->rect().width(), clip->rect().y() + clip->rect().height() / 2 + size * 2);
                    polygon << QPoint(clip->rect().x() + clip->rect().width(), clip->rect().y() + clip->rect().height() / 2 - size * 2);

                    m_visualTip = new QGraphicsPolygonItem(polygon);
                    ((QGraphicsPolygonItem*) m_visualTip)->setBrush(m_tipColor);
                    ((QGraphicsPolygonItem*) m_visualTip)->setPen(m_tipPen);

                    m_visualTip->setZValue(100);
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
            } else if (opMode == FADEIN) {
                if (m_visualTip == NULL) {
                    m_visualTip = new QGraphicsEllipseItem(clip->rect().x() + clip->fadeIn() * m_scale - size, clip->rect().y() - 8, size * 2, 16);
                    ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
                    ((QGraphicsEllipseItem*) m_visualTip)->setPen(m_tipPen);
                    m_visualTip->setZValue(100);
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
            } else if (opMode == FADEOUT) {
                if (m_visualTip == NULL) {
                    m_visualTip = new QGraphicsEllipseItem(clip->rect().x() + clip->rect().width() - clip->fadeOut() * m_scale - size, clip->rect().y() - 8, size*2, 16);
                    ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
                    ((QGraphicsEllipseItem*) m_visualTip)->setPen(m_tipPen);
                    m_visualTip->setZValue(100);
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
            } else if (opMode == TRANSITIONSTART) {
                if (m_visualTip == NULL) {
                    m_visualTip = new QGraphicsEllipseItem(-5, -5 , 10, 10);
                    ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
                    ((QGraphicsEllipseItem*) m_visualTip)->setPen(m_tipPen);
                    m_visualTip->setZValue(100);
                    m_animation = new QGraphicsItemAnimation;
                    m_animation->setItem(m_visualTip);
                    m_animation->setTimeLine(m_animationTimer);
                    m_visualTip->setPos(clip->rect().x() + 15, clip->rect().y() + clip->rect().height() / 2);
                    double scale = 2.0;
                    m_animation->setScaleAt(.5, scale, scale);
                    scale = 1.0;
                    m_animation->setScaleAt(1, scale, scale);
                    scene()->addItem(m_visualTip);
                    m_animationTimer->start();
                }
                setCursor(Qt::PointingHandCursor);
            } else if (opMode == TRANSITIONEND) {
                if (m_visualTip == NULL) {
                    m_visualTip = new QGraphicsEllipseItem(-5, -5 , 10, 10);
                    ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
                    ((QGraphicsEllipseItem*) m_visualTip)->setPen(m_tipPen);
                    m_visualTip->setZValue(100);
                    m_animation = new QGraphicsItemAnimation;
                    m_animation->setItem(m_visualTip);
                    m_animation->setTimeLine(m_animationTimer);
                    m_visualTip->setPos(clip->rect().x() + clip->rect().width() - 15 , clip->rect().y() + clip->rect().height() / 2);
                    double scale = 2.0;
                    m_animation->setScaleAt(.5, scale, scale);
                    scale = 1.0;
                    m_animation->setScaleAt(1, scale, scale);
                    scene()->addItem(m_visualTip);
                    m_animationTimer->start();
                }
                setCursor(Qt::PointingHandCursor);
            }
        } else {
            m_moveOpMode = NONE;
            if (event->buttons() != Qt::NoButton && event->modifiers() == Qt::NoModifier) {
                setCursorPos((int) mapToScene(event->pos().x(), 0).x() / m_scale);
            }
            if (m_visualTip) {
                if (m_animation) delete m_animation;
                m_animationTimer->stop();
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
void CustomTrackView::mousePressEvent(QMouseEvent * event) {
    activateMonitor();
    int pos = event->x();
    if (event->modifiers() == Qt::ControlModifier) {
        setDragMode(QGraphicsView::ScrollHandDrag);
        QGraphicsView::mousePressEvent(event);
        return;
    } else if (event->modifiers() == Qt::ShiftModifier) {
        setDragMode(QGraphicsView::RubberBandDrag);
        QGraphicsView::mousePressEvent(event);
        return;
    } else {
        bool collision = false;
        QList<QGraphicsItem *> collisionList = items(event->pos());
        AbstractClipItem *clipItem = NULL, *transitionItem = NULL;
        for (int i = 0; i < collisionList.size(); ++i) {
            QGraphicsItem *item = collisionList.at(i);
            if (item->type() == AVWIDGET || item->type() == TRANSITIONWIDGET) {
                // select item
                if (!item->isSelected()) {

                    item->setSelected(true);
                    update();
                }

                m_dragItem = (AbstractClipItem *) item;
                if (item->type() == AVWIDGET) {
                    clipItem = m_dragItem;
                } else if (item->type() == TRANSITIONWIDGET) {
                    transitionItem = m_dragItem;
                }
                m_clickPoint = QPoint(mapToScene(event->pos()).x() - m_dragItem->startPos().frames(m_document->fps()) * m_scale, event->pos().y() - m_dragItem->rect().top());
                m_operationMode = m_dragItem->operationMode(item->mapFromScene(mapToScene(event->pos())), m_scale);
                if (m_operationMode == MOVE) setCursor(Qt::ClosedHandCursor);
                if (m_operationMode == MOVE || m_operationMode == RESIZESTART)
                    m_startPos = QPointF(m_dragItem->startPos().frames(m_document->fps()), m_dragItem->track());
                else if (m_operationMode == RESIZEEND)
                    m_startPos = QPointF(m_dragItem->endPos().frames(m_document->fps()), m_dragItem->track());
                else if (m_operationMode == TRANSITIONSTART) {
                    Transition *tr = new Transition(
                        QRect(m_dragItem->startPos().frames(m_document->fps()) *m_scale , m_dragItem->rect().y() + m_dragItem->rect().height() / 2,
                              GenTime(2.5).frames(m_document->fps()) *m_scale ,  m_dragItem->rect().height()
                             ),
                        (ClipItem*)m_dragItem, "luma" , m_dragItem->startPos(), m_dragItem->startPos() + GenTime(2.5), m_document->fps());
                    tr->setTrack(m_dragItem->track());
                    scene()->addItem(tr);
                    //m_dragItem->addTransition(tra);
                }
                updateSnapPoints(m_dragItem);
                kDebug() << "//////// ITEM CLICKED: " << m_startPos;
                collision = true;
                break;
            }
        }
        emit clipItemSelected((ClipItem*) clipItem);
        emit transitionItemSelected((Transition*) transitionItem);
        if (!collision) {
            kDebug() << "//////// NO ITEM FOUND ON CLICK";
            m_dragItem = NULL;
            setCursor(Qt::ArrowCursor);
            QList<QGraphicsItem *> itemList = items();
            for (int i = 0; i < itemList.count(); i++)
                itemList.at(i)->setSelected(false);
            //emit clipItemSelected(NULL);
            if (event->button() == Qt::RightButton) {
                displayContextMenu(event->globalPos());
            } else setCursorPos((int) mapToScene(event->x(), 0).x() / m_scale);
        } else if (event->button() == Qt::RightButton) {
            m_operationMode = NONE;
            displayContextMenu(event->globalPos(), (ClipItem *) m_dragItem);
            m_dragItem = NULL;
        }
    }
    //kDebug()<<pos;
    //QGraphicsView::mousePressEvent(event);
}

void CustomTrackView::displayContextMenu(QPoint pos, ClipItem *clip) {
    m_timelineContextClipMenu->popup(pos);
}

void CustomTrackView::activateMonitor() {
    emit activateDocumentMonitor();
}

void CustomTrackView::dragEnterEvent(QDragEnterEvent * event) {
    if (event->mimeData()->hasFormat("kdenlive/producerslist")) {
        kDebug() << "///////////////  DRAG ENTERED, TEXT: " << event->mimeData()->data("kdenlive/producerslist");
        QStringList ids = QString(event->mimeData()->data("kdenlive/producerslist")).split(";");
        //TODO: drop of several clips
        for (int i = 0; i < ids.size(); ++i) {
        }
        DocClipBase *clip = m_document->getBaseClip(ids.at(0).toInt());
        if (clip == NULL) kDebug() << " WARNING))))))))) CLIP NOT FOUND : " << ids.at(0).toInt();
        addItem(clip, event->pos());
        event->acceptProposedAction();
    } else QGraphicsView::dragEnterEvent(event);
}

void CustomTrackView::slotRefreshEffects(ClipItem *clip) {
    int track = m_tracksList.count() - clip->track();
    GenTime pos = clip->startPos();
    m_document->renderer()->mltRemoveEffect(track, pos, "-1", false);
    for (int i = 0; i < clip->effectsCount(); i++) {
        m_document->renderer()->mltAddEffect(track, pos, clip->getEffectArgs(clip->effectAt(i)), false);
    }
    m_document->renderer()->doRefresh();
}

void CustomTrackView::addEffect(int track, GenTime pos, QDomElement effect) {
    ClipItem *clip = getClipItemAt(pos.frames(m_document->fps()) + 1, m_tracksList.count() - track);
    if (clip) {
        QMap <QString, QString> effectParams = clip->addEffect(effect);
        m_document->renderer()->mltAddEffect(track, pos, effectParams);
        emit clipItemSelected(clip);
    }
}

void CustomTrackView::deleteEffect(int track, GenTime pos, QDomElement effect) {
    QString index = effect.attribute("kdenlive_ix");
    m_document->renderer()->mltRemoveEffect(track, pos, index);
    ClipItem *clip = getClipItemAt(pos.frames(m_document->fps()) + 1, m_tracksList.count() - track);
    if (clip) {
        clip->deleteEffect(index);
        emit clipItemSelected(clip);
    }
}

void CustomTrackView::slotAddEffect(QDomElement effect, GenTime pos, int track) {
    QList<QGraphicsItem *> itemList;
    if (track == -1)
        itemList = items();
    else {
        ClipItem *clip = getClipItemAt(pos.frames(m_document->fps()) + 1, track);
        if (clip) itemList.append(clip);
        else kDebug() << "------   wrning, clip eff not found";
    }
    kDebug() << "// REQUESTING EFFECT ON CLIP: " << pos.frames(25) << ", TRK: " << track;
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET && (itemList.at(i)->isSelected() || track != -1)) {
            ClipItem *item = (ClipItem *)itemList.at(i);
            // the kdenlive_ix int is used to identify an effect in mlt's playlist, should
            // not be changed
            if (effect.attribute("kdenlive_ix").toInt() == 0)
                effect.setAttribute("kdenlive_ix", QString::number(item->effectsCounter()));
            AddEffectCommand *command = new AddEffectCommand(this, m_tracksList.count() - item->track(), item->startPos(), effect, true);
            m_commandStack->push(command);
        }
    }
    m_document->setModified(true);
}

void CustomTrackView::slotDeleteEffect(ClipItem *clip, QDomElement effect) {
    AddEffectCommand *command = new AddEffectCommand(this, m_tracksList.count() - clip->track(), clip->startPos(), effect, false);
    m_commandStack->push(command);
    m_document->setModified(true);
}

void CustomTrackView::updateEffect(int track, GenTime pos, QDomElement effect) {
    ClipItem *clip = getClipItemAt(pos.frames(m_document->fps()) + 1, m_tracksList.count() - track);
    if (clip) {
        QMap <QString, QString> effectParams = clip->getEffectArgs(effect);
        if (effectParams["disabled"] == "1") {
            QString index = effectParams["kdenlive_ix"];
            m_document->renderer()->mltRemoveEffect(track, pos, index);
        } else m_document->renderer()->mltEditEffect(m_tracksList.count() - clip->track(), clip->startPos(), effectParams);
    }
    m_document->setModified(true);
}

void CustomTrackView::slotChangeEffectState(ClipItem *clip, QDomElement effect, bool disable) {
    QDomElement oldEffect = effect.cloneNode().toElement();
    effect.setAttribute("disabled", disable);
    EditEffectCommand *command = new EditEffectCommand(this, m_tracksList.count() - clip->track(), clip->startPos(), oldEffect, effect, true);
    m_commandStack->push(command);
    m_document->setModified(true);
}

void CustomTrackView::slotUpdateClipEffect(ClipItem *clip, QDomElement oldeffect, QDomElement effect) {
    EditEffectCommand *command = new EditEffectCommand(this, m_tracksList.count() - clip->track(), clip->startPos(), oldeffect, effect, true);
    m_commandStack->push(command);
}

void CustomTrackView::slotAddTransition(ClipItem* clip , QDomElement transition, GenTime startTime , int startTrack) {
    AddTransitionCommand* command = new AddTransitionCommand(this, startTrack, transition, startTime, true);
    m_commandStack->push(command);
    m_document->setModified(true);
}

void CustomTrackView::addTransition(int startTrack, GenTime startPos , QDomElement e) {
    QMap < QString, QString> map;

    QDomNamedNodeMap attribs = e.attributes();
    for (int i = 0;i < attribs.count();i++) {
        if (attribs.item(i).nodeName() != "type" &&
                attribs.item(i).nodeName() != "start" &&
                attribs.item(i).nodeName() != "end"
           )
            map[attribs.item(i).nodeName()] = attribs.item(i).nodeValue();
    }
    //map["resource"] = "%luma12.pgm";
    kDebug() << "---- ADDING transition " << e.attribute("type") << ", on tracks " << m_tracksList.count() - e.attribute("transition_track").toInt() << " / " << getPreviousVideoTrack(e.attribute("transition_track").toInt());
    m_document->renderer()->mltAddTransition(e.attribute("type"), getPreviousVideoTrack(e.attribute("transition_track").toInt()), m_tracksList.count() - e.attribute("transition_track").toInt() ,
            GenTime(e.attribute("start").toInt(), m_document->renderer()->fps()),
            GenTime(e.attribute("end").toInt(), m_document->renderer()->fps()),
            map);

    m_document->setModified(true);
}

void CustomTrackView::deleteTransition(int, GenTime, QDomElement e) {
    QMap < QString, QString> map;
    QDomNamedNodeMap attribs = e.attributes();
    m_document->renderer()->mltDeleteTransition(e.attribute("type"), getPreviousVideoTrack(e.attribute("transition_track").toInt()), m_tracksList.count() - e.attribute("transition_track").toInt() ,
            GenTime(e.attribute("start").toInt(), m_document->renderer()->fps()),
            GenTime(e.attribute("end").toInt(), m_document->renderer()->fps()),
            map);
    m_document->setModified(true);
}

void CustomTrackView::slotTransitionUpdated(QDomElement old, QDomElement newEffect) {
    EditTransitionCommand *command = new EditTransitionCommand(this, newEffect.attribute("a_track").toInt(), GenTime(newEffect.attribute("start").toInt(), m_document->renderer()->fps()) , old, newEffect , true);
    m_commandStack->push(command);
    m_document->setModified(true);
}

void CustomTrackView::updateTransition(int track, GenTime pos, QDomElement oldTransition, QDomElement transition) {
    QMap < QString, QString> map;
    QDomNamedNodeMap attribs = transition.attributes();
    for (int i = 0;i < attribs.count();i++) {
        if (attribs.item(i).nodeName() != "type" &&
                attribs.item(i).nodeName() != "start" &&
                attribs.item(i).nodeName() != "end"
           )
            map[attribs.item(i).nodeName()] = attribs.item(i).nodeValue();
    }
    m_document->renderer()->mltUpdateTransition(oldTransition.attribute("type"), transition.attribute("type"), m_tracksList.count() - 1  - transition.attribute("transition_track").toInt(), m_tracksList.count() - transition.attribute("transition_track").toInt() ,
            GenTime(transition.attribute("start").toInt(), m_document->renderer()->fps()),
            GenTime(transition.attribute("end").toInt(), m_document->renderer()->fps()),
            map);
    m_document->setModified(true);
}

void CustomTrackView::addItem(DocClipBase *clip, QPoint pos) {
    int in = 0;
    GenTime out = clip->duration();
    //kdDebug()<<"- - - -CREATING CLIP, duration = "<<out<<", URL: "<<clip->fileURL();
    int trackTop = ((int) mapToScene(pos).y() / m_tracksHeight) * m_tracksHeight + 1;
    m_dropItem = new ClipItem(clip, ((int) mapToScene(pos).y() / m_tracksHeight), GenTime(), QRectF(mapToScene(pos).x() * m_scale, trackTop, out.frames(m_document->fps()) * m_scale, m_tracksHeight - 1), out, m_document->fps());
    scene()->addItem(m_dropItem);
}


void CustomTrackView::dragMoveEvent(QDragMoveEvent * event) {
    event->setDropAction(Qt::IgnoreAction);
    //kDebug()<<"+++++++++++++   DRAG MOVE, : "<<mapToScene(event->pos()).x()<<", SCAL: "<<m_scale;
    if (m_dropItem) {
        int track = (int) mapToScene(event->pos()).y() / m_tracksHeight; //) * (m_scale * 50) + m_scale;
        m_dropItem->moveTo(mapToScene(event->pos()).x() / m_scale, m_scale, (track - m_dropItem->track()) * m_tracksHeight, track);
        event->setDropAction(Qt::MoveAction);
        if (event->mimeData()->hasFormat("kdenlive/producerslist")) {
            event->acceptProposedAction();
        }
    } else {
        QGraphicsView::dragMoveEvent(event);
    }
}

void CustomTrackView::dragLeaveEvent(QDragLeaveEvent * event) {
    if (m_dropItem) {
        delete m_dropItem;
        m_dropItem = NULL;
    } else QGraphicsView::dragLeaveEvent(event);
}

void CustomTrackView::dropEvent(QDropEvent * event) {
    if (m_dropItem) {
        AddTimelineClipCommand *command = new AddTimelineClipCommand(this, m_dropItem->xml(), m_dropItem->clipProducer(), m_dropItem->track(), m_dropItem->startPos(), m_dropItem->rect(), m_dropItem->duration(), false, false);
        m_commandStack->push(command);
        m_dropItem->baseClip()->addReference();
        m_document->updateClip(m_dropItem->baseClip()->getId());
        // kDebug()<<"IIIIIIIIIIIIIIIIIIIIIIII TRAX CNT: "<<m_tracksList.count()<<", DROP: "<<m_dropItem->track();
        m_document->renderer()->mltInsertClip(m_tracksList.count() - m_dropItem->track(), m_dropItem->startPos(), m_dropItem->xml());
        m_document->setModified(true);
    } else QGraphicsView::dropEvent(event);
    m_dropItem = NULL;
}


QStringList CustomTrackView::mimeTypes() const {
    QStringList qstrList;
    // list of accepted mime types for drop
    qstrList.append("text/plain");
    qstrList.append("kdenlive/producerslist");
    return qstrList;
}

Qt::DropActions CustomTrackView::supportedDropActions() const {
    // returns what actions are supported when dropping
    return Qt::MoveAction;
}

void CustomTrackView::setDuration(int duration) {
    kDebug() << "/////////////  PRO DUR: " << duration << ", SCALE. " << m_scale << ", height: " << 50 * m_tracksList.count();
    m_projectDuration = duration;
    scene()->setSceneRect(0, 0, (m_projectDuration + 500) * m_scale, scene()->sceneRect().height()); //50 * m_tracksCount);
    horizontalScrollBar()->setMaximum((m_projectDuration + 500) * m_scale);
}

int CustomTrackView::duration() const {
    return m_projectDuration;
}

void CustomTrackView::addTrack(TrackInfo type) {
    m_tracksList << type;
    m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), m_tracksHeight * m_tracksList.count());
    setSceneRect(0, 0, sceneRect().width(), m_tracksHeight * m_tracksList.count());
    verticalScrollBar()->setMaximum(m_tracksHeight * m_tracksList.count());
    //setFixedHeight(50 * m_tracksCount);
}

void CustomTrackView::removeTrack() {
    // TODO: implement track deletion
    //m_tracksCount--;
    m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), m_tracksHeight * m_tracksList.count());
}


void CustomTrackView::slotSwitchTrackAudio(int ix) {
    int tracknumber = m_tracksList.count() - ix;
    kDebug() << "/////  MUTING TRK: " << ix << "; PL NUM: " << tracknumber;
    m_tracksList[tracknumber - 1].isMute = !m_tracksList.at(tracknumber - 1).isMute;
    m_document->renderer()->mltChangeTrackState(tracknumber, m_tracksList.at(tracknumber - 1).isMute, m_tracksList.at(tracknumber - 1).isBlind);
}

void CustomTrackView::slotSwitchTrackVideo(int ix) {
    int tracknumber = m_tracksList.count() - ix;
    m_tracksList[tracknumber - 1].isBlind = !m_tracksList.at(tracknumber - 1).isBlind;
    m_document->renderer()->mltChangeTrackState(tracknumber, m_tracksList.at(tracknumber - 1).isMute, m_tracksList.at(tracknumber - 1).isBlind);
}

void CustomTrackView::deleteClip(int clipId) {
    QList<QGraphicsItem *> itemList = items();
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = (ClipItem *)itemList.at(i);
            if (item->clipProducer() == clipId) {
                AddTimelineClipCommand *command = new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), item->track(), item->startPos(), item->rect(), item->duration(), true, true);
                m_commandStack->push(command);
                //delete item;
            }
        }
    }
}

void CustomTrackView::setCursorPos(int pos, bool seek) {
    emit cursorMoved(m_cursorPos * m_scale, pos * m_scale);
    m_cursorPos = pos;
    m_cursorLine->setPos(pos * m_scale, 0);
    if (seek) m_document->renderer()->seek(GenTime(pos, m_document->fps()));
    else if (m_autoScroll && m_scale < 50) checkScrolling();
}

void CustomTrackView::updateCursorPos() {
    m_cursorLine->setPos(m_cursorPos * m_scale, 0);
}

int CustomTrackView::cursorPos() {
    return m_cursorPos * m_scale;
}

void CustomTrackView::checkScrolling() {
    QRect rectInView = viewport()->rect();
    int delta = rectInView.width() / 3;
    int max = rectInView.right() + horizontalScrollBar()->value() - delta;
    //kDebug() << "CURSOR POS: "<<m_cursorPos<< "Scale: "<<m_scale;
    if (m_cursorPos * m_scale >= max) horizontalScrollBar()->setValue(horizontalScrollBar()->value() + 1 + m_scale);
}

void CustomTrackView::mouseReleaseEvent(QMouseEvent * event) {
    QGraphicsView::mouseReleaseEvent(event);
    setDragMode(QGraphicsView::NoDrag);
    if (m_dragItem == NULL) return;
    if (m_operationMode == MOVE) setCursor(Qt::OpenHandCursor);
    if (m_operationMode == MOVE && m_startPos.x() != m_dragItem->startPos().frames(m_document->fps())) {
        // move clip
        MoveClipCommand *command = new MoveClipCommand(this, m_startPos, QPointF(m_dragItem->startPos().frames(m_document->fps()), m_dragItem->track()), false);
        m_commandStack->push(command);
        if (m_dragItem->type() == AVWIDGET) m_document->renderer()->mltMoveClip(m_tracksList.count() - m_startPos.y(), m_tracksList.count() - m_dragItem->track(), m_startPos.x(), m_dragItem->startPos().frames(m_document->fps()));
    } else if (m_operationMode == RESIZESTART) {
        // resize start
        ResizeClipCommand *command = new ResizeClipCommand(this, m_startPos, QPointF(m_dragItem->startPos().frames(m_document->fps()), m_dragItem->track()), true, false);

        if (m_dragItem->type() == AVWIDGET) m_document->renderer()->mltResizeClipStart(m_tracksList.count() - m_dragItem->track(), m_dragItem->endPos(), m_dragItem->startPos(), GenTime(m_startPos.x(), m_document->fps()), m_dragItem->cropStart(), m_dragItem->cropStart() + m_dragItem->endPos() - m_dragItem->startPos());
        m_commandStack->push(command);
        m_document->renderer()->doRefresh();
    } else if (m_operationMode == RESIZEEND) {
        // resize end
        ResizeClipCommand *command = new ResizeClipCommand(this, m_startPos, QPointF(m_dragItem->endPos().frames(m_document->fps()), m_dragItem->track()), false, false);

        if (m_dragItem->type() == AVWIDGET) m_document->renderer()->mltResizeClipEnd(m_tracksList.count() - m_dragItem->track(), m_dragItem->startPos(), m_dragItem->cropStart(), m_dragItem->cropStart() + m_dragItem->endPos() - m_dragItem->startPos());
        m_commandStack->push(command);
        m_document->renderer()->doRefresh();
    }
    m_document->setModified(true);
    m_operationMode = NONE;
    m_dragItem = NULL;
}

void CustomTrackView::deleteClip(int track, GenTime startpos, const QRectF &rect) {
    ClipItem *item = getClipItemAt(startpos, track);
    if (!item) {
        kDebug() << "----------------  ERROR, CANNOT find clip to move at: " << rect.x();
        return;
    }
    item->baseClip()->removeReference();
    m_document->updateClip(item->baseClip()->getId());
    delete item;
    m_document->renderer()->mltRemoveClip(m_tracksList.count() - track, startpos);
    m_document->renderer()->doRefresh();
}

void CustomTrackView::deleteSelectedClips() {
    QList<QGraphicsItem *> itemList = items();
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET && itemList.at(i)->isSelected()) {
            ClipItem *item = (ClipItem *) itemList.at(i);
            AddTimelineClipCommand *command = new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), item->track(), item->startPos(), item->rect(), item->duration(), true, true);
            m_commandStack->push(command);
        }
    }
}

void CustomTrackView::addClip(QDomElement xml, int clipId, int track, GenTime startpos, const QRectF &rect, GenTime duration) {
    QRect r(startpos.frames(m_document->fps()) * m_scale, m_tracksHeight * track, duration.frames(m_document->fps()) * m_scale, m_tracksHeight - 1);
    DocClipBase *baseclip = m_document->clipManager()->getClipById(clipId);
    ClipItem *item = new ClipItem(baseclip, track, startpos, r, duration, m_document->fps());
    scene()->addItem(item);
    baseclip->addReference();
    m_document->updateClip(baseclip->getId());
    m_document->renderer()->mltInsertClip(m_tracksList.count() - track, startpos, xml);
    m_document->renderer()->doRefresh();
}

ClipItem *CustomTrackView::getClipItemAt(int pos, int track) {
    return (ClipItem *) scene()->itemAt(pos * m_scale, track * m_tracksHeight + m_tracksHeight / 2);
}

ClipItem *CustomTrackView::getClipItemAt(GenTime pos, int track) {
    return (ClipItem *) scene()->itemAt(pos.frames(m_document->fps()) * m_scale, track * m_tracksHeight + m_tracksHeight / 2);
}

void CustomTrackView::moveClip(const QPointF &startPos, const QPointF &endPos) {
    ClipItem *item = getClipItemAt(startPos.x() + 1, startPos.y());
    if (!item) {
        kDebug() << "----------------  ERROR, CANNOT find clip to move at: " << startPos.x() * m_scale * FRAME_SIZE + 1 << ", " << startPos.y() * m_tracksHeight + m_tracksHeight / 2;
        return;
    }
    kDebug() << "----------------  Move CLIP FROM: " << startPos.x() << ", END:" << endPos.x();
    item->moveTo(endPos.x(), m_scale, (endPos.y() - startPos.y()) * m_tracksHeight, endPos.y());
    m_document->renderer()->mltMoveClip(m_tracksList.count() - startPos.y(), m_tracksList.count() - endPos.y(), startPos.x(), endPos.x());
}

void CustomTrackView::resizeClip(const QPointF &startPos, const QPointF &endPos, bool resizeClipStart) {
    int offset;
    if (resizeClipStart) offset = 1;
    else offset = -1;
    ClipItem *item = getClipItemAt(startPos.x() + offset, startPos.y());
    if (!item) {
        kDebug() << "----------------  ERROR, CANNOT find clip to resize at: " << startPos;
        return;
    }
    qreal diff = endPos.x() - startPos.x();
    if (resizeClipStart) {
        m_document->renderer()->mltResizeClipStart(m_tracksList.count() - item->track(), item->endPos(), GenTime(endPos.x(), m_document->fps()), item->startPos(), item->cropStart() + GenTime(diff, m_document->fps()), item->cropStart() + GenTime(diff, m_document->fps()) + item->endPos() - GenTime(endPos.x(), m_document->fps()));
        item->resizeStart(endPos.x(), m_scale);
    } else {
        m_document->renderer()->mltResizeClipEnd(m_tracksList.count() - item->track(), item->startPos(), item->cropStart(), item->cropStart() + GenTime(endPos.x(), m_document->fps()) - item->startPos());
        item->resizeEnd(endPos.x(), m_scale);
    }
    m_document->renderer()->doRefresh();
}

double CustomTrackView::getSnapPointForPos(double pos) {
    for (int i = 0; i < m_snapPoints.size(); ++i) {
        if (abs(pos - m_snapPoints.at(i).frames(m_document->fps()) * m_scale) < 10) {
            //kDebug()<<" FOUND SNAP POINT AT: "<<m_snapPoints.at(i)<<", current pos: "<<pos / m_scale;
            return m_snapPoints.at(i).frames(m_document->fps()) * m_scale + 0.5;
        }
        if (m_snapPoints.at(i).frames(m_document->fps() * m_scale) > pos) break;
    }
    return pos;
}

void CustomTrackView::updateSnapPoints(AbstractClipItem *selected) {
    m_snapPoints.clear();
    GenTime offset;
    if (selected) offset = selected->duration();
    QList<QGraphicsItem *> itemList = items();
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET && itemList.at(i) != selected) {
            ClipItem *item = (ClipItem *)itemList.at(i);
            GenTime start = item->startPos();
            GenTime end = item->endPos();
            m_snapPoints.append(start);
            m_snapPoints.append(end);
            if (offset != GenTime()) {
                if (start > offset) m_snapPoints.append(start - offset);
                if (end > offset) m_snapPoints.append(end - offset);
            }
        }
    }
    qSort(m_snapPoints);
    //for (int i = 0; i < m_snapPoints.size(); ++i)
    //    kDebug() << "SNAP POINT: " << m_snapPoints.at(i).frames(25);
}


void CustomTrackView::setScale(double scaleFactor) {
    //scale(scaleFactor, scaleFactor);
    double pos = cursorPos() / m_scale;
    m_scale = scaleFactor;
    kDebug() << " HHHHHHHH  SCALING: " << m_scale;
    QList<QGraphicsItem *> itemList = items();

    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET || itemList.at(i)->type() == TRANSITIONWIDGET) {
            AbstractClipItem *clip = (AbstractClipItem *)itemList.at(i);
            clip->setRect(clip->startPos().frames(m_document->fps()) * m_scale, clip->rect().y(), clip->duration().frames(m_document->fps()) * m_scale, clip->rect().height());
        }
    }
    updateCursorPos();
    centerOn(QPointF(cursorPos(), m_tracksHeight));
    scene()->setSceneRect(0, 0, (m_projectDuration + 500) * m_scale, scene()->sceneRect().height());
}

void CustomTrackView::drawBackground(QPainter * painter, const QRectF & rect) {
    QRect rectInView = viewport()->rect();
    rectInView.moveTo(horizontalScrollBar()->value(), verticalScrollBar()->value());

    QColor base = palette().button().color();
    painter->setClipRect(rect);
    painter->drawLine(rectInView.left(), 0, rectInView.right(), 0);
    uint max = m_tracksList.count();
    for (uint i = 0; i < max;i++) {
        if (m_tracksList.at(max - i - 1).type == AUDIOTRACK) painter->fillRect(rectInView.left(), m_tracksHeight * i + 1, rectInView.right() - rectInView.left() + 1, m_tracksHeight - 1, QBrush(QColor(240, 240, 255)));
        painter->drawLine(rectInView.left(), m_tracksHeight * (i + 1), rectInView.right(), m_tracksHeight * (i + 1));
        //painter->drawText(QRectF(10, 50 * i, 100, 50 * i + 49), Qt::AlignLeft, i18n(" Track ") + QString::number(i + 1));
    }
    int lowerLimit = m_tracksHeight * m_tracksList.count() + 1;
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
