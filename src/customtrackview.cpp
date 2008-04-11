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
#include <QApplication>

#include <KDebug>
#include <KLocale>
#include <KUrl>
#include <KIcon>
#include <KCursor>

#include "customtrackview.h"
#include "docclipbase.h"
#include "clipitem.h"
#include "definitions.h"
#include "moveclipcommand.h"
#include "movetransitioncommand.h"
#include "resizeclipcommand.h"
#include "addtimelineclipcommand.h"
#include "addeffectcommand.h"
#include "editeffectcommand.h"
#include "addtransitioncommand.h"
#include "edittransitioncommand.h"
#include "razorclipcommand.h"
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
        : QGraphicsView(projectscene, parent), m_cursorPos(0), m_dropItem(NULL), m_cursorLine(NULL), m_operationMode(NONE), m_dragItem(NULL), m_visualTip(NULL), m_moveOpMode(NONE), m_animation(NULL), m_projectDuration(0), m_scale(1.0), m_clickPoint(QPoint()), m_document(doc), m_autoScroll(KdenliveSettings::autoscroll()), m_tracksHeight(KdenliveSettings::trackheight()), m_tool(SELECTTOOL) {
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

    KIcon razorIcon("edit-cut");
    m_razorCursor = QCursor(razorIcon.pixmap(22, 22));
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
        if (m_dragItem && m_tool == SELECTTOOL) { //event->button() == Qt::LeftButton) {
            // a button was pressed, delete visual tips
            if (m_operationMode == MOVE && (event->pos() - m_clickEvent).manhattanLength() >= QApplication::startDragDistance()) {
                double snappedPos = getSnapPointForPos(mapToScene(event->pos()).x() - m_clickPoint.x());
                //kDebug() << "///////  MOVE CLIP, EVENT Y: "<<m_clickPoint.y();//<<event->scenePos().y()<<", SCENE HEIGHT: "<<scene()->sceneRect().height();
                int moveTrack = (int)  mapToScene(event->pos() + QPoint(0, (m_dragItem->type() == TRANSITIONWIDGET ?/* m_tracksHeight*/ - m_clickPoint.y() : 0))).y() / m_tracksHeight;
                int currentTrack = m_dragItem->track();

                if (moveTrack > 1000)moveTrack = 0;
                else if (moveTrack > m_tracksList.count() - 1) moveTrack = m_tracksList.count() - 1;
                else if (moveTrack < 0) moveTrack = 0;

                int offset = moveTrack - currentTrack;
                if (offset != 0) offset = m_tracksHeight * offset;
                m_dragItem->moveTo((int)(snappedPos / m_scale), m_scale, offset, moveTrack);
            } else if (m_operationMode == RESIZESTART) {
                double snappedPos = getSnapPointForPos(mapToScene(event->pos()).x());
                m_dragItem->resizeStart((int)(snappedPos / m_scale), m_scale);
            } else if (m_operationMode == RESIZEEND) {
                double snappedPos = getSnapPointForPos(mapToScene(event->pos()).x());
                m_dragItem->resizeEnd((int)(snappedPos / m_scale), m_scale);
            } else if (m_operationMode == FADEIN) {
                int pos = (int)(mapToScene(event->pos()).x() / m_scale);
                m_dragItem->setFadeIn((int)(pos - m_dragItem->startPos().frames(m_document->fps())), m_scale);
            } else if (m_operationMode == FADEOUT) {
                int pos = (int)(mapToScene(event->pos()).x() / m_scale);
                m_dragItem->setFadeOut((int)(m_dragItem->endPos().frames(m_document->fps()) - pos), m_scale);
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
        if (m_tool == RAZORTOOL) {
            setCursor(m_razorCursor);
            QGraphicsView::mouseMoveEvent(event);
            return;
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
                    polygon << QPoint((int)clip->rect().x(), (int)(clip->rect().y() + clip->rect().height() / 2 - size * 2));
                    polygon << QPoint((int)(clip->rect().x() + size * 2), (int)(clip->rect().y() + clip->rect().height() / 2));
                    polygon << QPoint((int)clip->rect().x(), (int)(clip->rect().y() + clip->rect().height() / 2 + size * 2));
                    polygon << QPoint((int)clip->rect().x(), (int)(clip->rect().y() + clip->rect().height() / 2 - size * 2));

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
                    polygon << QPoint((int)(clip->rect().x() + clip->rect().width()), (int)(clip->rect().y() + clip->rect().height() / 2 - size * 2));
                    polygon << QPoint((int)(clip->rect().x() + clip->rect().width() - size * 2), (int)(clip->rect().y() + clip->rect().height() / 2));
                    polygon << QPoint((int)(clip->rect().x() + clip->rect().width()), (int)(clip->rect().y() + clip->rect().height() / 2 + size * 2));
                    polygon << QPoint((int)(clip->rect().x() + clip->rect().width()), (int)(clip->rect().y() + clip->rect().height() / 2 - size * 2));

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
                setCursorPos((int)(mapToScene(event->pos().x(), 0).x() / m_scale));
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
    m_clickEvent = event->pos();
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
        m_dragItem = NULL;
        QList<QGraphicsItem *> collisionList = items(event->pos());
        for (int i = 0; i < collisionList.size(); ++i) {
            QGraphicsItem *item = collisionList.at(i);
            if (item->type() == AVWIDGET || item->type() == TRANSITIONWIDGET) {
                if (m_tool == RAZORTOOL) {
                    if (item->type() == TRANSITIONWIDGET) return;
                    AbstractClipItem *clip = (AbstractClipItem *) item;
                    ItemInfo info;
                    info.startPos = clip->startPos();
                    info.endPos = clip->endPos();
                    info.track = clip->track();
                    RazorClipCommand* command = new RazorClipCommand(this, info, GenTime((int)(mapToScene(event->pos()).x() / m_scale), m_document->fps()), true);
                    m_commandStack->push(command);
                    m_document->setModified(true);
                    return;
                }
                // select item
                if (!item->isSelected()) {
                    QList<QGraphicsItem *> itemList = items();
                    for (int i = 0; i < itemList.count(); i++)
                        itemList.at(i)->setSelected(false);
                    item->setSelected(true);
                    update();
                }

                m_dragItem = (AbstractClipItem *) item;

                m_clickPoint = QPoint((int)(mapToScene(event->pos()).x() - m_dragItem->startPos().frames(m_document->fps()) * m_scale), (int)(event->pos().y() - m_dragItem->rect().top()));
                m_dragItemInfo.startPos = m_dragItem->startPos();
                m_dragItemInfo.endPos = m_dragItem->endPos();
                m_dragItemInfo.track = m_dragItem->track();

                m_operationMode = m_dragItem->operationMode(item->mapFromScene(mapToScene(event->pos())), m_scale);
                if (m_operationMode == MOVE) setCursor(Qt::ClosedHandCursor);
                if (m_operationMode == TRANSITIONSTART) {
                    ItemInfo info;
                    info.startPos = m_dragItem->startPos();
                    info.endPos = info.startPos + GenTime(2.5);
                    info.track = m_dragItem->track();
                    int transitiontrack = getPreviousVideoTrack(info.track);
                    slotAddTransition((ClipItem *) m_dragItem, info, transitiontrack);
                }
                if (m_operationMode == TRANSITIONEND) {
                    ItemInfo info;
                    info.endPos = m_dragItem->endPos();
                    info.startPos = info.endPos - GenTime(2.5);
                    info.track = m_dragItem->track();
                    int transitiontrack = info.track - 1;
                    slotAddTransition((ClipItem *) m_dragItem, info, transitiontrack);
                }
                updateSnapPoints(m_dragItem);
                collision = true;
                break;
            }
        }
        emit clipItemSelected((m_dragItem && m_dragItem->type() == AVWIDGET) ? (ClipItem*) m_dragItem : NULL);
        emit transitionItemSelected((m_dragItem && m_dragItem->type() == TRANSITIONWIDGET) ? (Transition*) m_dragItem : NULL);

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
            } else setCursorPos((int)(mapToScene(event->x(), 0).x() / m_scale));
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
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, m_tracksList.count() - track);
    if (clip) {
        QMap <QString, QString> effectParams = clip->addEffect(effect);
        m_document->renderer()->mltAddEffect(track, pos, effectParams);
        emit clipItemSelected(clip);
    }
}

void CustomTrackView::deleteEffect(int track, GenTime pos, QDomElement effect) {
    QString index = effect.attribute("kdenlive_ix");
    m_document->renderer()->mltRemoveEffect(track, pos, index);
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, m_tracksList.count() - track);
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
        ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, track);
        if (clip) itemList.append(clip);
        else kDebug() << "------   wrning, clip eff not found";
    }
    kDebug() << "// REQUESTING EFFECT ON CLIP: " << pos.frames(25) << ", TRK: " << track;
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET && itemList.at(i)->isSelected()) {
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
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, m_tracksList.count() - track);
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

