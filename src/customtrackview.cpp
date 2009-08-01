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


#include "customtrackview.h"
#include "customtrackscene.h"
#include "docclipbase.h"
#include "clipitem.h"
#include "definitions.h"
#include "moveclipcommand.h"
#include "movetransitioncommand.h"
#include "resizeclipcommand.h"
#include "editguidecommand.h"
#include "addtimelineclipcommand.h"
#include "addeffectcommand.h"
#include "editeffectcommand.h"
#include "moveeffectcommand.h"
#include "addtransitioncommand.h"
#include "edittransitioncommand.h"
#include "editkeyframecommand.h"
#include "changespeedcommand.h"
#include "addmarkercommand.h"
#include "razorclipcommand.h"
#include "kdenlivesettings.h"
#include "transition.h"
#include "clipmanager.h"
#include "renderer.h"
#include "markerdialog.h"
#include "mainwindow.h"
#include "ui_keyframedialog_ui.h"
#include "clipdurationdialog.h"
#include "abstractgroupitem.h"
#include "insertspacecommand.h"
#include "spacerdialog.h"
#include "addtrackcommand.h"
#include "changetrackcommand.h"
#include "movegroupcommand.h"
#include "ui_addtrack_ui.h"
#include "initeffects.h"
#include "locktrackcommand.h"
#include "groupclipscommand.h"
#include "splitaudiocommand.h"
#include "changecliptypecommand.h"

#include <KDebug>
#include <KLocale>
#include <KUrl>
#include <KIcon>
#include <KCursor>
#include <KColorScheme>

#include <QMouseEvent>
#include <QStylePainter>
#include <QGraphicsItem>
#include <QDomDocument>
#include <QScrollBar>
#include <QApplication>
#include <QInputDialog>


bool sortGuidesList(const Guide *g1 , const Guide *g2)
{
    return (*g1).position() < (*g2).position();
}


//TODO:
// disable animation if user asked it in KDE's global settings
// http://lists.kde.org/?l=kde-commits&m=120398724717624&w=2
// needs something like below (taken from dolphin)
// #include <kglobalsettings.h>
// const bool animate = KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects;
// const int duration = animate ? 1500 : 1;

CustomTrackView::CustomTrackView(KdenliveDoc *doc, CustomTrackScene* projectscene, QWidget *parent) :
        QGraphicsView(projectscene, parent),
        m_tracksHeight(KdenliveSettings::trackheight()),
        m_projectDuration(0),
        m_cursorPos(0),
        m_document(doc),
        m_scene(projectscene),
        m_cursorLine(NULL),
        m_operationMode(NONE),
        m_moveOpMode(NONE),
        m_dragItem(NULL),
        m_dragGuide(NULL),
        m_visualTip(NULL),
        m_animation(NULL),
        m_clickPoint(),
        m_autoScroll(KdenliveSettings::autoscroll()),
        m_pasteEffectsAction(NULL),
        m_ungroupAction(NULL),
        m_scrollOffset(0),
        m_clipDrag(false),
        m_findIndex(0),
        m_tool(SELECTTOOL),
        m_copiedItems(),
        m_menuPosition(),
        m_blockRefresh(false),
        m_selectionGroup(NULL)
{
    if (doc) m_commandStack = doc->commandStack();
    else m_commandStack = NULL;
    setMouseTracking(true);
    setAcceptDrops(true);
    KdenliveSettings::setTrackheight(m_tracksHeight);
    m_animationTimer = new QTimeLine(800);
    m_animationTimer->setFrameRange(0, 5);
    m_animationTimer->setUpdateInterval(100);
    m_animationTimer->setLoopCount(0);
    m_tipColor = QColor(0, 192, 0, 200);
    QColor border = QColor(255, 255, 255, 100);
    m_tipPen.setColor(border);
    m_tipPen.setWidth(3);
    setContentsMargins(0, 0, 0, 0);
    const int maxWidth = m_tracksHeight * m_document->tracksCount();
    setSceneRect(0, 0, sceneRect().width(), maxWidth);
    verticalScrollBar()->setMaximum(maxWidth);
    m_cursorLine = projectscene->addLine(0, 0, 0, maxWidth);
    m_cursorLine->setZValue(1000);

    KIcon razorIcon("edit-cut");
    m_razorCursor = QCursor(razorIcon.pixmap(22, 22));

    KIcon spacerIcon("kdenlive-spacer-tool");
    m_spacerCursor = QCursor(spacerIcon.pixmap(22, 22));
    verticalScrollBar()->setTracking(true);
    // Line below was supposed to scroll guides label with scrollbar, not implemented yet
    //connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(slotRefreshGuides()));
    connect(&m_scrollTimer, SIGNAL(timeout()), this, SLOT(slotCheckMouseScrolling()));
    m_scrollTimer.setInterval(100);
    m_scrollTimer.setSingleShot(true);

    connect(&m_thumbsTimer, SIGNAL(timeout()), this, SLOT(slotFetchNextThumbs()));
    m_thumbsTimer.setInterval(500);
    m_thumbsTimer.setSingleShot(true);
}

CustomTrackView::~CustomTrackView()
{
    qDeleteAll(m_guides);
    m_waitingThumbs.clear();
}

void CustomTrackView::setDocumentModified()
{
    m_document->setModified(true);
}

void CustomTrackView::setContextMenu(QMenu *timeline, QMenu *clip, QMenu *transition, QActionGroup *clipTypeGroup)
{
    m_timelineContextMenu = timeline;
    m_timelineContextClipMenu = clip;
    m_clipTypeGroup = clipTypeGroup;
    QList <QAction *> list = m_timelineContextClipMenu->actions();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->data().toString() == "paste_effects") m_pasteEffectsAction = list.at(i);
        else if (list.at(i)->data().toString() == "ungroup_clip") m_ungroupAction = list.at(i);
    }

    m_timelineContextTransitionMenu = transition;
    list = m_timelineContextTransitionMenu->actions();
    for (int i = 0; i < list.count(); i++) {
        if (list.at(i)->data().toString() == "auto") {
            m_autoTransition = list.at(i);
            break;
        }
    }

    m_timelineContextMenu->addSeparator();
    m_deleteGuide = new KAction(KIcon("edit-delete"), i18n("Delete Guide"), this);
    connect(m_deleteGuide, SIGNAL(triggered()), this, SLOT(slotDeleteTimeLineGuide()));
    m_timelineContextMenu->addAction(m_deleteGuide);

    m_editGuide = new KAction(KIcon("document-properties"), i18n("Edit Guide"), this);
    connect(m_editGuide, SIGNAL(triggered()), this, SLOT(slotEditTimeLineGuide()));
    m_timelineContextMenu->addAction(m_editGuide);
}

void CustomTrackView::checkAutoScroll()
{
    m_autoScroll = KdenliveSettings::autoscroll();
}

/*sQList <TrackInfo> CustomTrackView::tracksList() const {
    return m_scene->m_tracksList;
}*/

void CustomTrackView::checkTrackHeight()
{
    if (m_tracksHeight == KdenliveSettings::trackheight()) return;
    m_tracksHeight = KdenliveSettings::trackheight();
    emit trackHeightChanged();
    QList<QGraphicsItem *> itemList = items();
    ClipItem *item;
    Transition *transitionitem;
    bool snap = KdenliveSettings::snaptopoints();
    KdenliveSettings::setSnaptopoints(false);
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            item = (ClipItem*) itemList.at(i);
            item->setRect(0, 0, item->rect().width(), m_tracksHeight - 1);
            item->setPos((qreal) item->startPos().frames(m_document->fps()), (qreal) item->track() * m_tracksHeight + 1);
            item->resetThumbs(true);
        } else if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            transitionitem = (Transition*) itemList.at(i);
            transitionitem->setRect(0, 0, transitionitem->rect().width(), m_tracksHeight / 3 * 2 - 1);
            transitionitem->setPos((qreal) transitionitem->startPos().frames(m_document->fps()), (qreal) transitionitem->track() * m_tracksHeight + m_tracksHeight / 3 * 2);
        }
    }
    m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), m_tracksHeight * m_document->tracksCount());

    for (int i = 0; i < m_guides.count(); i++) {
        QLineF l = m_guides.at(i)->line();
        l.setP2(QPointF(l.x2(), m_tracksHeight * m_document->tracksCount()));
        m_guides.at(i)->setLine(l);
    }

    setSceneRect(0, 0, sceneRect().width(), m_tracksHeight * m_document->tracksCount());
//     verticalScrollBar()->setMaximum(m_tracksHeight * m_document->tracksCount());
    KdenliveSettings::setSnaptopoints(snap);
    viewport()->update();
}

/** Zoom or move viewport on mousewheel
 *
 * If mousewheel+Ctrl, zooms in/out on the timeline.
 *
 * With Ctrl, moves viewport towards end of timeline if down/back,
 * opposite on up/forward.
 *
 * See also http://www.kdenlive.org/mantis/view.php?id=265 */
void CustomTrackView::wheelEvent(QWheelEvent * e)
{
    if (e->modifiers() == Qt::ControlModifier) {
        if (e->delta() > 0) emit zoomIn();
        else emit zoomOut();
    } else {
        if (e->delta() <= 0) horizontalScrollBar()->setValue(horizontalScrollBar()->value() + horizontalScrollBar()->singleStep());
        else  horizontalScrollBar()->setValue(horizontalScrollBar()->value() - horizontalScrollBar()->singleStep());
    }
}

int CustomTrackView::getPreviousVideoTrack(int track)
{
    track = m_document->tracksCount() - track - 1;
    track --;
    for (int i = track; i > -1; i--) {
        if (m_document->trackInfoAt(i).type == VIDEOTRACK) return i + 1;
    }
    return 0;
}


void CustomTrackView::slotFetchNextThumbs()
{
    if (!m_waitingThumbs.isEmpty()) {
        ClipItem *item = m_waitingThumbs.takeFirst();
        while ((item == NULL) && !m_waitingThumbs.isEmpty()) {
            item = m_waitingThumbs.takeFirst();
        }
        if (item) item->slotFetchThumbs();
        if (!m_waitingThumbs.isEmpty()) m_thumbsTimer.start();
    }
}

void CustomTrackView::slotCheckMouseScrolling()
{
    if (m_scrollOffset == 0) {
        m_scrollTimer.stop();
        return;
    }
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() + m_scrollOffset);
    m_scrollTimer.start();
}

void CustomTrackView::slotCheckPositionScrolling()
{
    // If mouse is at a border of the view, scroll
    if (m_moveOpMode != SEEK) return;
    if (mapFromScene(m_cursorPos, 0).x() < 3) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - 2);
        QTimer::singleShot(200, this, SLOT(slotCheckPositionScrolling()));
        setCursorPos(mapToScene(QPoint(-2, 0)).x());
    } else if (viewport()->width() - 3 < mapFromScene(m_cursorPos + 1, 0).x()) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + 2);
        setCursorPos(mapToScene(QPoint(viewport()->width(), 0)).x() + 1);
        QTimer::singleShot(200, this, SLOT(slotCheckPositionScrolling()));
    }
}


// virtual

void CustomTrackView::mouseMoveEvent(QMouseEvent * event)
{
    int pos = event->x();
    int mappedXPos = (int)(mapToScene(event->pos()).x() + 0.5);
    emit mousePosition(mappedXPos);

    if (event->buttons() & Qt::MidButton) return;
    if (m_operationMode == RUBBERSELECTION || (event->modifiers() == Qt::ControlModifier && m_tool != SPACERTOOL)) {
        QGraphicsView::mouseMoveEvent(event);
        m_moveOpMode = NONE;
        return;
    }

    if (event->buttons() != Qt::NoButton) {
        bool move = (event->pos() - m_clickEvent).manhattanLength() >= QApplication::startDragDistance();
        if (m_dragItem && m_tool == SELECTTOOL) {
            if (m_operationMode == MOVE && move) {
                QGraphicsView::mouseMoveEvent(event);
                // If mouse is at a border of the view, scroll
                if (pos < 5) {
                    m_scrollOffset = -30;
                    m_scrollTimer.start();
                } else if (viewport()->width() - pos < 10) {
                    m_scrollOffset = 30;
                    m_scrollTimer.start();
                } else if (m_scrollTimer.isActive()) m_scrollTimer.stop();

            } else if (m_operationMode == RESIZESTART && move) {
                double snappedPos = getSnapPointForPos(mappedXPos);
                m_document->renderer()->pause();
                m_dragItem->resizeStart((int)(snappedPos));
            } else if (m_operationMode == RESIZEEND && move) {
                double snappedPos = getSnapPointForPos(mappedXPos);
                m_document->renderer()->pause();
                m_dragItem->resizeEnd((int)(snappedPos));
            } else if (m_operationMode == FADEIN && move) {
                ((ClipItem*) m_dragItem)->setFadeIn((int)(mappedXPos - m_dragItem->startPos().frames(m_document->fps())));
            } else if (m_operationMode == FADEOUT && move) {
                ((ClipItem*) m_dragItem)->setFadeOut((int)(m_dragItem->endPos().frames(m_document->fps()) - mappedXPos));
            } else if (m_operationMode == KEYFRAME && move) {
                GenTime keyFramePos = GenTime(mappedXPos, m_document->fps()) - m_dragItem->startPos() + m_dragItem->cropStart();
                double pos = mapToScene(event->pos()).toPoint().y();
                QRectF br = m_dragItem->sceneBoundingRect();
                double maxh = 100.0 / br.height();
                pos = (br.bottom() - pos) * maxh;
                m_dragItem->updateKeyFramePos(keyFramePos, pos);
            }

            delete m_animation;
            m_animation = NULL;
            delete m_visualTip;
            m_visualTip = NULL;
            return;
        } else if (m_operationMode == MOVEGUIDE) {
            delete m_animation;
            m_animation = NULL;
            delete m_visualTip;
            m_visualTip = NULL;
            QGraphicsView::mouseMoveEvent(event);
            return;
        } else if (m_operationMode == SPACER && move && m_selectionGroup) {
            // spacer tool
            int mappedClick = (int)(mapToScene(m_clickEvent).x() + 0.5);
            m_selectionGroup->setPos(mappedXPos + (((int) m_selectionGroup->boundingRect().topLeft().x() + 0.5) - mappedClick) , m_selectionGroup->pos().y());
        }
    }

    if (m_tool == RAZORTOOL) {
        setCursor(m_razorCursor);
        //QGraphicsView::mouseMoveEvent(event);
        //return;
    } else if (m_tool == SPACERTOOL) {
        setCursor(m_spacerCursor);
        return;
    }

    QList<QGraphicsItem *> itemList = items(event->pos());
    QGraphicsRectItem *item = NULL;
    OPERATIONTYPE opMode = NONE;

    if (itemList.count() == 1 && itemList.at(0)->type() == GUIDEITEM) {
        opMode = MOVEGUIDE;
    } else for (int i = 0; i < itemList.count(); i++) {
            if (itemList.at(i)->type() == AVWIDGET || itemList.at(i)->type() == TRANSITIONWIDGET) {
                item = (QGraphicsRectItem*) itemList.at(i);
                break;
            }
        }

    if (item && event->buttons() == Qt::NoButton) {
        AbstractClipItem *clip = static_cast <AbstractClipItem*>(item);
        if (m_tool == RAZORTOOL) {
            // razor tool over a clip, display current frame in monitor
            if (!m_blockRefresh && item->type() == AVWIDGET) {
                //TODO: solve crash when showing frame when moving razor over clip
                //emit showClipFrame(((ClipItem *) item)->baseClip(), mappedXPos - (clip->startPos() - clip->cropStart()).frames(m_document->fps()));
            }
            event->accept();
            return;
        }
        opMode = clip->operationMode(mapToScene(event->pos()));
        const double size = 5;
        if (opMode == m_moveOpMode) {
            QGraphicsView::mouseMoveEvent(event);
            return;
        } else {
            if (m_visualTip) {
                m_animationTimer->stop();
                delete m_animation;
                m_animation = NULL;
                delete m_visualTip;
                m_visualTip = NULL;
            }
        }
        m_moveOpMode = opMode;
        if (opMode == MOVE) {
            setCursor(Qt::OpenHandCursor);
        } else if (opMode == RESIZESTART) {
            setCursor(KCursor("left_side", Qt::SizeHorCursor));
            if (m_visualTip == NULL) {
                QRectF rect = clip->sceneBoundingRect();
                QPolygon polygon;
                polygon << QPoint(0, - size * 2);
                polygon << QPoint(size * 2, 0);
                polygon << QPoint(0, size * 2);
                polygon << QPoint(0, - size * 2);

                m_visualTip = new QGraphicsPolygonItem(polygon);
                ((QGraphicsPolygonItem*) m_visualTip)->setBrush(m_tipColor);
                ((QGraphicsPolygonItem*) m_visualTip)->setPen(m_tipPen);
                m_visualTip->setPos(rect.x(), rect.y() + rect.height() / 2);
                m_visualTip->setFlags(QGraphicsItem::ItemIgnoresTransformations);
                m_visualTip->setZValue(100);
                m_animation = new QGraphicsItemAnimation;
                m_animation->setItem(m_visualTip);
                m_animation->setTimeLine(m_animationTimer);
                m_animation->setScaleAt(.5, 2, 1);
                m_animation->setScaleAt(1, 1, 1);
                scene()->addItem(m_visualTip);
                m_animationTimer->start();
            }
        } else if (opMode == RESIZEEND) {
            setCursor(KCursor("right_side", Qt::SizeHorCursor));
            if (m_visualTip == NULL) {
                QRectF rect = clip->sceneBoundingRect();
                QPolygon polygon;
                polygon << QPoint(0, - size * 2);
                polygon << QPoint(- size * 2, 0);
                polygon << QPoint(0, size * 2);
                polygon << QPoint(0, - size * 2);

                m_visualTip = new QGraphicsPolygonItem(polygon);
                ((QGraphicsPolygonItem*) m_visualTip)->setBrush(m_tipColor);
                ((QGraphicsPolygonItem*) m_visualTip)->setPen(m_tipPen);
                m_visualTip->setFlags(QGraphicsItem::ItemIgnoresTransformations);
                m_visualTip->setPos(rect.right(), rect.y() + rect.height() / 2);
                m_visualTip->setZValue(100);
                m_animation = new QGraphicsItemAnimation;
                m_animation->setItem(m_visualTip);
                m_animation->setTimeLine(m_animationTimer);
                m_animation->setScaleAt(.5, 2, 1);
                m_animation->setScaleAt(1, 1, 1);
                scene()->addItem(m_visualTip);
                m_animationTimer->start();
            }
        } else if (opMode == FADEIN) {
            if (m_visualTip == NULL) {
                ClipItem *item = (ClipItem *) clip;
                QRectF rect = clip->sceneBoundingRect();
                m_visualTip = new QGraphicsEllipseItem(-size, -size, size * 2, size * 2);
                ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
                ((QGraphicsEllipseItem*) m_visualTip)->setPen(m_tipPen);
                m_visualTip->setPos(rect.x() + item->fadeIn(), rect.y());
                m_visualTip->setFlags(QGraphicsItem::ItemIgnoresTransformations);
                m_visualTip->setZValue(100);
                m_animation = new QGraphicsItemAnimation;
                m_animation->setItem(m_visualTip);
                m_animation->setTimeLine(m_animationTimer);
                m_animation->setScaleAt(.5, 2, 2);
                m_animation->setScaleAt(1, 1, 1);
                scene()->addItem(m_visualTip);
                m_animationTimer->start();
            }
            setCursor(Qt::PointingHandCursor);
        } else if (opMode == FADEOUT) {
            if (m_visualTip == NULL) {
                ClipItem *item = (ClipItem *) clip;
                QRectF rect = clip->sceneBoundingRect();
                m_visualTip = new QGraphicsEllipseItem(-size, -size, size * 2, size * 2);
                ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
                ((QGraphicsEllipseItem*) m_visualTip)->setPen(m_tipPen);
                m_visualTip->setPos(rect.right() - item->fadeOut(), rect.y());
                m_visualTip->setFlags(QGraphicsItem::ItemIgnoresTransformations);
                m_visualTip->setZValue(100);
                m_animation = new QGraphicsItemAnimation;
                m_animation->setItem(m_visualTip);
                m_animation->setTimeLine(m_animationTimer);
                m_animation->setScaleAt(.5, 2, 2);
                m_animation->setScaleAt(1, 1, 1);
                scene()->addItem(m_visualTip);
                m_animationTimer->start();
            }
            setCursor(Qt::PointingHandCursor);
        } else if (opMode == TRANSITIONSTART) {
            if (m_visualTip == NULL) {
                QRectF rect = clip->sceneBoundingRect();
                QPolygon polygon;
                polygon << QPoint(0, - size * 2);
                polygon << QPoint(size * 2, 0);
                polygon << QPoint(0, 0);
                polygon << QPoint(0, - size * 2);

                m_visualTip = new QGraphicsPolygonItem(polygon);
                ((QGraphicsPolygonItem*) m_visualTip)->setBrush(m_tipColor);
                ((QGraphicsPolygonItem*) m_visualTip)->setPen(m_tipPen);
                m_visualTip->setPos(rect.x(), rect.bottom());
                m_visualTip->setFlags(QGraphicsItem::ItemIgnoresTransformations);
                m_visualTip->setZValue(100);
                m_animation = new QGraphicsItemAnimation;
                m_animation->setItem(m_visualTip);
                m_animation->setTimeLine(m_animationTimer);
                m_animation->setScaleAt(.5, 2, 2);
                m_animation->setScaleAt(1, 1, 1);
                scene()->addItem(m_visualTip);
                m_animationTimer->start();
            }
            setCursor(Qt::PointingHandCursor);
        } else if (opMode == TRANSITIONEND) {
            if (m_visualTip == NULL) {
                QRectF rect = clip->sceneBoundingRect();
                QPolygon polygon;
                polygon << QPoint(0, - size * 2);
                polygon << QPoint(- size * 2, 0);
                polygon << QPoint(0, 0);
                polygon << QPoint(0, - size * 2);

                m_visualTip = new QGraphicsPolygonItem(polygon);
                ((QGraphicsPolygonItem*) m_visualTip)->setBrush(m_tipColor);
                ((QGraphicsPolygonItem*) m_visualTip)->setPen(m_tipPen);
                m_visualTip->setPos(rect.right(), rect.bottom());
                m_visualTip->setFlags(QGraphicsItem::ItemIgnoresTransformations);
                m_visualTip->setZValue(100);
                m_animation = new QGraphicsItemAnimation;
                m_animation->setItem(m_visualTip);
                m_animation->setTimeLine(m_animationTimer);
                m_animation->setScaleAt(.5, 2, 2);
                m_animation->setScaleAt(1, 1, 1);
                scene()->addItem(m_visualTip);
                m_animationTimer->start();
            }
            setCursor(Qt::PointingHandCursor);
        } else if (opMode == KEYFRAME) {
            setCursor(Qt::PointingHandCursor);
        }
    } // no clip under mouse
    else if (m_tool == RAZORTOOL) {
        event->accept();
        return;
    } else if (opMode == MOVEGUIDE) {
        m_moveOpMode = opMode;
        setCursor(Qt::SplitHCursor);
    } else {
        if (m_visualTip) {
            m_animationTimer->stop();
            delete m_animation;
            m_animation = NULL;
            delete m_visualTip;
            m_visualTip = NULL;

        }
        setCursor(Qt::ArrowCursor);
        if (event->buttons() != Qt::NoButton && event->modifiers() == Qt::NoModifier) {
            QGraphicsView::mouseMoveEvent(event);
            m_moveOpMode = SEEK;
            setCursorPos(mappedXPos);
            slotCheckPositionScrolling();
            return;
        } else m_moveOpMode = NONE;
    }
    QGraphicsView::mouseMoveEvent(event);
}

