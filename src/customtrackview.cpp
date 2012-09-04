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
#include "docclipbase.h"
#include "clipitem.h"
#include "definitions.h"
#include "commands/moveclipcommand.h"
#include "commands/movetransitioncommand.h"
#include "commands/resizeclipcommand.h"
#include "commands/editguidecommand.h"
#include "commands/addtimelineclipcommand.h"
#include "commands/addeffectcommand.h"
#include "commands/editeffectcommand.h"
#include "commands/moveeffectcommand.h"
#include "commands/addtransitioncommand.h"
#include "commands/edittransitioncommand.h"
#include "commands/editkeyframecommand.h"
#include "commands/changespeedcommand.h"
#include "commands/addmarkercommand.h"
#include "commands/razorclipcommand.h"
#include "kdenlivesettings.h"
#include "transition.h"
#include "clipmanager.h"
#include "renderer.h"
#include "markerdialog.h"
#include "mainwindow.h"
#include "ui_keyframedialog_ui.h"
#include "clipdurationdialog.h"
#include "abstractgroupitem.h"
#include "commands/insertspacecommand.h"
#include "spacerdialog.h"
#include "commands/addtrackcommand.h"
#include "commands/changeeffectstatecommand.h"
#include "commands/movegroupcommand.h"
#include "ui_addtrack_ui.h"
#include "initeffects.h"
#include "commands/locktrackcommand.h"
#include "commands/groupclipscommand.h"
#include "commands/splitaudiocommand.h"
#include "commands/changecliptypecommand.h"
#include "trackdialog.h"
#include "tracksconfigdialog.h"
#include "commands/configtrackscommand.h"
#include "commands/rebuildgroupcommand.h"
#include "commands/razorgroupcommand.h"
#include "profilesdialog.h"

#include "lib/audio/audioEnvelope.h"
#include "lib/audio/audioCorrelation.h"

#include <KDebug>
#include <KLocale>
#include <KUrl>
#include <KIcon>
#include <KCursor>
#include <KMessageBox>
#include <KIO/NetAccess>

#include <QMouseEvent>
#include <QStylePainter>
#include <QGraphicsItem>
#include <QDomDocument>
#include <QScrollBar>
#include <QApplication>
#include <QInputDialog>


#if QT_VERSION >= 0x040600
#include <QGraphicsDropShadowEffect>
#endif

#define SEEK_INACTIVE (-1)

//#define DEBUG

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
    m_selectionGroup(NULL),
    m_selectedTrack(0),
    m_audioCorrelator(NULL),
    m_audioAlignmentReference(NULL),
    m_controlModifier(false)
{
    if (doc) {
        m_commandStack = doc->commandStack();
    } else {
        m_commandStack = NULL;
    }
    m_ct = 0;
    setMouseTracking(true);
    setAcceptDrops(true);
    setFrameShape(QFrame::NoFrame);
    setLineWidth(0);
    //setCacheMode(QGraphicsView::CacheBackground);
    setAutoFillBackground(false);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setContentsMargins(0, 0, 0, 0);

    m_activeTrackBrush = KStatefulBrush(KColorScheme::View, KColorScheme::ActiveBackground, KSharedConfig::openConfig(KdenliveSettings::colortheme()));

    m_animationTimer = new QTimeLine(800);
    m_animationTimer->setFrameRange(0, 5);
    m_animationTimer->setUpdateInterval(100);
    m_animationTimer->setLoopCount(0);

    m_tipColor = QColor(0, 192, 0, 200);
    m_tipPen.setColor(QColor(255, 255, 255, 100));
    m_tipPen.setWidth(3);

    const int maxHeight = m_tracksHeight * m_document->tracksCount();
    setSceneRect(0, 0, sceneRect().width(), maxHeight);
    verticalScrollBar()->setMaximum(maxHeight);
    verticalScrollBar()->setTracking(true);
    // repaint guides when using vertical scroll
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(slotRefreshGuides()));

    m_cursorLine = projectscene->addLine(0, 0, 0, maxHeight);
    m_cursorLine->setZValue(1000);
    QPen pen1 = QPen();
    pen1.setWidth(1);
    pen1.setColor(palette().text().color());
    m_cursorLine->setPen(pen1);
    m_cursorLine->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);

    connect(&m_scrollTimer, SIGNAL(timeout()), this, SLOT(slotCheckMouseScrolling()));
    m_scrollTimer.setInterval(100);
    m_scrollTimer.setSingleShot(true);

    connect(&m_thumbsTimer, SIGNAL(timeout()), this, SLOT(slotFetchNextThumbs()));
    m_thumbsTimer.setInterval(500);
    m_thumbsTimer.setSingleShot(true);

    KIcon razorIcon("edit-cut");
    m_razorCursor = QCursor(razorIcon.pixmap(22, 22));

    KIcon spacerIcon("kdenlive-spacer-tool");
    m_spacerCursor = QCursor(spacerIcon.pixmap(22, 22));
}

CustomTrackView::~CustomTrackView()
{
    qDeleteAll(m_guides);
    m_guides.clear();
    m_waitingThumbs.clear();
    delete m_animationTimer;
}

//virtual
void CustomTrackView::keyPressEvent(QKeyEvent * event)
{
    if (event->key() == Qt::Key_Up) {
        slotTrackUp();
        event->accept();
    } else if (event->key() == Qt::Key_Down) {
        slotTrackDown();
        event->accept();
    } else QWidget::keyPressEvent(event);
}

void CustomTrackView::setDocumentModified()
{
    m_document->setModified(true);
}

void CustomTrackView::setContextMenu(QMenu *timeline, QMenu *clip, QMenu *transition, QActionGroup *clipTypeGroup, QMenu *markermenu)
{
    m_timelineContextMenu = timeline;
    m_timelineContextClipMenu = clip;
    m_clipTypeGroup = clipTypeGroup;
    connect(m_timelineContextMenu, SIGNAL(aboutToHide()), this, SLOT(slotResetMenuPosition()));

    m_markerMenu = new QMenu(i18n("Go to marker..."), this);
    m_markerMenu->setEnabled(false);
    markermenu->addMenu(m_markerMenu);
    connect(m_markerMenu, SIGNAL(triggered(QAction *)), this, SLOT(slotGoToMarker(QAction *)));
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

void CustomTrackView::slotDoResetMenuPosition()
{
    m_menuPosition = QPoint();
}

void CustomTrackView::slotResetMenuPosition()
{
    // after a short time (so that the action is triggered / or menu is closed, we reset the menu pos
    QTimer::singleShot(300, this, SLOT(slotDoResetMenuPosition()));
}

void CustomTrackView::checkAutoScroll()
{
    m_autoScroll = KdenliveSettings::autoscroll();
}

/*sQList <TrackInfo> CustomTrackView::tracksList() const {
    return m_scene->m_tracksList;
}*/


int CustomTrackView::getFrameWidth()
{
    return (int) (m_tracksHeight * m_document->mltProfile().display_aspect_num / m_document->mltProfile().display_aspect_den + 0.5);
}

void CustomTrackView::updateSceneFrameWidth()
{
    int frameWidth = getFrameWidth();
    QList<QGraphicsItem *> itemList = items();
    ClipItem *item;
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            item = (ClipItem*) itemList.at(i);
            item->resetFrameWidth(frameWidth);
            item->resetThumbs(true);
        }
    }
}

bool CustomTrackView::checkTrackHeight()
{
    if (m_tracksHeight == KdenliveSettings::trackheight()) return false;
    m_tracksHeight = KdenliveSettings::trackheight();
    emit trackHeightChanged();
    QList<QGraphicsItem *> itemList = items();
    ClipItem *item;
    Transition *transitionitem;
    int frameWidth = getFrameWidth();
    bool snap = KdenliveSettings::snaptopoints();
    KdenliveSettings::setSnaptopoints(false);
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            item = (ClipItem*) itemList.at(i);
            item->setRect(0, 0, item->rect().width(), m_tracksHeight - 1);
            item->setPos((qreal) item->startPos().frames(m_document->fps()), (qreal) item->track() * m_tracksHeight + 1);
            item->resetFrameWidth(frameWidth);
            item->resetThumbs(true);
        } else if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            transitionitem = (Transition*) itemList.at(i);
            transitionitem->setRect(0, 0, transitionitem->rect().width(), m_tracksHeight / 3 * 2 - 1);
            transitionitem->setPos((qreal) transitionitem->startPos().frames(m_document->fps()), (qreal) transitionitem->track() * m_tracksHeight + m_tracksHeight / 3 * 2);
        }
    }
    double newHeight = m_tracksHeight * m_document->tracksCount() * matrix().m22();
    m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), newHeight - 1);

    for (int i = 0; i < m_guides.count(); i++) {
        QLineF l = m_guides.at(i)->line();
        l.setP2(QPointF(l.x2(), newHeight));
        m_guides.at(i)->setLine(l);
    }

    setSceneRect(0, 0, sceneRect().width(), m_tracksHeight * m_document->tracksCount());
//     verticalScrollBar()->setMaximum(m_tracksHeight * m_document->tracksCount());
    KdenliveSettings::setSnaptopoints(snap);
    viewport()->update();
    return true;
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
        while (item == NULL && !m_waitingThumbs.isEmpty()) {
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
        seekCursorPos(mapToScene(QPoint(-2, 0)).x());
    } else if (viewport()->width() - 3 < mapFromScene(m_cursorPos + 1, 0).x()) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + 2);
        seekCursorPos(mapToScene(QPoint(viewport()->width(), 0)).x() + 1);
        QTimer::singleShot(200, this, SLOT(slotCheckPositionScrolling()));
    }
}


// virtual
void CustomTrackView::mouseMoveEvent(QMouseEvent * event)
{
    int pos = event->x();
    int mappedXPos = qMax((int)(mapToScene(event->pos()).x() + 0.5), 0);
   
    double snappedPos = getSnapPointForPos(mappedXPos);
    emit mousePosition(mappedXPos);

    if (event->buttons() & Qt::MidButton) return;
    if (dragMode() == QGraphicsView::RubberBandDrag || (event->modifiers() == Qt::ControlModifier && m_tool != SPACERTOOL && m_operationMode != RESIZESTART && m_operationMode != RESIZEEND)) {
        event->setAccepted(true);
        m_moveOpMode = NONE;
        QGraphicsView::mouseMoveEvent(event);
        return;
    }

    if (event->buttons() != Qt::NoButton) {
        bool move = (event->pos() - m_clickEvent).manhattanLength() >= QApplication::startDragDistance();
        if (m_dragItem && m_tool == SELECTTOOL) {
            if (m_operationMode == MOVE && move) {
		//m_dragItem->setProperty("y_absolute", event->pos().y());
                QGraphicsView::mouseMoveEvent(event);
                // If mouse is at a border of the view, scroll
                if (pos < 5) {
                    m_scrollOffset = -30;
                    m_scrollTimer.start();
                } else if (viewport()->width() - pos < 10) {
                    m_scrollOffset = 30;
                    m_scrollTimer.start();
                } else if (m_scrollTimer.isActive()) {
                    m_scrollTimer.stop();
                }
            } else if (m_operationMode == RESIZESTART && move) {
                m_document->renderer()->pause();
                if (!m_controlModifier && m_dragItem->type() == AVWIDGET && m_dragItem->parentItem() && m_dragItem->parentItem() != m_selectionGroup) {
                    AbstractGroupItem *parent = static_cast <AbstractGroupItem *>(m_dragItem->parentItem());
                    if (parent)
                        parent->resizeStart((int)(snappedPos - m_dragItemInfo.startPos.frames(m_document->fps())));
                } else {
                    m_dragItem->resizeStart((int)(snappedPos));
                }
                QString crop = m_document->timecode().getDisplayTimecode(m_dragItem->cropStart(), KdenliveSettings::frametimecode());
                QString duration = m_document->timecode().getDisplayTimecode(m_dragItem->cropDuration(), KdenliveSettings::frametimecode());
                QString offset = m_document->timecode().getDisplayTimecode(m_dragItem->cropStart() - m_dragItemInfo.cropStart, KdenliveSettings::frametimecode());
                emit displayMessage(i18n("Crop from start:") + ' ' + crop + ' ' + i18n("Duration:") + ' ' + duration + ' ' + i18n("Offset:") + ' ' + offset, InformationMessage);
            } else if (m_operationMode == RESIZEEND && move) {
                m_document->renderer()->pause();
                if (!m_controlModifier && m_dragItem->type() == AVWIDGET && m_dragItem->parentItem() && m_dragItem->parentItem() != m_selectionGroup) {
                    AbstractGroupItem *parent = static_cast <AbstractGroupItem *>(m_dragItem->parentItem());
                    if (parent)
                        parent->resizeEnd((int)(snappedPos - m_dragItemInfo.endPos.frames(m_document->fps())));
                } else {
                    m_dragItem->resizeEnd((int)(snappedPos));
                }
                QString duration = m_document->timecode().getDisplayTimecode(m_dragItem->cropDuration(), KdenliveSettings::frametimecode());
                QString offset = m_document->timecode().getDisplayTimecode(m_dragItem->cropDuration() - m_dragItemInfo.cropDuration, KdenliveSettings::frametimecode());
                emit displayMessage(i18n("Duration:") + ' ' + duration + ' ' + i18n("Offset:") + ' ' + offset, InformationMessage);
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
                QString position = m_document->timecode().getDisplayTimecodeFromFrames(m_dragItem->editedKeyFramePos(), KdenliveSettings::frametimecode());
                emit displayMessage(position + " : " + QString::number(m_dragItem->editedKeyFrameValue()), InformationMessage);
            }
            removeTipAnimation();
            return;
        } else if (m_operationMode == MOVEGUIDE) {
            removeTipAnimation();
            QGraphicsView::mouseMoveEvent(event);
            return;
        } else if (m_operationMode == SPACER && move && m_selectionGroup) {
            // spacer tool
            snappedPos = getSnapPointForPos(mappedXPos + m_spacerOffset);
            if (snappedPos < 0) snappedPos = 0;

            // Make sure there is no collision
            QList<QGraphicsItem *> children = m_selectionGroup->childItems();
            QPainterPath shape = m_selectionGroup->clipGroupShape(QPointF(snappedPos - m_selectionGroup->sceneBoundingRect().left(), 0));
            QList<QGraphicsItem*> collidingItems = scene()->items(shape, Qt::IntersectsItemShape);
            collidingItems.removeAll(m_selectionGroup);
            for (int i = 0; i < children.count(); i++) {
                if (children.at(i)->type() == GROUPWIDGET) {
                    QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                    for (int j = 0; j < subchildren.count(); j++)
                        collidingItems.removeAll(subchildren.at(j));
                }
                collidingItems.removeAll(children.at(i));
            }
            bool collision = false;
            int offset = 0;
            for (int i = 0; i < collidingItems.count(); i++) {
                if (!collidingItems.at(i)->isEnabled()) continue;
                if (collidingItems.at(i)->type() == AVWIDGET && snappedPos < m_selectionGroup->sceneBoundingRect().left()) {
                    AbstractClipItem *item = static_cast <AbstractClipItem *>(collidingItems.at(i));
                    // Moving backward, determine best pos
                    QPainterPath clipPath;
                    clipPath.addRect(item->sceneBoundingRect());
                    QPainterPath res = shape.intersected(clipPath);
                    offset = qMax(offset, (int)(res.boundingRect().width() + 0.5));
                }
            }
            snappedPos += offset;
            // make sure we have no collision
            shape = m_selectionGroup->clipGroupShape(QPointF(snappedPos - m_selectionGroup->sceneBoundingRect().left(), 0));
            collidingItems = scene()->items(shape, Qt::IntersectsItemShape);
            collidingItems.removeAll(m_selectionGroup);
            for (int i = 0; i < children.count(); i++) {
                if (children.at(i)->type() == GROUPWIDGET) {
                    QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                    for (int j = 0; j < subchildren.count(); j++)
                        collidingItems.removeAll(subchildren.at(j));
                }
                collidingItems.removeAll(children.at(i));
            }

            for (int i = 0; i < collidingItems.count(); i++) {
                if (!collidingItems.at(i)->isEnabled()) continue;
                if (collidingItems.at(i)->type() == AVWIDGET) {
                    collision = true;
                    break;
                }
            }


            if (!collision) {
                // Check transitions
                shape = m_selectionGroup->transitionGroupShape(QPointF(snappedPos - m_selectionGroup->sceneBoundingRect().left(), 0));
                collidingItems = scene()->items(shape, Qt::IntersectsItemShape);
                collidingItems.removeAll(m_selectionGroup);
                for (int i = 0; i < children.count(); i++) {
                    if (children.at(i)->type() == GROUPWIDGET) {
                        QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                        for (int j = 0; j < subchildren.count(); j++)
                            collidingItems.removeAll(subchildren.at(j));
                    }
                    collidingItems.removeAll(children.at(i));
                }
                offset = 0;

                for (int i = 0; i < collidingItems.count(); i++) {
                    if (collidingItems.at(i)->type() == TRANSITIONWIDGET && snappedPos < m_selectionGroup->sceneBoundingRect().left()) {
                        AbstractClipItem *item = static_cast <AbstractClipItem *>(collidingItems.at(i));
                        // Moving backward, determine best pos
                        QPainterPath clipPath;
                        clipPath.addRect(item->sceneBoundingRect());
                        QPainterPath res = shape.intersected(clipPath);
                        offset = qMax(offset, (int)(res.boundingRect().width() + 0.5));
                    }
                }

                snappedPos += offset;
                // make sure we have no collision
                shape = m_selectionGroup->transitionGroupShape(QPointF(snappedPos - m_selectionGroup->sceneBoundingRect().left(), 0));
                collidingItems = scene()->items(shape, Qt::IntersectsItemShape);
                collidingItems.removeAll(m_selectionGroup);
                for (int i = 0; i < children.count(); i++) {
                    if (children.at(i)->type() == GROUPWIDGET) {
                        QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                        for (int j = 0; j < subchildren.count(); j++)
                            collidingItems.removeAll(subchildren.at(j));
                    }
                    collidingItems.removeAll(children.at(i));
                }
                for (int i = 0; i < collidingItems.count(); i++) {
                    if (collidingItems.at(i)->type() == TRANSITIONWIDGET) {
                        collision = true;
                        break;
                    }
                }
            }

            if (!collision)
                m_selectionGroup->translate(snappedPos - m_selectionGroup->sceneBoundingRect().left(), 0);
            //m_selectionGroup->setPos(mappedXPos + (((int) m_selectionGroup->boundingRect().topLeft().x() + 0.5) - mappedClick) , m_selectionGroup->pos().y());
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
            if (false && !m_blockRefresh && item->type() == AVWIDGET) {
                //TODO: solve crash when showing frame when moving razor over clip
                emit showClipFrame(((ClipItem *) item)->baseClip(), QPoint(), false, mappedXPos - (clip->startPos() - clip->cropStart()).frames(m_document->fps()));
            }
            event->accept();
            return;
        }

        if (m_selectionGroup && clip->parentItem() == m_selectionGroup) {
            // all other modes break the selection, so the user probably wants to move it
            opMode = MOVE;
        } else {
	    if (clip->rect().width() * transform().m11() < 15) {
		// If the item is very small, only allow move
		opMode = MOVE;
	    }
            else opMode = clip->operationMode(mapToScene(event->pos()));
        }

        const double size = 5;
        if (opMode == m_moveOpMode) {
            QGraphicsView::mouseMoveEvent(event);
            return;
        } else {
            removeTipAnimation();
        }
        m_moveOpMode = opMode;
        setTipAnimation(clip, opMode, size);
        ClipItem *ci = NULL;
        if (item->type() == AVWIDGET)
            ci = static_cast <ClipItem *>(item);
        QString message;
        if (opMode == MOVE) {
            setCursor(Qt::OpenHandCursor);
            if (ci) {
                message = ci->clipName() + i18n(":");
                message.append(i18n(" Position:") + m_document->timecode().getDisplayTimecode(ci->info().startPos, KdenliveSettings::frametimecode()));
                message.append(i18n(" Duration:") + m_document->timecode().getDisplayTimecode(ci->cropDuration(),  KdenliveSettings::frametimecode()));
                if (clip->parentItem() && clip->parentItem()->type() == GROUPWIDGET) {
                    AbstractGroupItem *parent = static_cast <AbstractGroupItem *>(clip->parentItem());
                    if (clip->parentItem() == m_selectionGroup)
                        message.append(i18n(" Selection duration:"));
                    else
                        message.append(i18n(" Group duration:"));
                    message.append(m_document->timecode().getDisplayTimecode(parent->duration(), KdenliveSettings::frametimecode()));
                    if (parent->parentItem() && parent->parentItem()->type() == GROUPWIDGET) {
                        AbstractGroupItem *parent2 = static_cast <AbstractGroupItem *>(parent->parentItem());
                        message.append(i18n(" Selection duration:") + m_document->timecode().getDisplayTimecode(parent2->duration(), KdenliveSettings::frametimecode()));
                    }
                }
            }
        } else if (opMode == RESIZESTART) {
            setCursor(KCursor("left_side", Qt::SizeHorCursor));
            if (ci)
                message = i18n("Crop from start: ") + m_document->timecode().getDisplayTimecode(ci->cropStart(), KdenliveSettings::frametimecode());
            if (item->type() == AVWIDGET && item->parentItem() && item->parentItem() != m_selectionGroup)
                message.append(i18n("Use Ctrl to resize only current item, otherwise all items in this group will be resized at once."));
        } else if (opMode == RESIZEEND) {
            setCursor(KCursor("right_side", Qt::SizeHorCursor));
            if (ci)
                message = i18n("Duration: ") + m_document->timecode().getDisplayTimecode(ci->cropDuration(), KdenliveSettings::frametimecode());
            if (item->type() == AVWIDGET && item->parentItem() && item->parentItem() != m_selectionGroup)
                message.append(i18n("Use Ctrl to resize only current item, otherwise all items in this group will be resized at once."));
        } else if (opMode == FADEIN || opMode == FADEOUT) {
            setCursor(Qt::PointingHandCursor);
            if (ci && opMode == FADEIN && ci->fadeIn()) {
                message = i18n("Fade in duration: ");
                message.append(m_document->timecode().getDisplayTimecodeFromFrames(ci->fadeIn(), KdenliveSettings::frametimecode()));
            } else if (ci && opMode == FADEOUT && ci->fadeOut()) {
                message = i18n("Fade out duration: ");
                message.append(m_document->timecode().getDisplayTimecodeFromFrames(ci->fadeOut(), KdenliveSettings::frametimecode()));
            } else {
                message = i18n("Drag to add or resize a fade effect.");
            }
        } else if (opMode == TRANSITIONSTART || opMode == TRANSITIONEND) {
            setCursor(Qt::PointingHandCursor);
            message = i18n("Click to add a transition.");
        } else if (opMode == KEYFRAME) {
            setCursor(Qt::PointingHandCursor);
            emit displayMessage(i18n("Move keyframe above or below clip to remove it, double click to add a new one."), InformationMessage);
        }

        if (!message.isEmpty())
            emit displayMessage(message, InformationMessage);
    } // no clip under mouse
    else if (m_tool == RAZORTOOL) {
        event->accept();
        return;
    } else if (opMode == MOVEGUIDE) {
        m_moveOpMode = opMode;
        setCursor(Qt::SplitHCursor);
    } else {
        removeTipAnimation();
        setCursor(Qt::ArrowCursor);
        if (event->buttons() != Qt::NoButton && event->modifiers() == Qt::NoModifier) {
            QGraphicsView::mouseMoveEvent(event);
            m_moveOpMode = SEEK;
            seekCursorPos(mappedXPos);
            slotCheckPositionScrolling();
            return;
        } else m_moveOpMode = NONE;
    }
    QGraphicsView::mouseMoveEvent(event);
}

// virtual
void CustomTrackView::mousePressEvent(QMouseEvent * event)
{
    setFocus(Qt::MouseFocusReason);
    m_menuPosition = QPoint();

    // special cases (middle click button or ctrl / shift click
    if (event->button() == Qt::MidButton) {
        emit playMonitor();
        m_blockRefresh = false;
        m_operationMode = NONE;
        return;
    }

    if (event->modifiers() & Qt::ShiftModifier) {
        // Rectangle selection
        setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
        setDragMode(QGraphicsView::RubberBandDrag);
        if (!(event->modifiers() & Qt::ControlModifier)) {
            resetSelectionGroup();
            scene()->clearSelection();
        }
        m_blockRefresh = false;
        m_operationMode = RUBBERSELECTION;
        QGraphicsView::mousePressEvent(event);
        return;
    }

    m_blockRefresh = true;
    m_dragGuide = NULL;

    if (m_tool != RAZORTOOL) activateMonitor();
    else if (m_document->renderer()->playSpeed() != 0.0) {
        m_document->renderer()->pause();
        return;
    }
    m_clickEvent = event->pos();

    // check item under mouse
    QList<QGraphicsItem *> collisionList = items(m_clickEvent);
    if (event->modifiers() == Qt::ControlModifier && m_tool != SPACERTOOL && collisionList.count() == 0) {
        // Pressing Ctrl + left mouse button in an empty area scrolls the timeline
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
    AbstractClipItem *collisionClip = NULL;
    bool found = false;
    while (!m_dragGuide && ct < collisionList.count()) {
        if (collisionList.at(ct)->type() == AVWIDGET || collisionList.at(ct)->type() == TRANSITIONWIDGET) {
            collisionClip = static_cast <AbstractClipItem *>(collisionList.at(ct));
            if (collisionClip->isItemLocked())
                break;
            if (collisionClip == m_dragItem)
                collisionClip = NULL;
            else
                m_dragItem = collisionClip;
            found = true;
	    m_dragItem->setProperty("y_absolute", mapToScene(m_clickEvent).y() - m_dragItem->scenePos().y());
            m_dragItemInfo = m_dragItem->info();
            if (m_dragItem->parentItem() && m_dragItem->parentItem()->type() == GROUPWIDGET && m_dragItem->parentItem() != m_selectionGroup) {
                // kDebug()<<"// KLIK FOUND GRP: "<<m_dragItem->sceneBoundingRect();
                dragGroup = static_cast <AbstractGroupItem *>(m_dragItem->parentItem());
            }
            break;
        }
        ct++;
    }
    if (!found) {
        if (m_dragItem) emit clipItemSelected(NULL);
        m_dragItem = NULL;
    }
#if QT_VERSION >= 0x040600
    // Add shadow to dragged item, currently disabled because of painting artifacts
    //TODO: re-enable when fixed
    /*QGraphicsDropShadowEffect *eff = new QGraphicsDropShadowEffect();
    eff->setBlurRadius(5);
    eff->setOffset(3, 3);
    m_dragItem->setGraphicsEffect(eff);*/
#endif
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
        updateClipTypeActions(NULL);
        if (m_tool == SPACERTOOL) {
            QList<QGraphicsItem *> selection;
            if (event->modifiers() == Qt::ControlModifier) {
                // Ctrl + click, select all items on track after click position
                int track = (int)(mapToScene(m_clickEvent).y() / m_tracksHeight);
                if (m_document->trackInfoAt(m_document->tracksCount() - track - 1).isLocked) {
                    // Cannot use spacer on locked track
                    emit displayMessage(i18n("Cannot use spacer in a locked track"), ErrorMessage);
                    return;
                }

                QRectF rect(mapToScene(m_clickEvent).x(), track * m_tracksHeight + m_tracksHeight / 2, sceneRect().width() - mapToScene(m_clickEvent).x(), m_tracksHeight / 2 - 2);

                bool isOk;
                selection = checkForGroups(rect, &isOk);
                if (!isOk) {
                    // groups found on track, do not allow the move
                    emit displayMessage(i18n("Cannot use spacer in a track with a group"), ErrorMessage);
                    return;
                }

                kDebug() << "SPACER TOOL + CTRL, SELECTING ALL CLIPS ON TRACK " << track << " WITH SELECTION RECT " << m_clickEvent.x() << "/" <<  track * m_tracksHeight + 1 << "; " << mapFromScene(sceneRect().width(), 0).x() - m_clickEvent.x() << "/" << m_tracksHeight - 2;
            } else {
                // Select all items on all tracks after click position
                selection = items(event->pos().x(), 1, mapFromScene(sceneRect().width(), 0).x() - event->pos().x(), sceneRect().height());
                kDebug() << "SELELCTING ELEMENTS WITHIN =" << event->pos().x() << "/" <<  1 << ", " << mapFromScene(sceneRect().width(), 0).x() - event->pos().x() << "/" << sceneRect().height();
            }

            QList <GenTime> offsetList;
            // create group to hold selected items
            m_selectionGroup = new AbstractGroupItem(m_document->fps());
            scene()->addItem(m_selectionGroup);

            for (int i = 0; i < selection.count(); i++) {
                if (selection.at(i)->parentItem() == 0 && (selection.at(i)->type() == AVWIDGET || selection.at(i)->type() == TRANSITIONWIDGET)) {
                    AbstractClipItem *item = static_cast<AbstractClipItem *>(selection.at(i));
                    if (item->isItemLocked()) continue;
                    offsetList.append(item->startPos());
                    offsetList.append(item->endPos());
                    m_selectionGroup->addToGroup(selection.at(i));
                    selection.at(i)->setFlag(QGraphicsItem::ItemIsMovable, false);
                } else if (/*selection.at(i)->parentItem() == 0 && */selection.at(i)->type() == GROUPWIDGET) {
                    if (static_cast<AbstractGroupItem *>(selection.at(i))->isItemLocked()) continue;
                    QList<QGraphicsItem *> children = selection.at(i)->childItems();
                    for (int j = 0; j < children.count(); j++) {
                        AbstractClipItem *item = static_cast<AbstractClipItem *>(children.at(j));
                        offsetList.append(item->startPos());
                        offsetList.append(item->endPos());
                    }
                    m_selectionGroup->addToGroup(selection.at(i));
                    selection.at(i)->setFlag(QGraphicsItem::ItemIsMovable, false);
                } else if (selection.at(i)->parentItem() && !selection.contains(selection.at(i)->parentItem())) {
                    if (static_cast<AbstractGroupItem *>(selection.at(i)->parentItem())->isItemLocked()) continue;
                    //AbstractGroupItem *grp = static_cast<AbstractGroupItem *>(selection.at(i)->parentItem());
                    m_selectionGroup->addToGroup(selection.at(i)->parentItem());
                    selection.at(i)->parentItem()->setFlag(QGraphicsItem::ItemIsMovable, false);
                }
            }
            m_spacerOffset = m_selectionGroup->sceneBoundingRect().left() - (int)(mapToScene(m_clickEvent).x());
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
            m_operationMode = SPACER;
        } else {
            seekCursorPos((int)(mapToScene(event->x(), 0).x()));
        }
        //QGraphicsView::mousePressEvent(event);
        event->ignore();
        return;
    }

    // Razor tool
    if (m_tool == RAZORTOOL && m_dragItem) {
        GenTime cutPos = GenTime((int)(mapToScene(event->pos()).x()), m_document->fps());
        if (m_dragItem->type() == TRANSITIONWIDGET) {
            emit displayMessage(i18n("Cannot cut a transition"), ErrorMessage);
        } else {
            m_document->renderer()->pause();
            if (m_dragItem->parentItem() && m_dragItem->parentItem() != m_selectionGroup) {
                razorGroup((AbstractGroupItem *)m_dragItem->parentItem(), cutPos);
            } else {
                AbstractClipItem *clip = static_cast <AbstractClipItem *>(m_dragItem);
                RazorClipCommand* command = new RazorClipCommand(this, clip->info(), cutPos);
                m_commandStack->push(command);
            }
            setDocumentModified();
        }
        m_dragItem = NULL;
        event->accept();
        return;
    }

    bool itemSelected = false;
    if (m_dragItem->isSelected())
        itemSelected = true;
    else if (m_dragItem->parentItem() && m_dragItem->parentItem()->isSelected())
        itemSelected = true;
    else if (dragGroup && dragGroup->isSelected())
        itemSelected = true;

    if (event->modifiers() == Qt::ControlModifier || itemSelected == false) {
        if (event->modifiers() != Qt::ControlModifier) {
            resetSelectionGroup(false);
            m_scene->clearSelection();
            // A refresh seems necessary otherwise in zoomed mode, some clips disappear
            viewport()->update();
        } else resetSelectionGroup();
        dragGroup = NULL;
        if (m_dragItem->parentItem() && m_dragItem->parentItem()->type() == GROUPWIDGET) {
            dragGroup = static_cast <AbstractGroupItem *>(m_dragItem->parentItem());
        }
        bool selected = !m_dragItem->isSelected();
        if (dragGroup)
            dragGroup->setSelected(selected);
        else
            m_dragItem->setSelected(selected);

        groupSelectedItems();
        ClipItem *clip = static_cast <ClipItem *>(m_dragItem);
        updateClipTypeActions(dragGroup == NULL ? clip : NULL);
        m_pasteEffectsAction->setEnabled(m_copiedItems.count() == 1);
    }

    if (collisionClip != NULL || m_dragItem == NULL) {
        if (m_dragItem && m_dragItem->type() == AVWIDGET && !m_dragItem->isItemLocked()) {
            ClipItem *selected = static_cast <ClipItem*>(m_dragItem);
            emit clipItemSelected(selected);
        } else {
            emit clipItemSelected(NULL);
        }
    }

    // If clicked item is selected, allow move
    if (event->modifiers() != Qt::ControlModifier && m_operationMode == NONE) QGraphicsView::mousePressEvent(event);

    m_clickPoint = QPoint((int)(mapToScene(event->pos()).x() - m_dragItem->startPos().frames(m_document->fps())), (int)(event->pos().y() - m_dragItem->pos().y()));
    if (m_selectionGroup && m_dragItem->parentItem() == m_selectionGroup) {
        // all other modes break the selection, so the user probably wants to move it
        m_operationMode = MOVE;
    } else {
	if (m_dragItem->rect().width() * transform().m11() < 15) {
	    // If the item is very small, only allow move
	    m_operationMode = MOVE;
	}
        else m_operationMode = m_dragItem->operationMode(mapToScene(event->pos()));
    }
    m_controlModifier = (event->modifiers() == Qt::ControlModifier);

    // Update snap points
    if (m_selectionGroup == NULL) {
        if (m_operationMode == RESIZEEND || m_operationMode == RESIZESTART)
            updateSnapPoints(NULL);
        else
            updateSnapPoints(m_dragItem);
    } else {
        QList <GenTime> offsetList;
        QList<QGraphicsItem *> children = m_selectionGroup->childItems();
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
        } else {
            GenTime transitionDuration(65, m_document->fps());
            if (m_dragItem->cropDuration() < transitionDuration)
                info.endPos = m_dragItem->endPos();
            else
                info.endPos = info.startPos + transitionDuration;
        }
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
        } else {
            GenTime transitionDuration(65, m_document->fps());
            if (m_dragItem->cropDuration() < transitionDuration) info.startPos = m_dragItem->startPos();
            else info.startPos = info.endPos - transitionDuration;
        }
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
    QGraphicsView::mousePressEvent(event);
}