void CustomTrackView::cutClip(ItemInfo info, GenTime cutTime, bool cut) {
    if (cut) {
        // cut clip
        ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()), info.track);
        int cutPos = (int) cutTime.frames(m_document->fps());
        ItemInfo newPos;
        newPos.startPos = cutTime;
        newPos.endPos = info.endPos;
        newPos.track = info.track;
        ClipItem *dup = new ClipItem(item->baseClip(), newPos, m_scale, m_document->fps());
        dup->setCropStart(dup->cropStart() + (cutTime - info.startPos));
        item->resizeEnd(cutPos - 1, m_scale);
        scene()->addItem(dup);
        m_document->renderer()->mltCutClip(m_tracksList.count() - info.track, cutTime);
    } else {
        // uncut clip
        ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()), info.track);
        ClipItem *dup = getClipItemAt((int) cutTime.frames(m_document->fps()), info.track);
        delete dup;
        item->resizeEnd((int) info.endPos.frames(m_document->fps()), m_scale);
        m_document->renderer()->mltRemoveClip(m_tracksList.count() - info.track, cutTime);
        m_document->renderer()->mltResizeClipEnd(m_tracksList.count() - info.track, info.startPos, item->cropStart(), item->cropStart() + info.endPos - info.startPos);
    }
}

void CustomTrackView::slotAddTransition(ClipItem* clip, ItemInfo transitionInfo, int endTrack, QDomElement transition) {
    AddTransitionCommand* command = new AddTransitionCommand(this, transitionInfo, endTrack, transition, true);
    m_commandStack->push(command);
    m_document->setModified(true);
}