// virtual
void CustomTrackView::mousePressEvent(QMouseEvent * event)
{
    //kDebug() << "mousePressEvent STARTED";
    setFocus(Qt::MouseFocusReason);
    m_menuPosition = QPoint();

    // special cases (middle click button or ctrl / shift click
    if (event->button() == Qt::MidButton) {
        m_document->renderer()->switchPlay();
        m_blockRefresh = false;
        m_operationMode = NONE;
        return;
    }

    if (event->modifiers() & Qt::ShiftModifier) {
        setDragMode(QGraphicsView::RubberBandDrag);
        if (!(event->modifiers() & Qt::ControlModifier)) {
            resetSelectionGroup();
            scene()->clearSelection();
        }
        QGraphicsView::mousePressEvent(event);
        m_blockRefresh = false;
        m_operationMode = RUBBERSELECTION;
        return;
    }

    m_blockRefresh = true;
    m_dragItem = NULL;
    m_dragGuide = NULL;
    bool collision = false;

    if (m_tool != RAZORTOOL) activateMonitor();
    else if (m_document->renderer()->playSpeed() != 0.0) {
        m_document->renderer()->pause();
        return;
    }
    m_clickEvent = event->pos();

    // check item under mouse
    QList<QGraphicsItem *> collisionList = items(m_clickEvent);

    if (event->modifiers() == Qt::ControlModifier && m_tool != SPACERTOOL && collisionList.count() == 0) {
        setDragMode(QGraphicsView::ScrollHandDrag);
        QGraphicsView::mousePressEvent(event);
        m_blockRefresh = false;
        m_operationMode = NONE;
        return;
    }

    // if a guide and a clip were pressed, just select the guide
    for (int i = 0; i < collisionList.count(); ++i) {
        if (collisionList.at(i)->type() == GUIDEITEM) {
            // a guide item was pressed
            m_dragGuide = (Guide *) collisionList.at(i);
            if (event->button() == Qt::LeftButton) { // move it
                m_dragGuide->setFlag(QGraphicsItem::ItemIsMovable, true);
                collision = true;
                m_operationMode = MOVEGUIDE;
                // deselect all clips so that only the guide will move
                m_scene->clearSelection();
                resetSelectionGroup(false);
                updateSnapPoints(NULL);
                QGraphicsView::mousePressEvent(event);
                return;
            } else // show context menu
                break;
        }
    }

    // Find first clip, transition or group under mouse (when no guides selected)
    int ct = 0;
    AbstractGroupItem *dragGroup = NULL;
    while (!m_dragGuide && ct < collisionList.count()) {
        if (collisionList.at(ct)->type() == AVWIDGET || collisionList.at(ct)->type() == TRANSITIONWIDGET) {
            m_dragItem = static_cast <AbstractClipItem *>(collisionList.at(ct));
            m_dragItemInfo = m_dragItem->info();
            if (m_dragItem->parentItem() && m_dragItem->parentItem()->type() == GROUPWIDGET && m_dragItem->parentItem() != m_selectionGroup) {
                // kDebug()<<"// KLIK FOUND GRP: "<<m_dragItem->sceneBoundingRect();
                dragGroup = static_cast <AbstractGroupItem *>(m_dragItem->parentItem());
            }
            break;
        }
        ct++;
    }

    if (m_dragItem && m_dragItem->type() == TRANSITIONWIDGET) {
        // update transition menu action
        m_autoTransition->setChecked(static_cast<Transition *>(m_dragItem)->isAutomatic());
        m_autoTransition->setEnabled(true);
    } else m_autoTransition->setEnabled(false);

    // context menu requested
    if (event->button() == Qt::RightButton) {
        if (m_dragItem) {
            if (dragGroup) dragGroup->setSelected(true);
            else if (!m_dragItem->isSelected()) {
                resetSelectionGroup(false);
                m_scene->clearSelection();
                m_dragItem->setSelected(true);
            }
        } else if (!m_dragGuide) {
            // check if there is a guide close to mouse click
            QList<QGraphicsItem *> guidesCollisionList = items(event->pos().x() - 5, event->pos().y(), 10, 2); // a rect of height < 2 does not always collide with the guide
            for (int i = 0; i < guidesCollisionList.count(); i++) {
                if (guidesCollisionList.at(i)->type() == GUIDEITEM) {
                    m_dragGuide = static_cast <Guide *>(guidesCollisionList.at(i));
                    break;
                }
            }
            // keep this to support multiple guides context menu in the future (?)
            /*if (guidesCollisionList.at(0)->type() != GUIDEITEM)
                guidesCollisionList.removeAt(0);
            }
            if (!guidesCollisionList.isEmpty())
            m_dragGuide = static_cast <Guide *>(guidesCollisionList.at(0));*/
        }

        m_operationMode = NONE;
        displayContextMenu(event->globalPos(), m_dragItem, dragGroup);
        m_menuPosition = m_clickEvent;
        m_dragItem = NULL;
        event->accept();
        return;
    }

    // No item under click
    if (m_dragItem == NULL || m_tool == SPACERTOOL) {
        resetSelectionGroup(false);
        setCursor(Qt::ArrowCursor);
        m_scene->clearSelection();
        //event->accept();
        emit clipItemSelected(NULL);
        updateClipTypeActions(NULL);
        if (m_tool == SPACERTOOL) {
            QList<QGraphicsItem *> selection;
            if (event->modifiers() == Qt::ControlModifier) {
                // Ctrl + click, select all items on track after click position
                int track = (int)(mapToScene(m_clickEvent).y() / m_tracksHeight);
                selection = items(m_clickEvent.x(), track * m_tracksHeight + m_tracksHeight / 2, mapFromScene(sceneRect().width(), 0).x() - m_clickEvent.x(), m_tracksHeight / 2 - 2);

                kDebug() << "SPACER TOOL + CTRL, SELECTING ALL CLIPS ON TRACK " << track << " WITH SELECTION RECT " << m_clickEvent.x() << "/" <<  track * m_tracksHeight + 1 << "; " << mapFromScene(sceneRect().width(), 0).x() - m_clickEvent.x() << "/" << m_tracksHeight - 2;
            } else {
                // Select all items on all tracks after click position
                selection = items(event->pos().x(), 1, mapFromScene(sceneRect().width(), 0).x() - event->pos().x(), sceneRect().height());
                kDebug() << "SELELCTING ELEMENTS WITHIN =" << event->pos().x() << "/" <<  1 << ", " << mapFromScene(sceneRect().width(), 0).x() - event->pos().x() << "/" << sceneRect().height();
            }

            QList <GenTime> offsetList;
            for (int i = 0; i < selection.count(); i++) {
                if (selection.at(i)->type() == AVWIDGET || selection.at(i)->type() == TRANSITIONWIDGET) {
                    AbstractClipItem *item = static_cast<AbstractClipItem *>(selection.at(i));
                    offsetList.append(item->startPos());
                    offsetList.append(item->endPos());
                    selection.at(i)->setSelected(true);
                }
                if (selection.at(i)->type() == GROUPWIDGET) {
                    QList<QGraphicsItem *> children = selection.at(i)->childItems();
                    for (int j = 0; j < children.count(); j++) {
                        AbstractClipItem *item = static_cast<AbstractClipItem *>(children.at(j));
                        offsetList.append(item->startPos());
                        offsetList.append(item->endPos());
                    }
                    selection.at(i)->setSelected(true);
                }
            }

            if (!offsetList.isEmpty()) {
                qSort(offsetList);
                QList <GenTime> cleandOffsetList;
                GenTime startOffset = offsetList.takeFirst();
                for (int k = 0; k < offsetList.size(); k++) {
                    GenTime newoffset = offsetList.at(k) - startOffset;
                    if (newoffset != GenTime() && !cleandOffsetList.contains(newoffset)) {
                        cleandOffsetList.append(newoffset);
                    }
                }
                updateSnapPoints(NULL, cleandOffsetList, true);
            }
            groupSelectedItems(true);
            m_operationMode = SPACER;
        } else setCursorPos((int)(mapToScene(event->x(), 0).x()));
        QGraphicsView::mousePressEvent(event);
        kDebug() << "END mousePress EVENT ";
        return;
    }

    // Razor tool
    if (m_tool == RAZORTOOL && m_dragItem) {
        if (m_dragItem->type() == TRANSITIONWIDGET) {
            emit displayMessage(i18n("Cannot cut a transition"), ErrorMessage);
            event->accept();
            m_dragItem = NULL;
            return;
        } else if (m_dragItem->parentItem() && m_dragItem->parentItem() != m_selectionGroup) {
            emit displayMessage(i18n("Cannot cut a clip in a group"), ErrorMessage);
            event->accept();
            m_dragItem = NULL;
            return;
        }
        AbstractClipItem *clip = static_cast <AbstractClipItem *>(m_dragItem);
        RazorClipCommand* command = new RazorClipCommand(this, clip->info(), GenTime((int)(mapToScene(event->pos()).x()), m_document->fps()));
        m_document->renderer()->pause();
        m_commandStack->push(command);
        setDocumentModified();
        m_dragItem = NULL;
        event->accept();
        return;
    }

    if (dragGroup == NULL) updateSnapPoints(m_dragItem);
    else {
        QList <GenTime> offsetList;
        QList<QGraphicsItem *> children = dragGroup->childItems();
        for (int i = 0; i < children.count(); i++) {
            if (children.at(i)->type() == AVWIDGET || children.at(i)->type() == TRANSITIONWIDGET) {
                AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
                offsetList.append(item->startPos());
                offsetList.append(item->endPos());
            }
        }
        if (!offsetList.isEmpty()) {
            qSort(offsetList);
            GenTime startOffset = offsetList.takeFirst();
            QList <GenTime> cleandOffsetList;
            for (int k = 0; k < offsetList.size(); k++) {
                GenTime newoffset = offsetList.at(k) - startOffset;
                if (newoffset != GenTime() && !cleandOffsetList.contains(newoffset)) {
                    cleandOffsetList.append(newoffset);
                }
            }
            updateSnapPoints(NULL, cleandOffsetList, true);
        }
    }

    if (m_dragItem->type() == AVWIDGET && !m_dragItem->isItemLocked()) emit clipItemSelected((ClipItem*) m_dragItem);
    else emit clipItemSelected(NULL);

    bool itemSelected = false;
    if (m_dragItem->isSelected()) itemSelected = true;
    else if (m_dragItem->parentItem() && m_dragItem->parentItem()->isSelected()) itemSelected = true;
    else if (dragGroup && dragGroup->isSelected()) itemSelected = true;
    if (event->modifiers() == Qt::ControlModifier || itemSelected == false) {
        resetSelectionGroup();
        if (event->modifiers() != Qt::ControlModifier) m_scene->clearSelection();
        dragGroup = NULL;
        if (m_dragItem->parentItem() && m_dragItem->parentItem()->type() == GROUPWIDGET) {
            //kDebug()<<"// KLIK FOUND GRP: "<<m_dragItem->sceneBoundingRect();
            dragGroup = static_cast <AbstractGroupItem *>(m_dragItem->parentItem());
        }
        bool selected = !m_dragItem->isSelected();
        if (dragGroup) dragGroup->setSelected(selected);
        else m_dragItem->setSelected(selected);

        groupSelectedItems();
        ClipItem *clip = static_cast <ClipItem *>(m_dragItem);
        updateClipTypeActions(dragGroup == NULL ? clip : NULL);
        m_pasteEffectsAction->setEnabled(m_copiedItems.count() == 1);
    }

    // If clicked item is selected, allow move
    if (event->modifiers() != Qt::ControlModifier && m_operationMode == NONE/* && (m_dragItem->isSelected() || (dragGroup && dragGroup->isSelected()))*/) QGraphicsView::mousePressEvent(event);

    m_clickPoint = QPoint((int)(mapToScene(event->pos()).x() - m_dragItem->startPos().frames(m_document->fps())), (int)(event->pos().y() - m_dragItem->pos().y()));
    m_operationMode = m_dragItem->operationMode(mapToScene(event->pos()));

    if (m_operationMode == KEYFRAME) {
        m_dragItem->updateSelectedKeyFrame();
        m_blockRefresh = false;
        return;
    } else if (m_operationMode == MOVE) {
        setCursor(Qt::ClosedHandCursor);
    } else if (m_operationMode == TRANSITIONSTART && event->modifiers() != Qt::ControlModifier) {
        ItemInfo info;
        info.startPos = m_dragItem->startPos();
        info.track = m_dragItem->track();
        int transitiontrack = getPreviousVideoTrack(info.track);
        ClipItem *transitionClip = NULL;
        if (transitiontrack != 0) transitionClip = getClipItemAt((int) info.startPos.frames(m_document->fps()), m_document->tracksCount() - transitiontrack);
        if (transitionClip && transitionClip->endPos() < m_dragItem->endPos()) {
            info.endPos = transitionClip->endPos();
        } else info.endPos = info.startPos + GenTime(65, m_document->fps());
        if (info.endPos == info.startPos) info.endPos = info.startPos + GenTime(65, m_document->fps());
        // Check there is no other transition at that place
        double startY = info.track * m_tracksHeight + 1 + m_tracksHeight / 2;
        QRectF r(info.startPos.frames(m_document->fps()), startY, (info.endPos - info.startPos).frames(m_document->fps()), m_tracksHeight / 2);
        QList<QGraphicsItem *> selection = m_scene->items(r);
        bool transitionAccepted = true;
        for (int i = 0; i < selection.count(); i++) {
            if (selection.at(i)->type() == TRANSITIONWIDGET) {
                Transition *tr = static_cast <Transition *>(selection.at(i));
                if (tr->startPos() - info.startPos > GenTime(5, m_document->fps())) {
                    if (tr->startPos() < info.endPos) info.endPos = tr->startPos();
                } else transitionAccepted = false;
            }
        }
        if (transitionAccepted) slotAddTransition((ClipItem *) m_dragItem, info, transitiontrack);
        else emit displayMessage(i18n("Cannot add transition"), ErrorMessage);
    } else if (m_operationMode == TRANSITIONEND && event->modifiers() != Qt::ControlModifier) {
        ItemInfo info;
        info.endPos = GenTime(m_dragItem->endPos().frames(m_document->fps()), m_document->fps());
        info.track = m_dragItem->track();
        int transitiontrack = getPreviousVideoTrack(info.track);
        ClipItem *transitionClip = NULL;
        if (transitiontrack != 0) transitionClip = getClipItemAt((int) info.endPos.frames(m_document->fps()), m_document->tracksCount() - transitiontrack);
        if (transitionClip && transitionClip->startPos() > m_dragItem->startPos()) {
            info.startPos = transitionClip->startPos();
        } else info.startPos = info.endPos - GenTime(65, m_document->fps());
        if (info.endPos == info.startPos) info.startPos = info.endPos - GenTime(65, m_document->fps());
        QDomElement transition = MainWindow::transitions.getEffectByTag("luma", "dissolve").cloneNode().toElement();
        EffectsList::setParameter(transition, "reverse", "1");

        // Check there is no other transition at that place
        double startY = info.track * m_tracksHeight + 1 + m_tracksHeight / 2;
        QRectF r(info.startPos.frames(m_document->fps()), startY, (info.endPos - info.startPos).frames(m_document->fps()), m_tracksHeight / 2);
        QList<QGraphicsItem *> selection = m_scene->items(r);
        bool transitionAccepted = true;
        for (int i = 0; i < selection.count(); i++) {
            if (selection.at(i)->type() == TRANSITIONWIDGET) {
                Transition *tr = static_cast <Transition *>(selection.at(i));
                if (info.endPos - tr->endPos() > GenTime(5, m_document->fps())) {
                    if (tr->endPos() > info.startPos) info.startPos = tr->endPos();
                } else transitionAccepted = false;
            }
        }
        if (transitionAccepted) slotAddTransition((ClipItem *) m_dragItem, info, transitiontrack, transition);
        else emit displayMessage(i18n("Cannot add transition"), ErrorMessage);

    } else if ((m_operationMode == RESIZESTART || m_operationMode == RESIZEEND) && m_selectionGroup) {
        resetSelectionGroup(false);
        m_dragItem->setSelected(true);
    }

    m_blockRefresh = false;
    //kDebug()<<pos;
    //QGraphicsView::mousePressEvent(event);
}

void CustomTrackView::resetSelectionGroup(bool selectItems)
{
    if (m_selectionGroup) {
        // delete selection group
        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);

        QList<QGraphicsItem *> children = m_selectionGroup->childItems();
        scene()->destroyItemGroup(m_selectionGroup);
        for (int i = 0; i < children.count(); i++) {
            if (children.at(i)->type() == AVWIDGET || children.at(i)->type() == TRANSITIONWIDGET) {
                if (!static_cast <AbstractClipItem *>(children.at(i))->isItemLocked()) {
                    children.at(i)->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
                    children.at(i)->setSelected(selectItems);
                }
            } else if (children.at(i)->type() == GROUPWIDGET) {
                children.at(i)->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
                children.at(i)->setSelected(selectItems);
            }
        }
        m_selectionGroup = NULL;
        KdenliveSettings::setSnaptopoints(snap);
    }
}

void CustomTrackView::groupSelectedItems(bool force, bool createNewGroup)
{
    if (m_selectionGroup) {
        kDebug() << "///// ERROR, TRYING TO OVERRIDE EXISTING GROUP";
        return;
    }
    QList<QGraphicsItem *> selection = m_scene->selectedItems();
    if (selection.isEmpty()) return;
    QPointF top = selection.at(0)->sceneBoundingRect().topLeft();
    // Find top left position of selection
    for (int i = 1; i < selection.count(); i++) {
        if (selection.at(i)->type() == AVWIDGET || selection.at(i)->type() == TRANSITIONWIDGET || selection.at(i)->type() == GROUPWIDGET) {
            QPointF currenttop = selection.at(i)->sceneBoundingRect().topLeft();
            if (currenttop.x() < top.x()) top.setX(currenttop.x());
            if (currenttop.y() < top.y()) top.setY(currenttop.y());
        }
    }

    if (force || selection.count() > 1) {
        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);
        if (createNewGroup) {
            AbstractGroupItem *newGroup = m_document->clipManager()->createGroup();
            newGroup->translate(-top.x(), -top.y() + 1);
            newGroup->setPos(top.x(), top.y() - 1);
            scene()->addItem(newGroup);

            // CHeck if we are trying to include a group in a group
            QList <AbstractGroupItem *> groups;
            for (int i = 0; i < selection.count(); i++) {
                if (selection.at(i)->type() == GROUPWIDGET && !groups.contains(static_cast<AbstractGroupItem *>(selection.at(i)))) {
                    groups.append(static_cast<AbstractGroupItem *>(selection.at(i)));
                } else if (selection.at(i)->parentItem() && !groups.contains(static_cast<AbstractGroupItem *>(selection.at(i)->parentItem()))) groups.append(static_cast<AbstractGroupItem *>(selection.at(i)->parentItem()));
            }
            if (!groups.isEmpty()) {
                // ungroup previous groups
                while (!groups.isEmpty()) {
                    AbstractGroupItem *grp = groups.takeFirst();
                    m_document->clipManager()->removeGroup(grp);
                    scene()->destroyItemGroup(grp);
                }
                selection = m_scene->selectedItems();
            }

            for (int i = 0; i < selection.count(); i++) {
                if (selection.at(i)->type() == AVWIDGET || selection.at(i)->type() == TRANSITIONWIDGET) {
                    newGroup->addToGroup(selection.at(i));
                    selection.at(i)->setFlags(QGraphicsItem::ItemIsSelectable);
                }
            }
            KdenliveSettings::setSnaptopoints(snap);
        } else {
            m_selectionGroup = new AbstractGroupItem(m_document->fps());
            m_selectionGroup->translate(-top.x(), -top.y() + 1);
            m_selectionGroup->setPos(top.x(), top.y() - 1);
            scene()->addItem(m_selectionGroup);
            for (int i = 0; i < selection.count(); i++) {
                if (selection.at(i)->parentItem() == NULL && (selection.at(i)->type() == AVWIDGET || selection.at(i)->type() == TRANSITIONWIDGET || selection.at(i)->type() == GROUPWIDGET)) {
                    m_selectionGroup->addToGroup(selection.at(i));
                    selection.at(i)->setFlags(QGraphicsItem::ItemIsSelectable);
                }
            }
            KdenliveSettings::setSnaptopoints(snap);
            if (m_selectionGroup) {
                m_selectionGroupInfo.startPos = GenTime(m_selectionGroup->scenePos().x(), m_document->fps());
                m_selectionGroupInfo.track = m_selectionGroup->track();
            }
        }
    } else resetSelectionGroup();
}

void CustomTrackView::mouseDoubleClickEvent(QMouseEvent *event)
{
    kDebug() << "++++++++++++ DBL CLK";
    if (m_dragItem && m_dragItem->hasKeyFrames()) {
        if (m_moveOpMode == KEYFRAME) {
            // user double clicked on a keyframe, open edit dialog
            QDialog d(parentWidget());
            Ui::KeyFrameDialog_UI view;
            view.setupUi(&d);
            view.kfr_position->setText(m_document->timecode().getTimecode(GenTime(m_dragItem->selectedKeyFramePos(), m_document->fps()) - m_dragItem->cropStart(), m_document->fps()));
            view.kfr_value->setValue(m_dragItem->selectedKeyFrameValue());
            view.kfr_value->setFocus();
            if (d.exec() == QDialog::Accepted) {
                int pos = m_document->timecode().getFrameCount(view.kfr_position->text(), m_document->fps());
                m_dragItem->updateKeyFramePos(GenTime(pos, m_document->fps()) + m_dragItem->cropStart(), (double) view.kfr_value->value() * m_dragItem->keyFrameFactor());
                ClipItem *item = (ClipItem *)m_dragItem;
                QString previous = item->keyframes(item->selectedEffectIndex());
                item->updateKeyframeEffect();
                QString next = item->keyframes(item->selectedEffectIndex());
                EditKeyFrameCommand *command = new EditKeyFrameCommand(this, item->track(), item->startPos(), item->selectedEffectIndex(), previous, next, false);
                m_commandStack->push(command);
                updateEffect(m_document->tracksCount() - item->track(), item->startPos(), item->selectedEffect(), item->selectedEffectIndex());
                emit clipItemSelected(item, item->selectedEffectIndex());
            }

        } else  {
            // add keyframe
            GenTime keyFramePos = GenTime((int)(mapToScene(event->pos()).x()), m_document->fps()) - m_dragItem->startPos() + m_dragItem->cropStart();
            m_dragItem->addKeyFrame(keyFramePos, mapToScene(event->pos()).toPoint().y());
            ClipItem * item = (ClipItem *) m_dragItem;
            QString previous = item->keyframes(item->selectedEffectIndex());
            item->updateKeyframeEffect();
            QString next = item->keyframes(item->selectedEffectIndex());
            EditKeyFrameCommand *command = new EditKeyFrameCommand(this, m_dragItem->track(), m_dragItem->startPos(), item->selectedEffectIndex(), previous, next, false);
            m_commandStack->push(command);
            updateEffect(m_document->tracksCount() - item->track(), item->startPos(), item->selectedEffect(), item->selectedEffectIndex());
            emit clipItemSelected(item, item->selectedEffectIndex());
        }
    } else if (m_dragItem && !m_dragItem->isItemLocked()) {
        ClipDurationDialog d(m_dragItem, m_document->timecode(), this);
        GenTime minimum;
        GenTime maximum;
        if (m_dragItem->type() == TRANSITIONWIDGET) {
            getTransitionAvailableSpace(m_dragItem, minimum, maximum);
        } else {
            getClipAvailableSpace(m_dragItem, minimum, maximum);
        }
        //kDebug()<<"// GOT MOVE POS: "<<minimum.frames(25)<<" - "<<maximum.frames(25);
        d.setMargins(minimum, maximum);
        if (d.exec() == QDialog::Accepted) {
            if (m_dragItem->type() == TRANSITIONWIDGET) {
                // move & resize transition
                ItemInfo startInfo;
                startInfo.startPos = m_dragItem->startPos();
                startInfo.endPos = m_dragItem->endPos();
                startInfo.track = m_dragItem->track();
                ItemInfo endInfo;
                endInfo.startPos = d.startPos();
                endInfo.endPos = endInfo.startPos + d.duration();
                endInfo.track = m_dragItem->track();
                MoveTransitionCommand *command = new MoveTransitionCommand(this, startInfo, endInfo, true);
                m_commandStack->push(command);
            } else {
                // move and resize clip
                QUndoCommand *moveCommand = new QUndoCommand();
                moveCommand->setText(i18n("Edit clip"));
                ItemInfo clipInfo = m_dragItem->info();
                if (d.duration() < m_dragItem->cropDuration() || d.cropStart() != clipInfo.cropStart) {
                    // duration was reduced, so process it first
                    ItemInfo startInfo = clipInfo;
                    clipInfo.endPos = clipInfo.startPos + d.duration();
                    clipInfo.cropStart = d.cropStart();
                    new ResizeClipCommand(this, startInfo, clipInfo, true, moveCommand);
                }
                if (d.startPos() != clipInfo.startPos) {
                    ItemInfo startInfo = clipInfo;
                    clipInfo.startPos = d.startPos();
                    clipInfo.endPos = m_dragItem->endPos() + (clipInfo.startPos - startInfo.startPos);
                    new MoveClipCommand(this, startInfo, clipInfo, true, moveCommand);
                }
                if (d.duration() > m_dragItem->cropDuration()) {
                    // duration was increased, so process it after move
                    ItemInfo startInfo = clipInfo;
                    clipInfo.endPos = clipInfo.startPos + d.duration();
                    clipInfo.cropStart = d.cropStart();
                    new ResizeClipCommand(this, startInfo, clipInfo, true, moveCommand);
                }
                m_commandStack->push(moveCommand);
            }
        }
    } else {
        QList<QGraphicsItem *> collisionList = items(event->pos());
        if (collisionList.count() == 1 && collisionList.at(0)->type() == GUIDEITEM) {
            Guide *editGuide = (Guide *) collisionList.at(0);
            if (editGuide) slotEditGuide(editGuide->info());
        }
    }
}


void CustomTrackView::editKeyFrame(const GenTime pos, const int track, const int index, const QString keyframes)
{
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()), track);
    if (clip) {
        clip->setKeyframes(index, keyframes);
        updateEffect(m_document->tracksCount() - clip->track(), clip->startPos(), clip->effectAt(index), index);
    } else emit displayMessage(i18n("Cannot find clip with keyframe"), ErrorMessage);
}


void CustomTrackView::displayContextMenu(QPoint pos, AbstractClipItem *clip, AbstractGroupItem *group)
{
    m_deleteGuide->setEnabled(m_dragGuide != NULL);
    m_editGuide->setEnabled(m_dragGuide != NULL);
    if (clip == NULL) m_timelineContextMenu->popup(pos);
    else if (group != NULL) {
        m_pasteEffectsAction->setEnabled(m_copiedItems.count() == 1);
        m_ungroupAction->setEnabled(true);
        updateClipTypeActions(NULL);
        m_timelineContextClipMenu->popup(pos);
    } else {
        m_ungroupAction->setEnabled(false);
        if (clip->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem*>(clip);
            updateClipTypeActions(item);
            m_pasteEffectsAction->setEnabled(m_copiedItems.count() == 1);
            m_timelineContextClipMenu->popup(pos);
        } else if (clip->type() == TRANSITIONWIDGET) m_timelineContextTransitionMenu->popup(pos);
    }
}

void CustomTrackView::activateMonitor()
{
    emit activateDocumentMonitor();
}

void CustomTrackView::dragEnterEvent(QDragEnterEvent * event)
{
    if (event->mimeData()->hasFormat("kdenlive/clip")) {
        m_clipDrag = true;
        resetSelectionGroup();
        QStringList list = QString(event->mimeData()->data("kdenlive/clip")).split(';');
        m_selectionGroup = new AbstractGroupItem(m_document->fps());
        QPoint pos;
        DocClipBase *clip = m_document->getBaseClip(list.at(0));
        if (clip == NULL) kDebug() << " WARNING))))))))) CLIP NOT FOUND : " << list.at(0);
        ItemInfo info;
        info.startPos = GenTime();
        info.cropStart = GenTime(list.at(1).toInt(), m_document->fps());
        info.endPos = GenTime(list.at(2).toInt() - list.at(1).toInt(), m_document->fps());
        info.track = (int)(1 / m_tracksHeight);
        ClipItem *item = new ClipItem(clip, info, m_document->fps(), 1.0, 1);
        m_selectionGroup->addToGroup(item);
        item->setFlags(QGraphicsItem::ItemIsSelectable);
        //TODO: check if we do not overlap another clip when first dropping in timeline
        // if (insertPossible(m_selectionGroup, event->pos()))
        QList <GenTime> offsetList;
        offsetList.append(info.endPos);
        updateSnapPoints(NULL, offsetList);
        scene()->addItem(m_selectionGroup);
        event->acceptProposedAction();
    } else if (event->mimeData()->hasFormat("kdenlive/producerslist")) {
        m_clipDrag = true;
        QStringList ids = QString(event->mimeData()->data("kdenlive/producerslist")).split(';');
        m_scene->clearSelection();
        resetSelectionGroup(false);

        m_selectionGroup = new AbstractGroupItem(m_document->fps());
        QPoint pos;
        GenTime start;
        QList <GenTime> offsetList;
        for (int i = 0; i < ids.size(); ++i) {
            DocClipBase *clip = m_document->getBaseClip(ids.at(i));
            if (clip == NULL) kDebug() << " WARNING))))))))) CLIP NOT FOUND : " << ids.at(i);
            ItemInfo info;
            info.startPos = start;
            info.endPos = info.startPos + clip->duration();
            info.track = (int)(1 / m_tracksHeight);
            ClipItem *item = new ClipItem(clip, info, m_document->fps(), 1.0, 1, false);
            start += clip->duration();
            offsetList.append(start);
            m_selectionGroup->addToGroup(item);
            item->setFlags(QGraphicsItem::ItemIsSelectable);
            m_waitingThumbs.append(item);
        }
        //TODO: check if we do not overlap another clip when first dropping in timeline
        //if (insertPossible(m_selectionGroup, event->pos()))
        updateSnapPoints(NULL, offsetList);
        scene()->addItem(m_selectionGroup);
        m_thumbsTimer.start();
        event->acceptProposedAction();

    } else {
        // the drag is not a clip (may be effect, ...)
        m_clipDrag = false;
        QGraphicsView::dragEnterEvent(event);
    }
}