void CustomTrackView::rebuildGroup(int childTrack, GenTime childPos)
{
    const QPointF p((int)childPos.frames(m_document->fps()), childTrack * m_tracksHeight + m_tracksHeight / 2);
    QList<QGraphicsItem *> list = scene()->items(p);
    AbstractGroupItem *group = NULL;
    for (int i = 0; i < list.size(); i++) {
        if (!list.at(i)->isEnabled()) continue;
        if (list.at(i)->type() == GROUPWIDGET) {
            group = static_cast <AbstractGroupItem *>(list.at(i));
            break;
        }
    }
    rebuildGroup(group);
}

void CustomTrackView::rebuildGroup(AbstractGroupItem *group)
{
    if (group) {
        resetSelectionGroup(false);
        m_scene->clearSelection();

        QList <QGraphicsItem *> children = group->childItems();
        m_document->clipManager()->removeGroup(group);
        scene()->destroyItemGroup(group);
        for (int i = 0; i < children.count(); i++)
            children.at(i)->setSelected(true);
        groupSelectedItems(false, true);
    }
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
            if (children.at(i)->parentItem() == 0 && (children.at(i)->type() == AVWIDGET || children.at(i)->type() == TRANSITIONWIDGET)) {
                if (!static_cast <AbstractClipItem *>(children.at(i))->isItemLocked()) {
                    children.at(i)->setFlag(QGraphicsItem::ItemIsMovable, true);
                    children.at(i)->setSelected(selectItems);
                }
            } else if (children.at(i)->type() == GROUPWIDGET) {
                children.at(i)->setFlag(QGraphicsItem::ItemIsMovable, true);
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
    QRectF rectUnion;
    // Find top left position of selection
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->parentItem() == 0 && (selection.at(i)->type() == AVWIDGET || selection.at(i)->type() == TRANSITIONWIDGET || selection.at(i)->type() == GROUPWIDGET)) {
            rectUnion = rectUnion.united(selection.at(i)->sceneBoundingRect());
        } else if (selection.at(i)->parentItem()) {
            rectUnion = rectUnion.united(selection.at(i)->parentItem()->sceneBoundingRect());
        }
    }

    if (force || selection.count() > 1) {
        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);
        if (createNewGroup) {
            AbstractGroupItem *newGroup = m_document->clipManager()->createGroup();
            newGroup->setPos(rectUnion.left(), rectUnion.top() - 1);
            QPointF diff = newGroup->pos();
            newGroup->translate(-diff.x(), -diff.y());
            //newGroup->translate((int) -rectUnion.left(), (int) -rectUnion.top() + 1);

            scene()->addItem(newGroup);

            // Check if we are trying to include a group in a group
            QList <AbstractGroupItem *> groups;
            for (int i = 0; i < selection.count(); i++) {
                if (selection.at(i)->type() == GROUPWIDGET && !groups.contains(static_cast<AbstractGroupItem *>(selection.at(i))))
                    groups.append(static_cast<AbstractGroupItem *>(selection.at(i)));
                else if (selection.at(i)->parentItem() && !groups.contains(static_cast<AbstractGroupItem *>(selection.at(i)->parentItem())))
                    groups.append(static_cast<AbstractGroupItem *>(selection.at(i)->parentItem()));
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
                    selection.at(i)->setFlag(QGraphicsItem::ItemIsMovable, false);
                }
            }
            KdenliveSettings::setSnaptopoints(snap);
        } else {
            m_selectionGroup = new AbstractGroupItem(m_document->fps());
            m_selectionGroup->setPos(rectUnion.left(), rectUnion.top() - 1);
            QPointF diff = m_selectionGroup->pos();
            //m_selectionGroup->translate((int) - rectUnion.left(), (int) -rectUnion.top() + 1);
            m_selectionGroup->translate(- diff.x(), -diff.y());

            scene()->addItem(m_selectionGroup);
            for (int i = 0; i < selection.count(); i++) {
                if (selection.at(i)->parentItem() == 0 && (selection.at(i)->type() == AVWIDGET || selection.at(i)->type() == TRANSITIONWIDGET || selection.at(i)->type() == GROUPWIDGET)) {
                    m_selectionGroup->addToGroup(selection.at(i));
                    selection.at(i)->setFlag(QGraphicsItem::ItemIsMovable, false);
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
    if (m_dragItem && m_dragItem->hasKeyFrames()) {
        /*if (m_moveOpMode == KEYFRAME) {
            // user double clicked on a keyframe, open edit dialog
            //TODO: update for effects with several values per keyframe
            QDialog d(parentWidget());
            Ui::KeyFrameDialog_UI view;
            view.setupUi(&d);
            view.kfr_position->setText(m_document->timecode().getTimecode(GenTime(m_dragItem->selectedKeyFramePos(), m_document->fps()) - m_dragItem->cropStart()));
            view.kfr_value->setValue(m_dragItem->selectedKeyFrameValue());
            view.kfr_value->setFocus();
            if (d.exec() == QDialog::Accepted) {
                int pos = m_document->timecode().getFrameCount(view.kfr_position->text());
                m_dragItem->updateKeyFramePos(GenTime(pos, m_document->fps()) + m_dragItem->cropStart(), (double) view.kfr_value->value() * m_dragItem->keyFrameFactor());
                ClipItem *item = static_cast <ClipItem *>(m_dragItem);
                QString previous = item->keyframes(item->selectedEffectIndex());
                item->updateKeyframeEffect();
                QString next = item->keyframes(item->selectedEffectIndex());
                EditKeyFrameCommand *command = new EditKeyFrameCommand(this, item->track(), item->startPos(), item->selectedEffectIndex(), previous, next, false);
                m_commandStack->push(command);
                updateEffect(m_document->tracksCount() - item->track(), item->startPos(), item->selectedEffect(), item->selectedEffectIndex());
                emit clipItemSelected(item, item->selectedEffectIndex());
            }

        } else*/  {
            // add keyframe
            GenTime keyFramePos = GenTime((int)(mapToScene(event->pos()).x()), m_document->fps()) - m_dragItem->startPos() + m_dragItem->cropStart();
            int val = m_dragItem->addKeyFrame(keyFramePos, mapToScene(event->pos()).toPoint().y());
            ClipItem * item = static_cast <ClipItem *>(m_dragItem);
            //QString previous = item->keyframes(item->selectedEffectIndex());
            QDomElement oldEffect = item->selectedEffect().cloneNode().toElement();
            item->insertKeyframe(item->getEffectAtIndex(item->selectedEffectIndex()), keyFramePos.frames(m_document->fps()), val);
            //item->updateKeyframeEffect();
            //QString next = item->keyframes(item->selectedEffectIndex());
            QDomElement newEffect = item->selectedEffect().cloneNode().toElement();
            EditEffectCommand *command = new EditEffectCommand(this, m_document->tracksCount() - item->track(), item->startPos(), oldEffect, newEffect, item->selectedEffectIndex(), false, false);
            //EditKeyFrameCommand *command = new EditKeyFrameCommand(this, m_dragItem->track(), m_dragItem->startPos(), item->selectedEffectIndex(), previous, next, false);
            m_commandStack->push(command);
            updateEffect(m_document->tracksCount() - item->track(), item->startPos(), item->selectedEffect());
            emit clipItemSelected(item, item->selectedEffectIndex());
        }
    } else if (m_dragItem && !m_dragItem->isItemLocked()) {
        editItemDuration();
    } else {
        QList<QGraphicsItem *> collisionList = items(event->pos());
        if (collisionList.count() == 1 && collisionList.at(0)->type() == GUIDEITEM) {
            Guide *editGuide = (Guide *) collisionList.at(0);
            if (editGuide) slotEditGuide(editGuide->info());
        }
    }
}

void CustomTrackView::editItemDuration()
{
    AbstractClipItem *item;
    if (m_dragItem) {
        item = m_dragItem;
    } else {
        if (m_scene->selectedItems().count() == 1) {
            item = static_cast <AbstractClipItem *>(m_scene->selectedItems().at(0));
        } else {
            if (m_scene->selectedItems().empty())
                emit displayMessage(i18n("Cannot find clip to edit"), ErrorMessage);
            else
                emit displayMessage(i18n("Cannot edit the duration of multiple items"), ErrorMessage);
            return;
        }
    }

    if (!item) {
        emit displayMessage(i18n("Cannot find clip to edit"), ErrorMessage);
        return;
    }

    if (item->type() == GROUPWIDGET || (item->parentItem() && item->parentItem()->type() == GROUPWIDGET)) {
        emit displayMessage(i18n("Cannot edit an item in a group"), ErrorMessage);
        return;
    }

    if (!item->isItemLocked()) {
        GenTime minimum;
        GenTime maximum;
        if (item->type() == TRANSITIONWIDGET)
            getTransitionAvailableSpace(item, minimum, maximum);
        else
            getClipAvailableSpace(item, minimum, maximum);

        QPointer<ClipDurationDialog> d = new ClipDurationDialog(item,
                               m_document->timecode(), minimum, maximum, this);
        if (d->exec() == QDialog::Accepted) {
            ItemInfo clipInfo = item->info();
            ItemInfo startInfo = clipInfo;
            if (item->type() == TRANSITIONWIDGET) {
                // move & resize transition
                clipInfo.startPos = d->startPos();
                clipInfo.endPos = clipInfo.startPos + d->duration();
                clipInfo.track = item->track();
                MoveTransitionCommand *command = new MoveTransitionCommand(this, startInfo, clipInfo, true);
                updateTrackDuration(clipInfo.track, command);
                m_commandStack->push(command);
            } else {
                // move and resize clip
                ClipItem *clip = static_cast<ClipItem *>(item);
                QUndoCommand *moveCommand = new QUndoCommand();
                moveCommand->setText(i18n("Edit clip"));
                if (d->duration() < item->cropDuration() || d->cropStart() != clipInfo.cropStart) {
                    // duration was reduced, so process it first
                    clipInfo.endPos = clipInfo.startPos + d->duration();
                    clipInfo.cropStart = d->cropStart();

                    resizeClip(startInfo, clipInfo);
                    new ResizeClipCommand(this, startInfo, clipInfo, false, true, moveCommand);
                    adjustEffects(clip, startInfo, moveCommand);
                    new ResizeClipCommand(this, startInfo, clipInfo, false, true, moveCommand);
                }

                if (d->startPos() != clipInfo.startPos) {
                    startInfo = clipInfo;
                    clipInfo.startPos = d->startPos();
                    clipInfo.endPos = item->endPos() + (clipInfo.startPos - startInfo.startPos);
                    new MoveClipCommand(this, startInfo, clipInfo, true, moveCommand);
                }

                if (d->duration() > item->cropDuration()) {
                    // duration was increased, so process it after move
                    startInfo = clipInfo;
                    clipInfo.endPos = clipInfo.startPos + d->duration();
                    clipInfo.cropStart = d->cropStart();

                    resizeClip(startInfo, clipInfo);
                    new ResizeClipCommand(this, startInfo, clipInfo, false, true, moveCommand);
                    adjustEffects(clip, startInfo, moveCommand);
                    new ResizeClipCommand(this, startInfo, clipInfo, false, true, moveCommand);
                }
                updateTrackDuration(clipInfo.track, moveCommand);
                m_commandStack->push(moveCommand);
            }
        }
        delete d;
    } else {
        emit displayMessage(i18n("Item is locked"), ErrorMessage);
    }
}

void CustomTrackView::editKeyFrame(const GenTime & /*pos*/, const int /*track*/, const int /*index*/, const QString & /*keyframes*/)
{
    /*ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()), track);
    if (clip) {
        clip->setKeyframes(index, keyframes);
        updateEffect(m_document->tracksCount() - clip->track(), clip->startPos(), clip->effectAt(index), index, false);
    } else emit displayMessage(i18n("Cannot find clip with keyframe"), ErrorMessage);*/
}


void CustomTrackView::displayContextMenu(QPoint pos, AbstractClipItem *clip, AbstractGroupItem *group)
{
    m_deleteGuide->setEnabled(m_dragGuide != NULL);
    m_editGuide->setEnabled(m_dragGuide != NULL);
    m_markerMenu->clear();
    m_markerMenu->setEnabled(false);
    if (clip == NULL) {
        m_timelineContextMenu->popup(pos);
    } else if (group != NULL) {
        m_pasteEffectsAction->setEnabled(m_copiedItems.count() == 1);
        m_ungroupAction->setEnabled(true);
        updateClipTypeActions(NULL);
        m_timelineContextClipMenu->popup(pos);
    } else {
        m_ungroupAction->setEnabled(false);
        if (clip->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem*>(clip);
            //build go to marker menu
            if (item->baseClip()) {
                QList <CommentedTime> markers = item->baseClip()->commentedSnapMarkers();
                int offset = item->startPos().frames(m_document->fps());
                if (!markers.isEmpty()) {
                    for (int i = 0; i < markers.count(); i++) {
                        int pos = (int) markers.at(i).time().frames(m_document->timecode().fps());
                        QString position = m_document->timecode().getTimecode(markers.at(i).time()) + ' ' + markers.at(i).comment();
                        QAction *go = m_markerMenu->addAction(position);
                        go->setData(pos + offset);
                    }
                }
                m_markerMenu->setEnabled(!m_markerMenu->isEmpty());
            }
            updateClipTypeActions(item);
            m_pasteEffectsAction->setEnabled(m_copiedItems.count() == 1);
            m_timelineContextClipMenu->popup(pos);
        } else if (clip->type() == TRANSITIONWIDGET) {
            m_timelineContextTransitionMenu->popup(pos);
        }
    }
}

void CustomTrackView::activateMonitor()
{
    emit activateDocumentMonitor();
}

void CustomTrackView::insertClipCut(DocClipBase *clip, int in, int out)
{
    resetSelectionGroup();
    ItemInfo info;
    info.startPos = GenTime();
    info.cropStart = GenTime(in, m_document->fps());
    info.endPos = GenTime(out - in, m_document->fps());
    info.cropDuration = info.endPos - info.startPos;
    info.track = 0;

    // Check if clip can be inserted at that position
    ItemInfo pasteInfo = info;
    pasteInfo.startPos = GenTime(m_cursorPos, m_document->fps());
    pasteInfo.endPos = pasteInfo.startPos + info.endPos;
    pasteInfo.track = selectedTrack();
    if (!canBePastedTo(pasteInfo, AVWIDGET)) {
        emit displayMessage(i18n("Cannot insert clip in timeline"), ErrorMessage);
        return;
    }

    AddTimelineClipCommand *command = new AddTimelineClipCommand(this, clip->toXML(), clip->getId(), pasteInfo, EffectsList(), m_scene->editMode() == OVERWRITEEDIT, m_scene->editMode() == INSERTEDIT, true, false);
    updateTrackDuration(pasteInfo.track, command);
    m_commandStack->push(command);

    selectClip(true, false);
    // Automatic audio split
    if (KdenliveSettings::splitaudio())
        splitAudio();
}

bool CustomTrackView::insertDropClips(const QMimeData *data, const QPoint &pos)
{
    if (data->hasFormat("kdenlive/clip")) {
        m_clipDrag = true;
        m_scene->clearSelection();
        resetSelectionGroup(false);
        QStringList list = QString(data->data("kdenlive/clip")).split(';');
        DocClipBase *clip = m_document->getBaseClip(list.at(0));
        if (clip == NULL) {
            kDebug() << " WARNING))))))))) CLIP NOT FOUND : " << list.at(0);
            return false;
        }
        if (clip->getProducer() == NULL) {
            emit displayMessage(i18n("Clip not ready"), ErrorMessage);
            return false;
        }
        QPointF framePos = mapToScene(pos);
        ItemInfo info;
        info.startPos = GenTime();
        info.cropStart = GenTime(list.at(1).toInt(), m_document->fps());
        info.endPos = GenTime(list.at(2).toInt() - list.at(1).toInt(), m_document->fps());
        info.cropDuration = info.endPos - info.startPos;
        info.track = 0;

        // Check if clip can be inserted at that position
        ItemInfo pasteInfo = info;
        pasteInfo.startPos = GenTime((int)(framePos.x() + 0.5), m_document->fps());
        pasteInfo.endPos = pasteInfo.startPos + info.endPos;
        pasteInfo.track = (int)(framePos.y() / m_tracksHeight);
        framePos.setX((int)(framePos.x() + 0.5));
        framePos.setY(pasteInfo.track * m_tracksHeight);
        if (!canBePastedTo(pasteInfo, AVWIDGET)) {
            return true;
        }
        m_selectionGroup = new AbstractGroupItem(m_document->fps());
        ClipItem *item = new ClipItem(clip, info, m_document->fps(), 1.0, 1, getFrameWidth());
        m_selectionGroup->addToGroup(item);
        item->setFlag(QGraphicsItem::ItemIsMovable, false);

        QList <GenTime> offsetList;
        offsetList.append(info.endPos);
        updateSnapPoints(NULL, offsetList);
        m_selectionGroup->setPos(framePos);
        scene()->addItem(m_selectionGroup);
        m_selectionGroup->setSelected(true);
        return true;
    } else if (data->hasFormat("kdenlive/producerslist")) {
        m_clipDrag = true;
        QStringList ids = QString(data->data("kdenlive/producerslist")).split(';');
        m_scene->clearSelection();
        resetSelectionGroup(false);

        QList <GenTime> offsetList;
        QList <ItemInfo> infoList;
        QPointF framePos = mapToScene(pos);
        GenTime start = GenTime((int)(framePos.x() + 0.5), m_document->fps());
        int track = (int)(framePos.y() / m_tracksHeight);
        framePos.setX((int)(framePos.x() + 0.5));
        framePos.setY(track * m_tracksHeight);

        // Check if clips can be inserted at that position
        for (int i = 0; i < ids.size(); ++i) {
            DocClipBase *clip = m_document->getBaseClip(ids.at(i));
            if (clip == NULL) {
                kDebug() << " WARNING))))))))) CLIP NOT FOUND : " << ids.at(i);
                return false;
            }
            if (clip->getProducer() == NULL) {
                emit displayMessage(i18n("Clip not ready"), ErrorMessage);
                return false;
            }
            ItemInfo info;
            info.startPos = start;
            info.cropDuration = clip->duration();
            info.endPos = info.startPos + info.cropDuration;
            info.track = track;
            infoList.append(info);
            start += clip->duration();
        }
        if (!canBePastedTo(infoList, AVWIDGET)) {
            return true;
        }
        m_selectionGroup = new AbstractGroupItem(m_document->fps());
        start = GenTime();
        for (int i = 0; i < ids.size(); ++i) {
            DocClipBase *clip = m_document->getBaseClip(ids.at(i));
            ItemInfo info;
            info.startPos = start;
            info.cropDuration = clip->duration();
            info.endPos = info.startPos + info.cropDuration;
            info.track = 0;
            start += info.cropDuration;
            offsetList.append(start);
            ClipItem *item = new ClipItem(clip, info, m_document->fps(), 1.0, 1, getFrameWidth(), false);
            item->setFlag(QGraphicsItem::ItemIsMovable, false);
            m_selectionGroup->addToGroup(item);
            if (!clip->isPlaceHolder()) m_waitingThumbs.append(item);
        }

        updateSnapPoints(NULL, offsetList);
        m_selectionGroup->setPos(framePos);
        scene()->addItem(m_selectionGroup);
        //m_selectionGroup->setZValue(10);
        m_thumbsTimer.start();
        return true;

    } else {
        // the drag is not a clip (may be effect, ...)
        m_clipDrag = false;
        return false;
    }
}



void CustomTrackView::dragEnterEvent(QDragEnterEvent * event)
{
    if (insertDropClips(event->mimeData(), event->pos())) {
      if (event->source() == this) {
             event->setDropAction(Qt::MoveAction);
             event->accept();
         } else {
	     event->setDropAction(Qt::MoveAction);
             event->acceptProposedAction();
	 }
    } else QGraphicsView::dragEnterEvent(event);
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
    if (!m_document->renderer()->mltRemoveEffect(track, pos, -1, false, false)) {
        emit displayMessage(i18n("Problem deleting effect"), ErrorMessage);
        return;
    }
    bool success = true;
    for (int i = 0; i < clip->effectsCount(); i++) {
        if (!m_document->renderer()->mltAddEffect(track, pos, getEffectArgs(clip->effect(i)), false)) success = false;
    }
    if (!success) emit displayMessage(i18n("Problem adding effect to clip"), ErrorMessage);
    m_document->renderer()->doRefresh();
}

void CustomTrackView::addEffect(int track, GenTime pos, QDomElement effect)
{
    if (pos < GenTime()) {
        // Add track effect
        if (effect.attribute("id") == "speed") {
            emit displayMessage(i18n("Cannot add speed effect to track"), ErrorMessage);
            return;
        }
        clearSelection();
        m_document->addTrackEffect(track - 1, effect);
        m_document->renderer()->mltAddTrackEffect(track, getEffectArgs(effect));
        emit updateTrackEffectState(track - 1);
        emit showTrackEffects(track, m_document->trackInfoAt(track - 1));
        return;
    }
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()), m_document->tracksCount() - track);
    if (clip) {
        // Special case: speed effect
        if (effect.attribute("id") == "speed") {
            if (clip->clipType() != VIDEO && clip->clipType() != AV && clip->clipType() != PLAYLIST) {
                emit displayMessage(i18n("Problem adding effect to clip"), ErrorMessage);
                return;
            }
            QLocale locale;
            double speed = locale.toDouble(EffectsList::parameter(effect, "speed")) / 100.0;
            int strobe = EffectsList::parameter(effect, "strobe").toInt();
            if (strobe == 0) strobe = 1;
            doChangeClipSpeed(clip->info(), clip->speedIndependantInfo(), speed, 1.0, strobe, clip->baseClip()->getId());
            EffectsParameterList params = clip->addEffect(effect);
            m_document->renderer()->mltAddEffect(track, pos, params);
            if (clip->isSelected()) emit clipItemSelected(clip);
            return;
        }
        EffectsParameterList params = clip->addEffect(effect);
        if (!m_document->renderer()->mltAddEffect(track, pos, params))
            emit displayMessage(i18n("Problem adding effect to clip"), ErrorMessage);
	clip->setSelectedEffect(params.paramValue("kdenlive_ix").toInt());
        if (clip->isSelected()) emit clipItemSelected(clip);
    } else emit displayMessage(i18n("Cannot find clip to add effect"), ErrorMessage);
}

void CustomTrackView::deleteEffect(int track, GenTime pos, QDomElement effect)
{
    QString index = effect.attribute("kdenlive_ix");
    if (pos < GenTime()) {
        // Delete track effect
        if (m_document->renderer()->mltRemoveTrackEffect(track, index.toInt(), true)) {
	    m_document->removeTrackEffect(track - 1, effect);
	}
	else emit displayMessage(i18n("Problem deleting effect"), ErrorMessage);
        emit updateTrackEffectState(track - 1);
        emit showTrackEffects(track, m_document->trackInfoAt(track - 1));
        return;
    }
    // Special case: speed effect
    if (effect.attribute("id") == "speed") {
        ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()), m_document->tracksCount() - track);
        if (clip) {
            doChangeClipSpeed(clip->info(), clip->speedIndependantInfo(), 1.0, clip->speed(), 1, clip->baseClip()->getId());
            clip->deleteEffect(index);
            emit clipItemSelected(clip);
            m_document->renderer()->mltRemoveEffect(track, pos, index.toInt(), true);
            return;
        }
    }
    if (!m_document->renderer()->mltRemoveEffect(track, pos, index.toInt(), true)) {
        kDebug() << "// ERROR REMOV EFFECT: " << index << ", DISABLE: " << effect.attribute("disable");
        emit displayMessage(i18n("Problem deleting effect"), ErrorMessage);
        return;
    }
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()), m_document->tracksCount() - track);
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
    int offset = effect.attribute("clipstart").toInt();
    QDomElement namenode = effect.firstChildElement("name");
    if (!namenode.isNull()) effectName = i18n(namenode.text().toUtf8().data());
    else effectName = i18n("effect");
    effectCommand->setText(i18n("Add %1", effectName));
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            if (effect.tagName() == "effectgroup") {
		QDomNodeList effectlist = effect.elementsByTagName("effect");
		for (int j = 0; j < effectlist.count(); j++) {
		    QDomElement subeffect = effectlist.at(j).toElement();
		    if (subeffect.hasAttribute("kdenlive_info")) {
			// effect is in a group
			EffectInfo effectInfo;
			effectInfo.fromString(subeffect.attribute("kdenlive_info"));
			if (effectInfo.groupIndex < 0) {
			    // group needs to be appended
			    effectInfo.groupIndex = item->nextFreeEffectGroupIndex();
			    subeffect.setAttribute("kdenlive_info", effectInfo.toString());
			}
		    }
		    processEffect(item, subeffect, offset, effectCommand);
		}
	    }
            else {
		processEffect(item, effect, offset, effectCommand);
	    }
        }
    }
    if (effectCommand->childCount() > 0) {
        m_commandStack->push(effectCommand);
        setDocumentModified();
    } else delete effectCommand;
}