void CustomTrackView::addTransition(ItemInfo transitionInfo, int endTrack, QDomElement params) {
    Transition *tr = new Transition(transitionInfo, endTrack, m_scale, m_document->fps(), params);
    scene()->addItem(tr);

    //kDebug() << "---- ADDING transition " << e.attribute("tag") << ", on tracks " << m_tracksList.count() - e.attribute("transition_track").toInt() << " / " << getPreviousVideoTrack(e.attribute("transition_track").toInt());
    m_document->renderer()->mltAddTransition(tr->transitionTag(), endTrack, m_tracksList.count() - transitionInfo.track, transitionInfo.startPos, transitionInfo.endPos, tr->toXML());
    m_document->setModified(true);
}

void CustomTrackView::deleteTransition(ItemInfo transitionInfo, int endTrack, QDomElement params) {
    Transition *item = getTransitionItemAt((int)transitionInfo.startPos.frames(m_document->fps()) + 1, transitionInfo.track);
    m_document->renderer()->mltDeleteTransition(item->transitionTag(), endTrack, m_tracksList.count() - transitionInfo.track, transitionInfo.startPos, transitionInfo.endPos, item->toXML());
    delete item;
    m_document->setModified(true);
}

void CustomTrackView::slotTransitionUpdated(Transition *tr, QDomElement old) {
    EditTransitionCommand *command = new EditTransitionCommand(this, tr->track(), tr->startPos(), old, tr->toXML() , true);
    m_commandStack->push(command);
    m_document->setModified(true);
}