bool CustomTrackView::insertPossible(AbstractGroupItem *group, const QPoint &pos) const
{
    QPolygonF path;
    QList<QGraphicsItem *> children = group->childItems();
    for (int i = 0; i < children.count(); i++) {
        if (children.at(i)->type() == AVWIDGET) {
            ClipItem *clip = static_cast <ClipItem *>(children.at(i));
            ItemInfo info = clip->info();
            kDebug() << " / / INSERT : " << pos.x();
            QRectF shape = QRectF(clip->startPos().frames(m_document->fps()), clip->track() * m_tracksHeight + 1, clip->cropDuration().frames(m_document->fps()) - 0.02, m_tracksHeight - 1);
            kDebug() << " / / INSERT RECT: " << shape;
            path = path.united(QPolygonF(shape));
        }
    }

    QList<QGraphicsItem*> collindingItems = scene()->items(path, Qt::IntersectsItemShape);
    if (collindingItems.isEmpty()) return true;
    else {
        for (int i = 0; i < collindingItems.count(); i++) {
            QGraphicsItem *collision = collindingItems.at(i);
            if (collision->type() == AVWIDGET) {
                // Collision
                kDebug() << "// COLLISIION DETECTED";
                return false;
            }
        }
        return true;
    }

}


bool CustomTrackView::itemCollision(AbstractClipItem *item, ItemInfo newPos)
{
    QRectF shape = QRectF(newPos.startPos.frames(m_document->fps()), newPos.track * m_tracksHeight + 1, (newPos.endPos - newPos.startPos).frames(m_document->fps()) - 0.02, m_tracksHeight - 1);
    QList<QGraphicsItem*> collindingItems = scene()->items(shape, Qt::IntersectsItemShape);
    collindingItems.removeAll(item);
    if (collindingItems.isEmpty()) return false;
    else {
        for (int i = 0; i < collindingItems.count(); i++) {
            QGraphicsItem *collision = collindingItems.at(i);
            if (collision->type() == item->type()) {
                // Collision
                kDebug() << "// COLLISIION DETECTED";
                return true;
            }
        }
        return false;
    }
}

void CustomTrackView::slotRefreshEffects(ClipItem *clip)
{
    int track = m_document->tracksCount() - clip->track();
    GenTime pos = clip->startPos();
    if (!m_document->renderer()->mltRemoveEffect(track, pos, "-1", false, false)) {
        emit displayMessage(i18n("Problem deleting effect"), ErrorMessage);
        return;
    }
    bool success = true;
    for (int i = 0; i < clip->effectsCount(); i++) {
        if (!m_document->renderer()->mltAddEffect(track, pos, clip->getEffectArgs(clip->effectAt(i)), false)) success = false;
    }
    if (!success) emit displayMessage(i18n("Problem adding effect to clip"), ErrorMessage);
    m_document->renderer()->doRefresh();
}

void CustomTrackView::addEffect(int track, GenTime pos, QDomElement effect)
{
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, m_document->tracksCount() - track);
    if (clip) {
        // Special case: speed effect
        if (effect.attribute("id") == "speed") {
            if (clip->clipType() != VIDEO && clip->clipType() != AV && clip->clipType() != PLAYLIST) {
                emit displayMessage(i18n("Problem adding effect to clip"), ErrorMessage);
                return;
            }
            ItemInfo info = clip->info();
            double speed = EffectsList::parameter(effect, "speed").toDouble() / 100.0;
            int strobe = EffectsList::parameter(effect, "strobe").toInt();
            if (strobe == 0) strobe = 1;
            doChangeClipSpeed(info, speed, 1.0, strobe, clip->baseClip()->getId());
            clip->addEffect(effect);
            emit clipItemSelected(clip);
            return;
        }

        if (!m_document->renderer()->mltAddEffect(track, pos, clip->addEffect(effect)))
            emit displayMessage(i18n("Problem adding effect to clip"), ErrorMessage);
        emit clipItemSelected(clip);
    } else emit displayMessage(i18n("Cannot find clip to add effect"), ErrorMessage);
}

void CustomTrackView::deleteEffect(int track, GenTime pos, QDomElement effect)
{
    QString index = effect.attribute("kdenlive_ix");
    // Special case: speed effect
    if (effect.attribute("id") == "speed") {
        ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, m_document->tracksCount() - track);
        if (clip) {
            ItemInfo info = clip->info();
            doChangeClipSpeed(info, 1.0, clip->speed(), 1, clip->baseClip()->getId());
            clip->deleteEffect(index);
            emit clipItemSelected(clip);
            return;
        }
    }
    if (!m_document->renderer()->mltRemoveEffect(track, pos, index, true) && effect.attribute("disabled") != "1") {
        kDebug() << "// ERROR REMOV EFFECT: " << index << ", DISABLE: " << effect.attribute("disabled");
        emit displayMessage(i18n("Problem deleting effect"), ErrorMessage);
        return;
    }
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, m_document->tracksCount() - track);
    if (clip) {
        clip->deleteEffect(index);
        emit clipItemSelected(clip);
    }
}

void CustomTrackView::slotAddGroupEffect(QDomElement effect, AbstractGroupItem *group)
{
    QList<QGraphicsItem *> itemList = group->childItems();
    QUndoCommand *effectCommand = new QUndoCommand();
    QString effectName;
    QDomNode namenode = effect.elementsByTagName("name").item(0);
    if (!namenode.isNull()) effectName = i18n(namenode.toElement().text().toUtf8().data());
    else effectName = i18n("effect");
    effectCommand->setText(i18n("Add %1", effectName));
    int count = 0;
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            if (effect.attribute("type") == "audio") {
                // Don't add audio effects on video clips
                if (item->isVideoOnly() || (item->clipType() != AUDIO && item->clipType() != AV && item->clipType() != PLAYLIST)) continue;
            } else if (effect.hasAttribute("type") == false) {
                // Don't add video effect on audio clips
                if (item->isAudioOnly() || item->clipType() == AUDIO) continue;
            }

            if (item->hasEffect(effect.attribute("tag"), effect.attribute("id")) != -1 && effect.attribute("unique", "0") != "0") {
                emit displayMessage(i18n("Effect already present in clip"), ErrorMessage);
                continue;
            }
            if (item->isItemLocked()) {
                continue;
            }
            item->initEffect(effect);
            if (effect.attribute("tag") == "ladspa") {
                QString ladpsaFile = m_document->getLadspaFile();
                initEffects::ladspaEffectFile(ladpsaFile, effect.attribute("ladspaid").toInt(), getLadspaParams(effect));
                effect.setAttribute("src", ladpsaFile);
            }
            new AddEffectCommand(this, m_document->tracksCount() - item->track(), item->startPos(), effect, true, effectCommand);
            count++;
        }
    }
    if (count > 0) {
        m_commandStack->push(effectCommand);
        setDocumentModified();
    } else delete effectCommand;
}

void CustomTrackView::slotAddEffect(QDomElement effect, GenTime pos, int track)
{
    QList<QGraphicsItem *> itemList;
    QUndoCommand *effectCommand = new QUndoCommand();
    QString effectName;
    QDomNode namenode = effect.elementsByTagName("name").item(0);
    if (!namenode.isNull()) effectName = i18n(namenode.toElement().text().toUtf8().data());
    else effectName = i18n("effect");
    effectCommand->setText(i18n("Add %1", effectName));
    int count = 0;
    if (track == -1) itemList = scene()->selectedItems();
    if (itemList.isEmpty()) {
        ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, track);
        if (clip) itemList.append(clip);
        else emit displayMessage(i18n("Select a clip if you want to apply an effect"), ErrorMessage);
    }
    kDebug() << "// REQUESTING EFFECT ON CLIP: " << pos.frames(25) << ", TRK: " << track << "SELECTED ITEMS: " << itemList.count();
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = (ClipItem *)itemList.at(i);
            if (effect.attribute("type") == "audio") {
                // Don't add audio effects on video clips
                if (item->isVideoOnly() || (item->clipType() != AUDIO && item->clipType() != AV && item->clipType() != PLAYLIST)) {
                    emit displayMessage(i18n("Cannot add an audio effect to this clip"), ErrorMessage);
                    continue;
                }
            } else if (effect.hasAttribute("type") == false) {
                // Don't add video effect on audio clips
                if (item->isAudioOnly() || item->clipType() == AUDIO) {
                    emit displayMessage(i18n("Cannot add a video effect to this clip"), ErrorMessage);
                    continue;
                }
            }
            if (item->hasEffect(effect.attribute("tag"), effect.attribute("id")) != -1 && effect.attribute("unique", "0") != "0") {
                emit displayMessage(i18n("Effect already present in clip"), ErrorMessage);
                continue;
            }
            if (item->isItemLocked()) {
                continue;
            }
            item->initEffect(effect);
            if (effect.attribute("tag") == "ladspa") {
                QString ladpsaFile = m_document->getLadspaFile();
                initEffects::ladspaEffectFile(ladpsaFile, effect.attribute("ladspaid").toInt(), getLadspaParams(effect));
                effect.setAttribute("src", ladpsaFile);
            }
            new AddEffectCommand(this, m_document->tracksCount() - item->track(), item->startPos(), effect, true, effectCommand);
            count++;
        }
    }
    if (count > 0) {
        m_commandStack->push(effectCommand);
        setDocumentModified();
    } else delete effectCommand;
}

void CustomTrackView::slotDeleteEffect(ClipItem *clip, QDomElement effect)
{
    AddEffectCommand *command = new AddEffectCommand(this, m_document->tracksCount() - clip->track(), clip->startPos(), effect, false);
    m_commandStack->push(command);
    setDocumentModified();
}

void CustomTrackView::updateEffect(int track, GenTime pos, QDomElement effect, int ix, bool triggeredByUser)
{
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, m_document->tracksCount() - track);
    if (clip) {


        // Special case: speed effect
        if (effect.attribute("id") == "speed") {
            ItemInfo info = clip->info();
            double speed = EffectsList::parameter(effect, "speed").toDouble() / 100.0;
            int strobe = EffectsList::parameter(effect, "strobe").toInt();
            if (strobe == 0) strobe = 1;
            doChangeClipSpeed(info, speed, clip->speed(), strobe, clip->baseClip()->getId());
            clip->setEffectAt(ix, effect);
            if (ix == clip->selectedEffectIndex()) {
                clip->setSelectedEffect(ix);
            }
            return;
        }




        EffectsParameterList effectParams = clip->getEffectArgs(effect);
        if (effect.attribute("tag") == "ladspa") {
            // Update the ladspa affect file
            initEffects::ladspaEffectFile(effect.attribute("src"), effect.attribute("ladspaid").toInt(), getLadspaParams(effect));
        }
        // check if we are trying to reset a keyframe effect
        if (effectParams.hasParam("keyframes") && effectParams.paramValue("keyframes").isEmpty()) {
            clip->initEffect(effect);
            effectParams = clip->getEffectArgs(effect);
        }
        if (effectParams.paramValue("disabled") == "1") {
            if (m_document->renderer()->mltRemoveEffect(track, pos, effectParams.paramValue("kdenlive_ix"), false)) {
                kDebug() << "//////  DISABLING EFFECT: " << ix << ", CURRENTLA: " << clip->selectedEffectIndex();
            } else emit displayMessage(i18n("Problem deleting effect"), ErrorMessage);
        } else if (!m_document->renderer()->mltEditEffect(m_document->tracksCount() - clip->track(), clip->startPos(), effectParams))
            emit displayMessage(i18n("Problem editing effect"), ErrorMessage);

        clip->setEffectAt(ix, effect);
        if (ix == clip->selectedEffectIndex()) {
            clip->setSelectedEffect(ix);
            if (!triggeredByUser) emit clipItemSelected(clip, ix);
        }
        if (effect.attribute("tag") == "volume" || effect.attribute("tag") == "brightness") {
            // A fade effect was modified, update the clip
            if (effect.attribute("id") == "fadein" || effect.attribute("id") == "fade_from_black") {
                int pos = effectParams.paramValue("out").toInt() - effectParams.paramValue("in").toInt();
                clip->setFadeIn(pos);
            }
            if (effect.attribute("id") == "fadeout" || effect.attribute("id") == "fade_to_black") {
                int pos = effectParams.paramValue("out").toInt() - effectParams.paramValue("in").toInt();
                clip->setFadeOut(pos);
            }
        }
    }
    setDocumentModified();
}

void CustomTrackView::moveEffect(int track, GenTime pos, int oldPos, int newPos)
{
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()) + 1, m_document->tracksCount() - track);
    if (clip) {
        m_document->renderer()->mltMoveEffect(track, pos, oldPos, newPos);
        QDomElement act = clip->effectAt(newPos - 1).cloneNode().toElement();
        QDomElement before = clip->effectAt(oldPos - 1).cloneNode().toElement();
        clip->setEffectAt(oldPos - 1, act);
        clip->setEffectAt(newPos - 1, before);
        emit clipItemSelected(clip, newPos - 1);
    }
    setDocumentModified();
}

void CustomTrackView::slotChangeEffectState(ClipItem *clip, int effectPos, bool disable)
{
    QDomElement effect = clip->effectAt(effectPos);
    QDomElement oldEffect = effect.cloneNode().toElement();

    if (effect.attribute("id") == "speed") {
        if (clip) {
            ItemInfo info = clip->info();
            effect.setAttribute("disabled", disable);
            if (disable) doChangeClipSpeed(info, 1.0, clip->speed(), 1, clip->baseClip()->getId());
            else {
                double speed = EffectsList::parameter(effect, "speed").toDouble() / 100.0;
                int strobe = EffectsList::parameter(effect, "strobe").toInt();
                if (strobe == 0) strobe = 1;
                doChangeClipSpeed(info, speed, 1.0, strobe, clip->baseClip()->getId());
            }
            return;
        }
    }
    effect.setAttribute("disabled", disable);
    EditEffectCommand *command = new EditEffectCommand(this, m_document->tracksCount() - clip->track(), clip->startPos(), oldEffect, effect, effectPos, true);
    m_commandStack->push(command);
    setDocumentModified();;
}

void CustomTrackView::slotChangeEffectPosition(ClipItem *clip, int currentPos, int newPos)
{
    MoveEffectCommand *command = new MoveEffectCommand(this, m_document->tracksCount() - clip->track(), clip->startPos(), currentPos, newPos);
    m_commandStack->push(command);
    setDocumentModified();
}

void CustomTrackView::slotUpdateClipEffect(ClipItem *clip, QDomElement oldeffect, QDomElement effect, int ix)
{
    EditEffectCommand *command = new EditEffectCommand(this, m_document->tracksCount() - clip->track(), clip->startPos(), oldeffect, effect, ix, true);
    m_commandStack->push(command);
}

void CustomTrackView::cutClip(ItemInfo info, GenTime cutTime, bool cut)
{
    if (cut) {
        // cut clip
        ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()) + 1, info.track);
        if (!item || cutTime >= item->endPos() || cutTime <= item->startPos()) {
            emit displayMessage(i18n("Cannot find clip to cut"), ErrorMessage);
            kDebug() << "/////////  ERROR CUTTING CLIP : (" << item->startPos().frames(25) << "-" << item->endPos().frames(25) << "), INFO: (" << info.startPos.frames(25) << "-" << info.endPos.frames(25) << ")" << ", CUT: " << cutTime.frames(25);
            m_blockRefresh = false;
            return;
        }
        if (item->parentItem()) {
            // Item is part of a group, reset group
            resetSelectionGroup();
        }
        kDebug() << "/////////  CUTTING CLIP : (" << item->startPos().frames(25) << "-" << item->endPos().frames(25) << "), INFO: (" << info.startPos.frames(25) << "-" << info.endPos.frames(25) << ")" << ", CUT: " << cutTime.frames(25);

        m_document->renderer()->mltCutClip(m_document->tracksCount() - info.track, cutTime);
        int cutPos = (int) cutTime.frames(m_document->fps());
        ItemInfo newPos;
        double speed = item->speed();
        newPos.startPos = cutTime;
        newPos.endPos = info.endPos;
        if (speed == 1) newPos.cropStart = item->info().cropStart + (cutTime - info.startPos);
        else newPos.cropStart = item->info().cropStart + (cutTime - info.startPos) * speed;
        newPos.track = info.track;
        ClipItem *dup = item->clone(newPos);
        // remove unwanted effects (fade in) from 2nd part of cutted clip
        int ix = dup->hasEffect(QString(), "fadein");
        if (ix != -1) {
            QDomElement oldeffect = item->effectAt(ix);
            dup->deleteEffect(oldeffect.attribute("kdenlive_ix"));
        }
        ix = dup->hasEffect(QString(), "fade_from_black");
        if (ix != -1) {
            QDomElement oldeffect = item->effectAt(ix);
            dup->deleteEffect(oldeffect.attribute("kdenlive_ix"));
        }
        item->resizeEnd(cutPos, false);
        scene()->addItem(dup);
        if (item->checkKeyFrames()) slotRefreshEffects(item);
        if (dup->checkKeyFrames()) slotRefreshEffects(dup);
        item->baseClip()->addReference();
        m_document->updateClip(item->baseClip()->getId());
        setDocumentModified();
        kDebug() << "/////////  CUTTING CLIP RESULT: (" << item->startPos().frames(25) << "-" << item->endPos().frames(25) << "), DUP: (" << dup->startPos().frames(25) << "-" << dup->endPos().frames(25) << ")" << ", CUT: " << cutTime.frames(25);
        kDebug() << "//  CUTTING CLIP dONE";
    } else {
        // uncut clip

        ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()), info.track);
        ClipItem *dup = getClipItemAt((int) cutTime.frames(m_document->fps()) + 1, info.track);
        if (!item || !dup || item == dup) {
            emit displayMessage(i18n("Cannot find clip to uncut"), ErrorMessage);
            m_blockRefresh = false;
            return;
        }
        if (m_document->renderer()->mltRemoveClip(m_document->tracksCount() - info.track, cutTime) == false) {
            emit displayMessage(i18n("Error removing clip at %1 on track %2", m_document->timecode().getTimecodeFromFrames(cutTime.frames(m_document->fps())), info.track), ErrorMessage);
            return;
        }

        kDebug() << "// UNCUTTING CLIPS: ITEM 1 (" << item->startPos().frames(25) << "x" << item->endPos().frames(25) << ")";
        kDebug() << "// UNCUTTING CLIPS: ITEM 2 (" << dup->startPos().frames(25) << "x" << dup->endPos().frames(25) << ")";
        kDebug() << "// UNCUTTING CLIPS, INFO (" << info.startPos.frames(25) << "x" << info.endPos.frames(25) << ") , CUT: " << cutTime.frames(25);;
        //deleteClip(dup->info());


        if (dup->isSelected()) emit clipItemSelected(NULL);
        dup->baseClip()->removeReference();
        m_document->updateClip(dup->baseClip()->getId());
        scene()->removeItem(dup);
        delete dup;

        ItemInfo clipinfo = item->info();
        clipinfo.track = m_document->tracksCount() - clipinfo.track;
        bool success = m_document->renderer()->mltResizeClipEnd(clipinfo, info.endPos - info.startPos);
        if (success) {
            item->resizeEnd((int) info.endPos.frames(m_document->fps()));
            setDocumentModified();
        } else
            emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);

    }
    QTimer::singleShot(3000, this, SLOT(slotEnableRefresh()));
}

void CustomTrackView::slotEnableRefresh()
{
    m_blockRefresh = false;
}

void CustomTrackView::slotAddTransitionToSelectedClips(QDomElement transition)
{
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() == 1) {
        if (itemList.at(0)->type() == AVWIDGET) {
            ClipItem *item = (ClipItem *) itemList.at(0);
            ItemInfo info;
            info.track = item->track();
            ClipItem *transitionClip = NULL;
            const int transitiontrack = getPreviousVideoTrack(info.track);
            GenTime pos = GenTime((int)(mapToScene(m_menuPosition).x()), m_document->fps());
            if (pos < item->startPos() + item->cropDuration() / 2) {
                // add transition to clip start
                info.startPos = item->startPos();
                if (transitiontrack != 0) transitionClip = getClipItemAt((int) info.startPos.frames(m_document->fps()), m_document->tracksCount() - transitiontrack);
                if (transitionClip && transitionClip->endPos() < item->endPos()) {
                    info.endPos = transitionClip->endPos();
                } else info.endPos = info.startPos + GenTime(65, m_document->fps());
                // Check there is no other transition at that place
                double startY = info.track * m_tracksHeight + 1 + m_tracksHeight / 2;
                QRectF r(info.startPos.frames(m_document->fps()), startY, (info.endPos - info.startPos).frames(m_document->fps()), m_tracksHeight / 2);
                QList<QGraphicsItem *> selection = m_scene->items(r);
                bool transitionAccepted = true;
                for (int i = 0; i < selection.count(); i++) {
                    if (selection.at(i)->type() == TRANSITIONWIDGET) {
                        Transition *tr = static_cast <Transition *>(selection.at(i));
                        if (tr->startPos() - info.startPos > GenTime(5, m_document->fps())) {
                            if (tr->startPos() < info.endPos) info.endPos = tr->startPos();
                        } else transitionAccepted = false;
                    }
                }
                if (transitionAccepted) slotAddTransition(item, info, transitiontrack, transition);
                else emit displayMessage(i18n("Cannot add transition"), ErrorMessage);

            } else {
                // add transition to clip  end
                info.endPos = item->endPos();
                if (transitiontrack != 0) transitionClip = getClipItemAt((int) info.endPos.frames(m_document->fps()), m_document->tracksCount() - transitiontrack);
                if (transitionClip && transitionClip->startPos() > item->startPos()) {
                    info.startPos = transitionClip->startPos();
                } else info.startPos = info.endPos - GenTime(65, m_document->fps());
                if (transition.attribute("tag") == "luma") EffectsList::setParameter(transition, "reverse", "1");
                else if (transition.attribute("id") == "slide") EffectsList::setParameter(transition, "invert", "1");

                // Check there is no other transition at that place
                double startY = info.track * m_tracksHeight + 1 + m_tracksHeight / 2;
                QRectF r(info.startPos.frames(m_document->fps()), startY, (info.endPos - info.startPos).frames(m_document->fps()), m_tracksHeight / 2);
                QList<QGraphicsItem *> selection = m_scene->items(r);
                bool transitionAccepted = true;
                for (int i = 0; i < selection.count(); i++) {
                    if (selection.at(i)->type() == TRANSITIONWIDGET) {
                        Transition *tr = static_cast <Transition *>(selection.at(i));
                        if (info.endPos - tr->endPos() > GenTime(5, m_document->fps())) {
                            if (tr->endPos() > info.startPos) info.startPos = tr->endPos();
                        } else transitionAccepted = false;
                    }
                }
                if (transitionAccepted) slotAddTransition(item, info, transitiontrack, transition);
                else emit displayMessage(i18n("Cannot add transition"), ErrorMessage);
            }
        }
    } else for (int i = 0; i < itemList.count(); i++) {
            if (itemList.at(i)->type() == AVWIDGET) {
                ClipItem *item = (ClipItem *) itemList.at(i);
                ItemInfo info;
                info.startPos = item->startPos();
                info.endPos = info.startPos + GenTime(65, m_document->fps());
                info.track = item->track();

                // Check there is no other transition at that place
                double startY = info.track * m_tracksHeight + 1 + m_tracksHeight / 2;
                QRectF r(info.startPos.frames(m_document->fps()), startY, (info.endPos - info.startPos).frames(m_document->fps()), m_tracksHeight / 2);
                QList<QGraphicsItem *> selection = m_scene->items(r);
                bool transitionAccepted = true;
                for (int i = 0; i < selection.count(); i++) {
                    if (selection.at(i)->type() == TRANSITIONWIDGET) {
                        Transition *tr = static_cast <Transition *>(selection.at(i));
                        if (tr->startPos() - info.startPos > GenTime(5, m_document->fps())) {
                            if (tr->startPos() < info.endPos) info.endPos = tr->startPos();
                        } else transitionAccepted = false;
                    }
                }
                int transitiontrack = getPreviousVideoTrack(info.track);
                if (transitionAccepted) slotAddTransition(item, info, transitiontrack, transition);
                else emit displayMessage(i18n("Cannot add transition"), ErrorMessage);
            }
        }
}

void CustomTrackView::slotAddTransition(ClipItem* /*clip*/, ItemInfo transitionInfo, int endTrack, QDomElement transition)
{
    if (transitionInfo.startPos >= transitionInfo.endPos) {
        emit displayMessage(i18n("Invalid transition"), ErrorMessage);
        return;
    }
    AddTransitionCommand* command = new AddTransitionCommand(this, transitionInfo, endTrack, transition, false, true);
    m_commandStack->push(command);
    setDocumentModified();
}

void CustomTrackView::addTransition(ItemInfo transitionInfo, int endTrack, QDomElement params)
{
    Transition *tr = new Transition(transitionInfo, endTrack, m_document->fps(), params, true);
    //kDebug() << "---- ADDING transition " << params.attribute("value");
    if (m_document->renderer()->mltAddTransition(tr->transitionTag(), endTrack, m_document->tracksCount() - transitionInfo.track, transitionInfo.startPos, transitionInfo.endPos, tr->toXML())) {
        scene()->addItem(tr);
        setDocumentModified();
    } else {
        emit displayMessage(i18n("Cannot add transition"), ErrorMessage);
        delete tr;
    }
}

void CustomTrackView::deleteTransition(ItemInfo transitionInfo, int endTrack, QDomElement /*params*/)
{
    Transition *item = getTransitionItemAt(transitionInfo.startPos, transitionInfo.track);
    if (!item) {
        emit displayMessage(i18n("Select clip to delete"), ErrorMessage);
        return;
    }
    m_document->renderer()->mltDeleteTransition(item->transitionTag(), endTrack, m_document->tracksCount() - transitionInfo.track, transitionInfo.startPos, transitionInfo.endPos, item->toXML());
    if (m_dragItem == item) m_dragItem = NULL;
    delete item;
    emit transitionItemSelected(NULL);
    setDocumentModified();
}