void CustomTrackView::slotAddEffect(ClipItem *clip, QDomElement effect)
{
    if (clip) slotAddEffect(effect, clip->startPos(), clip->track());
}

void CustomTrackView::slotAddEffect(QDomElement effect, GenTime pos, int track)
{
    QList<QGraphicsItem *> itemList;
    QUndoCommand *effectCommand = new QUndoCommand();
    QString effectName;
    
    int offset = effect.attribute("clipstart").toInt();
    if (effect.tagName() == "effectgroup") {
	effectName = effect.attribute("name");
    } else {
	QDomElement namenode = effect.firstChildElement("name");
	if (!namenode.isNull()) effectName = i18n(namenode.text().toUtf8().data());
	else effectName = i18n("effect");
    }
    effectCommand->setText(i18n("Add %1", effectName));

    if (track == -1) itemList = scene()->selectedItems();
    if (itemList.isEmpty()) {
        ClipItem *clip = getClipItemAt((int) pos.frames(m_document->fps()), track);
        if (clip) itemList.append(clip);
        else emit displayMessage(i18n("Select a clip if you want to apply an effect"), ErrorMessage);
    }

    //expand groups
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == GROUPWIDGET) {
            QList<QGraphicsItem *> subitems = itemList.at(i)->childItems();
            for (int j = 0; j < subitems.count(); j++) {
                if (!itemList.contains(subitems.at(j))) itemList.append(subitems.at(j));
            }
        }
    }

    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
	    if (effect.tagName() == "effectgroup") {
		QDomNodeList effectlist = effect.elementsByTagName("effect");
		for (int j = 0; j < effectlist.count(); j++) {
		    QDomElement subeffect = effectlist.at(j).toElement();
		    if (subeffect.hasAttribute("kdenlive_info")) {
			// effect is in a group
			EffectInfo effectInfo;
			effectInfo.fromString(subeffect.attribute("kdenlive_info"));
			if (effectInfo.groupIndex < 0) {
			    // group needs to be appended
			    effectInfo.groupIndex = item->nextFreeEffectGroupIndex();
			    subeffect.setAttribute("kdenlive_info", effectInfo.toString());
			}
		    }
		    processEffect(item, subeffect, offset, effectCommand);
		}
	    }
            else processEffect(item, effect, offset, effectCommand);
        }
    }
    if (effectCommand->childCount() > 0) {
        m_commandStack->push(effectCommand);
        setDocumentModified();
	if (effectCommand->childCount() == 1) {
	    // Display newly added clip effect
	    for (int i = 0; i < itemList.count(); i++) {
		if (itemList.at(i)->type() == AVWIDGET) {
		    ClipItem *clip = static_cast<ClipItem *>(itemList.at(i));
		    clip->setSelectedEffect(clip->effectsCount());
		    if (!clip->isSelected()) {
			clearSelection(false);
			clip->setSelected(true);
			m_dragItem = clip;
			emit clipItemSelected(clip);
		    }
		    break;
		}
	    }
	}
    } else delete effectCommand;
}

void CustomTrackView::processEffect(ClipItem *item, QDomElement effect, int offset, QUndoCommand *effectCommand)
{
    if (effect.attribute("type") == "audio") {
	// Don't add audio effects on video clips
        if (item->isVideoOnly() || (item->clipType() != AUDIO && item->clipType() != AV && item->clipType() != PLAYLIST)) {
	    /* do not show error message when item is part of a group as the user probably knows what he does then
            * and the message is annoying when working with the split audio feature */
            if (!item->parentItem() || item->parentItem() == m_selectionGroup)
		emit displayMessage(i18n("Cannot add an audio effect to this clip"), ErrorMessage);
            return;
        }
    } else if (effect.attribute("type") == "video" || !effect.hasAttribute("type")) {
	// Don't add video effect on audio clips
        if (item->isAudioOnly() || item->clipType() == AUDIO) {
	    /* do not show error message when item is part of a group as the user probably knows what he does then
            * and the message is annoying when working with the split audio feature */
            if (!item->parentItem() || item->parentItem() == m_selectionGroup)
		emit displayMessage(i18n("Cannot add a video effect to this clip"), ErrorMessage);
            return;
        }
    }
    if (effect.attribute("unique", "0") != "0" && item->hasEffect(effect.attribute("tag"), effect.attribute("id")) != -1) {
	emit displayMessage(i18n("Effect already present in clip"), ErrorMessage);
        return;
    }
    if (item->isItemLocked()) {
	return;
    }

    if (effect.attribute("id") == "freeze" && m_cursorPos > item->startPos().frames(m_document->fps()) && m_cursorPos < item->endPos().frames(m_document->fps())) {
	item->initEffect(effect, m_cursorPos - item->startPos().frames(m_document->fps()), offset);
    } else {
	item->initEffect(effect, 0, offset);
    }
    new AddEffectCommand(this, m_document->tracksCount() - item->track(), item->startPos(), effect, true, effectCommand);
}

void CustomTrackView::slotDeleteEffect(ClipItem *clip, int track, QDomElement effect, bool affectGroup)
{
    if (clip == NULL) {
        // delete track effect
        AddEffectCommand *command = new AddEffectCommand(this, track, GenTime(-1), effect, false);
        m_commandStack->push(command);
        setDocumentModified();
        return;
    }
    if (affectGroup && clip->parentItem() && clip->parentItem() == m_selectionGroup) {
        //clip is in a group, also remove the effect in other clips of the group
        QList<QGraphicsItem *> items = m_selectionGroup->childItems();
        QUndoCommand *delCommand = new QUndoCommand();
        QString effectName;
        QDomElement namenode = effect.firstChildElement("name");
        if (!namenode.isNull()) effectName = i18n(namenode.text().toUtf8().data());
        else effectName = i18n("effect");
        delCommand->setText(i18n("Delete %1", effectName));

        //expand groups
        for (int i = 0; i < items.count(); i++) {
            if (items.at(i)->type() == GROUPWIDGET) {
                QList<QGraphicsItem *> subitems = items.at(i)->childItems();
                for (int j = 0; j < subitems.count(); j++) {
                    if (!items.contains(subitems.at(j))) items.append(subitems.at(j));
                }
            }
        }

        for (int i = 0; i < items.count(); i++) {
            if (items.at(i)->type() == AVWIDGET) {
                ClipItem *item = static_cast <ClipItem *>(items.at(i));
                int ix = item->hasEffect(effect.attribute("tag"), effect.attribute("id"));
                if (ix != -1) {
                    QDomElement eff = item->effectAtIndex(ix);
                    new AddEffectCommand(this, m_document->tracksCount() - item->track(), item->startPos(), eff, false, delCommand);
                }
            }
        }
        if (delCommand->childCount() > 0)
            m_commandStack->push(delCommand);
        else
            delete delCommand;
        setDocumentModified();
        return;
    }
    AddEffectCommand *command = new AddEffectCommand(this, m_document->tracksCount() - clip->track(), clip->startPos(), effect, false);
    m_commandStack->push(command);
    setDocumentModified();
}

void CustomTrackView::updateEffect(int track, GenTime pos, QDomElement insertedEffect, bool updateEffectStack)
{
    if (insertedEffect.isNull()) {
	kDebug()<<"// Trying to add null effect";
        emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
        return;
    }
    int ix = insertedEffect.attribute("kdenlive_ix").toInt();
    QDomElement effect = insertedEffect.cloneNode().toElement();
    //kDebug() << "// update effect ix: " << effect.attribute("kdenlive_ix")<<", GAIN: "<<EffectsList::parameter(effect, "gain");
    if (pos < GenTime()) {
        // editing a track effect
        EffectsParameterList effectParams = getEffectArgs(effect);
        // check if we are trying to reset a keyframe effect
        /*if (effectParams.hasParam("keyframes") && effectParams.paramValue("keyframes").isEmpty()) {
            clip->initEffect(effect);
            effectParams = getEffectArgs(effect);
        }*/
        if (!m_document->renderer()->mltEditEffect(m_document->tracksCount() - track, pos, effectParams)) {
            emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
	}
        m_document->setTrackEffect(m_document->tracksCount() - track - 1, ix, effect);
        emit updateTrackEffectState(track);
        setDocumentModified();
        return;

    }
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()), m_document->tracksCount() - track);
    if (clip) {
        // Special case: speed effect
        if (effect.attribute("id") == "speed") {
            if (effect.attribute("disable") == "1") {
                doChangeClipSpeed(clip->info(), clip->speedIndependantInfo(), 1.0, clip->speed(), 1, clip->baseClip()->getId());
            } else {
                QLocale locale;
                double speed = locale.toDouble(EffectsList::parameter(effect, "speed")) / 100.0;
                int strobe = EffectsList::parameter(effect, "strobe").toInt();
                if (strobe == 0) strobe = 1;
                doChangeClipSpeed(clip->info(), clip->speedIndependantInfo(), speed, clip->speed(), strobe, clip->baseClip()->getId());
            }
            clip->updateEffect(effect);
	    if (updateEffectStack && clip->isSelected())
		emit clipItemSelected(clip);
	    if (ix == clip->selectedEffectIndex()) {
		// make sure to update display of clip keyframes
		clip->setSelectedEffect(ix);
	    }
            return;
        }

        EffectsParameterList effectParams = getEffectArgs(effect);
        // check if we are trying to reset a keyframe effect
        if (effectParams.hasParam("keyframes") && effectParams.paramValue("keyframes").isEmpty()) {
            clip->initEffect(effect);
            effectParams = getEffectArgs(effect);
        }

        if (effect.attribute("tag") == "volume" || effect.attribute("tag") == "brightness") {
            // A fade effect was modified, update the clip
            if (effect.attribute("id") == "fadein" || effect.attribute("id") == "fade_from_black") {
                int pos = EffectsList::parameter(effect, "out").toInt() - EffectsList::parameter(effect, "in").toInt();
                clip->setFadeIn(pos);
            }
            if (effect.attribute("id") == "fadeout" || effect.attribute("id") == "fade_to_black") {
                int pos = EffectsList::parameter(effect, "out").toInt() - EffectsList::parameter(effect, "in").toInt();
                clip->setFadeOut(pos);
            }
        }
	bool success = m_document->renderer()->mltEditEffect(m_document->tracksCount() - clip->track(), clip->startPos(), effectParams);

        if (success) {
	    clip->updateEffect(effect);
	    if (updateEffectStack && clip->isSelected()) {
		emit clipItemSelected(clip);
	    }
	    if (ix == clip->selectedEffectIndex()) {
		// make sure to update display of clip keyframes
		clip->setSelectedEffect(ix);
	    }
	}
	else emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
    }
    else emit displayMessage(i18n("Cannot find clip to update effect"), ErrorMessage);
    setDocumentModified();
}

void CustomTrackView::updateEffectState(int track, GenTime pos, QList <int> effectIndexes, bool disable, bool updateEffectStack)
{
    if (pos < GenTime()) {
        // editing a track effect
        if (!m_document->renderer()->mltEnableEffects(m_document->tracksCount() - track, pos, effectIndexes, disable)) {
            emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
	    return;
	}
        m_document->enableTrackEffects(m_document->tracksCount() - track - 1, effectIndexes, disable);
        emit updateTrackEffectState(track);
        setDocumentModified();
        return;
    }
    // editing a clip effect
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()), m_document->tracksCount() - track);
    if (clip) {
	bool success = m_document->renderer()->mltEnableEffects(m_document->tracksCount() - clip->track(), clip->startPos(), effectIndexes, disable);
	if (success) {
	    clip->enableEffects(effectIndexes, disable);
	    if (updateEffectStack && clip->isSelected()) {
		emit clipItemSelected(clip);
	    }
	    if (effectIndexes.contains(clip->selectedEffectIndex())) {
		// make sure to update display of clip keyframes
		clip->setSelectedEffect(clip->selectedEffectIndex());
	    }
	}
	else emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
    }
    else emit displayMessage(i18n("Cannot find clip to update effect"), ErrorMessage);
}

void CustomTrackView::moveEffect(int track, GenTime pos, QList <int> oldPos, QList <int> newPos)
{
    if (pos < GenTime()) {
        // Moving track effect
        int documentTrack = m_document->tracksCount() - track - 1;
        int max = m_document->getTrackEffects(documentTrack).count();
	int new_position = newPos.at(0);
	if (new_position > max) {
	    new_position = max;
	}
	int old_position = oldPos.at(0);
	for (int i = 0; i < newPos.count(); i++) {
	    QDomElement act = m_document->getTrackEffect(documentTrack, new_position);
	    if (old_position > new_position) {
		// Moving up, we need to adjust index
		old_position = oldPos.at(i);
		new_position = newPos.at(i);
	    }
	    QDomElement before = m_document->getTrackEffect(documentTrack, old_position);
	    if (!act.isNull() && !before.isNull()) {
		m_document->setTrackEffect(documentTrack, new_position, before);
		m_document->renderer()->mltMoveEffect(m_document->tracksCount() - track, pos, old_position, new_position);
	    } else emit displayMessage(i18n("Cannot move effect"), ErrorMessage);
	}
	emit showTrackEffects(m_document->tracksCount() - track, m_document->trackInfoAt(documentTrack));
        return;
    }
    ClipItem *clip = getClipItemAt((int)pos.frames(m_document->fps()), m_document->tracksCount() - track);
    if (clip) {
	int new_position = newPos.at(0);
	if (new_position > clip->effectsCount()) {
	    new_position = clip->effectsCount();
	}
	int old_position = oldPos.at(0);
	for (int i = 0; i < newPos.count(); i++) {
	    QDomElement act = clip->effectAtIndex(new_position);
	    if (old_position > new_position) {
		// Moving up, we need to adjust index
		old_position = oldPos.at(i);
		new_position = newPos.at(i);
	    }
	    QDomElement before = clip->effectAtIndex(old_position);
	    if (act.isNull() || before.isNull()) {
		emit displayMessage(i18n("Cannot move effect"), ErrorMessage);
		return;
	    }
	    clip->moveEffect(before, new_position);
	    // special case: speed effect, which is a pseudo-effect, not appearing in MLT's effects
	    if (act.attribute("id") == "speed") {
		m_document->renderer()->mltUpdateEffectPosition(track, pos, old_position, new_position);
	    } else if (before.attribute("id") == "speed") {
		m_document->renderer()->mltUpdateEffectPosition(track, pos, new_position, old_position);
	    } else m_document->renderer()->mltMoveEffect(track, pos, old_position, new_position);
	}
	clip->setSelectedEffect(newPos.at(0));
	emit clipItemSelected(clip);
        setDocumentModified();
    } else emit displayMessage(i18n("Cannot move effect"), ErrorMessage);
}

void CustomTrackView::slotChangeEffectState(ClipItem *clip, int track, QList <int> effectIndexes, bool disable)
{
    ChangeEffectStateCommand *command;
    if (clip == NULL) {
        // editing track effect
        command = new ChangeEffectStateCommand(this, m_document->tracksCount() - track, GenTime(-1), effectIndexes, disable, false, true);
    } else {
	// Check if we have a speed effect, disabling / enabling it needs a special procedure since it is a pseudoo effect
	QList <int> speedEffectIndexes;
	for (int i = 0; i < effectIndexes.count(); i++) {
	    QDomElement effect = clip->effectAtIndex(effectIndexes.at(i));
	    if (effect.attribute("id") == "speed") {
		// speed effect
		speedEffectIndexes << effectIndexes.at(i);
		QDomElement newEffect = effect.cloneNode().toElement();
		newEffect.setAttribute("disable", (int) disable);
		EditEffectCommand *editcommand = new EditEffectCommand(this, m_document->tracksCount() - clip->track(), clip->startPos(), effect, newEffect, effectIndexes.at(i), false, true);
		m_commandStack->push(editcommand);
	    }
	}
	for (int j = 0; j < speedEffectIndexes.count(); j++) {
	    effectIndexes.removeAll(speedEffectIndexes.at(j));
	}
        command = new ChangeEffectStateCommand(this, m_document->tracksCount() - clip->track(), clip->startPos(), effectIndexes, disable, false, true);
    }
    m_commandStack->push(command);
    setDocumentModified();
}

void CustomTrackView::slotChangeEffectPosition(ClipItem *clip, int track, QList <int> currentPos, int newPos)
{
    MoveEffectCommand *command;
    if (clip == NULL) {
        // editing track effect
        command = new MoveEffectCommand(this, m_document->tracksCount() - track, GenTime(-1), currentPos, newPos);
    } else command = new MoveEffectCommand(this, m_document->tracksCount() - clip->track(), clip->startPos(), currentPos, newPos);
    m_commandStack->push(command);
    setDocumentModified();
}

void CustomTrackView::slotUpdateClipEffect(ClipItem *clip, int track, QDomElement oldeffect, QDomElement effect, int ix, bool refreshEffectStack)
{
    EditEffectCommand *command;
    if (clip) command = new EditEffectCommand(this, m_document->tracksCount() - clip->track(), clip->startPos(), oldeffect, effect, ix, refreshEffectStack, true);
    else command = new EditEffectCommand(this, m_document->tracksCount() - track, GenTime(-1), oldeffect, effect, ix, refreshEffectStack, true);
    m_commandStack->push(command);
}

void CustomTrackView::slotUpdateClipRegion(ClipItem *clip, int ix, QString region)
{
    QDomElement effect = clip->getEffectAtIndex(ix);
    QDomElement oldeffect = effect.cloneNode().toElement();
    effect.setAttribute("region", region);
    EditEffectCommand *command = new EditEffectCommand(this, m_document->tracksCount() - clip->track(), clip->startPos(), oldeffect, effect, ix, true, true);
    m_commandStack->push(command);
}

ClipItem *CustomTrackView::cutClip(ItemInfo info, GenTime cutTime, bool cut, bool execute)
{
    if (cut) {
        // cut clip
        ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()), info.track);
        if (!item || cutTime >= item->endPos() || cutTime <= item->startPos()) {
            emit displayMessage(i18n("Cannot find clip to cut"), ErrorMessage);
            if (item)
                kDebug() << "/////////  ERROR CUTTING CLIP : (" << item->startPos().frames(25) << "-" << item->endPos().frames(25) << "), INFO: (" << info.startPos.frames(25) << "-" << info.endPos.frames(25) << ")" << ", CUT: " << cutTime.frames(25);
            else
                kDebug() << "/// ERROR NO CLIP at: " << info.startPos.frames(m_document->fps()) << ", track: " << info.track;
            m_blockRefresh = false;
            return NULL;
        }

        if (execute) {
	    if (!m_document->renderer()->mltCutClip(m_document->tracksCount() - info.track, cutTime)) {
		// Error cuting clip in playlist
		m_blockRefresh = false;
		return NULL;
	    }
	}
        int cutPos = (int) cutTime.frames(m_document->fps());
        ItemInfo newPos;
        newPos.startPos = cutTime;
        newPos.endPos = info.endPos;
        newPos.cropStart = item->info().cropStart + (cutTime - info.startPos);
        newPos.track = info.track;
        newPos.cropDuration = newPos.endPos - newPos.startPos;

        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);
        ClipItem *dup = item->clone(newPos);

        // remove unwanted effects
        // fade in from 2nd part of the clip
        int ix = dup->hasEffect(QString(), "fadein");
        if (ix != -1) {
            QDomElement oldeffect = dup->effectAtIndex(ix);
            dup->deleteEffect(oldeffect.attribute("kdenlive_ix"));
        }
        ix = dup->hasEffect(QString(), "fade_from_black");
        if (ix != -1) {
            QDomElement oldeffect = dup->effectAtIndex(ix);
            dup->deleteEffect(oldeffect.attribute("kdenlive_ix"));
        }
        // fade out from 1st part of the clip
        ix = item->hasEffect(QString(), "fadeout");
        if (ix != -1) {
            QDomElement oldeffect = item->effectAtIndex(ix);
            item->deleteEffect(oldeffect.attribute("kdenlive_ix"));
        }
        ix = item->hasEffect(QString(), "fade_to_black");
        if (ix != -1) {
            QDomElement oldeffect = item->effectAtIndex(ix);
            item->deleteEffect(oldeffect.attribute("kdenlive_ix"));
        }


        item->resizeEnd(cutPos);
        scene()->addItem(dup);
        if (item->checkKeyFrames())
            slotRefreshEffects(item);
        if (dup->checkKeyFrames())
            slotRefreshEffects(dup);

        item->baseClip()->addReference();
        m_document->updateClip(item->baseClip()->getId());
        setDocumentModified();
        KdenliveSettings::setSnaptopoints(snap);
        if (execute && item->isSelected())
            emit clipItemSelected(item);
        return dup;
    } else {
        // uncut clip

        ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()), info.track);
        ClipItem *dup = getClipItemAt((int) cutTime.frames(m_document->fps()), info.track);

        if (!item || !dup || item == dup) {
            emit displayMessage(i18n("Cannot find clip to uncut"), ErrorMessage);
            m_blockRefresh = false;
            return NULL;
        }
        if (m_document->renderer()->mltRemoveClip(m_document->tracksCount() - info.track, cutTime) == false) {
            emit displayMessage(i18n("Error removing clip at %1 on track %2", m_document->timecode().getTimecodeFromFrames(cutTime.frames(m_document->fps())), info.track), ErrorMessage);
            return NULL;
        }

        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);

        // join fade effects again
        int ix = dup->hasEffect(QString(), "fadeout");
        if (ix != -1) {
            QDomElement effect = dup->effectAtIndex(ix);
            item->addEffect(effect);
        }
        ix = dup->hasEffect(QString(), "fade_to_black");
        if (ix != -1) {
            QDomElement effect = dup->effectAtIndex(ix);
            item->addEffect(effect);
        }

        m_waitingThumbs.removeAll(dup);
        bool selected = item->isSelected();
        if (dup->isSelected()) {
            selected = true;
            item->setSelected(true);
            emit clipItemSelected(NULL);
        }
        dup->baseClip()->removeReference();
        m_document->updateClip(dup->baseClip()->getId());
        scene()->removeItem(dup);
        delete dup;
        dup = NULL;

        ItemInfo clipinfo = item->info();
        clipinfo.track = m_document->tracksCount() - clipinfo.track;
        bool success = m_document->renderer()->mltResizeClipEnd(clipinfo, info.endPos - info.startPos);
        if (success) {
            item->resizeEnd((int) info.endPos.frames(m_document->fps()));
            setDocumentModified();
        } else {
            emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
        }
        KdenliveSettings::setSnaptopoints(snap);
        if (execute && selected)
            emit clipItemSelected(item);
        return item;
    }
    //QTimer::singleShot(3000, this, SLOT(slotEnableRefresh()));
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

void CustomTrackView::addTransition(ItemInfo transitionInfo, int endTrack, QDomElement params, bool refresh)
{
    Transition *tr = new Transition(transitionInfo, endTrack, m_document->fps(), params, true);
    //kDebug() << "---- ADDING transition " << params.attribute("value");
    if (m_document->renderer()->mltAddTransition(tr->transitionTag(), endTrack, m_document->tracksCount() - transitionInfo.track, transitionInfo.startPos, transitionInfo.endPos, tr->toXML(), refresh)) {
        scene()->addItem(tr);
        setDocumentModified();
    } else {
        emit displayMessage(i18n("Cannot add transition"), ErrorMessage);
        delete tr;
    }
}

void CustomTrackView::deleteTransition(ItemInfo transitionInfo, int endTrack, QDomElement /*params*/, bool refresh)
{
    Transition *item = getTransitionItemAt(transitionInfo.startPos, transitionInfo.track);
    if (!item) {
        emit displayMessage(i18n("Select clip to delete"), ErrorMessage);
        return;
    }
    m_document->renderer()->mltDeleteTransition(item->transitionTag(), endTrack, m_document->tracksCount() - transitionInfo.track, transitionInfo.startPos, transitionInfo.endPos, item->toXML(), refresh);
    if (m_dragItem == item) m_dragItem = NULL;

#if QT_VERSION >= 0x040600
    // animate item deletion
    item->closeAnimation();
#else
    delete item;
#endif
    emit transitionItemSelected(NULL);
    setDocumentModified();
}

void CustomTrackView::slotTransitionUpdated(Transition *tr, QDomElement old)
{
    //kDebug() << "TRANS UPDATE, TRACKS: " << old.attribute("transition_btrack") << ", NEW: " << tr->toXML().attribute("transition_btrack");
    QDomElement xml = tr->toXML();
    if (old.isNull() || xml.isNull()) {
        emit displayMessage(i18n("Cannot update transition"), ErrorMessage);
        return;
    }
    EditTransitionCommand *command = new EditTransitionCommand(this, tr->track(), tr->startPos(), old, xml, false);
    updateTrackDuration(tr->track(), command);
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
    
    bool force = false;
    if (oldTransition.attribute("transition_atrack") != transition.attribute("transition_atrack") || oldTransition.attribute("transition_btrack") != transition.attribute("transition_btrack"))
        force = true;
    m_document->renderer()->mltUpdateTransition(oldTransition.attribute("tag"), transition.attribute("tag"), transition.attribute("transition_btrack").toInt(), m_document->tracksCount() - transition.attribute("transition_atrack").toInt(), item->startPos(), item->endPos(), transition, force);
    //kDebug() << "ORIGINAL TRACK: "<< oldTransition.attribute("transition_btrack") << ", NEW TRACK: "<<transition.attribute("transition_btrack");
    item->setTransitionParameters(transition);
    if (updateTransitionWidget) {
        ItemInfo info = item->info();
        QPoint p;
        ClipItem *transitionClip = getClipItemAt(info.startPos, info.track);
        if (transitionClip && transitionClip->baseClip()) {
            QString size = transitionClip->baseClip()->getProperty("frame_size");
            double factor = transitionClip->baseClip()->getProperty("aspect_ratio").toDouble();
            if (factor == 0) factor = 1.0;
            p.setX((int)(size.section('x', 0, 0).toInt() * factor + 0.5));
            p.setY(size.section('x', 1, 1).toInt());
        }
        emit transitionItemSelected(item, getPreviousVideoTrack(info.track), p, true);
    }
    setDocumentModified();
}

void CustomTrackView::dragMoveEvent(QDragMoveEvent * event)
{
    if (m_clipDrag) {
        const QPointF pos = mapToScene(event->pos());
        if (m_selectionGroup) {
            m_selectionGroup->setPos(pos);
            emit mousePosition((int)(m_selectionGroup->scenePos().x() + 0.5));
            event->acceptProposedAction();
        } else {
            // Drag enter was not possible, try again at mouse position
            insertDropClips(event->mimeData(), event->pos());
            event->accept();
        }
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
        QList <ClipItem *> brokenClips;

        for (int i = 0; i < items.count(); i++) {
            ClipItem *item = static_cast <ClipItem *>(items.at(i));
            if (!hasVideoClip && (item->clipType() == AV || item->clipType() == VIDEO)) hasVideoClip = true;
            if (items.count() == 1) {
                updateClipTypeActions(item);
            } else {
                updateClipTypeActions(NULL);
            }

            //TODO: take care of edit mode for undo
            item->baseClip()->addReference();
            //item->setZValue(item->defaultZValue());
            m_document->updateClip(item->baseClip()->getId());
            ItemInfo info = item->info();

            int tracknumber = m_document->tracksCount() - info.track - 1;
            bool isLocked = m_document->trackInfoAt(tracknumber).isLocked;
            if (isLocked) item->setItemLocked(true);
            ItemInfo clipInfo = info;
            clipInfo.track = m_document->tracksCount() - item->track();

            int worked = m_document->renderer()->mltInsertClip(clipInfo, item->xml(), item->baseClip()->getProducer(item->track()), m_scene->editMode() == OVERWRITEEDIT, m_scene->editMode() == INSERTEDIT);
            if (worked == -1) {
                emit displayMessage(i18n("Cannot insert clip in timeline"), ErrorMessage);
                brokenClips.append(item);
                continue;
            }
            adjustTimelineClips(m_scene->editMode(), item, ItemInfo(), addCommand);

            new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), item->info(), item->effectList(), m_scene->editMode() == OVERWRITEEDIT, m_scene->editMode() == INSERTEDIT, false, false, addCommand);
            updateTrackDuration(info.track, addCommand);

            if (item->baseClip()->isTransparent() && getTransitionItemAtStart(info.startPos, info.track) == NULL) {
                // add transparency transition
                QDomElement trans = MainWindow::transitions.getEffectByTag("composite", "composite").cloneNode().toElement();
                new AddTransitionCommand(this, info, getPreviousVideoTrack(info.track), trans, false, true, addCommand);
            }
            item->setSelected(true);
        }
        qDeleteAll(brokenClips);
        brokenClips.clear();
        if (addCommand->childCount() > 0) m_commandStack->push(addCommand);
        else delete addCommand;

        // Automatic audio split
        if (KdenliveSettings::splitaudio())
            splitAudio();
        setDocumentModified();

        /*
        // debug info
        QRectF rect(0, 1 * m_tracksHeight + m_tracksHeight / 2, sceneRect().width(), 2);
        QList<QGraphicsItem *> selection = m_scene->items(rect);
        QStringList timelineList;

        kDebug()<<"// ITEMS on TRACK: "<<selection.count();
        for (int i = 0; i < selection.count(); i++) {
               if (selection.at(i)->type() == AVWIDGET) {
                   ClipItem *clip = static_cast <ClipItem *>(selection.at(i));
                   int start = clip->startPos().frames(m_document->fps());
                   int end = clip->endPos().frames(m_document->fps());
                   timelineList.append(QString::number(start) + "-" + QString::number(end));
            }
        }
        kDebug() << "// COMPARE:\n" << timelineList << "\n-------------------";
        */

        m_pasteEffectsAction->setEnabled(m_copiedItems.count() == 1);
        if (items.count() > 1) {
            groupSelectedItems(true);
        } else if (items.count() == 1) {
            m_dragItem = static_cast <AbstractClipItem *>(items.at(0));
            emit clipItemSelected((ClipItem*)m_dragItem, false);
        }
        event->setDropAction(Qt::MoveAction);
        event->accept();

        /// \todo enable when really working
//        alignAudio();

    } else QGraphicsView::dropEvent(event);
    setFocus();
}