void CustomTrackView::updateTransition(int track, GenTime pos, QDomElement oldTransition, QDomElement transition) {
    Transition *item = getTransitionItemAt((int)pos.frames(m_document->fps()) + 1, track);
    m_document->renderer()->mltUpdateTransition(oldTransition.attribute("tag"), transition.attribute("tag"), transition.attribute("transition_btrack").toInt(), m_tracksList.count() - transition.attribute("transition_atrack").toInt(), item->startPos(), item->endPos(), transition);
    repaint();
    m_document->setModified(true);
}

void CustomTrackView::addItem(DocClipBase *clip, QPoint pos) {
    ItemInfo info;
    info.startPos = GenTime((int)(mapToScene(pos).x() / m_scale), m_document->fps());
    info.endPos = info.startPos + clip->duration();
    info.track = (int)(pos.y() / m_tracksHeight);
    //kDebug()<<"------------  ADDING CLIP ITEM----: "<<info.startPos.frames(25)<<", "<<info.endPos.frames(25)<<", "<<info.track;
    m_dropItem = new ClipItem(clip, info, m_scale, m_document->fps());
    scene()->addItem(m_dropItem);
}


void CustomTrackView::dragMoveEvent(QDragMoveEvent * event) {
    event->setDropAction(Qt::IgnoreAction);
    kDebug() << "+++++++++++++   DRAG MOVE, : " << mapToScene(event->pos()).x() << ", SCAL: " << m_scale;
    if (m_dropItem) {
        int track = (int)(event->pos().y() / m_tracksHeight);  //) * (m_scale * 50) + m_scale;
        m_dropItem->moveTo((int)(mapToScene(event->pos()).x() / m_scale), m_scale, (int)((track - m_dropItem->track()) * m_tracksHeight), track);
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
        ItemInfo info;
        info.startPos = m_dropItem->startPos();
        info.endPos = m_dropItem->endPos();
        info.track = m_dropItem->track();
        AddTimelineClipCommand *command = new AddTimelineClipCommand(this, m_dropItem->xml(), m_dropItem->clipProducer(), info, false, false);
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
    kDebug() << "/////////////  PRO DUR: " << duration << ", SCALE. " << (m_projectDuration + 500) * m_scale << ", height: " << 50 * m_tracksList.count();
    m_projectDuration = duration;
    setSceneRect(0, 0, (m_projectDuration + 100) * m_scale, sceneRect().height());
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
                ItemInfo info;
                info.startPos = item->startPos();
                info.endPos = item->endPos();
                info.track = item->track();
                AddTimelineClipCommand *command = new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), info, true, true);
                m_commandStack->push(command);
                //delete item;
            }
        }
    }
}

void CustomTrackView::setCursorPos(int pos, bool seek) {
    emit cursorMoved((int)(m_cursorPos * m_scale), (int)(pos * m_scale));
    m_cursorPos = pos;
    m_cursorLine->setPos(pos * m_scale, 0);
    if (seek) m_document->renderer()->seek(GenTime(pos, m_document->fps()));
    else if (m_autoScroll && m_scale < 50) checkScrolling();
}

void CustomTrackView::updateCursorPos() {
    m_cursorLine->setPos(m_cursorPos * m_scale, 0);
}

int CustomTrackView::cursorPos() {
    return (int)(m_cursorPos * m_scale);
}

void CustomTrackView::checkScrolling() {
    QRect rectInView = viewport()->rect();
    int delta = rectInView.width() / 3;
    int max = rectInView.right() + horizontalScrollBar()->value() - delta;
    //kDebug() << "CURSOR POS: "<<m_cursorPos<< "Scale: "<<m_scale;
    if (m_cursorPos * m_scale >= max) horizontalScrollBar()->setValue((int)(horizontalScrollBar()->value() + 1 + m_scale));
}