void CustomTrackView::slotTransitionUpdated(Transition *tr, QDomElement old)
{
    kDebug() << "TRANS UPDATE, TRACKS: " << old.attribute("transition_btrack") << ", NEW: " << tr->toXML().attribute("transition_btrack");
    EditTransitionCommand *command = new EditTransitionCommand(this, tr->track(), tr->startPos(), old, tr->toXML(), false);
    m_commandStack->push(command);
    setDocumentModified();
}

void CustomTrackView::updateTransition(int track, GenTime pos, QDomElement oldTransition, QDomElement transition, bool updateTransitionWidget)
{
    Transition *item = getTransitionItemAt(pos, track);
    if (!item) {
        kWarning() << "Unable to find transition at pos :" << pos.frames(m_document->fps()) << ", ON track: " << track;
        return;
    }
    m_document->renderer()->mltUpdateTransition(oldTransition.attribute("tag"), transition.attribute("tag"), transition.attribute("transition_btrack").toInt(), m_document->tracksCount() - transition.attribute("transition_atrack").toInt(), item->startPos(), item->endPos(), transition);
    //kDebug() << "ORIGINAL TRACK: "<< oldTransition.attribute("transition_btrack") << ", NEW TRACK: "<<transition.attribute("transition_btrack");
    item->setTransitionParameters(transition);
    if (updateTransitionWidget) {
        ItemInfo info = item->info();
        QPoint p;
        ClipItem *transitionClip = getClipItemAt(info.startPos, info.track);
        if (transitionClip && transitionClip->baseClip()) {
            QString size = transitionClip->baseClip()->getProperty("frame_size");
            p.setX(size.section('x', 0, 0).toInt());
            p.setY(size.section('x', 1, 1).toInt());
        }
        emit transitionItemSelected(item, getPreviousVideoTrack(info.track), p, true);
    }
    setDocumentModified();
}

void CustomTrackView::dragMoveEvent(QDragMoveEvent * event)
{
    event->setDropAction(Qt::IgnoreAction);
    const QPointF pos = mapToScene(event->pos());
    if (m_selectionGroup && m_clipDrag) {
        m_selectionGroup->setPos(pos.x(), pos.y());
        emit mousePosition((int)(m_selectionGroup->scenePos().x() + 0.5));
        event->setDropAction(Qt::MoveAction);
        event->acceptProposedAction();
    } else {
        QGraphicsView::dragMoveEvent(event);
    }
}

void CustomTrackView::dragLeaveEvent(QDragLeaveEvent * event)
{
    if (m_selectionGroup && m_clipDrag) {
        m_thumbsTimer.stop();
        m_waitingThumbs.clear();
        QList<QGraphicsItem *> items = m_selectionGroup->childItems();
        qDeleteAll(items);
        scene()->destroyItemGroup(m_selectionGroup);
        m_selectionGroup = NULL;
    } else QGraphicsView::dragLeaveEvent(event);
}

void CustomTrackView::dropEvent(QDropEvent * event)
{
    if (m_selectionGroup && m_clipDrag) {
        QList<QGraphicsItem *> items = m_selectionGroup->childItems();
        resetSelectionGroup();
        m_scene->clearSelection();
        bool hasVideoClip = false;
        QUndoCommand *addCommand = new QUndoCommand();
        addCommand->setText(i18n("Add timeline clip"));

        for (int i = 0; i < items.count(); i++) {
            ClipItem *item = static_cast <ClipItem *>(items.at(i));
            if (!hasVideoClip && (item->clipType() == AV || item->clipType() == VIDEO)) hasVideoClip = true;
            if (items.count() == 1) {
                updateClipTypeActions(item);
            } else {
                updateClipTypeActions(NULL);
            }

            new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), item->info(), item->effectList(), false, false, addCommand);
            item->baseClip()->addReference();
            m_document->updateClip(item->baseClip()->getId());
            ItemInfo info = item->info();

            int tracknumber = m_document->tracksCount() - info.track - 1;
            bool isLocked = m_document->trackInfoAt(tracknumber).isLocked;
            if (isLocked) item->setItemLocked(true);
            ItemInfo clipInfo = info;
            clipInfo.track = m_document->tracksCount() - item->track();
            if (m_document->renderer()->mltInsertClip(clipInfo, item->xml(), item->baseClip()->producer(item->track())) == -1) {
                emit displayMessage(i18n("Cannot insert clip in timeline"), ErrorMessage);
            }

            if (item->baseClip()->isTransparent() && getTransitionItemAtStart(info.startPos, info.track) == NULL) {
                // add transparency transition
                QDomElement trans = MainWindow::transitions.getEffectByTag("composite", "composite").cloneNode().toElement();
                new AddTransitionCommand(this, info, getPreviousVideoTrack(info.track), trans, false, true, addCommand);
            }
            item->setSelected(true);
        }
        m_commandStack->push(addCommand);
        setDocumentModified();
        m_pasteEffectsAction->setEnabled(m_copiedItems.count() == 1);
        if (items.count() > 1) groupSelectedItems(true);
        event->setDropAction(Qt::MoveAction);
        event->accept();
    } else QGraphicsView::dropEvent(event);
    setFocus();
}


QStringList CustomTrackView::mimeTypes() const
{
    QStringList qstrList;
    // list of accepted mime types for drop
    qstrList.append("text/plain");
    qstrList.append("kdenlive/producerslist");
    qstrList.append("kdenlive/clip");
    return qstrList;
}

Qt::DropActions CustomTrackView::supportedDropActions() const
{
    // returns what actions are supported when dropping
    return Qt::MoveAction;
}

void CustomTrackView::setDuration(int duration)
{
    int diff = qAbs(duration - sceneRect().width());
    if (diff * matrix().m11() > -50) {
        if (matrix().m11() < 0.4) setSceneRect(0, 0, (duration + 100 / matrix().m11()), sceneRect().height());
        else setSceneRect(0, 0, (duration + 300), sceneRect().height());
    }
    m_projectDuration = duration;
}

int CustomTrackView::duration() const
{
    return m_projectDuration;
}

void CustomTrackView::addTrack(TrackInfo type, int ix)
{
    if (ix == -1 || ix == m_document->tracksCount()) {
        m_document->insertTrack(ix, type);
        m_document->renderer()->mltInsertTrack(1, type.type == VIDEOTRACK);
    } else {
        m_document->insertTrack(m_document->tracksCount() - ix, type);
        // insert track in MLT playlist
        m_document->renderer()->mltInsertTrack(m_document->tracksCount() - ix, type.type == VIDEOTRACK);

        double startY = ix * m_tracksHeight + 1 + m_tracksHeight / 2;
        QRectF r(0, startY, sceneRect().width(), sceneRect().height() - startY);
        QList<QGraphicsItem *> selection = m_scene->items(r);
        resetSelectionGroup();

        m_selectionGroup = new AbstractGroupItem(m_document->fps());
        scene()->addItem(m_selectionGroup);
        for (int i = 0; i < selection.count(); i++) {
            if ((!selection.at(i)->parentItem()) && (selection.at(i)->type() == AVWIDGET || selection.at(i)->type() == TRANSITIONWIDGET || selection.at(i)->type() == GROUPWIDGET)) {
                m_selectionGroup->addToGroup(selection.at(i));
                selection.at(i)->setFlags(QGraphicsItem::ItemIsSelectable);
            }
        }
        // Move graphic items
        m_selectionGroup->translate(0, m_tracksHeight);

        // adjust track number
        QList<QGraphicsItem *> children = m_selectionGroup->childItems();
        for (int i = 0; i < children.count(); i++) {
            if (children.at(i)->type() == GROUPWIDGET) {
                AbstractGroupItem *grp = static_cast<AbstractGroupItem*>(children.at(i));
                children << grp->childItems();
                continue;
            }
            AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
            item->updateItem();
            ItemInfo clipinfo = item->info();
            if (item->type() == AVWIDGET) {
                ClipItem *clip = static_cast <ClipItem *>(item);
                // We add a move clip command so that we get the correct producer for new track number
                if (clip->clipType() == AV || clip->clipType() == AUDIO) {
                    Mlt::Producer *prod;
                    if (clip->isAudioOnly()) prod = clip->baseClip()->audioProducer(clipinfo.track);
                    else if (clip->isVideoOnly()) prod = clip->baseClip()->videoProducer();
                    else prod = clip->baseClip()->producer(clipinfo.track);
                    m_document->renderer()->mltUpdateClipProducer((int)(m_document->tracksCount() - clipinfo.track), clipinfo.startPos.frames(m_document->fps()), prod);
                    kDebug() << "// UPDATING CLIP TO TRACK PROD: " << clipinfo.track;
                }
            } else if (item->type() == TRANSITIONWIDGET) {
                Transition *tr = static_cast <Transition *>(item);
                int track = tr->transitionEndTrack();
                if (track >= ix) {
                    tr->updateTransitionEndTrack(getPreviousVideoTrack(clipinfo.track));
                }
            }
        }
        resetSelectionGroup(false);
    }

    int maxHeight = m_tracksHeight * m_document->tracksCount();
    for (int i = 0; i < m_guides.count(); i++) {
        QLineF l = m_guides.at(i)->line();
        l.setP2(QPointF(l.x2(), maxHeight));
        m_guides.at(i)->setLine(l);
    }
    m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), maxHeight);
    setSceneRect(0, 0, sceneRect().width(), maxHeight);
    QTimer::singleShot(300, this, SIGNAL(trackHeightChanged()));
    viewport()->update();
    //setFixedHeight(50 * m_tracksCount);
}

void CustomTrackView::removeTrack(int ix)
{
    // Delete track in MLT playlist
    m_document->renderer()->mltDeleteTrack(m_document->tracksCount() - ix);
    m_document->deleteTrack(m_document->tracksCount() - ix - 1);

    double startY = ix * (m_tracksHeight + 1) + m_tracksHeight / 2;
    QRectF r(0, startY, sceneRect().width(), sceneRect().height() - startY);
    QList<QGraphicsItem *> selection = m_scene->items(r);

    resetSelectionGroup();

    m_selectionGroup = new AbstractGroupItem(m_document->fps());
    scene()->addItem(m_selectionGroup);
    for (int i = 0; i < selection.count(); i++) {
        if ((!selection.at(i)->parentItem()) && (selection.at(i)->type() == AVWIDGET || selection.at(i)->type() == TRANSITIONWIDGET || selection.at(i)->type() == GROUPWIDGET)) {
            m_selectionGroup->addToGroup(selection.at(i));
            selection.at(i)->setFlags(QGraphicsItem::ItemIsSelectable);
        }
    }
    // Move graphic items
    qreal ydiff = 0 - (int) m_tracksHeight;
    m_selectionGroup->translate(0, ydiff);

    // adjust track number
    QList<QGraphicsItem *> children = m_selectionGroup->childItems();
    //kDebug() << "// FOUND CLIPS TO MOVE: " << children.count();
    for (int i = 0; i < children.count(); i++) {
        if (children.at(i)->type() == GROUPWIDGET) {
            AbstractGroupItem *grp = static_cast<AbstractGroupItem*>(children.at(i));
            children << grp->childItems();
            continue;
        }
        if (children.at(i)->type() == AVWIDGET) {
            ClipItem *clip = static_cast <ClipItem *>(children.at(i));
            clip->updateItem();
            ItemInfo clipinfo = clip->info();
            // We add a move clip command so that we get the correct producer for new track number
            if (clip->clipType() == AV || clip->clipType() == AUDIO) {
                Mlt::Producer *prod;
                if (clip->isAudioOnly()) prod = clip->baseClip()->audioProducer(clipinfo.track);
                else if (clip->isVideoOnly()) prod = clip->baseClip()->videoProducer();
                else prod = clip->baseClip()->producer(clipinfo.track);
                m_document->renderer()->mltUpdateClipProducer((int)(m_document->tracksCount() - clipinfo.track), clipinfo.startPos.frames(m_document->fps()), prod);
            }
        } else if (children.at(i)->type() == TRANSITIONWIDGET) {
            Transition *tr = static_cast <Transition *>(children.at(i));
            tr->updateItem();
            int track = tr->transitionEndTrack();
            if (track >= ix) {
                ItemInfo clipinfo = tr->info();
                tr->updateTransitionEndTrack(getPreviousVideoTrack(clipinfo.track));
            }
        }
    }
    resetSelectionGroup(false);

    int maxHeight = m_tracksHeight * m_document->tracksCount();
    for (int i = 0; i < m_guides.count(); i++) {
        QLineF l = m_guides.at(i)->line();
        l.setP2(QPointF(l.x2(), maxHeight));
        m_guides.at(i)->setLine(l);
    }
    m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), maxHeight);
    setSceneRect(0, 0, sceneRect().width(), maxHeight);
    QTimer::singleShot(300, this, SIGNAL(trackHeightChanged()));
    viewport()->update();
}

void CustomTrackView::changeTrack(int ix, TrackInfo type)
{
    int tracknumber = m_document->tracksCount() - ix;
    m_document->setTrackType(tracknumber - 1, type);
    m_document->renderer()->mltChangeTrackState(tracknumber, m_document->trackInfoAt(tracknumber - 1).isMute, m_document->trackInfoAt(tracknumber - 1).isBlind);
    QTimer::singleShot(300, this, SIGNAL(trackHeightChanged()));
    viewport()->update();
}


void CustomTrackView::slotSwitchTrackAudio(int ix)
{
    /*for (int i = 0; i < m_document->tracksCount(); i++)
        kDebug() << "TRK " << i << " STATE: " << m_document->trackInfoAt(i).isMute << m_document->trackInfoAt(i).isBlind;*/
    int tracknumber = m_document->tracksCount() - ix - 1;
    m_document->switchTrackAudio(tracknumber, !m_document->trackInfoAt(tracknumber).isMute);
    kDebug() << "NEXT TRK STATE: " << m_document->trackInfoAt(tracknumber).isMute << m_document->trackInfoAt(tracknumber).isBlind;
    m_document->renderer()->mltChangeTrackState(tracknumber + 1, m_document->trackInfoAt(tracknumber).isMute, m_document->trackInfoAt(tracknumber).isBlind);
    setDocumentModified();
}

void CustomTrackView::slotSwitchTrackLock(int ix)
{
    int tracknumber = m_document->tracksCount() - ix - 1;
    LockTrackCommand *command = new LockTrackCommand(this, ix, !m_document->trackInfoAt(tracknumber).isLocked);
    m_commandStack->push(command);
}


void CustomTrackView::lockTrack(int ix, bool lock)
{
    int tracknumber = m_document->tracksCount() - ix - 1;
    m_document->switchTrackLock(tracknumber, lock);
    emit doTrackLock(ix, lock);
    QList<QGraphicsItem *> selection = m_scene->items(0, ix * m_tracksHeight + m_tracksHeight / 2, sceneRect().width(), m_tracksHeight / 2 - 2);

    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() != AVWIDGET && selection.at(i)->type() != TRANSITIONWIDGET) continue;
        if (selection.at(i)->isSelected()) {
            if (selection.at(i)->type() == AVWIDGET) emit clipItemSelected(NULL);
            else emit transitionItemSelected(NULL);
        }
        static_cast <AbstractClipItem *>(selection.at(i))->setItemLocked(lock);
    }
    kDebug() << "NEXT TRK STATE: " << m_document->trackInfoAt(tracknumber).isLocked;
    viewport()->update();
    setDocumentModified();
}

void CustomTrackView::slotSwitchTrackVideo(int ix)
{
    int tracknumber = m_document->tracksCount() - ix;
    m_document->switchTrackVideo(tracknumber - 1, !m_document->trackInfoAt(tracknumber - 1).isBlind);
    m_document->renderer()->mltChangeTrackState(tracknumber, m_document->trackInfoAt(tracknumber - 1).isMute, m_document->trackInfoAt(tracknumber - 1).isBlind);
    setDocumentModified();
}

void CustomTrackView::slotRemoveSpace()
{
    GenTime pos;
    int track = 0;
    if (m_menuPosition.isNull()) {
        pos = GenTime(cursorPos(), m_document->fps());
        bool ok;
        track = QInputDialog::getInteger(this, i18n("Remove Space"), i18n("Track"), 0, 0, m_document->tracksCount() - 1, 1, &ok);
        if (!ok) return;
    } else {
        pos = GenTime((int)(mapToScene(m_menuPosition).x()), m_document->fps());
        track = (int)(mapToScene(m_menuPosition).y() / m_tracksHeight);
    }
    ClipItem *item = getClipItemAt(pos, track);
    if (item) {
        emit displayMessage(i18n("You must be in an empty space to remove space (time: %1, track: %2)", m_document->timecode().getTimecodeFromFrames(mapToScene(m_menuPosition).x()), track), ErrorMessage);
        return;
    }
    int length = m_document->renderer()->mltGetSpaceLength(pos, m_document->tracksCount() - track, true);
    //kDebug() << "// GOT LENGT; " << length;
    if (length <= 0) {
        emit displayMessage(i18n("You must be in an empty space to remove space (time=%1, track:%2)", m_document->timecode().getTimecodeFromFrames(mapToScene(m_menuPosition).x()), track), ErrorMessage);
        return;
    }

    QRectF r(pos.frames(m_document->fps()), track * m_tracksHeight + m_tracksHeight / 2, sceneRect().width() - pos.frames(m_document->fps()), m_tracksHeight / 2 - 1);
    QList<QGraphicsItem *> items = m_scene->items(r);

    QList<ItemInfo> clipsToMove;
    QList<ItemInfo> transitionsToMove;

    for (int i = 0; i < items.count(); i++) {
        if (items.at(i)->type() == AVWIDGET || items.at(i)->type() == TRANSITIONWIDGET) {
            AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
            ItemInfo info = item->info();
            if (item->type() == AVWIDGET) {
                clipsToMove.append(info);
            } else if (item->type() == TRANSITIONWIDGET) {
                transitionsToMove.append(info);
            }
        }
    }

    InsertSpaceCommand *command = new InsertSpaceCommand(this, clipsToMove, transitionsToMove, track, GenTime(-length, m_document->fps()), true);
    m_commandStack->push(command);
}

void CustomTrackView::slotInsertSpace()
{
    GenTime pos;
    int track = 0;
    if (m_menuPosition.isNull()) {
        pos = GenTime(cursorPos(), m_document->fps());
    } else {
        pos = GenTime((int)(mapToScene(m_menuPosition).x()), m_document->fps());
        track = (int)(mapToScene(m_menuPosition).y() / m_tracksHeight) + 1;
    }
    SpacerDialog d(GenTime(65, m_document->fps()), m_document->timecode(), track, m_document->tracksCount(), this);
    if (d.exec() != QDialog::Accepted) return;
    GenTime spaceDuration = d.selectedDuration();
    track = d.selectedTrack();
    ClipItem *item = getClipItemAt(pos, track);
    if (item) pos = item->startPos();

    int minh = 0;
    int maxh = sceneRect().height();
    if (track != -1) {
        minh = track * m_tracksHeight + m_tracksHeight / 2;
        maxh = m_tracksHeight / 2 - 1;
    }

    QRectF r(pos.frames(m_document->fps()), minh, sceneRect().width() - pos.frames(m_document->fps()), maxh);
    QList<QGraphicsItem *> items = m_scene->items(r);

    QList<ItemInfo> clipsToMove;
    QList<ItemInfo> transitionsToMove;

    for (int i = 0; i < items.count(); i++) {
        if (items.at(i)->type() == AVWIDGET || items.at(i)->type() == TRANSITIONWIDGET) {
            AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
            ItemInfo info = item->info();
            if (item->type() == AVWIDGET) {
                clipsToMove.append(info);
            } else if (item->type() == TRANSITIONWIDGET) {
                transitionsToMove.append(info);
            }
        }
    }

    InsertSpaceCommand *command = new InsertSpaceCommand(this, clipsToMove, transitionsToMove, track, spaceDuration, true);
    m_commandStack->push(command);
}

void CustomTrackView::insertSpace(QList<ItemInfo> clipsToMove, QList<ItemInfo> transToMove, int track, const GenTime duration, const GenTime offset)
{
    int diff = duration.frames(m_document->fps());
    resetSelectionGroup();
    m_selectionGroup = new AbstractGroupItem(m_document->fps());
    scene()->addItem(m_selectionGroup);
    ClipItem *clip;
    Transition *transition;

    // Create lists with start pos for each track
    QMap <int, int> trackClipStartList;
    QMap <int, int> trackTransitionStartList;

    for (int i = 1; i < m_document->tracksCount() + 1; i++) {
        trackClipStartList[i] = -1;
        trackTransitionStartList[i] = -1;
    }

    if (!clipsToMove.isEmpty()) for (int i = 0; i < clipsToMove.count(); i++) {
            clip = getClipItemAtStart(clipsToMove.at(i).startPos + offset, clipsToMove.at(i).track);
            if (clip) {
                if (clip->parentItem()) {
                    m_selectionGroup->addToGroup(clip->parentItem());
                    clip->parentItem()->setFlags(QGraphicsItem::ItemIsSelectable);
                } else {
                    m_selectionGroup->addToGroup(clip);
                    clip->setFlags(QGraphicsItem::ItemIsSelectable);
                }
                if (trackClipStartList.value(m_document->tracksCount() - clipsToMove.at(i).track) == -1 || clipsToMove.at(i).startPos.frames(m_document->fps()) < trackClipStartList.value(m_document->tracksCount() - clipsToMove.at(i).track))
                    trackClipStartList[m_document->tracksCount() - clipsToMove.at(i).track] = clipsToMove.at(i).startPos.frames(m_document->fps());
            } else emit displayMessage(i18n("Cannot move clip at position %1, track %2", m_document->timecode().getTimecodeFromFrames(clipsToMove.at(i).startPos.frames(m_document->fps())), clipsToMove.at(i).track), ErrorMessage);
        }
    if (!transToMove.isEmpty()) for (int i = 0; i < transToMove.count(); i++) {
            transition = getTransitionItemAtStart(transToMove.at(i).startPos + offset, transToMove.at(i).track);
            if (transition) {
                if (transition->parentItem()) m_selectionGroup->addToGroup(transition->parentItem());
                m_selectionGroup->addToGroup(transition);
                if (trackTransitionStartList.value(m_document->tracksCount() - transToMove.at(i).track) == -1 || transToMove.at(i).startPos.frames(m_document->fps()) < trackTransitionStartList.value(m_document->tracksCount() - transToMove.at(i).track))
                    trackTransitionStartList[m_document->tracksCount() - transToMove.at(i).track] = transToMove.at(i).startPos.frames(m_document->fps());
                transition->setFlags(QGraphicsItem::ItemIsSelectable);
            } else emit displayMessage(i18n("Cannot move transition at position %1, track %2", m_document->timecode().getTimecodeFromFrames(transToMove.at(i).startPos.frames(m_document->fps())), transToMove.at(i).track), ErrorMessage);
        }
    m_selectionGroup->translate(diff, 0);

    // update items coordinates
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET || itemList.at(i)->type() == TRANSITIONWIDGET) {
            static_cast < AbstractClipItem *>(itemList.at(i))->updateItem();
        } else if (itemList.at(i)->type() == GROUPWIDGET) {
            QList<QGraphicsItem *> children = itemList.at(i)->childItems();
            for (int j = 0; j < children.count(); j++) {
                static_cast < AbstractClipItem *>(children.at(j))->updateItem();
            }
        }
    }
    resetSelectionGroup(false);
    if (track != -1) track = m_document->tracksCount() - track;
    m_document->renderer()->mltInsertSpace(trackClipStartList, trackTransitionStartList, track, duration, offset);
}

void CustomTrackView::deleteClip(const QString &clipId)
{
    resetSelectionGroup();
    QList<QGraphicsItem *> itemList = items();
    QUndoCommand *deleteCommand = new QUndoCommand();
    deleteCommand->setText(i18n("Delete timeline clips"));
    int count = 0;
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = (ClipItem *)itemList.at(i);
            if (item->clipProducer() == clipId) {
                count++;
                if (item->parentItem()) {
                    // Clip is in a group, destroy the group
                    new GroupClipsCommand(this, QList<ItemInfo>() << item->info(), QList<ItemInfo>(), false, deleteCommand);
                }
                new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), item->info(), item->effectList(), true, true, deleteCommand);
            }
        }
    }
    if (count == 0) delete deleteCommand;
    else m_commandStack->push(deleteCommand);
}

void CustomTrackView::setCursorPos(int pos, bool seek)
{
    if (pos == m_cursorPos) return;
    emit cursorMoved((int)(m_cursorPos), (int)(pos));
    m_cursorPos = pos;
    if (seek) m_document->renderer()->seek(GenTime(m_cursorPos, m_document->fps()));
    else if (m_autoScroll) checkScrolling();
    m_cursorLine->setPos(m_cursorPos, 0);
}

void CustomTrackView::updateCursorPos()
{
    m_cursorLine->setPos(m_cursorPos, 0);
}

int CustomTrackView::cursorPos()
{
    return (int)(m_cursorPos);
}

void CustomTrackView::moveCursorPos(int delta)
{
    if (m_cursorPos + delta < 0) delta = 0 - m_cursorPos;
    emit cursorMoved((int)(m_cursorPos), (int)((m_cursorPos + delta)));
    m_cursorPos += delta;
    m_cursorLine->setPos(m_cursorPos, 0);
    m_document->renderer()->seek(GenTime(m_cursorPos, m_document->fps()));
}

void CustomTrackView::initCursorPos(int pos)
{
    emit cursorMoved((int)(m_cursorPos), (int)(pos));
    m_cursorPos = pos;
    m_cursorLine->setPos(pos, 0);
    checkScrolling();
}

void CustomTrackView::checkScrolling()
{
    ensureVisible(m_cursorPos, verticalScrollBar()->value() + 10, 2, 2, 50, 0);
}