void CustomTrackView::adjustTimelineClips(EDITMODE mode, ClipItem *item, ItemInfo posinfo, QUndoCommand *command)
{
    bool snap = KdenliveSettings::snaptopoints();
    KdenliveSettings::setSnaptopoints(false);
    if (mode == OVERWRITEEDIT) {
        // if we are in overwrite mode, move clips accordingly
        ItemInfo info;
        if (item == NULL) info = posinfo;
        else info = item->info();
        QRectF rect(info.startPos.frames(m_document->fps()), info.track * m_tracksHeight + m_tracksHeight / 2, (info.endPos - info.startPos).frames(m_document->fps()) - 1, 5);
        QList<QGraphicsItem *> selection = m_scene->items(rect);
        if (item) selection.removeAll(item);
        for (int i = 0; i < selection.count(); i++) {
            if (!selection.at(i)->isEnabled()) continue;
            if (selection.at(i)->type() == AVWIDGET) {
                ClipItem *clip = static_cast<ClipItem *>(selection.at(i));
                if (clip->startPos() < info.startPos) {
                    if (clip->endPos() > info.endPos) {
                        ItemInfo clipInfo = clip->info();
                        ItemInfo dupInfo = clipInfo;
                        GenTime diff = info.startPos - clipInfo.startPos;
                        dupInfo.startPos = info.startPos;
                        dupInfo.cropStart += diff;
                        dupInfo.cropDuration = clipInfo.endPos - info.startPos;
                        ItemInfo newdupInfo = dupInfo;
                        GenTime diff2 = info.endPos - info.startPos;
                        newdupInfo.startPos = info.endPos;
                        newdupInfo.cropStart += diff2;
                        newdupInfo.cropDuration = clipInfo.endPos - info.endPos;
                        new RazorClipCommand(this, clipInfo, info.startPos, false, command);
                        new ResizeClipCommand(this, dupInfo, newdupInfo, false, false, command);
                        ClipItem *dup = cutClip(clipInfo, info.startPos, true, false);
                        if (dup) dup->resizeStart(info.endPos.frames(m_document->fps()));
                    } else {
                        ItemInfo newclipInfo = clip->info();
                        newclipInfo.endPos = info.startPos;
                        new ResizeClipCommand(this, clip->info(), newclipInfo, false, false, command);
                        clip->resizeEnd(info.startPos.frames(m_document->fps()));
                    }
                } else if (clip->endPos() <= info.endPos) {
                    new AddTimelineClipCommand(this, clip->xml(), clip->clipProducer(), clip->info(), clip->effectList(), false, false, false, true, command);
                    m_waitingThumbs.removeAll(clip);
                    scene()->removeItem(clip);
                    delete clip;
                    clip = NULL;
                } else {
                    ItemInfo newclipInfo = clip->info();
                    newclipInfo.startPos = info.endPos;
                    new ResizeClipCommand(this, clip->info(), newclipInfo, false, false, command);
                    clip->resizeStart(info.endPos.frames(m_document->fps()));
                }
            }
        }
    } else if (mode == INSERTEDIT) {
        // if we are in push mode, move clips accordingly
        ItemInfo info;
        if (item == NULL) info = posinfo;
        else info = item->info();
        QRectF rect(info.startPos.frames(m_document->fps()), info.track * m_tracksHeight + m_tracksHeight / 2, (info.endPos - info.startPos).frames(m_document->fps()) - 1, 5);
        QList<QGraphicsItem *> selection = m_scene->items(rect);
        if (item) selection.removeAll(item);
        for (int i = 0; i < selection.count(); i++) {
            if (selection.at(i)->type() == AVWIDGET) {
                ClipItem *clip = static_cast<ClipItem *>(selection.at(i));
                if (clip->startPos() < info.startPos) {
                    if (clip->endPos() > info.startPos) {
                        ItemInfo clipInfo = clip->info();
                        ItemInfo dupInfo = clipInfo;
                        GenTime diff = info.startPos - clipInfo.startPos;
                        dupInfo.startPos = info.startPos;
                        dupInfo.cropStart += diff;
                        dupInfo.cropDuration = clipInfo.endPos - info.startPos;
                        new RazorClipCommand(this, clipInfo, info.startPos, false, command);
                        // Commented out; variable dup unused. --granjow
                        //ClipItem *dup = cutClip(clipInfo, info.startPos, true, false);
                        cutClip(clipInfo, info.startPos, true, false);
                    }
                }
                // TODO: add insertspacecommand
            }
        }
    }

    KdenliveSettings::setSnaptopoints(snap);
}


void CustomTrackView::adjustTimelineTransitions(EDITMODE mode, Transition *item, QUndoCommand *command)
{
    if (mode == OVERWRITEEDIT) {
        // if we are in overwrite or push mode, move clips accordingly
        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);
        ItemInfo info = item->info();
        QRectF rect(info.startPos.frames(m_document->fps()), info.track * m_tracksHeight + m_tracksHeight, (info.endPos - info.startPos).frames(m_document->fps()) - 1, 5);
        QList<QGraphicsItem *> selection = m_scene->items(rect);
        selection.removeAll(item);
        for (int i = 0; i < selection.count(); i++) {
            if (!selection.at(i)->isEnabled()) continue;
            if (selection.at(i)->type() == TRANSITIONWIDGET) {
                Transition *tr = static_cast<Transition *>(selection.at(i));
                if (tr->startPos() < info.startPos) {
                    ItemInfo firstPos = tr->info();
                    ItemInfo newPos = firstPos;
                    firstPos.endPos = item->startPos();
                    newPos.startPos = item->endPos();
                    new MoveTransitionCommand(this, tr->info(), firstPos, true, command);
                    if (tr->endPos() > info.endPos) {
                        // clone transition
                        new AddTransitionCommand(this, newPos, tr->transitionEndTrack(), tr->toXML(), false, true, command);
                    }
                } else if (tr->endPos() > info.endPos) {
                    // just resize
                    ItemInfo firstPos = tr->info();
                    firstPos.startPos = item->endPos();
                    new MoveTransitionCommand(this, tr->info(), firstPos, true, command);
                } else {
                    // remove transition
                    new AddTransitionCommand(this, tr->info(), tr->transitionEndTrack(), tr->toXML(), true, true, command);
                }
            }
        }
        KdenliveSettings::setSnaptopoints(snap);
    }
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
    QList <TransitionInfo> transitionInfos;
    if (ix == -1 || ix == m_document->tracksCount()) {
        m_document->insertTrack(0, type);
        transitionInfos = m_document->renderer()->mltInsertTrack(1, type.type == VIDEOTRACK);
    } else {
        m_document->insertTrack(m_document->tracksCount() - ix, type);
        // insert track in MLT playlist
        transitionInfos = m_document->renderer()->mltInsertTrack(m_document->tracksCount() - ix, type.type == VIDEOTRACK);

        double startY = ix * m_tracksHeight + 1 + m_tracksHeight / 2;
        QRectF r(0, startY, sceneRect().width(), sceneRect().height() - startY);
        QList<QGraphicsItem *> selection = m_scene->items(r);
        resetSelectionGroup();

        m_selectionGroup = new AbstractGroupItem(m_document->fps());
        scene()->addItem(m_selectionGroup);
        for (int i = 0; i < selection.count(); i++) {
            if ((!selection.at(i)->parentItem()) && (selection.at(i)->type() == AVWIDGET || selection.at(i)->type() == TRANSITIONWIDGET || selection.at(i)->type() == GROUPWIDGET)) {
                m_selectionGroup->addToGroup(selection.at(i));
                selection.at(i)->setFlag(QGraphicsItem::ItemIsMovable, false);
            }
        }
        // Move graphic items
        m_selectionGroup->translate(0, m_tracksHeight);

        // adjust track number
        Mlt::Tractor *tractor = m_document->renderer()->lockService();
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
                // slowmotion clips are not track dependant, so no need to update them
                if (clip->speed() != 1.0) continue;
                // We add a move clip command so that we get the correct producer for new track number
                if (clip->clipType() == AV || clip->clipType() == AUDIO) {
                    Mlt::Producer *prod = clip->getProducer(clipinfo.track);
                    if (m_document->renderer()->mltUpdateClipProducer(tractor, (int)(m_document->tracksCount() - clipinfo.track), clipinfo.startPos.frames(m_document->fps()), prod) == false) {
                        // problem updating clip
                        emit displayMessage(i18n("Cannot update clip (time: %1, track: %2)", clipinfo.startPos.frames(m_document->fps()), clipinfo.track), ErrorMessage);
                    }
                }
            } /*else if (item->type() == TRANSITIONWIDGET) {
                Transition *tr = static_cast <Transition *>(item);
                int track = tr->transitionEndTrack();
                if (track >= ix) {
                    tr->updateTransitionEndTrack(getPreviousVideoTrack(clipinfo.track));
                }
            }*/
        }
        // Sync transition tracks with MLT playlist
        Transition *tr;        
	TransitionInfo info;
	for (int i = 0; i < transitionInfos.count(); i++) {
	    info = transitionInfos.at(i);
	    tr = getTransitionItem(info);
	    if (tr) tr->setForcedTrack(info.forceTrack, info.a_track);
	    else kDebug()<<"// Cannot update TRANSITION AT: "<<info.b_track<<" / "<<info.startPos.frames(m_document->fps()); 
	}
	
        resetSelectionGroup(false);
        m_document->renderer()->unlockService(tractor);
    }

    int maxHeight = m_tracksHeight * m_document->tracksCount() * matrix().m22();
    for (int i = 0; i < m_guides.count(); i++) {
        QLineF l = m_guides.at(i)->line();
        l.setP2(QPointF(l.x2(), maxHeight));
        m_guides.at(i)->setLine(l);
    }

    m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), maxHeight - 1);
    setSceneRect(0, 0, sceneRect().width(), m_tracksHeight * m_document->tracksCount());
    viewport()->update();
    //QTimer::singleShot(500, this, SIGNAL(trackHeightChanged()));
    //setFixedHeight(50 * m_tracksCount);

    updateTrackNames(ix, true);
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
            selection.at(i)->setFlag(QGraphicsItem::ItemIsMovable, false);
        }
    }
    // Move graphic items
    qreal ydiff = 0 - (int) m_tracksHeight;
    m_selectionGroup->translate(0, ydiff);
    Mlt::Tractor *tractor = m_document->renderer()->lockService();

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
            if (clip->clipType() == AV || clip->clipType() == AUDIO || clip->clipType() == PLAYLIST) {
                Mlt::Producer *prod = clip->getProducer(clipinfo.track);
                if (prod == NULL || !m_document->renderer()->mltUpdateClipProducer(tractor, (int)(m_document->tracksCount() - clipinfo.track), clipinfo.startPos.frames(m_document->fps()), prod)) {
                    emit displayMessage(i18n("Cannot update clip (time: %1, track: %2)", clipinfo.startPos.frames(m_document->fps()), clipinfo.track), ErrorMessage);
                }
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
    m_document->renderer()->unlockService(tractor);

    int maxHeight = m_tracksHeight * m_document->tracksCount() * matrix().m22();
    for (int i = 0; i < m_guides.count(); i++) {
        QLineF l = m_guides.at(i)->line();
        l.setP2(QPointF(l.x2(), maxHeight));
        m_guides.at(i)->setLine(l);
    }
    m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), maxHeight - 1);
    setSceneRect(0, 0, sceneRect().width(), m_tracksHeight * m_document->tracksCount());

    m_selectedTrack = qMin(m_selectedTrack, m_document->tracksCount() - 1);
    viewport()->update();

    updateTrackNames(ix, false);
    //QTimer::singleShot(500, this, SIGNAL(trackHeightChanged()));
}

void CustomTrackView::configTracks(QList < TrackInfo > trackInfos)
{
    for (int i = 0; i < trackInfos.count(); ++i) {
        m_document->setTrackType(i, trackInfos.at(i));
        m_document->renderer()->mltChangeTrackState(i + 1, m_document->trackInfoAt(i).isMute, m_document->trackInfoAt(i).isBlind);
        lockTrack(m_document->tracksCount() - i - 1, m_document->trackInfoAt(i).isLocked, false);
    }

    viewport()->update();
    emit trackHeightChanged();
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


void CustomTrackView::lockTrack(int ix, bool lock, bool requestUpdate)
{
    int tracknumber = m_document->tracksCount() - ix - 1;
    m_document->switchTrackLock(tracknumber, lock);
    if (requestUpdate)
        emit doTrackLock(ix, lock);
    AbstractClipItem *clip = NULL;
    QList<QGraphicsItem *> selection = m_scene->items(0, ix * m_tracksHeight + m_tracksHeight / 2, sceneRect().width(), m_tracksHeight / 2 - 2);

    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == GROUPWIDGET && (AbstractGroupItem *)selection.at(i) != m_selectionGroup) {
            if (selection.at(i)->parentItem() && m_selectionGroup) {
                selection.removeAll((QGraphicsItem*)m_selectionGroup);
                resetSelectionGroup();
            }

            bool changeGroupLock = true;
            bool hasClipOnTrack = false;
            QList <QGraphicsItem *> children =  selection.at(i)->childItems();
            for (int j = 0; j < children.count(); ++j) {
                if (children.at(j)->isSelected()) {
                    if (children.at(j)->type() == AVWIDGET)
                        emit clipItemSelected(NULL);
                    else if (children.at(j)->type() == TRANSITIONWIDGET)
                        emit transitionItemSelected(NULL);
                    else
                        continue;
                }

                AbstractClipItem * child = static_cast <AbstractClipItem *>(children.at(j));
                if (child == m_dragItem)
                    m_dragItem = NULL;

                // only unlock group, if it is not locked by another track too
                if (!lock && child->track() != ix && m_document->trackInfoAt(m_document->tracksCount() - child->track() - 1).isLocked)
                    changeGroupLock = false;
                
                // only (un-)lock if at least one clip is on the track
                if (child->track() == ix)
                    hasClipOnTrack = true;
            }
            if (changeGroupLock && hasClipOnTrack)
                ((AbstractGroupItem*)selection.at(i))->setItemLocked(lock);
        } else if((selection.at(i)->type() == AVWIDGET || selection.at(i)->type() == TRANSITIONWIDGET)) {
            if (selection.at(i)->parentItem()) {
                if (selection.at(i)->parentItem() == m_selectionGroup) {
                    selection.removeAll((QGraphicsItem*)m_selectionGroup);
                    resetSelectionGroup();
                } else {
                    // groups are handled separately
                    continue;
                }
            }

            if (selection.at(i)->isSelected()) {
                if (selection.at(i)->type() == AVWIDGET)
                    emit clipItemSelected(NULL);
                else
                    emit transitionItemSelected(NULL);
            }
            clip = static_cast <AbstractClipItem *>(selection.at(i));
            clip->setItemLocked(lock);
            if (clip == m_dragItem)
                m_dragItem = NULL;
        }
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

QList<QGraphicsItem *> CustomTrackView::checkForGroups(const QRectF &rect, bool *ok)
{
    // Check there is no group going over several tracks there, or that would result in timeline corruption
    QList<QGraphicsItem *> selection = scene()->items(rect);
    *ok = true;
    int maxHeight = m_tracksHeight * 1.5;
    for (int i = 0; i < selection.count(); i++) {
        // Check that we don't try to move a group with clips on other tracks
        if (selection.at(i)->type() == GROUPWIDGET && (selection.at(i)->boundingRect().height() >= maxHeight)) {
            *ok = false;
            break;
        } else if (selection.at(i)->parentItem() && (selection.at(i)->parentItem()->boundingRect().height() >= maxHeight)) {
            *ok = false;
            break;
        }
    }
    return selection;
}

void CustomTrackView::slotRemoveSpace()
{
    GenTime pos;
    int track = 0;
    if (m_menuPosition.isNull()) {
        pos = GenTime(cursorPos(), m_document->fps());

        QPointer<TrackDialog> d = new TrackDialog(m_document, parentWidget());
        d->comboTracks->setCurrentIndex(m_selectedTrack);
        d->label->setText(i18n("Track"));
        d->before_select->setHidden(true);
        d->setWindowTitle(i18n("Remove Space"));
        d->video_track->setHidden(true);
        d->audio_track->setHidden(true);
        if (d->exec() != QDialog::Accepted) {
            delete d;
            return;
        }
        track = d->comboTracks->currentIndex();
        delete d;
    } else {
        pos = GenTime((int)(mapToScene(m_menuPosition).x()), m_document->fps());
        track = (int)(mapToScene(m_menuPosition).y() / m_tracksHeight);
    }

    if (m_document->isTrackLocked(m_document->tracksCount() - track - 1)) {
        emit displayMessage(i18n("Cannot remove space in a locked track"), ErrorMessage);
        return;
    }

    ClipItem *item = getClipItemAt(pos, track);
    if (item) {
        emit displayMessage(i18n("You must be in an empty space to remove space (time: %1, track: %2)", m_document->timecode().getTimecodeFromFrames(mapToScene(m_menuPosition).x()), track), ErrorMessage);
        return;
    }
    int length = m_document->renderer()->mltGetSpaceLength(pos, m_document->tracksCount() - track, true);
    if (length <= 0) {
        emit displayMessage(i18n("You must be in an empty space to remove space (time: %1, track: %2)", m_document->timecode().getTimecodeFromFrames(mapToScene(m_menuPosition).x()), track), ErrorMessage);
        return;
    }

    // Make sure there is no group in the way
    QRectF rect(pos.frames(m_document->fps()), track * m_tracksHeight + m_tracksHeight / 2, sceneRect().width() - pos.frames(m_document->fps()), m_tracksHeight / 2 - 2);

    bool isOk;
    QList<QGraphicsItem *> items = checkForGroups(rect, &isOk);
    if (!isOk) {
        // groups found on track, do not allow the move
        emit displayMessage(i18n("Cannot remove space in a track with a group"), ErrorMessage);
        return;
    }

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

    if (!transitionsToMove.isEmpty()) {
        // Make sure that by moving the items, we don't get a transition collision
        // Find first transition
        ItemInfo info = transitionsToMove.at(0);
        for (int i = 1; i < transitionsToMove.count(); i++)
            if (transitionsToMove.at(i).startPos < info.startPos) info = transitionsToMove.at(i);

        // make sure there are no transitions on the way
        QRectF rect(info.startPos.frames(m_document->fps()) - length, track * m_tracksHeight + m_tracksHeight / 2, length - 1, m_tracksHeight / 2 - 2);
        items = scene()->items(rect);
        int transitionCorrection = -1;
        for (int i = 0; i < items.count(); i++) {
            if (items.at(i)->type() == TRANSITIONWIDGET) {
                // There is a transition on the way
                AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
                int transitionEnd = item->endPos().frames(m_document->fps());
                if (transitionEnd > transitionCorrection) transitionCorrection = transitionEnd;
            }
        }

        if (transitionCorrection > 0) {
            // We need to fix the move length
            length = info.startPos.frames(m_document->fps()) - transitionCorrection;
        }
            
        // Make sure we don't send transition before 0
        if (info.startPos.frames(m_document->fps()) < length) {
            // reduce length to maximum possible
            length = info.startPos.frames(m_document->fps());
        }           
    }

    InsertSpaceCommand *command = new InsertSpaceCommand(this, clipsToMove, transitionsToMove, track, GenTime(-length, m_document->fps()), true);
    updateTrackDuration(track, command);
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
    QPointer<SpacerDialog> d = new SpacerDialog(GenTime(65, m_document->fps()),
                m_document->timecode(), track, m_document->tracksList(), this);
    if (d->exec() != QDialog::Accepted) {
        delete d;
        return;
    }
    GenTime spaceDuration = d->selectedDuration();
    track = d->selectedTrack();
    delete d;

    QList<QGraphicsItem *> items;
    if (track >= 0) {
        if (m_document->isTrackLocked(m_document->tracksCount() - track - 1)) {
            emit displayMessage(i18n("Cannot insert space in a locked track"), ErrorMessage);
            return;
        }

        ClipItem *item = getClipItemAt(pos, track);
        if (item) pos = item->startPos();

        // Make sure there is no group in the way
        QRectF rect(pos.frames(m_document->fps()), track * m_tracksHeight + m_tracksHeight / 2, sceneRect().width() - pos.frames(m_document->fps()), m_tracksHeight / 2 - 2);
        bool isOk;
        items = checkForGroups(rect, &isOk);
        if (!isOk) {
            // groups found on track, do not allow the move
            emit displayMessage(i18n("Cannot insert space in a track with a group"), ErrorMessage);
            return;
        }
    } else {
        QRectF rect(pos.frames(m_document->fps()), 0, sceneRect().width() - pos.frames(m_document->fps()), m_document->tracksCount() * m_tracksHeight);
        items = scene()->items(rect);
    }

    QList<ItemInfo> clipsToMove;
    QList<ItemInfo> transitionsToMove;

    for (int i = 0; i < items.count(); i++) {
        if (items.at(i)->type() == AVWIDGET || items.at(i)->type() == TRANSITIONWIDGET) {
            AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
            ItemInfo info = item->info();
            if (item->type() == AVWIDGET)
                clipsToMove.append(info);
            else if (item->type() == TRANSITIONWIDGET)
                transitionsToMove.append(info);
        }
    }

    if (!clipsToMove.isEmpty() || !transitionsToMove.isEmpty()) {
        InsertSpaceCommand *command = new InsertSpaceCommand(this, clipsToMove, transitionsToMove, track, spaceDuration, true);
        updateTrackDuration(track, command);
        m_commandStack->push(command);
    }
}

void CustomTrackView::insertSpace(QList<ItemInfo> clipsToMove, QList<ItemInfo> transToMove, int track, const GenTime &duration, const GenTime &offset)
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
                    clip->parentItem()->setFlag(QGraphicsItem::ItemIsMovable, false);
                } else {
                    m_selectionGroup->addToGroup(clip);
                    clip->setFlag(QGraphicsItem::ItemIsMovable, false);
                }
                if (trackClipStartList.value(m_document->tracksCount() - clipsToMove.at(i).track) == -1 || clipsToMove.at(i).startPos.frames(m_document->fps()) < trackClipStartList.value(m_document->tracksCount() - clipsToMove.at(i).track))
                    trackClipStartList[m_document->tracksCount() - clipsToMove.at(i).track] = clipsToMove.at(i).startPos.frames(m_document->fps());
            } else emit {
                    displayMessage(i18n("Cannot move clip at position %1, track %2", m_document->timecode().getTimecodeFromFrames((clipsToMove.at(i).startPos + offset).frames(m_document->fps())), clipsToMove.at(i).track), ErrorMessage);
                }
            }
    if (!transToMove.isEmpty()) for (int i = 0; i < transToMove.count(); i++) {
            transition = getTransitionItemAtStart(transToMove.at(i).startPos + offset, transToMove.at(i).track);
            if (transition) {
                if (transition->parentItem()) {
                    m_selectionGroup->addToGroup(transition->parentItem());
                    transition->parentItem()->setFlag(QGraphicsItem::ItemIsMovable, false);
                } else {
                    m_selectionGroup->addToGroup(transition);
                    transition->setFlag(QGraphicsItem::ItemIsMovable, false);
                }
                if (trackTransitionStartList.value(m_document->tracksCount() - transToMove.at(i).track) == -1 || transToMove.at(i).startPos.frames(m_document->fps()) < trackTransitionStartList.value(m_document->tracksCount() - transToMove.at(i).track))
                    trackTransitionStartList[m_document->tracksCount() - transToMove.at(i).track] = transToMove.at(i).startPos.frames(m_document->fps());
            } else emit displayMessage(i18n("Cannot move transition at position %1, track %2", m_document->timecode().getTimecodeFromFrames(transToMove.at(i).startPos.frames(m_document->fps())), transToMove.at(i).track), ErrorMessage);
        }
    m_selectionGroup->translate(diff, 0);

    // update items coordinates
    QList<QGraphicsItem *> itemList = m_selectionGroup->childItems();

    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET || itemList.at(i)->type() == TRANSITIONWIDGET) {
            static_cast < AbstractClipItem *>(itemList.at(i))->updateItem();
        } else if (itemList.at(i)->type() == GROUPWIDGET) {
            QList<QGraphicsItem *> children = itemList.at(i)->childItems();
            for (int j = 0; j < children.count(); j++) {
                AbstractClipItem * clp = static_cast < AbstractClipItem *>(children.at(j));
                clp->updateItem();
            }
        }
    }
    resetSelectionGroup(false);
    if (track != -1)
        track = m_document->tracksCount() - track;
    m_document->renderer()->mltInsertSpace(trackClipStartList, trackTransitionStartList, track, duration, offset);
}

void CustomTrackView::deleteClip(const QString &clipId)
{
    resetSelectionGroup();
    QList<QGraphicsItem *> itemList = items();
    QUndoCommand *deleteCommand = new QUndoCommand();
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
                new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), item->info(), item->effectList(), false, false, true, true, deleteCommand);
            }
        }
    }
    deleteCommand->setText(i18np("Delete timeline clip", "Delete timeline clips", count));
    if (count == 0) {
        delete deleteCommand;
    } else {
        updateTrackDuration(-1, deleteCommand);
        m_commandStack->push(deleteCommand);
    }
}

void CustomTrackView::seekCursorPos(int pos)
{
    m_document->renderer()->seek(pos);
    emit updateRuler();
}

int CustomTrackView::seekPosition() const
{
    return m_document->renderer()->requestedSeekPosition;
}