void CustomTrackView::mouseReleaseEvent(QMouseEvent * event) {
    QGraphicsView::mouseReleaseEvent(event);
    setDragMode(QGraphicsView::NoDrag);
    if (m_dragItem == NULL) return;
    ItemInfo info;
    info.startPos = m_dragItem->startPos();
    info.endPos = m_dragItem->endPos();
    info.track = m_dragItem->track();

    if (m_operationMode == MOVE && m_dragItemInfo.startPos != info.startPos) {
        setCursor(Qt::OpenHandCursor);
        // move clip
        if (m_dragItem->type() == AVWIDGET) {
            MoveClipCommand *command = new MoveClipCommand(this, m_dragItemInfo, info, false);
            m_commandStack->push(command);
            m_document->renderer()->mltMoveClip((int)(m_tracksList.count() - m_dragItemInfo.track), (int)(m_tracksList.count() - m_dragItem->track()), (int) m_dragItemInfo.startPos.frames(m_document->fps()), (int)(m_dragItem->startPos().frames(m_document->fps())));
        }
        if (m_dragItem->type() == TRANSITIONWIDGET) {
            MoveTransitionCommand *command = new MoveTransitionCommand(this, m_dragItemInfo, info, false);
            m_commandStack->push(command);
            //kDebug()<<"/// MOVING TRS FROM: "<<(int)(m_tracksList.count() - m_startPos.y())<<", OFFSET: "<<(int) (m_dragItem->track() - m_startPos.y());
            Transition *transition = (Transition *) m_dragItem;
            m_document->renderer()->mltMoveTransition(transition->transitionTag(), (int)(m_tracksList.count() - m_dragItemInfo.track), (int)(m_dragItemInfo.track - m_dragItem->track()), m_dragItemInfo.startPos, m_dragItemInfo.endPos, info.startPos, info.endPos);
        }

    } else if (m_operationMode == RESIZESTART) {
        // resize start
        if (m_dragItem->type() == AVWIDGET) {
            m_document->renderer()->mltResizeClipStart(m_tracksList.count() - m_dragItem->track(), m_dragItem->endPos(), m_dragItem->startPos(), m_dragItemInfo.startPos, m_dragItem->cropStart(), m_dragItem->cropStart() + m_dragItem->endPos() - m_dragItem->startPos());
            ResizeClipCommand *command = new ResizeClipCommand(this, m_dragItemInfo, info, false);
            m_commandStack->push(command);
        } else if (m_dragItem->type() == TRANSITIONWIDGET) {
            MoveTransitionCommand *command = new MoveTransitionCommand(this, m_dragItemInfo, info, false);
            m_commandStack->push(command);
            Transition *transition = (Transition *) m_dragItem;
            m_document->renderer()->mltMoveTransition(transition->transitionTag(), (int)(m_tracksList.count() - m_dragItemInfo.track), 0, m_dragItemInfo.startPos, m_dragItemInfo.endPos, info.startPos, info.endPos);
        }

        m_document->renderer()->doRefresh();
    } else if (m_operationMode == RESIZEEND) {
        // resize end
        if (m_dragItem->type() == AVWIDGET) {
            ResizeClipCommand *command = new ResizeClipCommand(this, m_dragItemInfo, info, false);
            m_document->renderer()->mltResizeClipEnd(m_tracksList.count() - m_dragItem->track(), m_dragItem->startPos(), m_dragItem->cropStart(), m_dragItem->cropStart() + m_dragItem->endPos() - m_dragItem->startPos());
            m_commandStack->push(command);
        } else if (m_dragItem->type() == TRANSITIONWIDGET) {

            MoveTransitionCommand *command = new MoveTransitionCommand(this, m_dragItemInfo, info, false);
            m_commandStack->push(command);
            Transition *transition = (Transition *) m_dragItem;
            m_document->renderer()->mltMoveTransition(transition->transitionTag(), (int)(m_tracksList.count() - m_dragItemInfo.track), 0, m_dragItemInfo.startPos, m_dragItemInfo.endPos, info.startPos, info.endPos);
        }
        m_document->renderer()->doRefresh();
    }
    m_document->setModified(true);
    m_operationMode = NONE;
    m_dragItem = NULL;
}

void CustomTrackView::deleteClip(ItemInfo info) {
    ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()), info.track);
    if (!item) {
        kDebug() << "----------------  ERROR, CANNOT find clip to move at...";// << rect.x();
        return;
    }
    item->baseClip()->removeReference();
    m_document->updateClip(item->baseClip()->getId());
    delete item;
    m_document->renderer()->mltRemoveClip(m_tracksList.count() - info.track, info.startPos);
    m_document->renderer()->doRefresh();
}