void CustomTrackView::mouseReleaseEvent(QMouseEvent * event)
{
    if (m_moveOpMode == SEEK) m_moveOpMode = NONE;
    QGraphicsView::mouseReleaseEvent(event);
    if (m_scrollTimer.isActive()) m_scrollTimer.stop();
    if (event->button() == Qt::MidButton) {
        return;
    }
    setDragMode(QGraphicsView::NoDrag);
    if (m_operationMode == MOVEGUIDE) {
        setCursor(Qt::ArrowCursor);
        m_operationMode = NONE;
        m_dragGuide->setFlag(QGraphicsItem::ItemIsMovable, false);
        GenTime newPos = GenTime(m_dragGuide->pos().x(), m_document->fps());
        if (newPos != m_dragGuide->position()) {
            EditGuideCommand *command = new EditGuideCommand(this, m_dragGuide->position(), m_dragGuide->label(), newPos, m_dragGuide->label(), false);
            m_commandStack->push(command);
            m_dragGuide->updateGuide(GenTime(m_dragGuide->pos().x(), m_document->fps()));
            qSort(m_guides.begin(), m_guides.end(), sortGuidesList);
            m_document->syncGuides(m_guides);
        }
        m_dragGuide = NULL;
        m_dragItem = NULL;
        return;
    } else if (m_operationMode == SPACER && m_selectionGroup) {
        int track;
        if (event->modifiers() != Qt::ControlModifier) {
            // We are moving all tracks
            track = -1;
        } else track = (int)(mapToScene(m_clickEvent).y() / m_tracksHeight);
        GenTime timeOffset = GenTime(m_selectionGroup->scenePos().x(), m_document->fps()) - m_selectionGroupInfo.startPos;
        if (timeOffset != GenTime()) {
            QList<QGraphicsItem *> items = m_selectionGroup->childItems();

            QList<ItemInfo> clipsToMove;
            QList<ItemInfo> transitionsToMove;

            // Create lists with start pos for each track
            QMap <int, int> trackClipStartList;
            QMap <int, int> trackTransitionStartList;

            for (int i = 1; i < m_document->tracksCount() + 1; i++) {
                trackClipStartList[i] = -1;
                trackTransitionStartList[i] = -1;
            }

            int max = items.count();
            for (int i = 0; i < max; i++) {
                if (items.at(i)->type() == GROUPWIDGET)
                    items += static_cast <QGraphicsItemGroup *>(items.at(i))->childItems();
            }

            for (int i = 0; i < items.count(); i++) {
                if (items.at(i)->type() == AVWIDGET) {
                    AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
                    ItemInfo info = item->info();
                    clipsToMove.append(info);
                    item->updateItem();
                    if (trackClipStartList.value(m_document->tracksCount() - info.track) == -1 || info.startPos.frames(m_document->fps()) < trackClipStartList.value(m_document->tracksCount() - info.track))
                        trackClipStartList[m_document->tracksCount() - info.track] = info.startPos.frames(m_document->fps());
                } else if (items.at(i)->type() == TRANSITIONWIDGET) {
                    AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
                    ItemInfo info = item->info();
                    transitionsToMove.append(info);
                    item->updateItem();
                    if (trackTransitionStartList.value(m_document->tracksCount() - info.track) == -1 || info.startPos.frames(m_document->fps()) < trackTransitionStartList.value(m_document->tracksCount() - info.track))
                        trackTransitionStartList[m_document->tracksCount() - info.track] = info.startPos.frames(m_document->fps());
                }
            }

            InsertSpaceCommand *command = new InsertSpaceCommand(this, clipsToMove, transitionsToMove, track, timeOffset, false);
            m_commandStack->push(command);
            if (track != -1) track = m_document->tracksCount() - track;
            kDebug() << "SPACER TRACK:" << track;
            m_document->renderer()->mltInsertSpace(trackClipStartList, trackTransitionStartList, track, timeOffset, GenTime());
        }
        resetSelectionGroup(false);
        m_operationMode = NONE;
    } else if (m_operationMode == RUBBERSELECTION) {
        //kDebug() << "// END RUBBER SELECT";
        resetSelectionGroup();
        groupSelectedItems();
        m_operationMode = NONE;
    }

    if (m_dragItem == NULL && m_selectionGroup == NULL) {
        emit transitionItemSelected(NULL);
        return;
    }
    ItemInfo info;
    if (m_dragItem) info = m_dragItem->info();

    if (m_operationMode == MOVE) {
        setCursor(Qt::OpenHandCursor);

        if (m_dragItem->parentItem() == 0) {
            // we are moving one clip, easy
            if (m_dragItem->type() == AVWIDGET && (m_dragItemInfo.startPos != info.startPos || m_dragItemInfo.track != info.track)) {
                ClipItem *item = static_cast <ClipItem *>(m_dragItem);
                Mlt::Producer *prod;
                if (item->isAudioOnly()) prod = item->baseClip()->audioProducer(m_dragItemInfo.track);
                else if (item->isVideoOnly()) prod = item->baseClip()->videoProducer();
                else prod = item->baseClip()->producer(m_dragItemInfo.track);
                bool success = m_document->renderer()->mltMoveClip((int)(m_document->tracksCount() - m_dragItemInfo.track), (int)(m_document->tracksCount() - m_dragItem->track()), (int) m_dragItemInfo.startPos.frames(m_document->fps()), (int)(m_dragItem->startPos().frames(m_document->fps())), prod);

                if (success) {
                    kDebug() << "// get trans info";
                    int tracknumber = m_document->tracksCount() - item->track() - 1;
                    bool isLocked = m_document->trackInfoAt(tracknumber).isLocked;
                    if (isLocked) item->setItemLocked(true);

                    QUndoCommand *moveCommand = new QUndoCommand();
                    moveCommand->setText(i18n("Move clip"));
                    new MoveClipCommand(this, m_dragItemInfo, info, false, moveCommand);
                    // Also move automatic transitions (on lower track)
                    Transition *tr = getTransitionItemAtStart(m_dragItemInfo.startPos, m_dragItemInfo.track);
                    kDebug() << "// get trans info2";
                    if (tr && tr->isAutomatic()) {
                        ItemInfo trInfo = tr->info();
                        ItemInfo newTrInfo = trInfo;
                        newTrInfo.track = info.track;
                        newTrInfo.startPos = m_dragItem->startPos();
                        if (m_dragItemInfo.track == info.track && !item->baseClip()->isTransparent() && getClipItemAtEnd(newTrInfo.endPos, m_document->tracksCount() - tr->transitionEndTrack())) {
                            // transition end should stay the same
                        } else {
                            // transition end should be adjusted to clip
                            newTrInfo.endPos = newTrInfo.endPos + (newTrInfo.startPos - trInfo.startPos);
                        }
                        if (newTrInfo.startPos < newTrInfo.endPos) new MoveTransitionCommand(this, trInfo, newTrInfo, true, moveCommand);
                    }
                    if (tr == NULL || tr->endPos() < m_dragItemInfo.endPos) {
                        // Check if there is a transition at clip end
                        tr = getTransitionItemAtEnd(m_dragItemInfo.endPos, m_dragItemInfo.track);
                        if (tr && tr->isAutomatic()) {
                            ItemInfo trInfo = tr->info();
                            ItemInfo newTrInfo = trInfo;
                            newTrInfo.track = info.track;
                            newTrInfo.endPos = m_dragItem->endPos();
                            if (m_dragItemInfo.track == info.track && !item->baseClip()->isTransparent() && getClipItemAtStart(trInfo.startPos, m_document->tracksCount() - tr->transitionEndTrack())) {
                                // transition start should stay the same
                            } else {
                                // transition start should be moved
                                newTrInfo.startPos = newTrInfo.startPos + (newTrInfo.endPos - trInfo.endPos);
                            }
                            if (newTrInfo.startPos < newTrInfo.endPos)
                                new MoveTransitionCommand(this, trInfo, newTrInfo, true, moveCommand);
                        }
                    }
                    // Also move automatic transitions (on upper track)
                    tr = getTransitionItemAtStart(m_dragItemInfo.startPos, m_dragItemInfo.track - 1);
                    if (m_dragItemInfo.track == info.track && tr && tr->isAutomatic() && (m_document->tracksCount() - tr->transitionEndTrack()) == m_dragItemInfo.track) {
                        ItemInfo trInfo = tr->info();
                        ItemInfo newTrInfo = trInfo;
                        newTrInfo.startPos = m_dragItem->startPos();
                        ClipItem * upperClip = getClipItemAt(m_dragItemInfo.startPos, m_dragItemInfo.track - 1);
                        if (!upperClip || !upperClip->baseClip()->isTransparent()) {
                            if (!getClipItemAtEnd(newTrInfo.endPos, tr->track())) {
                                // transition end should be adjusted to clip on upper track
                                newTrInfo.endPos = newTrInfo.endPos + (newTrInfo.startPos - trInfo.startPos);
                            }
                            if (newTrInfo.startPos < newTrInfo.endPos) new MoveTransitionCommand(this, trInfo, newTrInfo, true, moveCommand);
                        }
                    }
                    if (m_dragItemInfo.track == info.track && (tr == NULL || tr->endPos() < m_dragItemInfo.endPos)) {
                        // Check if there is a transition at clip end
                        tr = getTransitionItemAtEnd(m_dragItemInfo.endPos, m_dragItemInfo.track - 1);
                        if (tr && tr->isAutomatic() && (m_document->tracksCount() - tr->transitionEndTrack()) == m_dragItemInfo.track) {
                            ItemInfo trInfo = tr->info();
                            ItemInfo newTrInfo = trInfo;
                            newTrInfo.endPos = m_dragItem->endPos();
                            ClipItem * upperClip = getClipItemAt(m_dragItemInfo.startPos, m_dragItemInfo.track - 1);
                            if (!upperClip || !upperClip->baseClip()->isTransparent()) {
                                if (!getClipItemAtStart(trInfo.startPos, tr->track())) {
                                    // transition start should be moved
                                    newTrInfo.startPos = newTrInfo.startPos + (newTrInfo.endPos - trInfo.endPos);
                                }
                                if (newTrInfo.startPos < newTrInfo.endPos) new MoveTransitionCommand(this, trInfo, newTrInfo, true, moveCommand);
                            }
                        }
                    }
                    m_commandStack->push(moveCommand);
                } else {
                    // undo last move and emit error message
                    bool snap = KdenliveSettings::snaptopoints();
                    KdenliveSettings::setSnaptopoints(false);
                    item->setPos((int) m_dragItemInfo.startPos.frames(m_document->fps()), (int)(m_dragItemInfo.track * m_tracksHeight + 1));
                    KdenliveSettings::setSnaptopoints(snap);
                    emit displayMessage(i18n("Cannot move clip to position %1", m_document->timecode().getTimecodeFromFrames(m_dragItemInfo.startPos.frames(m_document->fps()))), ErrorMessage);
                }
                setDocumentModified();
            }
            if (m_dragItem->type() == TRANSITIONWIDGET && (m_dragItemInfo.startPos != info.startPos || m_dragItemInfo.track != info.track)) {
                Transition *transition = static_cast <Transition *>(m_dragItem);
                if (!m_document->renderer()->mltMoveTransition(transition->transitionTag(), (int)(m_document->tracksCount() - m_dragItemInfo.track), (int)(m_document->tracksCount() - m_dragItem->track()), transition->transitionEndTrack(), m_dragItemInfo.startPos, m_dragItemInfo.endPos, info.startPos, info.endPos)) {
                    // Moving transition failed, revert to previous position
                    emit displayMessage(i18n("Cannot move transition"), ErrorMessage);
                    transition->setPos((int) m_dragItemInfo.startPos.frames(m_document->fps()), (m_dragItemInfo.track) * m_tracksHeight + 1);
                } else {
                    MoveTransitionCommand *command = new MoveTransitionCommand(this, m_dragItemInfo, info, false);
                    m_commandStack->push(command);
                    transition->updateTransitionEndTrack(getPreviousVideoTrack(m_dragItem->track()));
                }
            }
        } else {
            // Moving several clips. We need to delete them and readd them to new position,
            // or they might overlap each other during the move
            QGraphicsItemGroup *group = static_cast <QGraphicsItemGroup *>(m_dragItem->parentItem());
            QList<QGraphicsItem *> items = group->childItems();

            QList<ItemInfo> clipsToMove;
            QList<ItemInfo> transitionsToMove;

            GenTime timeOffset = GenTime(m_dragItem->scenePos().x(), m_document->fps()) - m_dragItemInfo.startPos;
            const int trackOffset = (int)(m_dragItem->scenePos().y() / m_tracksHeight) - m_dragItemInfo.track;
            //kDebug() << "// MOVED SEVERAL CLIPS" << timeOffset.frames(25);
            if (timeOffset != GenTime() || trackOffset != 0) {
                // remove items in MLT playlist

                // Expand groups
                int max = items.count();
                for (int i = 0; i < max; i++) {
                    if (items.at(i)->type() == GROUPWIDGET) {
                        items += items.at(i)->childItems();
                    }
                }

                for (int i = 0; i < items.count(); i++) {
                    if (items.at(i)->type() != AVWIDGET && items.at(i)->type() != TRANSITIONWIDGET) continue;
                    AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
                    ItemInfo info = item->info();
                    if (item->type() == AVWIDGET) {
                        if (m_document->renderer()->mltRemoveClip(m_document->tracksCount() - info.track, info.startPos) == false) {
                            // error, clip cannot be removed from playlist
                            emit displayMessage(i18n("Error removing clip at %1 on track %2", m_document->timecode().getTimecodeFromFrames(info.startPos.frames(m_document->fps())), info.track), ErrorMessage);
                        } else {
                            clipsToMove.append(info);
                        }
                    } else {
                        transitionsToMove.append(info);
                        Transition *tr = static_cast <Transition*>(item);
                        m_document->renderer()->mltDeleteTransition(tr->transitionTag(), tr->transitionEndTrack(), m_document->tracksCount() - info.track, info.startPos, info.endPos, tr->toXML());
                    }
                }

                for (int i = 0; i < items.count(); i++) {
                    // re-add items in correct place
                    if (items.at(i)->type() != AVWIDGET && items.at(i)->type() != TRANSITIONWIDGET) continue;
                    AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
                    item->updateItem();
                    ItemInfo info = item->info();
                    int tracknumber = m_document->tracksCount() - info.track - 1;
                    bool isLocked = m_document->trackInfoAt(tracknumber).isLocked;
                    if (isLocked) {
                        group->removeFromGroup(item);
                        item->setItemLocked(true);
                    }

                    if (item->type() == AVWIDGET) {
                        ClipItem *clip = static_cast <ClipItem*>(item);
                        info.track = m_document->tracksCount() - info.track;
                        Mlt::Producer *prod;
                        if (clip->isAudioOnly()) prod = clip->baseClip()->audioProducer(info.track);
                        else if (clip->isVideoOnly()) prod = clip->baseClip()->videoProducer();
                        else prod = clip->baseClip()->producer(info.track);
                        m_document->renderer()->mltInsertClip(info, clip->xml(), prod);
                        for (int i = 0; i < clip->effectsCount(); i++) {
                            m_document->renderer()->mltAddEffect(info.track, info.startPos, clip->getEffectArgs(clip->effectAt(i)), false);
                        }
                    } else {
                        Transition *tr = static_cast <Transition*>(item);
                        int newTrack = tr->transitionEndTrack();
                        if (!tr->forcedTrack()) {
                            newTrack = getPreviousVideoTrack(info.track);
                        }
                        tr->updateTransitionEndTrack(newTrack);
                        m_document->renderer()->mltAddTransition(tr->transitionTag(), newTrack, m_document->tracksCount() - info.track, info.startPos, info.endPos, tr->toXML());
                    }
                }

                MoveGroupCommand *move = new MoveGroupCommand(this, clipsToMove, transitionsToMove, timeOffset, trackOffset, false);
                m_commandStack->push(move);

                //QPointF top = group->sceneBoundingRect().topLeft();
                //QPointF oldpos = m_selectionGroup->scenePos();
                //kDebug()<<"SELECTION GRP POS: "<<m_selectionGroup->scenePos()<<", TOP: "<<top;
                //group->setPos(top);
                //TODO: get rid of the 3 lines below
                if (m_selectionGroup) {
                    m_selectionGroupInfo.startPos = GenTime(m_selectionGroup->scenePos().x(), m_document->fps());
                    m_selectionGroupInfo.track = m_selectionGroup->track();
                }
                setDocumentModified();
            }
        }
        m_document->renderer()->doRefresh();
    } else if (m_operationMode == RESIZESTART && m_dragItem->startPos() != m_dragItemInfo.startPos) {
        // resize start
        if (m_dragItem->type() == AVWIDGET) {
            ItemInfo resizeinfo = m_dragItemInfo;
            resizeinfo.track = m_document->tracksCount() - resizeinfo.track;
            bool success = m_document->renderer()->mltResizeClipStart(resizeinfo, m_dragItem->startPos() - m_dragItemInfo.startPos);
            if (success) {
                QUndoCommand *resizeCommand = new QUndoCommand();
                resizeCommand->setText(i18n("Resize clip"));

                // Check if there is an automatic transition on that clip (lower track)
                Transition *transition = getTransitionItemAtStart(m_dragItemInfo.startPos, m_dragItemInfo.track);
                if (transition && transition->isAutomatic()) {
                    ItemInfo trInfo = transition->info();
                    ItemInfo newTrInfo = trInfo;
                    newTrInfo.startPos = m_dragItem->startPos();
                    if (newTrInfo.startPos < newTrInfo.endPos)
                        new MoveTransitionCommand(this, trInfo, newTrInfo, true, resizeCommand);
                }
                // Check if there is an automatic transition on that clip (upper track)
                transition = getTransitionItemAtStart(m_dragItemInfo.startPos, m_dragItemInfo.track - 1);
                if (transition && transition->isAutomatic() && (m_document->tracksCount() - transition->transitionEndTrack()) == m_dragItemInfo.track) {
                    ItemInfo trInfo = transition->info();
                    ItemInfo newTrInfo = trInfo;
                    newTrInfo.startPos = m_dragItem->startPos();
                    ClipItem * upperClip = getClipItemAt(m_dragItemInfo.startPos, m_dragItemInfo.track - 1);
                    if ((!upperClip || !upperClip->baseClip()->isTransparent()) && newTrInfo.startPos < newTrInfo.endPos) {
                        new MoveTransitionCommand(this, trInfo, newTrInfo, true, resizeCommand);
                    }
                }
                updateClipFade(static_cast <ClipItem *>(m_dragItem));
                new ResizeClipCommand(this, m_dragItemInfo, info, false, resizeCommand);
                m_commandStack->push(resizeCommand);
            } else {
                m_dragItem->resizeStart((int) m_dragItemInfo.startPos.frames(m_document->fps()));
                emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
            }
        } else if (m_dragItem->type() == TRANSITIONWIDGET) {
            Transition *transition = static_cast <Transition *>(m_dragItem);
            if (!m_document->renderer()->mltMoveTransition(transition->transitionTag(), (int)(m_document->tracksCount() - m_dragItemInfo.track), (int)(m_document->tracksCount() - m_dragItemInfo.track), transition->transitionEndTrack(), m_dragItemInfo.startPos, m_dragItemInfo.endPos, info.startPos, info.endPos)) {
                // Cannot resize transition
                transition->resizeStart((int) m_dragItemInfo.startPos.frames(m_document->fps()));
                emit displayMessage(i18n("Cannot resize transition"), ErrorMessage);
            } else {
                MoveTransitionCommand *command = new MoveTransitionCommand(this, m_dragItemInfo, info, false);
                m_commandStack->push(command);
            }

        }
        if (m_dragItem->parentItem() && m_dragItem->parentItem() != m_selectionGroup) {
            // Item was resized, rebuild group;
            AbstractGroupItem *group = static_cast <AbstractGroupItem *>(m_dragItem->parentItem());
            QList <QGraphicsItem *> children = group->childItems();
            m_document->clipManager()->removeGroup(group);
            scene()->destroyItemGroup(group);
            for (int i = 0; i < children.count(); i++) {
                children.at(i)->setSelected(true);
            }
            groupSelectedItems(false, true);
        }
        //m_document->renderer()->doRefresh();
    } else if (m_operationMode == RESIZEEND && m_dragItem->endPos() != m_dragItemInfo.endPos) {
        // resize end
        if (m_dragItem->type() == AVWIDGET) {
            ItemInfo resizeinfo = info;
            resizeinfo.track = m_document->tracksCount() - resizeinfo.track;
            bool success = m_document->renderer()->mltResizeClipEnd(resizeinfo, resizeinfo.endPos - resizeinfo.startPos);
            if (success) {
                QUndoCommand *resizeCommand = new QUndoCommand();
                resizeCommand->setText(i18n("Resize clip"));

                // Check if there is an automatic transition on that clip (lower track)
                Transition *tr = getTransitionItemAtEnd(m_dragItemInfo.endPos, m_dragItemInfo.track);
                if (tr && tr->isAutomatic()) {
                    ItemInfo trInfo = tr->info();
                    ItemInfo newTrInfo = trInfo;
                    newTrInfo.endPos = m_dragItem->endPos();
                    if (newTrInfo.endPos > newTrInfo.startPos) new MoveTransitionCommand(this, trInfo, newTrInfo, true, resizeCommand);
                }

                // Check if there is an automatic transition on that clip (upper track)
                tr = getTransitionItemAtEnd(m_dragItemInfo.endPos, m_dragItemInfo.track - 1);
                if (tr && tr->isAutomatic() && (m_document->tracksCount() - tr->transitionEndTrack()) == m_dragItemInfo.track) {
                    ItemInfo trInfo = tr->info();
                    ItemInfo newTrInfo = trInfo;
                    newTrInfo.endPos = m_dragItem->endPos();
                    ClipItem * upperClip = getClipItemAtEnd(m_dragItemInfo.endPos, m_dragItemInfo.track - 1);
                    if ((!upperClip || !upperClip->baseClip()->isTransparent()) && newTrInfo.endPos > newTrInfo.startPos) {
                        new MoveTransitionCommand(this, trInfo, newTrInfo, true, resizeCommand);
                    }
                }

                new ResizeClipCommand(this, m_dragItemInfo, info, false, resizeCommand);
                m_commandStack->push(resizeCommand);
                updateClipFade(static_cast <ClipItem *>(m_dragItem));
            } else {
                m_dragItem->resizeEnd((int) m_dragItemInfo.endPos.frames(m_document->fps()));
                emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
            }
        } else if (m_dragItem->type() == TRANSITIONWIDGET) {
            Transition *transition = static_cast <Transition *>(m_dragItem);
            if (!m_document->renderer()->mltMoveTransition(transition->transitionTag(), (int)(m_document->tracksCount() - m_dragItemInfo.track), (int)(m_document->tracksCount() - m_dragItemInfo.track), 0, m_dragItemInfo.startPos, m_dragItemInfo.endPos, info.startPos, info.endPos)) {
                // Cannot resize transition
                transition->resizeEnd((int) m_dragItemInfo.endPos.frames(m_document->fps()));
                emit displayMessage(i18n("Cannot resize transition"), ErrorMessage);
            } else {
                MoveTransitionCommand *command = new MoveTransitionCommand(this, m_dragItemInfo, info, false);
                m_commandStack->push(command);
            }
        }
        if (m_dragItem->parentItem() && m_dragItem->parentItem() != m_selectionGroup) {
            // Item was resized, rebuild group;
            AbstractGroupItem *group = static_cast <AbstractGroupItem *>(m_dragItem->parentItem());
            QList <QGraphicsItem *> children = group->childItems();
            m_document->clipManager()->removeGroup(group);
            scene()->destroyItemGroup(group);
            for (int i = 0; i < children.count(); i++) {
                children.at(i)->setSelected(true);
            }
            groupSelectedItems(false, true);
        }
        //m_document->renderer()->doRefresh();
    } else if (m_operationMode == FADEIN) {
        // resize fade in effect
        ClipItem * item = (ClipItem *) m_dragItem;
        int ix = item->hasEffect("volume", "fadein");
        if (ix != -1) {
            QDomElement oldeffect = item->effectAt(ix);
            int start = item->cropStart().frames(m_document->fps());
            int end = item->fadeIn();
            if (end == 0) {
                slotDeleteEffect(item, oldeffect);
            } else {
                end += start;
                QDomElement effect = oldeffect.cloneNode().toElement();
                EffectsList::setParameter(oldeffect, "in", QString::number(start));
                EffectsList::setParameter(oldeffect, "out", QString::number(end));
                slotUpdateClipEffect(item, effect, oldeffect, ix);
                emit clipItemSelected(item, ix);
            }
        } else if (item->fadeIn() != 0 && item->hasEffect("", "fade_from_black") == -1) {
            QDomElement effect = MainWindow::audioEffects.getEffectByTag("volume", "fadein").cloneNode().toElement();
            EffectsList::setParameter(effect, "out", QString::number(item->fadeIn()));
            slotAddEffect(effect, m_dragItem->startPos(), m_dragItem->track());
        }
        ix = item->hasEffect("volume", "fade_from_black");
        if (ix != -1) {
            QDomElement oldeffect = item->effectAt(ix);
            int start = item->cropStart().frames(m_document->fps());
            int end = item->fadeIn();
            if (end == 0) {
                slotDeleteEffect(item, oldeffect);
            } else {
                end += start;
                QDomElement effect = oldeffect.cloneNode().toElement();
                EffectsList::setParameter(oldeffect, "in", QString::number(start));
                EffectsList::setParameter(oldeffect, "out", QString::number(end));
                slotUpdateClipEffect(item, effect, oldeffect, ix);
                emit clipItemSelected(item, ix);
            }
        }
    } else if (m_operationMode == FADEOUT) {
        // resize fade in effect
        ClipItem * item = (ClipItem *) m_dragItem;
        int ix = item->hasEffect("volume", "fadeout");
        if (ix != -1) {
            QDomElement oldeffect = item->effectAt(ix);
            int end = (item->cropDuration() + item->cropStart()).frames(m_document->fps());
            int start = item->fadeOut();
            if (start == 0) {
                slotDeleteEffect(item, oldeffect);
            } else {
                start = end - start;
                QDomElement effect = oldeffect.cloneNode().toElement();
                EffectsList::setParameter(oldeffect, "in", QString::number(start));
                EffectsList::setParameter(oldeffect, "out", QString::number(end));
                // kDebug()<<"EDIT FADE OUT : "<<start<<"x"<<end;
                slotUpdateClipEffect(item, effect, oldeffect, ix);
                emit clipItemSelected(item, ix);
            }
        } else if (item->fadeOut() != 0 && item->hasEffect("", "fade_to_black") == -1) {
            QDomElement effect = MainWindow::audioEffects.getEffectByTag("volume", "fadeout").cloneNode().toElement();
            EffectsList::setParameter(effect, "in", QString::number(item->fadeOut()));
            EffectsList::setParameter(effect, "out", QString::number(0));
            slotAddEffect(effect, m_dragItem->startPos(), m_dragItem->track());
        }
        ix = item->hasEffect("brightness", "fade_to_black");
        if (ix != -1) {
            QDomElement oldeffect = item->effectAt(ix);
            int end = (item->cropDuration() + item->cropStart()).frames(m_document->fps());
            int start = item->fadeOut();
            if (start == 0) {
                slotDeleteEffect(item, oldeffect);
            } else {
                start = end - start;
                QDomElement effect = oldeffect.cloneNode().toElement();
                EffectsList::setParameter(oldeffect, "in", QString::number(start));
                EffectsList::setParameter(oldeffect, "out", QString::number(end));
                // kDebug()<<"EDIT FADE OUT : "<<start<<"x"<<end;
                slotUpdateClipEffect(item, effect, oldeffect, ix);
                emit clipItemSelected(item, ix);
            }
        }
    } else if (m_operationMode == KEYFRAME) {
        // update the MLT effect
        ClipItem * item = (ClipItem *) m_dragItem;
        QString previous = item->keyframes(item->selectedEffectIndex());
        item->updateKeyframeEffect();
        QString next = item->keyframes(item->selectedEffectIndex());
        EditKeyFrameCommand *command = new EditKeyFrameCommand(this, item->track(), item->startPos(), item->selectedEffectIndex(), previous, next, false);
        m_commandStack->push(command);
        updateEffect(m_document->tracksCount() - item->track(), item->startPos(), item->selectedEffect(), item->selectedEffectIndex());
        emit clipItemSelected(item, item->selectedEffectIndex());
    }
    if (m_dragItem && m_dragItem->type() == TRANSITIONWIDGET && m_dragItem->isSelected()) {
        // A transition is selected
        QPoint p;
        ClipItem *transitionClip = getClipItemAt(m_dragItemInfo.startPos, m_dragItemInfo.track);
        if (transitionClip && transitionClip->baseClip()) {
            QString size = transitionClip->baseClip()->getProperty("frame_size");
            p.setX(size.section('x', 0, 0).toInt());
            p.setY(size.section('x', 1, 1).toInt());
        }
        emit transitionItemSelected(static_cast <Transition *>(m_dragItem), getPreviousVideoTrack(m_dragItem->track()), p);
    } else emit transitionItemSelected(NULL);
    if (m_operationMode != NONE && m_operationMode != MOVE) setDocumentModified();
    m_operationMode = NONE;
}