void CustomTrackView::setCursorPos(int pos, bool seek)
{
    if (pos != m_cursorPos) {
	emit cursorMoved((int)(m_cursorPos), (int)(pos));
	m_cursorPos = pos;
	m_cursorLine->setPos(m_cursorPos, 0);
    }
    else if (m_autoScroll) checkScrolling();
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
    int currentPos = m_document->renderer()->requestedSeekPosition;
    if (currentPos == SEEK_INACTIVE) currentPos = m_document->renderer()->seekFramePosition();
    if (currentPos + delta < 0) delta = 0 - currentPos;
    currentPos += delta;
    m_document->renderer()->seek(currentPos);
    emit updateRuler();
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
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
#if QT_VERSION >= 0x040600
    if (m_dragItem) m_dragItem->setGraphicsEffect(NULL);
#endif
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
        GenTime timeOffset = GenTime((int)(m_selectionGroup->scenePos().x()), m_document->fps()) - m_selectionGroupInfo.startPos;

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

            for (int i = 0; i < items.count(); i++) {
                if (items.at(i)->type() == GROUPWIDGET)
                    items += items.at(i)->childItems();
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
            if (!clipsToMove.isEmpty() || !transitionsToMove.isEmpty()) {
                InsertSpaceCommand *command = new InsertSpaceCommand(this, clipsToMove, transitionsToMove, track, timeOffset, false);
                updateTrackDuration(track, command);
                m_commandStack->push(command);
                if (track != -1) track = m_document->tracksCount() - track;
                kDebug() << "SPACER TRACK:" << track;
                m_document->renderer()->mltInsertSpace(trackClipStartList, trackTransitionStartList, track, timeOffset, GenTime());
                setDocumentModified();
            }
        }
        resetSelectionGroup(false);
        m_operationMode = NONE;
    } else if (m_operationMode == RUBBERSELECTION) {
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
                Mlt::Producer *prod = item->getProducer(info.track);
                bool success = m_document->renderer()->mltMoveClip((int)(m_document->tracksCount() - m_dragItemInfo.track), (int)(m_document->tracksCount() - info.track), (int) m_dragItemInfo.startPos.frames(m_document->fps()), (int)(info.startPos.frames(m_document->fps())), prod, m_scene->editMode() == OVERWRITEEDIT, m_scene->editMode() == INSERTEDIT);

                if (success) {
                    QUndoCommand *moveCommand = new QUndoCommand();
                    moveCommand->setText(i18n("Move clip"));
                    adjustTimelineClips(m_scene->editMode(), item, ItemInfo(), moveCommand);

                    int tracknumber = m_document->tracksCount() - item->track() - 1;
                    bool isLocked = m_document->trackInfoAt(tracknumber).isLocked;
                    if (isLocked) item->setItemLocked(true);
                    new MoveClipCommand(this, m_dragItemInfo, info, false, moveCommand);
                    // Also move automatic transitions (on lower track)
                    Transition *startTransition = getTransitionItemAtStart(m_dragItemInfo.startPos, m_dragItemInfo.track);
                    ItemInfo startTrInfo;
                    ItemInfo newStartTrInfo;
                    bool moveStartTrans = false;
                    bool moveEndTrans = false;
                    if (startTransition && startTransition->isAutomatic()) {
                        startTrInfo = startTransition->info();
                        newStartTrInfo = startTrInfo;
                        newStartTrInfo.track = info.track;
                        newStartTrInfo.startPos = info.startPos;
                        if (m_dragItemInfo.track == info.track && !item->baseClip()->isTransparent() && getClipItemAtEnd(newStartTrInfo.endPos, m_document->tracksCount() - startTransition->transitionEndTrack())) {
                            // transition end should stay the same
                        } else {
                            // transition end should be adjusted to clip
                            newStartTrInfo.endPos = newStartTrInfo.endPos + (newStartTrInfo.startPos - startTrInfo.startPos);
                        }
                        if (newStartTrInfo.startPos < newStartTrInfo.endPos) moveStartTrans = true;
                    }
                    if (startTransition == NULL || startTransition->endPos() < m_dragItemInfo.endPos) {
                        // Check if there is a transition at clip end
                        Transition *tr = getTransitionItemAtEnd(m_dragItemInfo.endPos, m_dragItemInfo.track);
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
                            if (newTrInfo.startPos < newTrInfo.endPos) {
                                moveEndTrans = true;
                                if (moveStartTrans) {
                                    // we have to move both transitions, remove the start one so that there is no collision
                                    new AddTransitionCommand(this, startTrInfo, startTransition->transitionEndTrack(), startTransition->toXML(), true, true, moveCommand);
                                }
                                adjustTimelineTransitions(m_scene->editMode(), tr, moveCommand);
                                new MoveTransitionCommand(this, trInfo, newTrInfo, true, moveCommand);
                                if (moveStartTrans) {
                                    // re-add transition in correct place
                                    int transTrack = startTransition->transitionEndTrack();
                                    if (m_dragItemInfo.track != info.track && !startTransition->forcedTrack()) {
                                        transTrack = getPreviousVideoTrack(info.track);
                                    }
                                    adjustTimelineTransitions(m_scene->editMode(), startTransition, moveCommand);
                                    new AddTransitionCommand(this, newStartTrInfo, transTrack, startTransition->toXML(), false, true, moveCommand);
                                }
                            }
                        }
                    }

                    if (moveStartTrans && !moveEndTrans) {
                        adjustTimelineTransitions(m_scene->editMode(), startTransition, moveCommand);
                        new MoveTransitionCommand(this, startTrInfo, newStartTrInfo, true, moveCommand);
                    }

                    // Also move automatic transitions (on upper track)
                    Transition *tr = getTransitionItemAtStart(m_dragItemInfo.startPos, m_dragItemInfo.track - 1);
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
                            if (newTrInfo.startPos < newTrInfo.endPos) {
                                adjustTimelineTransitions(m_scene->editMode(), tr, moveCommand);
                                new MoveTransitionCommand(this, trInfo, newTrInfo, true, moveCommand);
                            }
                        }
                    }
                    if (m_dragItemInfo.track == info.track && (tr == NULL || tr->endPos() < m_dragItemInfo.endPos)) {
                        // Check if there is a transition at clip end
                        tr = getTransitionItemAtEnd(m_dragItemInfo.endPos, m_dragItemInfo.track - 1);
                        if (tr && tr->isAutomatic() && (m_document->tracksCount() - tr->transitionEndTrack()) == m_dragItemInfo.track) {
                            ItemInfo trInfo = tr->info();
                            ItemInfo newTrInfo = trInfo;
                            newTrInfo.endPos = m_dragItem->endPos();
                            kDebug() << "CLIP ENDS AT: " << newTrInfo.endPos.frames(25);
                            kDebug() << "CLIP STARTS AT: " << newTrInfo.startPos.frames(25);
                            ClipItem * upperClip = getClipItemAt(m_dragItemInfo.startPos, m_dragItemInfo.track - 1);
                            if (!upperClip || !upperClip->baseClip()->isTransparent()) {
                                if (!getClipItemAtStart(trInfo.startPos, tr->track())) {
                                    // transition start should be moved
                                    newTrInfo.startPos = newTrInfo.startPos + (newTrInfo.endPos - trInfo.endPos);
                                }
                                if (newTrInfo.startPos < newTrInfo.endPos) {
                                    adjustTimelineTransitions(m_scene->editMode(), tr, moveCommand);
                                    new MoveTransitionCommand(this, trInfo, newTrInfo, true, moveCommand);
                                }
                            }
                        }
                    }
                    updateTrackDuration(info.track, moveCommand);
                    if (m_dragItemInfo.track != info.track)
                        updateTrackDuration(m_dragItemInfo.track, moveCommand);
                    m_commandStack->push(moveCommand);
                    //checkTrackSequence(m_dragItem->track());
                } else {
                    // undo last move and emit error message
                    bool snap = KdenliveSettings::snaptopoints();
                    KdenliveSettings::setSnaptopoints(false);
                    item->setPos((int) m_dragItemInfo.startPos.frames(m_document->fps()), (int)(m_dragItemInfo.track * m_tracksHeight + 1));
                    KdenliveSettings::setSnaptopoints(snap);
                    emit displayMessage(i18n("Cannot move clip to position %1", m_document->timecode().getTimecodeFromFrames(info.startPos.frames(m_document->fps()))), ErrorMessage);
                }
                setDocumentModified();
            } else if (m_dragItem->type() == TRANSITIONWIDGET && (m_dragItemInfo.startPos != info.startPos || m_dragItemInfo.track != info.track)) {
                Transition *transition = static_cast <Transition *>(m_dragItem);
                transition->updateTransitionEndTrack(getPreviousVideoTrack(m_dragItem->track()));
                if (!m_document->renderer()->mltMoveTransition(transition->transitionTag(), (int)(m_document->tracksCount() - m_dragItemInfo.track), (int)(m_document->tracksCount() - m_dragItem->track()), transition->transitionEndTrack(), m_dragItemInfo.startPos, m_dragItemInfo.endPos, info.startPos, info.endPos)) {
                    // Moving transition failed, revert to previous position
                    emit displayMessage(i18n("Cannot move transition"), ErrorMessage);
                    transition->setPos((int) m_dragItemInfo.startPos.frames(m_document->fps()), (m_dragItemInfo.track) * m_tracksHeight + 1);
                } else {
                    QUndoCommand *moveCommand = new QUndoCommand();
                    moveCommand->setText(i18n("Move transition"));
                    adjustTimelineTransitions(m_scene->editMode(), transition, moveCommand);
                    new MoveTransitionCommand(this, m_dragItemInfo, info, false, moveCommand);
                    updateTrackDuration(info.track, moveCommand);
                    if (m_dragItemInfo.track != info.track)
                        updateTrackDuration(m_dragItemInfo.track, moveCommand);
                    m_commandStack->push(moveCommand);
                    setDocumentModified();
                }
            }
        } else {
            // Moving several clips. We need to delete them and readd them to new position,
            // or they might overlap each other during the move
            QGraphicsItemGroup *group;
            if (m_selectionGroup)
                group = static_cast <QGraphicsItemGroup *>(m_selectionGroup);
            else
                group = static_cast <QGraphicsItemGroup *>(m_dragItem->parentItem());
            QList<QGraphicsItem *> items = group->childItems();

            QList<ItemInfo> clipsToMove;
            QList<ItemInfo> transitionsToMove;

            GenTime timeOffset = GenTime(m_dragItem->scenePos().x(), m_document->fps()) - m_dragItemInfo.startPos;
            const int trackOffset = (int)(m_dragItem->scenePos().y() / m_tracksHeight) - m_dragItemInfo.track;

            QUndoCommand *moveGroup = new QUndoCommand();
            moveGroup->setText(i18n("Move group"));
            if (timeOffset != GenTime() || trackOffset != 0) {
                // remove items in MLT playlist

                // Expand groups
                int max = items.count();
                for (int i = 0; i < max; i++) {
                    if (items.at(i)->type() == GROUPWIDGET) {
                        items += items.at(i)->childItems();
                    }
                }
                m_document->renderer()->blockSignals(true);
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
                m_document->renderer()->blockSignals(false);
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
                        int trackProducer = info.track;
                        info.track = m_document->tracksCount() - info.track;
                        adjustTimelineClips(m_scene->editMode(), clip, ItemInfo(), moveGroup);
                        m_document->renderer()->mltInsertClip(info, clip->xml(), clip->getProducer(trackProducer), m_scene->editMode() == OVERWRITEEDIT, m_scene->editMode() == INSERTEDIT);
                        for (int i = 0; i < clip->effectsCount(); i++) {
                            m_document->renderer()->mltAddEffect(info.track, info.startPos, getEffectArgs(clip->effect(i)), false);
                        }
                    } else {
                        Transition *tr = static_cast <Transition*>(item);
                        int newTrack = tr->transitionEndTrack();
                        if (!tr->forcedTrack()) {
                            newTrack = getPreviousVideoTrack(info.track);
                        }
                        tr->updateTransitionEndTrack(newTrack);
                        adjustTimelineTransitions(m_scene->editMode(), tr, moveGroup);
                        m_document->renderer()->mltAddTransition(tr->transitionTag(), newTrack, m_document->tracksCount() - info.track, info.startPos, info.endPos, tr->toXML());
                    }
                }
                new MoveGroupCommand(this, clipsToMove, transitionsToMove, timeOffset, trackOffset, false, moveGroup);
                updateTrackDuration(-1, moveGroup);
                m_commandStack->push(moveGroup);

                //QPointF top = group->sceneBoundingRect().topLeft();
                //QPointF oldpos = m_selectionGroup->scenePos();
                //kDebug()<<"SELECTION GRP POS: "<<m_selectionGroup->scenePos()<<", TOP: "<<top;
                //group->setPos(top);
                //TODO: get rid of the 3 lines below
                if (m_selectionGroup) {
                    m_selectionGroupInfo.startPos = GenTime(m_selectionGroup->scenePos().x(), m_document->fps());
                    m_selectionGroupInfo.track = m_selectionGroup->track();

                    for (int i = 0; i < items.count(); ++i) {
                        if (items.at(i)->type() == GROUPWIDGET) {
                            rebuildGroup((AbstractGroupItem*)items.at(i));
                            items.removeAt(i);
                            --i;
                        }
                    }
                    for (int i = 0; i < items.count(); ++i) {
                        if (items.at(i)) {
                            items.at(i)->setSelected(true);
                            if (items.at(i)->parentItem())
                                items.at(i)->parentItem()->setSelected(true);
                        }
                    }
                    resetSelectionGroup();
                    groupSelectedItems();
                } else {
                    rebuildGroup((AbstractGroupItem *)group);
                }
                setDocumentModified();
            }
        }
        m_document->renderer()->doRefresh();
    } else if (m_operationMode == RESIZESTART && m_dragItem->startPos() != m_dragItemInfo.startPos) {
        // resize start
        if (!m_controlModifier && m_dragItem->type() == AVWIDGET && m_dragItem->parentItem() && m_dragItem->parentItem() != m_selectionGroup) {
            AbstractGroupItem *parent = static_cast <AbstractGroupItem *>(m_dragItem->parentItem());
            if (parent) {
                QUndoCommand *resizeCommand = new QUndoCommand();
                resizeCommand->setText(i18n("Resize group"));
                QList <QGraphicsItem *> items = parent->childItems();
                QList <ItemInfo> infos = parent->resizeInfos();
                parent->clearResizeInfos();
                int itemcount = 0;
                for (int i = 0; i < items.count(); ++i) {
                    AbstractClipItem *item = static_cast<AbstractClipItem *>(items.at(i));
                    if (item && item->type() == AVWIDGET) {
                        ItemInfo info = infos.at(itemcount);
                        prepareResizeClipStart(item, info, item->startPos().frames(m_document->fps()), false, resizeCommand);
                        ++itemcount;
                    }
                }
                m_commandStack->push(resizeCommand);
            }
        } else {
            prepareResizeClipStart(m_dragItem, m_dragItemInfo, m_dragItem->startPos().frames(m_document->fps()));
        }
    } else if (m_operationMode == RESIZEEND && m_dragItem->endPos() != m_dragItemInfo.endPos) {
        // resize end
        if (!m_controlModifier && m_dragItem->type() == AVWIDGET && m_dragItem->parentItem() && m_dragItem->parentItem() != m_selectionGroup) {
            AbstractGroupItem *parent = static_cast <AbstractGroupItem *>(m_dragItem->parentItem());
            if (parent) {
                QUndoCommand *resizeCommand = new QUndoCommand();
                resizeCommand->setText(i18n("Resize group"));
                QList <QGraphicsItem *> items = parent->childItems();
                QList <ItemInfo> infos = parent->resizeInfos();
                parent->clearResizeInfos();
                int itemcount = 0;
                for (int i = 0; i < items.count(); ++i) {
                    AbstractClipItem *item = static_cast<AbstractClipItem *>(items.at(i));
                    if (item && item->type() == AVWIDGET) {
                        ItemInfo info = infos.at(itemcount);
                        prepareResizeClipEnd(item, info, item->endPos().frames(m_document->fps()), false, resizeCommand);
                        ++itemcount;
                    }
                }
                updateTrackDuration(-1, resizeCommand);
                m_commandStack->push(resizeCommand);
            }
        } else {
            prepareResizeClipEnd(m_dragItem, m_dragItemInfo, m_dragItem->endPos().frames(m_document->fps()));
        }
    } else if (m_operationMode == FADEIN) {
        // resize fade in effect
        ClipItem * item = static_cast <ClipItem *>(m_dragItem);
        int ix = item->hasEffect("volume", "fadein");
        int ix2 = item->hasEffect("", "fade_from_black");
        if (ix != -1) {
            QDomElement oldeffect = item->effectAtIndex(ix);
            int start = item->cropStart().frames(m_document->fps());
            int end = item->fadeIn();
            if (end == 0) {
                slotDeleteEffect(item, -1, oldeffect, false);
            } else {
                end += start;
                QDomElement effect = oldeffect.cloneNode().toElement();
                EffectsList::setParameter(oldeffect, "in", QString::number(start));
                EffectsList::setParameter(oldeffect, "out", QString::number(end));
                slotUpdateClipEffect(item, -1, effect, oldeffect, ix);
                emit clipItemSelected(item);
            }
        } else if (item->fadeIn() != 0 && ix2 == -1) {
            QDomElement effect;
            if (item->isVideoOnly() || (item->clipType() != AUDIO && item->clipType() != AV && item->clipType() != PLAYLIST)) {
                // add video fade
                effect = MainWindow::videoEffects.getEffectByTag("", "fade_from_black").cloneNode().toElement();
            } else effect = MainWindow::audioEffects.getEffectByTag("volume", "fadein").cloneNode().toElement();
            EffectsList::setParameter(effect, "out", QString::number(item->fadeIn()));
            slotAddEffect(effect, m_dragItem->startPos(), m_dragItem->track());
        }
        if (ix2 != -1) {
            QDomElement oldeffect = item->effectAtIndex(ix2);
            int start = item->cropStart().frames(m_document->fps());
            int end = item->fadeIn();
            if (end == 0) {
                slotDeleteEffect(item, -1, oldeffect, false);
            } else {
                end += start;
                QDomElement effect = oldeffect.cloneNode().toElement();
                EffectsList::setParameter(oldeffect, "in", QString::number(start));
                EffectsList::setParameter(oldeffect, "out", QString::number(end));
                slotUpdateClipEffect(item, -1, effect, oldeffect, ix2);
                emit clipItemSelected(item);
            }
        }
    } else if (m_operationMode == FADEOUT) {
        // resize fade out effect
        ClipItem * item = static_cast <ClipItem *>(m_dragItem);
        int ix = item->hasEffect("volume", "fadeout");
        int ix2 = item->hasEffect("", "fade_to_black");
        if (ix != -1) {
            QDomElement oldeffect = item->effectAtIndex(ix);
            int end = (item->cropDuration() + item->cropStart()).frames(m_document->fps());
            int start = item->fadeOut();
            if (start == 0) {
                slotDeleteEffect(item, -1, oldeffect, false);
            } else {
                start = end - start;
                QDomElement effect = oldeffect.cloneNode().toElement();
                EffectsList::setParameter(oldeffect, "in", QString::number(start));
                EffectsList::setParameter(oldeffect, "out", QString::number(end));
                // kDebug()<<"EDIT FADE OUT : "<<start<<"x"<<end;
                slotUpdateClipEffect(item, -1, effect, oldeffect, ix);
                emit clipItemSelected(item);
            }
        } else if (item->fadeOut() != 0 && ix2 == -1) {
            QDomElement effect;
            if (item->isVideoOnly() || (item->clipType() != AUDIO && item->clipType() != AV && item->clipType() != PLAYLIST)) {
                // add video fade
                effect = MainWindow::videoEffects.getEffectByTag("", "fade_to_black").cloneNode().toElement();
            } else effect = MainWindow::audioEffects.getEffectByTag("volume", "fadeout").cloneNode().toElement();
            int end = (item->cropDuration() + item->cropStart()).frames(m_document->fps());
            int start = end-item->fadeOut();
            EffectsList::setParameter(effect, "in", QString::number(start));
            EffectsList::setParameter(effect, "out", QString::number(end));
            slotAddEffect(effect, m_dragItem->startPos(), m_dragItem->track());
        }
        if (ix2 != -1) {
            QDomElement oldeffect = item->effectAtIndex(ix2);
            int end = (item->cropDuration() + item->cropStart()).frames(m_document->fps());
            int start = item->fadeOut();
            if (start == 0) {
                slotDeleteEffect(item, -1, oldeffect, false);
            } else {
                start = end - start;
                QDomElement effect = oldeffect.cloneNode().toElement();
                EffectsList::setParameter(oldeffect, "in", QString::number(start));
                EffectsList::setParameter(oldeffect, "out", QString::number(end));
                // kDebug()<<"EDIT FADE OUT : "<<start<<"x"<<end;
                slotUpdateClipEffect(item, -1, effect, oldeffect, ix2);
                emit clipItemSelected(item);
            }
        }
    } else if (m_operationMode == KEYFRAME) {
        // update the MLT effect
        ClipItem * item = static_cast <ClipItem *>(m_dragItem);
        QDomElement oldEffect = item->selectedEffect().cloneNode().toElement();

        // check if we want to remove keyframe
        double val = mapToScene(event->pos()).toPoint().y();
        QRectF br = item->sceneBoundingRect();
        double maxh = 100.0 / br.height();
        val = (br.bottom() - val) * maxh;
        int start = item->cropStart().frames(m_document->fps());
        int end = (item->cropStart() + item->cropDuration()).frames(m_document->fps()) - 1;

        if ((val < -50 || val > 150) && item->editedKeyFramePos() != start && item->editedKeyFramePos() != end && item->keyFrameNumber() > 1) {
            //delete keyframe
            item->movedKeyframe(item->getEffectAtIndex(item->selectedEffectIndex()), item->selectedKeyFramePos(), -1, 0);
        } else {
            item->movedKeyframe(item->getEffectAtIndex(item->selectedEffectIndex()), item->selectedKeyFramePos(), item->editedKeyFramePos(), item->editedKeyFrameValue());
        }

        QDomElement newEffect = item->selectedEffect().cloneNode().toElement();
        //item->updateKeyframeEffect();
        //QString next = item->keyframes(item->selectedEffectIndex());
        //EditKeyFrameCommand *command = new EditKeyFrameCommand(this, item->track(), item->startPos(), item->selectedEffectIndex(), previous, next, false);
        EditEffectCommand *command = new EditEffectCommand(this, m_document->tracksCount() - item->track(), item->startPos(), oldEffect, newEffect, item->selectedEffectIndex(), false, false);

        m_commandStack->push(command);
        updateEffect(m_document->tracksCount() - item->track(), item->startPos(), item->selectedEffect());
        emit clipItemSelected(item);
    }
    if (m_dragItem && m_dragItem->type() == TRANSITIONWIDGET && m_dragItem->isSelected()) {
        // A transition is selected
        QPoint p;
        ClipItem *transitionClip = getClipItemAt(m_dragItemInfo.startPos, m_dragItemInfo.track);
        if (transitionClip && transitionClip->baseClip()) {
            QString size = transitionClip->baseClip()->getProperty("frame_size");
            double factor = transitionClip->baseClip()->getProperty("aspect_ratio").toDouble();
            if (factor == 0) factor = 1.0;
            p.setX((int)(size.section('x', 0, 0).toInt() * factor + 0.5));
            p.setY(size.section('x', 1, 1).toInt());
        }
        emit transitionItemSelected(static_cast <Transition *>(m_dragItem), getPreviousVideoTrack(m_dragItem->track()), p);
    } else emit transitionItemSelected(NULL);
    if (m_operationMode != NONE && m_operationMode != MOVE) setDocumentModified();
    m_operationMode = NONE;
}

void CustomTrackView::deleteClip(ItemInfo info, bool refresh)
{
    ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()), info.track);
    m_ct++;
    if (!item) kDebug()<<"// PROBLEM FINDING CLIP ITEM TO REMOVVE!!!!!!!!!";
    else kDebug()<<"// deleting CLIP: "<<info.startPos.frames(m_document->fps())<<", "<<item->baseClip()->fileURL();
    //m_document->renderer()->saveSceneList(QString("/tmp/error%1.mlt").arg(m_ct), QDomElement());
    if (!item || m_document->renderer()->mltRemoveClip(m_document->tracksCount() - info.track, info.startPos) == false) {
        emit displayMessage(i18n("Error removing clip at %1 on track %2", m_document->timecode().getTimecodeFromFrames(info.startPos.frames(m_document->fps())), info.track), ErrorMessage);
        kDebug()<<"CANNOT REMOVE: "<<info.startPos.frames(m_document->fps())<<", TK: "<<info.track;
        //m_document->renderer()->saveSceneList(QString("/tmp/error%1.mlt").arg(m_ct), QDomElement());
        return;
    }
    m_waitingThumbs.removeAll(item);
    item->stopThumbs();
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

    if (m_dragItem == item) m_dragItem = NULL;

#if QT_VERSION >= 0x040600
    // animate item deletion
    item->closeAnimation();
    /*if (refresh) item->closeAnimation();
    else {
        // no refresh, means we have several operations chained, we need to delete clip immediatly
        // so that it does not get in the way of the other
        delete item;
        item = NULL;
    }*/
#else
    delete item;
    item = NULL;
#endif

    setDocumentModified();
    if (refresh) m_document->renderer()->doRefresh();
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

    int groupCount = 0;
    int clipCount = 0;
    int transitionCount = 0;
    // expand & destroy groups
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == GROUPWIDGET) {
            groupCount++;
            QList<QGraphicsItem *> children = itemList.at(i)->childItems();
            QList <ItemInfo> clipInfos;
            QList <ItemInfo> transitionInfos;
            for (int j = 0; j < children.count(); j++) {
                if (children.at(j)->type() == AVWIDGET) {
                    AbstractClipItem *clip = static_cast <AbstractClipItem *>(children.at(j));
                    if (!clip->isItemLocked()) clipInfos.append(clip->info());
                } else if (children.at(j)->type() == TRANSITIONWIDGET) {
                    AbstractClipItem *clip = static_cast <AbstractClipItem *>(children.at(j));
                    if (!clip->isItemLocked()) transitionInfos.append(clip->info());
                }
                if (itemList.contains(children.at(j))) {
                    children.removeAt(j);
                    j--;
                }
            }
            itemList += children;
            if (clipInfos.count() > 0)
                new GroupClipsCommand(this, clipInfos, transitionInfos, false, deleteSelected);

        } else if (itemList.at(i)->parentItem() && itemList.at(i)->parentItem()->type() == GROUPWIDGET)
            itemList.insert(i + 1, itemList.at(i)->parentItem());
    }

    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            clipCount++;
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            //kDebug()<<"// DELETE CLP AT: "<<item->info().startPos.frames(25);
            new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), item->info(), item->effectList(), false, false, true, true, deleteSelected);
            emit clipItemSelected(NULL);
        } else if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            transitionCount++;
            Transition *item = static_cast <Transition *>(itemList.at(i));
            //kDebug()<<"// DELETE TRANS AT: "<<item->info().startPos.frames(25);
            new AddTransitionCommand(this, item->info(), item->transitionEndTrack(), item->toXML(), true, true, deleteSelected);
            emit transitionItemSelected(NULL);
        }
    }
    if (groupCount > 0 && clipCount == 0 && transitionCount == 0)
        deleteSelected->setText(i18np("Delete selected group", "Delete selected groups", groupCount));
    else if (clipCount > 0 && groupCount == 0 && transitionCount == 0)
        deleteSelected->setText(i18np("Delete selected clip", "Delete selected clips", clipCount));
    else if (transitionCount > 0 && groupCount == 0 && clipCount == 0)
        deleteSelected->setText(i18np("Delete selected transition", "Delete selected transitions", transitionCount));
    else deleteSelected->setText(i18n("Delete selected items"));
    updateTrackDuration(-1, deleteSelected);
    m_commandStack->push(deleteSelected);
}


void CustomTrackView::doChangeClipSpeed(ItemInfo info, ItemInfo speedIndependantInfo, const double speed, const double oldspeed, int strobe, const QString &id)
{
    Q_UNUSED(id)
    //DocClipBase *baseclip = m_document->clipManager()->getClipById(id);

    ClipItem *item = getClipItemAt((int) info.startPos.frames(m_document->fps()), info.track);
    if (!item) {
        kDebug() << "ERROR: Cannot find clip for speed change";
        emit displayMessage(i18n("Cannot find clip for speed change"), ErrorMessage);
        return;
    }
    info.track = m_document->tracksCount() - item->track();
    int endPos = m_document->renderer()->mltChangeClipSpeed(info, speedIndependantInfo, speed, oldspeed, strobe, item->getProducer(item->track(), false));
    if (endPos >= 0) {
        item->setSpeed(speed, strobe);
        item->updateRectGeometry();
        if (item->cropDuration().frames(m_document->fps()) != endPos)
            item->resizeEnd((int) info.startPos.frames(m_document->fps()) + endPos - 1);
        updatePositionEffects(item, info);
        setDocumentModified();
    } else {
        emit displayMessage(i18n("Invalid clip"), ErrorMessage);
    }
}

void CustomTrackView::cutSelectedClips()
{
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    QList<AbstractGroupItem *> groups;
    GenTime currentPos = GenTime(m_cursorPos, m_document->fps());
    for (int i = 0; i < itemList.count(); ++i) {
        if (!itemList.at(i))
            continue;
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            if (item->parentItem() && item->parentItem() != m_selectionGroup) {
                AbstractGroupItem *group = static_cast <AbstractGroupItem *>(item->parentItem());
                if (!groups.contains(group))
                    groups << group;
            } else if (currentPos > item->startPos() && currentPos < item->endPos()) {
                RazorClipCommand *command = new RazorClipCommand(this, item->info(), currentPos);
                m_commandStack->push(command);
            }
        } else if (itemList.at(i)->type() == GROUPWIDGET && itemList.at(i) != m_selectionGroup) {
            AbstractGroupItem *group = static_cast<AbstractGroupItem *>(itemList.at(i));
            if (!groups.contains(group))
                groups << group;
        }
    }

    for (int i = 0; i < groups.count(); ++i)
        razorGroup(groups.at(i), currentPos);
}

void CustomTrackView::razorGroup(AbstractGroupItem* group, GenTime cutPos)
{
    if (group) {
        QList <QGraphicsItem *> children = group->childItems();
        QList <ItemInfo> clips1, transitions1;
        QList <ItemInfo> clipsCut, transitionsCut;
        QList <ItemInfo> clips2, transitions2;
        for (int i = 0; i < children.count(); ++i) {
            children.at(i)->setSelected(false);
            AbstractClipItem *child = static_cast <AbstractClipItem *>(children.at(i));
            if (child->type() == AVWIDGET) {
                if (cutPos > child->endPos())
                    clips1 << child->info();
                else if (cutPos < child->startPos())
                    clips2 << child->info();
                else
                    clipsCut << child->info();
            } else {
                if (cutPos > child->endPos())
                    transitions1 << child->info();
                else if (cutPos < child->startPos())
                    transitions2 << child->info();
                else
                    transitionsCut << child->info();
            }
        }
        if (clipsCut.isEmpty() && transitionsCut.isEmpty() && ((clips1.isEmpty() && transitions1.isEmpty()) || (clips2.isEmpty() && transitions2.isEmpty())))
            return;
        RazorGroupCommand *command = new RazorGroupCommand(this, clips1, transitions1, clipsCut, transitionsCut, clips2, transitions2, cutPos);
        m_commandStack->push(command);
    }
}

void CustomTrackView::slotRazorGroup(QList <ItemInfo> clips1, QList <ItemInfo> transitions1, QList <ItemInfo> clipsCut, QList <ItemInfo> transitionsCut, QList <ItemInfo> clips2, QList <ItemInfo> transitions2, GenTime cutPos, bool cut)
{
    if (cut) {
        for (int i = 0; i < clipsCut.count(); ++i) {
            ClipItem *clip = getClipItemAt(clipsCut.at(i).startPos.frames(m_document->fps()), clipsCut.at(i).track);
            if (clip) {
                ClipItem *clipBehind = cutClip(clipsCut.at(i), cutPos, true);
                clips1 << clip->info();
                if (clipBehind != NULL)
                    clips2 << clipBehind->info();
            }
        }
        /* TODO: cut transitionsCut
         * For now just append them to group1 */
        transitions1 << transitionsCut;
        doGroupClips(clips1, transitions1, true);
        doGroupClips(clips2, transitions2, true);
    } else {
        /* we might also just use clipsCut.at(0)->parentItem().
         * Do this loop just in case something went wrong during cut */
        for (int i = 0; i < clipsCut.count(); ++i) {
            ClipItem *clip = getClipItemAt(cutPos.frames(m_document->fps()), clipsCut.at(i).track);
            if (clip && clip->parentItem() && clip->parentItem()->type() == GROUPWIDGET) {
                AbstractGroupItem *group = static_cast <AbstractGroupItem *>(clip->parentItem());
                QList <QGraphicsItem *> children = group->childItems();
                QList <ItemInfo> groupClips;
                QList <ItemInfo> groupTrans;
                for (int j = 0; j < children.count(); ++j) {
                    if (children.at(j)->type() == AVWIDGET)
                        groupClips << ((AbstractClipItem *)children.at(j))->info();
                    else if (children.at(j)->type() == TRANSITIONWIDGET)
                        groupTrans << ((AbstractClipItem *)children.at(j))->info();
                }
                doGroupClips(groupClips, groupTrans, false);
                break;
            }
        }
        for (int i = 0; i < clipsCut.count(); ++i)
            cutClip(clipsCut.at(i), cutPos, false);
        // TODO: uncut transitonsCut
        doGroupClips(QList <ItemInfo>() << clips1 << clipsCut << clips2, QList <ItemInfo>() << transitions1 << transitionsCut << transitions2, true);
    }
}