void CustomTrackView::deleteSelectedClips() {
    QList<QGraphicsItem *> itemList = items();
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET && itemList.at(i)->isSelected()) {
            ClipItem *item = (ClipItem *) itemList.at(i);
            ItemInfo info;
            info.startPos = item->startPos();
            info.endPos = item->endPos();
            info.track = item->track();
            AddTimelineClipCommand *command = new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), info, true, true);
            m_commandStack->push(command);
        }
    }
}

void CustomTrackView::cutSelectedClips() {
    QList<QGraphicsItem *> itemList = items();
    GenTime currentPos = GenTime(m_cursorPos, m_document->fps());
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET && itemList.at(i)->isSelected()) {
            ClipItem *item = (ClipItem *) itemList.at(i);
            ItemInfo info;
            info.startPos = item->startPos();
            info.endPos = item->endPos();
            if (currentPos > info.startPos && currentPos <  info.endPos) {
                info.track = item->track();
                RazorClipCommand *command = new RazorClipCommand(this, info, currentPos, true);
                m_commandStack->push(command);
            }
        }
    }
}

void CustomTrackView::addClip(QDomElement xml, int clipId, ItemInfo info) {
    DocClipBase *baseclip = m_document->clipManager()->getClipById(clipId);
    ClipItem *item = new ClipItem(baseclip, info, m_scale, m_document->fps());
    scene()->addItem(item);
    baseclip->addReference();
    m_document->updateClip(baseclip->getId());
    m_document->renderer()->mltInsertClip(m_tracksList.count() - info.track, info.startPos, xml);
    m_document->renderer()->doRefresh();
}

ClipItem *CustomTrackView::getClipItemAt(int pos, int track) {
    QList<QGraphicsItem *> list = scene()->items(QPointF(pos * m_scale, track * m_tracksHeight + m_tracksHeight / 2));
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->type() == AVWIDGET) {
            clip = static_cast <ClipItem *>(list.at(i));
            break;
        }
    }
    return clip;
}

ClipItem *CustomTrackView::getClipItemAt(GenTime pos, int track) {
    int framepos = (int)(pos.frames(m_document->fps()) * m_scale);
    return getClipItemAt(framepos, track);
}

Transition *CustomTrackView::getTransitionItemAt(int pos, int track) {
    QList<QGraphicsItem *> list = scene()->items(QPointF(pos * m_scale, track * m_tracksHeight + m_tracksHeight / 2));
    Transition *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->type() == TRANSITIONWIDGET) {
            clip = static_cast <Transition *>(list.at(i));
            break;
        }
    }
    return clip;
}

Transition *CustomTrackView::getTransitionItemAt(GenTime pos, int track) {
    int framepos = (int)(pos.frames(m_document->fps()) * m_scale);
    return getTransitionItemAt(framepos, track);
}

void CustomTrackView::moveClip(const ItemInfo start, const ItemInfo end) {
    ClipItem *item = getClipItemAt((int) start.startPos.frames(m_document->fps()) + 1, start.track);
    if (!item) {
        kDebug() << "----------------  ERROR, CANNOT find clip to move at.. ";// << startPos.x() * m_scale * FRAME_SIZE + 1 << ", " << startPos.y() * m_tracksHeight + m_tracksHeight / 2;
        return;
    }
    //kDebug() << "----------------  Move CLIP FROM: " << startPos.x() << ", END:" << endPos.x() << ",TRACKS: " << startPos.y() << " TO " << endPos.y();
    item->moveTo((int) end.startPos.frames(m_document->fps()), m_scale, (int)((end.track - start.track) * m_tracksHeight), end.track);
    m_document->renderer()->mltMoveClip((int)(m_tracksList.count() - start.track), (int)(m_tracksList.count() - end.track), (int) start.startPos.frames(m_document->fps()), (int)end.startPos.frames(m_document->fps()));
}