void CustomTrackView::deleteClip(ItemInfo info)
{
    ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()), info.track);

    if (!item || m_document->renderer()->mltRemoveClip(m_document->tracksCount() - info.track, info.startPos) == false) {
        emit displayMessage(i18n("Error removing clip at %1 on track %2", m_document->timecode().getTimecodeFromFrames(info.startPos.frames(m_document->fps())), info.track), ErrorMessage);
        return;
    }
    if (item->isSelected()) emit clipItemSelected(NULL);
    item->baseClip()->removeReference();
    m_document->updateClip(item->baseClip()->getId());

    /*if (item->baseClip()->isTransparent()) {
        // also remove automatic transition
        Transition *tr = getTransitionItemAt(info.startPos, info.track);
        if (tr && tr->isAutomatic()) {
            m_document->renderer()->mltDeleteTransition(tr->transitionTag(), tr->transitionEndTrack(), m_document->tracksCount() - info.track, info.startPos, info.endPos, tr->toXML());
            scene()->removeItem(tr);
            delete tr;
        }
    }*/
    scene()->removeItem(item);
    m_waitingThumbs.removeAll(item);
    if (m_dragItem == item) m_dragItem = NULL;
    delete item;
    item = NULL;
    setDocumentModified();
    m_document->renderer()->doRefresh();
}

void CustomTrackView::deleteSelectedClips()
{
    resetSelectionGroup();
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() == 0) {
        emit displayMessage(i18n("Select clip to delete"), ErrorMessage);
        return;
    }
    scene()->clearSelection();
    QUndoCommand *deleteSelected = new QUndoCommand();
    deleteSelected->setText(i18n("Delete selected items"));
    bool resetGroup = false;

    // expand & destroy groups
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == GROUPWIDGET) {
            QList<QGraphicsItem *> children = itemList.at(i)->childItems();
            itemList += children;
            QList <ItemInfo> clipInfos;
            QList <ItemInfo> transitionInfos;
            GenTime currentPos = GenTime(m_cursorPos, m_document->fps());
            for (int j = 0; j < children.count(); j++) {
                if (children.at(j)->type() == AVWIDGET) {
                    AbstractClipItem *clip = static_cast <AbstractClipItem *>(children.at(j));
                    if (!clip->isItemLocked()) clipInfos.append(clip->info());
                } else if (children.at(j)->type() == TRANSITIONWIDGET) {
                    AbstractClipItem *clip = static_cast <AbstractClipItem *>(children.at(j));
                    if (!clip->isItemLocked()) transitionInfos.append(clip->info());
                }
            }
            if (clipInfos.count() > 0) {
                new GroupClipsCommand(this, clipInfos, transitionInfos, false, deleteSelected);
            }
        }
    }

    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            if (item->parentItem()) resetGroup = true;
            //kDebug()<<"// DELETE CLP AT: "<<item->info().startPos.frames(25);
            new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), item->info(), item->effectList(), true, true, deleteSelected);
            emit clipItemSelected(NULL);
        } else if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            Transition *item = static_cast <Transition *>(itemList.at(i));
            //kDebug()<<"// DELETE TRANS AT: "<<item->info().startPos.frames(25);
            if (item->parentItem()) resetGroup = true;
            new AddTransitionCommand(this, item->info(), item->transitionEndTrack(), item->toXML(), true, true, deleteSelected);
            emit transitionItemSelected(NULL);
        }
    }

    m_commandStack->push(deleteSelected);
}

void CustomTrackView::changeClipSpeed()
{
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() == 0) {
        emit displayMessage(i18n("Select clip to change speed"), ErrorMessage);
        return;
    }
    QUndoCommand *changeSelected = new QUndoCommand();
    changeSelected->setText("Edit clip speed");
    int count = 0;
    int percent = -1;
    bool ok;
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            ItemInfo info = item->info();
            if (percent == -1) percent = QInputDialog::getInteger(this, i18n("Edit Clip Speed"), i18n("New speed (percents)"), item->speed() * 100, 1, 10000, 1, &ok);
            if (!ok) break;
            double speed = (double) percent / 100.0;
            if (item->speed() != speed && (item->clipType() == VIDEO || item->clipType() == AV)) {
                count++;
                //new ChangeSpeedCommand(this, info, item->speed(), speed, item->clipProducer(), changeSelected);
            }
        }
    }
    if (count > 0) m_commandStack->push(changeSelected);
    else delete changeSelected;
}

void CustomTrackView::doChangeClipSpeed(ItemInfo info, const double speed, const double oldspeed, int strobe, const QString &id)
{
    DocClipBase *baseclip = m_document->clipManager()->getClipById(id);
    ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()), info.track);
    if (!item) {
        kDebug() << "ERROR: Cannot find clip for speed change";
        emit displayMessage(i18n("Cannot find clip for speed change"), ErrorMessage);
        return;
    }
    info.track = m_document->tracksCount() - item->track();
    int endPos = m_document->renderer()->mltChangeClipSpeed(info, speed, oldspeed, strobe, baseclip->producer());
    if (endPos >= 0) {
        item->setSpeed(speed, strobe);
        item->updateRectGeometry();
        if (item->cropDuration().frames(m_document->fps()) > endPos)
            item->AbstractClipItem::resizeEnd(info.startPos.frames(m_document->fps()) + endPos, speed);
        setDocumentModified();
    } else emit displayMessage(i18n("Invalid clip"), ErrorMessage);
}

void CustomTrackView::cutSelectedClips()
{
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    GenTime currentPos = GenTime(m_cursorPos, m_document->fps());
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            if (item->parentItem() && item->parentItem() != m_selectionGroup) {
                emit displayMessage(i18n("Cannot cut a clip in a group"), ErrorMessage);
            } else if (currentPos > item->startPos() && currentPos <  item->endPos()) {
                RazorClipCommand *command = new RazorClipCommand(this, item->info(), currentPos);
                m_commandStack->push(command);
            }
        }
    }
}

void CustomTrackView::groupClips(bool group)
{
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    QList <ItemInfo> clipInfos;
    QList <ItemInfo> transitionInfos;
    GenTime currentPos = GenTime(m_cursorPos, m_document->fps());

    // Expand groups
    int max = itemList.count();
    for (int i = 0; i < max; i++) {
        if (itemList.at(i)->type() == GROUPWIDGET) {
            itemList += itemList.at(i)->childItems();
        }
    }

    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            AbstractClipItem *clip = static_cast <AbstractClipItem *>(itemList.at(i));
            if (!clip->isItemLocked()) clipInfos.append(clip->info());
        } else if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            AbstractClipItem *clip = static_cast <AbstractClipItem *>(itemList.at(i));
            if (!clip->isItemLocked()) transitionInfos.append(clip->info());
        }
    }
    if (clipInfos.count() > 0) {
        GroupClipsCommand *command = new GroupClipsCommand(this, clipInfos, transitionInfos, group);
        m_commandStack->push(command);
    }
}

void CustomTrackView::doGroupClips(QList <ItemInfo> clipInfos, QList <ItemInfo> transitionInfos, bool group)
{
    resetSelectionGroup();
    m_scene->clearSelection();
    if (!group) {
        for (int i = 0; i < clipInfos.count(); i++) {
            ClipItem *clip = getClipItemAt(clipInfos.at(i).startPos, clipInfos.at(i).track);
            if (clip == NULL) continue;
            if (clip->parentItem() && clip->parentItem()->type() == GROUPWIDGET) {
                AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(clip->parentItem());
                m_document->clipManager()->removeGroup(grp);
                scene()->destroyItemGroup(grp);
            }
            clip->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
        }
        for (int i = 0; i < transitionInfos.count(); i++) {
            Transition *tr = getTransitionItemAt(transitionInfos.at(i).startPos, transitionInfos.at(i).track);
            if (tr == NULL) continue;
            if (tr->parentItem() && tr->parentItem()->type() == GROUPWIDGET) {
                AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(tr->parentItem());
                m_document->clipManager()->removeGroup(grp);
                scene()->destroyItemGroup(grp);
            }
            tr->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
        }
        setDocumentModified();
        return;
    }

    QList <QGraphicsItemGroup *> groups;
    for (int i = 0; i < clipInfos.count(); i++) {
        ClipItem *clip = getClipItemAt(clipInfos.at(i).startPos, clipInfos.at(i).track);
        if (clip) {
            clip->setSelected(true);
        }
    }
    for (int i = 0; i < transitionInfos.count(); i++) {
        Transition *clip = getTransitionItemAt(transitionInfos.at(i).startPos, transitionInfos.at(i).track);
        if (clip) {
            clip->setSelected(true);
        }
    }

    groupSelectedItems(false, true);
    setDocumentModified();
}

void CustomTrackView::addClip(QDomElement xml, const QString &clipId, ItemInfo info, EffectsList effects)
{
    DocClipBase *baseclip = m_document->clipManager()->getClipById(clipId);
    if (baseclip == NULL) {
        emit displayMessage(i18n("No clip copied"), ErrorMessage);
        return;
    }
    ClipItem *item = new ClipItem(baseclip, info, m_document->fps(), xml.attribute("speed", "1").toDouble(), xml.attribute("strobe", "1").toInt());
    item->setEffectList(effects);
    if (xml.hasAttribute("audio_only")) item->setAudioOnly(true);
    else if (xml.hasAttribute("video_only")) item->setVideoOnly(true);
    scene()->addItem(item);

    int tracknumber = m_document->tracksCount() - info.track - 1;
    bool isLocked = m_document->trackInfoAt(tracknumber).isLocked;
    if (isLocked) item->setItemLocked(true);

    baseclip->addReference();
    m_document->updateClip(baseclip->getId());
    info.track = m_document->tracksCount() - info.track;
    Mlt::Producer *prod;
    if (item->isAudioOnly()) prod = baseclip->audioProducer(info.track);
    else if (item->isVideoOnly()) prod = baseclip->videoProducer();
    else prod = baseclip->producer(info.track);
    m_document->renderer()->mltInsertClip(info, xml, prod);
    for (int i = 0; i < item->effectsCount(); i++) {
        m_document->renderer()->mltAddEffect(info.track, info.startPos, item->getEffectArgs(item->effectAt(i)), false);
    }
    setDocumentModified();
    m_document->renderer()->doRefresh();
    m_waitingThumbs.append(item);
    m_thumbsTimer.start();
}

void CustomTrackView::slotUpdateClip(const QString &clipId)
{
    QList<QGraphicsItem *> list = scene()->items();
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->type() == AVWIDGET) {
            clip = static_cast <ClipItem *>(list.at(i));
            if (clip->clipProducer() == clipId) {
                ItemInfo info = clip->info();
                info.track = m_document->tracksCount() - clip->track();
                m_document->renderer()->mltUpdateClip(info, clip->xml(), clip->baseClip()->producer());
                clip->refreshClip(true);
                clip->update();
            }
        }
    }
}

ClipItem *CustomTrackView::getClipItemAtEnd(GenTime pos, int track)
{
    int framepos = (int)(pos.frames(m_document->fps()));
    QList<QGraphicsItem *> list = scene()->items(QPointF(framepos - 1, track * m_tracksHeight + m_tracksHeight / 2));
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); i++) {
        if (list.at(i)->type() == AVWIDGET) {
            ClipItem *test = static_cast <ClipItem *>(list.at(i));
            if (test->endPos() == pos) clip = test;
            break;
        }
    }
    return clip;
}

ClipItem *CustomTrackView::getClipItemAtStart(GenTime pos, int track)
{
    QList<QGraphicsItem *> list = scene()->items(QPointF(pos.frames(m_document->fps()), track * m_tracksHeight + m_tracksHeight / 2));
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); i++) {
        if (list.at(i)->type() == AVWIDGET) {
            ClipItem *test = static_cast <ClipItem *>(list.at(i));
            if (test->startPos() == pos) clip = test;
            break;
        }
    }
    return clip;
}

ClipItem *CustomTrackView::getClipItemAt(int pos, int track)
{
    QList<QGraphicsItem *> list = scene()->items(QPointF(pos, track * m_tracksHeight + m_tracksHeight / 2));
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); i++) {
        if (list.at(i)->type() == AVWIDGET) {
            clip = static_cast <ClipItem *>(list.at(i));
            break;
        }
    }
    return clip;
}

ClipItem *CustomTrackView::getClipItemAt(GenTime pos, int track)
{
    int framepos = (int)(pos.frames(m_document->fps()));
    return getClipItemAt(framepos, track);
}

Transition *CustomTrackView::getTransitionItemAt(int pos, int track)
{
    QList<QGraphicsItem *> list = scene()->items(QPointF(pos, (track + 1) * m_tracksHeight));
    Transition *clip = NULL;
    for (int i = 0; i < list.size(); i++) {
        if (list.at(i)->type() == TRANSITIONWIDGET) {
            clip = static_cast <Transition *>(list.at(i));
            break;
        }
    }
    return clip;
}

Transition *CustomTrackView::getTransitionItemAt(GenTime pos, int track)
{
    return getTransitionItemAt(pos.frames(m_document->fps()), track);
}

Transition *CustomTrackView::getTransitionItemAtEnd(GenTime pos, int track)
{
    int framepos = (int)(pos.frames(m_document->fps()));
    QList<QGraphicsItem *> list = scene()->items(QPointF(framepos - 1, (track + 1) * m_tracksHeight));
    Transition *clip = NULL;
    for (int i = 0; i < list.size(); i++) {
        if (list.at(i)->type() == TRANSITIONWIDGET) {
            Transition *test = static_cast <Transition *>(list.at(i));
            if (test->endPos() == pos) clip = test;
            break;
        }
    }
    return clip;
}

Transition *CustomTrackView::getTransitionItemAtStart(GenTime pos, int track)
{
    QList<QGraphicsItem *> list = scene()->items(QPointF(pos.frames(m_document->fps()), (track + 1) * m_tracksHeight));
    Transition *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->type() == TRANSITIONWIDGET) {
            Transition *test = static_cast <Transition *>(list.at(i));
            if (test->startPos() == pos) clip = test;
            break;
        }
    }
    return clip;
}

void CustomTrackView::moveClip(const ItemInfo start, const ItemInfo end)
{
    if (m_selectionGroup) resetSelectionGroup(false);
    ClipItem *item = getClipItemAt((int) start.startPos.frames(m_document->fps()) + 1, start.track);
    if (!item) {
        emit displayMessage(i18n("Cannot move clip at time: %1 on track %2", m_document->timecode().getTimecodeFromFrames(start.startPos.frames(m_document->fps())), start.track), ErrorMessage);
        kDebug() << "----------------  ERROR, CANNOT find clip to move at.. ";
        return;
    }
    Mlt::Producer *prod;
    if (item->isAudioOnly()) prod = item->baseClip()->audioProducer(end.track);
    else if (item->isVideoOnly()) prod = item->baseClip()->videoProducer();
    else prod = item->baseClip()->producer(end.track);

    bool success = m_document->renderer()->mltMoveClip((int)(m_document->tracksCount() - start.track), (int)(m_document->tracksCount() - end.track), (int) start.startPos.frames(m_document->fps()), (int)end.startPos.frames(m_document->fps()), prod);
    if (success) {
        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);
        item->setPos((int) end.startPos.frames(m_document->fps()), (int)(end.track * m_tracksHeight + 1));

        int tracknumber = m_document->tracksCount() - end.track - 1;
        bool isLocked = m_document->trackInfoAt(tracknumber).isLocked;
        m_scene->clearSelection();
        if (isLocked) item->setItemLocked(true);
        else {
            if (item->isItemLocked()) item->setItemLocked(false);
            item->setSelected(true);
        }
        if (item->baseClip()->isTransparent()) {
            // Also move automatic transition
            Transition *tr = getTransitionItemAt(start.startPos, start.track);
            if (tr && tr->isAutomatic()) {
                tr->updateTransitionEndTrack(getPreviousVideoTrack(end.track));
                m_document->renderer()->mltMoveTransition(tr->transitionTag(), m_document->tracksCount() - start.track, m_document->tracksCount() - end.track, tr->transitionEndTrack(), start.startPos, start.endPos, end.startPos, end.endPos);
                tr->setPos((int) end.startPos.frames(m_document->fps()), (int)(end.track * m_tracksHeight + 1));
            }
        }
        KdenliveSettings::setSnaptopoints(snap);
        setDocumentModified();
    } else {
        // undo last move and emit error message
        emit displayMessage(i18n("Cannot move clip to position %1", m_document->timecode().getTimecodeFromFrames(end.startPos.frames(m_document->fps()))), ErrorMessage);
    }
    m_document->renderer()->doRefresh();
    //kDebug() << " // MOVED CLIP TO: " << end.startPos.frames(25) << ", ITEM START: " << item->startPos().frames(25);
}

void CustomTrackView::moveGroup(QList <ItemInfo> startClip, QList <ItemInfo> startTransition, const GenTime offset, const int trackOffset, bool reverseMove)
{
    // Group Items
    /*kDebug() << "//GRP MOVE, REVERS:" << reverseMove;
    kDebug() << "// GROUP MOV; OFFSET: " << offset.frames(25) << ", TK OFF: " << trackOffset;*/
    resetSelectionGroup();
    m_scene->clearSelection();

    for (int i = 0; i < startClip.count(); i++) {
        if (reverseMove) {
            startClip[i].startPos = startClip.at(i).startPos - offset;
            startClip[i].track = startClip.at(i).track - trackOffset;
        }
        //kDebug()<<"//LKING FR CLIP AT:"<<startClip.at(i).startPos.frames(25)<<", TK:"<<startClip.at(i).track;
        ClipItem *clip = getClipItemAt(startClip.at(i).startPos, startClip.at(i).track);
        if (clip) {
            clip->setItemLocked(false);
            if (clip->parentItem()) clip->parentItem()->setSelected(true);
            else clip->setSelected(true);
            m_document->renderer()->mltRemoveClip(m_document->tracksCount() - startClip.at(i).track, startClip.at(i).startPos);
        } else kDebug() << "//MISSING CLIP AT: " << startClip.at(i).startPos.frames(25);
    }
    for (int i = 0; i < startTransition.count(); i++) {
        if (reverseMove) {
            startTransition[i].startPos = startTransition.at(i).startPos - offset;
            startTransition[i].track = startTransition.at(i).track - trackOffset;
        }
        Transition *tr = getTransitionItemAt(startTransition.at(i).startPos, startTransition.at(i).track);
        if (tr) {
            tr->setItemLocked(false);
            if (tr->parentItem()) tr->parentItem()->setSelected(true);
            else tr->setSelected(true);
            m_document->renderer()->mltDeleteTransition(tr->transitionTag(), tr->transitionEndTrack(), m_document->tracksCount() - startTransition.at(i).track, startTransition.at(i).startPos, startTransition.at(i).endPos, tr->toXML());
        } else kDebug() << "//MISSING TRANSITION AT: " << startTransition.at(i).startPos.frames(25);
    }
    groupSelectedItems(true);
    if (m_selectionGroup) {
        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);

        m_selectionGroup->moveBy(offset.frames(m_document->fps()), trackOffset *(qreal) m_tracksHeight);

        kDebug() << "%% GRP NEW POS: " << m_selectionGroup->scenePos().x();

        QList<QGraphicsItem *> children = m_selectionGroup->childItems();
        // Expand groups
        int max = children.count();
        for (int i = 0; i < max; i++) {
            if (children.at(i)->type() == GROUPWIDGET) {
                children += children.at(i)->childItems();
            }
        }
        kDebug() << "// GRP MOVE; FOUND CHILDREN:" << children.count();

        for (int i = 0; i < children.count(); i++) {
            // re-add items in correct place
            if (children.at(i)->type() != AVWIDGET && children.at(i)->type() != TRANSITIONWIDGET) continue;
            AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
            item->updateItem();
            ItemInfo info = item->info();
            int tracknumber = m_document->tracksCount() - info.track - 1;
            bool isLocked = m_document->trackInfoAt(tracknumber).isLocked;
            if (isLocked) item->setItemLocked(true);
            else if (item->isItemLocked()) item->setItemLocked(false);

            if (item->type() == AVWIDGET) {
                ClipItem *clip = static_cast <ClipItem*>(item);
                info.track = m_document->tracksCount() - info.track;
                Mlt::Producer *prod;
                if (clip->isAudioOnly()) prod = clip->baseClip()->audioProducer(info.track);
                else if (clip->isVideoOnly()) prod = clip->baseClip()->videoProducer();
                else prod = clip->baseClip()->producer(info.track);
                m_document->renderer()->mltInsertClip(info, clip->xml(), prod);
                kDebug() << "// inserting new clp: " << info.startPos.frames(25);
            } else if (item->type() == TRANSITIONWIDGET) {
                Transition *tr = static_cast <Transition*>(item);
                int newTrack = tr->transitionEndTrack();
                kDebug() << "/// TRANSITION CURR TRK: " << newTrack;
                if (!tr->forcedTrack()) {
                    newTrack += trackOffset;
                    if (newTrack < 0 || newTrack > m_document->tracksCount()) newTrack = getPreviousVideoTrack(info.track);
                }
                tr->updateTransitionEndTrack(newTrack);
                kDebug() << "/// TRANSITION UPDATED TRK: " << newTrack;
                m_document->renderer()->mltAddTransition(tr->transitionTag(), newTrack, m_document->tracksCount() - info.track, info.startPos, info.endPos, tr->toXML());
            }
        }
        KdenliveSettings::setSnaptopoints(snap);
        m_document->renderer()->doRefresh();
    } else kDebug() << "///////// WARNING; NO GROUP TO MOVE";
}

void CustomTrackView::moveTransition(const ItemInfo start, const ItemInfo end)
{
    Transition *item = getTransitionItemAt(start.startPos, start.track);
    if (!item) {
        emit displayMessage(i18n("Cannot move transition at time: %1 on track %2", m_document->timecode().getTimecodeFromFrames(start.startPos.frames(m_document->fps())), start.track), ErrorMessage);
        kDebug() << "----------------  ERROR, CANNOT find transition to move... ";// << startPos.x() * m_scale * FRAME_SIZE + 1 << ", " << startPos.y() * m_tracksHeight + m_tracksHeight / 2;
        return;
    }
    //kDebug() << "----------------  Move TRANSITION FROM: " << startPos.x() << ", END:" << endPos.x() << ",TRACKS: " << oldtrack << " TO " << newtrack;
    bool snap = KdenliveSettings::snaptopoints();
    KdenliveSettings::setSnaptopoints(false);
    //kDebug()<<"///  RESIZE TRANS START: ("<< startPos.x()<<"x"<< startPos.y()<<") / ("<<endPos.x()<<"x"<< endPos.y()<<")";
    if (end.endPos - end.startPos == start.endPos - start.startPos) {
        // Transition was moved
        item->setPos((int) end.startPos.frames(m_document->fps()), (end.track) * m_tracksHeight + 1);
    } else if (end.endPos == start.endPos) {
        // Transition start resize
        item->resizeStart((int) end.startPos.frames(m_document->fps()));
    } else if (end.startPos == start.startPos) {
        // Transition end resize;
        item->resizeEnd((int) end.endPos.frames(m_document->fps()));
    } else {
        // Move & resize
        item->setPos((int) end.startPos.frames(m_document->fps()), (end.track) * m_tracksHeight + 1);
        item->resizeStart((int) end.startPos.frames(m_document->fps()));
        item->resizeEnd((int) end.endPos.frames(m_document->fps()));
    }
    //item->moveTransition(GenTime((int) (endPos.x() - startPos.x()), m_document->fps()));
    KdenliveSettings::setSnaptopoints(snap);
    item->updateTransitionEndTrack(getPreviousVideoTrack(end.track));
    m_document->renderer()->mltMoveTransition(item->transitionTag(), m_document->tracksCount() - start.track, m_document->tracksCount() - end.track, item->transitionEndTrack(), start.startPos, start.endPos, end.startPos, end.endPos);
    if (m_dragItem && m_dragItem == item) {
        QPoint p;
        ClipItem *transitionClip = getClipItemAt(item->startPos(), item->track());
        if (transitionClip && transitionClip->baseClip()) {
            QString size = transitionClip->baseClip()->getProperty("frame_size");
            p.setX(size.section('x', 0, 0).toInt());
            p.setY(size.section('x', 1, 1).toInt());
        }
        emit transitionItemSelected(item, getPreviousVideoTrack(item->track()), p);
    }
    m_document->renderer()->doRefresh();
}