void CustomTrackView::groupClips(bool group)
{
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    QList <ItemInfo> clipInfos;
    QList <ItemInfo> transitionInfos;

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
            clip->setFlag(QGraphicsItem::ItemIsMovable, true);
        }
        for (int i = 0; i < transitionInfos.count(); i++) {
            Transition *tr = getTransitionItemAt(transitionInfos.at(i).startPos, transitionInfos.at(i).track);
            if (tr == NULL) continue;
            if (tr->parentItem() && tr->parentItem()->type() == GROUPWIDGET) {
                AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(tr->parentItem());
                m_document->clipManager()->removeGroup(grp);
                scene()->destroyItemGroup(grp);
            }
            tr->setFlag(QGraphicsItem::ItemIsMovable, true);
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

void CustomTrackView::addClip(QDomElement xml, const QString &clipId, ItemInfo info, EffectsList effects, bool overwrite, bool push, bool refresh)
{
    DocClipBase *baseclip = m_document->clipManager()->getClipById(clipId);
    if (baseclip == NULL) {
        emit displayMessage(i18n("No clip copied"), ErrorMessage);
        return;
    }

    if (baseclip->getProducer() == NULL) {
        // If the clip has no producer, we must wait until it is created...
        m_mutex.lock();
        emit displayMessage(i18n("Waiting for clip..."), InformationMessage);
        emit forceClipProcessing(clipId);
        qApp->processEvents();
        for (int i = 0; i < 10; i++) {
            if (baseclip->getProducer() == NULL) {
                qApp->processEvents();
                m_producerNotReady.wait(&m_mutex, 200);
            } else break;
        }
        if (baseclip->getProducer() == NULL) {
            emit displayMessage(i18n("Cannot insert clip..."), ErrorMessage);
            m_mutex.unlock();
            return;
        }
        emit displayMessage(QString(), InformationMessage);
        m_mutex.unlock();
    }

    ClipItem *item = new ClipItem(baseclip, info, m_document->fps(), xml.attribute("speed", "1").toDouble(), xml.attribute("strobe", "1").toInt(), getFrameWidth());
    item->setEffectList(effects);
    if (xml.hasAttribute("audio_only")) item->setAudioOnly(true);
    else if (xml.hasAttribute("video_only")) item->setVideoOnly(true);
    scene()->addItem(item);

    int producerTrack = info.track;
    int tracknumber = m_document->tracksCount() - info.track - 1;
    bool isLocked = m_document->trackInfoAt(tracknumber).isLocked;
    if (isLocked) item->setItemLocked(true);

    baseclip->addReference();
    m_document->updateClip(baseclip->getId());
    info.track = m_document->tracksCount() - info.track;
    m_document->renderer()->mltInsertClip(info, xml, item->getProducer(producerTrack), overwrite, push);
    for (int i = 0; i < item->effectsCount(); i++) {
        m_document->renderer()->mltAddEffect(info.track, info.startPos, getEffectArgs(item->effect(i)), false);
    }
    setDocumentModified();
    if (refresh)
        m_document->renderer()->doRefresh();
    if (!baseclip->isPlaceHolder())
        m_waitingThumbs.append(item);
    m_thumbsTimer.start();
}

void CustomTrackView::slotUpdateClip(const QString &clipId, bool reload)
{
    QMutexLocker locker(&m_mutex);
    QList<QGraphicsItem *> list = scene()->items();
    QList <ClipItem *>clipList;
    ClipItem *clip = NULL;
    DocClipBase *baseClip = NULL;
    Mlt::Tractor *tractor = m_document->renderer()->lockService();
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->type() == AVWIDGET) {
            clip = static_cast <ClipItem *>(list.at(i));
            if (clip->clipProducer() == clipId) {
                if (baseClip == NULL) {
                    baseClip = clip->baseClip();
                }
                ItemInfo info = clip->info();
                Mlt::Producer *prod = NULL;
                if (clip->isAudioOnly()) prod = baseClip->audioProducer(info.track);
                else if (clip->isVideoOnly()) prod = baseClip->videoProducer();
                else prod = baseClip->getProducer(info.track);
                if (reload && !m_document->renderer()->mltUpdateClip(tractor, info, clip->xml(), prod)) {
                    emit displayMessage(i18n("Cannot update clip (time: %1, track: %2)", info.startPos.frames(m_document->fps()), info.track), ErrorMessage);
                }
                else clipList.append(clip);
            }
        }
    }
    for (int i = 0; i < clipList.count(); i++)
        clipList.at(i)->refreshClip(true, true);
    m_document->renderer()->unlockService(tractor);
}

ClipItem *CustomTrackView::getClipItemAtEnd(GenTime pos, int track)
{
    int framepos = (int)(pos.frames(m_document->fps()));
    QList<QGraphicsItem *> list = scene()->items(QPointF(framepos - 1, track * m_tracksHeight + m_tracksHeight / 2));
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); i++) {
        if (!list.at(i)->isEnabled()) continue;
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
        if (!list.at(i)->isEnabled()) continue;
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
    const QPointF p(pos, track * m_tracksHeight + m_tracksHeight / 2);
    QList<QGraphicsItem *> list = scene()->items(p);
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); i++) {
        if (!list.at(i)->isEnabled()) continue;
        if (list.at(i)->type() == AVWIDGET) {
            clip = static_cast <ClipItem *>(list.at(i));
            break;
        }
    }
    return clip;
}

ClipItem *CustomTrackView::getClipItemAt(GenTime pos, int track)
{
    return getClipItemAt((int) pos.frames(m_document->fps()), track);
}


Transition *CustomTrackView::getTransitionItem(TransitionInfo info)
{
    int pos = info.startPos.frames(m_document->fps());
    int track = m_document->tracksCount() - info.b_track;
    return getTransitionItemAt(pos, track);
}

Transition *CustomTrackView::getTransitionItemAt(int pos, int track)
{
    const QPointF p(pos, track * m_tracksHeight + Transition::itemOffset() + 1);
    QList<QGraphicsItem *> list = scene()->items(p);
    Transition *clip = NULL;
    for (int i = 0; i < list.size(); i++) {
        if (!list.at(i)->isEnabled()) continue;
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
    const QPointF p(framepos - 1, track * m_tracksHeight + Transition::itemOffset() + 1);
    QList<QGraphicsItem *> list = scene()->items(p);
    Transition *clip = NULL;
    for (int i = 0; i < list.size(); i++) {
        if (!list.at(i)->isEnabled()) continue;
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
    const QPointF p(pos.frames(m_document->fps()), track * m_tracksHeight + Transition::itemOffset() + 1);
    QList<QGraphicsItem *> list = scene()->items(p);
    Transition *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (!list.at(i)->isEnabled()) continue;
        if (list.at(i)->type() == TRANSITIONWIDGET) {
            Transition *test = static_cast <Transition *>(list.at(i));
            if (test->startPos() == pos) clip = test;
            break;
        }
    }
    return clip;
}

bool CustomTrackView::moveClip(const ItemInfo &start, const ItemInfo &end, bool refresh, ItemInfo *out_actualEnd)
{
    if (m_selectionGroup) resetSelectionGroup(false);
    ClipItem *item = getClipItemAt((int) start.startPos.frames(m_document->fps()), start.track);
    if (!item) {
        emit displayMessage(i18n("Cannot move clip at time: %1 on track %2", m_document->timecode().getTimecodeFromFrames(start.startPos.frames(m_document->fps())), start.track), ErrorMessage);
        kDebug() << "----------------  ERROR, CANNOT find clip to move at.. ";
        return false;
    }
    Mlt::Producer *prod = item->getProducer(end.track);

#ifdef DEBUG
    qDebug() << "Moving item " << (long)item << " from .. to:";
    qDebug() << item->info();
    qDebug() << start;
    qDebug() << end;
#endif
    bool success = m_document->renderer()->mltMoveClip((int)(m_document->tracksCount() - start.track), (int)(m_document->tracksCount() - end.track),
                                                       (int) start.startPos.frames(m_document->fps()), (int)end.startPos.frames(m_document->fps()),
                                                       prod);
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
    if (refresh) m_document->renderer()->doRefresh();
    if (out_actualEnd != NULL) {
        *out_actualEnd = item->info();
#ifdef DEBUG
        qDebug() << "Actual end position updated:" << *out_actualEnd;
#endif
    }
#ifdef DEBUG
    qDebug() << item->info();
#endif
    return success;
}

void CustomTrackView::moveGroup(QList <ItemInfo> startClip, QList <ItemInfo> startTransition, const GenTime &offset, const int trackOffset, bool reverseMove)
{
    // Group Items
    resetSelectionGroup();
    m_scene->clearSelection();

    m_selectionGroup = new AbstractGroupItem(m_document->fps());
    scene()->addItem(m_selectionGroup);

    m_document->renderer()->blockSignals(true);
    for (int i = 0; i < startClip.count(); i++) {
        if (reverseMove) {
            startClip[i].startPos = startClip.at(i).startPos - offset;
            startClip[i].track = startClip.at(i).track - trackOffset;
        }
        ClipItem *clip = getClipItemAt(startClip.at(i).startPos, startClip.at(i).track);
        if (clip) {
            clip->setItemLocked(false);
            if (clip->parentItem()) {
                m_selectionGroup->addToGroup(clip->parentItem());
                clip->parentItem()->setFlag(QGraphicsItem::ItemIsMovable, false);
            } else {
                m_selectionGroup->addToGroup(clip);
                clip->setFlag(QGraphicsItem::ItemIsMovable, false);
            }
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
            if (tr->parentItem()) {
                m_selectionGroup->addToGroup(tr->parentItem());
                tr->parentItem()->setFlag(QGraphicsItem::ItemIsMovable, false);
            } else {
                m_selectionGroup->addToGroup(tr);
                tr->setFlag(QGraphicsItem::ItemIsMovable, false);
            }
            m_document->renderer()->mltDeleteTransition(tr->transitionTag(), tr->transitionEndTrack(), m_document->tracksCount() - startTransition.at(i).track, startTransition.at(i).startPos, startTransition.at(i).endPos, tr->toXML());
        } else kDebug() << "//MISSING TRANSITION AT: " << startTransition.at(i).startPos.frames(25);
    }
    m_document->renderer()->blockSignals(false);

    if (m_selectionGroup) {
        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);

        m_selectionGroup->translate(offset.frames(m_document->fps()), trackOffset *(qreal) m_tracksHeight);
        //m_selectionGroup->moveBy(offset.frames(m_document->fps()), trackOffset *(qreal) m_tracksHeight);

        QList<QGraphicsItem *> children = m_selectionGroup->childItems();
        // Expand groups
        int max = children.count();
        for (int i = 0; i < max; i++) {
            if (children.at(i)->type() == GROUPWIDGET) {
                children += children.at(i)->childItems();
                //AbstractGroupItem *grp = static_cast<AbstractGroupItem *>(children.at(i));
                //grp->moveBy(offset.frames(m_document->fps()), trackOffset *(qreal) m_tracksHeight);
                /*m_document->clipManager()->removeGroup(grp);
                m_scene->destroyItemGroup(grp);*/
                children.removeAll(children.at(i));
                i--;
            }
        }

        for (int i = 0; i < children.count(); i++) {
            // re-add items in correct place
            if (children.at(i)->type() != AVWIDGET && children.at(i)->type() != TRANSITIONWIDGET) continue;
            AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
            item->updateItem();
            ItemInfo info = item->info();
            int tracknumber = m_document->tracksCount() - info.track - 1;
            bool isLocked = m_document->trackInfoAt(tracknumber).isLocked;
            if (isLocked)
                item->setItemLocked(true);
            else if (item->isItemLocked())
                item->setItemLocked(false);

            if (item->type() == AVWIDGET) {
                ClipItem *clip = static_cast <ClipItem*>(item);
                int trackProducer = info.track;
                info.track = m_document->tracksCount() - info.track;
                m_document->renderer()->mltInsertClip(info, clip->xml(), clip->getProducer(trackProducer));
                for (int i = 0; i < clip->effectsCount(); i++) {
                    m_document->renderer()->mltAddEffect(info.track, info.startPos, getEffectArgs(clip->effect(i)), false);
                }
            } else if (item->type() == TRANSITIONWIDGET) {
                Transition *tr = static_cast <Transition*>(item);
                int newTrack;
                if (!tr->forcedTrack()) {
                    newTrack = getPreviousVideoTrack(info.track);
                } else {
                    newTrack = tr->transitionEndTrack() + trackOffset;
                    if (newTrack < 0 || newTrack > m_document->tracksCount()) newTrack = getPreviousVideoTrack(info.track);
                }
                tr->updateTransitionEndTrack(newTrack);
                m_document->renderer()->mltAddTransition(tr->transitionTag(), newTrack, m_document->tracksCount() - info.track, info.startPos, info.endPos, tr->toXML());
            }
        }

        resetSelectionGroup(false);

        for (int i = 0; i < children.count(); i++) {
            if (children.at(i)->parentItem())
                rebuildGroup((AbstractGroupItem*)children.at(i)->parentItem());
        }
        clearSelection();

        KdenliveSettings::setSnaptopoints(snap);
        m_document->renderer()->doRefresh();
    } else kDebug() << "///////// WARNING; NO GROUP TO MOVE";
}