void CustomTrackView::moveTransition(const ItemInfo start, const ItemInfo end) {
    Transition *item = getTransitionItemAt((int)start.startPos.frames(m_document->fps()) + 1, start.track);
    if (!item) {
        kDebug() << "----------------  ERROR, CANNOT find transition to move... ";// << startPos.x() * m_scale * FRAME_SIZE + 1 << ", " << startPos.y() * m_tracksHeight + m_tracksHeight / 2;
        return;
    }
    //kDebug() << "----------------  Move TRANSITION FROM: " << startPos.x() << ", END:" << endPos.x() << ",TRACKS: " << oldtrack << " TO " << newtrack;

    //kDebug()<<"///  RESIZE TRANS START: ("<< startPos.x()<<"x"<< startPos.y()<<") / ("<<endPos.x()<<"x"<< endPos.y()<<")";
    if (end.endPos - end.startPos == start.endPos - start.startPos) {
        // Transition was moved
        item->moveTo((int) end.startPos.frames(m_document->fps()), m_scale, (end.track - start.track) * m_tracksHeight, end.track);
    } else if (end.endPos == start.endPos) {
        // Transition start resize
        item->resizeStart((int) end.startPos.frames(m_document->fps()), m_scale);
    } else {
        // Transition end resize;
        item->resizeEnd((int) end.endPos.frames(m_document->fps()), m_scale);
    }
    //item->moveTransition(GenTime((int) (endPos.x() - startPos.x()), m_document->fps()));
    m_document->renderer()->mltMoveTransition(item->transitionTag(), m_tracksList.count() - start.track, start.track - end.track, start.startPos, start.endPos, end.startPos, end.endPos);
}

void CustomTrackView::resizeClip(const ItemInfo start, const ItemInfo end) {
    int offset;
    bool resizeClipStart = true;
    if (start.startPos == end.startPos) resizeClipStart = false;
    if (resizeClipStart) offset = 1;
    else offset = -1;
    ClipItem *item = getClipItemAt((int)(start.startPos.frames(m_document->fps()) + offset), start.track);
    if (!item) {
        kDebug() << "----------------  ERROR, CANNOT find clip to resize at... "; // << startPos;
        return;
    }
    if (resizeClipStart) {
        m_document->renderer()->mltResizeClipStart(m_tracksList.count() - item->track(), item->endPos(), end.startPos, item->startPos(), item->cropStart() + end.startPos - start.startPos, item->cropStart() + end.startPos - start.startPos + item->endPos() - end.startPos);
        item->resizeStart((int) end.startPos.frames(m_document->fps()), m_scale);
    } else {
        m_document->renderer()->mltResizeClipEnd(m_tracksList.count() - item->track(), item->startPos(), item->cropStart(), item->cropStart() + end.startPos - item->startPos());
        item->resizeEnd((int) end.startPos.frames(m_document->fps()), m_scale);
    }
    m_document->renderer()->doRefresh();
}

double CustomTrackView::getSnapPointForPos(double pos) {
    for (int i = 0; i < m_snapPoints.size(); ++i) {
        if (abs((int)(pos - m_snapPoints.at(i).frames(m_document->fps()) * m_scale)) < 10) {
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
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            GenTime start = item->startPos();
            GenTime end = item->endPos();
            m_snapPoints.append(start);
            m_snapPoints.append(end);
            if (offset != GenTime()) {
                if (start > offset) m_snapPoints.append(start - offset);
                if (end > offset) m_snapPoints.append(end - offset);
            }
        } else if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            Transition *transition = static_cast <Transition*>(itemList.at(i));
            GenTime start = transition->startPos();
            GenTime end = transition->endPos();
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

void CustomTrackView::setTool(PROJECTTOOL tool) {
    m_tool = tool;
}

void CustomTrackView::setScale(double scaleFactor) {
    //scale(scaleFactor, scaleFactor);
    double pos = cursorPos() / m_scale;
    m_scale = scaleFactor;
    int vert = verticalScrollBar()->value();
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
    setSceneRect(0, 0, (m_projectDuration + 100) * m_scale, sceneRect().height());
    verticalScrollBar()->setValue(vert);
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