void CustomTrackView::resizeClip(const ItemInfo start, const ItemInfo end)
{
    bool resizeClipStart = true;
    if (start.startPos == end.startPos) resizeClipStart = false;
    /*if (resizeClipStart) offset = 1;
    else offset = -1;*/
    ClipItem *item = getClipItemAt((int)(start.startPos.frames(m_document->fps())), start.track);
    if (!item) {
        emit displayMessage(i18n("Cannot move clip at time: %1 on track %2", m_document->timecode().getTimecodeFromFrames(start.startPos.frames(m_document->fps())), start.track), ErrorMessage);
        kDebug() << "----------------  ERROR, CANNOT find clip to resize at... "; // << startPos;
        return;
    }
    if (item->parentItem()) {
        // Item is part of a group, reset group
        resetSelectionGroup();
    }
    bool snap = KdenliveSettings::snaptopoints();
    KdenliveSettings::setSnaptopoints(false);
    if (resizeClipStart && start.startPos != end.startPos) {
        ItemInfo clipinfo = item->info();
        clipinfo.track = m_document->tracksCount() - clipinfo.track;
        bool success = m_document->renderer()->mltResizeClipStart(clipinfo, end.startPos - item->startPos());
        if (success) {
            kDebug() << "RESIZE CLP STRAT TO:" << end.startPos.frames(m_document->fps()) << ", OLD ST: " << start.startPos.frames(25);
            item->resizeStart((int) end.startPos.frames(m_document->fps()));
            updateClipFade(item);
        } else emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
    } else if (!resizeClipStart) {
        ItemInfo clipinfo = item->info();
        clipinfo.track = m_document->tracksCount() - clipinfo.track;
        bool success = m_document->renderer()->mltResizeClipEnd(clipinfo, end.endPos - clipinfo.startPos);
        if (success) {
            item->resizeEnd((int) end.endPos.frames(m_document->fps()));
            updateClipFade(item);
        } else emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
    }
    if (end.cropStart != start.cropStart) {
        kDebug() << "// RESIZE CROP, DIFF: " << (end.cropStart - start.cropStart).frames(25);
        ItemInfo clipinfo = end;
        clipinfo.track = m_document->tracksCount() - end.track;
        bool success = m_document->renderer()->mltResizeClipCrop(clipinfo, end.cropStart - start.cropStart);
        if (success) {
            item->setCropStart(end.cropStart);
            item->resetThumbs(true);
        }
    }
    m_document->renderer()->doRefresh();
    KdenliveSettings::setSnaptopoints(snap);
}

void CustomTrackView::updateClipFade(ClipItem * item)
{
    int end = item->fadeIn();
    if (end != 0) {
        // there is a fade in effect
        int effectPos = item->hasEffect("volume", "fadein");
        if (effectPos != -1) {
            QDomElement oldeffect = item->effectAt(effectPos);
            int start = item->cropStart().frames(m_document->fps());
            int max = item->cropDuration().frames(m_document->fps());
            if (end > max) {
                // Make sure the fade effect is not longer than the clip
                item->setFadeIn(max);
                end = item->fadeIn();
            }
            end += start;
            EffectsList::setParameter(oldeffect, "in", QString::number(start));
            EffectsList::setParameter(oldeffect, "out", QString::number(end));
            if (!m_document->renderer()->mltEditEffect(m_document->tracksCount() - item->track(), item->startPos(), item->getEffectArgs(oldeffect)))
                emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
            // if fade effect is displayed, update the effect edit widget with new clip duration
            if (item->isSelected() && effectPos == item->selectedEffectIndex()) emit clipItemSelected(item, effectPos);
        }
        effectPos = item->hasEffect("brightness", "fade_from_black");
        if (effectPos != -1) {
            QDomElement oldeffect = item->effectAt(effectPos);
            int start = item->cropStart().frames(m_document->fps());
            int max = item->cropDuration().frames(m_document->fps());
            if (end > max) {
                // Make sure the fade effect is not longer than the clip
                item->setFadeIn(max);
                end = item->fadeIn();
            }
            end += start;
            EffectsList::setParameter(oldeffect, "in", QString::number(start));
            EffectsList::setParameter(oldeffect, "out", QString::number(end));
            if (!m_document->renderer()->mltEditEffect(m_document->tracksCount() - item->track(), item->startPos(), item->getEffectArgs(oldeffect)))
                emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
            // if fade effect is displayed, update the effect edit widget with new clip duration
            if (item->isSelected() && effectPos == item->selectedEffectIndex()) emit clipItemSelected(item, effectPos);
        }
    }
    int start = item->fadeOut();
    if (start != 0) {
        // there is a fade out effect
        int effectPos = item->hasEffect("volume", "fadeout");
        if (effectPos != -1) {
            QDomElement oldeffect = item->effectAt(effectPos);
            int max = item->cropDuration().frames(m_document->fps());
            int end = max + item->cropStart().frames(m_document->fps());
            if (start > max) {
                // Make sure the fade effect is not longer than the clip
                item->setFadeOut(max);
                start = item->fadeOut();
            }
            start = end - start;
            EffectsList::setParameter(oldeffect, "in", QString::number(start));
            EffectsList::setParameter(oldeffect, "out", QString::number(end));
            if (!m_document->renderer()->mltEditEffect(m_document->tracksCount() - item->track(), item->startPos(), item->getEffectArgs(oldeffect)))
                emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
            // if fade effect is displayed, update the effect edit widget with new clip duration
            if (item->isSelected() && effectPos == item->selectedEffectIndex()) emit clipItemSelected(item, effectPos);
        }
        effectPos = item->hasEffect("brightness", "fade_to_black");
        if (effectPos != -1) {
            QDomElement oldeffect = item->effectAt(effectPos);
            int max = item->cropDuration().frames(m_document->fps());
            int end = max + item->cropStart().frames(m_document->fps());
            if (start > max) {
                // Make sure the fade effect is not longer than the clip
                item->setFadeOut(max);
                start = item->fadeOut();
            }
            start = end - start;
            EffectsList::setParameter(oldeffect, "in", QString::number(start));
            EffectsList::setParameter(oldeffect, "out", QString::number(end));
            if (!m_document->renderer()->mltEditEffect(m_document->tracksCount() - item->track(), item->startPos(), item->getEffectArgs(oldeffect)))
                emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
            // if fade effect is displayed, update the effect edit widget with new clip duration
            if (item->isSelected() && effectPos == item->selectedEffectIndex()) emit clipItemSelected(item, effectPos);
        }
    }
}

double CustomTrackView::getSnapPointForPos(double pos)
{
    return m_scene->getSnapPointForPos(pos, KdenliveSettings::snaptopoints());
}

void CustomTrackView::updateSnapPoints(AbstractClipItem *selected, QList <GenTime> offsetList, bool skipSelectedItems)
{
    QList <GenTime> snaps;
    if (selected && offsetList.isEmpty()) offsetList.append(selected->cropDuration());
    QList<QGraphicsItem *> itemList = items();
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i) == selected) continue;
        if (skipSelectedItems && itemList.at(i)->isSelected()) continue;
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            GenTime start = item->startPos();
            GenTime end = item->endPos();
            snaps.append(start);
            snaps.append(end);
            if (!offsetList.isEmpty()) {
                for (int j = 0; j < offsetList.size(); j++) {
                    if (start > offsetList.at(j)) snaps.append(start - offsetList.at(j));
                    if (end > offsetList.at(j)) snaps.append(end - offsetList.at(j));
                }
            }
            // Add clip markers
            QList < GenTime > markers = item->snapMarkers();
            for (int j = 0; j < markers.size(); ++j) {
                GenTime t = markers.at(j);
                snaps.append(t);
                if (!offsetList.isEmpty()) {
                    for (int k = 0; k < offsetList.size(); k++) {
                        if (t > offsetList.at(k)) snaps.append(t - offsetList.at(k));
                    }
                }
            }
        } else if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            Transition *transition = static_cast <Transition*>(itemList.at(i));
            GenTime start = transition->startPos();
            GenTime end = transition->endPos();
            snaps.append(start);
            snaps.append(end);
            if (!offsetList.isEmpty()) {
                for (int j = 0; j < offsetList.size(); j++) {
                    if (start > offsetList.at(j)) snaps.append(start - offsetList.at(j));
                    if (end > offsetList.at(j)) snaps.append(end - offsetList.at(j));
                }
            }
        }
    }

    // add cursor position
    GenTime pos = GenTime(m_cursorPos, m_document->fps());
    snaps.append(pos);
    if (!offsetList.isEmpty()) {
        for (int j = 0; j < offsetList.size(); j++) {
            snaps.append(pos - offsetList.at(j));
        }
    }

    // add guides
    for (int i = 0; i < m_guides.count(); i++) {
        snaps.append(m_guides.at(i)->position());
        if (!offsetList.isEmpty()) {
            for (int j = 0; j < offsetList.size(); j++) {
                snaps.append(m_guides.at(i)->position() - offsetList.at(j));
            }
        }
    }

    qSort(snaps);
    m_scene->setSnapList(snaps);
    //for (int i = 0; i < m_snapPoints.size(); ++i)
    //    kDebug() << "SNAP POINT: " << m_snapPoints.at(i).frames(25);
}

void CustomTrackView::slotSeekToPreviousSnap()
{
    updateSnapPoints(NULL);
    GenTime res = m_scene->previousSnapPoint(GenTime(m_cursorPos, m_document->fps()));
    setCursorPos((int) res.frames(m_document->fps()));
    checkScrolling();
}

void CustomTrackView::slotSeekToNextSnap()
{
    updateSnapPoints(NULL);
    GenTime res = m_scene->nextSnapPoint(GenTime(m_cursorPos, m_document->fps()));
    setCursorPos((int) res.frames(m_document->fps()));
    checkScrolling();
}

void CustomTrackView::clipStart()
{
    ClipItem *item = getMainActiveClip();
    if (item != NULL) {
        setCursorPos((int) item->startPos().frames(m_document->fps()));
        checkScrolling();
    }
}

void CustomTrackView::clipEnd()
{
    ClipItem *item = getMainActiveClip();
    if (item != NULL) {
        setCursorPos((int) item->endPos().frames(m_document->fps()) - 1);
        checkScrolling();
    }
}

void CustomTrackView::slotAddClipMarker(const QString &id, GenTime t, QString c)
{
    QString oldcomment = m_document->clipManager()->getClipById(id)->markerComment(t);
    AddMarkerCommand *command = new AddMarkerCommand(this, oldcomment, c, id, t);
    m_commandStack->push(command);
}

void CustomTrackView::slotDeleteClipMarker(const QString &comment, const QString &id, const GenTime &position)
{
    AddMarkerCommand *command = new AddMarkerCommand(this, comment, QString(), id, position);
    m_commandStack->push(command);
}

void CustomTrackView::slotDeleteAllClipMarkers(const QString &id)
{
    DocClipBase *base = m_document->clipManager()->getClipById(id);
    QList <CommentedTime> markers = base->commentedSnapMarkers();

    if (markers.isEmpty()) {
        emit displayMessage(i18n("Clip has no markers"), ErrorMessage);
        return;
    }
    QUndoCommand *deleteMarkers = new QUndoCommand();
    deleteMarkers->setText("Delete clip markers");

    for (int i = 0; i < markers.size(); i++) {
        new AddMarkerCommand(this, markers.at(i).comment(), QString(), id, markers.at(i).time(), deleteMarkers);
    }
    m_commandStack->push(deleteMarkers);
}

void CustomTrackView::addMarker(const QString &id, const GenTime &pos, const QString comment)
{
    DocClipBase *base = m_document->clipManager()->getClipById(id);
    if (!comment.isEmpty()) base->addSnapMarker(pos, comment);
    else base->deleteSnapMarker(pos);
    setDocumentModified();
    viewport()->update();
}

int CustomTrackView::hasGuide(int pos, int offset)
{
    for (int i = 0; i < m_guides.count(); i++) {
        int guidePos = m_guides.at(i)->position().frames(m_document->fps());
        if (qAbs(guidePos - pos) <= offset) return guidePos;
        else if (guidePos > pos) return -1;
    }
    return -1;
}

void CustomTrackView::editGuide(const GenTime oldPos, const GenTime pos, const QString &comment)
{
    if (oldPos > GenTime() && pos > GenTime()) {
        // move guide
        for (int i = 0; i < m_guides.count(); i++) {
            if (m_guides.at(i)->position() == oldPos) {
                Guide *item = m_guides.at(i);
                item->updateGuide(pos, comment);
                break;
            }
        }
    } else if (pos > GenTime()) addGuide(pos, comment);
    else {
        // remove guide
        bool found = false;
        for (int i = 0; i < m_guides.count(); i++) {
            if (m_guides.at(i)->position() == oldPos) {
                delete m_guides.takeAt(i);
                found = true;
                break;
            }
        }
        if (!found) emit displayMessage(i18n("No guide at cursor time"), ErrorMessage);
    }
    qSort(m_guides.begin(), m_guides.end(), sortGuidesList);
    m_document->syncGuides(m_guides);
}

bool CustomTrackView::addGuide(const GenTime pos, const QString &comment)
{
    for (int i = 0; i < m_guides.count(); i++) {
        if (m_guides.at(i)->position() == pos) {
            emit displayMessage(i18n("A guide already exists at position %1", m_document->timecode().getTimecodeFromFrames(pos.frames(m_document->fps()))), ErrorMessage);
            return false;
        }
    }
    Guide *g = new Guide(this, pos, comment, m_document->fps(), m_tracksHeight * m_document->tracksCount());
    scene()->addItem(g);
    m_guides.append(g);
    qSort(m_guides.begin(), m_guides.end(), sortGuidesList);
    m_document->syncGuides(m_guides);
    return true;
}

void CustomTrackView::slotAddGuide()
{
    CommentedTime marker(GenTime(m_cursorPos, m_document->fps()), i18n("Guide"));
    MarkerDialog d(NULL, marker, m_document->timecode(), i18n("Add Guide"), this);
    if (d.exec() != QDialog::Accepted) return;
    if (addGuide(d.newMarker().time(), d.newMarker().comment())) {
        EditGuideCommand *command = new EditGuideCommand(this, GenTime(), QString(), d.newMarker().time(), d.newMarker().comment(), false);
        m_commandStack->push(command);
    }
}

void CustomTrackView::slotEditGuide(int guidePos)
{
    GenTime pos;
    if (guidePos == -1) pos = GenTime(m_cursorPos, m_document->fps());
    else pos = GenTime(guidePos, m_document->fps());
    bool found = false;
    for (int i = 0; i < m_guides.count(); i++) {
        if (m_guides.at(i)->position() == pos) {
            slotEditGuide(m_guides.at(i)->info());
            found = true;
            break;
        }
    }
    if (!found) emit displayMessage(i18n("No guide at cursor time"), ErrorMessage);
}

void CustomTrackView::slotEditGuide(CommentedTime guide)
{
    MarkerDialog d(NULL, guide, m_document->timecode(), i18n("Edit Guide"), this);
    if (d.exec() == QDialog::Accepted) {
        EditGuideCommand *command = new EditGuideCommand(this, guide.time(), guide.comment(), d.newMarker().time(), d.newMarker().comment(), true);
        m_commandStack->push(command);
    }
}


void CustomTrackView::slotEditTimeLineGuide()
{
    if (m_dragGuide == NULL) return;
    CommentedTime guide = m_dragGuide->info();
    MarkerDialog d(NULL, guide, m_document->timecode(), i18n("Edit Guide"), this);
    if (d.exec() == QDialog::Accepted) {
        EditGuideCommand *command = new EditGuideCommand(this, guide.time(), guide.comment(), d.newMarker().time(), d.newMarker().comment(), true);
        m_commandStack->push(command);
    }
}

void CustomTrackView::slotDeleteGuide(int guidePos)
{
    GenTime pos;
    if (guidePos == -1) pos = GenTime(m_cursorPos, m_document->fps());
    else pos = GenTime(guidePos, m_document->fps());
    bool found = false;
    for (int i = 0; i < m_guides.count(); i++) {
        if (m_guides.at(i)->position() == pos) {
            EditGuideCommand *command = new EditGuideCommand(this, m_guides.at(i)->position(), m_guides.at(i)->label(), GenTime(), QString(), true);
            m_commandStack->push(command);
            found = true;
            break;
        }
    }
    if (!found) emit displayMessage(i18n("No guide at cursor time"), ErrorMessage);
}


void CustomTrackView::slotDeleteTimeLineGuide()
{
    if (m_dragGuide == NULL) return;
    EditGuideCommand *command = new EditGuideCommand(this, m_dragGuide->position(), m_dragGuide->label(), GenTime(), QString(), true);
    m_commandStack->push(command);
}


void CustomTrackView::slotDeleteAllGuides()
{
    QUndoCommand *deleteAll = new QUndoCommand();
    deleteAll->setText("Delete all guides");
    for (int i = 0; i < m_guides.count(); i++) {
        new EditGuideCommand(this, m_guides.at(i)->position(), m_guides.at(i)->label(), GenTime(), QString(), true, deleteAll);
    }
    m_commandStack->push(deleteAll);
}

void CustomTrackView::setTool(PROJECTTOOL tool)
{
    m_tool = tool;
}

void CustomTrackView::setScale(double scaleFactor, double verticalScale)
{
    QMatrix matrix;
    matrix = matrix.scale(scaleFactor, verticalScale);
    m_scene->setScale(scaleFactor, verticalScale);
    m_animationTimer->stop();
    delete m_visualTip;
    m_visualTip = NULL;
    delete m_animation;
    m_animation = NULL;
    double verticalPos = mapToScene(QPoint(0, viewport()->height() / 2)).y();
    setMatrix(matrix);
    int diff = sceneRect().width() - m_projectDuration;
    if (diff * matrix.m11() < 50) {
        if (matrix.m11() < 0.4) setSceneRect(0, 0, (m_projectDuration + 100 / matrix.m11()), sceneRect().height());
        else setSceneRect(0, 0, (m_projectDuration + 300), sceneRect().height());
    }
    centerOn(QPointF(cursorPos(), verticalPos));
}

void CustomTrackView::slotRefreshGuides()
{
    if (KdenliveSettings::showmarkers()) {
        kDebug() << "// refresh GUIDES";
        for (int i = 0; i < m_guides.count(); i++) {
            m_guides.at(i)->update();
        }
    }
}

void CustomTrackView::drawBackground(QPainter * painter, const QRectF &rect)
{
    QRectF r = rect;
    r.setWidth(r.width() + 1);
    painter->setClipRect(r);
    painter->drawLine(r.left(), 0, r.right(), 0);
    uint max = m_document->tracksCount();
    KColorScheme scheme(palette().currentColorGroup(), KColorScheme::Window);
    QColor lockedColor = scheme.background(KColorScheme::NegativeBackground).color();
    QColor audioColor = palette().alternateBase().color();
    QColor base = scheme.background(KColorScheme::NormalBackground).color();
    for (uint i = 0; i < max; i++) {
        if (m_document->trackInfoAt(max - i - 1).isLocked == true) painter->fillRect(r.left(), m_tracksHeight * i + 1, r.right() - r.left() + 1, m_tracksHeight - 1, QBrush(lockedColor));
        else if (m_document->trackInfoAt(max - i - 1).type == AUDIOTRACK) painter->fillRect(r.left(), m_tracksHeight * i + 1, r.right() - r.left() + 1, m_tracksHeight - 1, QBrush(audioColor));
        painter->drawLine(r.left(), m_tracksHeight *(i + 1), r.right(), m_tracksHeight *(i + 1));
    }
    int lowerLimit = m_tracksHeight * m_document->tracksCount() + 1;
    if (height() > lowerLimit)
        painter->fillRect(QRectF(r.left(), lowerLimit, r.width(), height() - lowerLimit), QBrush(base));
}

bool CustomTrackView::findString(const QString &text)
{
    QString marker;
    for (int i = 0; i < m_searchPoints.size(); ++i) {
        marker = m_searchPoints.at(i).comment();
        if (marker.contains(text, Qt::CaseInsensitive)) {
            setCursorPos(m_searchPoints.at(i).time().frames(m_document->fps()), true);
            int vert = verticalScrollBar()->value();
            int hor = cursorPos();
            ensureVisible(hor, vert + 10, 2, 2, 50, 0);
            m_findIndex = i;
            return true;
        }
    }
    return false;
}

bool CustomTrackView::findNextString(const QString &text)
{
    QString marker;
    for (int i = m_findIndex + 1; i < m_searchPoints.size(); ++i) {
        marker = m_searchPoints.at(i).comment();
        if (marker.contains(text, Qt::CaseInsensitive)) {
            setCursorPos(m_searchPoints.at(i).time().frames(m_document->fps()), true);
            int vert = verticalScrollBar()->value();
            int hor = cursorPos();
            ensureVisible(hor, vert + 10, 2, 2, 50, 0);
            m_findIndex = i;
            return true;
        }
    }
    m_findIndex = -1;
    return false;
}

void CustomTrackView::initSearchStrings()
{
    m_searchPoints.clear();
    QList<QGraphicsItem *> itemList = items();
    for (int i = 0; i < itemList.count(); i++) {
        // parse all clip names
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            GenTime start = item->startPos();
            CommentedTime t(start, item->clipName());
            m_searchPoints.append(t);
            // add all clip markers
            QList < CommentedTime > markers = item->commentedSnapMarkers();
            m_searchPoints += markers;
        }
    }

    // add guides
    for (int i = 0; i < m_guides.count(); i++) {
        m_searchPoints.append(m_guides.at(i)->info());
    }

    qSort(m_searchPoints);
}

void CustomTrackView::clearSearchStrings()
{
    m_searchPoints.clear();
    m_findIndex = 0;
}

void CustomTrackView::copyClip()
{
    qDeleteAll(m_copiedItems);
    m_copiedItems.clear();
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() == 0) {
        emit displayMessage(i18n("Select a clip before copying"), ErrorMessage);
        return;
    }
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *dup = static_cast <ClipItem *>(itemList.at(i));
            m_copiedItems.append(dup->clone(dup->info()));
        } else if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            Transition *dup = static_cast <Transition *>(itemList.at(i));
            m_copiedItems.append(dup->clone());
        }
    }
}

bool CustomTrackView::canBePastedTo(ItemInfo info, int type) const
{
    QRectF rect((double) info.startPos.frames(m_document->fps()), (double)(info.track * m_tracksHeight + 1), (double)(info.endPos - info.startPos).frames(m_document->fps()), (double)(m_tracksHeight - 1));
    QList<QGraphicsItem *> collisions = scene()->items(rect, Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisions.count(); i++) {
        if (collisions.at(i)->type() == type) return false;
    }
    return true;
}

bool CustomTrackView::canBePasted(QList<AbstractClipItem *> items, GenTime offset, int trackOffset) const
{
    for (int i = 0; i < items.count(); i++) {
        ItemInfo info = items.at(i)->info();
        info.startPos += offset;
        info.endPos += offset;
        info.track += trackOffset;
        if (!canBePastedTo(info, items.at(i)->type())) return false;
    }
    return true;
}

bool CustomTrackView::canBeMoved(QList<AbstractClipItem *> items, GenTime offset, int trackOffset) const
{
    QPainterPath movePath;
    movePath.moveTo(0, 0);

    for (int i = 0; i < items.count(); i++) {
        ItemInfo info = items.at(i)->info();
        info.startPos = info.startPos + offset;
        info.endPos = info.endPos + offset;
        info.track = info.track + trackOffset;
        if (info.startPos < GenTime()) {
            // No clip should go below 0
            return false;
        }
        QRectF rect((double) info.startPos.frames(m_document->fps()), (double)(info.track * m_tracksHeight + 1), (double)(info.endPos - info.startPos).frames(m_document->fps()), (double)(m_tracksHeight - 1));
        movePath.addRect(rect);
    }
    QList<QGraphicsItem *> collisions = scene()->items(movePath, Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisions.count(); i++) {
        if ((collisions.at(i)->type() == AVWIDGET || collisions.at(i)->type() == TRANSITIONWIDGET) && !items.contains(static_cast <AbstractClipItem *>(collisions.at(i)))) {
            kDebug() << "  ////////////   CLIP COLLISION, MOVE NOT ALLOWED";
            return false;
        }
    }
    return true;
}

void CustomTrackView::pasteClip()
{
    if (m_copiedItems.count() == 0) {
        emit displayMessage(i18n("No clip copied"), ErrorMessage);
        return;
    }
    QPoint position;
    if (m_menuPosition.isNull()) {
        position = mapFromGlobal(QCursor::pos());
        if (!underMouse() || position.y() > m_tracksHeight * m_document->tracksCount()) {
            emit displayMessage(i18n("Cannot paste selected clips"), ErrorMessage);
            return;
        }
    } else position = m_menuPosition;
    GenTime pos = GenTime((int)(mapToScene(position).x()), m_document->fps());
    int track = (int)(position.y() / m_tracksHeight);
    ItemInfo first = m_copiedItems.at(0)->info();

    GenTime offset = pos - first.startPos;
    int trackOffset = track - first.track;

    if (!canBePasted(m_copiedItems, offset, trackOffset)) {
        emit displayMessage(i18n("Cannot paste selected clips"), ErrorMessage);
        return;
    }
    QUndoCommand *pasteClips = new QUndoCommand();
    pasteClips->setText("Paste clips");

    for (int i = 0; i < m_copiedItems.count(); i++) {
        // parse all clip names
        if (m_copiedItems.at(i) && m_copiedItems.at(i)->type() == AVWIDGET) {
            ClipItem *clip = static_cast <ClipItem *>(m_copiedItems.at(i));
            ItemInfo info;
            info.startPos = clip->startPos() + offset;
            info.endPos = clip->endPos() + offset;
            info.cropStart = clip->cropStart();
            info.track = clip->track() + trackOffset;
            if (canBePastedTo(info, AVWIDGET)) {
                new AddTimelineClipCommand(this, clip->xml(), clip->clipProducer(), info, clip->effectList(), true, false, pasteClips);
            } else emit displayMessage(i18n("Cannot paste clip to selected place"), ErrorMessage);
        } else if (m_copiedItems.at(i) && m_copiedItems.at(i)->type() == TRANSITIONWIDGET) {
            Transition *tr = static_cast <Transition *>(m_copiedItems.at(i));
            ItemInfo info;
            info.startPos = tr->startPos() + offset;
            info.endPos = tr->endPos() + offset;
            info.track = tr->track() + trackOffset;
            if (canBePastedTo(info, TRANSITIONWIDGET)) {
                if (info.startPos >= info.endPos) {
                    emit displayMessage(i18n("Invalid transition"), ErrorMessage);
                } else new AddTransitionCommand(this, info, tr->transitionEndTrack() + trackOffset, tr->toXML(), false, true, pasteClips);
            } else emit displayMessage(i18n("Cannot paste transition to selected place"), ErrorMessage);
        }
    }
    m_commandStack->push(pasteClips);
}

void CustomTrackView::pasteClipEffects()
{
    if (m_copiedItems.count() != 1 || m_copiedItems.at(0)->type() != AVWIDGET) {
        emit displayMessage(i18n("You must copy exactly one clip before pasting effects"), ErrorMessage);
        return;
    }
    ClipItem *clip = static_cast < ClipItem *>(m_copiedItems.at(0));
    EffectsList effects = clip->effectList();

    QUndoCommand *paste = new QUndoCommand();
    paste->setText("Paste effects");

    QList<QGraphicsItem *> clips = scene()->selectedItems();
    for (int i = 0; i < clips.count(); ++i) {
        if (clips.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast < ClipItem *>(clips.at(i));
            for (int i = 0; i < clip->effectsCount(); i++) {
                new AddEffectCommand(this, m_document->tracksCount() - item->track(), item->startPos(), clip->effectAt(i), true, paste);
            }
        }
    }
    m_commandStack->push(paste);
}