void CustomTrackView::moveTransition(const ItemInfo &start, const ItemInfo &end, bool refresh)
{
    Transition *item = getTransitionItemAt(start.startPos, start.track);
    if (!item) {
        emit displayMessage(i18n("Cannot move transition at time: %1 on track %2", m_document->timecode().getTimecodeFromFrames(start.startPos.frames(m_document->fps())), start.track), ErrorMessage);
        kDebug() << "----------------  ERROR, CANNOT find transition to move... ";// << startPos.x() * m_scale * FRAME_SIZE + 1 << ", " << startPos.y() * m_tracksHeight + m_tracksHeight / 2;
        return;
    }

    bool snap = KdenliveSettings::snaptopoints();
    KdenliveSettings::setSnaptopoints(false);

    if (end.endPos - end.startPos == start.endPos - start.startPos) {
        // Transition was moved
        item->setPos((int) end.startPos.frames(m_document->fps()), (end.track) * m_tracksHeight + 1);
    } else if (end.endPos == start.endPos) {
        // Transition start resize
        item->resizeStart((int) end.startPos.frames(m_document->fps()));
    } else if (end.startPos == start.startPos) {
        // Transition end resize;
        kDebug() << "// resize END: " << end.endPos.frames(m_document->fps());
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
    m_document->renderer()->mltMoveTransition(item->transitionTag(), m_document->tracksCount() - start.track, m_document->tracksCount() - item->track(), item->transitionEndTrack(), start.startPos, start.endPos, item->startPos(), item->endPos());
    if (m_dragItem && m_dragItem == item) {
        QPoint p;
        ClipItem *transitionClip = getClipItemAt(item->startPos(), item->track());
        if (transitionClip && transitionClip->baseClip()) {
            QString size = transitionClip->baseClip()->getProperty("frame_size");
            double factor = transitionClip->baseClip()->getProperty("aspect_ratio").toDouble();
            if (factor == 0) factor = 1.0;
            p.setX((int)(size.section('x', 0, 0).toInt() * factor + 0.5));
            p.setY(size.section('x', 1, 1).toInt());
        }
        emit transitionItemSelected(item, getPreviousVideoTrack(item->track()), p);
    }
    if (refresh) m_document->renderer()->doRefresh();
    setDocumentModified();
}

void CustomTrackView::resizeClip(const ItemInfo &start, const ItemInfo &end, bool dontWorry)
{
    bool resizeClipStart = (start.startPos != end.startPos);
    ClipItem *item = getClipItemAtStart(start.startPos, start.track);
    if (!item) {
        if (dontWorry) return;
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

    if (resizeClipStart) {
        ItemInfo clipinfo = item->info();
        clipinfo.track = m_document->tracksCount() - clipinfo.track;
        bool success = m_document->renderer()->mltResizeClipStart(clipinfo, end.startPos - clipinfo.startPos);
        if (success)
            item->resizeStart((int) end.startPos.frames(m_document->fps()));
        else
            emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
    } else {
        ItemInfo clipinfo = item->info();
        clipinfo.track = m_document->tracksCount() - clipinfo.track;
        bool success = m_document->renderer()->mltResizeClipEnd(clipinfo, end.endPos - clipinfo.startPos);
        if (success)
            item->resizeEnd((int) end.endPos.frames(m_document->fps()));
        else
            emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
    }
    if (!resizeClipStart && end.cropStart != start.cropStart) {
        kDebug() << "// RESIZE CROP, DIFF: " << (end.cropStart - start.cropStart).frames(25);
        ItemInfo clipinfo = end;
        clipinfo.track = m_document->tracksCount() - end.track;
        bool success = m_document->renderer()->mltResizeClipCrop(clipinfo, end.cropStart);
        if (success) {
            item->setCropStart(end.cropStart);
            item->resetThumbs(true);
        }
    }
    m_document->renderer()->doRefresh();
    KdenliveSettings::setSnaptopoints(snap);
    setDocumentModified();
}

void CustomTrackView::prepareResizeClipStart(AbstractClipItem* item, ItemInfo oldInfo, int pos, bool check, QUndoCommand *command)
{
    if (pos == oldInfo.startPos.frames(m_document->fps()))
        return;
    bool snap = KdenliveSettings::snaptopoints();
    if (check) {
        KdenliveSettings::setSnaptopoints(false);
        item->resizeStart(pos);
        if (item->startPos().frames(m_document->fps()) != pos) {
            item->resizeStart(oldInfo.startPos.frames(m_document->fps()));
            emit displayMessage(i18n("Not possible to resize"), ErrorMessage);
            KdenliveSettings::setSnaptopoints(snap);
            return;
        }
        KdenliveSettings::setSnaptopoints(snap);
    }

    bool hasParentCommand = false;
    if (command) {
        hasParentCommand = true;
    } else {
        command = new QUndoCommand();
        command->setText(i18n("Resize clip start"));
    }

    // do this here, too, because otherwise undo won't update the group
    if (item->parentItem() && item->parentItem() != m_selectionGroup)
        new RebuildGroupCommand(this, item->info().track, item->endPos() - GenTime(1, m_document->fps()), command);

    ItemInfo info = item->info();
    if (item->type() == AVWIDGET) {
        ItemInfo resizeinfo = oldInfo;
        resizeinfo.track = m_document->tracksCount() - resizeinfo.track;
        bool success = m_document->renderer()->mltResizeClipStart(resizeinfo, item->startPos() - oldInfo.startPos);
        if (success) {
            // Check if there is an automatic transition on that clip (lower track)
            Transition *transition = getTransitionItemAtStart(oldInfo.startPos, oldInfo.track);
            if (transition && transition->isAutomatic()) {
                ItemInfo trInfo = transition->info();
                ItemInfo newTrInfo = trInfo;
                newTrInfo.startPos = item->startPos();
                if (newTrInfo.startPos < newTrInfo.endPos)
                    new MoveTransitionCommand(this, trInfo, newTrInfo, true, command);
            }
            // Check if there is an automatic transition on that clip (upper track)
            transition = getTransitionItemAtStart(oldInfo.startPos, oldInfo.track - 1);
            if (transition && transition->isAutomatic() && (m_document->tracksCount() - transition->transitionEndTrack()) == oldInfo.track) {
                ItemInfo trInfo = transition->info();
                ItemInfo newTrInfo = trInfo;
                newTrInfo.startPos = item->startPos();
                ClipItem * upperClip = getClipItemAt(oldInfo.startPos, oldInfo.track - 1);
                if ((!upperClip || !upperClip->baseClip()->isTransparent()) && newTrInfo.startPos < newTrInfo.endPos)
                    new MoveTransitionCommand(this, trInfo, newTrInfo, true, command);
            }

            ClipItem *clip = static_cast < ClipItem * >(item);

            // Hack:
            // Since we must always resize clip before updating the keyframes, we
            // put a resize command before & after checking keyframes so that
            // we are sure the resize is performed before whenever we do or undo the action
            new ResizeClipCommand(this, oldInfo, info, false, true, command);
            adjustEffects(clip, oldInfo, command);
            new ResizeClipCommand(this, oldInfo, info, false, true, command);
        } else {
            KdenliveSettings::setSnaptopoints(false);
            item->resizeStart((int) oldInfo.startPos.frames(m_document->fps()));
            KdenliveSettings::setSnaptopoints(snap);
            emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
        }
    } else if (item->type() == TRANSITIONWIDGET) {
        Transition *transition = static_cast <Transition *>(item);
        if (!m_document->renderer()->mltMoveTransition(transition->transitionTag(), (int)(m_document->tracksCount() - oldInfo.track), (int)(m_document->tracksCount() - oldInfo.track), transition->transitionEndTrack(), oldInfo.startPos, oldInfo.endPos, info.startPos, info.endPos)) {
            // Cannot resize transition
            KdenliveSettings::setSnaptopoints(false);
            transition->resizeStart((int) oldInfo.startPos.frames(m_document->fps()));
            KdenliveSettings::setSnaptopoints(snap);
            emit displayMessage(i18n("Cannot resize transition"), ErrorMessage);
        } else {
            MoveTransitionCommand *moveCommand = new MoveTransitionCommand(this, oldInfo, info, false, command);
            if (command == NULL)
                m_commandStack->push(moveCommand);
        }

    }
    if (item->parentItem() && item->parentItem() != m_selectionGroup)
        new RebuildGroupCommand(this, item->info().track, item->endPos() - GenTime(1, m_document->fps()), command);

    if (!hasParentCommand)
        m_commandStack->push(command);
}

void CustomTrackView::prepareResizeClipEnd(AbstractClipItem* item, ItemInfo oldInfo, int pos, bool check, QUndoCommand *command)
{
    if (pos == oldInfo.endPos.frames(m_document->fps()))
        return;
    bool snap = KdenliveSettings::snaptopoints();
    if (check) {
        KdenliveSettings::setSnaptopoints(false);
        item->resizeEnd(pos);
        if (item->endPos().frames(m_document->fps()) != pos) {
            item->resizeEnd(oldInfo.endPos.frames(m_document->fps()));
            emit displayMessage(i18n("Not possible to resize"), ErrorMessage);
            KdenliveSettings::setSnaptopoints(snap);
            return;
        }
        KdenliveSettings::setSnaptopoints(snap);
    }

    bool hasParentCommand = false;
    if (command) {
        hasParentCommand = true;
    } else {
        command = new QUndoCommand();
    }

    // do this here, too, because otherwise undo won't update the group
    if (item->parentItem() && item->parentItem() != m_selectionGroup)
        new RebuildGroupCommand(this, item->info().track, item->startPos(), command);

    ItemInfo info = item->info();
    if (item->type() == AVWIDGET) {
        if (!hasParentCommand) command->setText(i18n("Resize clip end"));
        ItemInfo resizeinfo = info;
        resizeinfo.track = m_document->tracksCount() - resizeinfo.track;
        bool success = m_document->renderer()->mltResizeClipEnd(resizeinfo, resizeinfo.cropDuration);
        if (success) {
            // Check if there is an automatic transition on that clip (lower track)
            Transition *tr = getTransitionItemAtEnd(oldInfo.endPos, oldInfo.track);
            if (tr && tr->isAutomatic()) {
                ItemInfo trInfo = tr->info();
                ItemInfo newTrInfo = trInfo;
                newTrInfo.endPos = item->endPos();
                if (newTrInfo.endPos > newTrInfo.startPos)
                    new MoveTransitionCommand(this, trInfo, newTrInfo, true, command);
            }

            // Check if there is an automatic transition on that clip (upper track)
            tr = getTransitionItemAtEnd(oldInfo.endPos, oldInfo.track - 1);
            if (tr && tr->isAutomatic() && (m_document->tracksCount() - tr->transitionEndTrack()) == oldInfo.track) {
                ItemInfo trInfo = tr->info();
                ItemInfo newTrInfo = trInfo;
                newTrInfo.endPos = item->endPos();
                ClipItem * upperClip = getClipItemAtEnd(oldInfo.endPos, oldInfo.track - 1);
                if ((!upperClip || !upperClip->baseClip()->isTransparent()) && newTrInfo.endPos > newTrInfo.startPos)
                    new MoveTransitionCommand(this, trInfo, newTrInfo, true, command);

            }

            ClipItem *clip = static_cast < ClipItem * >(item);

            // Hack:
            // Since we must always resize clip before updating the keyframes, we
            // put a resize command before & after checking keyframes so that
            // we are sure the resize is performed before whenever we do or undo the action
            new ResizeClipCommand(this, oldInfo, info, false, true, command);
            adjustEffects(clip, oldInfo, command);
            new ResizeClipCommand(this, oldInfo, info, false, true, command);
        } else {
            KdenliveSettings::setSnaptopoints(false);
            item->resizeEnd((int) oldInfo.endPos.frames(m_document->fps()));
            KdenliveSettings::setSnaptopoints(true);
            emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
        }
    } else if (item->type() == TRANSITIONWIDGET) {
        if (!hasParentCommand) command->setText(i18n("Resize transition end"));
        Transition *transition = static_cast <Transition *>(item);
        if (!m_document->renderer()->mltMoveTransition(transition->transitionTag(), (int)(m_document->tracksCount() - oldInfo.track), (int)(m_document->tracksCount() - oldInfo.track), transition->transitionEndTrack(), oldInfo.startPos, oldInfo.endPos, info.startPos, info.endPos)) {
            // Cannot resize transition
            KdenliveSettings::setSnaptopoints(false);
            transition->resizeEnd((int) oldInfo.endPos.frames(m_document->fps()));
            KdenliveSettings::setSnaptopoints(true);
            emit displayMessage(i18n("Cannot resize transition"), ErrorMessage);
        } else {
            // Check transition keyframes
            QDomElement old = transition->toXML();
            if (transition->updateKeyframes()) {
                QDomElement xml = transition->toXML();
                m_document->renderer()->mltUpdateTransition(xml.attribute("tag"), xml.attribute("tag"), xml.attribute("transition_btrack").toInt(), m_document->tracksCount() - xml.attribute("transition_atrack").toInt(), transition->startPos(), transition->endPos(), xml);
                new EditTransitionCommand(this, transition->track(), transition->startPos(), old, xml, false, command);
            }
            new MoveTransitionCommand(this, oldInfo, info, false, command);
        }
    }
    if (item->parentItem() && item->parentItem() != m_selectionGroup)
        new RebuildGroupCommand(this, item->info().track, item->startPos(), command);

    if (!hasParentCommand) {
        updateTrackDuration(oldInfo.track, command);
        m_commandStack->push(command);
    }
}

void CustomTrackView::updatePositionEffects(ClipItem* item, ItemInfo info, bool standalone)
{
    int end = item->fadeIn();
    if (end != 0) {
        // there is a fade in effect
        int effectPos = item->hasEffect("volume", "fadein");
        if (effectPos != -1) {
            QDomElement effect = item->getEffectAtIndex(effectPos);
            int start = item->cropStart().frames(m_document->fps());
            int max = item->cropDuration().frames(m_document->fps());
            if (end > max) {
                // Make sure the fade effect is not longer than the clip
                item->setFadeIn(max);
                end = item->fadeIn();
            }
            end += start;
            EffectsList::setParameter(effect, "in", QString::number(start));
            EffectsList::setParameter(effect, "out", QString::number(end));
            if (standalone) {
                if (!m_document->renderer()->mltEditEffect(m_document->tracksCount() - item->track(), item->startPos(), getEffectArgs(effect)))
                    emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
                // if fade effect is displayed, update the effect edit widget with new clip duration
                if (item->isSelected() && effectPos == item->selectedEffectIndex())
                    emit clipItemSelected(item);
            }
        }
        effectPos = item->hasEffect("brightness", "fade_from_black");
        if (effectPos != -1) {
            QDomElement effect = item->getEffectAtIndex(effectPos);
            int start = item->cropStart().frames(m_document->fps());
            int max = item->cropDuration().frames(m_document->fps());
            if (end > max) {
                // Make sure the fade effect is not longer than the clip
                item->setFadeIn(max);
                end = item->fadeIn();
            }
            end += start;
            EffectsList::setParameter(effect, "in", QString::number(start));
            EffectsList::setParameter(effect, "out", QString::number(end));
            if (standalone) {
                if (!m_document->renderer()->mltEditEffect(m_document->tracksCount() - item->track(), item->startPos(), getEffectArgs(effect)))
                    emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
                // if fade effect is displayed, update the effect edit widget with new clip duration
                if (item->isSelected() && effectPos == item->selectedEffectIndex())
                    emit clipItemSelected(item);
            }
        }
    }

    int start = item->fadeOut();
    if (start != 0) {
        // there is a fade out effect
        int effectPos = item->hasEffect("volume", "fadeout");
        if (effectPos != -1) {
            QDomElement effect = item->getEffectAtIndex(effectPos);
            int max = item->cropDuration().frames(m_document->fps());
            int end = max + item->cropStart().frames(m_document->fps());
            if (start > max) {
                // Make sure the fade effect is not longer than the clip
                item->setFadeOut(max);
                start = item->fadeOut();
            }
            start = end - start;
            EffectsList::setParameter(effect, "in", QString::number(start));
            EffectsList::setParameter(effect, "out", QString::number(end));
            if (standalone) {
                if (!m_document->renderer()->mltEditEffect(m_document->tracksCount() - item->track(), item->startPos(), getEffectArgs(effect)))
                    emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
                // if fade effect is displayed, update the effect edit widget with new clip duration
                if (item->isSelected() && effectPos == item->selectedEffectIndex())
                    emit clipItemSelected(item);
            }
        }
        effectPos = item->hasEffect("brightness", "fade_to_black");
        if (effectPos != -1) {
            QDomElement effect = item->getEffectAtIndex(effectPos);
            int max = item->cropDuration().frames(m_document->fps());
            int end = max + item->cropStart().frames(m_document->fps());
            if (start > max) {
                // Make sure the fade effect is not longer than the clip
                item->setFadeOut(max);
                start = item->fadeOut();
            }
            start = end - start;
            EffectsList::setParameter(effect, "in", QString::number(start));
            EffectsList::setParameter(effect, "out", QString::number(end));
            if (standalone) {
                if (!m_document->renderer()->mltEditEffect(m_document->tracksCount() - item->track(), item->startPos(), getEffectArgs(effect)))
                    emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
                // if fade effect is displayed, update the effect edit widget with new clip duration
                if (item->isSelected() && effectPos == item->selectedEffectIndex())
                    emit clipItemSelected(item);
            }
        }
    }

    int effectPos = item->hasEffect("freeze", "freeze");
    if (effectPos != -1) {
        // Freeze effect needs to be adjusted with clip resize
        int diff = (info.startPos - item->startPos()).frames(m_document->fps());
        QDomElement eff = item->getEffectAtIndex(effectPos);
        if (!eff.isNull() && diff != 0) {
            int freeze_pos = EffectsList::parameter(eff, "frame").toInt() + diff;
            EffectsList::setParameter(eff, "frame", QString::number(freeze_pos));
            if (standalone) {
                if (item->isSelected() && item->selectedEffect().attribute("id") == "freeze") {
                    emit clipItemSelected(item);
                }
            }
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

    // add render zone
    QPoint z = m_document->zone();
    snaps.append(GenTime(z.x(), m_document->fps()));
    snaps.append(GenTime(z.y(), m_document->fps()));

    qSort(snaps);
    m_scene->setSnapList(snaps);
    //for (int i = 0; i < m_snapPoints.size(); ++i)
    //    kDebug() << "SNAP POINT: " << m_snapPoints.at(i).frames(25);
}

void CustomTrackView::slotSeekToPreviousSnap()
{
    updateSnapPoints(NULL);
    GenTime res = m_scene->previousSnapPoint(GenTime(m_cursorPos, m_document->fps()));
    seekCursorPos((int) res.frames(m_document->fps()));
    checkScrolling();
}

void CustomTrackView::slotSeekToNextSnap()
{
    updateSnapPoints(NULL);
    GenTime res = m_scene->nextSnapPoint(GenTime(m_cursorPos, m_document->fps()));
    seekCursorPos((int) res.frames(m_document->fps()));
    checkScrolling();
}

void CustomTrackView::clipStart()
{
    AbstractClipItem *item = getMainActiveClip();
    if (item != NULL) {
        seekCursorPos((int) item->startPos().frames(m_document->fps()));
        checkScrolling();
    }
}

void CustomTrackView::clipEnd()
{
    AbstractClipItem *item = getMainActiveClip();
    if (item != NULL) {
        seekCursorPos((int) item->endPos().frames(m_document->fps()) - 1);
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

void CustomTrackView::addMarker(const QString &id, const GenTime &pos, const QString &comment)
{
    DocClipBase *base = m_document->clipManager()->getClipById(id);
    if (!comment.isEmpty()) base->addSnapMarker(pos, comment);
    else base->deleteSnapMarker(pos);
    emit updateClipMarkers(base);
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

void CustomTrackView::buildGuidesMenu(QMenu *goMenu) const
{
    QAction *act;
    goMenu->clear();
    double fps = m_document->fps();
    for (int i = 0; i < m_guides.count(); i++) {
        act = goMenu->addAction(m_guides.at(i)->label() + '/' + Timecode::getStringTimecode(m_guides.at(i)->position().frames(fps), fps));
        act->setData(m_guides.at(i)->position().frames(m_document->fps()));
    }
    goMenu->setEnabled(!m_guides.isEmpty());
}

void CustomTrackView::editGuide(const GenTime &oldPos, const GenTime &pos, const QString &comment)
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

bool CustomTrackView::addGuide(const GenTime &pos, const QString &comment)
{
    for (int i = 0; i < m_guides.count(); i++) {
        if (m_guides.at(i)->position() == pos) {
            emit displayMessage(i18n("A guide already exists at position %1", m_document->timecode().getTimecodeFromFrames(pos.frames(m_document->fps()))), ErrorMessage);
            return false;
        }
    }
    Guide *g = new Guide(this, pos, comment, m_tracksHeight * m_document->tracksCount() * matrix().m22());
    scene()->addItem(g);
    m_guides.append(g);
    qSort(m_guides.begin(), m_guides.end(), sortGuidesList);
    m_document->syncGuides(m_guides);
    return true;
}

void CustomTrackView::slotAddGuide(bool dialog)
{
    CommentedTime marker(GenTime(m_cursorPos, m_document->fps()), i18n("Guide"));
    if (dialog) {
        QPointer<MarkerDialog> d = new MarkerDialog(NULL, marker,
                             m_document->timecode(), i18n("Add Guide"), this);
        if (d->exec() != QDialog::Accepted) {
            delete d;
            return;
        }
        marker = d->newMarker();
        delete d;
    } else {
        marker.setComment(m_document->timecode().getDisplayTimecodeFromFrames(m_cursorPos, false));
    }
    if (addGuide(marker.time(), marker.comment())) {
        EditGuideCommand *command = new EditGuideCommand(this, GenTime(), QString(), marker.time(), marker.comment(), false);
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
    QPointer<MarkerDialog> d = new MarkerDialog(NULL, guide, m_document->timecode(), i18n("Edit Guide"), this);
    if (d->exec() == QDialog::Accepted) {
        EditGuideCommand *command = new EditGuideCommand(this, guide.time(), guide.comment(), d->newMarker().time(), d->newMarker().comment(), true);
        m_commandStack->push(command);
    }
    delete d;
}


void CustomTrackView::slotEditTimeLineGuide()
{
    if (m_dragGuide == NULL) return;
    CommentedTime guide = m_dragGuide->info();
    QPointer<MarkerDialog> d = new MarkerDialog(NULL, guide,
                     m_document->timecode(), i18n("Edit Guide"), this);
    if (d->exec() == QDialog::Accepted) {
        EditGuideCommand *command = new EditGuideCommand(this, guide.time(), guide.comment(), d->newMarker().time(), d->newMarker().comment(), true);
        m_commandStack->push(command);
    }
    delete d;
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
    QMatrix newmatrix;
    newmatrix = newmatrix.scale(scaleFactor, verticalScale);
    m_scene->setScale(scaleFactor, verticalScale);
    removeTipAnimation();
    bool adjust = false;
    if (verticalScale != matrix().m22()) adjust = true;
    setMatrix(newmatrix);
    if (adjust) {
        double newHeight = m_tracksHeight * m_document->tracksCount() * matrix().m22();
        m_cursorLine->setLine(m_cursorLine->line().x1(), 0, m_cursorLine->line().x1(), newHeight - 1);
        for (int i = 0; i < m_guides.count(); i++) {
            QLineF l = m_guides.at(i)->line();
            l.setP2(QPointF(l.x2(), newHeight));
            m_guides.at(i)->setLine(l);
        }
        setSceneRect(0, 0, sceneRect().width(), m_tracksHeight * m_document->tracksCount());
    }

    int diff = sceneRect().width() - m_projectDuration;
    if (diff * newmatrix.m11() < 50) {
        if (newmatrix.m11() < 0.4)
            setSceneRect(0, 0, (m_projectDuration + 100 / newmatrix.m11()), sceneRect().height());
        else
            setSceneRect(0, 0, (m_projectDuration + 300), sceneRect().height());
    }
    double verticalPos = mapToScene(QPoint(0, viewport()->height() / 2)).y();
    centerOn(QPointF(cursorPos(), verticalPos));
}

void CustomTrackView::slotRefreshGuides()
{
    if (KdenliveSettings::showmarkers()) {
        for (int i = 0; i < m_guides.count(); i++)
            m_guides.at(i)->update();
    }
}

void CustomTrackView::drawBackground(QPainter * painter, const QRectF &rect)
{
    painter->setClipRect(rect);
    QPen pen1 = painter->pen();
    pen1.setColor(palette().dark().color());
    painter->setPen(pen1);
    double min = rect.left();
    double max = rect.right();
    painter->drawLine(QPointF(min, 0), QPointF(max, 0));
    int maxTrack = m_document->tracksCount();
    QColor lockedColor = palette().button().color();
    QColor audioColor = palette().alternateBase().color();
    for (int i = 0; i < maxTrack; i++) {
        TrackInfo info = m_document->trackInfoAt(maxTrack - i - 1);
        if (info.isLocked || info.type == AUDIOTRACK || i == m_selectedTrack) {
            const QRectF track(min, m_tracksHeight * i + 1, max - min, m_tracksHeight - 1);
            if (i == m_selectedTrack)
                painter->fillRect(track, m_activeTrackBrush.brush(this));
            else
                painter->fillRect(track, info.isLocked ? lockedColor : audioColor);
        }
        painter->drawLine(QPointF(min, m_tracksHeight *(i + 1)), QPointF(max, m_tracksHeight *(i + 1)));
    }
}

bool CustomTrackView::findString(const QString &text)
{
    QString marker;
    for (int i = 0; i < m_searchPoints.size(); ++i) {
        marker = m_searchPoints.at(i).comment();
        if (marker.contains(text, Qt::CaseInsensitive)) {
            seekCursorPos(m_searchPoints.at(i).time().frames(m_document->fps()));
            int vert = verticalScrollBar()->value();
            int hor = cursorPos();
            ensureVisible(hor, vert + 10, 2, 2, 50, 0);
            m_findIndex = i;
            return true;
        }
    }
    return false;
}

void CustomTrackView::selectFound(QString track, QString pos)
{
    seekCursorPos(m_document->timecode().getFrameCount(pos));
    slotSelectTrack(track.toInt());
    selectClip(true);
    int vert = verticalScrollBar()->value();
    int hor = cursorPos();
    ensureVisible(hor, vert + 10, 2, 2, 50, 0);
}

bool CustomTrackView::findNextString(const QString &text)
{
    QString marker;
    for (int i = m_findIndex + 1; i < m_searchPoints.size(); ++i) {
        marker = m_searchPoints.at(i).comment();
        if (marker.contains(text, Qt::CaseInsensitive)) {
            seekCursorPos(m_searchPoints.at(i).time().frames(m_document->fps()));
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
    for (int i = 0; i < m_guides.count(); i++)
        m_searchPoints.append(m_guides.at(i)->info());

    qSort(m_searchPoints);
}

void CustomTrackView::clearSearchStrings()
{
    m_searchPoints.clear();
    m_findIndex = 0;
}

QList<ItemInfo> CustomTrackView::findId(const QString &clipId)
{
    QList<ItemInfo> matchingInfo;
    QList<QGraphicsItem *> itemList = items();
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            ClipItem *item = (ClipItem *)itemList.at(i);
            if (item->clipProducer() == clipId)
                matchingInfo << item->info();
        }
    }
    return matchingInfo;
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
            ClipItem *clip = static_cast <ClipItem *>(itemList.at(i));
            ClipItem *clone = clip->clone(clip->info());
            m_copiedItems.append(clone);
        } else if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            Transition *dup = static_cast <Transition *>(itemList.at(i));
            m_copiedItems.append(dup->clone());
        }
    }
}

bool CustomTrackView::canBePastedTo(ItemInfo info, int type) const
{
    if (m_scene->editMode() != NORMALEDIT) {
        // If we are in overwrite mode, always allow the move
        return true;
    }
    int height = m_tracksHeight - 2;
    int offset = 0;
    if (type == TRANSITIONWIDGET) {
        height = Transition::itemHeight();
        offset = Transition::itemOffset();
    }
    else if (type == AVWIDGET) {
        height = ClipItem::itemHeight();
        offset = ClipItem::itemOffset();
    }
    QRectF rect((double) info.startPos.frames(m_document->fps()), (double)(info.track * m_tracksHeight + 1 + offset), (double)(info.endPos - info.startPos).frames(m_document->fps()), (double) height);
    QList<QGraphicsItem *> collisions = scene()->items(rect, Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisions.count(); i++) {
        if (collisions.at(i)->type() == type) {
            return false;
        }
    }
    return true;
}

bool CustomTrackView::canBePastedTo(QList <ItemInfo> infoList, int type) const
{
    QPainterPath path;
    for (int i = 0; i < infoList.count(); i++) {
        const QRectF rect((double) infoList.at(i).startPos.frames(m_document->fps()), (double)(infoList.at(i).track * m_tracksHeight + 1), (double)(infoList.at(i).endPos - infoList.at(i).startPos).frames(m_document->fps()), (double)(m_tracksHeight - 1));
        path.addRect(rect);
    }
    QList<QGraphicsItem *> collisions = scene()->items(path);
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
    int track = -1;
    GenTime pos = GenTime(-1);
    if (m_menuPosition.isNull()) {
        position = mapFromGlobal(QCursor::pos());
        if (!contentsRect().contains(position) || mapToScene(position).y() / m_tracksHeight > m_document->tracksCount()) {
            track = m_selectedTrack;
            pos = GenTime(m_cursorPos, m_document->fps());
            /*emit displayMessage(i18n("Cannot paste selected clips"), ErrorMessage);
            return;*/
        }
    } else {
        position = m_menuPosition;
    }

    if (pos == GenTime(-1))
        pos = GenTime((int)(mapToScene(position).x()), m_document->fps());
    if (track == -1)
        track = (int)(mapToScene(position).y() / m_tracksHeight);

    GenTime leftPos = m_copiedItems.at(0)->startPos();
    int lowerTrack = m_copiedItems.at(0)->track();
    int upperTrack = m_copiedItems.at(0)->track();
    for (int i = 1; i < m_copiedItems.count(); i++) {
        if (m_copiedItems.at(i)->startPos() < leftPos) leftPos = m_copiedItems.at(i)->startPos();
        if (m_copiedItems.at(i)->track() < lowerTrack) lowerTrack = m_copiedItems.at(i)->track();
        if (m_copiedItems.at(i)->track() > upperTrack) upperTrack = m_copiedItems.at(i)->track();
    }

    GenTime offset = pos - leftPos;
    int trackOffset = track - lowerTrack;

    if (lowerTrack + trackOffset < 0) trackOffset = 0 - lowerTrack;
    if (upperTrack + trackOffset > m_document->tracksCount() - 1) trackOffset = m_document->tracksCount() - upperTrack - 1;
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
            ItemInfo info = clip->info();
            info.startPos += offset;
            info.endPos += offset;
            info.track += trackOffset;
            if (canBePastedTo(info, AVWIDGET)) {
                new AddTimelineClipCommand(this, clip->xml(), clip->clipProducer(), info, clip->effectList(), m_scene->editMode() == OVERWRITEEDIT, m_scene->editMode() == INSERTEDIT, true, false, pasteClips);
            } else emit displayMessage(i18n("Cannot paste clip to selected place"), ErrorMessage);
        } else if (m_copiedItems.at(i) && m_copiedItems.at(i)->type() == TRANSITIONWIDGET) {
            Transition *tr = static_cast <Transition *>(m_copiedItems.at(i));
            ItemInfo info;
            info.startPos = tr->startPos() + offset;
            info.endPos = tr->endPos() + offset;
            info.track = tr->track() + trackOffset;
            int transitionEndTrack;
            if (!tr->forcedTrack()) transitionEndTrack = getPreviousVideoTrack(info.track);
            else transitionEndTrack = tr->transitionEndTrack();
            if (canBePastedTo(info, TRANSITIONWIDGET)) {
                if (info.startPos >= info.endPos) {
                    emit displayMessage(i18n("Invalid transition"), ErrorMessage);
                } else new AddTransitionCommand(this, info, transitionEndTrack, tr->toXML(), false, true, pasteClips);
            } else emit displayMessage(i18n("Cannot paste transition to selected place"), ErrorMessage);
        }
    }
    updateTrackDuration(-1, pasteClips);
    m_commandStack->push(pasteClips);
}

void CustomTrackView::pasteClipEffects()
{
    if (m_copiedItems.count() != 1 || m_copiedItems.at(0)->type() != AVWIDGET) {
        emit displayMessage(i18n("You must copy exactly one clip before pasting effects"), ErrorMessage);
        return;
    }
    ClipItem *clip = static_cast < ClipItem *>(m_copiedItems.at(0));

    QUndoCommand *paste = new QUndoCommand();
    paste->setText("Paste effects");

    QList<QGraphicsItem *> clips = scene()->selectedItems();

    // expand groups
    for (int i = 0; i < clips.count(); ++i) {
        if (clips.at(i)->type() == GROUPWIDGET) {
            QList<QGraphicsItem *> children = clips.at(i)->childItems();
            for (int j = 0; j < children.count(); j++) {
                if (children.at(j)->type() == AVWIDGET && !clips.contains(children.at(j))) {
                    clips.append(children.at(j));
                }
            }
        }
    }

    for (int i = 0; i < clips.count(); ++i) {
        if (clips.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast < ClipItem *>(clips.at(i));
            for (int j = 0; j < clip->effectsCount(); j++) {
                QDomElement eff = clip->effect(j);
                if (eff.attribute("unique", "0") == "0" || item->hasEffect(eff.attribute("tag"), eff.attribute("id")) == -1) {
                    adjustKeyfames(clip->cropStart(), item->cropStart(), item->cropDuration(), eff);
                    new AddEffectCommand(this, m_document->tracksCount() - item->track(), item->startPos(), eff, true, paste);
                }
            }
        }
    }
    if (paste->childCount() > 0) m_commandStack->push(paste);
    else delete paste;

    //adjust effects (fades, ...)
    for (int i = 0; i < clips.count(); ++i) {
        if (clips.at(i)->type() == AVWIDGET) {
            ClipItem *item = static_cast < ClipItem *>(clips.at(i));
            updatePositionEffects(item, item->info());
        }
    }
}


void CustomTrackView::adjustKeyfames(GenTime oldstart, GenTime newstart, GenTime duration, QDomElement xml)
{
    // parse parameters to check if we need to adjust to the new crop start
    int diff = (newstart - oldstart).frames(m_document->fps());
    int max = (newstart + duration).frames(m_document->fps());
    QLocale locale;
    QDomNodeList params = xml.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull() && (e.attribute("type") == "keyframe" || e.attribute("type") == "simplekeyframe")) {
            QString def = e.attribute("default");
            // Effect has a keyframe type parameter, we need to adjust the values
            QStringList keys = e.attribute("keyframes").split(';', QString::SkipEmptyParts);
            QStringList newKeyFrames;
            foreach(const QString &str, keys) {
                int pos = str.section(':', 0, 0).toInt();
                double val = str.section(':', 1, 1).toDouble();
                pos += diff;
                if (pos > max) {
                    newKeyFrames.append(QString::number(max) + ':' + locale.toString(val));
                    break;
                } else newKeyFrames.append(QString::number(pos) + ':' + locale.toString(val));
            }
            //kDebug()<<"ORIGIN: "<<keys<<", FIXED: "<<newKeyFrames;
            e.setAttribute("keyframes", newKeyFrames.join(";"));
        }
    }
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

AbstractClipItem *CustomTrackView::getMainActiveClip() const
{
    QList<QGraphicsItem *> clips = scene()->selectedItems();
    if (clips.isEmpty()) {
        return getClipUnderCursor();
    } else {
        AbstractClipItem *item = NULL;
        for (int i = 0; i < clips.count(); ++i) {
            if (clips.at(i)->type() == AVWIDGET) {
                item = static_cast < AbstractClipItem *>(clips.at(i));
                if (clips.count() > 1 && item->startPos().frames(m_document->fps()) <= m_cursorPos && item->endPos().frames(m_document->fps()) >= m_cursorPos) break;
            }
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
    prepareResizeClipStart(clip, clip->info(), m_cursorPos, true);
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
    prepareResizeClipEnd(clip, clip->info(), m_cursorPos, true);
}

void CustomTrackView::slotUpdateAllThumbs()
{
    if (!isEnabled()) return;
    QList<QGraphicsItem *> itemList = items();
    //if (itemList.isEmpty()) return;
    ClipItem *item;
    const QString thumbBase = m_document->projectFolder().path() + "/thumbs/";
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == AVWIDGET) {
            item = static_cast <ClipItem *>(itemList.at(i));
            if (item && item->isEnabled() && item->clipType() != COLOR && item->clipType() != AUDIO) {
                // Check if we have a cached thumbnail
                if (item->clipType() == IMAGE || item->clipType() == TEXT) {
                    QString thumb = thumbBase + item->baseClip()->getClipHash() + "_0.png";
                    if (QFile::exists(thumb)) {
                        QPixmap pix(thumb);
                        if (pix.isNull()) KIO::NetAccess::del(KUrl(thumb), this);
                        item->slotSetStartThumb(pix);
                    }
                } else {
                    QString startThumb = thumbBase + item->baseClip()->getClipHash() + '_';
                    QString endThumb = startThumb;
                    startThumb.append(QString::number((int) item->speedIndependantCropStart().frames(m_document->fps())) + ".png");
                    endThumb.append(QString::number((int) (item->speedIndependantCropStart() + item->speedIndependantCropDuration()).frames(m_document->fps()) - 1) + ".png");
                    if (QFile::exists(startThumb)) {
                        QPixmap pix(startThumb);
                        if (pix.isNull()) KIO::NetAccess::del(KUrl(startThumb), this);
                        item->slotSetStartThumb(pix);
                    }
                    if (QFile::exists(endThumb)) {
                        QPixmap pix(endThumb);
                        if (pix.isNull()) KIO::NetAccess::del(KUrl(endThumb), this);
                        item->slotSetEndThumb(pix);
                    }
                }
                item->refreshClip(false, false);
            }
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
                    startThumb.append(QString::number((int) item->speedIndependantCropStart().frames(m_document->fps())) + ".png");
                    endThumb.append(QString::number((int) (item->speedIndependantCropStart() + item->speedIndependantCropDuration()).frames(m_document->fps()) - 1) + ".png");
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
    QPointer<TrackDialog> d = new TrackDialog(m_document, parentWidget());
    d->comboTracks->setCurrentIndex(ix);
    d->label->setText(i18n("Insert track"));
    d->setWindowTitle(i18n("Insert New Track"));

    if (d->exec() == QDialog::Accepted) {
        ix = d->comboTracks->currentIndex();
        if (d->before_select->currentIndex() == 1)
            ix++;
        TrackInfo info;
        info.duration = 0;
        info.isMute = false;
        info.isLocked = false;
	info.effectsList = EffectsList(true);
        if (d->video_track->isChecked()) {
            info.type = VIDEOTRACK;
            info.isBlind = false;
        } else {
            info.type = AUDIOTRACK;
            info.isBlind = true;
        }
        AddTrackCommand *addTrack = new AddTrackCommand(this, ix, info, true);
        m_commandStack->push(addTrack);
        setDocumentModified();
    }
    delete d;
}

void CustomTrackView::slotDeleteTrack(int ix)
{
    if (m_document->tracksCount() < 2) return;
    QPointer<TrackDialog> d = new TrackDialog(m_document, parentWidget());
    d->comboTracks->setCurrentIndex(ix);
    d->label->setText(i18n("Delete track"));
    d->before_select->setHidden(true);
    d->setWindowTitle(i18n("Delete Track"));
    d->video_track->setHidden(true);
    d->audio_track->setHidden(true);
    if (d->exec() == QDialog::Accepted) {
        ix = d->comboTracks->currentIndex();
        TrackInfo info = m_document->trackInfoAt(m_document->tracksCount() - ix - 1);
        deleteTimelineTrack(ix, info);
        setDocumentModified();
        /*AddTrackCommand* command = new AddTrackCommand(this, ix, info, false);
        m_commandStack->push(command);*/
    }
    delete d;
}

void CustomTrackView::slotConfigTracks(int ix)
{
    QPointer<TracksConfigDialog> d = new TracksConfigDialog(m_document,
                                                        ix, parentWidget());
    if (d->exec() == QDialog::Accepted) {
        ConfigTracksCommand *configTracks = new ConfigTracksCommand(this, m_document->tracksList(), d->tracksList());
        m_commandStack->push(configTracks);
        QList <int> toDelete = d->deletedTracks();
        for (int i = 0; i < toDelete.count(); ++i) {
            TrackInfo info = m_document->trackInfoAt(m_document->tracksCount() - toDelete.at(i) + i - 1);
            deleteTimelineTrack(toDelete.at(i) - i, info);
        }
        setDocumentModified();
    }
    delete d;
}

void CustomTrackView::deleteTimelineTrack(int ix, TrackInfo trackinfo)
{
    if (m_document->tracksCount() < 2) return;
    double startY = ix * m_tracksHeight + 1 + m_tracksHeight / 2;
    QRectF r(0, startY, sceneRect().width(), m_tracksHeight / 2 - 1);
    QList<QGraphicsItem *> selection = m_scene->items(r);
    QUndoCommand *deleteTrack = new QUndoCommand();
    deleteTrack->setText("Delete track");

    // Delete all clips in selected track
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == AVWIDGET) {
            ClipItem *item =  static_cast <ClipItem *>(selection.at(i));
            new AddTimelineClipCommand(this, item->xml(), item->clipProducer(), item->info(), item->effectList(), false, false, false, true, deleteTrack);
            m_waitingThumbs.removeAll(item);
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

    new AddTrackCommand(this, ix, trackinfo, false, deleteTrack);
    m_commandStack->push(deleteTrack);
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
    setDocumentModified();
}

void CustomTrackView::clipNameChanged(const QString &id, const QString &name)
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

void CustomTrackView::loadGroups(const QDomNodeList &groups)
{
    for (int i = 0; i < groups.count(); i++) {
        QDomNodeList children = groups.at(i).childNodes();
        scene()->clearSelection();
        for (int nodeindex = 0; nodeindex < children.count(); nodeindex++) {
            QDomElement elem = children.item(nodeindex).toElement();
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
        emit displayMessage(i18n("You must select at least one clip for this action"), ErrorMessage);
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
                    EffectsList effects;
                    effects.clone(clip->effectList());
                    new SplitAudioCommand(this, clip->track(), clip->startPos(), effects, splitCommand);
                }
            }
        }
    }
    if (splitCommand->childCount()) {
        updateTrackDuration(-1, splitCommand);
        m_commandStack->push(splitCommand);
    }
}

void CustomTrackView::setAudioAlignReference()
{
    QList<QGraphicsItem *> selection = scene()->selectedItems();
    if (selection.isEmpty() || selection.size() > 1) {
        emit displayMessage(i18n("You must select exactly one clip for the audio reference."), ErrorMessage);
        return;
    }
    if (m_audioCorrelator != NULL) {
        delete m_audioCorrelator;
    }
    if (selection.at(0)->type() == AVWIDGET) {
        ClipItem *clip = static_cast<ClipItem*>(selection.at(0));
        if (clip->clipType() == AV || clip->clipType() == AUDIO) {
            m_audioAlignmentReference = clip;

            AudioEnvelope *envelope = new AudioEnvelope(clip->getProducer(clip->track()));
            m_audioCorrelator = new AudioCorrelation(envelope);


#ifdef DEBUG
            envelope->drawEnvelope().save("kdenlive-audio-reference-envelope.png");
            envelope->dumpInfo();
#endif


            emit displayMessage(i18n("Audio align reference set."), InformationMessage);
        }
        return;
    }
    emit displayMessage(i18n("Reference for audio alignment must contain audio data."), ErrorMessage);
}

void CustomTrackView::alignAudio()
{
    bool referenceOK = true;
    if (m_audioCorrelator == NULL) {
        referenceOK = false;
    }
    if (referenceOK) {
        if (!scene()->items().contains(m_audioAlignmentReference)) {
            // The reference item has been deleted from the timeline (or so)
            referenceOK = false;
        }
    }
    if (!referenceOK) {
        emit displayMessage(i18n("Audio alignment reference not yet set."), InformationMessage);
        return;
    }

    int counter = 0;
    QList<QGraphicsItem *> selection = scene()->selectedItems();
    foreach (QGraphicsItem *item, selection) {
        if (item->type() == AVWIDGET) {

            ClipItem *clip = static_cast<ClipItem*>(item);
            if (clip == m_audioAlignmentReference) {
                continue;
            }

            if (clip->clipType() == AV || clip->clipType() == AUDIO) {
                AudioEnvelope *envelope = new AudioEnvelope(clip->getProducer(clip->track()),
                                                            clip->info().cropStart.frames(m_document->fps()),
                                                            clip->info().cropDuration.frames(m_document->fps()));

                // FFT only for larger vectors. We could use it all time, but for small vectors
                // the (anyway not noticeable) overhead is smaller with a nested for loop correlation.
                int index = m_audioCorrelator->addChild(envelope, envelope->envelopeSize() > 200);
                int shift = m_audioCorrelator->getShift(index);
                counter++;


#ifdef DEBUG
                m_audioCorrelator->info(index)->toImage().save("kdenlive-audio-align-cross-correlation.png");
                envelope->drawEnvelope().save("kdenlive-audio-align-envelope.png");
                envelope->dumpInfo();

                int targetPos = m_audioAlignmentReference->startPos().frames(m_document->fps()) + shift;
                qDebug() << "Reference starts at " << m_audioAlignmentReference->startPos().frames(m_document->fps());
                qDebug() << "We will start at " << targetPos;
                qDebug() << "to shift by " << shift;
                qDebug() << "(eventually)";
                qDebug() << "(maybe)";
#endif


                QUndoCommand *moveCommand = new QUndoCommand();

                GenTime add(shift, m_document->fps());
                ItemInfo start = clip->info();

                ItemInfo end = start;
                end.startPos = m_audioAlignmentReference->startPos() + add - m_audioAlignmentReference->cropStart();
                end.endPos = end.startPos + start.cropDuration;

                if ( end.startPos.seconds() < 0 ) {
                    // Clip would start before 0, so crop it first
                    GenTime cropBy = -end.startPos;

#ifdef DEBUG
                    qDebug() << "Need to crop clip. " << start;
                    qDebug() << "end.startPos: " << end.startPos.toString() << ", cropBy: " << cropBy.toString();
#endif

                    ItemInfo resized = start;
                    resized.startPos += cropBy;

                    resizeClip(start, resized);
                    new ResizeClipCommand(this, start, resized, false, false, moveCommand);

                    start = clip->info();
                    end.startPos += cropBy;

#ifdef DEBUG
                    qDebug() << "Clip cropped. " << start;
                    qDebug() << "Moving to: " << end;
#endif
                }

                if (itemCollision(clip, end)) {
                    delete moveCommand;
                    emit displayMessage(i18n("Unable to move clip due to collision."), ErrorMessage);
                    return;
                }

                moveCommand->setText(i18n("Auto-align clip"));
                new MoveClipCommand(this, start, end, true, moveCommand);
                updateTrackDuration(clip->track(), moveCommand);
                m_commandStack->push(moveCommand);

            }
        }
    }

    if (counter == 0) {
        emit displayMessage(i18n("No audio clips selected."), ErrorMessage);
    } else {
        emit displayMessage(i18n("Auto-aligned %1 clips.", counter), InformationMessage);
    }
}

void CustomTrackView::doSplitAudio(const GenTime &pos, int track, EffectsList effects, bool split)
{
    ClipItem *clip = getClipItemAt(pos, track);
    if (clip == NULL) {
        kDebug() << "// Cannot find clip to split!!!";
        return;
    }
    if (split) {
        int start = pos.frames(m_document->fps());
        int freetrack = m_document->tracksCount() - track - 1;

        // do not split audio when we are on an audio track
        if (m_document->trackInfoAt(freetrack).type == AUDIOTRACK)
            return;

        for (; freetrack > 0; freetrack--) {
            //kDebug() << "// CHK DOC TRK:" << freetrack << ", DUR:" << m_document->renderer()->mltTrackDuration(freetrack);
            if (m_document->trackInfoAt(freetrack - 1).type == AUDIOTRACK && !m_document->trackInfoAt(freetrack - 1).isLocked) {
                //kDebug() << "// CHK DOC TRK:" << freetrack << ", DUR:" << m_document->renderer()->mltTrackDuration(freetrack);
                if (m_document->renderer()->mltTrackDuration(freetrack) < start || m_document->renderer()->mltGetSpaceLength(pos, freetrack, false) >= clip->cropDuration().frames(m_document->fps())) {
                    //kDebug() << "FOUND SPACE ON TRK: " << freetrack;
                    break;
                }
            }
        }
        kDebug() << "GOT TRK: " << track;
        if (freetrack == 0) {
            emit displayMessage(i18n("No empty space to put clip audio"), ErrorMessage);
        } else {
            ItemInfo info = clip->info();
            info.track = m_document->tracksCount() - freetrack;
            addClip(clip->xml(), clip->clipProducer(), info, clip->effectList());
            scene()->clearSelection();
            clip->setSelected(true);
            ClipItem *audioClip = getClipItemAt(start, info.track);
            if (audioClip) {
                Mlt::Tractor *tractor = m_document->renderer()->lockService();
                clip->setVideoOnly(true);
                if (m_document->renderer()->mltUpdateClipProducer(tractor, m_document->tracksCount() - track, start, clip->baseClip()->videoProducer()) == false) {
                    emit displayMessage(i18n("Cannot update clip (time: %1, track: %2)", start, track), ErrorMessage);
                }
                if (m_document->renderer()->mltUpdateClipProducer(tractor, m_document->tracksCount() - info.track, start, clip->baseClip()->audioProducer(info.track)) == false) {
                    emit displayMessage(i18n("Cannot update clip (time: %1, track: %2)", start, info.track), ErrorMessage);
                }
                m_document->renderer()->unlockService(tractor);
                audioClip->setSelected(true);
                audioClip->setAudioOnly(true);

                // keep video effects, move audio effects to audio clip
                int videoIx = 0;
                int audioIx = 0;
                for (int i = 0; i < effects.count(); ++i) {
                    if (effects.at(i).attribute("type") == "audio") {
                        deleteEffect(m_document->tracksCount() - track, pos, clip->effect(videoIx));
                        audioIx++;
                    } else {
                        deleteEffect(freetrack, pos, audioClip->effect(audioIx));
                        videoIx++;
                    }
                }

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
                Mlt::Tractor *tractor = m_document->renderer()->lockService();
                if (!m_document->renderer()->mltUpdateClipProducer(
                            tractor,
                            m_document->tracksCount() - info.track,
                            info.startPos.frames(m_document->fps()),
                            clip->baseClip()->getProducer(info.track))) {

                    emit displayMessage(i18n("Cannot update clip (time: %1, track: %2)",
                                             info.startPos.frames(m_document->fps()), info.track),
                                        ErrorMessage);
                }
                m_document->renderer()->unlockService(tractor);

                // re-add audio effects
                for (int i = 0; i < effects.count(); ++i) {
                    if (effects.at(i).attribute("type") == "audio") {
                        addEffect(m_document->tracksCount() - track, pos, effects.at(i));
                    }
                }

                break;
            }
        }
        clip->setFlag(QGraphicsItem::ItemIsMovable, true);
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
    Mlt::Tractor *tractor = m_document->renderer()->lockService();
    if (videoOnly) {
        int start = pos.frames(m_document->fps());
        clip->setVideoOnly(true);
        clip->setAudioOnly(false);
        if (m_document->renderer()->mltUpdateClipProducer(tractor, m_document->tracksCount() - track, start, clip->baseClip()->videoProducer()) == false) {
            emit displayMessage(i18n("Cannot update clip (time: %1, track: %2)", start, track), ErrorMessage);
        }
    } else if (audioOnly) {
        int start = pos.frames(m_document->fps());
        clip->setAudioOnly(true);
        clip->setVideoOnly(false);
        if (m_document->renderer()->mltUpdateClipProducer(tractor, m_document->tracksCount() - track, start, clip->baseClip()->audioProducer(track)) == false) {
            emit displayMessage(i18n("Cannot update clip (time: %1, track: %2)", start, track), ErrorMessage);
        }
    } else {
        int start = pos.frames(m_document->fps());
        clip->setAudioOnly(false);
        clip->setVideoOnly(false);
        if (m_document->renderer()->mltUpdateClipProducer(tractor, m_document->tracksCount() - track, start, clip->baseClip()->getProducer(track)) == false) {
            emit displayMessage(i18n("Cannot update clip (time: %1, track: %2)", start, track), ErrorMessage);
        }
    }
    m_document->renderer()->unlockService(tractor);
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

void CustomTrackView::slotGoToMarker(QAction *action)
{
    int pos = action->data().toInt();
    seekCursorPos(pos);
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

double CustomTrackView::fps() const
{
    return m_document->fps();
}

void CustomTrackView::updateProjectFps()
{
    // update all clips to the new fps
    resetSelectionGroup();
    scene()->clearSelection();
    m_dragItem = NULL;
    QList<QGraphicsItem *> itemList = items();
    for (int i = 0; i < itemList.count(); i++) {
        // remove all items and re-add them one by one
        if (itemList.at(i) != m_cursorLine && itemList.at(i)->parentItem() == NULL) m_scene->removeItem(itemList.at(i));
    }
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->parentItem() == 0 && (itemList.at(i)->type() == AVWIDGET || itemList.at(i)->type() == TRANSITIONWIDGET)) {
            AbstractClipItem *clip = static_cast <AbstractClipItem *>(itemList.at(i));
            clip->updateFps(m_document->fps());
            m_scene->addItem(clip);
        } else if (itemList.at(i)->type() == GROUPWIDGET) {
            AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(itemList.at(i));
            QList<QGraphicsItem *> children = grp->childItems();
            for (int j = 0; j < children.count(); j++) {
                if (children.at(j)->type() == AVWIDGET || children.at(j)->type() == TRANSITIONWIDGET) {
                    AbstractClipItem *clip = static_cast <AbstractClipItem *>(children.at(j));
                    clip->setFlag(QGraphicsItem::ItemIsMovable, true);
                    clip->updateFps(m_document->fps());
                }
            }
            m_document->clipManager()->removeGroup(grp);
            m_scene->addItem(grp);
            scene()->destroyItemGroup(grp);
            scene()->clearSelection();
            for (int j = 0; j < children.count(); j++) {
                if (children.at(j)->type() == AVWIDGET || children.at(j)->type() == TRANSITIONWIDGET) {
                    //children.at(j)->setParentItem(0);
                    children.at(j)->setSelected(true);
                }
            }
            groupSelectedItems(true, true);
        } else if (itemList.at(i)->type() == GUIDEITEM) {
            Guide *g = static_cast<Guide *>(itemList.at(i));
            g->updatePos();
            m_scene->addItem(g);
        }
    }
    viewport()->update();
}

void CustomTrackView::slotTrackDown()
{
    if (m_selectedTrack > m_document->tracksCount() - 2) m_selectedTrack = 0;
    else m_selectedTrack++;
    emit updateTrackHeaders();
    QRectF rect(mapToScene(QPoint(10, 0)).x(), m_selectedTrack * m_tracksHeight, 10, m_tracksHeight);
    ensureVisible(rect, 0, 0);
    viewport()->update();
}

void CustomTrackView::slotTrackUp()
{
    if (m_selectedTrack > 0) m_selectedTrack--;
    else m_selectedTrack = m_document->tracksCount() - 1;
    emit updateTrackHeaders();
    QRectF rect(mapToScene(QPoint(10, 0)).x(), m_selectedTrack * m_tracksHeight, 10, m_tracksHeight);
    ensureVisible(rect, 0, 0);
    viewport()->update();
}

int CustomTrackView::selectedTrack() const
{
    return m_selectedTrack;
}

QStringList CustomTrackView::selectedClips() const
{
    QStringList clipIds;
    QList<QGraphicsItem *> selection = m_scene->selectedItems();
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == AVWIDGET) {
            ClipItem *item = (ClipItem *)selection.at(i);
            clipIds << item->clipProducer();
        }
    }
    return clipIds;
}

QList<ClipItem *> CustomTrackView::selectedClipItems() const
{
    QList<ClipItem *> clips;
    QList<QGraphicsItem *> selection = m_scene->selectedItems();
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == AVWIDGET) {
            clips.append((ClipItem *)selection.at(i));
        }
    }
    return clips;
}

void CustomTrackView::slotSelectTrack(int ix)
{
    m_selectedTrack = qMax(0, ix);
    m_selectedTrack = qMin(ix, m_document->tracksCount() - 1);
    emit updateTrackHeaders();
    QRectF rect(mapToScene(QPoint(10, 0)).x(), m_selectedTrack * m_tracksHeight, 10, m_tracksHeight);
    ensureVisible(rect, 0, 0);
    viewport()->update();
}

void CustomTrackView::slotSelectClipsInTrack()
{
    QRectF rect(0, m_selectedTrack * m_tracksHeight + m_tracksHeight / 2, sceneRect().width(), m_tracksHeight / 2 - 1);
    QList<QGraphicsItem *> selection = m_scene->items(rect);
    m_scene->clearSelection();
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == AVWIDGET || selection.at(i)->type() == TRANSITIONWIDGET || selection.at(i)->type() == GROUPWIDGET) {
            selection.at(i)->setSelected(true);
        }
    }    
    resetSelectionGroup();
    groupSelectedItems();
}

void CustomTrackView::slotSelectAllClips()
{
    QList<QGraphicsItem *> selection = m_scene->items();
    m_scene->clearSelection();
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == AVWIDGET || selection.at(i)->type() == TRANSITIONWIDGET  || selection.at(i)->type() == GROUPWIDGET) {
            selection.at(i)->setSelected(true);
        }
    }
    resetSelectionGroup();
    groupSelectedItems();
}

void CustomTrackView::selectClip(bool add, bool group, int track, int pos)
{
    QRectF rect;
    if (track != -1 && pos != -1)
        rect = QRectF(pos, track * m_tracksHeight + m_tracksHeight / 2, 1, 1);
    else
        rect = QRectF(m_cursorPos, m_selectedTrack * m_tracksHeight + m_tracksHeight / 2, 1, 1);
    QList<QGraphicsItem *> selection = m_scene->items(rect);
    resetSelectionGroup(group);
    if (!group) m_scene->clearSelection();
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == AVWIDGET) {
            selection.at(i)->setSelected(add);
            break;
        }
    }
    if (group) groupSelectedItems();
}

void CustomTrackView::selectTransition(bool add, bool group)
{
    QRectF rect(m_cursorPos, m_selectedTrack * m_tracksHeight + m_tracksHeight, 1, 1);
    QList<QGraphicsItem *> selection = m_scene->items(rect);
    resetSelectionGroup(group);
    if (!group) m_scene->clearSelection();
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == TRANSITIONWIDGET) {
            selection.at(i)->setSelected(add);
            break;
        }
    }
    if (group) groupSelectedItems();
}

QStringList CustomTrackView::extractTransitionsLumas()
{
    QStringList urls;
    QList<QGraphicsItem *> itemList = items();
    Transition *transitionitem;
    QDomElement transitionXml;
    for (int i = 0; i < itemList.count(); i++) {
        if (itemList.at(i)->type() == TRANSITIONWIDGET) {
            transitionitem = static_cast <Transition*>(itemList.at(i));
            transitionXml = transitionitem->toXML();
            // luma files in transitions can be in "resource" or "luma" property
            QString luma = EffectsList::parameter(transitionXml, "luma");
            if (luma.isEmpty()) luma = EffectsList::parameter(transitionXml, "resource");
            if (!luma.isEmpty()) urls << KUrl(luma).path();
        }
    }
    urls.removeDuplicates();
    return urls;
}

void CustomTrackView::setEditMode(EDITMODE mode)
{
    m_scene->setEditMode(mode);
}

void CustomTrackView::checkTrackSequence(int track)
{
    QList <int> times = m_document->renderer()->checkTrackSequence(m_document->tracksCount() - track);
    //track = m_document->tracksCount() -track;
    QRectF rect(0, track * m_tracksHeight + m_tracksHeight / 2, sceneRect().width(), 2);
    QList<QGraphicsItem *> selection = m_scene->items(rect);
    QList <int> timelineList;
    timelineList.append(0);
    for (int i = 0; i < selection.count(); i++) {
        if (selection.at(i)->type() == AVWIDGET) {
            ClipItem *clip = static_cast <ClipItem *>(selection.at(i));
            int start = clip->startPos().frames(m_document->fps());
            int end = clip->endPos().frames(m_document->fps());
            if (!timelineList.contains(start)) timelineList.append(start);
            if (!timelineList.contains(end)) timelineList.append(end);
        }
    }
    qSort(timelineList);
    kDebug() << "// COMPARE:\n" << times << "\n" << timelineList << "\n-------------------";
    if (times != timelineList) KMessageBox::sorry(this, i18n("error"), i18n("TRACTOR"));
}

void CustomTrackView::insertZoneOverwrite(QStringList data, int in)
{
    if (data.isEmpty()) return;
    DocClipBase *clip = m_document->getBaseClip(data.at(0));
    ItemInfo info;
    info.startPos = GenTime(in, m_document->fps());
    info.cropStart = GenTime(data.at(1).toInt(), m_document->fps());
    info.endPos = info.startPos + GenTime(data.at(2).toInt(), m_document->fps()) - info.cropStart;
    info.cropDuration = info.endPos - info.startPos;
    info.track = m_selectedTrack;
    QUndoCommand *addCommand = new QUndoCommand();
    addCommand->setText(i18n("Insert clip"));
    adjustTimelineClips(OVERWRITEEDIT, NULL, info, addCommand);
    new AddTimelineClipCommand(this, clip->toXML(), clip->getId(), info, EffectsList(), true, false, true, false, addCommand);
    updateTrackDuration(info.track, addCommand);
    m_commandStack->push(addCommand);

    selectClip(true, false, m_selectedTrack, in);
    // Automatic audio split
    if (KdenliveSettings::splitaudio())
        splitAudio();
}

void CustomTrackView::clearSelection(bool emitInfo)
{
    resetSelectionGroup();
    scene()->clearSelection();
    m_dragItem = NULL;
    if (emitInfo) emit clipItemSelected(NULL);
}

void CustomTrackView::updatePalette()
{
    m_activeTrackBrush = KStatefulBrush(KColorScheme::View, KColorScheme::ActiveBackground, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    if (m_cursorLine) {
        QPen pen1 = QPen();
        pen1.setWidth(1);
        pen1.setColor(palette().text().color());
        m_cursorLine->setPen(pen1);
    }
}

void CustomTrackView::removeTipAnimation()
{
    if (m_visualTip) {
        scene()->removeItem(m_visualTip);
        m_animationTimer->stop();
        delete m_animation;
        m_animation = NULL;
        delete m_visualTip;
        m_visualTip = NULL;
    }
}

void CustomTrackView::setTipAnimation(AbstractClipItem *clip, OPERATIONTYPE mode, const double size)
{
    if (m_visualTip == NULL) {
        QRectF rect = clip->sceneBoundingRect();
        m_animation = new QGraphicsItemAnimation;
        m_animation->setTimeLine(m_animationTimer);
        m_animation->setScaleAt(1, 1, 1);
        QPolygon polygon;
        switch (mode) {
        case FADEIN:
        case FADEOUT:
            m_visualTip = new QGraphicsEllipseItem(-size, -size, size * 2, size * 2);
            ((QGraphicsEllipseItem*) m_visualTip)->setBrush(m_tipColor);
            ((QGraphicsEllipseItem*) m_visualTip)->setPen(m_tipPen);
            if (mode == FADEIN)
                m_visualTip->setPos(rect.x() + ((ClipItem *) clip)->fadeIn(), rect.y());
            else
                m_visualTip->setPos(rect.right() - ((ClipItem *) clip)->fadeOut(), rect.y());

            m_animation->setScaleAt(.5, 2, 2);
            break;
        case RESIZESTART:
        case RESIZEEND:
            polygon << QPoint(0, - size * 2);
            if (mode == RESIZESTART)
                polygon << QPoint(size * 2, 0);
            else
                polygon << QPoint(- size * 2, 0);
            polygon << QPoint(0, size * 2);
            polygon << QPoint(0, - size * 2);

            m_visualTip = new QGraphicsPolygonItem(polygon);
            ((QGraphicsPolygonItem*) m_visualTip)->setBrush(m_tipColor);
            ((QGraphicsPolygonItem*) m_visualTip)->setPen(m_tipPen);
            if (mode == RESIZESTART)
                m_visualTip->setPos(rect.x(), rect.y() + rect.height() / 2);
            else
                m_visualTip->setPos(rect.right(), rect.y() + rect.height() / 2);

            m_animation->setScaleAt(.5, 2, 1);
            break;
        case TRANSITIONSTART:
        case TRANSITIONEND:
            polygon << QPoint(0, - size * 2);
            if (mode == TRANSITIONSTART)
                polygon << QPoint(size * 2, 0);
            else
                polygon << QPoint(- size * 2, 0);
            polygon << QPoint(0, 0);
            polygon << QPoint(0, - size * 2);

            m_visualTip = new QGraphicsPolygonItem(polygon);
            ((QGraphicsPolygonItem*) m_visualTip)->setBrush(m_tipColor);
            ((QGraphicsPolygonItem*) m_visualTip)->setPen(m_tipPen);
            if (mode == TRANSITIONSTART)
                m_visualTip->setPos(rect.x(), rect.bottom());
            else
                m_visualTip->setPos(rect.right(), rect.bottom());

            m_animation->setScaleAt(.5, 2, 2);
            break;
        default:
            delete m_animation;
            return;
        }

        m_visualTip->setFlags(QGraphicsItem::ItemIgnoresTransformations);
        m_visualTip->setZValue(100);
        scene()->addItem(m_visualTip);
        m_animation->setItem(m_visualTip);
        m_animationTimer->start();
    }
}

bool CustomTrackView::hasAudio(int track) const
{
    QRectF rect(0, (double)(track * m_tracksHeight + 1), (double) sceneRect().width(), (double)(m_tracksHeight - 1));
    QList<QGraphicsItem *> collisions = scene()->items(rect, Qt::IntersectsItemBoundingRect);
    QGraphicsItem *item;
    for (int i = 0; i < collisions.count(); i++) {
        item = collisions.at(i);
        if (!item->isEnabled()) continue;
        if (item->type() == AVWIDGET) {
            ClipItem *clip = static_cast <ClipItem *>(item);
            if (!clip->isVideoOnly() && (clip->clipType() == AUDIO || clip->clipType() == AV || clip->clipType() == PLAYLIST)) return true;
        }
    }
    return false;
}

void CustomTrackView::slotAddTrackEffect(const QDomElement &effect, int ix)
{
    
    QUndoCommand *effectCommand = new QUndoCommand();
    QString effectName;
    if (effect.tagName() == "effectgroup") {
        effectName = effect.attribute("name");
    } else {
        QDomElement namenode = effect.firstChildElement("name");
        if (!namenode.isNull()) effectName = i18n(namenode.text().toUtf8().data());
        else effectName = i18n("effect");
    }
    effectCommand->setText(i18n("Add %1", effectName));
    if (effect.tagName() == "effectgroup") {
        QDomNodeList effectlist = effect.elementsByTagName("effect");
        for (int j = 0; j < effectlist.count(); j++) {
            QDomElement trackeffect = effectlist.at(j).toElement();
            if (trackeffect.attribute("unique", "0") != "0" && m_document->hasTrackEffect(m_document->tracksCount() - ix - 1, trackeffect.attribute("tag"), trackeffect.attribute("id")) != -1) {
                emit displayMessage(i18n("Effect already present in track"), ErrorMessage);
                continue;
            }
            new AddEffectCommand(this, m_document->tracksCount() - ix, GenTime(-1), trackeffect, true, effectCommand);
        }
    }
    else {
        if (effect.attribute("unique", "0") != "0" && m_document->hasTrackEffect(m_document->tracksCount() - ix - 1, effect.attribute("tag"), effect.attribute("id")) != -1) {
            emit displayMessage(i18n("Effect already present in track"), ErrorMessage);
            delete effectCommand;
            return;
        }
        new AddEffectCommand(this, m_document->tracksCount() - ix, GenTime(-1), effect, true, effectCommand);
    }

    if (effectCommand->childCount() > 0) {
        m_commandStack->push(effectCommand);
        setDocumentModified();
    }
    else delete effectCommand;
}


EffectsParameterList CustomTrackView::getEffectArgs(const QDomElement &effect)
{
    EffectsParameterList parameters;
    QLocale locale;
    parameters.addParam("tag", effect.attribute("tag"));
    //if (effect.hasAttribute("region")) parameters.addParam("region", effect.attribute("region"));
    parameters.addParam("kdenlive_ix", effect.attribute("kdenlive_ix"));
    parameters.addParam("kdenlive_info", effect.attribute("kdenlive_info"));
    parameters.addParam("id", effect.attribute("id"));
    if (effect.hasAttribute("src")) parameters.addParam("src", effect.attribute("src"));
    if (effect.hasAttribute("disable")) parameters.addParam("disable", effect.attribute("disable"));
    if (effect.hasAttribute("in")) parameters.addParam("in", effect.attribute("in"));
    if (effect.hasAttribute("out")) parameters.addParam("out", effect.attribute("out"));
    if (effect.attribute("id") == "region") {
	QDomNodeList subeffects = effect.elementsByTagName("effect");
	for (int i = 0; i < subeffects.count(); i++) {
	    QDomElement subeffect = subeffects.at(i).toElement();
	    int subeffectix = subeffect.attribute("region_ix").toInt();
	    parameters.addParam(QString("filter%1").arg(subeffectix), subeffect.attribute("id"));
	    parameters.addParam(QString("filter%1.tag").arg(subeffectix), subeffect.attribute("tag"));
	    parameters.addParam(QString("filter%1.kdenlive_info").arg(subeffectix), subeffect.attribute("kdenlive_info"));
	    QDomNodeList subparams = subeffect.elementsByTagName("parameter");
	    adjustEffectParameters(parameters, subparams, QString("filter%1.").arg(subeffectix));
	}
    }

    QDomNodeList params = effect.elementsByTagName("parameter");
    adjustEffectParameters(parameters, params);
    
    return parameters;
}


void CustomTrackView::adjustEffectParameters(EffectsParameterList &parameters, QDomNodeList params, const QString &prefix)
{
  QLocale locale;
  for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
	QString paramname = prefix + e.attribute("name");
        if (e.attribute("type") == "geometry" && !e.hasAttribute("fixed")) {
            // effects with geometry param need in / out synced with the clip, request it...
            parameters.addParam("_sync_in_out", "1");
        }
        if (e.attribute("type") == "simplekeyframe") {
            QStringList values = e.attribute("keyframes").split(';', QString::SkipEmptyParts);
            double factor = e.attribute("factor", "1").toDouble();
            double offset = e.attribute("offset", "0").toDouble();
            for (int j = 0; j < values.count(); j++) {
                QString pos = values.at(j).section(':', 0, 0);
                double val = (values.at(j).section(':', 1, 1).toDouble() - offset) / factor;
                values[j] = pos + '=' + locale.toString(val);
            }
            // kDebug() << "/ / / /SENDING KEYFR:" << values;
            parameters.addParam(paramname, values.join(";"));
            /*parameters.addParam(e.attribute("name"), e.attribute("keyframes").replace(":", "="));
            parameters.addParam("max", e.attribute("max"));
            parameters.addParam("min", e.attribute("min"));
            parameters.addParam("factor", e.attribute("factor", "1"));*/
        } else if (e.attribute("type") == "keyframe") {
            kDebug() << "/ / / /SENDING KEYFR EFFECT TYPE";
            parameters.addParam("keyframes", e.attribute("keyframes"));
            parameters.addParam("max", e.attribute("max"));
            parameters.addParam("min", e.attribute("min"));
            parameters.addParam("factor", e.attribute("factor", "1"));
            parameters.addParam("offset", e.attribute("offset", "0"));
            parameters.addParam("starttag", e.attribute("starttag", "start"));
            parameters.addParam("endtag", e.attribute("endtag", "end"));
        } else if (e.attribute("namedesc").contains(';')) {
            QString format = e.attribute("format");
            QStringList separators = format.split("%d", QString::SkipEmptyParts);
            QStringList values = e.attribute("value").split(QRegExp("[,:;x]"));
            QString neu;
            QTextStream txtNeu(&neu);
            if (values.size() > 0)
                txtNeu << (int)values[0].toDouble();
            for (int i = 0; i < separators.size() && i + 1 < values.size(); i++) {
                txtNeu << separators[i];
                txtNeu << (int)(values[i+1].toDouble());
            }
            parameters.addParam("start", neu);
        } else {
            if (e.attribute("factor", "1") != "1" || e.attribute("offset", "0") != "0") {
                double fact;
                if (e.attribute("factor").contains('%')) {
                    fact = ProfilesDialog::getStringEval(m_document->mltProfile(), e.attribute("factor"));
                } else {
                    fact = e.attribute("factor", "1").toDouble();
                }
                double offset = e.attribute("offset", "0").toDouble();
                parameters.addParam(paramname, locale.toString((e.attribute("value").toDouble() - offset) / fact));
            } else {
                parameters.addParam(paramname, e.attribute("value"));
            }
        }
    }
}


void CustomTrackView::updateTrackNames(int track, bool added)
{
    QList <TrackInfo> tracks = m_document->tracksList();
    int max = tracks.count();
    int docTrack = max - track - 1;

    // count number of tracks of each type
    int videoTracks = 0;
    int audioTracks = 0;
    for (int i = max - 1; i >= 0; --i) {
        TrackInfo info = tracks.at(i);
        if (info.type == VIDEOTRACK)
            videoTracks++;
        else
            audioTracks++;

        if (i <= docTrack) {
            QString type = (info.type == VIDEOTRACK ? "Video " : "Audio ");
            int typeNumber = (info.type == VIDEOTRACK ? videoTracks : audioTracks);

            if (added) {
                if (i == docTrack || info.trackName == type + QString::number(typeNumber - 1)) {
                    info.trackName = type + QString::number(typeNumber);
                    m_document->setTrackType(i, info);
                }
            } else {
                if (info.trackName == type + QString::number(typeNumber + 1)) {
                    info.trackName = type + QString::number(typeNumber);
                    m_document->setTrackType(i, info);
                }
            }
        }
    }
    emit tracksChanged();
}

void CustomTrackView::updateTrackDuration(int track, QUndoCommand *command)
{
    Q_UNUSED(command)

    QList<int> tracks;
    if (track >= 0) {
        tracks << m_document->tracksCount() - track - 1;
    } else {
        // negative track number -> update all tracks
        for (int i = 0; i < m_document->tracksCount(); ++i)
            tracks << i;
    }
    int t, duration;
    for (int i = 0; i < tracks.count(); ++i) {
        t = tracks.at(i);
        // t + 1 because of black background track
        duration = m_document->renderer()->mltTrackDuration(t + 1);
        if (duration != m_document->trackDuration(t)) {
            m_document->setTrackDuration(t, duration);

            // update effects
            EffectsList effects = m_document->getTrackEffects(t);
            for (int j = 0; j < effects.count(); ++j) {
                /* TODO
                 * - lookout for keyframable parameters and update them so all keyframes are in the new range (0 - duration)
                 * - update the effectstack if necessary
                 */
            }
        }
    }
}

void CustomTrackView::slotRefreshThumbs(const QString &id, bool resetThumbs)
{
    QList<QGraphicsItem *> list = scene()->items();
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->type() == AVWIDGET) {
            clip = static_cast <ClipItem *>(list.at(i));
            if (clip->clipProducer() == id) {
                clip->refreshClip(true, resetThumbs);
            }
        }
    }
}

void CustomTrackView::adjustEffects(ClipItem* item, ItemInfo oldInfo, QUndoCommand* command)
{
    QMap<int, QDomElement> effects = item->adjustEffectsToDuration(m_document->width(), m_document->height(), oldInfo);

    if (effects.count()) {
        QMap<int, QDomElement>::const_iterator i = effects.constBegin();
        while (i != effects.constEnd()) {
            new EditEffectCommand(this, m_document->tracksCount() - item->track(), item->startPos(), i.value(), item->effect(i.key()), i.value().attribute("kdenlive_ix").toInt(), true, true, command);
            ++i;
        }
    }
}


void CustomTrackView::slotGotFilterJobResults(const QString &/*id*/, int startPos, int track, const QString &filter, stringMap filterParams)
{
    ClipItem *clip = getClipItemAt(GenTime(startPos, m_document->fps()), track);
    if (clip == NULL) {
        emit displayMessage(i18n("Cannot find clip for effect update %1.", filter), ErrorMessage);
        return;
    }
    QDomElement newEffect;
    QDomElement effect = clip->getEffectAtIndex(clip->selectedEffectIndex());
    if (effect.attribute("id") == filter) {
        newEffect = effect.cloneNode().toElement();
        QMap<QString, QString>::const_iterator i = filterParams.constBegin();
        while (i != filterParams.constEnd()) {
            EffectsList::setParameter(newEffect, i.key(), i.value());
            kDebug()<<"// RESULT FILTER: "<<i.key()<<"="<< i.value();
            ++i;
        }
        EditEffectCommand *command = new EditEffectCommand(this, m_document->tracksCount() - clip->track(), clip->startPos(), effect, newEffect, clip->selectedEffectIndex(), true, true);
        m_commandStack->push(command);
        emit clipItemSelected(clip);
    }
    
}