ClipItem *CustomTrackView::getClipUnderCursor() const
{
    QRectF rect((double) m_cursorPos, 0.0, 1.0, (double)(m_tracksHeight * m_document->tracksCount()));
    QList<QGraphicsItem *> collisions = scene()->items(rect, Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisions.count(); i++) {
        if (collisions.at(i)->type() == AVWIDGET) {
            return static_cast < ClipItem *>(collisions.at(i));
        }
    }
    return NULL;
}

ClipItem *CustomTrackView::getMainActiveClip() const
{
    QList<QGraphicsItem *> clips = scene()->selectedItems();
    if (clips.isEmpty()) {
        return getClipUnderCursor();
    } else {
        ClipItem *item = NULL;
        for (int i = 0; i < clips.count(); ++i) {
            if (clips.at(i)->type() == AVWIDGET)
                item = static_cast < ClipItem *>(clips.at(i));
            if (item->startPos().frames(m_document->fps()) <= m_cursorPos && item->endPos().frames(m_document->fps()) >= m_cursorPos) break;
        }
        if (item) return item;
    }
    return NULL;
}

ClipItem *CustomTrackView::getActiveClipUnderCursor(bool allowOutsideCursor) const
{
    QList<QGraphicsItem *> clips = scene()->selectedItems();
    if (clips.isEmpty()) {
        return getClipUnderCursor();
    } else {
        ClipItem *item;
        // remove all items in the list that are not clips
        for (int i = 0; i < clips.count();) {
            if (clips.at(i)->type() != AVWIDGET) clips.removeAt(i);
            else i++;
        }
        if (clips.count() == 1 && allowOutsideCursor) return static_cast < ClipItem *>(clips.at(0));
        for (int i = 0; i < clips.count(); ++i) {
            if (clips.at(i)->type() == AVWIDGET) {
                item = static_cast < ClipItem *>(clips.at(i));
                if (item->startPos().frames(m_document->fps()) <= m_cursorPos && item->endPos().frames(m_document->fps()) >= m_cursorPos)
                    return item;
            }
        }
    }
    return NULL;
}

void CustomTrackView::setInPoint()
{
    AbstractClipItem *clip = getActiveClipUnderCursor(true);
    if (clip == NULL) {
        if (m_dragItem && m_dragItem->type() == TRANSITIONWIDGET) {
            clip = m_dragItem;
        } else {
            emit displayMessage(i18n("You must select one clip for this action"), ErrorMessage);
            return;
        }
    }
    ItemInfo startInfo = clip->info();
    ItemInfo endInfo = startInfo;
    endInfo.startPos = GenTime(m_cursorPos, m_document->fps());
    if (endInfo.startPos >= startInfo.endPos) {
        // Check for invalid resize
        emit displayMessage(i18n("Invalid action"), ErrorMessage);
        return;
    } else if (endInfo.startPos < startInfo.startPos) {
        int length = m_document->renderer()->mltGetSpaceLength(endInfo.startPos, m_document->tracksCount() - startInfo.track, false);
        if ((clip->type() == TRANSITIONWIDGET && itemCollision(clip, endInfo) == true) || (
                    (clip->type() == AVWIDGET) && length < (startInfo.startPos - endInfo.startPos).frames(m_document->fps()))) {
            emit displayMessage(i18n("Invalid action"), ErrorMessage);
            return;
        }
    }
    if (clip->type() == TRANSITIONWIDGET) {
        m_commandStack->push(new MoveTransitionCommand(this, startInfo, endInfo, true));
    } else m_commandStack->push(new ResizeClipCommand(this, startInfo, endInfo, true));
}

void CustomTrackView::setOutPoint()
{
    AbstractClipItem *clip = getActiveClipUnderCursor(true);
    if (clip == NULL) {
        if (m_dragItem && m_dragItem->type() == TRANSITIONWIDGET) {
            clip = m_dragItem;
        } else {
            emit displayMessage(i18n("You must select one clip for this action"), ErrorMessage);
            return;
        }
    }
    ItemInfo startInfo = clip->info();
    ItemInfo endInfo = clip->info();
    endInfo.endPos = GenTime(m_cursorPos, m_document->fps());
    if (endInfo.endPos <= startInfo.startPos) {
        // Check for invalid resize
        emit displayMessage(i18n("Invalid action"), ErrorMessage);
        return;
    } else if (endInfo.endPos > startInfo.endPos) {
        int length = m_document->renderer()->mltGetSpaceLength(endInfo.endPos, m_document->tracksCount() - startInfo.track, false);
        if ((clip->type() == TRANSITIONWIDGET && itemCollision(clip, endInfo) == true) || (clip->type() == AVWIDGET && length < (endInfo.endPos - startInfo.endPos).frames(m_document->fps()))) {
            emit displayMessage(i18n("Invalid action"), ErrorMessage);
            return;
        }
    }


    if (clip->type() == TRANSITIONWIDGET) {
        m_commandStack->push(new MoveTransitionCommand(this, startInfo, endInfo, true));
    } else m_commandStack->push(new ResizeClipCommand(this, startInfo, endInfo, true));
}

void CustomTrackView::slotUpdateAllThumbs()
{
    QList<QGraphicsItem *> itemList = items();
    //if (itemList.isEmpty()) return;
    ClipItem *item;
    const QString thumbBase = m_document->projectFolder().path() + "/thumbs/";
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            item = static_cast <ClipItem *>(itemList.at(i));
            if (item->clipType() != COLOR && item->clipType() != AUDIO) {
                // Check if we have a cached thumbnail
                if (item->clipType() == IMAGE || item->clipType() == TEXT) {
                    QString thumb = thumbBase + item->baseClip()->getClipHash() + "_0.png";
                    if (QFile::exists(thumb)) {
                        QPixmap pix(thumb);
                        item->slotSetStartThumb(pix);
                    }
                } else {
                    QString startThumb = thumbBase + item->baseClip()->getClipHash() + '_';
                    QString endThumb = startThumb;
                    startThumb.append(QString::number(item->cropStart().frames(m_document->fps())) + ".png");
                    endThumb.append(QString::number((item->cropStart() + item->cropDuration()).frames(m_document->fps()) - 1) + ".png");
                    if (QFile::exists(startThumb)) {
                        QPixmap pix(startThumb);
                        item->slotSetStartThumb(pix);
                    }
                    if (QFile::exists(endThumb)) {
                        QPixmap pix(endThumb);
                        item->slotSetEndThumb(pix);
                    }
                }
            }
            item->refreshClip(false);
            qApp->processEvents();
        }
    }
    viewport()->update();
}

void CustomTrackView::saveThumbnails()
{
    QList<QGraphicsItem *> itemList = items();
    ClipItem *item;
    QString thumbBase = m_document->projectFolder().path() + "/thumbs/";
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            item = static_cast <ClipItem *>(itemList.at(i));
            if (item->clipType() != COLOR) {
                // Check if we have a cached thumbnail
                if (item->clipType() == IMAGE || item->clipType() == TEXT || item->clipType() == AUDIO) {
                    QString thumb = thumbBase + item->baseClip()->getClipHash() + "_0.png";
                    if (!QFile::exists(thumb)) {
                        QPixmap pix(item->startThumb());
                        pix.save(thumb);
                    }
                } else {
                    QString startThumb = thumbBase + item->baseClip()->getClipHash() + '_';
                    QString endThumb = startThumb;
                    startThumb.append(QString::number(item->cropStart().frames(m_document->fps())) + ".png");
                    endThumb.append(QString::number((item->cropStart() + item->cropDuration()).frames(m_document->fps()) - 1) + ".png");
                    if (!QFile::exists(startThumb)) {
                        QPixmap pix(item->startThumb());
                        pix.save(startThumb);
                    }
                    if (!QFile::exists(endThumb)) {
                        QPixmap pix(item->endThumb());
                        pix.save(endThumb);
                    }
                }
            }
        }
    }
}


void CustomTrackView::slotInsertTrack(int ix)
{
    kDebug() << "// INSERTING TRK: " << ix;
    QDialog d(parentWidget());
    Ui::AddTrack_UI view;
    view.setupUi(&d);
    view.track_nb->setMaximum(m_document->tracksCount() - 1);
    view.track_nb->setValue(ix);
    d.setWindowTitle(i18n("Insert Track"));

    if (d.exec() == QDialog::Accepted) {
        ix = view.track_nb->value();
        if (view.before_select->currentIndex() == 1) {
            ix++;
        }
        TrackInfo info;
        if (view.video_track->isChecked()) {
            info.type = VIDEOTRACK;
            info.isMute = false;
            info.isBlind = false;
            info.isLocked = false;
        } else {
            info.type = AUDIOTRACK;
            info.isMute = false;
            info.isBlind = true;
            info.isLocked = false;
        }
        AddTrackCommand *addTrack = new AddTrackCommand(this, ix, info, true);
        m_commandStack->push(addTrack);
        setDocumentModified();
    }
}

void CustomTrackView::slotDeleteTrack(int ix)
{
    bool ok;
    ix = QInputDialog::getInteger(this, i18n("Remove Track"), i18n("Track"), ix, 0, m_document->tracksCount() - 1, 1, &ok);
    if (ok) {
        TrackInfo info = m_document->trackInfoAt(m_document->tracksCount() - ix - 1);
        deleteTimelineTrack(ix, info);
        setDocumentModified();
        /*AddTrackCommand* command = new AddTrackCommand(this, ix, info, false);
        m_commandStack->push(command);*/
    }
}

void CustomTrackView::slotChangeTrack(int ix)
{
    QDialog d(parentWidget());
    Ui::AddTrack_UI view;
    view.setupUi(&d);
    view.label->setText(i18n("Change track"));
    view.before_select->setHidden(true);
    view.track_nb->setMaximum(m_document->tracksCount() - 1);
    view.track_nb->setValue(ix);
    d.setWindowTitle(i18n("Change Track Type"));

    if (m_document->trackInfoAt(m_document->tracksCount() - ix - 1).type == VIDEOTRACK)
        view.video_track->setChecked(true);
    else
        view.audio_track->setChecked(true);

    if (d.exec() == QDialog::Accepted) {
        TrackInfo info;
        info.isLocked = false;
        info.isMute = false;
        ix = view.track_nb->value();

        if (view.video_track->isChecked()) {
            info.type = VIDEOTRACK;
            info.isBlind = false;
        } else {
            info.type = AUDIOTRACK;
            info.isBlind = true;
        }
        changeTimelineTrack(ix, info);
        setDocumentModified();
    }
}


void CustomTrackView::deleteTimelineTrack(int ix, TrackInfo trackinfo)
{
    double startY = ix * m_tracksHeight + 1 + m_tracksHeight / 2;
    QRectF r(0, startY, sceneRect().width(), m_tracksHeight / 2 - 1);
    QList<QGraphicsItem *> selection = m_scene->items(r);
    QUndoCommand *deleteTrack = new QUndoCommand();
    deleteTrack->setText("Delete track");

    // Delete all clips in selected track
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == AVWIDGET) {
            ClipItem *item =  static_cast <ClipItem *>(selection.at(i));
            new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), item->info(), item->effectList(), false, true, deleteTrack);
            m_scene->removeItem(item);
            delete item;
            item = NULL;
        } else if (selection.at(i)->type() == TRANSITIONWIDGET) {
            Transition *item =  static_cast <Transition *>(selection.at(i));
            new AddTransitionCommand(this, item->info(), item->transitionEndTrack(), item->toXML(), true, false, deleteTrack);
            m_scene->removeItem(item);
            delete item;
            item = NULL;
        }
    }

    selection = m_scene->items();
    new AddTrackCommand(this, ix, trackinfo, false, deleteTrack);
    m_commandStack->push(deleteTrack);
}

void CustomTrackView::changeTimelineTrack(int ix, TrackInfo trackinfo)
{
    TrackInfo oldinfo = m_document->trackInfoAt(m_document->tracksCount() - ix - 1);
    ChangeTrackCommand *changeTrack = new ChangeTrackCommand(this, ix, oldinfo, trackinfo);
    m_commandStack->push(changeTrack);
}

void CustomTrackView::autoTransition()
{
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() != 1 || itemList.at(0)->type() != TRANSITIONWIDGET) {
        emit displayMessage(i18n("You must select one transition for this action"), ErrorMessage);
        return;
    }
    Transition *tr = static_cast <Transition*>(itemList.at(0));
    tr->setAutomatic(!tr->isAutomatic());
    QDomElement transition = tr->toXML();
    m_document->renderer()->mltUpdateTransition(transition.attribute("tag"), transition.attribute("tag"), transition.attribute("transition_btrack").toInt(), m_document->tracksCount() - transition.attribute("transition_atrack").toInt(), tr->startPos(), tr->endPos(), transition);
}


QStringList CustomTrackView::getLadspaParams(QDomElement effect) const
{
    QStringList result;
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull() && e.attribute("type") == "constant") {
            if (e.hasAttribute("factor")) {
                double factor = e.attribute("factor").toDouble();
                double value = e.attribute("value").toDouble();
                value = value / factor;
                result.append(QString::number(value));
            } else result.append(e.attribute("value"));
        }
    }
    return result;
}

void CustomTrackView::clipNameChanged(const QString id, const QString name)
{
    QList<QGraphicsItem *> list = scene()->items();
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->type() == AVWIDGET) {
            clip = static_cast <ClipItem *>(list.at(i));
            if (clip->clipProducer() == id) {
                clip->setClipName(name);
            }
        }
    }
    viewport()->update();
}

void CustomTrackView::getClipAvailableSpace(AbstractClipItem *item, GenTime &minimum, GenTime &maximum)
{
    minimum = GenTime();
    maximum = GenTime();
    QList<QGraphicsItem *> selection;
    selection = m_scene->items(0, item->track() * m_tracksHeight + m_tracksHeight / 2, sceneRect().width(), 2);
    selection.removeAll(item);
    for (int i = 0; i < selection.count(); i++) {
        AbstractClipItem *clip = static_cast <AbstractClipItem *>(selection.at(i));
        if (clip && clip->type() == AVWIDGET) {
            if (clip->endPos() <= item->startPos() && clip->endPos() > minimum) minimum = clip->endPos();
            if (clip->startPos() > item->startPos() && (clip->startPos() < maximum || maximum == GenTime())) maximum = clip->startPos();
        }
    }
}

void CustomTrackView::getTransitionAvailableSpace(AbstractClipItem *item, GenTime &minimum, GenTime &maximum)
{
    minimum = GenTime();
    maximum = GenTime();
    QList<QGraphicsItem *> selection;
    selection = m_scene->items(0, (item->track() + 1) * m_tracksHeight, sceneRect().width(), 2);
    selection.removeAll(item);
    for (int i = 0; i < selection.count(); i++) {
        AbstractClipItem *clip = static_cast <AbstractClipItem *>(selection.at(i));
        if (clip && clip->type() == TRANSITIONWIDGET) {
            if (clip->endPos() <= item->startPos() && clip->endPos() > minimum) minimum = clip->endPos();
            if (clip->startPos() > item->startPos() && (clip->startPos() < maximum || maximum == GenTime())) maximum = clip->startPos();
        }
    }
}


void CustomTrackView::loadGroups(const QDomNodeList groups)
{
    for (int i = 0; i < groups.count(); i++) {
        QDomNodeList children = groups.at(i).childNodes();
        scene()->clearSelection();
        for (int nodeindex = 0; nodeindex < children.count(); nodeindex++) {
            QDomNode n = children.item(nodeindex);
            QDomElement elem = n.toElement();
            int pos = elem.attribute("position").toInt();
            int track = elem.attribute("track").toInt();
            if (elem.tagName() == "clipitem") {
                ClipItem *clip = getClipItemAt(pos, track); //m_document->tracksCount() - transitiontrack);
                if (clip) clip->setSelected(true);
            } else {
                Transition *clip = getTransitionItemAt(pos, track); //m_document->tracksCount() - transitiontrack);
                if (clip) clip->setSelected(true);
            }
        }
        groupSelectedItems(false, true);
    }
}

void CustomTrackView::splitAudio()
{
    resetSelectionGroup();
    QList<QGraphicsItem *> selection = scene()->selectedItems();
    if (selection.isEmpty()) {
        emit displayMessage(i18n("You must select one clip for this action"), ErrorMessage);
        return;
    }
    QUndoCommand *splitCommand = new QUndoCommand();
    splitCommand->setText(i18n("Split audio"));
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == AVWIDGET) {
            ClipItem *clip = static_cast <ClipItem *>(selection.at(i));
            if (clip->clipType() == AV || clip->clipType() == PLAYLIST) {
                if (clip->parentItem()) {
                    emit displayMessage(i18n("Cannot split audio of grouped clips"), ErrorMessage);
                } else {
                    new SplitAudioCommand(this, clip->track(), clip->startPos(), splitCommand);
                }
            }
        }
    }
    m_commandStack->push(splitCommand);
}

void CustomTrackView::doSplitAudio(const GenTime &pos, int track, bool split)
{
    ClipItem *clip = getClipItemAt(pos, track);
    if (clip == NULL) {
        kDebug() << "// Cannot find clip to split!!!";
        return;
    }
    if (split) {
        int start = pos.frames(m_document->fps());
        int freetrack = m_document->tracksCount() - track - 1;
        for (; freetrack > 0; freetrack--) {
            kDebug() << "// CHK DOC TRK:" << freetrack << ", DUR:" << m_document->renderer()->mltTrackDuration(freetrack);
            if (m_document->trackInfoAt(freetrack - 1).type == AUDIOTRACK) {
                kDebug() << "// CHK DOC TRK:" << freetrack << ", DUR:" << m_document->renderer()->mltTrackDuration(freetrack);
                if (m_document->renderer()->mltTrackDuration(freetrack) < start || m_document->renderer()->mltGetSpaceLength(pos, freetrack, false) >= clip->cropDuration().frames(m_document->fps())) {
                    kDebug() << "FOUND SPACE ON TRK: " << freetrack;
                    break;
                }
            }
        }
        kDebug() << "GOT TRK: " << track;
        if (freetrack == 0) {
            emit displayMessage(i18n("No empty space to put clip audio"), ErrorMessage);
        } else {
            ItemInfo info;
            info.startPos = clip->startPos();
            info.endPos = clip->endPos();
            info.cropStart = clip->cropStart();
            info.track = m_document->tracksCount() - freetrack;
            addClip(clip->xml(), clip->clipProducer(), info, clip->effectList());
            scene()->clearSelection();
            clip->setSelected(true);
            ClipItem *audioClip = getClipItemAt(start, info.track);
            if (audioClip) {
                clip->setVideoOnly(true);
                m_document->renderer()->mltUpdateClipProducer(m_document->tracksCount() - track, start, clip->baseClip()->videoProducer());
                m_document->renderer()->mltUpdateClipProducer(m_document->tracksCount() - info.track, start, clip->baseClip()->audioProducer(info.track));
                audioClip->setSelected(true);
                audioClip->setAudioOnly(true);
                groupSelectedItems(false, true);
            }
        }
    } else {
        // unsplit clip: remove audio part and change video part to normal clip
        if (clip->parentItem() == NULL || clip->parentItem()->type() != GROUPWIDGET) {
            kDebug() << "//CANNOT FIND CLP GRP";
            return;
        }
        AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(clip->parentItem());
        QList<QGraphicsItem *> children = grp->childItems();
        if (children.count() != 2) {
            kDebug() << "//SOMETHING IS WRONG WITH CLP GRP";
            return;
        }
        for (int i = 0; i < children.count(); i++) {
            if (children.at(i) != clip) {
                ClipItem *clp = static_cast <ClipItem *>(children.at(i));
                ItemInfo info = clip->info();
                deleteClip(clp->info());
                clip->setVideoOnly(false);
                m_document->renderer()->mltUpdateClipProducer(m_document->tracksCount() - info.track, info.startPos.frames(m_document->fps()), clip->baseClip()->producer(info.track));
                break;
            }
        }
        clip->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
        m_document->clipManager()->removeGroup(grp);
        scene()->destroyItemGroup(grp);
    }
}

void CustomTrackView::setVideoOnly()
{
    resetSelectionGroup();
    QList<QGraphicsItem *> selection = scene()->selectedItems();
    if (selection.isEmpty()) {
        emit displayMessage(i18n("You must select one clip for this action"), ErrorMessage);
        return;
    }
    QUndoCommand *videoCommand = new QUndoCommand();
    videoCommand->setText(i18n("Video only"));
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == AVWIDGET) {
            ClipItem *clip = static_cast <ClipItem *>(selection.at(i));
            if (clip->clipType() == AV || clip->clipType() == PLAYLIST) {
                if (clip->parentItem()) {
                    emit displayMessage(i18n("Cannot change grouped clips"), ErrorMessage);
                } else {
                    new ChangeClipTypeCommand(this, clip->track(), clip->startPos(), true, false, clip->isVideoOnly(), clip->isAudioOnly(), videoCommand);
                }
            }
        }
    }
    m_commandStack->push(videoCommand);
}

void CustomTrackView::setAudioOnly()
{
    resetSelectionGroup();
    QList<QGraphicsItem *> selection = scene()->selectedItems();
    if (selection.isEmpty()) {
        emit displayMessage(i18n("You must select one clip for this action"), ErrorMessage);
        return;
    }
    QUndoCommand *videoCommand = new QUndoCommand();
    videoCommand->setText(i18n("Audio only"));
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == AVWIDGET) {
            ClipItem *clip = static_cast <ClipItem *>(selection.at(i));
            if (clip->clipType() == AV || clip->clipType() == PLAYLIST) {
                if (clip->parentItem()) {
                    emit displayMessage(i18n("Cannot change grouped clips"), ErrorMessage);
                } else {
                    new ChangeClipTypeCommand(this, clip->track(), clip->startPos(), false, true, clip->isVideoOnly(), clip->isAudioOnly(), videoCommand);
                }
            }
        }
    }
    m_commandStack->push(videoCommand);
}

void CustomTrackView::setAudioAndVideo()
{
    resetSelectionGroup();
    QList<QGraphicsItem *> selection = scene()->selectedItems();
    if (selection.isEmpty()) {
        emit displayMessage(i18n("You must select one clip for this action"), ErrorMessage);
        return;
    }
    QUndoCommand *videoCommand = new QUndoCommand();
    videoCommand->setText(i18n("Audio and Video"));
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == AVWIDGET) {
            ClipItem *clip = static_cast <ClipItem *>(selection.at(i));
            if (clip->clipType() == AV || clip->clipType() == PLAYLIST) {
                if (clip->parentItem()) {
                    emit displayMessage(i18n("Cannot change grouped clips"), ErrorMessage);
                } else {
                    new ChangeClipTypeCommand(this, clip->track(), clip->startPos(), false, false, clip->isVideoOnly(), clip->isAudioOnly(), videoCommand);
                }
            }
        }
    }
    m_commandStack->push(videoCommand);
}

void CustomTrackView::doChangeClipType(const GenTime &pos, int track, bool videoOnly, bool audioOnly)
{
    ClipItem *clip = getClipItemAt(pos, track);
    if (clip == NULL) {
        kDebug() << "// Cannot find clip to split!!!";
        return;
    }
    if (videoOnly) {
        int start = pos.frames(m_document->fps());
        clip->setVideoOnly(true);
        clip->setAudioOnly(false);
        m_document->renderer()->mltUpdateClipProducer(m_document->tracksCount() - track, start, clip->baseClip()->videoProducer());
    } else if (audioOnly) {
        int start = pos.frames(m_document->fps());
        clip->setAudioOnly(true);
        clip->setVideoOnly(false);
        m_document->renderer()->mltUpdateClipProducer(m_document->tracksCount() - track, start, clip->baseClip()->audioProducer(track));
    } else {
        int start = pos.frames(m_document->fps());
        clip->setAudioOnly(false);
        clip->setVideoOnly(false);
        m_document->renderer()->mltUpdateClipProducer(m_document->tracksCount() - track, start, clip->baseClip()->producer(track));
    }
    clip->update();
    setDocumentModified();
}

void CustomTrackView::updateClipTypeActions(ClipItem *clip)
{
    if (clip == NULL || (clip->clipType() != AV && clip->clipType() != PLAYLIST)) {
        m_clipTypeGroup->setEnabled(false);
    } else {
        m_clipTypeGroup->setEnabled(true);
        QList <QAction *> actions = m_clipTypeGroup->actions();
        QString lookup;
        if (clip->isAudioOnly()) lookup = "clip_audio_only";
        else if (clip->isVideoOnly()) lookup = "clip_video_only";
        else  lookup = "clip_audio_and_video";
        for (int i = 0; i < actions.count(); i++) {
            if (actions.at(i)->data().toString() == lookup) {
                actions.at(i)->setChecked(true);
                break;
            }
        }
    }
}

void CustomTrackView::reloadTransitionLumas()
{
    QString lumaNames;
    QString lumaFiles;
    QDomElement lumaTransition = MainWindow::transitions.getEffectByTag("luma", "luma");
    QDomNodeList params = lumaTransition.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute("tag") == "resource") {
            lumaNames = e.attribute("paramlistdisplay");
            lumaFiles = e.attribute("paramlist");
            break;
        }
    }

    QList<QGraphicsItem *> itemList = items();
    Transition *transitionitem;
    QDomElement transitionXml;
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            transitionitem = static_cast <Transition*>(itemList.at(i));
            transitionXml = transitionitem->toXML();
            if (transitionXml.attribute("id") == "luma" && transitionXml.attribute("tag") == "luma") {
                QDomNodeList params = transitionXml.elementsByTagName("parameter");
                for (int i = 0; i < params.count(); i++) {
                    QDomElement e = params.item(i).toElement();
                    if (e.attribute("tag") == "resource") {
                        e.setAttribute("paramlistdisplay", lumaNames);
                        e.setAttribute("paramlist", lumaFiles);
                        break;
                    }
                }
            }
            if (transitionXml.attribute("id") == "composite" && transitionXml.attribute("tag") == "composite") {
                QDomNodeList params = transitionXml.elementsByTagName("parameter");
                for (int i = 0; i < params.count(); i++) {
                    QDomElement e = params.item(i).toElement();
                    if (e.attribute("tag") == "luma") {
                        e.setAttribute("paramlistdisplay", lumaNames);
                        e.setAttribute("paramlist", lumaFiles);
                        break;
                    }
                }
            }
        }
    }
    emit transitionItemSelected(NULL);
}

#include "customtrackview.moc"
