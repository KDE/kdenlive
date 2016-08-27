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
#include "timeline.h"
#include "track.h"
#include "clipitem.h"
#include "timelinecommands.h"
#include "transition.h"
#include "markerdialog.h"
#include "clipdurationdialog.h"
#include "abstractgroupitem.h"
#include "spacerdialog.h"
#include "trackdialog.h"
#include "tracksconfigdialog.h"
#include "mltcontroller/clipcontroller.h"
#include "mltcontroller/effectscontroller.h"
#include "definitions.h"
#include "kdenlivesettings.h"
#include "renderer.h"
#include "bin/projectclip.h"
#include "mainwindow.h"
#include "transitionhandler.h"
#include "project/clipmanager.h"
#include "utils/KoIconUtils.h"
#include "effectslist/initeffects.h"
#include "effectstack/widgets/keyframeimport.h"
#include "dialogs/profilesdialog.h"
#include "managers/guidemanager.h"
#include "managers/razormanager.h"
#include "managers/selectmanager.h"
#include "ui_keyframedialog_ui.h"
#include "ui_addtrack_ui.h"
#include "managers/trimmanager.h"
#include "managers/spacermanager.h"
#include "managers/movemanager.h"
#include "managers/resizemanager.h"
#include "lib/audio/audioEnvelope.h"
#include "lib/audio/audioCorrelation.h"

#include <QDebug>
#include <klocalizedstring.h>
#include <KCursor>
#include <KMessageBox>
#include <KRecentDirs>

#include <QUrl>
#include <QIcon>
#include <QMouseEvent>
#include <QGraphicsItem>
#include <QScrollBar>
#include <QApplication>
#include <QMimeData>

#include <QGraphicsDropShadowEffect>

#define SEEK_INACTIVE (-1)
//#define DEBUG

bool sortGuidesList(const Guide *g1 , const Guide *g2)
{
    return (*g1).position() < (*g2).position();
}

CustomTrackView::CustomTrackView(KdenliveDoc *doc, Timeline *timeline, CustomTrackScene* projectscene, QWidget *parent) :
    QGraphicsView(projectscene, parent)
  , m_tracksHeight(KdenliveSettings::trackheight())
  , m_projectDuration(0)
  , m_cursorPos(0)
  , m_cursorOffset(0)
  , m_document(doc)
  , m_timeline(timeline)
  , m_scene(projectscene)
  , m_cursorLine(NULL)
  , m_cutLine(NULL)
  , m_operationMode(None)
  , m_moveOpMode(None)
  , m_dragItem(NULL)
  , m_dragGuide(NULL)
  , m_visualTip(NULL)
  , m_keyProperties(NULL)
  , m_autoScroll(KdenliveSettings::autoscroll())
  , m_timelineContextMenu(NULL)
  , m_timelineContextClipMenu(NULL)
  , m_timelineContextTransitionMenu(NULL)
  , m_timelineContextKeyframeMenu(NULL)
  , m_selectKeyframeType(NULL)
  , m_markerMenu(NULL)
  , m_autoTransition(NULL)
  , m_pasteEffectsAction(NULL)
  , m_ungroupAction(NULL)
  , m_editGuide(NULL)
  , m_deleteGuide(NULL)
  , m_clipTypeGroup(NULL)
  , m_scrollOffset(0)
  , m_clipDrag(false)
  , m_findIndex(0)
  , m_tool(SelectTool)
  , m_copiedItems()
  , m_menuPosition()
  , m_selectionGroup(NULL)
  , m_selectedTrack(1)
  , m_audioCorrelator(NULL)
  , m_audioAlignmentReference(NULL)
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
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    setContentsMargins(0, 0, 0, 0);
    KColorScheme scheme(palette().currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    m_selectedTrackColor = scheme.background(KColorScheme::ActiveBackground ).color();
    m_selectedTrackColor.setAlpha(150);

    m_lockedTrackColor = scheme.background(KColorScheme::NegativeBackground ).color();
    m_lockedTrackColor.setAlpha(150);

    m_keyPropertiesTimer = new QTimeLine(800);
    m_keyPropertiesTimer->setFrameRange(0, 5);
    m_keyPropertiesTimer->setUpdateInterval(100);
    m_keyPropertiesTimer->setLoopCount(0);

    m_tipColor = QColor(0, 192, 0, 200);
    m_tipPen.setColor(QColor(255, 255, 255, 100));
    m_tipPen.setWidth(3);

    setSceneRect(0, 0, sceneRect().width(), m_tracksHeight);
    verticalScrollBar()->setMaximum(m_tracksHeight);
    verticalScrollBar()->setTracking(true);
    // repaint guides when using vertical scroll
    connect(verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(slotRefreshGuides()));
    connect(horizontalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(slotRefreshCutLine()));

    m_cursorLine = projectscene->addLine(0, 0, 0, m_tracksHeight);
    m_cursorLine->setZValue(1000);
    QPen pen1 = QPen();
    pen1.setWidth(1);
    QColor line(palette().text().color());
    line.setAlpha(100);
    pen1.setColor(line);
    m_cursorLine->setPen(pen1);

    connect(&m_scrollTimer, SIGNAL(timeout()), this, SLOT(slotCheckMouseScrolling()));
    m_scrollTimer.setInterval(100);
    m_scrollTimer.setSingleShot(true);

    QIcon razorIcon = KoIconUtils::themedIcon(QStringLiteral("edit-cut"));
    m_razorCursor = QCursor(razorIcon.pixmap(32, 32));
    connect(m_document->renderer(), SIGNAL(prepareTimelineReplacement(QString)), this, SLOT(slotPrepareTimelineReplacement(QString)), Qt::DirectConnection);
    connect(m_document->renderer(), SIGNAL(replaceTimelineProducer(QString)), this, SLOT(slotReplaceTimelineProducer(QString)), Qt::DirectConnection);
    connect(m_document->renderer(), SIGNAL(updateTimelineProducer(QString)), this, SLOT(slotUpdateTimelineProducer(QString)));
    connect(m_document->renderer(), SIGNAL(rendererPosition(int)), this, SLOT(setCursorPos(int)));
    scale(1, 1);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_disableClipAction = new QAction(QIcon::fromTheme(QStringLiteral("visibility")), i18n("Disable Clip"), this);
    connect(m_disableClipAction, &QAction::triggered, this, &CustomTrackView::disableClip);
    m_disableClipAction->setCheckable(true);
    m_document->doAddAction(QStringLiteral("clip_disabled"), m_disableClipAction, QKeySequence());
    QAction *pasteAction = m_document->getAction(KStandardAction::name(KStandardAction::Paste));
    if (pasteAction)
        pasteAction->setEnabled(false);
}

CustomTrackView::~CustomTrackView()
{
    qDeleteAll(m_toolManagers);
    qDeleteAll(m_guides);
    m_guides.clear();
    delete m_keyPropertiesTimer;
}

void CustomTrackView::initTools()
{
    m_toolManagers.insert(TrimType, new TrimManager(this, m_commandStack));
    m_toolManagers.insert(SpacerType, new SpacerManager(this, m_commandStack));
    m_toolManagers.insert(ResizeType, new ResizeManager(this, m_commandStack));
    m_toolManagers.insert(RazorType, new RazorManager(this, m_commandStack));
    m_toolManagers.insert(MoveType, new MoveManager(m_timeline->transitionHandler, this, m_commandStack));
    m_toolManagers.insert(SelectType, new SelectManager(this, m_commandStack));
    m_toolManagers.insert(GuideType, new GuideManager(this, m_commandStack));
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
    m_clipTypeGroup = clipTypeGroup;
    m_timelineContextMenu = timeline;
    m_timelineContextClipMenu = clip;
    m_timelineContextTransitionMenu = transition;
    m_timelineContextClipMenu->addAction(m_disableClipAction);
    connect(m_timelineContextTransitionMenu, SIGNAL(aboutToHide()), this, SLOT(slotResetMenuPosition()));
    connect(m_timelineContextMenu, SIGNAL(aboutToHide()), this, SLOT(slotResetMenuPosition()));
    connect(m_timelineContextClipMenu, SIGNAL(aboutToHide()), this, SLOT(slotResetMenuPosition()));
    connect(m_timelineContextTransitionMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotContextMenuActivated()));
    connect(m_timelineContextMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotContextMenuActivated()));
    connect(m_timelineContextClipMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotContextMenuActivated()));

    m_markerMenu = new QMenu(i18n("Go to marker..."), this);
    m_markerMenu->setEnabled(false);
    markermenu->addMenu(m_markerMenu);
    connect(m_markerMenu, SIGNAL(triggered(QAction*)), this, SLOT(slotGoToMarker(QAction*)));
    QList <QAction *> list = m_timelineContextClipMenu->actions();
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->data().toString() == QLatin1String("paste_effects")) m_pasteEffectsAction = list.at(i);
        else if (list.at(i)->data().toString() == QLatin1String("ungroup_clip")) m_ungroupAction = list.at(i);
        else if (list.at(i)->data().toString() == QLatin1String("A")) m_audioActions.append(list.at(i));
        else if (list.at(i)->data().toString() == QLatin1String("A+V")) m_avActions.append(list.at(i));
    }
    list = m_timelineContextTransitionMenu->actions();
    for (int i = 0; i < list.count(); ++i) {
        if (list.at(i)->data().toString() == QLatin1String("auto")) {
            m_autoTransition = list.at(i);
            break;
        }
    }

    m_timelineContextMenu->addSeparator();
    m_deleteGuide = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete Guide"), this);
    connect(m_deleteGuide, SIGNAL(triggered()), this, SLOT(slotDeleteTimeLineGuide()));
    m_timelineContextMenu->addAction(m_deleteGuide);

    m_editGuide = new QAction(QIcon::fromTheme(QStringLiteral("document-properties")), i18n("Edit Guide"), this);
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

void CustomTrackView::slotContextMenuActivated()
{
    // Menu disappeared, restore default operation mode
    m_operationMode = None;
}

void CustomTrackView::checkAutoScroll()
{
    m_autoScroll = KdenliveSettings::autoscroll();
}

int CustomTrackView::getFrameWidth() const
{
    return (int) (m_tracksHeight * m_document->dar() + 0.5);
}

void CustomTrackView::updateSceneFrameWidth(bool fpsChanged)
{
    int frameWidth = getFrameWidth();
    if (fpsChanged && m_projectDuration > 0) {
        reloadTimeline();
    } else {
        QList<QGraphicsItem *> itemList = items();
        ClipItem *item;
        for (int i = 0; i < itemList.count(); ++i) {
            if (itemList.at(i)->type() == AVWidget) {
                item = static_cast<ClipItem*>(itemList.at(i));
                item->resetFrameWidth(frameWidth);
            }
        }
    }
}

bool CustomTrackView::checkTrackHeight(bool force)
{
    if (!force && m_tracksHeight == KdenliveSettings::trackheight() && sceneRect().height() == m_tracksHeight * m_timeline->visibleTracksCount()) return false;
    int frameWidth = getFrameWidth();
    if (m_tracksHeight != KdenliveSettings::trackheight()) {
        QList<QGraphicsItem *> itemList = items();
        ClipItem *item;
        Transition *transitionitem;
        m_tracksHeight = KdenliveSettings::trackheight();
        // Remove all items, and re-add them one by one to avoid collisions
        for (int i = 0; i < itemList.count(); ++i) {
            if (itemList.at(i)->type() == AVWidget || itemList.at(i)->type() == TransitionWidget) {
                m_scene->removeItem(itemList.at(i));
            }
        }
        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);
        for (int i = 0; i < itemList.count(); ++i) {
            if (itemList.at(i)->type() == AVWidget) {
                item = static_cast<ClipItem*>(itemList.at(i));
                item->setRect(0, 0, item->rect().width(), m_tracksHeight - 1);
                item->setPos((qreal) item->startPos().frames(m_document->fps()), getPositionFromTrack(item->track()) + 1);
                m_scene->addItem(item);
                item->resetFrameWidth(frameWidth);
            } else if (itemList.at(i)->type() == TransitionWidget) {
                transitionitem = static_cast<Transition*>(itemList.at(i));
                transitionitem->setRect(0, 0, transitionitem->rect().width(), m_tracksHeight / 3 * 2 - 1);
                transitionitem->setPos((qreal) transitionitem->startPos().frames(m_document->fps()), getPositionFromTrack(transitionitem->track()) + transitionitem->itemOffset());
                m_scene->addItem(transitionitem);
            }
        }
        KdenliveSettings::setSnaptopoints(snap);
    }
    setSceneRect(0, 0, sceneRect().width(), m_tracksHeight * m_timeline->visibleTracksCount());
    double newHeight = m_tracksHeight * m_timeline->visibleTracksCount() * matrix().m22();
    if (m_cursorLine->flags() & QGraphicsItem::ItemIgnoresTransformations) {
        m_cursorLine->setLine(0, 0, 0, newHeight - 1);
    } else {
        m_cursorLine->setLine(0, 0, 0, m_tracksHeight * m_timeline->visibleTracksCount() - 1);
    }
    if (m_cutLine) {
        m_cutLine->setLine(0, 0, 0, m_tracksHeight * m_scene->scale().y());
    }
    for (int i = 0; i < m_guides.count(); ++i) {
        m_guides.at(i)->setLine(0, 0, 0, newHeight - 1);
    }
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
        if (m_moveOpMode == None || m_moveOpMode == WaitingForConfirm || m_moveOpMode == ZoomTimeline) {
            if (e->delta() > 0) emit zoomIn();
            else emit zoomOut();
        }
    } else if (e->modifiers() == Qt::AltModifier) {
        if (m_moveOpMode == None || m_moveOpMode == WaitingForConfirm || m_moveOpMode == ZoomTimeline) {
            if (e->delta() > 0) slotSeekToNextSnap();
            else slotSeekToPreviousSnap();
        }
    } else {
        if (m_moveOpMode == ResizeStart || m_moveOpMode == ResizeEnd) {
            // Don't allow scg + resizing
            return;
        }
        if (m_operationMode == None || m_operationMode == ZoomTimeline) {
            // Prevent unwanted object move
            m_scene->isZooming = true;
        }
        if (e->delta() <= 0) horizontalScrollBar()->setValue(horizontalScrollBar()->value() + horizontalScrollBar()->singleStep());
        else  horizontalScrollBar()->setValue(horizontalScrollBar()->value() - horizontalScrollBar()->singleStep());
        if (m_operationMode == None || m_operationMode == ZoomTimeline) {
            m_scene->isZooming = false;
        }
    }
}

int CustomTrackView::getPreviousVideoTrack(int track)
{
    int i = track - 1;
    for (; i > 0; i--) {
        if (m_timeline->getTrackInfo(i).type == VideoTrack) break;
    }
    return i;
}

int CustomTrackView::getNextVideoTrack(int track)
{
    for (; track < m_timeline->visibleTracksCount(); track++) {
        if (m_timeline->getTrackInfo(track).type == VideoTrack) break;
    }
    return track;
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
    if (m_moveOpMode != Seek) return;
    if (mapFromScene(m_cursorPos, 0).x() < 3) {
        if (horizontalScrollBar()->value() == 0) return;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - 2);
        QTimer::singleShot(200, this, SLOT(slotCheckPositionScrolling()));
        seekCursorPos(mapToScene(QPoint(-2, 0)).x());
    } else if (viewport()->width() - 3 < mapFromScene(m_cursorPos + 1, 0).x()) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + 2);
        seekCursorPos(mapToScene(QPoint(viewport()->width(), 0)).x() + 1);
        QTimer::singleShot(200, this, SLOT(slotCheckPositionScrolling()));
    }
}

void CustomTrackView::slotAlignPlayheadToMousePos()
{
    /* get curser point ref in screen coord */
    QPoint ps = QCursor::pos();
    /* get xPos in scene coord */
    int mappedXPos = qMax((int)(mapToScene(mapFromGlobal(ps)).x() + 0.5), 0);
    /* move playhead to new xPos*/
    seekCursorPos(mappedXPos);
}

int CustomTrackView::getMousePos() const
{
    return qMax((int)(mapToScene(mapFromGlobal(QCursor::pos())).x() + 0.5), 0);
}

void CustomTrackView::spaceToolMoveToSnapPos(double snappedPos)
{
    // Make sure there is no collision
    QList<QGraphicsItem *> children = m_selectionGroup->childItems();
    QPainterPath shape = m_selectionGroup->clipGroupSpacerShape(QPointF(snappedPos - m_selectionGroup->sceneBoundingRect().left(), 0));
    QList<QGraphicsItem*> collidingItems = scene()->items(shape, Qt::IntersectsItemShape);
    collidingItems.removeAll(m_selectionGroup);
    for (int i = 0; i < children.count(); ++i) {
        if (children.at(i)->type() == GroupWidget) {
            QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
            for (int j = 0; j < subchildren.count(); ++j)
                collidingItems.removeAll(subchildren.at(j));
        }
        collidingItems.removeAll(children.at(i));
    }
    bool collision = false;
    int offset = 0;
    for (int i = 0; i < collidingItems.count(); ++i) {
        if (!collidingItems.at(i)->isEnabled()) continue;
        if (collidingItems.at(i)->type() == AVWidget && snappedPos < m_selectionGroup->sceneBoundingRect().left()) {
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
    shape = m_selectionGroup->clipGroupSpacerShape(QPointF(snappedPos - m_selectionGroup->sceneBoundingRect().left(), 0));
    collidingItems = scene()->items(shape, Qt::IntersectsItemShape);
    collidingItems.removeAll(m_selectionGroup);
    for (int i = 0; i < children.count(); ++i) {
        if (children.at(i)->type() == GroupWidget) {
            QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
            for (int j = 0; j < subchildren.count(); ++j)
                collidingItems.removeAll(subchildren.at(j));
        }
        collidingItems.removeAll(children.at(i));
    }

    for (int i = 0; i < collidingItems.count(); ++i) {
        if (!collidingItems.at(i)->isEnabled()) continue;
        if (collidingItems.at(i)->type() == AVWidget) {
            collision = true;
            break;
        }
    }


    if (!collision) {
        // Check transitions
        shape = m_selectionGroup->transitionGroupShape(QPointF(snappedPos - m_selectionGroup->sceneBoundingRect().left(), 0));
        collidingItems = scene()->items(shape, Qt::IntersectsItemShape);
        collidingItems.removeAll(m_selectionGroup);
        for (int i = 0; i < children.count(); ++i) {
            if (children.at(i)->type() == GroupWidget) {
                QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                for (int j = 0; j < subchildren.count(); ++j)
                    collidingItems.removeAll(subchildren.at(j));
            }
            collidingItems.removeAll(children.at(i));
        }
        offset = 0;

        for (int i = 0; i < collidingItems.count(); ++i) {
            if (collidingItems.at(i)->type() == TransitionWidget && snappedPos < m_selectionGroup->sceneBoundingRect().left()) {
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
        for (int i = 0; i < children.count(); ++i) {
            if (children.at(i)->type() == GroupWidget) {
                QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                for (int j = 0; j < subchildren.count(); ++j)
                    collidingItems.removeAll(subchildren.at(j));
            }
            collidingItems.removeAll(children.at(i));
        }
        for (int i = 0; i < collidingItems.count(); ++i) {
            if (collidingItems.at(i)->type() == TransitionWidget) {
                collision = true;
                break;
            }
        }
    }

    if (!collision)
        m_selectionGroup->setTransform(QTransform::fromTranslate(snappedPos - m_selectionGroup->sceneBoundingRect().left(), 0), true);
}

// virtual
void CustomTrackView::mouseMoveEvent(QMouseEvent * event)
{
    int pos = event->x();
    int mappedXPos = qMax((int)(mapToScene(event->pos()).x()), 0);
    double snappedPos = getSnapPointForPos(mappedXPos);
    emit mousePosition(mappedXPos);

    if (m_cutLine) {
        m_cutLine->setPos(mappedXPos, getPositionFromTrack(getTrackFromPos(mapToScene(event->pos()).y())));
    }
    if (m_moveOpMode == Seek && event->buttons() != Qt::NoButton) {
        QGraphicsView::mouseMoveEvent(event);
        if (mappedXPos != m_document->renderer()->getCurrentSeekPosition() && mappedXPos != cursorPos()) {
            seekCursorPos(mappedXPos);
            slotCheckPositionScrolling();
        }
        return;
    }

    if (m_moveOpMode == ScrollTimeline) {
        QGraphicsView::mouseMoveEvent(event);
        return;
    }
    if (event->buttons() & Qt::MidButton) return;
    if (m_moveOpMode == RubberSelection) {
	QGraphicsView::mouseMoveEvent(event);
        return;
    }

    if (m_moveOpMode == WaitingForConfirm && event->buttons() != Qt::NoButton) {
	bool move = (event->pos() - m_clickEvent).manhattanLength() >= QApplication::startDragDistance();
	if (move) {
	    m_moveOpMode = m_operationMode;
	}
    }
    if (m_moveOpMode == Spacer) {
        // spacer tool
        m_toolManagers.value(SpacerType)->mouseMove(mappedXPos);
        event->accept();
        return;
    }
    if (m_moveOpMode != None && m_moveOpMode != WaitingForConfirm && event->buttons() != Qt::NoButton) {
        if (m_dragItem && m_operationMode != ZoomTimeline) m_clipDrag = true;
        if (m_dragItem && m_tool == SelectTool) {
            if (m_moveOpMode == MoveOperation && m_clipDrag) {
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
            } else if (m_moveOpMode == RollingStart || m_moveOpMode == RollingEnd) {
                m_toolManagers.value(TrimType)->mouseMove(snappedPos);
            } else if (m_moveOpMode == ResizeStart || m_moveOpMode == ResizeEnd) {
                emit pauseMonitor();
                m_toolManagers.value(ResizeType)->mouseMove(snappedPos);
            } else if (m_moveOpMode == FadeIn) {
                static_cast<ClipItem*>(m_dragItem)->setFadeIn(static_cast<int>(mappedXPos - m_dragItem->startPos().frames(m_document->fps())));
            } else if (m_moveOpMode == FadeOut) {
                static_cast<ClipItem*>(m_dragItem)->setFadeOut(static_cast<int>(m_dragItem->endPos().frames(m_document->fps()) - mappedXPos));
            } else if (m_moveOpMode == KeyFrame) {
                GenTime keyFramePos = GenTime(mappedXPos, m_document->fps()) - m_dragItem->startPos();
                double value = m_dragItem->mapFromScene(mapToScene(event->pos()).toPoint()).y();
                m_dragItem->updateKeyFramePos(keyFramePos.frames(fps()), value);
                QString position = m_document->timecode().getDisplayTimecodeFromFrames(m_dragItem->selectedKeyFramePos(), KdenliveSettings::frametimecode());
                emit displayMessage(position + " : " + QString::number(m_dragItem->editedKeyFrameValue()), InformationMessage);
            }
            removeTipAnimation();
            event->accept();
            return;
        } else if (m_moveOpMode == MoveGuide) {
            removeTipAnimation();
            QGraphicsView::mouseMoveEvent(event);
            return;
        }
    }

    QList<QGraphicsItem *> itemList = items(event->pos());
    QGraphicsRectItem *item = NULL;

    bool abort = false;
    GuideManager::checkOperation(itemList, this, event, m_operationMode, abort);
    if (abort) {
        return;
    }

    if (!abort) {
        for (int i = 0; i < itemList.count(); ++i) {
            if (itemList.at(i)->type() == AVWidget || itemList.at(i)->type() == TransitionWidget) {
                item = (QGraphicsRectItem*) itemList.at(i);
                break;
            }
        }
    }
    switch (m_tool) {
        case RazorTool:
            setCursor(m_razorCursor);
            RazorManager::checkOperation(item, this, event, mappedXPos, m_operationMode, abort);
            break;
        case SelectTool:
        default:
            SelectManager::checkOperation(item, this, event, m_selectionGroup, m_operationMode, m_moveOpMode);
            break;
    }
}

QString CustomTrackView::getDisplayTimecode(const GenTime &time) const
{
    return m_document->timecode().getDisplayTimecode(time, KdenliveSettings::frametimecode());
}

QString CustomTrackView::getDisplayTimecodeFromFrames(int frames) const
{
    return m_document->timecode().getDisplayTimecodeFromFrames(frames, KdenliveSettings::frametimecode());
}

void CustomTrackView::graphicsViewMouseEvent(QMouseEvent * event)
{
    QGraphicsView::mouseMoveEvent(event);
}

void CustomTrackView::createRectangleSelection(Qt::KeyboardModifiers modifiers)
{
    setDragMode(QGraphicsView::RubberBandDrag);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    if (!(modifiers & Qt::ControlModifier)) {
        resetSelectionGroup();
        if (m_dragItem) {
            emit clipItemSelected(NULL);
            m_dragItem->setMainSelectedClip(false);
            m_dragItem = NULL;
        }
        scene()->clearSelection();
    }
    m_moveOpMode = RubberSelection;
}

QList<QGraphicsItem *> CustomTrackView::selectAllItemsToTheRight(int x)
{
    QRectF r = m_scene->sceneRect();
    r.setLeft(x);
    return m_scene->items(r);
}

int CustomTrackView::spaceToolSelectTrackOnly(int track, QList<QGraphicsItem *> &selection)
{
    if (m_timeline->getTrackInfo(track).isLocked) {
        // Cannot use spacer on locked track
        emit displayMessage(i18n("Cannot use spacer in a locked track"), ErrorMessage);
        return -1;
    }
    QRectF rect(mapToScene(m_clickEvent).x(), getPositionFromTrack(track) + m_tracksHeight / 2, sceneRect().width() - mapToScene(m_clickEvent).x(), m_tracksHeight / 2 - 2);
    bool isOk;
    selection = checkForGroups(rect, &isOk);
    if (!isOk) {
        // groups found on track, do not allow the move
        emit displayMessage(i18n("Cannot use spacer in a track with a group"), ErrorMessage);
        return -1;
    }
    //qDebug() << "SPACER TOOL + CTRL, SELECTING ALL CLIPS ON TRACK " << track << " WITH SELECTION RECT " << m_clickEvent.x() << '/' <<  track * m_tracksHeight + 1 << "; " << mapFromScene(sceneRect().width(), 0).x() - m_clickEvent.x() << '/' << m_tracksHeight - 2;
    return 0;
}

GenTime CustomTrackView::createGroupForSelectedItems(QList<QGraphicsItem *> &selection)
{
    QList <GenTime> offsetList;
    // create group to hold selected items
    m_selectionMutex.lock();
    m_selectionGroup = new AbstractGroupItem(m_document->fps());
    scene()->addItem(m_selectionGroup);

    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->parentItem() == 0 && (selection.at(i)->type() == AVWidget || selection.at(i)->type() == TransitionWidget)) {
            AbstractClipItem *item = static_cast<AbstractClipItem *>(selection.at(i));
            if (item->isItemLocked()) continue;
            offsetList.append(item->startPos());
            offsetList.append(item->endPos());
            m_selectionGroup->addItem(selection.at(i));
        } else if (selection.at(i)->type() == GroupWidget) {
            if (static_cast<AbstractGroupItem *>(selection.at(i))->isItemLocked()) continue;
            QList<QGraphicsItem *> children = selection.at(i)->childItems();
            for (int j = 0; j < children.count(); ++j) {
                AbstractClipItem *item = static_cast<AbstractClipItem *>(children.at(j));
                offsetList.append(item->startPos());
                offsetList.append(item->endPos());
            }
            m_selectionGroup->addItem(selection.at(i));
        } else if (selection.at(i)->parentItem() && !selection.contains(selection.at(i)->parentItem())) {
            if (static_cast<AbstractGroupItem *>(selection.at(i)->parentItem())->isItemLocked()) continue;
            m_selectionGroup->addItem(selection.at(i)->parentItem());
        }
    }
    m_selectionMutex.unlock();
    if (!offsetList.isEmpty()) {
        qSort(offsetList);
        QList <GenTime> cleandOffsetList;
        GenTime startOffset = offsetList.takeFirst();
        for (int k = 0; k < offsetList.size(); ++k) {
            GenTime newoffset = offsetList.at(k) - startOffset;
            if (newoffset != GenTime() && !cleandOffsetList.contains(newoffset)) {
                cleandOffsetList.append(newoffset);
            }
        }
        updateSnapPoints(NULL, cleandOffsetList, true);
    }
    return m_selectionGroup->startPos();
}

void CustomTrackView::updateTimelineSelection()
{
    if (m_dragItem) {
	m_dragItem->setZValue(99);
	if (m_dragItem->parentItem()) m_dragItem->parentItem()->setZValue(99);
	
	// clip selected, update effect stack
	if (m_dragItem->type() == AVWidget && !m_dragItem->isItemLocked()) {
	    ClipItem *selected = static_cast <ClipItem*>(m_dragItem);
	    emit clipItemSelected(selected, false);
	} else {
	    emit clipItemSelected(NULL);
	}
	if (m_dragItem->type() == TransitionWidget && m_dragItem->isEnabled()) {
	    // update transition menu action
	    m_autoTransition->setChecked(static_cast<Transition *>(m_dragItem)->isAutomatic());
	    m_autoTransition->setEnabled(true);
	    // A transition is selected
	    QPoint p;
	    ClipItem *transitionClip = getClipItemAtStart(m_dragItemInfo.startPos, m_dragItemInfo.track);
	    if (transitionClip && transitionClip->binClip()) {
		int frameWidth = transitionClip->binClip()->getProducerIntProperty(QStringLiteral("meta.media.width"));
		int frameHeight = transitionClip->binClip()->getProducerIntProperty(QStringLiteral("meta.media.height"));
		double factor = transitionClip->binClip()->getProducerProperty(QStringLiteral("aspect_ratio")).toDouble();
		if (factor == 0) factor = 1.0;
		p.setX((int)(frameWidth * factor + 0.5));
		p.setY(frameHeight);
	    }
	    emit transitionItemSelected(static_cast <Transition *>(m_dragItem), getPreviousVideoTrack(m_dragItem->track()), p);
	} else {
	    emit transitionItemSelected(NULL);
	    m_autoTransition->setEnabled(false);
	}
    } else {
	    emit clipItemSelected(NULL);
	    emit transitionItemSelected(NULL);
	    m_autoTransition->setEnabled(false);
    }
}

// virtual
void CustomTrackView::mousePressEvent(QMouseEvent * event)
{
    setFocus(Qt::MouseFocusReason);
    m_menuPosition = QPoint();
    if (m_moveOpMode == MoveOperation) {
        // click while dragging, ignore
        event->ignore();
        return;
    }
    m_moveOpMode = WaitingForConfirm;
    m_clipDrag = false;

    // special cases (middle click button or ctrl / shift click)
    if (event->button() == Qt::MidButton) {
        if (m_operationMode == KeyFrame) {
            if (m_dragItem->type() == AVWidget) {
                ClipItem *item = static_cast<ClipItem *>(m_dragItem);
                m_dragItem->insertKeyframe(item->getEffectAtIndex(item->selectedEffectIndex()), m_dragItem->selectedKeyFramePos(), -1, true);
                m_dragItem->update();
            }
        } else {
            emit playMonitor();
            m_operationMode = None;
        }
        return;
    }
    if (m_tool == SelectTool && event->button() == Qt::LeftButton) {
        if (m_toolManagers.value(SelectType)->mousePress(ItemInfo(), event->modifiers())) {
            QGraphicsView::mousePressEvent(event);
            return;
        }
    }
    m_dragGuide = NULL;
    m_clickEvent = event->pos();

    // check item under mouse
    QList<QGraphicsItem *> collisionList = items(m_clickEvent);
    if (event->button() == Qt::LeftButton && event->modifiers() == Qt::ControlModifier && m_tool != SpacerTool && collisionList.count() == 0) {
        // Pressing Ctrl + left mouse button in an empty area scrolls the timeline
        setDragMode(QGraphicsView::ScrollHandDrag);
        m_moveOpMode = ScrollTimeline;
        QGraphicsView::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton && m_toolManagers.value(GuideType)->mousePress(ItemInfo(), event->modifiers(), collisionList)) {
        QGraphicsView::mousePressEvent(event);
        return;
    }

    // Find first clip, transition or group under mouse (when no guides selected)
    int ct = 0;
    AbstractGroupItem *dragGroup = NULL;
    AbstractClipItem *collisionClip = NULL;
    bool found = false;
    QList<int> lockedTracks;
    double yOffset = 0;
    m_selectionMutex.lock();
    while (!m_dragGuide && ct < collisionList.count()) {
        if (collisionList.at(ct)->type() == AVWidget || collisionList.at(ct)->type() == TransitionWidget) {
            collisionClip = static_cast <AbstractClipItem *>(collisionList.at(ct));
            if (collisionClip->isItemLocked() || !collisionClip->isEnabled()) {
                ct++;
                continue;
            }
            if (collisionClip == m_dragItem) {
                collisionClip = NULL;
            }
            else {
                if (m_dragItem) {
                    m_dragItem->setMainSelectedClip(false);
                }
                m_dragItem = collisionClip;
                m_dragItem->setMainSelectedClip(true);
            }
            found = true;
	    bool allowAudioOnly = false;
	    if (KdenliveSettings::splitaudio() && m_dragItem->type() == AVWidget) {
		ClipItem *clp = static_cast<ClipItem *>(m_dragItem);
		if (clp) {
		    if (clp->clipType() == Audio || clp->clipState() == PlaylistState::AudioOnly) {
			allowAudioOnly = true;
		    }
		}
	    }
            for (int i = 1; i < m_timeline->tracksCount(); ++i) {
		TrackInfo nfo = m_timeline->getTrackInfo(i);
                if (nfo.isLocked || (allowAudioOnly && nfo.type == VideoTrack))
		    lockedTracks << i;
            }
            yOffset = mapToScene(m_clickEvent).y() - m_dragItem->scenePos().y();
            m_dragItem->setProperty("y_absolute", yOffset);
            m_dragItem->setProperty("locked_tracks", QVariant::fromValue(lockedTracks));
            m_dragItemInfo = m_dragItem->info();
            if (m_selectionGroup) {
                m_selectionGroup->setProperty("y_absolute", yOffset);
                m_selectionGroup->setProperty("locked_tracks", QVariant::fromValue(lockedTracks));
            }
            if (m_dragItem->parentItem() && m_dragItem->parentItem()->type() == GroupWidget && m_dragItem->parentItem() != m_selectionGroup) {
                QGraphicsItem *topGroup = m_dragItem->parentItem();
                while (topGroup->parentItem() && topGroup->parentItem()->type() == GroupWidget && topGroup->parentItem() != m_selectionGroup) {
                    topGroup = topGroup->parentItem();
                }
                dragGroup = static_cast <AbstractGroupItem *>(topGroup);
                dragGroup->setProperty("y_absolute", yOffset);
                dragGroup->setProperty("locked_tracks", QVariant::fromValue(lockedTracks));
            }
            break;
        }
        ct++;
    }
    m_selectionMutex.unlock();
    if (!found) {
        if (m_dragItem)
            m_dragItem->setMainSelectedClip(false);
        m_dragItem = NULL;
    }

    // No item under click
    if (m_dragItem == NULL && m_tool != SpacerTool) {
        resetSelectionGroup(false);
        m_scene->clearSelection();
        updateClipTypeActions(NULL);
	updateTimelineSelection();
        if (event->button() == Qt::LeftButton) {
	    m_moveOpMode = Seek;
            setCursor(Qt::ArrowCursor);
            seekCursorPos((int)(mapToScene(event->x(), 0).x()));
            event->setAccepted(true);
	    QGraphicsView::mousePressEvent(event);
            return;
        }
    }

    // context menu requested
    if (event->button() == Qt::RightButton) {
        // Check if we want keyframes context menu
        if (!m_dragItem && !m_dragGuide) {
            // check if there is a guide close to mouse click
            QList<QGraphicsItem *> guidesCollisionList = items(event->pos().x() - 5, event->pos().y(), 10, 2); // a rect of height < 2 does not always collide with the guide
            for (int i = 0; i < guidesCollisionList.count(); ++i) {
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
        m_menuPosition = m_clickEvent;
        /* if (dragGroup == NULL) {
            if (m_dragItem && m_dragItem->parentItem() && m_dragItem->parentItem() != m_selectionGroup)
                dragGroup = static_cast<AbstractGroupItem*> (m_dragItem->parentItem());
        }
        */
        if (m_dragItem) {
            if (!m_dragItem->isSelected()) {
                resetSelectionGroup();
                m_scene->clearSelection();
                m_dragItem->setSelected(true);
            }
	    m_dragItem->setZValue(99);
	    if (m_dragItem->parentItem()) m_dragItem->parentItem()->setZValue(99);
	}
	event->accept();
        updateTimelineSelection();
	return;
    }

    QPointF clickPoint = mapToScene(event->pos());
    ItemInfo clickInfo;
    clickInfo.startPos = GenTime(clickPoint.x(), m_document->fps());
    clickInfo.track = getTrackFromPos(clickPoint.y());
    if (event->button() == Qt::LeftButton) {
        if (m_tool == SpacerTool) {
            m_toolManagers.value(SpacerType)->mousePress(clickInfo, event->modifiers());
            QGraphicsView::mousePressEvent(event);
            return;
        }
        // Razor tool
        if (m_tool == RazorTool) {
            m_document->renderer()->switchPlay(false);
            m_toolManagers.value(RazorType)->mousePress(clickInfo);
            event->accept();
            return;
        }
    }
    if (m_dragItem) {
        bool itemSelected = false;
        bool selected = true;
        if (m_dragItem->isSelected()) {
            itemSelected = true;
            selected = false;
        } else if (m_dragItem->parentItem() && m_dragItem->parentItem()->isSelected()) {
            itemSelected = true;
        } else if (dragGroup && dragGroup->isSelected()) {
            itemSelected = true;
        }

        if (event->modifiers() & Qt::ControlModifier && m_operationMode != ResizeEnd && m_operationMode != ResizeStart)  {
            // Handle ctrl click events
            resetSelectionGroup();
            m_dragItem->setSelected(selected);
            groupSelectedItems(QList <QGraphicsItem*>(), false, true);
            if (selected) {
                m_selectionMutex.lock();
                if (m_selectionGroup) {
                    m_selectionGroup->setProperty("y_absolute", yOffset);
                    m_selectionGroup->setProperty("locked_tracks", QVariant::fromValue(lockedTracks));
                }
                m_selectionMutex.unlock();
            }
            else {
                m_dragItem->setMainSelectedClip(false);
                m_dragItem = NULL;
            }
            updateTimelineSelection();
            return;

            resetSelectionGroup();
            m_dragItem->setSelected(selected);
            groupSelectedItems(QList <QGraphicsItem*>(), false, true);
            if (selected) {
                m_selectionMutex.lock();
                if (m_selectionGroup) {
                    m_selectionGroup->setProperty("y_absolute", yOffset);
                    m_selectionGroup->setProperty("locked_tracks", QVariant::fromValue(lockedTracks));
                }
                m_selectionMutex.unlock();
            }
            else {
                m_dragItem->setMainSelectedClip(false);
                m_dragItem = NULL;
            }
            updateTimelineSelection();
            return;
        }
        if (itemSelected == false) {
            // User clicked a non selected item, select it
            resetSelectionGroup(false);
            m_scene->clearSelection();
            m_dragItem->setSelected(true);
            m_dragItem->setZValue(99);
            if (m_dragItem->parentItem()) m_dragItem->parentItem()->setZValue(99);

            if (m_dragItem && m_dragItem->type() == AVWidget) {
                ClipItem *clip = static_cast<ClipItem*>(m_dragItem);
                updateClipTypeActions(dragGroup == NULL ? clip : NULL);
                m_pasteEffectsAction->setEnabled(m_copiedItems.count() == 1);
            }
            else updateClipTypeActions(NULL);
        }
        else {
            m_selectionMutex.lock();
            if (m_selectionGroup) {
                QList<QGraphicsItem *> children = m_selectionGroup->childItems();
                for (int i = 0; i < children.count(); ++i) {
                    children.at(i)->setSelected(itemSelected);
                }
                m_selectionGroup->setSelected(itemSelected);

            }
            if (dragGroup) {
                dragGroup->setSelected(itemSelected);
            }
            m_dragItem->setSelected(itemSelected);
            m_selectionMutex.unlock();
        }

        m_selectionMutex.lock();
        if (m_selectionGroup) {
            m_selectionGroup->setProperty("y_absolute", yOffset);
            m_selectionGroup->setProperty("locked_tracks", QVariant::fromValue(lockedTracks));
        }
        m_selectionMutex.unlock();

        updateTimelineSelection();
    }
    if (event->button() == Qt::LeftButton) {
        if (m_dragItem) {
            if (m_selectionGroup && m_dragItem->parentItem() == m_selectionGroup) {
                // all other modes break the selection, so the user probably wants to move it
                m_operationMode = MoveOperation;
            } else {
                if (m_dragItem->rect().width() * transform().m11() < 15) {
                    // If the item is very small, only allow move
                    m_operationMode = MoveOperation;
                }
                else m_operationMode = m_dragItem->operationMode(m_dragItem->mapFromScene(mapToScene(event->pos())), event->modifiers());
                if (m_operationMode == ResizeEnd) {
                    // FIXME: find a better way to avoid move in ClipItem::itemChange?
                    m_dragItem->setProperty("resizingEnd", true);
                }
            }
        }
        else m_operationMode = None;
    }
    // Update snap points
    if (m_selectionGroup == NULL) {
        if (m_operationMode == ResizeEnd || m_operationMode == ResizeStart) {
            updateSnapPoints(NULL);
        } else {
            updateSnapPoints(m_dragItem);
        }
    } else {
        m_selectionMutex.lock();
        QList <GenTime> offsetList;
        QList<QGraphicsItem *> children = m_selectionGroup->childItems();
        for (int i = 0; i < children.count(); ++i) {
            if (children.at(i)->type() == AVWidget || children.at(i)->type() == TransitionWidget) {
                AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
                offsetList.append(item->startPos());
                offsetList.append(item->endPos());
            }
        }
        if (!offsetList.isEmpty()) {
            qSort(offsetList);
            GenTime startOffset = offsetList.takeFirst();
            QList <GenTime> cleandOffsetList;
            for (int k = 0; k < offsetList.size(); ++k) {
                GenTime newoffset = offsetList.at(k) - startOffset;
                if (newoffset != GenTime() && !cleandOffsetList.contains(newoffset)) {
                    cleandOffsetList.append(newoffset);
                }
            }
            updateSnapPoints(NULL, cleandOffsetList, true);
        }
        m_selectionMutex.unlock();
    }

    if (m_operationMode == ResizeEnd || m_operationMode == ResizeStart) {
        // Start Ripple edit
        if (event->modifiers() & Qt::ControlModifier && event->modifiers() & Qt::ShiftModifier) {
            // Rolling edit
            if (!m_toolManagers.value(TrimType)->mousePress(m_dragItemInfo)) {
                event->ignore();
                return;
            }
        } else {
            // Resize
            m_toolManagers.value(ResizeType)->mousePress(m_dragItemInfo, event->modifiers());
        }
    }

    QGraphicsView::mousePressEvent(event);
    if (m_operationMode == KeyFrame) {
        m_dragItem->prepareKeyframeMove();
        return;
    } else if (m_operationMode == MoveOperation) {
        m_toolManagers.value(MoveType)->mousePress(m_dragItem->info());
    } else if (m_operationMode == TransitionStart && event->modifiers() != Qt::ControlModifier) {
        ItemInfo info;
        info.startPos = m_dragItem->startPos();
        info.track = m_dragItem->track();
        int transitiontrack = getPreviousVideoTrack(info.track);
        ClipItem *transitionClip = NULL;
        if (transitiontrack != 0) transitionClip = getClipItemAtMiddlePoint(info.startPos.frames(m_document->fps()), transitiontrack);
        if (transitionClip && transitionClip->endPos() < m_dragItem->endPos()) {
            info.endPos = transitionClip->endPos();
        } else {
            GenTime transitionDuration(m_document->getFramePos(KdenliveSettings::transition_duration()), m_document->fps());
            if (m_dragItem->cropDuration() < transitionDuration)
                info.endPos = m_dragItem->endPos();
            else
                info.endPos = info.startPos + transitionDuration;
        }
        if (info.endPos == info.startPos) info.endPos = info.startPos + GenTime(m_document->getFramePos(KdenliveSettings::transition_duration()), m_document->fps());
        // Check there is no other transition at that place
        double startY = getPositionFromTrack(info.track) + 1 + m_tracksHeight / 2;
        QRectF r(info.startPos.frames(m_document->fps()), startY, (info.endPos - info.startPos).frames(m_document->fps()), m_tracksHeight / 2);
        QList<QGraphicsItem *> selection = m_scene->items(r);
        bool transitionAccepted = true;
        for (int i = 0; i < selection.count(); ++i) {
            if (selection.at(i)->type() == TransitionWidget) {
                Transition *tr = static_cast <Transition *>(selection.at(i));
                if (tr->startPos() - info.startPos > GenTime(5, m_document->fps())) {
                    if (tr->startPos() < info.endPos) info.endPos = tr->startPos();
                } else transitionAccepted = false;
            }
        }
        if (transitionAccepted) slotAddTransition(static_cast<ClipItem*>(m_dragItem), info, transitiontrack);
        else emit displayMessage(i18n("Cannot add transition"), ErrorMessage);
    } else if (m_operationMode == TransitionEnd && event->modifiers() != Qt::ControlModifier) {
        ItemInfo info;
        info.endPos = GenTime(m_dragItem->endPos().frames(m_document->fps()), m_document->fps());
        info.track = m_dragItem->track();
        int transitiontrack = getPreviousVideoTrack(info.track);
        ClipItem *transitionClip = NULL;
        if (transitiontrack != 0) transitionClip = getClipItemAtMiddlePoint(info.endPos.frames(m_document->fps()), transitiontrack);
        if (transitionClip && transitionClip->startPos() > m_dragItem->startPos()) {
            info.startPos = transitionClip->startPos();
        } else {
            GenTime transitionDuration(m_document->getFramePos(KdenliveSettings::transition_duration()), m_document->fps());
            if (m_dragItem->cropDuration() < transitionDuration)
                info.startPos = m_dragItem->startPos();
            else
                info.startPos = info.endPos - transitionDuration;
        }
        if (info.endPos == info.startPos) {
            GenTime transitionDuration(m_document->getFramePos(KdenliveSettings::transition_duration()), m_document->fps());
            if (info.endPos < transitionDuration) {
                info.startPos = GenTime();
            } else {
                info.startPos = info.endPos - transitionDuration;
            }
        }
        QDomElement transition = MainWindow::transitions.getEffectByTag(QStringLiteral("luma"), QStringLiteral("dissolve")).cloneNode().toElement();
        EffectsList::setParameter(transition, QStringLiteral("reverse"), QStringLiteral("1"));

        // Check there is no other transition at that place
	double startY = getPositionFromTrack(info.track) + 1 + m_tracksHeight / 2;
        QRectF r(info.startPos.frames(m_document->fps()), startY, (info.endPos - info.startPos).frames(m_document->fps()), m_tracksHeight / 2);
        QList<QGraphicsItem *> selection = m_scene->items(r);
        bool transitionAccepted = true;
        for (int i = 0; i < selection.count(); ++i) {
            if (selection.at(i)->type() == TransitionWidget) {
                Transition *tr = static_cast <Transition *>(selection.at(i));
                if (info.endPos - tr->endPos() > GenTime(5, m_document->fps())) {
                    if (tr->endPos() > info.startPos) info.startPos = tr->endPos();
                } else transitionAccepted = false;
            }
        }
        if (transitionAccepted) slotAddTransition(static_cast<ClipItem*>(m_dragItem), info, transitiontrack, transition);
        else emit displayMessage(i18n("Cannot add transition"), ErrorMessage);

    }
}

void CustomTrackView::rebuildGroup(int childTrack, const GenTime &childPos)
{
    const QPointF p((int)childPos.frames(m_document->fps()), getPositionFromTrack(childTrack) + m_tracksHeight / 2);
    QList<QGraphicsItem *> list = scene()->items(p);
    AbstractGroupItem *group = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (!list.at(i)->isEnabled()) continue;
        if (list.at(i)->type() == GroupWidget) {
            group = static_cast <AbstractGroupItem *>(list.at(i));
            break;
        }
    }
    rebuildGroup(group);
}

void CustomTrackView::rebuildGroup(AbstractGroupItem *group)
{
    if (group) {
        m_selectionMutex.lock();
        if (group == m_selectionGroup) m_selectionGroup = NULL;
        QList <QGraphicsItem *> children = group->childItems();
        m_document->clipManager()->removeGroup(group);
        for (int i = 0; i < children.count(); ++i) {
            group->removeFromGroup(children.at(i));
        }
        scene()->destroyItemGroup(group);
        m_selectionMutex.unlock();
        groupSelectedItems(children, group != m_selectionGroup, true);
    }
}

void CustomTrackView::resetSelectionGroup(bool selectItems)
{
    QMutexLocker lock(&m_selectionMutex);
    if (m_selectionGroup) {
        // delete selection group
        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);

        QList<QGraphicsItem *> children = m_selectionGroup->childItems();
        scene()->destroyItemGroup(m_selectionGroup);
        m_selectionGroup = NULL;
        for (int i = 0; i < children.count(); ++i) {
            if (children.at(i)->parentItem() == 0) {
                if ((children.at(i)->type() == AVWidget || children.at(i)->type() == TransitionWidget)) {
                    if (!static_cast <AbstractClipItem *>(children.at(i))->isItemLocked()) {
                        children.at(i)->setFlag(QGraphicsItem::ItemIsMovable, true);
                        children.at(i)->setSelected(selectItems);
                    }
                } else if (children.at(i)->type() == GroupWidget) {
                    children.at(i)->setFlag(QGraphicsItem::ItemIsMovable, true);
                    children.at(i)->setSelected(selectItems);
                }
            }
        }
        KdenliveSettings::setSnaptopoints(snap);
    }
}

GenTime CustomTrackView::groupSelectedItems(QList <QGraphicsItem *> selection, bool createNewGroup, bool selectNewGroup)
{
    QMutexLocker lock(&m_selectionMutex);
    if (m_selectionGroup) {
        qDebug() << "///// ERROR, TRYING TO OVERRIDE EXISTING GROUP";
        return GenTime();
    }
    if (selection.isEmpty()) selection = m_scene->selectedItems();
    // Split groups and items
    QSet <QGraphicsItemGroup *> groupsList;
    QSet <QGraphicsItem *> itemsList;

    for (int i = 0; i < selection.count(); ++i) {
        if (selectNewGroup) selection.at(i)->setSelected(true);
        if (selection.at(i)->type() == GroupWidget) {
            AbstractGroupItem *it = static_cast <AbstractGroupItem *> (selection.at(i));
            while (it->parentItem() && it->parentItem()->type() == GroupWidget) {
                it = static_cast <AbstractGroupItem *>(it->parentItem());
            }
            if (!it || it->isItemLocked()) continue;
            groupsList.insert(it);
        }
    }
    bool lockGroup = false;
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == AVWidget || selection.at(i)->type() == TransitionWidget) {
            if (selection.at(i)->parentItem() && selection.at(i)->parentItem()->type() == GroupWidget) {
                AbstractGroupItem *it = static_cast <AbstractGroupItem *> (selection.at(i)->parentItem());
                while (it->parentItem() && it->parentItem()->type() == GroupWidget) {
                    it = static_cast <AbstractGroupItem *>(it->parentItem());
                }
                if (!it || it->isItemLocked()) continue;
                groupsList.insert(it);
            }
            else {
                AbstractClipItem *it = static_cast<AbstractClipItem *> (selection.at(i));
                if (!it) continue;
                if (it->isItemLocked()) lockGroup = true;
                itemsList.insert(selection.at(i));
            }
        }
    }
    if (itemsList.isEmpty() && groupsList.isEmpty()) return GenTime();
    if (itemsList.count() == 1 && groupsList.isEmpty()) {
        // only one item selected:
        QSetIterator<QGraphicsItem *> it(itemsList);
        m_dragItem = static_cast<AbstractClipItem *>(it.next());
        m_dragItem->setMainSelectedClip(true);
        m_dragItem->setSelected(true);
        return m_dragItem->startPos();
    }

    QRectF rectUnion;
    // Find top left position of selection
    foreach (const QGraphicsItemGroup *value, groupsList) {
        rectUnion = rectUnion.united(value->sceneBoundingRect());
    }
    foreach (const QGraphicsItem *value, itemsList) {
        rectUnion = rectUnion.united(value->sceneBoundingRect());
    }
    bool snap = KdenliveSettings::snaptopoints();
    KdenliveSettings::setSnaptopoints(false);
    if (createNewGroup) {
        AbstractGroupItem *newGroup = m_document->clipManager()->createGroup();
        newGroup->setPos(rectUnion.left(), rectUnion.top() - 1);
        QPointF diff = newGroup->pos();
        newGroup->setTransform(QTransform::fromTranslate(-diff.x(), -diff.y()), true);
        //newGroup->translate((int) -rectUnion.left(), (int) -rectUnion.top() + 1);

        // Check if we are trying to include a group in a group
        foreach (QGraphicsItemGroup *value, groupsList) {
            newGroup->addItem(value);
        }

        foreach (QGraphicsItemGroup *value, groupsList) {
            QList<QGraphicsItem *> children = value->childItems();
            for (int i = 0; i < children.count(); ++i) {
                if (children.at(i)->type() == AVWidget || children.at(i)->type() == TransitionWidget)
                    itemsList.insert(children.at(i));
            }
            AbstractGroupItem *grp = static_cast<AbstractGroupItem *>(value);
            m_document->clipManager()->removeGroup(grp);
            if (grp == m_selectionGroup) m_selectionGroup = NULL;
            scene()->destroyItemGroup(grp);
        }

        foreach (QGraphicsItem *value, itemsList) {
            newGroup->addItem(value);
        }
        if (lockGroup)
            newGroup->setItemLocked(true);
        scene()->addItem(newGroup);
        KdenliveSettings::setSnaptopoints(snap);
        if (selectNewGroup) newGroup->setSelected(true);
        return newGroup->startPos();
    } else {
        m_selectionGroup = new AbstractGroupItem(m_document->fps());
        m_selectionGroup->setPos(rectUnion.left(), rectUnion.top() - 1);
        QPointF diff = m_selectionGroup->pos();
        //m_selectionGroup->translate((int) - rectUnion.left(), (int) -rectUnion.top() + 1);
        m_selectionGroup->setTransform(QTransform::fromTranslate(- diff.x(), -diff.y()), true);

        scene()->addItem(m_selectionGroup);
        foreach (QGraphicsItemGroup *value, groupsList) {
            m_selectionGroup->addItem(value);
        }
        foreach (QGraphicsItem *value, itemsList) {
            m_selectionGroup->addItem(value);
        }
        KdenliveSettings::setSnaptopoints(snap);
        if (m_selectionGroup) {
            if (selectNewGroup) m_selectionGroup->setSelected(true);
            return m_selectionGroup->startPos();
        }
    }
    return GenTime();
}

void CustomTrackView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (m_dragItem && m_dragItem->keyframesCount() > 0) {
        // add keyframe
        GenTime keyFramePos = GenTime((int)(mapToScene(event->pos()).x()), m_document->fps()) - m_dragItem->startPos();// + m_dragItem->cropStart();
        int single = m_dragItem->keyframesCount();
        double val = m_dragItem->getKeyFrameClipHeight(mapToScene(event->pos()).y() - m_dragItem->scenePos().y());
        ClipItem * item = static_cast <ClipItem *>(m_dragItem);
        QDomElement oldEffect = item->selectedEffect().cloneNode().toElement();
        if (single == 1) {
            item->insertKeyframe(item->getEffectAtIndex(item->selectedEffectIndex()), (item->cropDuration()).frames(m_document->fps()) - 1, -1, true);
        }
        //QString previous = item->keyframes(item->selectedEffectIndex());
        item->insertKeyframe(item->getEffectAtIndex(item->selectedEffectIndex()), keyFramePos.frames(m_document->fps()), val);
        //QString next = item->keyframes(item->selectedEffectIndex());
        QDomElement newEffect = item->selectedEffect().cloneNode().toElement();
        EditEffectCommand *command = new EditEffectCommand(this, item->track(), item->startPos(), oldEffect, newEffect, item->selectedEffectIndex(), false, false, true);
        m_commandStack->push(command);
        updateEffect(item->track(), item->startPos(), item->selectedEffect());
        emit clipItemSelected(item, item->selectedEffectIndex());
    } else if (m_dragItem && !m_dragItem->isItemLocked()) {
        editItemDuration();
    } else {
        QList<QGraphicsItem *> collisionList = items(event->pos());
        if (collisionList.count() == 1 && collisionList.at(0)->type() == GUIDEITEM) {
            Guide *editGuide = static_cast<Guide*>(collisionList.at(0));
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

    if (item->type() == GroupWidget || (item->parentItem() && item->parentItem()->type() == GroupWidget)) {
        emit displayMessage(i18n("Cannot edit an item in a group"), ErrorMessage);
        return;
    }

    if (!item->isItemLocked()) {
        GenTime minimum;
        GenTime maximum;
        if (item->type() == TransitionWidget)
            getTransitionAvailableSpace(item, minimum, maximum);
        else
            getClipAvailableSpace(item, minimum, maximum);

        QPointer<ClipDurationDialog> d = new ClipDurationDialog(item,
                                                                m_document->timecode(), minimum, maximum, this);
        if (d->exec() == QDialog::Accepted) {
            ItemInfo clipInfo = item->info();
            ItemInfo startInfo = clipInfo;
            if (item->type() == TransitionWidget) {
                // move & resize transition
                clipInfo.startPos = d->startPos();
                clipInfo.endPos = clipInfo.startPos + d->duration();
                clipInfo.track = item->track();
                MoveTransitionCommand *command = new MoveTransitionCommand(this, startInfo, clipInfo, true, true);
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
                    // TODO: find a way to apply adjusteffect after the resize command was done / undone
                    new ResizeClipCommand(this, startInfo, clipInfo, false, true, moveCommand);
                    adjustEffects(clip, startInfo, moveCommand);
                }

                if (d->startPos() != clipInfo.startPos) {
                    startInfo = clipInfo;
                    clipInfo.startPos = d->startPos();
                    clipInfo.endPos = item->endPos() + (clipInfo.startPos - startInfo.startPos);
                    new MoveClipCommand(this, startInfo, clipInfo, false, true, moveCommand);
                }

                if (d->duration() > item->cropDuration()) {
                    // duration was increased, so process it after move
                    startInfo = clipInfo;
                    clipInfo.endPos = clipInfo.startPos + d->duration();
                    clipInfo.cropStart = d->cropStart();

                    resizeClip(startInfo, clipInfo);
                    // TODO: find a way to apply adjusteffect after the resize command was done / undone
                    new ResizeClipCommand(this, startInfo, clipInfo, false, true, moveCommand);
                    adjustEffects(clip, startInfo, moveCommand);
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


void CustomTrackView::contextMenuEvent(QContextMenuEvent * event)
{
    if (m_operationMode == KeyFrame) {
        displayKeyframesMenu(event->globalPos(), m_dragItem);
    } else {
        displayContextMenu(event->globalPos(), m_dragItem);
    }
    event->accept();
}

void CustomTrackView::displayKeyframesMenu(QPoint pos, AbstractClipItem *clip)
{
    if (!m_timelineContextKeyframeMenu) {
        m_timelineContextKeyframeMenu = new QMenu(this);
        // Keyframe type widget
        m_selectKeyframeType = new KSelectAction(KoIconUtils::themedIcon(QStringLiteral("keyframes")), i18n("Interpolation"), this);
        QAction *discrete = new QAction(KoIconUtils::themedIcon(QStringLiteral("discrete")), i18n("Discrete"), this);
        discrete->setData((int) mlt_keyframe_discrete);
        discrete->setCheckable(true);
        m_selectKeyframeType->addAction(discrete);
        QAction *linear = new QAction(KoIconUtils::themedIcon(QStringLiteral("linear")), i18n("Linear"), this);
        linear->setData((int) mlt_keyframe_linear);
        linear->setCheckable(true);
        m_selectKeyframeType->addAction(linear);
        QAction *curve = new QAction(KoIconUtils::themedIcon(QStringLiteral("smooth")), i18n("Smooth"), this);
        curve->setData((int) mlt_keyframe_smooth);
        curve->setCheckable(true);
        m_selectKeyframeType->addAction(curve);
        m_timelineContextKeyframeMenu->addAction(m_selectKeyframeType);
        m_attachKeyframeToEnd = new QAction(i18n("Attach keyframe to end"), this);
        m_attachKeyframeToEnd->setCheckable(true);
        m_timelineContextKeyframeMenu->addAction(m_attachKeyframeToEnd);
        connect(m_selectKeyframeType, SIGNAL(triggered(QAction*)), this, SLOT(slotEditKeyframeType(QAction*)));
        connect(m_attachKeyframeToEnd, SIGNAL(triggered(bool)), this, SLOT(slotAttachKeyframeToEnd(bool)));
    }
    m_attachKeyframeToEnd->setChecked(clip->isAttachedToEnd());
    m_selectKeyframeType->setCurrentAction(clip->parseKeyframeActions(m_selectKeyframeType->actions()));
    m_timelineContextKeyframeMenu->exec(pos);
}

void CustomTrackView::slotAttachKeyframeToEnd(bool attach)
{
    ClipItem * item = static_cast <ClipItem *>(m_dragItem);
    QDomElement oldEffect = item->selectedEffect().cloneNode().toElement();
    item->attachKeyframeToEnd(item->getEffectAtIndex(item->selectedEffectIndex()), attach);
    QDomElement newEffect = item->selectedEffect().cloneNode().toElement();
    EditEffectCommand *command = new EditEffectCommand(this, item->track(), item->startPos(), oldEffect, newEffect, item->selectedEffectIndex(), false, false, false);
    m_commandStack->push(command);
    updateEffect(item->track(), item->startPos(), item->selectedEffect());
    emit clipItemSelected(item, item->selectedEffectIndex());
}

void CustomTrackView::slotEditKeyframeType(QAction *action)
{
    int type = action->data().toInt();
    ClipItem * item = static_cast <ClipItem *>(m_dragItem);
    QDomElement oldEffect = item->selectedEffect().cloneNode().toElement();
    item->editKeyframeType(item->getEffectAtIndex(item->selectedEffectIndex()), type);
    QDomElement newEffect = item->selectedEffect().cloneNode().toElement();
    EditEffectCommand *command = new EditEffectCommand(this, item->track(), item->startPos(), oldEffect, newEffect, item->selectedEffectIndex(), false, false, false);
    m_commandStack->push(command);
    updateEffect(item->track(), item->startPos(), item->selectedEffect());
    emit clipItemSelected(item, item->selectedEffectIndex());
}

void CustomTrackView::displayContextMenu(QPoint pos, AbstractClipItem *clip)
{
    bool isGroup = clip != NULL && clip->parentItem() && clip->parentItem()->type() == GroupWidget && clip->parentItem() != m_selectionGroup;
    m_deleteGuide->setEnabled(m_dragGuide != NULL);
    m_editGuide->setEnabled(m_dragGuide != NULL);
    m_markerMenu->clear();
    m_markerMenu->setEnabled(false);
    if (clip == NULL) {
        m_timelineContextMenu->popup(pos);
    } else if (isGroup) {
        m_pasteEffectsAction->setEnabled(m_copiedItems.count() == 1);
        m_ungroupAction->setEnabled(true);
        if (clip->type() == AVWidget) {
            ClipItem *item = static_cast <ClipItem*>(clip);
            m_disableClipAction->setChecked(item->clipState() == PlaylistState::Disabled);
        } else {
            m_disableClipAction->setChecked(false);
        }
        updateClipTypeActions(NULL);
        m_timelineContextClipMenu->popup(pos);
    } else {
        m_ungroupAction->setEnabled(false);
        if (clip->type() == AVWidget) {
            ClipItem *item = static_cast <ClipItem*>(clip);
            m_disableClipAction->setChecked(item->clipState() == PlaylistState::Disabled);
            //build go to marker menu
            ClipController *controller = m_document->getClipController(item->getBinId());
            if (controller) {
                QList <CommentedTime> markers = controller->commentedSnapMarkers();
                int offset = (item->startPos()- item->cropStart()).frames(m_document->fps());
                if (!markers.isEmpty()) {
                    for (int i = 0; i < markers.count(); ++i) {
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
        } else if (clip->type() == TransitionWidget) {
            m_timelineContextTransitionMenu->exec(pos);
        }
    }
}

void CustomTrackView::activateMonitor()
{
    emit activateDocumentMonitor();
}

void CustomTrackView::insertClipCut(const QString &id, int in, int out)
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
    PlaylistState::ClipState state = PlaylistState::Original;
    if (KdenliveSettings::splitaudio()) {
        if (m_timeline->videoTarget > -1) {
            pasteInfo.track = m_timeline->videoTarget;
            if (m_timeline->audioTarget == -1)
                state = PlaylistState::VideoOnly;
        }
        else if (m_timeline->audioTarget > -1) {
            pasteInfo.track = m_timeline->audioTarget;
            state = PlaylistState::AudioOnly;
        } else {
            emit displayMessage(i18n("Please select target track(s) to perform operation"), ErrorMessage);
            return;
        }
    } else {
        pasteInfo.track = selectedTrack();
    }
    if (m_timeline->getTrackInfo(pasteInfo.track).isLocked) {
        emit displayMessage(i18n("Cannot perform operation on a locked track"), ErrorMessage);
        return;
    }
    bool ok = canBePastedTo(pasteInfo, AVWidget);
    if (!ok) {
        // Cannot be inserted at cursor pos, insert at end of track
        int duration = GenTime(m_timeline->track(pasteInfo.track)->length()).frames(m_document->fps());
        pasteInfo.startPos = GenTime(duration, m_document->fps());
        pasteInfo.endPos = pasteInfo.startPos + info.endPos;
        ok = canBePastedTo(pasteInfo, AVWidget);
    }
    if (!ok) {
        emit displayMessage(i18n("Cannot insert clip in timeline"), ErrorMessage);
        return;
    }

    // Add refresh command for undo
    QUndoCommand *addCommand = new QUndoCommand();
    addCommand->setText(i18n("Add timeline clip"));
    new AddTimelineClipCommand(this, id, pasteInfo, EffectsList(), state, true, false, true, addCommand);

    // Automatic audio split
    if (KdenliveSettings::splitaudio() && m_timeline->audioTarget > -1 && m_timeline->videoTarget > -1) {
        if (!m_timeline->getTrackInfo(m_timeline->audioTarget).isLocked && m_document->getBinClip(id)->isSplittable())
            splitAudio(false, pasteInfo, m_timeline->audioTarget, addCommand);
    }
    else
        updateTrackDuration(pasteInfo.track, addCommand);
    m_commandStack->push(addCommand);

    selectClip(true, false);
}

bool CustomTrackView::insertDropClips(const QMimeData *data, const QPoint &pos)
{
    m_clipDrag = data->hasFormat(QStringLiteral("kdenlive/clip")) || data->hasFormat(QStringLiteral("kdenlive/producerslist"));
    // This is not a clip drag, maybe effect or other...
    if (!m_clipDrag) return false;
    m_scene->clearSelection();
    if (m_dragItem)
        m_dragItem->setMainSelectedClip(false);
    m_dragItem = NULL;
    resetSelectionGroup(false);
    QPointF framePos = mapToScene(pos);
    int track = getTrackFromPos(framePos.y());
    QMutexLocker lock(&m_selectionMutex);
    if (track <= 0 || track > m_timeline->tracksCount() - 1 || m_timeline->getTrackInfo(track).isLocked) return true;
    if (data->hasFormat(QStringLiteral("kdenlive/clip"))) {
        QStringList list = QString(data->data(QStringLiteral("kdenlive/clip"))).split(';');
        ProjectClip *clip = m_document->getBinClip(list.at(0));
        if (clip == NULL) {
            //qDebug() << " WARNING))))))))) CLIP NOT FOUND : " << list.at(0);
            return false;
        }
        if (!clip->isReady()) {
            emit displayMessage(i18n("Clip not ready"), ErrorMessage);
            return false;
        }

        // Check if clip can be inserted at that position
        ItemInfo info;
        info.startPos = GenTime((int)(framePos.x() + 0.5), m_document->fps());
        info.cropStart = GenTime(list.at(1).toInt(), m_document->fps());
        info.cropDuration = GenTime(list.at(2).toInt() - list.at(1).toInt() + 1, m_document->fps());
        info.endPos = info.startPos + info.cropDuration;
        info.track = track;
        framePos.setX((int)(framePos.x() + 0.5));
        framePos.setY(getPositionFromTrack(track));
        if (m_scene->editMode() == TimelineMode::NormalEdit && !canBePastedTo(info, AVWidget)) {
            return true;
        }
        QList<int> lockedTracks;
	bool allowAudioOnly = false;
	if (KdenliveSettings::splitaudio()) {
	    if (clip) {
		if (clip->clipType() == Audio) {
		    allowAudioOnly = true;
		}
	    }
	}
        for (int i = 1; i < m_timeline->tracksCount(); ++i) {
	    TrackInfo nfo = m_timeline->getTrackInfo(i);
            if (nfo.isLocked || (allowAudioOnly && nfo.type == VideoTrack))
		lockedTracks << i;
        }
        if (lockedTracks.contains(track)) {
	    return false;
	}

        m_selectionGroup = new AbstractGroupItem(m_document->fps());
        ClipItem *item = new ClipItem(clip, info, m_document->fps(), 1.0, 1, getFrameWidth());
        connect(item, &AbstractClipItem::selectItem, this, &CustomTrackView::slotSelectItem);
        m_selectionGroup->addItem(item);

        QList <GenTime> offsetList;
        offsetList.append(info.endPos);
        updateSnapPoints(NULL, offsetList);

        m_selectionGroup->setProperty("locked_tracks", QVariant::fromValue(lockedTracks));
        m_selectionGroup->setPos(framePos);
        scene()->addItem(m_selectionGroup);
        m_selectionGroup->setSelected(true);
    } else if (data->hasFormat(QStringLiteral("kdenlive/producerslist"))) {
        QStringList ids = QString(data->data(QStringLiteral("kdenlive/producerslist"))).split(';');
        QList <GenTime> offsetList;
        QList <ItemInfo> infoList;
        GenTime start = GenTime((int)(framePos.x() + 0.5), m_document->fps());
        framePos.setX((int)(framePos.x() + 0.5));
        framePos.setY(getPositionFromTrack(track));

        // Check if user dragged a folder
        for (int i = 0; i < ids.size(); ++i) {
            if (ids.at(i).startsWith(QLatin1String("#"))) {
                QString folderId = ids.at(i);
                folderId.remove(0, 1);
                QStringList clipsInFolder = m_document->getBinFolderClipIds(folderId);
                ids.removeAt(i);
                for (int j = 0; j < clipsInFolder.count(); j++) {
                    ids.insert(i + j, clipsInFolder.at(j));
                }
            }
        }

        // Check if clips can be inserted at that position
	bool allowAudioOnly = false;
        for (int i = 0; i < ids.size(); ++i) {
            QString clipData = ids.at(i);
	    QString clipId = clipData.section('/', 0, 0);
	    ProjectClip *clip = m_document->getBinClip(clipId);
            if (!clip || !clip->isReady()) {
                emit displayMessage(i18n("Clip not ready"), ErrorMessage);
                return false;
            }
            ItemInfo info;
            info.startPos = start;
            if (clipData.contains('/')) {
                // this is a clip zone, set in / out
                int in = clipData.section('/', 1, 1).toInt();
                int out = clipData.section('/', 2, 2).toInt();
                info.cropStart = GenTime(in, m_document->fps());
                info.cropDuration = GenTime(out - in + 1, m_document->fps());
            }
            else {
                info.cropDuration = clip->duration();
            }
            info.endPos = info.startPos + info.cropDuration;
            info.track = track;
            infoList.append(info);
            start += info.cropDuration;
	    if (KdenliveSettings::splitaudio() && clip->clipType() == Audio)
		allowAudioOnly = true;
        }
        if (m_scene->editMode() == TimelineMode::NormalEdit && !canBePastedTo(infoList, AVWidget)) {
            return true;
        }
	QList<int> lockedTracks;
	bool locked = false;
	for (int i = 1; i < m_timeline->tracksCount(); ++i) {
	    TrackInfo nfo = m_timeline->getTrackInfo(i);
	    if (nfo.isLocked) {
		lockedTracks << i;
	    } else if (allowAudioOnly && nfo.type == VideoTrack) {
		if (track == i) {
		    locked = true;
		}
		lockedTracks << i;
	    }
	}
	if (locked)
	    return false;
        if (ids.size() > 1) {
            m_selectionGroup = new AbstractGroupItem(m_document->fps());
        }
        start = GenTime();
        for (int i = 0; i < ids.size(); ++i) {
            QString clipData = ids.at(i);
	    QString clipId = clipData.section('/', 0, 0);
            ProjectClip *clip = m_document->getBinClip(clipId);
            ItemInfo info;
            info.startPos = start;
            if (clipData.contains('/')) {
                // this is a clip zone, set in / out
                int in = clipData.section('/', 1, 1).toInt();
                int out = clipData.section('/', 2, 2).toInt();
                info.cropStart = GenTime(in, m_document->fps());
                info.cropDuration = GenTime(out - in + 1, m_document->fps());
            }
            else {
                info.cropDuration = clip->duration();
            }
            info.endPos = info.startPos + info.cropDuration;
            info.track = 0;
            start += info.cropDuration;
            offsetList.append(start);
            ClipItem *item = new ClipItem(clip, info, m_document->fps(), 1.0, 1, getFrameWidth(), true);
            connect(item, &AbstractClipItem::selectItem, this, &CustomTrackView::slotSelectItem);
            item->setPos(info.startPos.frames(m_document->fps()), item->itemOffset());
            if (ids.size() > 1) m_selectionGroup->addItem(item);
            else {
                m_dragItem = item;
                m_dragItem->setMainSelectedClip(true);
            }
            item->setSelected(true);
        }

        updateSnapPoints(NULL, offsetList);

        if (m_selectionGroup) {
            m_selectionGroup->setProperty("locked_tracks", QVariant::fromValue(lockedTracks));
            scene()->addItem(m_selectionGroup);
            m_selectionGroup->setPos(framePos);
        }
        else if (m_dragItem) {
            m_dragItem->setProperty("locked_tracks", QVariant::fromValue(lockedTracks));
            scene()->addItem(m_dragItem);
            m_dragItem->setPos(framePos);
        }
        //m_selectionGroup->setZValue(10);
    }
    return true;
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
    } else {
        QGraphicsView::dragEnterEvent(event);
    }
}

bool CustomTrackView::itemCollision(AbstractClipItem *item, const ItemInfo &newPos)
{
    QRectF shape = QRectF(newPos.startPos.frames(m_document->fps()), getPositionFromTrack(newPos.track) + 1, (newPos.endPos - newPos.startPos).frames(m_document->fps()) - 0.02, m_tracksHeight - 1);
    QList<QGraphicsItem*> collindingItems = scene()->items(shape, Qt::IntersectsItemShape);
    collindingItems.removeAll(item);
    if (collindingItems.isEmpty()) {
        return false;
    } else {
        for (int i = 0; i < collindingItems.count(); ++i) {
            QGraphicsItem *collision = collindingItems.at(i);
            if (collision->type() == item->type()) {
                // Collision
                //qDebug() << "// COLLISIION DETECTED";
                return true;
            }
        }
        return false;
    }
}

void CustomTrackView::slotRefreshEffects(ClipItem *clip)
{
    // Does not seem used anywhere...
    int track = clip->track();
    GenTime pos = clip->startPos();
    if (!m_timeline->track(track)->removeEffect(pos.seconds(), -1, false)) {
        emit displayMessage(i18n("Problem deleting effect"), ErrorMessage);
        return;
    }
    bool success = true;
    for (int i = 0; i < clip->effectsCount(); ++i) {
        if (!m_timeline->track(track)->addEffect(pos.seconds(), EffectsController::getEffectArgs(m_document->getProfileInfo(), clip->effect(i)))) success = false;
    }
    if (!success) emit displayMessage(i18n("Problem adding effect to clip"), ErrorMessage);
    if (clip->hasVisibleVideo())
        monitorRefresh(clip->info());
}

void CustomTrackView::addEffect(int track, GenTime pos, QDomElement effect)
{
    if (pos < GenTime()) {
        // Add track effect
        if (effect.attribute(QStringLiteral("id")) == QLatin1String("speed")) {
            emit displayMessage(i18n("Cannot add speed effect to track"), ErrorMessage);
            return;
        }
        clearSelection();
        m_timeline->addTrackEffect(track, effect);
        if (effect.attribute(QStringLiteral("type")) != QLatin1String("audio"))
            monitorRefresh();
        emit updateTrackEffectState(track);
        emit showTrackEffects(track, m_timeline->getTrackInfo(track));
        return;
    }
    ClipItem *clip = getClipItemAtStart(pos, track);
    if (clip) {
        // Special case: speed effect
        if (effect.attribute(QStringLiteral("id")) == QLatin1String("speed")) {
            QLocale locale;
            locale.setNumberOptions(QLocale::OmitGroupSeparator);
            double speed = locale.toDouble(EffectsList::parameter(effect, QStringLiteral("speed"))) / 100.0;
            int strobe = EffectsList::parameter(effect, QStringLiteral("strobe")).toInt();
            if (strobe == 0) strobe = 1;
            doChangeClipSpeed(clip->info(), clip->speedIndependantInfo(), clip->clipState(), speed, strobe, clip->getBinId());
            EffectsParameterList params = clip->addEffect(m_document->getProfileInfo(), effect);
            if (clip->isSelected()) emit clipItemSelected(clip);
            monitorRefresh(clip->info(), true);
            return;
        }
        EffectsParameterList params = clip->addEffect(m_document->getProfileInfo(), effect);
        if (!m_timeline->track(track)->addEffect(pos.seconds(), params)) {
            emit displayMessage(i18n("Problem adding effect to clip"), ErrorMessage);
            clip->deleteEffect(params.paramValue(QStringLiteral("kdenlive_ix")).toInt());
        }
        else {
            clip->setSelectedEffect(params.paramValue(QStringLiteral("kdenlive_ix")).toInt());
            if (clip->hasVisibleVideo() && effect.attribute(QStringLiteral("type")) != QLatin1String("audio"))
                monitorRefresh(clip->info(), true);
        }
        if (clip->isMainSelectedClip()) emit clipItemSelected(clip);
    } else emit displayMessage(i18n("Cannot find clip to add effect"), ErrorMessage);
}

void CustomTrackView::deleteEffect(int track, const GenTime &pos, const QDomElement &effect)
{
    int index = effect.attribute(QStringLiteral("kdenlive_ix")).toInt();
    if (pos < GenTime()) {
        // Delete track effect
        if (!m_timeline->removeTrackEffect(track, index, effect)) {
            emit displayMessage(i18n("Problem deleting effect"), ErrorMessage);
        }
        emit updateTrackEffectState(track);
        if (effect.attribute(QStringLiteral("type")) != QLatin1String("audio"))
            monitorRefresh();
        emit showTrackEffects(track, m_timeline->getTrackInfo(track));
        return;
    }
    // Special case: speed effect
    if (effect.attribute(QStringLiteral("id")) == QLatin1String("speed")) {
        ClipItem *clip = getClipItemAtStart(pos, track);
        if (clip) {
            doChangeClipSpeed(clip->info(), clip->speedIndependantInfo(), clip->clipState(), 1.0, 1, clip->getBinId(), true);
            clip->deleteEffect(index);
            monitorRefresh(clip->info());
            emit clipItemSelected(clip);
            return;
        }
    }
    if (!m_timeline->track(track)->removeEffect(pos.seconds(), index, true)) {
        //qDebug() << "// ERROR REMOV EFFECT: " << index << ", DISABLE: " << effect.attribute("disable");
        emit displayMessage(i18n("Problem deleting effect"), ErrorMessage);
        return;
    }
    ClipItem *clip = getClipItemAtStart(pos, track);
    if (clip) {
        if (clip->deleteEffect(index) && clip->hasVisibleVideo()) {
            monitorRefresh(clip->info(), true);
        }
        if (clip->isMainSelectedClip()) emit clipItemSelected(clip);
    }
}

void CustomTrackView::slotAddGroupEffect(QDomElement effect, AbstractGroupItem *group, AbstractClipItem *dropTarget)
{
    QList<QGraphicsItem *> itemList = group->childItems();
    QUndoCommand *effectCommand = new QUndoCommand();
    QString effectName;
    int offset = effect.attribute(QStringLiteral("clipstart")).toInt();
    QDomElement namenode = effect.firstChildElement(QStringLiteral("name"));
    if (!namenode.isNull()) effectName = i18n(namenode.text().toUtf8().data());
    else effectName = i18n("effect");
    effectCommand->setText(i18n("Add %1", effectName));
    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == GroupWidget) {
            itemList << itemList.at(i)->childItems();
        }
        if (itemList.at(i)->type() == AVWidget) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            if (effect.tagName() == QLatin1String("effectgroup")) {
                QDomNodeList effectlist = effect.elementsByTagName(QStringLiteral("effect"));
                for (int j = 0; j < effectlist.count(); ++j) {
                    QDomElement subeffect = effectlist.at(j).toElement();
                    if (subeffect.hasAttribute(QStringLiteral("kdenlive_info"))) {
                        // effect is in a group
                        EffectInfo effectInfo;
                        effectInfo.fromString(subeffect.attribute(QStringLiteral("kdenlive_info")));
                        if (effectInfo.groupIndex < 0) {
                            // group needs to be appended
                            effectInfo.groupIndex = item->nextFreeEffectGroupIndex();
                            subeffect.setAttribute(QStringLiteral("kdenlive_info"), effectInfo.toString());
                        }
                    }
                    processEffect(item, subeffect.cloneNode().toElement(), offset, effectCommand);
                }
            }
            else {
                processEffect(item, effect.cloneNode().toElement(), offset, effectCommand);
            }
        }
    }
    if (effectCommand->childCount() > 0) {
        m_commandStack->push(effectCommand);
    } else delete effectCommand;
    if (dropTarget) {
        clearSelection(false);
        m_dragItem = dropTarget;
        m_dragItem->setSelected(true);
        m_dragItem->setMainSelectedClip(true);
        emit clipItemSelected(static_cast<ClipItem *>(dropTarget));
    }
}

void CustomTrackView::slotAddEffect(ClipItem *clip, const QDomElement &effect, int track)
{
    if (clip == NULL) {
        // delete track effect
        AddEffectCommand *command = new AddEffectCommand(this, track, GenTime(-1), effect, true);
        m_commandStack->push(command);
        return;
    }
    else slotAddEffect(effect, clip->startPos(), clip->track());
}

void CustomTrackView::slotDropEffect(ClipItem *clip, QDomElement effect, GenTime pos, int track)
{
    if (clip == NULL) return;
    slotAddEffect(effect, pos, track);
    if (clip->parentItem()) {
        // Clip is in a group, should not happen
        //qDebug()<<"/// DROPPED ON ITEM IN GRP";
    }
    else if (clip != m_dragItem) {
        clearSelection(false);
        m_dragItem = clip;
        m_dragItem->setMainSelectedClip(true);
        clip->setSelected(true);
        emit clipItemSelected(clip);
    }
}

void CustomTrackView::slotSelectItem(AbstractClipItem *item)
{
    clearSelection(false);
    m_dragItem = item;
    m_dragItem->setMainSelectedClip(true);
    item->setSelected(true);
    if (item->type() == AVWidget)
        emit clipItemSelected((ClipItem*)item);
    else if (item->type() == TransitionWidget)
        emit transitionItemSelected((Transition*)item);
}

void CustomTrackView::slotDropTransition(ClipItem *clip, QDomElement transition, QPointF scenePos)
{
    if (clip == NULL) return;
    m_menuPosition = mapFromScene(scenePos);
    slotAddTransitionToSelectedClips(transition, QList<QGraphicsItem *>() << clip);
    m_menuPosition = QPoint();
}

void CustomTrackView::removeSplitOverlay()
{
    m_timeline->removeSplitOverlay();
}

bool CustomTrackView::createSplitOverlay(Mlt::Filter *filter)
{
    if (!m_dragItem || m_dragItem->type() != AVWidget) {
        //TODO manage split clip
        return false;
    }
    return m_timeline->createOverlay(filter, m_dragItem->track(), m_dragItem->startPos().frames(m_document->fps()));
}

void CustomTrackView::rippleMode(bool enable)
{
    if (!enable) {
        m_timeline->removeSplitOverlay();
        emit loadMonitorScene(MonitorSceneDefault, false);
        monitorRefresh();
        return;
    }
    if (!m_dragItem || m_dragItem->type() != AVWidget) {
        qDebug()<<"* * * * *RIPPLEMODE ERROR";
        emit displayMessage(i18n("Select a clip to enter ripple mode"), InformationMessage);
        return;
    }
    if (m_operationMode == ResizeEnd)
        m_moveOpMode = RollingEnd;
    else if (m_operationMode == ResizeStart)
        m_moveOpMode = RollingStart;
    int ripplePos = m_moveOpMode == RollingStart ? m_dragItem->startPos().frames(m_document->fps()) : m_dragItem->endPos().frames(m_document->fps());
    if (m_timeline->createRippleWindow(m_dragItem->track(), ripplePos)) {
        emit loadMonitorScene(MonitorSceneRipple, true);
        monitorRefresh();
    }
}

void CustomTrackView::slotAddEffectToCurrentItem(QDomElement effect)
{
    slotAddEffect(effect, GenTime(), -1);
}

void CustomTrackView::slotAddEffect(QDomElement effect, const GenTime &pos, int track)
{
    QList<QGraphicsItem *> itemList;
    QUndoCommand *effectCommand = new QUndoCommand();
    QString effectName;
    int offset = effect.attribute(QStringLiteral("clipstart")).toInt();
    if (effect.tagName() == QLatin1String("effectgroup")) {
        effectName = effect.attribute(QStringLiteral("name"));
    } else {
        QDomElement namenode = effect.firstChildElement(QStringLiteral("name"));
        if (!namenode.isNull()) effectName = i18n(namenode.text().toUtf8().data());
        else effectName = i18n("effect");
    }
    effectCommand->setText(i18n("Add %1", effectName));

    if (track == -1) {
        itemList = scene()->selectedItems();
    }
    else if (itemList.isEmpty()) {
        ClipItem *clip = getClipItemAtStart(pos, track);
        if (clip) itemList.append(clip);
    }
    if (itemList.isEmpty()) {
        emit displayMessage(i18n("Select a clip if you want to apply an effect"), ErrorMessage);
    }

    //expand groups
    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == GroupWidget) {
            QList<QGraphicsItem *> subitems = itemList.at(i)->childItems();
            for (int j = 0; j < subitems.count(); ++j) {
                if (!itemList.contains(subitems.at(j))) itemList.append(subitems.at(j));
            }
        }
    }

    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == AVWidget) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            if (effect.tagName() == QLatin1String("effectgroup")) {
                QDomNodeList effectlist = effect.elementsByTagName(QStringLiteral("effect"));
                for (int j = 0; j < effectlist.count(); ++j) {
                    QDomElement subeffect = effectlist.at(j).toElement();
                    if (subeffect.hasAttribute(QStringLiteral("kdenlive_info"))) {
                        // effect is in a group
                        EffectInfo effectInfo;
                        effectInfo.fromString(subeffect.attribute(QStringLiteral("kdenlive_info")));
                        if (effectInfo.groupIndex < 0) {
                            // group needs to be appended
                            effectInfo.groupIndex = item->nextFreeEffectGroupIndex();
                            subeffect.setAttribute(QStringLiteral("kdenlive_info"), effectInfo.toString());
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
    } else delete effectCommand;
}

void CustomTrackView::processEffect(ClipItem *item, QDomElement effect, int offset, QUndoCommand *effectCommand)
{
    if (effect.attribute(QStringLiteral("type")) == QLatin1String("audio")) {
        // Don't add audio effects on video clips
        if (item->clipState() == PlaylistState::VideoOnly || (item->clipType() != Audio && item->clipType() != AV && item->clipType() != Playlist)) {
            /* do not show error message when item is part of a group as the user probably knows what he does then
            * and the message is annoying when working with the split audio feature */
            if (!item->parentItem() || item->parentItem() == m_selectionGroup)
                emit displayMessage(i18n("Cannot add an audio effect to this clip"), ErrorMessage);
            return;
        }
    } else if (effect.attribute(QStringLiteral("type")) == QLatin1String("video") || !effect.hasAttribute(QStringLiteral("type"))) {
        // Don't add video effect on audio clips
        if (item->clipState() == PlaylistState::AudioOnly || item->clipType() == Audio) {
            /* do not show error message when item is part of a group as the user probably knows what he does then
            * and the message is annoying when working with the split audio feature */
            if (!item->parentItem() || item->parentItem() == m_selectionGroup)
                emit displayMessage(i18n("Cannot add a video effect to this clip"), ErrorMessage);
            return;
        }
    }
    if (effect.attribute(QStringLiteral("unique"), QStringLiteral("0")) != QLatin1String("0") && item->hasEffect(effect.attribute(QStringLiteral("tag")), effect.attribute(QStringLiteral("id"))) != -1) {
        emit displayMessage(i18n("Effect already present in clip"), ErrorMessage);
        return;
    }
    if (item->isItemLocked()) {
        return;
    }

    if (effect.attribute(QStringLiteral("id")) == QLatin1String("freeze") && m_cursorPos > item->startPos().frames(m_document->fps()) && m_cursorPos < item->endPos().frames(m_document->fps())) {
        item->initEffect(m_document->getProfileInfo() , effect, m_cursorPos - item->startPos().frames(m_document->fps()), offset);
    } else {
        item->initEffect(m_document->getProfileInfo() , effect, 0, offset);
    }
    new AddEffectCommand(this, item->track(), item->startPos(), effect, true, effectCommand);
}

void CustomTrackView::slotDeleteEffectGroup(ClipItem *clip, int track, QDomDocument doc, bool affectGroup)
{
    QUndoCommand *delCommand = new QUndoCommand();
    QString effectName = doc.documentElement().attribute(QStringLiteral("name"));
    delCommand->setText(i18n("Delete %1", effectName));
    QDomNodeList effects = doc.elementsByTagName(QStringLiteral("effect"));
    for (int i = 0; i < effects.count(); ++i) {
        slotDeleteEffect(clip, track, effects.at(i).toElement(), affectGroup, delCommand);
    }
    m_commandStack->push(delCommand);
}

void CustomTrackView::slotDeleteEffect(ClipItem *clip, int track, QDomElement effect, bool affectGroup, QUndoCommand *parentCommand)
{
    if (clip == NULL) {
        // delete track effect
        AddEffectCommand *command = new AddEffectCommand(this, track, GenTime(-1), effect, false, parentCommand);
        if (parentCommand == NULL) 
            m_commandStack->push(command);
        return;
    }
    AddEffectCommand *command = NULL;
    if (affectGroup && clip->parentItem() && clip->parentItem() == m_selectionGroup) {
        //clip is in a group, also remove the effect in other clips of the group
        QList<QGraphicsItem *> items = m_selectionGroup->childItems();
        QUndoCommand *delCommand = parentCommand == NULL ? new QUndoCommand() : parentCommand;
        QString effectName;
        QDomElement namenode = effect.firstChildElement(QStringLiteral("name"));
        if (!namenode.isNull()) effectName = i18n(namenode.text().toUtf8().data());
        else effectName = i18n("effect");
        delCommand->setText(i18n("Delete %1", effectName));

        //expand groups
        for (int i = 0; i < items.count(); ++i) {
            if (items.at(i)->type() == GroupWidget) {
                QList<QGraphicsItem *> subitems = items.at(i)->childItems();
                for (int j = 0; j < subitems.count(); ++j) {
                    if (!items.contains(subitems.at(j))) items.append(subitems.at(j));
                }
            }
        }

        for (int i = 0; i < items.count(); ++i) {
            if (items.at(i)->type() == AVWidget) {
                ClipItem *item = static_cast <ClipItem *>(items.at(i));
                int ix = item->hasEffect(effect.attribute(QStringLiteral("tag")), effect.attribute(QStringLiteral("id")));
                if (ix != -1) {
                    QDomElement eff = item->effectAtIndex(ix);
                    new AddEffectCommand(this, item->track(), item->startPos(), eff, false, delCommand);
                }
            }
        }
        if (parentCommand == NULL) {
            if (delCommand->childCount() > 0)
                m_commandStack->push(delCommand);
            else
                delete delCommand;
        }
        return;
    } else {
        command = new AddEffectCommand(this, clip->track(), clip->startPos(), effect, false, parentCommand);
    }
    if (parentCommand == NULL) {
        m_commandStack->push(command);
    }
}

void CustomTrackView::updateEffect(int track, GenTime pos, QDomElement insertedEffect, bool updateEffectStack, bool replaceEffect, bool refreshMonitor)
{
    if (insertedEffect.isNull()) {
        //qDebug()<<"// Trying to add null effect";
        emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
        return;
    }
    int ix = insertedEffect.attribute(QStringLiteral("kdenlive_ix")).toInt();
    QDomElement effect = insertedEffect.cloneNode().toElement();
    //qDebug() << "// update effect ix: " << effect.attribute("kdenlive_ix")<<", TAG: "<< insertedEffect.attribute("tag");
    if (pos < GenTime()) {
        // editing a track effect
        EffectsParameterList effectParams = EffectsController::getEffectArgs(m_document->getProfileInfo(), effect);
        // check if we are trying to reset a keyframe effect
        /*if (effectParams.hasParam("keyframes") && effectParams.paramValue("keyframes").isEmpty()) {
            clip->initEffect(m_document->getProfileInfo() , effect);
            effectParams = EffectsController::getEffectArgs(effect);
        }*/
        if (!m_timeline->track(track)->editTrackEffect(effectParams, replaceEffect)) {
            emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
        }
        m_timeline->setTrackEffect(track, ix, effect);
        if (refreshMonitor && effect.attribute(QStringLiteral("type")) != QLatin1String("audio"))
            monitorRefresh();
        emit updateTrackEffectState(track);
        return;

    }
    ClipItem *clip = getClipItemAtStart(pos, track);
    if (clip) {
        // Special case: speed effect
        if (effect.attribute(QStringLiteral("id")) == QLatin1String("speed")) {
            if (effect.attribute(QStringLiteral("disable")) == QLatin1String("1")) {
                doChangeClipSpeed(clip->info(), clip->speedIndependantInfo(), clip->clipState(), 1.0, 1, clip->getBinId());
            } else {
                QLocale locale;
                locale.setNumberOptions(QLocale::OmitGroupSeparator);
                double speed = locale.toDouble(EffectsList::parameter(effect, QStringLiteral("speed"))) / 100.0;
                int strobe = EffectsList::parameter(effect, QStringLiteral("strobe")).toInt();
                if (strobe == 0) strobe = 1;
                doChangeClipSpeed(clip->info(), clip->speedIndependantInfo(), clip->clipState(), speed, strobe, clip->getBinId());
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

        EffectsParameterList effectParams = EffectsController::getEffectArgs(m_document->getProfileInfo(), effect);

        // check if we are trying to reset a keyframe effect
        if (effectParams.hasParam(QStringLiteral("keyframes")) && effectParams.paramValue(QStringLiteral("keyframes")).isEmpty()) {
            clip->initEffect(m_document->getProfileInfo() , effect);
            effectParams = EffectsController::getEffectArgs(m_document->getProfileInfo(), effect);
        }

        // Check if a fade effect was changed
        QString effectId = effect.attribute(QStringLiteral("id"));
        if (effectId == QLatin1String("fadein") || effectId == QLatin1String("fade_from_black") || effectId == QLatin1String("fadeout") || effectId == QLatin1String("fade_to_black")) {
            clip->setSelectedEffect(clip->selectedEffectIndex());
        }

        bool success = m_timeline->track(clip->track())->editEffect(clip->startPos().seconds(), effectParams, replaceEffect);
        if (success) {
            clip->updateEffect(effect);
            if (refreshMonitor && clip->hasVisibleVideo() && effect.attribute(QStringLiteral("type")) != QLatin1String("audio"))
                monitorRefresh(clip->info(), true);
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
}

void CustomTrackView::updateEffectState(int track, GenTime pos, QList <int> effectIndexes, bool disable, bool updateEffectStack)
{
    if (pos < GenTime()) {
        // editing a track effect
        if (!m_timeline->track(track)->enableTrackEffects(effectIndexes, disable)) {
            emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
            return;
        }
        if (m_timeline->enableTrackEffects(track, effectIndexes, disable))
            monitorRefresh();
        emit updateTrackEffectState(track);
        emit showTrackEffects(track, m_timeline->getTrackInfo(track));
        return;
    }
    // editing a clip effect
    ClipItem *clip = getClipItemAtStart(pos, track);
    if (clip) {
        bool success = m_timeline->track(clip->track())->enableEffects(clip->startPos().seconds(), effectIndexes, disable);
        if (success) {
            if (clip->enableEffects(effectIndexes, disable) && clip->hasVisibleVideo())
                monitorRefresh(clip->info(), true);
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

void CustomTrackView::moveEffect(int track, const GenTime &pos, const QList <int> &oldPos, const QList <int> &newPos)
{
    if (pos < GenTime()) {
        // Moving track effect
        int max = m_timeline->getTrackEffects(track).count();
        int new_position = newPos.at(0);
        if (new_position > max) {
            new_position = max;
        }
        int old_position = oldPos.at(0);
        for (int i = 0; i < newPos.count(); ++i) {
            QDomElement act = m_timeline->getTrackEffect(track, new_position);
            if (old_position > new_position) {
                // Moving up, we need to adjust index
                old_position = oldPos.at(i);
                new_position = newPos.at(i);
            }
            QDomElement before = m_timeline->getTrackEffect(track, old_position);
            if (!act.isNull() && !before.isNull()) {
                m_timeline->setTrackEffect(track, new_position, before);
                m_timeline->track(track)->moveTrackEffect(old_position, new_position);
                if (before.attribute(QStringLiteral("type")) != QLatin1String("audio")) {
                    monitorRefresh();
                    m_timeline->invalidateTrack(track);
                }
            } else emit displayMessage(i18n("Cannot move effect"), ErrorMessage);
        }
        emit showTrackEffects(track, m_timeline->getTrackInfo(track));
        return;
    }
    ClipItem *clip = getClipItemAtStart(pos, track);
    if (clip) {
        int new_position = newPos.at(0);
        if (new_position > clip->effectsCount()) {
            new_position = clip->effectsCount();
        }
        int old_position = oldPos.at(0);
        for (int i = 0; i < newPos.count(); ++i) {
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
            if (clip->hasEffect(QStringLiteral("timewarp"), QStringLiteral("speed")) != -1) {
                // special case, speed effect is not in MLT's filter list, so decrease indexes
                old_position--;
                new_position--;
            }
            // special case: speed effect, which is a pseudo-effect, not appearing in MLT's effects
            m_timeline->track(track)->moveEffect(pos.seconds(), old_position, new_position);
            if (clip->hasVisibleVideo() && before.attribute(QStringLiteral("type")) != QLatin1String("audio"))
                monitorRefresh(clip->info(), true);
        }
        clip->setSelectedEffect(newPos.at(0));
        emit clipItemSelected(clip);
    } else emit displayMessage(i18n("Cannot move effect"), ErrorMessage);
}

void CustomTrackView::slotChangeEffectState(ClipItem *clip, int track, QList <int> effectIndexes, bool disable)
{
    ChangeEffectStateCommand *command;
    if (clip == NULL) {
        // editing track effect
        command = new ChangeEffectStateCommand(this, track, GenTime(-1), effectIndexes, disable, false, true);
    } else {
        // Check if we have a speed effect, disabling / enabling it needs a special procedure since it is a pseudoo effect
        QList <int> speedEffectIndexes;
        for (int i = 0; i < effectIndexes.count(); ++i) {
            QDomElement effect = clip->effectAtIndex(effectIndexes.at(i));
            if (effect.attribute(QStringLiteral("id")) == QLatin1String("speed")) {
                // speed effect
                speedEffectIndexes << effectIndexes.at(i);
                QDomElement newEffect = effect.cloneNode().toElement();
                newEffect.setAttribute(QStringLiteral("disable"), (int) disable);
                EditEffectCommand *editcommand = new EditEffectCommand(this, clip->track(), clip->startPos(), effect, newEffect, effectIndexes.at(i), false, true, true);
                m_commandStack->push(editcommand);
            }
        }
        for (int j = 0; j < speedEffectIndexes.count(); ++j) {
            effectIndexes.removeAll(speedEffectIndexes.at(j));
        }
        command = new ChangeEffectStateCommand(this, clip->track(), clip->startPos(), effectIndexes, disable, false, true);
    }
    m_commandStack->push(command);
}

void CustomTrackView::slotChangeEffectPosition(ClipItem *clip, int track, QList <int> currentPos, int newPos)
{
    MoveEffectCommand *command;
    if (clip == NULL) {
        // editing track effect
        command = new MoveEffectCommand(this, track, GenTime(-1), currentPos, newPos);
    } else command = new MoveEffectCommand(this, clip->track(), clip->startPos(), currentPos, newPos);
    m_commandStack->push(command);
}

void CustomTrackView::slotUpdateClipEffect(ClipItem *clip, int track, QDomElement oldeffect, QDomElement effect, int ix, bool refreshEffectStack)
{
    EditEffectCommand *command;
    if (clip) command = new EditEffectCommand(this, clip->track(), clip->startPos(), oldeffect.cloneNode().toElement(), effect.cloneNode().toElement(), ix, refreshEffectStack, true, true);
    else command = new EditEffectCommand(this, track, GenTime(-1), oldeffect.cloneNode().toElement(), effect.cloneNode().toElement(), ix, refreshEffectStack, true, true);
    m_commandStack->push(command);
}

void CustomTrackView::slotUpdateClipRegion(ClipItem *clip, int ix, QString region)
{
    QDomElement effect = clip->getEffectAtIndex(ix);
    QDomElement oldeffect = effect.cloneNode().toElement();
    effect.setAttribute(QStringLiteral("region"), region);
    EditEffectCommand *command = new EditEffectCommand(this, clip->track(), clip->startPos(), oldeffect, effect, ix, true, true, true);
    m_commandStack->push(command);
}

void CustomTrackView::cutClip(const ItemInfo &info, const GenTime &cutTime, bool cut, const EffectsList &oldStack, bool execute)
{
    if (cut) {
        // cut clip
        ClipItem *item = getClipItemAtStart(info.startPos, info.track, info.endPos);
        bool selectDup = false;
        ItemInfo selectedInfo;
        if (m_dragItem && m_dragItem->type() == AVWidget)
            selectedInfo = m_dragItem->info();
        if (item == m_dragItem) {
            clearSelection();
            selectDup = true;
        }
        if (!item || !info.contains(cutTime)) {
             emit displayMessage(i18n("Cannot find clip to cut"), ErrorMessage);
             return;
        }
        if (execute) {
            if (!m_timeline->track(info.track)->cut(cutTime.seconds())) {
                // Error cuting clip in playlist
                qDebug() << "/// ERROR CUTTING CLIP PLAYLIST!!";
                return;
            }
        }
        if (!selectDup && info.track == selectedInfo.track && (selectedInfo.contains(info.startPos) || selectedInfo.contains(info.endPos))) {
            clearSelection();
        }
        delete item;
        m_timeline->reloadTrack(info, false);

        // remove unwanted effects
        // fade in from 2nd part of the clip
        item = getClipItemAtStart(info.startPos, info.track, cutTime);
        if (!item)
            qDebug()<<"* * * CANNOT FIND CUT SRC CLIP AT: "<<info.startPos.frames(25);
        ClipItem *dup = getClipItemAtStart(cutTime, info.track, info.endPos);
        int ix = -1;
        if (!dup) {
            qDebug()<<"* * * CANNOT FIND CUT CLIP AT: "<<cutTime.frames(25);
        } else {
            dup->binClip()->addRef();
            ix = dup->hasEffect(QString(), QStringLiteral("fadein"));
            if (ix != -1) {
                QDomElement oldeffect = dup->effectAtIndex(ix);
                dup->deleteEffect(oldeffect.attribute(QStringLiteral("kdenlive_ix")).toInt());
            }
            ix = dup->hasEffect(QString(), QStringLiteral("fade_from_black"));
            if (ix != -1) {
                QDomElement oldeffect = dup->effectAtIndex(ix);
                dup->deleteEffect(oldeffect.attribute(QStringLiteral("kdenlive_ix")).toInt());
            }
        }
        if (item) {
            // fade out from 1st part of the clip
            ix = item->hasEffect(QString(), QStringLiteral("fadeout"));
            if (ix != -1) {
                QDomElement oldeffect = item->effectAtIndex(ix);
                item->deleteEffect(oldeffect.attribute(QStringLiteral("kdenlive_ix")).toInt());
            }
            ix = item->hasEffect(QString(), QStringLiteral("fade_to_black"));
            if (ix != -1) {
                QDomElement oldeffect = item->effectAtIndex(ix);
                item->deleteEffect(oldeffect.attribute(QStringLiteral("kdenlive_ix")).toInt());
            }

            if (item->checkKeyFrames(m_document->width(), m_document->height(), (info.cropDuration + info.cropStart).frames(m_document->fps())))
                slotRefreshEffects(item);
        }
        if (dup) {
            if (dup->checkKeyFrames(m_document->width(), m_document->height(), (info.cropDuration + info.cropStart).frames(m_document->fps()), (cutTime - info.startPos).frames(m_document->fps())))
                slotRefreshEffects(dup);
            if (selectDup) {
                dup->setSelected(true);
                dup->setMainSelectedClip(true);
                m_dragItem = dup;
                emit clipItemSelected(dup);
            } else if (selectedInfo.isValid() && m_dragItem == NULL) {
                m_dragItem = getClipItemAtStart(selectedInfo.startPos, selectedInfo.track);
                if (m_dragItem) {
                    m_dragItem->setSelected(true);
                    m_dragItem->setMainSelectedClip(true);
                    emit clipItemSelected(static_cast<ClipItem *>(m_dragItem));
                }
            }
        }
        return;
    } else {
        // uncut clip
        ClipItem *item = getClipItemAtStart(info.startPos, info.track);
        ClipItem *dup = getClipItemAtStart(cutTime, info.track);
        bool selectDup = false;
        if (m_dragItem == item || m_dragItem == dup) {
            emit clipItemSelected(NULL);
            selectDup = true;
        }
        if (!item || !dup || item == dup) {
            emit displayMessage(i18n("Cannot find clip to uncut"), ErrorMessage);
            return;
        }
        if (!m_timeline->track(info.track)->del(cutTime.seconds())) {
            emit displayMessage(i18n("Error removing clip at %1 on track %2", m_document->timecode().getTimecodeFromFrames(cutTime.frames(m_document->fps())), m_timeline->getTrackInfo(info.track).trackName), ErrorMessage);
            return;
        }
        dup->binClip()->removeRef();
        m_timeline->track(info.track)->resize(info.startPos.seconds(), (info.endPos - cutTime).seconds(), true);
        m_timeline->reloadTrack(info.track, info.startPos.frames(m_document->fps()), info.endPos.frames(m_document->fps()));
        item = getClipItemAtStart(info.startPos, info.track);
        // Restore original effects
        item->setEffectList(oldStack);
        if (selectDup) {
            item->setSelected(true);
            item->setMainSelectedClip(true);
            m_dragItem = item;
            emit clipItemSelected(item);
        }
    }
}

Transition *CustomTrackView::cutTransition(const ItemInfo &info, const GenTime &cutTime, bool cut, const QDomElement &oldStack, bool execute)
{
    if (cut) {
        // cut clip
        Transition *item = getTransitionItemAtStart(info.startPos, info.track);
        if (!item || cutTime >= item->endPos() || cutTime <= item->startPos()) {
            emit displayMessage(i18n("Cannot find transition to cut"), ErrorMessage);
            if (item)
                qDebug() << "/////////  ERROR CUTTING transition : (" << item->startPos().frames(25) << '-' << item->endPos().frames(25) << "), INFO: (" << info.startPos.frames(25) << '-' << info.endPos.frames(25) << ')' << ", CUT: " << cutTime.frames(25);
            else
                qDebug() << "/// ERROR NO transition at: " << info.startPos.frames(m_document->fps()) << ", track: " << info.track;
            return NULL;
        }

        bool success = true;
        if (execute) {
            success = m_timeline->transitionHandler->moveTransition(item->transitionTag(), info.track, info.track, item->transitionEndTrack(), info.startPos, info.endPos, info.startPos, cutTime);
            if (success) {
                success = m_timeline->transitionHandler->addTransition(item->transitionTag(), item->transitionEndTrack(), info.track, cutTime, info.endPos, item->toXML());
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
        Transition *dup = item->clone(newPos);
        connect(dup, &AbstractClipItem::selectItem, this, &CustomTrackView::slotSelectItem);
        dup->setPos(newPos.startPos.frames(m_document->fps()), getPositionFromTrack(newPos.track) + 1 + dup->itemOffset());

        item->resizeEnd(cutPos);
        scene()->addItem(dup);

        if (item->checkKeyFrames(m_document->width(), m_document->height(), (info.cropDuration + info.cropStart).frames(m_document->fps()))) {
            m_timeline->transitionHandler->updateTransitionParams(item->transitionTag(), item->transitionEndTrack(), info.track, info.startPos, cutTime, item->toXML());
        }
        if (dup->checkKeyFrames(m_document->width(), m_document->height(), (info.cropDuration + info.cropStart).frames(m_document->fps()), (cutTime - item->startPos()).frames(m_document->fps()))) {
            m_timeline->transitionHandler->updateTransitionParams(item->transitionTag(), item->transitionEndTrack(), info.track, cutTime, info.endPos, dup->toXML());
        }

        KdenliveSettings::setSnaptopoints(snap);
        return dup;
    } else {
        // uncut transition
        Transition *item = getTransitionItemAtStart(info.startPos, info.track);
        Transition *dup = getTransitionItemAtStart(cutTime, info.track);

        if (!item || !dup || item == dup) {
            emit displayMessage(i18n("Cannot find transition to uncut"), ErrorMessage);
            return NULL;
        }

        ItemInfo transitionInfo = dup->info();
        if (!m_timeline->transitionHandler->deleteTransition(dup->transitionTag(), dup->transitionEndTrack(), transitionInfo.track, cutTime, transitionInfo.endPos, dup->toXML(), false)) {
            emit displayMessage(i18n("Error removing transition at %1 on track %2", m_document->timecode().getTimecodeFromFrames(cutTime.frames(m_document->fps())), m_timeline->getTrackInfo(info.track).trackName), ErrorMessage);
            return NULL;
        }
        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);

        if (dup->isSelected() && dup == m_dragItem) {
            item->setSelected(true);
            item->setMainSelectedClip(true);
            m_dragItem = item;
            emit transitionItemSelected(item);
        }
        scene()->removeItem(dup);
        delete dup;
        dup = NULL;

        ItemInfo clipinfo = item->info();
        bool success = m_timeline->transitionHandler->moveTransition(item->transitionTag(), clipinfo.track, clipinfo.track, item->transitionEndTrack(), clipinfo.startPos, clipinfo.endPos, clipinfo.startPos, transitionInfo.endPos);

        if (success) {
            item->resizeEnd((int) info.endPos.frames(m_document->fps()));
            item->setTransitionParameters(oldStack);
            m_timeline->transitionHandler->updateTransitionParams(item->transitionTag(), item->transitionEndTrack(), info.track, info.startPos, info.endPos, oldStack);
        } else {
            emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
        }
        KdenliveSettings::setSnaptopoints(snap);
        return item;
    }
}

void CustomTrackView::slotAddTransitionToSelectedClips(QDomElement transition, QList<QGraphicsItem *> itemList)
{
    if (itemList.isEmpty()) itemList = scene()->selectedItems();
    if (itemList.count() == 1) {
        if (itemList.at(0)->type() == AVWidget) {
            ClipItem *item = static_cast<ClipItem*>(itemList.at(0));
            ItemInfo info;
            info.track = item->track();
            ClipItem *transitionClip = NULL;
            const int transitiontrack = getPreviousVideoTrack(info.track);
            GenTime pos = GenTime((int)(mapToScene(m_menuPosition).x()), m_document->fps());
            if (pos < item->startPos() + item->cropDuration() / 2) {
                // add transition to clip start
                info.startPos = item->startPos();
                if (transitiontrack != 0) transitionClip = getClipItemAtMiddlePoint(info.startPos.frames(m_document->fps()), transitiontrack);
                if (transitionClip && transitionClip->endPos() < item->endPos()) {
                    info.endPos = transitionClip->endPos();
                } else info.endPos = info.startPos + GenTime(m_document->getFramePos(KdenliveSettings::transition_duration()), m_document->fps());
                // Check there is no other transition at that place
		double startY = getPositionFromTrack(info.track) + 1 + m_tracksHeight / 2;
                QRectF r(info.startPos.frames(m_document->fps()), startY, (info.endPos - info.startPos).frames(m_document->fps()), m_tracksHeight / 2);
                QList<QGraphicsItem *> selection = m_scene->items(r);
                bool transitionAccepted = true;
                for (int i = 0; i < selection.count(); ++i) {
                    if (selection.at(i)->type() == TransitionWidget) {
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
                if (transitiontrack != 0) transitionClip = getClipItemAtMiddlePoint(info.endPos.frames(m_document->fps()), transitiontrack);
                if (transitionClip && transitionClip->startPos() > item->startPos()) {
                    info.startPos = transitionClip->startPos();
                } else {
                    GenTime duration(m_document->getFramePos(KdenliveSettings::transition_duration()), m_document->fps());
                    if (info.endPos < duration) {
                        info.startPos = GenTime();
                    } else {
                        info.startPos = info.endPos - duration;
                    }
                }
                if (transition.attribute(QStringLiteral("tag")) == QLatin1String("luma")) EffectsList::setParameter(transition, QStringLiteral("reverse"), QStringLiteral("1"));
                else if (transition.attribute(QStringLiteral("id")) == QLatin1String("slide")) EffectsList::setParameter(transition, QStringLiteral("invert"), QStringLiteral("1"));

                // Check there is no other transition at that place
                double startY = getPositionFromTrack(info.track) + 1 + m_tracksHeight / 2;
                QRectF r(info.startPos.frames(m_document->fps()), startY, (info.endPos - info.startPos).frames(m_document->fps()), m_tracksHeight / 2);
                QList<QGraphicsItem *> selection = m_scene->items(r);
                bool transitionAccepted = true;
                for (int i = 0; i < selection.count(); ++i) {
                    if (selection.at(i)->type() == TransitionWidget) {
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
    } else for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == AVWidget) {
            ClipItem *item = static_cast<ClipItem*>(itemList.at(i));
            ItemInfo info;
            info.startPos = item->startPos();
            info.endPos = info.startPos + GenTime(m_document->getFramePos(KdenliveSettings::transition_duration()), m_document->fps());
            info.track = item->track();

            // Check there is no other transition at that place
            double startY = getPositionFromTrack(info.track) + 1 + m_tracksHeight / 2;
            QRectF r(info.startPos.frames(m_document->fps()), startY, (info.endPos - info.startPos).frames(m_document->fps()), m_tracksHeight / 2);
            QList<QGraphicsItem *> selection = m_scene->items(r);
            bool transitionAccepted = true;
            for (int i = 0; i < selection.count(); ++i) {
                if (selection.at(i)->type() == TransitionWidget) {
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
}

void CustomTrackView::addTransition(const ItemInfo &transitionInfo, int endTrack, const QDomElement &params, bool refresh)
{
    Transition *tr = new Transition(transitionInfo, endTrack, m_document->fps(), params, true);
    connect(tr, &AbstractClipItem::selectItem, this, &CustomTrackView::slotSelectItem);
    tr->setPos(transitionInfo.startPos.frames(m_document->fps()), getPositionFromTrack(transitionInfo.track) + tr->itemOffset() + 1);
    ////qDebug() << "---- ADDING transition " << params.attribute("value");
    if (m_timeline->transitionHandler->addTransition(tr->transitionTag(), endTrack, transitionInfo.track, transitionInfo.startPos, transitionInfo.endPos, tr->toXML())) {
        scene()->addItem(tr);
        if (refresh)
            monitorRefresh(transitionInfo, true);
    } else {
        emit displayMessage(i18n("Cannot add transition"), ErrorMessage);
        delete tr;
    }
}

void CustomTrackView::deleteTransition(const ItemInfo &transitionInfo, int endTrack, QDomElement /*params*/, bool refresh)
{
    Transition *item = getTransitionItemAt(transitionInfo.startPos, transitionInfo.track);
    if (!item) {
        //TODO: rename to "Select transition to delete
        emit displayMessage(i18n("Select clip to delete"), ErrorMessage);
        return;
    }
    m_timeline->transitionHandler->deleteTransition(item->transitionTag(), endTrack, transitionInfo.track, transitionInfo.startPos, transitionInfo.endPos, item->toXML(), refresh);
    if (m_dragItem == item) {
        m_dragItem->setMainSelectedClip(false);
        m_dragItem = NULL;
    }
    if (refresh)
        monitorRefresh(transitionInfo, true);
    // animate item deletion
    item->closeAnimation();
    emit transitionItemSelected(NULL);
}

void CustomTrackView::slotTransitionUpdated(Transition *tr, QDomElement old)
{
    ////qDebug() << "TRANS UPDATE, TRACKS: " << old.attribute("transition_btrack") << ", NEW: " << tr->toXML().attribute("transition_btrack");
    QDomElement xml = tr->toXML();
    if (old.isNull() || xml.isNull()) {
        emit displayMessage(i18n("Cannot update transition"), ErrorMessage);
        return;
    }
    EditTransitionCommand *command = new EditTransitionCommand(this, tr->track(), tr->startPos(), old, xml, false);
    updateTrackDuration(tr->track(), command);
    m_commandStack->push(command);
}

void CustomTrackView::updateTransition(int track, const GenTime &pos, const QDomElement &oldTransition, const QDomElement &transition, bool updateTransitionWidget)
{
    Transition *item = getTransitionItemAt(pos, track);
    if (!item) {
        qWarning() << "Unable to find transition at pos :" << pos.frames(m_document->fps()) << ", ON track: " << track;
        return;
    }

    bool force = false;
    if (oldTransition.attribute(QStringLiteral("transition_atrack")) != transition.attribute(QStringLiteral("transition_atrack")) || oldTransition.attribute(QStringLiteral("transition_btrack")) != transition.attribute(QStringLiteral("transition_btrack")))
        force = true;
    m_timeline->transitionHandler->updateTransition(oldTransition.attribute(QStringLiteral("tag")), transition.attribute(QStringLiteral("tag")), transition.attribute(QStringLiteral("transition_btrack")).toInt(), transition.attribute(QStringLiteral("transition_atrack")).toInt(), item->startPos(), item->endPos(), transition, force);
    ////qDebug() << "ORIGINAL TRACK: "<< oldTransition.attribute("transition_btrack") << ", NEW TRACK: "<<transition.attribute("transition_btrack");
    item->setTransitionParameters(transition);
    if (updateTransitionWidget && item->isSelected()) {
        ItemInfo info = item->info();
        QPoint p;
        ClipItem *transitionClip = getClipItemAtStart(info.startPos, info.track);
        if (transitionClip && transitionClip->binClip()) {
            int frameWidth = transitionClip->binClip()->getProducerIntProperty(QStringLiteral("meta.media.width"));
            int frameHeight = transitionClip->binClip()->getProducerIntProperty(QStringLiteral("meta.media.height"));
            double factor = transitionClip->binClip()->getProducerProperty(QStringLiteral("aspect_ratio")).toDouble();
            if (factor == 0) factor = 1.0;
            p.setX((int)(frameWidth * factor + 0.5));
            p.setY(frameHeight);
        }
        emit transitionItemSelected(item, getPreviousVideoTrack(info.track), p, true);
    }
    monitorRefresh(item->info(), true);
}

void CustomTrackView::dragMoveEvent(QDragMoveEvent * event)
{
    if (m_clipDrag) {
        const QPointF pos = mapToScene(event->pos());
        if (m_selectionGroup) {
            m_selectionGroup->setPos(pos);
            emit mousePosition((int)(m_selectionGroup->scenePos().x() + 0.5));
            event->acceptProposedAction();
        } else if (m_dragItem) {
            m_dragItem->setPos(pos);
            emit mousePosition((int)(m_dragItem->scenePos().x() + 0.5));
            event->acceptProposedAction();
        } else {
            // Drag enter was not possible, try again at mouse position
            insertDropClips(event->mimeData(), event->pos());
            event->accept();
        }
    }
    else {
        QGraphicsView::dragMoveEvent(event);
    }
}

void CustomTrackView::dragLeaveEvent(QDragLeaveEvent * event)
{
    if ((m_selectionGroup || m_dragItem) && m_clipDrag) {
        QList<QGraphicsItem *> items;
        QMutexLocker lock(&m_selectionMutex);
        if (m_selectionGroup) items = m_selectionGroup->childItems();
        else if (m_dragItem) {
            m_dragItem->setMainSelectedClip(false);
            items.append(m_dragItem);
        }
        qDeleteAll(items);
        if (m_selectionGroup) scene()->destroyItemGroup(m_selectionGroup);
        m_selectionGroup = NULL;
        m_dragItem = NULL;
        event->accept();
    }
    else {
        QGraphicsView::dragLeaveEvent(event);
    }
}

void CustomTrackView::enterEvent(QEvent * event)
{
      if (m_tool == RazorTool && !m_cutLine) {
          m_cutLine = m_scene->addLine(0, 0, 0, m_tracksHeight * m_scene->scale().y());
          m_cutLine->setZValue(1000);
          QPen pen1 = QPen();
          pen1.setWidth(1);
          QColor line(Qt::red);
          pen1.setColor(line);
          m_cutLine->setPen(pen1);
          m_cutLine->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
      }
      QGraphicsView::enterEvent(event);
}

void CustomTrackView::leaveEvent(QEvent * event)
{
      removeTipAnimation();
      if (m_cutLine) {
          delete m_cutLine;
          m_cutLine = NULL;
      }
      QGraphicsView::leaveEvent(event);
}

void CustomTrackView::dropEvent(QDropEvent * event)
{
    GenTime startPos;
    GenTime duration;
    if ((m_selectionGroup || m_dragItem) && m_clipDrag) {
        QList<QGraphicsItem *> items;
        if (m_selectionGroup) {
            items = m_selectionGroup->childItems();
            startPos = GenTime((int) m_selectionGroup->scenePos().x(), m_document->fps());
            duration = m_selectionGroup->duration();
        } else if (m_dragItem) {
            m_dragItem->setMainSelectedClip(false);
            startPos = m_dragItem->startPos();
            duration = m_dragItem->cropDuration();
            items.append(m_dragItem);
        }
        resetSelectionGroup();
        m_dragItem = NULL;
        m_scene->clearSelection();
        QUndoCommand *addCommand = new QUndoCommand();
        addCommand->setText(i18n("Add timeline clip"));
        QList <ClipItem *> brokenClips;

        // Add refresh command for undo
        RefreshMonitorCommand *firstRefresh = new RefreshMonitorCommand(this, ItemInfo(), false, true, addCommand);
        for (int i = 0; i < items.count(); ++i) {
            m_scene->removeItem(items.at(i));
        }
        QList <ItemInfo> range;
        if (m_scene->editMode() == TimelineMode::InsertEdit) {
            cutTimeline(startPos.frames(m_document->fps()), QList <ItemInfo>(), QList <ItemInfo>(), addCommand);
            ItemInfo info;
            info.startPos = startPos;
            info.cropDuration = duration;
            new AddSpaceCommand(this, info, QList <ItemInfo>(), true, addCommand);
        } else if (m_scene->editMode() == TimelineMode::OverwriteEdit) {
            // Should we really overwrite all unlocked tracks ? If not we need to calculate the track for audio split first
            extractZone(QPoint(startPos.frames(m_document->fps()), (startPos+duration).frames(m_document->fps())), false, QList <ItemInfo>(), addCommand);
            /*for (int i = 0; i < items.count(); ++i) {
                if (items.at(i)->type() != AVWidget)
                    continue;
                ItemInfo info = items.at(i)->info();
                extractZone(QPoint(info.startPos.frames(m_document->fps()), (info.startPos+info.cropDuration).frames(m_document->fps())), false, QList <ItemInfo>(), addCommand, info.track);
            }*/
        }
        ItemInfo info;
        for (int i = 0; i < items.count(); ++i) {
	    if (items.at(i)->type() != AVWidget)
	      continue;
            ClipItem *item = static_cast <ClipItem *>(items.at(i));
	    ClipType cType = item->clipType();
            if (items.count() == 1) {
                updateClipTypeActions(item);
            } else {
                updateClipTypeActions(NULL);
            }
            info = item->info();
            QString clipBinId = item->getBinId();
	    PlaylistState::ClipState pState = item->clipState();
            if (item->hasVisibleVideo()) {
                range << info;
            }
	    if (KdenliveSettings::splitaudio()) {
		if (m_timeline->getTrackInfo(info.track).type == AudioTrack) {
		    if (cType != Audio)
			pState = PlaylistState::AudioOnly;
		} else if (item->isSplittable()) {
		    pState = PlaylistState::VideoOnly;
		}
	    }
            new AddTimelineClipCommand(this, clipBinId, info, item->effectList(), pState, true, false, false, addCommand);
            // Automatic audio split
            if (KdenliveSettings::splitaudio() && item->isSplittable() && pState != PlaylistState::AudioOnly)
                splitAudio(false, info, m_timeline->audioTarget, addCommand);
            else
                updateTrackDuration(info.track, addCommand);

            // Disabled since we now have working track compositing
            /*if (item->binClip()->isTransparent() && getTransitionItemAtStart(info.startPos, info.track) == NULL) {
                // add transparency transition if space is available
                if (canBePastedTo(info, TransitionWidget)) {
                    QDomElement trans = MainWindow::transitions.getEffectByTag(QStringLiteral("affine"), QString()).cloneNode().toElement();
                    new AddTransitionCommand(this, info, getPreviousVideoTrack(info.track), trans, false, true, addCommand);
                }
            }*/
        }
        qDeleteAll(items);
        // Add refresh command for redo
        firstRefresh->updateRange(range);
        new RefreshMonitorCommand(this, range, true, false, addCommand);
        if (addCommand->childCount() > 0) m_commandStack->push(addCommand);
        else delete addCommand;
        /*
        m_pasteEffectsAction->setEnabled(m_copiedItems.count() == 1);*/
        event->setDropAction(Qt::MoveAction);
        event->accept();
        ClipItem *clp = getClipItemAtStart(info.startPos, info.track);
        if (clp) {
            slotSelectItem(clp);
        }

        /// \todo enable when really working
        //        alignAudio();

    }
    else {
        QGraphicsView::dropEvent(event);
    }
    setFocus();
}

void CustomTrackView::cutTimeline(int cutPos, QList <ItemInfo> excludedClips, QList <ItemInfo> excludedTransitions, QUndoCommand *masterCommand, int track)
{
    QRectF rect;
    if (track == -1) {
        // Cut all tracks
        rect = QRectF(cutPos, 0, 1, m_timeline->visibleTracksCount() * m_tracksHeight);
    } else {
        // Cut only selected track
        rect = QRectF(cutPos, getPositionFromTrack(track) + m_tracksHeight / 2, 1, 2);
    }
    QList<QGraphicsItem *> selection = m_scene->items(rect);
    // We are going to move clips that are after zone, so break locked groups first.
    QList<ItemInfo> clipsToCut;
    QList<ItemInfo> transitionsToCut;
    for (int i = 0; i < selection.count(); ++i) {
        AbstractClipItem *item = static_cast<AbstractClipItem *> (selection.at(i));
        if (!item || !item->isEnabled() || !item->parentItem()) continue;
        ItemInfo moveInfo = item->info();
        // Skip locked tracks
        if (m_timeline->getTrackInfo(moveInfo.track).isLocked)
            continue;
        if (item->type() == AVWidget) {
            if (excludedClips.contains(moveInfo)) {
                continue;
            }
            clipsToCut.append(moveInfo);
        } else if (item->type() == TransitionWidget) {
            if (excludedTransitions.contains(moveInfo)) {
                continue;
            }
            transitionsToCut.append(moveInfo);
        }
    }
    if (!clipsToCut.isEmpty() || !transitionsToCut.isEmpty()) {
        breakLockedGroups(clipsToCut, transitionsToCut, masterCommand);
    }
    // Razor
    GenTime cutPosition = GenTime(cutPos, m_document->fps());
    for (int i = 0; i < selection.count(); ++i) {
        if (!selection.at(i)->isEnabled()) continue;
        if (selection.at(i)->type() == AVWidget) {
            ClipItem *clip = static_cast<ClipItem *>(selection.at(i));
            // Skip locked tracks
            if (m_timeline->getTrackInfo(clip->track()).isLocked)
                continue;
            ItemInfo info = clip->info();
            if (excludedClips.contains(info) || cutPosition == info.startPos) {
                continue;
            }
            new RazorClipCommand(this, info, clip->effectList(), cutPosition, true, masterCommand);
        } else if (selection.at(i)->type() == TransitionWidget) {
            Transition *trans = static_cast<Transition *>(selection.at(i));
            // Skip locked tracks
            if (m_timeline->getTrackInfo(trans->track()).isLocked)
                continue;
            ItemInfo info = trans->info();
            if (excludedTransitions.contains(info) || cutPosition == info.startPos) {
                continue;
            }
            new RazorTransitionCommand(this, info, trans->toXML(), cutPosition, true, masterCommand);
        }
    }
}

void CustomTrackView::extractZone(QPoint z, bool closeGap, QList <ItemInfo> excludedClips, QUndoCommand *masterCommand, int track)
{
    // remove track zone and close gap
    if (closeGap && m_timeline->getTrackInfo(m_selectedTrack).isLocked) {
        // Cannot perform an Extract operation on a locked track
        emit displayMessage(i18n("Cannot perform operation on a locked track"), ErrorMessage);
        return;
    }
    if (z.isNull()) {
        z = m_document->zone();
        z.setY(z.y() + 1);
    }
    QRectF rect;
    if (track == -1) {
        // All tracks
        rect = QRectF(z.x(), 0, z.y() - z.x() - 1, m_timeline->visibleTracksCount() * m_tracksHeight);
    } else {
        // one track only
        rect = QRectF(z.x(), getPositionFromTrack(track) + m_tracksHeight / 2, z.y() - z.x() - 1, 2);
    }
    QList<QGraphicsItem *> selection = m_scene->items(rect);
    QList<QGraphicsItem *> gapSelection;
    if (selection.isEmpty())
        return;
    GenTime inPoint(z.x(), m_document->fps());
    GenTime outPoint(z.y(), m_document->fps());
    bool hasMasterCommand = masterCommand != NULL;
    if (!hasMasterCommand) {
        masterCommand = new QUndoCommand();
        masterCommand->setText(i18n("Remove Zone"));
    }

    if (closeGap) {
        // We are going to move clips that are after zone, so break locked groups first.
        QRectF gapRect = QRectF(z.x(), 0, sceneRect().width() - z.x(), m_timeline->visibleTracksCount() * m_tracksHeight);
        gapSelection = m_scene->items(gapRect);
        QList<ItemInfo> clipsToMove;
        QList<ItemInfo> transitionsToMove;
        for (int i = 0; i < gapSelection.count(); ++i) {
            if (!gapSelection.at(i)->isEnabled()) continue;
            if (gapSelection.at(i)->type() == AVWidget) {
                ClipItem *clip = static_cast<ClipItem *>(gapSelection.at(i));
                // Skip locked tracks
                if (m_timeline->getTrackInfo(clip->track()).isLocked)
                    continue;
                ItemInfo moveInfo = clip->info();
                if (clip->type() == AVWidget) {
                    clipsToMove.append(moveInfo);
                } else if (clip->type() == TransitionWidget) {
                    transitionsToMove.append(moveInfo);
                }
            }
        }
        if (!clipsToMove.isEmpty() || !transitionsToMove.isEmpty()) {
            breakLockedGroups(clipsToMove, transitionsToMove, masterCommand);
        }
    }
    QList <ItemInfo> range;
    RefreshMonitorCommand *firstRefresh = new RefreshMonitorCommand(this, ItemInfo(), false, true, masterCommand);
    for (int i = 0; i < selection.count(); ++i) {
        if (!selection.at(i)->isEnabled()) continue;
        if (selection.at(i)->type() == AVWidget) {
            ClipItem *clip = static_cast<ClipItem *>(selection.at(i));
            // Skip locked tracks
            if (m_timeline->getTrackInfo(clip->track()).isLocked)
                continue;
            ItemInfo baseInfo = clip->info();
            if (excludedClips.contains(baseInfo))
                continue;
            bool refresh = clip->hasVisibleVideo();
            if (clip->startPos() < inPoint) {
                ItemInfo info = baseInfo;
                info.startPos = inPoint;
                info.cropDuration = info.endPos - info.startPos;
                if (clip->endPos() > outPoint) {
                    new RazorClipCommand(this, baseInfo, clip->effectList(), outPoint, true, masterCommand);
                    info.cropDuration = outPoint - inPoint;
                    info.endPos = outPoint;
                    baseInfo.endPos = outPoint;
                    baseInfo.cropDuration = outPoint - baseInfo.startPos;
                }
                new RazorClipCommand(this, baseInfo, clip->effectList(), inPoint, true, masterCommand);
                if (refresh) {
                    range << info;
                }
                new AddTimelineClipCommand(this, clip->getBinId(), info, clip->effectList(), clip->clipState(), true, true, false, masterCommand);
            } else if (clip->endPos() > outPoint) {
                new RazorClipCommand(this, baseInfo, clip->effectList(), outPoint, true, masterCommand);
                ItemInfo newInfo = baseInfo;
                newInfo.endPos = outPoint;
                newInfo.cropDuration = newInfo.endPos - newInfo.startPos;
                if (refresh) {
                    range << newInfo;
                }
                new AddTimelineClipCommand(this, clip->getBinId(), newInfo, clip->effectList(), clip->clipState(), true, true, false, masterCommand);
            } else {
                // Clip is entirely inside zone, delete it
                if (refresh) {
                    range << baseInfo;
                }
                new AddTimelineClipCommand(this, clip->getBinId(), baseInfo, clip->effectList(), clip->clipState(), true, true, false, masterCommand);
            }
        } else if (selection.at(i)->type() == TransitionWidget) {
            Transition *trans = static_cast<Transition *>(selection.at(i));
            // Skip locked tracks
            if (m_timeline->getTrackInfo(trans->track()).isLocked)
                continue;
            ItemInfo baseInfo = trans->info();
            if (excludedClips.contains(baseInfo))
                continue;
            QDomElement xml = trans->toXML();
            if (trans->startPos() < inPoint) {
                ItemInfo info = baseInfo;
                ItemInfo newInfo = baseInfo;
                info.startPos = inPoint;
                info.cropDuration = info.endPos - info.startPos;
                if (trans->endPos() > outPoint) {
                    // Transition starts before and ends after zone, proceed cuts
                    new RazorTransitionCommand(this, baseInfo, xml, outPoint, true, masterCommand);
                    info.cropDuration = outPoint - inPoint;
                    info.endPos = outPoint;
                    newInfo.endPos = outPoint;
                    newInfo.cropDuration = outPoint - newInfo.startPos;
                }
                new RazorTransitionCommand(this, newInfo, xml, inPoint, true, masterCommand);
                new AddTransitionCommand(this, info, trans->transitionEndTrack(), xml, true, true, masterCommand);
            } else if (trans->endPos() > outPoint) {
                // Cut and remove first part
                new RazorTransitionCommand(this, baseInfo, xml, outPoint, true, masterCommand);
                ItemInfo info = baseInfo;
                info.endPos = outPoint;
                info.cropDuration = info.endPos - info.startPos;
                new AddTransitionCommand(this, info, trans->transitionEndTrack(), xml, true, true, masterCommand);
            } else {
                // Transition is entirely inside zone, delete it
                new AddTransitionCommand(this, baseInfo, trans->transitionEndTrack(), xml, true, true, masterCommand);
            }
        }
    }
    if (closeGap) {
        // Remove empty zone
        QList<ItemInfo> clipsToMove;
        QList<ItemInfo> transitionsToMove;
        for (int i = 0; i < gapSelection.count(); ++i) {
            if (gapSelection.at(i)->type() == AVWidget || gapSelection.at(i)->type() == TransitionWidget) {
                AbstractClipItem *item = static_cast <AbstractClipItem *>(gapSelection.at(i));
                if (m_timeline->getTrackInfo(item->track()).isLocked) {
                    continue;
                }
                ItemInfo moveInfo = item->info();
                if (item->type() == AVWidget) {
                    if (moveInfo.startPos < outPoint) {
                        if (moveInfo.endPos <= outPoint) {
                            continue;
                        }
                        moveInfo.startPos = outPoint;
                        moveInfo.cropDuration = moveInfo.endPos - moveInfo.startPos;
                    }
                    clipsToMove.append(moveInfo);
                }
                else if (item->type() == TransitionWidget) {
                    if (moveInfo.startPos < outPoint) {
                        if (moveInfo.endPos <= outPoint) {
                            continue;
                        }
                        moveInfo.startPos = outPoint;
                        moveInfo.cropDuration = moveInfo.endPos - moveInfo.startPos;
                    }
                    transitionsToMove.append(moveInfo);
                }
            }
        }
        if (!clipsToMove.isEmpty() || !transitionsToMove.isEmpty()) {
            new InsertSpaceCommand(this, clipsToMove, transitionsToMove, -1, -(outPoint - inPoint), true, masterCommand);
            updateTrackDuration(-1, masterCommand);
        }
    }
    // Add refresh command for redo
    firstRefresh->updateRange(range);
    new RefreshMonitorCommand(this, range, true, false, masterCommand);
    if (!hasMasterCommand) {
        m_commandStack->push(masterCommand);
    }
    return;
}

void CustomTrackView::adjustTimelineTransitions(TimelineMode::EditMode mode, Transition *item, QUndoCommand *command)
{
    if (mode == TimelineMode::OverwriteEdit) {
        // if we are in overwrite or push mode, move clips accordingly
        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);
        ItemInfo info = item->info();
        QRectF rect(info.startPos.frames(m_document->fps()), getPositionFromTrack(info.track) + m_tracksHeight, (info.endPos - info.startPos).frames(m_document->fps()) - 1, 5);
        QList<QGraphicsItem *> selection = m_scene->items(rect);
        selection.removeAll(item);
        for (int i = 0; i < selection.count(); ++i) {
            if (!selection.at(i)->isEnabled()) continue;
            if (selection.at(i)->type() == TransitionWidget) {
                Transition *tr = static_cast<Transition *>(selection.at(i));
                if (tr->startPos() < info.startPos) {
                    ItemInfo firstPos = tr->info();
                    ItemInfo newPos = firstPos;
                    firstPos.endPos = item->startPos();
                    newPos.startPos = item->endPos();
                    new MoveTransitionCommand(this, tr->info(), firstPos, true, false, command);
                    if (tr->endPos() > info.endPos) {
                        // clone transition
                        new AddTransitionCommand(this, newPos, tr->transitionEndTrack(), tr->toXML(), false, true, command);
                    }
                } else if (tr->endPos() > info.endPos) {
                    // just resize
                    ItemInfo firstPos = tr->info();
                    firstPos.startPos = item->endPos();
                    new MoveTransitionCommand(this, tr->info(), firstPos, true, false, command);
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
    qstrList.append(QStringLiteral("text/plain"));
    qstrList.append(QStringLiteral("kdenlive/producerslist"));
    qstrList.append(QStringLiteral("kdenlive/clip"));
    return qstrList;
}

Qt::DropActions CustomTrackView::supportedDropActions() const
{
    // returns what actions are supported when dropping
    return Qt::MoveAction;
}

void CustomTrackView::setDuration(int duration)
{
    if (m_projectDuration == duration) return;
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

void CustomTrackView::addTrack(const TrackInfo &type, int ix)
{
    clearSelection();
    emit transitionItemSelected(NULL);
    QList <TransitionInfo> transitionInfos;
    if (ix == -1 || ix > m_timeline->tracksCount()) {
        ix = m_timeline->tracksCount() + 1;
    }
    if (ix <= m_timeline->videoTarget)
        m_timeline->videoTarget++;
    if (ix <= m_timeline->audioTarget)
        m_timeline->audioTarget++;
    // Prepare groups for reload
    QDomDocument doc;
    doc.setContent(m_document->groupsXml());
    QDomNodeList groups;
    if (!doc.isNull()) {
        groups = doc.documentElement().elementsByTagName("group");
        for (int nodeindex = 0; nodeindex < groups.count(); ++nodeindex) {
            QDomNode grp = groups.item(nodeindex);
            QDomNodeList nodes = grp.childNodes();
            for (int itemindex = 0; itemindex < nodes.count(); ++itemindex) {
                QDomElement elem = nodes.item(itemindex).toElement();
                if (!elem.hasAttribute("track")) continue;
                int track = elem.attribute("track").toInt();
                if (track <= ix) {
                    // No change
                    continue;
                }
                else {
                    elem.setAttribute("track", track + 1);
                }
            }
        }
    }
    // insert track in MLT's playlist
    transitionInfos = m_document->renderer()->mltInsertTrack(ix,  type.trackName, type.type == VideoTrack);
    Mlt::Tractor *tractor = m_document->renderer()->lockService();
    m_document->renderer()->unlockService(tractor);
    // Reload timeline and m_tracks structure from MLT's playlist
    reloadTimeline();
    // Refresh track compositing and audio mix
    m_timeline->refreshTransitions();
    loadGroups(groups);
}

void CustomTrackView::reloadTimeline()
{
    removeTipAnimation();
    emit clipItemSelected(NULL);
    emit transitionItemSelected(NULL);
    QList<QGraphicsItem *> selection = m_scene->items();
    selection.removeAll(m_cursorLine);
    for (int i = 0; i < m_guides.count(); ++i) {
        selection.removeAll(m_guides.at(i));
    }
    qDeleteAll<>(selection);
    m_timeline->getTracks();
    m_timeline->getTransitions();
    int maxHeight = m_tracksHeight * m_timeline->visibleTracksCount() * matrix().m22();
    for (int i = 0; i < m_guides.count(); ++i) {
        m_guides.at(i)->setLine(0, 0, 0, maxHeight - 1);
    }
    if (m_cursorLine->flags() & QGraphicsItem::ItemIgnoresTransformations) {
        m_cursorLine->setLine(0, 0, 0, maxHeight - 1);
    } else {
        m_cursorLine->setLine(0, 0, 0, m_tracksHeight * m_timeline->visibleTracksCount() - 1);
    }
    viewport()->update();
}


void CustomTrackView::removeTrack(int ix)
{
    // Clear effect stack
    clearSelection();
    emit transitionItemSelected(NULL);
    // Make sure the selected track index is not outside range
    m_selectedTrack = qBound(1, m_selectedTrack, m_timeline->tracksCount() - 2);
    if (ix == m_timeline->audioTarget) {
        m_timeline->audioTarget = -1;
    } else if (m_timeline->audioTarget > ix) {
        m_timeline->audioTarget--;
    }
    if (ix == m_timeline->videoTarget) {
        m_timeline->videoTarget = -1;
    } else if (m_timeline->videoTarget > ix) {
        m_timeline->videoTarget--;
    }

    //Delete composite transition
    Mlt::Tractor *tractor = m_document->renderer()->lockService();
    QScopedPointer<Mlt::Field> field(tractor->field());
    if (m_timeline->getTrackInfo(ix).type == VideoTrack) {
        QScopedPointer<Mlt::Transition> tr(m_timeline->transitionHandler->getTrackTransition(QStringList() << QStringLiteral("qtblend") << QStringLiteral("frei0r.cairoblend") << QStringLiteral("movit.overlay"), ix, -1));
        if (tr) {
            field->disconnect_service(*tr.data());
        }
        QScopedPointer<Mlt::Transition> mixTr(m_timeline->transitionHandler->getTrackTransition(QStringList() << QStringLiteral("mix"), ix, -1));
        if (mixTr) {
            field->disconnect_service(*mixTr.data());
        }
    }
    // Prepare groups for reload
    QDomDocument doc;
    doc.setContent(m_document->groupsXml());
    QDomNodeList groups;
    if (!doc.isNull()) {
        groups = doc.documentElement().elementsByTagName("group");
        for (int nodeindex = 0; nodeindex < groups.count(); ++nodeindex) {
            QDomNode grp = groups.item(nodeindex);
            QDomNodeList nodes = grp.childNodes();
            for (int itemindex = 0; itemindex < nodes.count(); ++itemindex) {
                QDomElement elem = nodes.item(itemindex).toElement();
                if (!elem.hasAttribute("track")) continue;
                int track = elem.attribute("track").toInt();
                if (track < ix) {
                    // No change
                    continue;
                }
                else if (track > ix) {
                    elem.setAttribute("track", track - 1);
                }
                else {
                    // track == ix
                    // A grouped item was on deleted track, remove it from group
                    elem.setAttribute("track", -1);
                }
            }
        }
    }

    //Manually remove all transitions issued from track ix, otherwise  MLT will relocate it to another track
    m_timeline->transitionHandler->deleteTrackTransitions(ix);

    // Delete track in MLT playlist
    tractor->remove_track(ix);
    m_document->renderer()->unlockService(tractor);
    reloadTimeline();
    // Refresh track compositing and audio mix
    m_timeline->refreshTransitions();
    loadGroups(groups);
}

void CustomTrackView::configTracks(const QList < TrackInfo > &trackInfos)
{
    //TODO: fix me, use UNDO/REDO
    for (int i = 0; i < trackInfos.count(); ++i) {
        m_timeline->setTrackInfo(m_timeline->visibleTracksCount() - i, trackInfos.at(i));
    }
    viewport()->update();
}

void CustomTrackView::slotSwitchTrackLock(int ix, bool enable, bool applyToAll)
{
    QUndoCommand *command = NULL;
    if (!applyToAll) {
        command = new LockTrackCommand(this, ix, enable);
    } else {
        command = new QUndoCommand;
        command->setText(i18n("Switch All Track Lock"));
        for (int i = 1; i <= m_timeline->visibleTracksCount(); ++i) {
            if (i == ix) 
                continue;
            new LockTrackCommand(this, i, enable, command);
        }
    }
    m_commandStack->push(command);
}


void CustomTrackView::lockTrack(int ix, bool lock, bool requestUpdate)
{
    m_timeline->lockTrack(ix, lock);
    if (requestUpdate)
        emit doTrackLock(ix, lock);
    AbstractClipItem *clip = NULL;
    QList<QGraphicsItem *> selection = m_scene->items(QRectF(0, getPositionFromTrack(ix) + m_tracksHeight / 2, sceneRect().width(), m_tracksHeight / 2 - 2));
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == GroupWidget && static_cast<AbstractGroupItem*>(selection.at(i)) != m_selectionGroup) {
            if (selection.at(i)->parentItem() && m_selectionGroup) {
                selection.removeAll(static_cast<QGraphicsItem*>(m_selectionGroup));
                resetSelectionGroup();
            }

            bool changeGroupLock = true;
            bool hasClipOnTrack = false;
            QList <QGraphicsItem *> children =  selection.at(i)->childItems();
            for (int j = 0; j < children.count(); ++j) {
                if (children.at(j)->isSelected()) {
                    if (children.at(j)->type() == AVWidget)
                        emit clipItemSelected(NULL);
                    else if (children.at(j)->type() == TransitionWidget)
                        emit transitionItemSelected(NULL);
                    else
                        continue;
                }

                AbstractClipItem * child = static_cast <AbstractClipItem *>(children.at(j));
                if (child) {
                    if (child == m_dragItem) {
                        m_dragItem->setMainSelectedClip(false);
                        m_dragItem = NULL;
                    }

                    // only unlock group, if it is not locked by another track too
                    if (!lock && child->track() != ix && m_timeline->getTrackInfo(child->track()).isLocked)
                        changeGroupLock = false;

                    // only (un-)lock if at least one clip is on the track
                    if (child->track() == ix)
                        hasClipOnTrack = true;
                }
            }
            if (changeGroupLock && hasClipOnTrack)
                static_cast<AbstractGroupItem*>(selection.at(i))->setItemLocked(lock);
        } else if((selection.at(i)->type() == AVWidget || selection.at(i)->type() == TransitionWidget)) {
            if (selection.at(i)->parentItem()) {
                if (selection.at(i)->parentItem() == m_selectionGroup) {
                    selection.removeAll(static_cast<QGraphicsItem*>(m_selectionGroup));
                    resetSelectionGroup();
                } else {
                    // groups are handled separately
                    continue;
                }
            }

            if (selection.at(i)->isSelected()) {
                if (selection.at(i)->type() == AVWidget)
                    emit clipItemSelected(NULL);
                else
                    emit transitionItemSelected(NULL);
            }
            clip = static_cast <AbstractClipItem *>(selection.at(i));
            clip->setItemLocked(lock);
            if (clip == m_dragItem) {
                m_dragItem->setMainSelectedClip(false);
                m_dragItem = NULL;
            }
        }
    }
    //qDebug() << "NEXT TRK STATE: " << m_timeline->getTrackInfo(tracknumber).isLocked;
    viewport()->update();
}

QList<QGraphicsItem *> CustomTrackView::checkForGroups(const QRectF &rect, bool *ok)
{
    // Check there is no group going over several tracks there, or that would result in timeline corruption
    QList<QGraphicsItem *> selection = scene()->items(rect);
    *ok = true;
    int maxHeight = m_tracksHeight * 1.5;
    for (int i = 0; i < selection.count(); ++i) {
        // Check that we don't try to move a group with clips on other tracks
        if (selection.at(i)->type() == GroupWidget && (selection.at(i)->boundingRect().height() >= maxHeight)) {
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

        QPointer<TrackDialog> d = new TrackDialog(m_timeline, parentWidget());
        d->comboTracks->setCurrentIndex(m_selectedTrack);
        d->label->setText(i18n("Track"));
        d->track_name->setHidden(true);
        d->name_label->setHidden(true);
        d->before_select->setHidden(true);
        d->setWindowTitle(i18n("Remove Space"));
        d->video_track->setHidden(true);
        d->audio_track->setHidden(true);
        if (d->exec() != QDialog::Accepted) {
            delete d;
            return;
        }
        // TODO check that this is the correct index
        track = m_timeline->visibleTracksCount() - d->comboTracks->currentIndex();
        delete d;
    } else {
        pos = GenTime((int)(mapToScene(m_menuPosition).x()), m_document->fps());
        track = getTrackFromPos(mapToScene(m_menuPosition).y());
    }

    if (m_timeline->isTrackLocked(track)) {
        emit displayMessage(i18n("Cannot remove space in a locked track"), ErrorMessage);
        return;
    }

    ClipItem *item = getClipItemAtMiddlePoint(pos.frames(m_document->fps()), track);
    if (item) {
        emit displayMessage(i18n("You must be in an empty space to remove space (time: %1, track: %2)", m_document->timecode().getTimecodeFromFrames(mapToScene(m_menuPosition).x()), track), ErrorMessage);
        return;
    }
    int length = m_timeline->getSpaceLength(pos, track, true);
    if (length <= 0) {
        emit displayMessage(i18n("You must be in an empty space to remove space (time: %1, track: %2)", m_document->timecode().getTimecodeFromFrames(mapToScene(m_menuPosition).x()), track), ErrorMessage);
        return;
    }

    // Make sure there is no group in the way
    QRectF rect(pos.frames(m_document->fps()), getPositionFromTrack(track) + m_tracksHeight / 2, sceneRect().width() - pos.frames(m_document->fps()), m_tracksHeight / 2 - 2);

    bool isOk;
    QList<QGraphicsItem *> items = checkForGroups(rect, &isOk);
    if (!isOk) {
        // groups found on track, do not allow the move
        emit displayMessage(i18n("Cannot remove space in a track with a group"), ErrorMessage);
        return;
    }

    QList<ItemInfo> clipsToMove;
    QList<ItemInfo> transitionsToMove;

    for (int i = 0; i < items.count(); ++i) {
        if (items.at(i)->type() == AVWidget || items.at(i)->type() == TransitionWidget) {
            AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
            ItemInfo info = item->info();
            if (item->type() == AVWidget) {
                clipsToMove.append(info);
            } else if (item->type() == TransitionWidget) {
                transitionsToMove.append(info);
            }
        }
    }

    if (!transitionsToMove.isEmpty()) {
        // Make sure that by moving the items, we don't get a transition collision
        // Find first transition
        ItemInfo info = transitionsToMove.at(0);
        for (int i = 1; i < transitionsToMove.count(); ++i)
            if (transitionsToMove.at(i).startPos < info.startPos) info = transitionsToMove.at(i);

        // make sure there are no transitions on the way
        QRectF rect(info.startPos.frames(m_document->fps()) - length, getPositionFromTrack(track) + m_tracksHeight / 2, length - 1, m_tracksHeight / 2 - 2);
        items = scene()->items(rect);
        int transitionCorrection = -1;
        for (int i = 0; i < items.count(); ++i) {
            if (items.at(i)->type() == TransitionWidget) {
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
    QUndoCommand *command = new QUndoCommand;
    command->setText(length > 0 ? i18n("Remove space") : i18n("Insert space"));
    breakLockedGroups(clipsToMove, transitionsToMove, command);
    new InsertSpaceCommand(this, clipsToMove, transitionsToMove, track, GenTime(-length, m_document->fps()), true, command);
    updateTrackDuration(track, command);
    m_commandStack->push(command);
}

void CustomTrackView::slotInsertSpace()
{
    int pos;
    int track = 0;
    if (m_menuPosition.isNull()) {
        pos = cursorPos();
    } else {
        pos = (int) mapToScene(m_menuPosition).x();
        track = getTrackFromPos(mapToScene(m_menuPosition).y());
    }
    QPointer<SpacerDialog> d = new SpacerDialog(GenTime(65, m_document->fps()),
                                                m_document->timecode(), track, m_timeline->getTracksInfo(), this);
    if (d->exec() != QDialog::Accepted) {
        delete d;
        return;
    }
    GenTime spaceDuration = d->selectedDuration();
    track = d->selectedTrack();
    delete d;

    QList<QGraphicsItem *> items;
    if (track >= 0) {
        if (m_timeline->isTrackLocked(track)) {
            emit displayMessage(i18n("Cannot insert space in a locked track"), ErrorMessage);
            return;
        }

        ClipItem *item = getClipItemAtMiddlePoint(pos, track);
        if (item) pos = item->startPos().frames(m_document->fps());

        // Make sure there is no group in the way
        QRectF rect(pos, getPositionFromTrack(track) + m_tracksHeight / 2, sceneRect().width() - pos, m_tracksHeight / 2 - 2);
        bool isOk;
        items = checkForGroups(rect, &isOk);
        if (!isOk) {
            // groups found on track, do not allow the move
            emit displayMessage(i18n("Cannot insert space in a track with a group"), ErrorMessage);
            return;
        }
    } else {
        QRectF rect(pos, 0, sceneRect().width() - pos, m_timeline->visibleTracksCount() * m_tracksHeight);
        items = scene()->items(rect);
    }

    QList<ItemInfo> clipsToMove;
    QList<ItemInfo> transitionsToMove;

    for (int i = 0; i < items.count(); ++i) {
        if (items.at(i)->type() == AVWidget || items.at(i)->type() == TransitionWidget) {
            AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
            ItemInfo info = item->info();
            if (item->type() == AVWidget)
                clipsToMove.append(info);
            else if (item->type() == TransitionWidget)
                transitionsToMove.append(info);
        }
    }
    if (!clipsToMove.isEmpty() || !transitionsToMove.isEmpty()) {
        QUndoCommand *command = new QUndoCommand;
        command->setText(spaceDuration < GenTime() ? i18n("Remove space") : i18n("Insert space"));
        breakLockedGroups(clipsToMove, transitionsToMove, command);
        new InsertSpaceCommand(this, clipsToMove, transitionsToMove, track, spaceDuration, true, command);
        updateTrackDuration(track, command);
        m_commandStack->push(command);
    }
}

void CustomTrackView::insertTimelineSpace(GenTime startPos, GenTime duration, int track, QList <ItemInfo> excludeList)
{
    int pos = startPos.frames(m_document->fps());
    QRectF rect;
    if (track == -1) {
        // all tracks
        rect = QRectF(pos, 0, sceneRect().width() - pos, m_timeline->visibleTracksCount() * m_tracksHeight);
    } else {
        // selected track only
        rect = QRectF(pos, getPositionFromTrack(track) + m_tracksHeight / 2, sceneRect().width() - pos, m_timeline->visibleTracksCount() * 2);
    }
    QList<QGraphicsItem *> items = scene()->items(rect);
    QList<ItemInfo> clipsToMove;
    QList<ItemInfo> transitionsToMove;
    QList<AbstractClipItem *> excludedItems;

    for (int i = 0; i < items.count(); ++i) {
        if (items.at(i)->type() == AVWidget || items.at(i)->type() == TransitionWidget) {
            AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
            if (m_timeline->getTrackInfo(item->track()).isLocked)
                continue;
            if (excludeList.contains(item->info())) {
                excludedItems << item;
		item->setItemLocked(true);
                continue;
            }
            if (item->type() == AVWidget)
                clipsToMove.append(item->info());
            else if (item->type() == TransitionWidget)
                transitionsToMove.append(item->info());
        }
    }
    if (!clipsToMove.isEmpty() || !transitionsToMove.isEmpty()) {
        insertSpace(clipsToMove, transitionsToMove, -1, duration, GenTime());
    }
    foreach(AbstractClipItem *item, excludedItems) {
      item->setItemLocked(false);
    }
}

void CustomTrackView::insertSpace(QList<ItemInfo> clipsToMove, QList<ItemInfo> transToMove, int track, const GenTime &duration, const GenTime &offset)
{
    int diff = duration.frames(m_document->fps());
    resetSelectionGroup();
    m_selectionMutex.lock();
    m_selectionGroup = new AbstractGroupItem(m_document->fps());
    scene()->addItem(m_selectionGroup);

    // Create lists with start pos for each track
    QMap <int, int> trackClipStartList;
    QMap <int, int> trackTransitionStartList;

    for (int i = 1; i < m_timeline->tracksCount() + 1; ++i) {
        trackClipStartList[i] = -1;
        trackTransitionStartList[i] = -1;
    }
    if (!clipsToMove.isEmpty()) for (int i = 0; i < clipsToMove.count(); ++i) {
        ClipItem *clip = getClipItemAtStart(clipsToMove.at(i).startPos + offset, clipsToMove.at(i).track);
        if (clip) {
            if (clip->parentItem()) {
                m_selectionGroup->addItem(clip->parentItem());
            } else {
                m_selectionGroup->addItem(clip);
            }
            if (trackClipStartList.value(clipsToMove.at(i).track) == -1 || clipsToMove.at(i).startPos.frames(m_document->fps()) < trackClipStartList.value(clipsToMove.at(i).track))
                trackClipStartList[clipsToMove.at(i).track] = clipsToMove.at(i).startPos.frames(m_document->fps());
        } else {
            emit displayMessage(i18n("Cannot move clip at position %1, track %2", m_document->timecode().getTimecodeFromFrames((clipsToMove.at(i).startPos + offset).frames(m_document->fps())), clipsToMove.at(i).track), ErrorMessage);
        }
    }
    if (!transToMove.isEmpty()) for (int i = 0; i < transToMove.count(); ++i) {
        Transition *transition = getTransitionItemAtStart(transToMove.at(i).startPos + offset, transToMove.at(i).track);
        if (transition) {
            if (transition->parentItem()) {
                // If group has a locked item, ungroup first
                AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(transition->parentItem());
                if (grp->isItemLocked()) {
                    m_document->clipManager()->removeGroup(grp);
                    if (grp == m_selectionGroup) m_selectionGroup = NULL;
                    scene()->destroyItemGroup(grp);
                    transition->setItemLocked(false);
                    m_selectionGroup->addItem(transition);
                } else {
                    m_selectionGroup->addItem(transition->parentItem());
                }
            } else {
                m_selectionGroup->addItem(transition);
            }
            if (trackTransitionStartList.value(transToMove.at(i).track) == -1 || transToMove.at(i).startPos.frames(m_document->fps()) < trackTransitionStartList.value(transToMove.at(i).track))
                trackTransitionStartList[transToMove.at(i).track] = transToMove.at(i).startPos.frames(m_document->fps());
        } else emit displayMessage(i18n("Cannot move transition at position %1, track %2", m_document->timecode().getTimecodeFromFrames(transToMove.at(i).startPos.frames(m_document->fps())), transToMove.at(i).track), ErrorMessage);
    }

    m_selectionGroup->setTransform(QTransform::fromTranslate(diff, 0), true);

    // update items coordinates
    QList<QGraphicsItem *> itemList = m_selectionGroup->childItems();
    QList<AbstractGroupItem *> groupList;

    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == AVWidget || itemList.at(i)->type() == TransitionWidget) {
            int realTrack = getTrackFromPos(itemList.at(i)->scenePos().y());
            static_cast < AbstractClipItem *>(itemList.at(i))->updateItem(realTrack);
        } else if (itemList.at(i)->type() == GroupWidget) {
            AbstractGroupItem *group = static_cast < AbstractGroupItem *>(itemList.at(i));
            groupList << group;
            m_document->clipManager()->removeGroup(group);
            QList<QGraphicsItem *> children = itemList.at(i)->childItems();
            for (int j = 0; j < children.count(); ++j) {
                AbstractClipItem * clp = static_cast < AbstractClipItem *>(children.at(j));
                int realTrack = getTrackFromPos(clp->scenePos().y());
                clp->updateItem(realTrack);
            }
        }
    }
    m_selectionMutex.unlock();
    resetSelectionGroup(false);
    // Rebuild groups after translate
    foreach (AbstractGroupItem *grp, groupList) {
        rebuildGroup(grp);
    }
    m_document->renderer()->mltInsertSpace(trackClipStartList, trackTransitionStartList, track, duration, offset);
}

void CustomTrackView::deleteClip(const QString &clipId, QUndoCommand *deleteCommand)
{
    resetSelectionGroup();
    QList<QGraphicsItem *> itemList = items();
    int count = 0;
    QList <ItemInfo> range;
    RefreshMonitorCommand *firstRefresh = new RefreshMonitorCommand(this, ItemInfo(), false, true, deleteCommand);
    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == AVWidget) {
            ClipItem *item = static_cast<ClipItem*>(itemList.at(i));
            if (item->getBinId() == clipId) {
                count++;
                if (item->hasVisibleVideo())
                    range << item->info();
                if (item->parentItem()) {
                    // Clip is in a group, destroy the group
                    new GroupClipsCommand(this, QList<ItemInfo>() << item->info(), QList<ItemInfo>(), false, true, deleteCommand);
                }
                new AddTimelineClipCommand(this, item->getBinId(), item->info(), item->effectList(), item->clipState(), true, true, false, deleteCommand);
                // Check if it is a title clip with automatic transition, than remove it
                if (item->clipType() == Text) {
                    Transition *tr = getTransitionItemAtStart(item->startPos(), item->track());
                    if (tr && tr->endPos() == item->endPos()) {
                        new AddTransitionCommand(this, tr->info(), tr->transitionEndTrack(), tr->toXML(), true, true, deleteCommand);
                    }
                }
            }
        }
    }
    if (count > 0) {
        firstRefresh->updateRange(range);
        new RefreshMonitorCommand(this, range, true, false, deleteCommand);
        updateTrackDuration(-1, deleteCommand);
    }
}

void CustomTrackView::seekCursorPos(int pos)
{
    emit updateRuler(pos);
    m_document->renderer()->seek(pos);
}

int CustomTrackView::seekPosition() const
{
    int seek = m_document->renderer()->requestedSeekPosition;
    if (seek == SEEK_INACTIVE)
        return m_cursorPos;
    return seek;
}


void CustomTrackView::setCursorPos(int pos)
{
    if (pos != m_cursorPos) {
        emit cursorMoved(m_cursorPos, pos);
        m_cursorPos = pos;
        m_cursorLine->setPos(m_cursorPos + m_cursorOffset, 0);
        if (m_autoScroll) checkScrolling();
    }
    //else emit updateRuler();
}

int CustomTrackView::cursorPos() const
{
    return m_cursorPos;
}

void CustomTrackView::moveCursorPos(int delta)
{
    int currentPos = m_document->renderer()->requestedSeekPosition;
    if (currentPos == SEEK_INACTIVE) {
        currentPos = m_document->renderer()->seekFramePosition() + delta;
    }
    else {
        currentPos += delta;
    }
    emit updateRuler(currentPos);
    m_document->renderer()->seek(qMax(0, currentPos));
}

void CustomTrackView::initCursorPos(int pos)
{
    emit cursorMoved(m_cursorPos, pos);
    m_cursorPos = pos;
    m_cursorLine->setPos(m_cursorPos + 0.5, 0);
    checkScrolling();
}

void CustomTrackView::checkScrolling()
{
    QGraphicsView::ViewportUpdateMode mode = viewportUpdateMode();
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);    
    ensureVisible(seekPosition(), verticalScrollBar()->value() + 10, 2, 2, 50, 0);
    setViewportUpdateMode(mode);
}

void CustomTrackView::scrollToStart()
{
    horizontalScrollBar()->setValue(0);
}

void CustomTrackView::completeSpaceOperation(int track, GenTime &timeOffset)
{
  QList <AbstractGroupItem*> groups;

  if (timeOffset != GenTime()) 
  {
    QList<QGraphicsItem *> items = m_selectionGroup->childItems();

    QList<ItemInfo> clipsToMove;
    QList<ItemInfo> transitionsToMove;

    // Create lists with start pos for each track
    QMap <int, int> trackClipStartList;
    QMap <int, int> trackTransitionStartList;

    for (int i = 1; i < m_timeline->tracksCount() + 1; ++i) 
    {
      trackClipStartList[i] = -1;
      trackTransitionStartList[i] = -1;
    }
    for (int i = 0; i < items.count(); ++i) 
    {
      if (items.at(i)->type() == GroupWidget) 
      {
	AbstractGroupItem* group = static_cast<AbstractGroupItem*>(items.at(i));
	if (!groups.contains(group)) groups.append(group);
	items += items.at(i)->childItems();
      }
    }

    for (int i = 0; i < items.count(); ++i) 
    {
      if (items.at(i)->type() == AVWidget) 
      {
	AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
	ItemInfo info = item->info();
	clipsToMove.append(info);
        int realTrack = getTrackFromPos(item->scenePos().y());
	item->updateItem(realTrack);
	if (trackClipStartList.value(info.track) == -1 || 
	    info.startPos.frames(m_document->fps()) < trackClipStartList.value(info.track))
	  trackClipStartList[info.track] = info.startPos.frames(m_document->fps());
      } 
      else if (items.at(i)->type() == TransitionWidget) 
      {
	AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
	ItemInfo info = item->info();
	transitionsToMove.append(info);
        int realTrack = getTrackFromPos(item->scenePos().y());
	item->updateItem(realTrack);
	if (trackTransitionStartList.value(info.track) == -1 || 
	    info.startPos.frames(m_document->fps()) < trackTransitionStartList.value(info.track))
	  trackTransitionStartList[info.track] = info.startPos.frames(m_document->fps());
      }
    }
    if (!clipsToMove.isEmpty() || !transitionsToMove.isEmpty()) 
    {
        QUndoCommand *command = new QUndoCommand;
        command->setText(timeOffset < GenTime() ? i18n("Remove space") : i18n("Insert space"));
        //TODO: break groups upstream
        breakLockedGroups(clipsToMove, transitionsToMove, command, false);
        new InsertSpaceCommand(this, clipsToMove, transitionsToMove, track, timeOffset, false, command);
        updateTrackDuration(track, command);
        m_commandStack->push(command);
        m_document->renderer()->mltInsertSpace(trackClipStartList, trackTransitionStartList, track, timeOffset, GenTime());
    }
  }
  resetSelectionGroup();
  for (int i = 0; i < groups.count(); ++i) 
  {
      rebuildGroup(groups.at(i));
  }

  clearSelection();
  m_operationMode = None;
}

void CustomTrackView::mouseReleaseEvent(QMouseEvent * event)
{
    if (event->button() != Qt::LeftButton) {
	QGraphicsView::mouseReleaseEvent(event);
        return;
    }

    if (event->modifiers() & Qt::ControlModifier)  {
	event->ignore();
    }

    QGraphicsView::mouseReleaseEvent(event);
    setDragMode(QGraphicsView::NoDrag);

    if (m_moveOpMode == Seek || m_moveOpMode == ScrollTimeline || m_moveOpMode == ZoomTimeline) {
        m_moveOpMode = None;
        return;
    }
    m_clipDrag = false;
    if (m_scrollTimer.isActive()) m_scrollTimer.stop();

    /*if (m_dragItem == NULL && m_selectionGroup == NULL) {
        emit transitionItemSelected(NULL);
	m_moveOpMode = None;
        return;
    }*/
    QPointF clickPoint = mapToScene(event->pos());
    GenTime clickFrame(clickPoint.x(), m_document->fps());

    switch ((int) (m_moveOpMode == WaitingForConfirm ? m_operationMode : m_moveOpMode)) {
        case MoveGuide:
            m_toolManagers.value(GuideType)->mouseRelease();
            break;
        case Spacer:
            m_toolManagers.value(SpacerType)->mouseRelease(GenTime(m_selectionGroup->sceneBoundingRect().left(), m_document->fps()));
            break;
        case RubberSelection:
            m_toolManagers.value(SelectType)->mouseRelease();
            break;
        case MoveOperation:
            m_toolManagers.value(MoveType)->mouseRelease();
            break;
        case RollingStart:
        case RollingEnd:
            m_toolManagers.value(TrimType)->mouseRelease(m_selectionGroup ? m_selectionGroup->startPos() : GenTime());
            break;
        case ResizeStart:
        case ResizeEnd:
            m_toolManagers.value(ResizeType)->mouseRelease();
            break;
    }


    if (m_moveOpMode == FadeIn && m_dragItem) {
        ClipItem * item = static_cast <ClipItem *>(m_dragItem);
        // find existing video fade, if none then audio fade

        int fadeIndex = item->hasEffect(QLatin1String(""), QStringLiteral("fade_from_black"));
        int fadeIndex2 = item->hasEffect(QStringLiteral("volume"), QStringLiteral("fadein"));
        if (fadeIndex >= 0 && fadeIndex2 >= 0) {
            // We have 2 fadin effects, use currently selected or first one
            int current = item->selectedEffectIndex();
            if (fadeIndex != current) {
                if (fadeIndex2 == current) {
                    fadeIndex = current;
                }
                else fadeIndex = qMin(fadeIndex, fadeIndex2);
            }
        }
        else fadeIndex = qMax(fadeIndex, fadeIndex2);
        // resize fade in effect
        if (fadeIndex >= 0) {
            QDomElement oldeffect = item->effectAtIndex(fadeIndex);
            int end = item->fadeIn();
            if (end == 0) {
                slotDeleteEffect(item, -1, oldeffect, false);
            } else {
                int start = item->cropStart().frames(m_document->fps());
                end += start;
                QDomElement effect = oldeffect.cloneNode().toElement();
                EffectsList::setParameter(oldeffect, QStringLiteral("in"), QString::number(start));
                EffectsList::setParameter(oldeffect, QStringLiteral("out"), QString::number(end));
                slotUpdateClipEffect(item, -1, effect, oldeffect, fadeIndex);
                emit clipItemSelected(item);
            }
        // new fade in
        } else if (item->fadeIn() != 0) {
            QDomElement effect;
            if (item->clipState() == PlaylistState::VideoOnly || (item->clipType() != Audio && item->clipState() != PlaylistState::AudioOnly && item->clipType() != Playlist)) {
                effect = MainWindow::videoEffects.getEffectByTag(QLatin1String(""), QStringLiteral("fade_from_black")).cloneNode().toElement();
            } else {
                effect = MainWindow::audioEffects.getEffectByTag(QStringLiteral("volume"), QStringLiteral("fadein")).cloneNode().toElement();
            }
            EffectsList::setParameter(effect, QStringLiteral("out"), QString::number(item->fadeIn()));
            slotAddEffect(effect, m_dragItem->startPos(), m_dragItem->track());
        }

    } else if (m_moveOpMode == FadeOut && m_dragItem) {
        ClipItem * item = static_cast <ClipItem *>(m_dragItem);
        // find existing video fade, if none then audio fade

        int fadeIndex = item->hasEffect(QLatin1String(""), QStringLiteral("fade_to_black"));
        int fadeIndex2 = item->hasEffect(QStringLiteral("volume"), QStringLiteral("fadeout"));
        if (fadeIndex >= 0 && fadeIndex2 >= 0) {
            // We have 2 fadin effects, use currently selected or first one
            int current = item->selectedEffectIndex();
            if (fadeIndex != current) {
                if (fadeIndex2 == current) {
                    fadeIndex = current;
                }
                else fadeIndex = qMin(fadeIndex, fadeIndex2);
            }
        }
        else fadeIndex = qMax(fadeIndex, fadeIndex2);
        // resize fade out effect
        if (fadeIndex >= 0) {
            QDomElement oldeffect = item->effectAtIndex(fadeIndex);
            int start = item->fadeOut();
            if (start == 0) {
                slotDeleteEffect(item, -1, oldeffect, false);
            } else {
                int end = (item->cropDuration() + item->cropStart()).frames(m_document->fps());
                start = end - start;
                QDomElement effect = oldeffect.cloneNode().toElement();
                EffectsList::setParameter(oldeffect, QStringLiteral("in"), QString::number(start));
                EffectsList::setParameter(oldeffect, QStringLiteral("out"), QString::number(end));
                slotUpdateClipEffect(item, -1, effect, oldeffect, fadeIndex);
                emit clipItemSelected(item);
            }
        // new fade out
        } else if (item->fadeOut() != 0) {
            QDomElement effect;
            if (item->clipState() == PlaylistState::VideoOnly || (item->clipType() != Audio && item->clipState() != PlaylistState::AudioOnly && item->clipType() != Playlist)) {
                effect = MainWindow::videoEffects.getEffectByTag(QLatin1String(""), QStringLiteral("fade_to_black")).cloneNode().toElement();
            } else {
                effect = MainWindow::audioEffects.getEffectByTag(QStringLiteral("volume"), QStringLiteral("fadeout")).cloneNode().toElement();
            }
            int end = (item->cropDuration() + item->cropStart()).frames(m_document->fps());
            int start = end-item->fadeOut();
            EffectsList::setParameter(effect, QStringLiteral("in"), QString::number(start));
            EffectsList::setParameter(effect, QStringLiteral("out"), QString::number(end));
            slotAddEffect(effect, m_dragItem->startPos(), m_dragItem->track());
        }

    } else if (m_moveOpMode == KeyFrame && m_dragItem) {
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

        if ((val < -50 || val > 150) && item->selectedKeyFramePos() != start && item->selectedKeyFramePos() != end && item->keyframesCount() > 1) {
            //delete keyframe
            item->removeKeyframe(item->getEffectAtIndex(item->selectedEffectIndex()), item->selectedKeyFramePos());
        } else {
            item->movedKeyframe(item->getEffectAtIndex(item->selectedEffectIndex()), item->selectedKeyFramePos(), item->originalKeyFramePos());
        }

        QDomElement newEffect = item->selectedEffect().cloneNode().toElement();

        EditEffectCommand *command = new EditEffectCommand(this, item->track(), item->startPos(), oldEffect, newEffect, item->selectedEffectIndex(), false, false, true);
        m_commandStack->push(command);
        updateEffect(item->track(), item->startPos(), item->selectedEffect());
        emit clipItemSelected(item);
    } else if (m_moveOpMode == WaitingForConfirm && m_operationMode == KeyFrame && m_dragItem) {
        emit setActiveKeyframe(m_dragItem->selectedKeyFramePos());
    }

    m_moveOpMode = None;
}

void CustomTrackView::deleteClip(ItemInfo info, bool refresh)
{
    ClipItem *item = getClipItemAtStart(info.startPos, info.track, info.endPos);
    m_ct++;
    if (!item) {
        return;
    }
    //m_document->renderer()->saveSceneList(QString("/tmp/error%1.mlt").arg(m_ct), QDomElement());
    if (!m_timeline->track(info.track)->del(info.startPos.seconds())) {
        qDebug()<<" / / /CANNOT DELETE CLIP AT: "<<info.startPos.frames(25);
        emit displayMessage(i18n("Error removing clip at %1 on track %2", m_document->timecode().getTimecodeFromFrames(info.startPos.frames(m_document->fps())), m_timeline->getTrackInfo(info.track).trackName), ErrorMessage);
        return;
    }
    item->stopThumbs();
    item->binClip()->removeRef();
    if (item->isSelected()) emit clipItemSelected(NULL);
    if (m_dragItem == item) {
        m_dragItem->setMainSelectedClip(false);
        m_dragItem = NULL;
    }
    if (refresh && !item->hasVisibleVideo()) {
        refresh = false;
    }
    delete item;
    item = NULL;
    if (refresh) 
        monitorRefresh(info, true);
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
    RefreshMonitorCommand *firstRefresh = new RefreshMonitorCommand(this, ItemInfo(), false, true, deleteSelected);

    int groupCount = 0;
    int clipCount = 0;
    int transitionCount = 0;
    // expand & destroy groups
    QList <ItemInfo> range;
    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == GroupWidget) {
            groupCount++;
            QList<QGraphicsItem *> children = itemList.at(i)->childItems();
            QList <ItemInfo> clipInfos;
            QList <ItemInfo> transitionInfos;
            for (int j = 0; j < children.count(); ++j) {
                if (children.at(j)->type() == AVWidget) {
                    AbstractClipItem *clip = static_cast <AbstractClipItem *>(children.at(j));
                    if (!clip->isItemLocked()) clipInfos.append(clip->info());
                } else if (children.at(j)->type() == TransitionWidget) {
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
                new GroupClipsCommand(this, clipInfos, transitionInfos, false, true, deleteSelected);

        } else if (itemList.at(i)->parentItem() && itemList.at(i)->parentItem()->type() == GroupWidget)
            itemList.insert(i + 1, itemList.at(i)->parentItem());
    }
    emit clipItemSelected(NULL);
    emit transitionItemSelected(NULL);
    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == AVWidget) {
            clipCount++;
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            new AddTimelineClipCommand(this, item->getBinId(), item->info(), item->effectList(), item->clipState(), true, true, false, deleteSelected);
            // Check if it is a title clip with automatic transition, than remove it
            if (item->clipType() == Text) {
                Transition *tr = getTransitionItemAtStart(item->startPos(), item->track());
                if (tr && !itemList.contains(tr) && tr->isAutomatic() && tr->endPos() == item->endPos()) {
                    new AddTransitionCommand(this, tr->info(), tr->transitionEndTrack(), tr->toXML(), true, true, deleteSelected);
                }
            }
        } else if (itemList.at(i)->type() == TransitionWidget) {
            transitionCount++;
            Transition *item = static_cast <Transition *>(itemList.at(i));
            new AddTransitionCommand(this, item->info(), item->transitionEndTrack(), item->toXML(), true, true, deleteSelected);
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
    firstRefresh->updateRange(range);
    new RefreshMonitorCommand(this, range, true, false, deleteSelected);
    m_commandStack->push(deleteSelected);
}


void CustomTrackView::doChangeClipSpeed(ItemInfo info, const ItemInfo &speedIndependantInfo, PlaylistState::ClipState state, const double speed, int strobe, const QString &id, bool removeEffect)
{
    ClipItem *item = getClipItemAtStart(info.startPos, info.track);
    if (!item) {
        //qDebug() << "ERROR: Cannot find clip for speed change";
        emit displayMessage(i18n("Cannot find clip for speed change"), ErrorMessage);
        return;
    }
    if (speed == item->speed()) {
        // Nothing to do, abort
        return;
    }
    int endPos = m_timeline->changeClipSpeed(info, speedIndependantInfo, state, speed, strobe, m_document->renderer()->getBinProducer(id), removeEffect);
    if (endPos >= 0) {
        item->setSpeed(speed, strobe);
        item->updateRectGeometry();
        if (item->cropDuration().frames(m_document->fps()) != endPos - 1) {
            item->resizeEnd((int) info.startPos.frames(m_document->fps()) + endPos );
	}
        updatePositionEffects(item, info, false);
    } else {
        emit displayMessage(i18n("Invalid clip"), ErrorMessage);
    }
}

void CustomTrackView::cutSelectedClips(QList<QGraphicsItem *> itemList, GenTime currentPos)
{
    if (itemList.isEmpty())
        itemList = scene()->selectedItems();
    QList<AbstractGroupItem *> groups;
    if (currentPos == GenTime())
        currentPos = GenTime(m_cursorPos, m_document->fps());
    if (itemList.isEmpty()) {
        // Fetch clip on selected track / under cursor
        ClipItem *under = getClipItemAtMiddlePoint(m_cursorPos, m_selectedTrack);
        if (under)
            itemList << under;
    }
    for (int i = 0; i < itemList.count(); ++i) {
        if (!itemList.at(i))
            continue;
        if (itemList.at(i)->type() == AVWidget) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            if (item->parentItem() && item->parentItem() != m_selectionGroup) {
                AbstractGroupItem *group = static_cast <AbstractGroupItem *>(item->parentItem());
                if (!groups.contains(group))
                    groups << group;
            } else if (currentPos > item->startPos() && currentPos < item->endPos()) {
                RazorClipCommand *command = new RazorClipCommand(this, item->info(), item->effectList(), currentPos);
                m_commandStack->push(command);
            }
        } else if (itemList.at(i)->type() == GroupWidget && itemList.at(i) != m_selectionGroup) {
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
        QUndoCommand *command = new QUndoCommand;
        command->setText(i18n("Cut Group"));
        groupClips(false, children, false, command);
        QList <ItemInfo> clips1, transitions1;
        QList <ItemInfo> transitionsCut;
        QList <ItemInfo> clips2, transitions2;
        QVector <QGraphicsItem *> clipsToCut;

        // Collect info
        for (int i = 0; i < children.count(); ++i) {
            children.at(i)->setSelected(false);
            AbstractClipItem *child = static_cast <AbstractClipItem *>(children.at(i));
			if (!child) continue;
            if (child->type() == AVWidget) {
                if (cutPos >= child->endPos())
                    clips1 << child->info();
                else if (cutPos <= child->startPos())
                    clips2 << child->info();
                else {
                    clipsToCut << child;
                }
            } else {
                if (cutPos > child->endPos())
                    transitions1 << child->info();
                else if (cutPos < child->startPos())
                    transitions2 << child->info();
                else {
                    //transitionsCut << child->info();
                    // Transition cut not implemented, leave it in first group...
                    transitions1 << child->info();
                }
            }
        }
        if (clipsToCut.isEmpty() && transitionsCut.isEmpty() && ((clips1.isEmpty() && transitions1.isEmpty()) || (clips2.isEmpty() && transitions2.isEmpty()))) {
            delete command;
            return;
        }
        // Process the cut
        for (int i = 0; i < clipsToCut.count(); ++i) {
            ClipItem *clip = static_cast<ClipItem *>(clipsToCut.at(i));
            new RazorClipCommand(this, clip->info(), clip->effectList(), cutPos, true, command);
            ItemInfo info = clip->info();
            info.endPos = GenTime(cutPos.frames(m_document->fps()) - 1, m_document->fps());
            info.cropDuration = info.endPos - info.startPos;
            ItemInfo cutInfo = info;
            cutInfo.startPos = cutPos;
            cutInfo.cropDuration = cutInfo.endPos - cutInfo.startPos;
            clips1 << info;
            clips2 << cutInfo;
        }
        new GroupClipsCommand(this, clips1, transitions1, true, true, command);
        new GroupClipsCommand(this, clips2, transitions2, true, true, command);
        m_commandStack->push(command);
    }
}

void CustomTrackView::groupClips(bool group, QList<QGraphicsItem *> itemList, bool forceLock, QUndoCommand *command, bool doIt)
{
    if (itemList.isEmpty()) itemList = scene()->selectedItems();
    QList <ItemInfo> clipInfos;
    QList <ItemInfo> transitionInfos;
    QList<QGraphicsItem *> existingGroups;

    // Expand groups
    int max = itemList.count();
    for (int i = 0; i < max; ++i) {
        if (itemList.at(i)->type() == GroupWidget) {
            if (!existingGroups.contains(itemList.at(i))) {
                existingGroups << itemList.at(i);
            }
            itemList += itemList.at(i)->childItems();
        }
    }

    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == AVWidget) {
            AbstractClipItem *clip = static_cast <AbstractClipItem *>(itemList.at(i));
            if (forceLock || !clip->isItemLocked()) clipInfos.append(clip->info());
        } else if (itemList.at(i)->type() == TransitionWidget) {
            AbstractClipItem *clip = static_cast <AbstractClipItem *>(itemList.at(i));
            if (forceLock || !clip->isItemLocked()) transitionInfos.append(clip->info());
        }
    }
    if (clipInfos.count() > 0) {
        // break previous groups
        QUndoCommand *metaCommand = NULL;
        if (group && !command && !existingGroups.isEmpty()) {
            metaCommand = new QUndoCommand();
            metaCommand->setText(i18n("Group clips"));
        }
        for (int i = 0; group && i < existingGroups.count(); ++i) {
            AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(existingGroups.at(i));
            QList <ItemInfo> clipGrpInfos;
            QList <ItemInfo> transitionGrpInfos;
            QList <QGraphicsItem *> items = grp->childItems();
            for (int j = 0; j < items.count(); ++j) {
                if (items.at(j)->type() == AVWidget) {
                    AbstractClipItem *clip = static_cast <AbstractClipItem *>(items.at(j));
                    if (forceLock || !clip->isItemLocked()) clipGrpInfos.append(clip->info());
                } else if (items.at(j)->type() == TransitionWidget) {
                    AbstractClipItem *clip = static_cast <AbstractClipItem *>(items.at(j));
                    if (forceLock || !clip->isItemLocked()) transitionGrpInfos.append(clip->info());
                }
            }
            if (!clipGrpInfos.isEmpty() || !transitionGrpInfos.isEmpty()) {
                if (command) {
                    new GroupClipsCommand(this, clipGrpInfos, transitionGrpInfos, false, doIt, command);
                }
                else {
                    new GroupClipsCommand(this, clipGrpInfos, transitionGrpInfos, false, doIt, metaCommand);
                }
                if (!doIt) {
                    //Action must be performed right now
                    doGroupClips(clipGrpInfos, transitionGrpInfos, false);
                }
            }
        }
        if (command) {
            // Create new group
            new GroupClipsCommand(this, clipInfos, transitionInfos, group, doIt, command);
        } else {
            if (metaCommand) {
                new GroupClipsCommand(this, clipInfos, transitionInfos, group, doIt, metaCommand);
                m_commandStack->push(metaCommand);
            }
            else {
                GroupClipsCommand *command = new GroupClipsCommand(this, clipInfos, transitionInfos, group, doIt);
                m_commandStack->push(command);
            }
        }
        if (!doIt) {
            //Action must be performed right now
            doGroupClips(clipInfos, transitionInfos, group);
        }
    }
}

void CustomTrackView::doGroupClips(QList <ItemInfo> clipInfos, QList <ItemInfo> transitionInfos, bool group)
{
    resetSelectionGroup();
    m_scene->clearSelection();
    if (!group) {
        // ungroup, find main group to destroy it...
        for (int i = 0; i < clipInfos.count(); ++i) {
            ClipItem *clip = getClipItemAtStart(clipInfos.at(i).startPos, clipInfos.at(i).track);
            if (clip == NULL) {
                qDebug()<<" * ** Cannot find UNGROUP clip at: "<<clipInfos.at(i).startPos.frames(25)<<" / "<<clipInfos.at(i).track;
                continue;
            }
            if (clip->parentItem() && clip->parentItem()->type() == GroupWidget) {
                AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(clip->parentItem());
                m_document->clipManager()->removeGroup(grp);
                if (grp == m_selectionGroup) m_selectionGroup = NULL;
                scene()->destroyItemGroup(grp);
            }
            clip->setItemLocked(m_timeline->getTrackInfo(clip->track()).isLocked);
        }
        for (int i = 0; i < transitionInfos.count(); ++i) {
            Transition *tr = getTransitionItemAt(transitionInfos.at(i).startPos, transitionInfos.at(i).track);
            if (tr == NULL) continue;
            if (tr->parentItem() && tr->parentItem()->type() == GroupWidget) {
                AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(tr->parentItem());
                m_document->clipManager()->removeGroup(grp);
                if (grp == m_selectionGroup) m_selectionGroup = NULL;
                scene()->destroyItemGroup(grp);
                grp = NULL;
            }
            tr->setItemLocked(m_timeline->getTrackInfo(tr->track()).isLocked);
        }
        return;
    }
    QList <QGraphicsItem *>list;
    for (int i = 0; i < clipInfos.count(); ++i) {
        ClipItem *clip = getClipItemAtStart(clipInfos.at(i).startPos, clipInfos.at(i).track);
        if (clip) {
            list.append(clip);
            //clip->setSelected(true);
        }
    }
    for (int i = 0; i < transitionInfos.count(); ++i) {
        Transition *clip = getTransitionItemAt(transitionInfos.at(i).startPos, transitionInfos.at(i).track);
        if (clip) {
            list.append(clip);
            //clip->setSelected(true);
        }
    }
    groupSelectedItems(list, true, true);
}

void CustomTrackView::slotInfoProcessingFinished()
{
    m_producerNotReady.wakeAll();
}

void CustomTrackView::addClip(const QString &clipId, ItemInfo info, EffectsList effects, PlaylistState::ClipState state, bool refresh)
{
    ProjectClip *binClip = m_document->getBinClip(clipId);
    if (!binClip) {
        emit displayMessage(i18n("Cannot insert clip..."), ErrorMessage);
        return;
    }
    if (!binClip->isReady()) {
        // If the clip has no producer, we must wait until it is created...
        emit displayMessage(i18n("Waiting for clip..."), InformationMessage);
	m_document->forceProcessing(clipId);
        // If the clip is not ready, give it 10x3 seconds to complete the task...
        for (int i = 0; i < 10; ++i) {
            if (!binClip->isReady()) {
                m_mutex.lock();
                m_producerNotReady.wait(&m_mutex, 3000);
                m_mutex.unlock();
            } else break;
        }
        if (!binClip->isReady()) {
            emit displayMessage(i18n("Cannot insert clip..."), ErrorMessage);
            return;
        }
        emit displayMessage(QString(), InformationMessage);
    }
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    // Get speed and strobe values from effects
    double speed = 1.0;
    int strobe = 1;
    QDomElement speedEffect = effects.effectById("speed");
    if (!speedEffect.isNull()) {
        QDomNodeList nodes = speedEffect.elementsByTagName("parameter");
        for (int i = 0; i < nodes.count(); ++i) {
            QDomElement e = nodes.item(i).toElement();
            if (e.attribute("name") == "speed") {
                speed = locale.toDouble(e.attribute("value", "1"));
                int factor = e.attribute("factor", "1").toInt();
                speed /= factor;
                if (speed == 0) speed = 1;
            }
            else if (e.attribute("name") == "strobe") {
                strobe = e.attribute("value", "1").toInt();
            }
        }
    }
    ClipItem *item = new ClipItem(binClip, info, m_document->fps(), speed, strobe, getFrameWidth());
    connect(item, &AbstractClipItem::selectItem, this, &CustomTrackView::slotSelectItem);
    item->setPos(info.startPos.frames(m_document->fps()), getPositionFromTrack(info.track) + 1 + item->itemOffset());
    item->setState(state);
    item->setEffectList(effects);
    scene()->addItem(item);
    bool isLocked = m_timeline->getTrackInfo(info.track).isLocked;
    if (isLocked) item->setItemLocked(true);
    bool duplicate = true;
    Mlt::Producer *prod;
    if (speed != 1.0) {
        prod = m_document->renderer()->getBinProducer(clipId);
        QString url = QString::fromUtf8(prod->get("resource"));
        //url.append('?' + locale.toString(speed));
        Track::SlowmoInfo slowInfo;
        slowInfo.speed = speed;
        slowInfo.strobe = strobe;
        slowInfo.state = state;
        Mlt::Producer *copy = m_document->renderer()->getSlowmotionProducer(slowInfo.toString(locale) + url);
        if (copy == NULL) {
            url.prepend(locale.toString(speed) + ":");
            Mlt::Properties passProperties;
            Mlt::Properties original(prod->get_properties());
            passProperties.pass_list(original, ClipController::getPassPropertiesList(false));
            copy = m_timeline->track(info.track)->buildSlowMoProducer(passProperties, url, clipId, slowInfo);
        }
        if (copy == NULL) {
            emit displayMessage(i18n("Cannot insert clip..."), ErrorMessage);
            return;
        }
        prod = copy;
        duplicate = false;
    }
    else if (item->clipState() == PlaylistState::VideoOnly) {
        prod = m_document->renderer()->getBinVideoProducer(clipId);
    }
    else {
        prod = m_document->renderer()->getBinProducer(clipId);
    }
    binClip->addRef();
    m_timeline->track(info.track)->add(info.startPos.seconds(), prod, info.cropStart.seconds(), (info.cropStart+info.cropDuration).seconds(), state, duplicate, TimelineMode::NormalEdit);// m_scene->editMode());

    for (int i = 0; i < item->effectsCount(); ++i) {
        m_timeline->track(info.track)->addEffect(info.startPos.seconds(), EffectsController::getEffectArgs(m_document->getProfileInfo(), item->effect(i)));
    }
    if (refresh && item->hasVisibleVideo()) {
        monitorRefresh(info, true);
    }
}

void CustomTrackView::slotUpdateClip(const QString &clipId, bool reload)
{
    QMutexLocker locker(&m_mutex);
    QList<QGraphicsItem *> list = scene()->items();
    QList <ClipItem *>clipList;
    ClipItem *clip = NULL;
    //TODO: move the track replacement code in track.cpp
    Mlt::Tractor *tractor = m_document->renderer()->lockService();
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->type() == AVWidget) {
            clip = static_cast <ClipItem *>(list.at(i));
            if (clip->getBinId() == clipId) {
                //TODO: get audio / video only producers
                /*ItemInfo info = clip->info();
                if (clip->isAudioOnly()) prod = baseClip->getTrackProducer(info.track);
                else if (clip->isVideoOnly()) prod = baseClip->getTrackProducer(info.track);
                else prod = baseClip->getTrackProducer(info.track);*/
                if (reload) {
                    /*Mlt::Producer *prod = m_document->renderer()->getTrackProducer(clipId, info.track, clip->isAudioOnly(), clip->isVideoOnly());
                    if (!m_document->renderer()->mltUpdateClip(tractor, info, clip->xml(), prod)) {
                        emit displayMessage(i18n("Cannot update clip (time: %1, track: %2)", info.startPos.frames(m_document->fps()), info.track), ErrorMessage);
                    }*/
                }
                else clipList.append(clip);
            }
        }
    }
    for (int i = 0; i < clipList.count(); ++i)
        clipList.at(i)->refreshClip(true, true);
    m_document->renderer()->unlockService(tractor);
}

ClipItem *CustomTrackView::getClipItemAtEnd(GenTime pos, int track)
{
    int framepos = (int)(pos.frames(m_document->fps()));
    QList<QGraphicsItem *> list = scene()->items(QPointF(framepos - 1, getPositionFromTrack(track) + m_tracksHeight / 2));
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (!list.at(i)->isEnabled()) continue;
        if (list.at(i)->type() == AVWidget) {
            ClipItem *test = static_cast <ClipItem *>(list.at(i));
            if (test->endPos() == pos) clip = test;
            break;
        }
    }
    return clip;
}

ClipItem *CustomTrackView::getClipItemAtStart(GenTime pos, int track, GenTime end)
{
    QList<QGraphicsItem *> list = scene()->items(QPointF(pos.frames(m_document->fps()), getPositionFromTrack(track) + m_tracksHeight / 2));
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (!list.at(i)->isEnabled()) {
            /*ClipItem *test = static_cast <ClipItem *>(list.at(i));
            qDebug()<<" * * *DISABLED CLIP: "<<i<<", "<<test->startPos().frames(25);*/
            continue;
        }
        if (list.at(i)->type() == AVWidget) {
            ClipItem *test = static_cast <ClipItem *>(list.at(i));
            //qDebug()<<" * * *ENABLED CLIP: "<<i<<", "<<test->startPos().frames(25)<<" = "<<pos.frames(25);
            if (test->startPos() == pos) {
              if (end > GenTime()) {
                  if (test->endPos() != end) {
                      //qDebug()<<" - - - -- - -NO END MATCH: "<<test->endPos().frames(25)<<" = "<< end.frames(25);
                      continue;
                  }
              }
	      clip = test;
	      break;
	    }
	}
    }
    return clip;
}

ClipItem *CustomTrackView::getMovedClipItem(ItemInfo info, GenTime offset, int trackOffset)
{
    QList<QGraphicsItem *> list = scene()->items(QPointF((info.startPos + offset).frames(m_document->fps()), getPositionFromTrack(info.track + trackOffset) + m_tracksHeight / 2));
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        //if (!list.at(i)->isEnabled()) continue;
        if (list.at(i)->type() == AVWidget) {
            ClipItem *test = static_cast <ClipItem *>(list.at(i));
            if (test->startPos() == info.startPos) {
		if (test->endPos() != info.endPos) {
		    continue;
                }
            }
	    clip = test;
	    break;
	}
    }
    return clip;
}

ClipItem *CustomTrackView::getClipItemAtMiddlePoint(int pos, int track)
{
    const QPointF p(pos, getPositionFromTrack(track) + m_tracksHeight / 2);
    QList<QGraphicsItem *> list = scene()->items(p);
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (!list.at(i)->isEnabled()) continue;
        if (list.at(i)->type() == AVWidget) {
            clip = static_cast <ClipItem *>(list.at(i));
            break;
        }
    }
    return clip;
}

Transition *CustomTrackView::getTransitionItemAt(int pos, int track, bool alreadyMoved)
{
    const QPointF p(pos, getPositionFromTrack(track) + Transition::itemOffset() + 1);
    QList<QGraphicsItem *> list = scene()->items(p);
    Transition *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (!alreadyMoved && !list.at(i)->isEnabled()) {
            continue;
        }
        if (list.at(i)->type() == TransitionWidget) {
            clip = static_cast <Transition *>(list.at(i));
            break;
        }
    }
    return clip;
}

Transition *CustomTrackView::getTransitionItemAt(GenTime pos, int track, bool alreadyMoved)
{
    return getTransitionItemAt(pos.frames(m_document->fps()), track, alreadyMoved);
}

Transition *CustomTrackView::getTransitionItemAtEnd(GenTime pos, int track)
{
    int framepos = (int)(pos.frames(m_document->fps()));
    const QPointF p(framepos - 1, getPositionFromTrack(track) + Transition::itemOffset() + 1);
    QList<QGraphicsItem *> list = scene()->items(p);
    Transition *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (!list.at(i)->isEnabled()) continue;
        if (list.at(i)->type() == TransitionWidget) {
            Transition *test = static_cast <Transition *>(list.at(i));
            if (test->endPos() == pos) clip = test;
            break;
        }
    }
    return clip;
}

Transition *CustomTrackView::getTransitionItemAtStart(GenTime pos, int track)
{
    const QPointF p(pos.frames(m_document->fps()), getPositionFromTrack(track) + Transition::itemOffset() + 1);
    QList<QGraphicsItem *> list = scene()->items(p);
    Transition *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (!list.at(i)->isEnabled()) continue;
        if (list.at(i)->type() == TransitionWidget) {
            Transition *test = static_cast <Transition *>(list.at(i));
            if (test->startPos() == pos) clip = test;
            break;
        }
    }
    return clip;
}

bool CustomTrackView::moveClip(const ItemInfo &start, const ItemInfo &end, bool refresh, bool alreadyMoved, ItemInfo *out_actualEnd)
{
    if (m_selectionGroup) {
        resetSelectionGroup(false);
    }
    ClipItem *item = NULL;
    if (alreadyMoved) {
        item = getClipItemAtStart(end.startPos, end.track);
    } else {
        item = getClipItemAtStart(start.startPos, start.track);
    }
    if (!item) {
        emit displayMessage(i18n("Cannot move clip at time: %1 on track %2", m_document->timecode().getTimecodeFromFrames(start.startPos.frames(m_document->fps())), start.track), ErrorMessage);
        //qDebug() << "----------------  ERROR, CANNOT find clip to move at.. ";
        return false;
    }

#ifdef DEBUG
    qDebug() << "Moving item " << (long)item << " from .. to:";
    qDebug() << item->info();
    qDebug() << start;
    qDebug() << end;
#endif
    bool success = m_timeline->moveClip(start.track, start.startPos.seconds(), end.track, end.startPos.seconds(), item->clipState(), m_scene->editMode(), item->needsDuplicate());
    QList <ItemInfo> range;
    if (item->hasVisibleVideo()) {
        range << start << end;
    } else {
        refresh = false;
    }
    if (!success) {
        // undo last move and emit error message
        emit displayMessage(i18n("Cannot move clip to position %1", m_document->timecode().getTimecodeFromFrames(end.startPos.frames(m_document->fps()))), ErrorMessage);
    } else if (!alreadyMoved) {
        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);
        item->setPos((int) end.startPos.frames(m_document->fps()), getPositionFromTrack(end.track) + 1);

        bool isLocked = m_timeline->getTrackInfo(end.track).isLocked;
        m_scene->clearSelection();
        if (isLocked) item->setItemLocked(true);
        else {
            if (item->isItemLocked()) item->setItemLocked(false);
            item->setSelected(true);
        }
        //TODO
        /*
        if (item->baseClip()->isTransparent()) {
            // Also move automatic transition
            Transition *tr = getTransitionItemAt(start.startPos, start.track);
            if (tr && tr->isAutomatic()) {
                tr->updateTransitionEndTrack(getPreviousVideoTrack(end.track));
                m_document->renderer()->mltMoveTransition(tr->transitionTag(), m_timeline->tracksCount() - start.track, m_timeline->tracksCount() - end.track, tr->transitionEndTrack(), start.startPos, start.endPos, end.startPos, end.endPos);
                tr->setPos((int) end.startPos.frames(m_document->fps()), (int)(end.track * m_tracksHeight + 1));
            }
        }*/
        KdenliveSettings::setSnaptopoints(snap);
    }
    if (refresh)
        monitorRefresh(range, true);
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

void CustomTrackView::moveGroup(QList<ItemInfo> startClip, QList<ItemInfo> startTransition, const GenTime &offset, const int trackOffset, bool alreadyMoved, bool reverseMove)
{
    // Group Items
    resetSelectionGroup();
    m_scene->clearSelection();
    m_selectionMutex.lock();
    m_selectionGroup = new AbstractGroupItem(m_document->fps());
    scene()->addItem(m_selectionGroup);
    QList <ItemInfo> range;
    m_document->renderer()->blockSignals(true);
    for (int i = 0; i < startClip.count(); ++i) {
        if (reverseMove) {
            startClip[i].startPos = startClip.at(i).startPos - offset;
            startClip[i].track = startClip.at(i).track - trackOffset;
        }
        ClipItem *clip = NULL;
	if (alreadyMoved) {
	    clip = getMovedClipItem(startClip.at(i), offset, trackOffset);
	} else {
	    clip = getClipItemAtStart(startClip.at(i).startPos, startClip.at(i).track);
	}
        if (clip) {
            if (clip->hasVisibleVideo()) {
                ItemInfo updateInfo = startClip.at(i);
                range << updateInfo;
                updateInfo.startPos += offset;
                updateInfo.endPos += offset;
                range << updateInfo;
            }
            if (clip->parentItem()) {
                m_selectionGroup->addItem(clip->parentItem());
                // If timeline clip is already at destination, make sure it is not moved twice
                if (alreadyMoved)
                    clip->parentItem()->setEnabled(false);
            } else {
                m_selectionGroup->addItem(clip);
                if (alreadyMoved)
                    clip->setEnabled(false);
            }
            m_timeline->track(startClip.at(i).track)->del(startClip.at(i).startPos.seconds(), false);
        } else qDebug() << "//MISSING CLIP AT: " << startClip.at(i).startPos.frames(25)<<" / track: "<<startClip.at(i).track<<" / OFFSET: "<<trackOffset;
    }
    for (int i = 0; i < startTransition.count(); ++i) {
        if (reverseMove) {
            startTransition[i].startPos = startTransition.at(i).startPos - offset;
            startTransition[i].track = startTransition.at(i).track - trackOffset;
        }
        Transition *tr = NULL;
	if (alreadyMoved)
	    tr = getTransitionItemAt(startTransition.at(i).startPos + offset, startTransition.at(i).track + trackOffset, true);
	else 
	    tr = getTransitionItemAt(startTransition.at(i).startPos, startTransition.at(i).track);
        if (tr) {
            ItemInfo updateInfo = startTransition.at(i);
            range << updateInfo;
            updateInfo.startPos += offset;
            updateInfo.endPos += offset;
            range << updateInfo;
            if (tr->parentItem()) {
                m_selectionGroup->addItem(tr->parentItem());
                if (alreadyMoved)
                    tr->parentItem()->setEnabled(false);
            } else {
                m_selectionGroup->addItem(tr);
                if (alreadyMoved)
                    tr->setEnabled(false);
            }
            m_timeline->transitionHandler->deleteTransition(tr->transitionTag(), tr->transitionEndTrack(), startTransition.at(i).track, startTransition.at(i).startPos, startTransition.at(i).endPos, tr->toXML());
        } else qDebug() << "//MISSING TRANSITION AT: " << startTransition.at(i).startPos.frames(25)<<", track: "<<startTransition.at(i).track;
    }
    m_document->renderer()->blockSignals(false);

    if (m_selectionGroup) {
        bool snap = KdenliveSettings::snaptopoints();
        KdenliveSettings::setSnaptopoints(false);

	if (!alreadyMoved)
	    m_selectionGroup->setTransform(QTransform::fromTranslate(offset.frames(m_document->fps()), -trackOffset *(qreal) m_tracksHeight), true);

        QList<QGraphicsItem *> children = m_selectionGroup->childItems();
        QList <AbstractGroupItem*> groupList;
        // Expand groups
        int max = children.count();
        for (int i = 0; i < max; ++i) {
            if (children.at(i)->type() == GroupWidget) {
                QList <QGraphicsItem *> groupChildren = children.at(i)->childItems();
                for (int j = 0; j < groupChildren.count(); j++) {
                    AbstractClipItem *item = static_cast<AbstractClipItem*>(groupChildren.at(j));
                    ItemInfo nfo = item->info();
                    item->updateItem(nfo.track + trackOffset);
                }
                children += groupChildren;
                //AbstractGroupItem *grp = static_cast<AbstractGroupItem *>(children.at(i));
                //grp->moveBy(offset.frames(m_document->fps()), trackOffset *(qreal) m_tracksHeight);
                /*m_document->clipManager()->removeGroup(grp);
                m_scene->destroyItemGroup(grp);*/
                AbstractGroupItem *group = static_cast<AbstractGroupItem*>(children.at(i));
                if (!groupList.contains(group)) groupList.append(group);
                children.removeAll(children.at(i));
                --i;
            }
        }
        for (int i = 0; i < children.count(); ++i) {
            // re-add items in correct place
            if (children.at(i)->type() != AVWidget && children.at(i)->type() != TransitionWidget) continue;
            AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
            item->setEnabled(true);
            ItemInfo info = item->info();
            if (!alreadyMoved) {
                item->updateItem(info.track + trackOffset);
                info = item->info();
            }
            bool isLocked = m_timeline->getTrackInfo(info.track).isLocked;
            if (isLocked)
                item->setItemLocked(true);

            if (item->type() == AVWidget) {
                ClipItem *clip = static_cast <ClipItem*>(item);
                Mlt::Producer *prod;
                if (clip->clipState() == PlaylistState::VideoOnly) {
                    prod = m_document->renderer()->getBinVideoProducer(clip->getBinId());
                }
                else {
                    prod = m_document->renderer()->getBinProducer(clip->getBinId());
                }
                m_timeline->track(info.track)->add(info.startPos.seconds(), prod, info.cropStart.seconds(), (info.cropStart + info.cropDuration).seconds(), clip->clipState(), true, m_scene->editMode());

                for (int i = 0; i < clip->effectsCount(); ++i) {
                    m_timeline->track(info.track)->addEffect(info.startPos.seconds(), EffectsController::getEffectArgs(m_document->getProfileInfo(), clip->effect(i)));
                }
            } else if (item->type() == TransitionWidget) {
                Transition *tr = static_cast <Transition*>(item);
                int newTrack;
                if (!tr->forcedTrack()) {
                    newTrack = getPreviousVideoTrack(info.track);
                } else {
                    newTrack = tr->transitionEndTrack() + trackOffset;
                    if (newTrack < 0 || newTrack > m_timeline->tracksCount()) newTrack = getPreviousVideoTrack(info.track);
                }
                tr->updateTransitionEndTrack(newTrack);
                m_timeline->transitionHandler->addTransition(tr->transitionTag(), newTrack, info.track, info.startPos, info.endPos, tr->toXML());
            }
        }
        m_selectionMutex.unlock();
        resetSelectionGroup(true);
        for (int i = 0; i < groupList.count(); ++i) {
            rebuildGroup(groupList.at(i));
        }
        groupSelectedItems(children, false);
        m_timeline->checkDuration();
        //clearSelection();
        KdenliveSettings::setSnaptopoints(snap);
        //TODO: calculate affected ranges and invalidate previews
        monitorRefresh(range, true);
    } else qDebug() << "///////// WARNING; NO GROUP TO MOVE";
}

void CustomTrackView::moveTransition(const ItemInfo &start, const ItemInfo &end, bool refresh)
{
    Transition *item = getTransitionItemAt(start.startPos, start.track);
    if (!item) {
        emit displayMessage(i18n("Cannot move transition at time: %1 on track %2", m_document->timecode().getTimecodeFromFrames(start.startPos.frames(m_document->fps())), start.track), ErrorMessage);
        //qDebug() << "----------------  ERROR, CANNOT find transition to move... ";// << startPos.x() * m_scale * FRAME_SIZE + 1 << ", " << startPos.y() * m_tracksHeight + m_tracksHeight / 2;
        return;
    }

    bool snap = KdenliveSettings::snaptopoints();
    KdenliveSettings::setSnaptopoints(false);

    if (end.endPos - end.startPos == start.endPos - start.startPos) {
        // Transition was moved
        item->setPos((int) end.startPos.frames(m_document->fps()), getPositionFromTrack(end.track) + 1);
    } else if (end.endPos == start.endPos) {
        // Transition start resize
        item->resizeStart((int) end.startPos.frames(m_document->fps()));
    } else if (end.startPos == start.startPos) {
        // Transition end resize;
        //qDebug() << "// resize END: " << end.endPos.frames(m_document->fps());
        item->resizeEnd((int) end.endPos.frames(m_document->fps()));
    } else {
        // Move & resize
        item->setPos((int) end.startPos.frames(m_document->fps()), getPositionFromTrack(end.track) + 1);
        item->resizeStart((int) end.startPos.frames(m_document->fps()));
        item->resizeEnd((int) end.endPos.frames(m_document->fps()));
    }
    //item->transitionHandler->moveTransition(GenTime((int) (endPos.x() - startPos.x()), m_document->fps()));
    KdenliveSettings::setSnaptopoints(snap);
    item->updateTransitionEndTrack(getPreviousVideoTrack(end.track));
    m_timeline->transitionHandler->moveTransition(item->transitionTag(), start.track, item->track(), item->transitionEndTrack(), start.startPos, start.endPos, item->startPos(), item->endPos());
    if (m_dragItem && m_dragItem == item) {
        QPoint p;
        ClipItem *transitionClip = getClipItemAtStart(item->startPos(), item->track());
        if (transitionClip && transitionClip->binClip()) {
            int frameWidth = transitionClip->binClip()->getProducerIntProperty(QStringLiteral("meta.media.width"));
            int frameHeight = transitionClip->binClip()->getProducerIntProperty(QStringLiteral("meta.media.height"));
            double factor = transitionClip->binClip()->getProducerProperty(QStringLiteral("aspect_ratio")).toDouble();
            if (factor == 0) factor = 1.0;
            p.setX((int)(frameWidth * factor + 0.5));
            p.setY(frameHeight);
        }
        emit transitionItemSelected(item, getPreviousVideoTrack(item->track()), p);
    }
    if (refresh) {
        monitorRefresh(QList <ItemInfo>() << start << end, true);
    }
}

void CustomTrackView::resizeClip(const ItemInfo &start, const ItemInfo &end, bool dontWorry)
{
    bool resizeClipStart = (start.startPos != end.startPos);
    ClipItem *item = getClipItemAtStart(start.startPos, start.track, start.startPos + start.cropDuration);
    if (!item) {
        if (dontWorry) return;
        emit displayMessage(i18n("Cannot move clip at time: %1 on track %2", m_document->timecode().getTimecodeFromFrames(start.startPos.frames(m_document->fps())), start.track), ErrorMessage);
        //qDebug() << "----------------  ERROR, CANNOT find clip to resize at... "; // << startPos;
        return;
    }
    if (item->parentItem()) {
        // Item is part of a group, reset group
        resetSelectionGroup();
    }

    bool snap = KdenliveSettings::snaptopoints();
    KdenliveSettings::setSnaptopoints(false);

    if (resizeClipStart) {
        if (m_timeline->track(start.track)->resize(start.startPos.seconds(), (end.startPos - start.startPos).seconds(), false))
            item->resizeStart((int) end.startPos.frames(m_document->fps()));
    } else {
        if (m_timeline->track(start.track)->resize(start.startPos.seconds(), (end.endPos - start.endPos).seconds(), true))
            item->resizeEnd((int) end.endPos.frames(m_document->fps()));
    }

    if (!resizeClipStart && end.cropStart != start.cropStart) {
        //qDebug() << "// RESIZE CROP, DIFF: " << (end.cropStart - start.cropStart).frames(25);
        ItemInfo clipinfo = end;
        clipinfo.track = end.track;
        bool success = m_document->renderer()->mltResizeClipCrop(clipinfo, end.cropStart);
        if (success) {
            item->setCropStart(end.cropStart);
            item->resetThumbs(true);
        }
    }
    if (item->hasVisibleVideo()) {
        ItemInfo range;
        if (resizeClipStart) {
            if (start.startPos < end.startPos) {
                range.startPos = start.startPos;
                range.endPos = end.startPos;
            } else {
                range.startPos = end.startPos;
                range.endPos = start.startPos;
            }
        } else {
            if (start.endPos < end.endPos) {
                range.startPos = start.endPos;
                range.endPos = end.endPos;
            } else {
                range.startPos = end.endPos;
                range.endPos = start.endPos;
            }
        }
        monitorRefresh(range, true);
    }
    if (item == m_dragItem) {
        // clip is selected, update effect stack
        emit clipItemSelected(item);
    }
    KdenliveSettings::setSnaptopoints(snap);
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
    if (item->type() == AVWidget) {
        bool success = m_timeline->track(oldInfo.track)->resize(oldInfo.startPos.seconds(), (item->startPos() - oldInfo.startPos).seconds(), false);
        if (success) {
            // Check if there is an automatic transition on that clip (lower track)
            Transition *transition = getTransitionItemAtStart(oldInfo.startPos, oldInfo.track);
            if (transition && transition->isAutomatic()) {
                ItemInfo trInfo = transition->info();
                ItemInfo newTrInfo = trInfo;
                newTrInfo.startPos = item->startPos();
                newTrInfo.cropDuration = trInfo.endPos - newTrInfo.startPos;
                if (newTrInfo.startPos < newTrInfo.endPos) {
                    QDomElement old = transition->toXML();
                    if (transition->updateKeyframes(trInfo, newTrInfo)) {
                        QDomElement xml = transition->toXML();
                        m_timeline->transitionHandler->updateTransition(xml.attribute("tag"), xml.attribute("tag"), xml.attribute("transition_btrack").toInt(),  xml.attribute("transition_atrack").toInt(), newTrInfo.startPos, newTrInfo.endPos, xml);
                        new EditTransitionCommand(this, transition->track(), transition->startPos(), old, xml, false, command);
                    }
                    new MoveTransitionCommand(this, trInfo, newTrInfo, true, false, command);
                }
            }
            // Check if there is an automatic transition on that clip (upper track)
            transition = getTransitionItemAtStart(oldInfo.startPos, oldInfo.track + 1);
            if (transition && transition->isAutomatic() && transition->transitionEndTrack() == oldInfo.track) {
                ItemInfo trInfo = transition->info();
                ItemInfo newTrInfo = trInfo;
                newTrInfo.startPos = item->startPos();
                newTrInfo.cropDuration = trInfo.endPos - newTrInfo.startPos;
                ClipItem * upperClip = getClipItemAtStart(oldInfo.startPos, oldInfo.track + 1);
                //TODO
                if ((!upperClip /*|| !upperClip->baseClip()->isTransparent()*/) && newTrInfo.startPos < newTrInfo.endPos) {
                    QDomElement old = transition->toXML();
                    if (transition->updateKeyframes(trInfo, newTrInfo)) {
                        QDomElement xml = transition->toXML();
                        m_timeline->transitionHandler->updateTransition(xml.attribute("tag"), xml.attribute("tag"), xml.attribute("transition_btrack").toInt(),  xml.attribute("transition_atrack").toInt(), newTrInfo.startPos, newTrInfo.endPos, xml);
                        new EditTransitionCommand(this, transition->track(), transition->startPos(), old, xml, false, command);
                    }
                    new MoveTransitionCommand(this, trInfo, newTrInfo, true, false, command);
                }
            }

            ClipItem *clip = static_cast < ClipItem * >(item);

            // Hack:
            // Since we must always resize clip before updating the keyframes, we
            // put a resize command before & after checking keyframes so that
            // we are sure the resize is performed before whenever we do or undo the action
            // TODO: find a way to apply adjusteffect after the resize command was done / undone
            new ResizeClipCommand(this, oldInfo, info, false, true, command);
            adjustEffects(clip, oldInfo, command);
        } else {
            KdenliveSettings::setSnaptopoints(false);
            item->resizeStart((int) oldInfo.startPos.frames(m_document->fps()));
            KdenliveSettings::setSnaptopoints(snap);
            emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
        }
    } else if (item->type() == TransitionWidget) {
        Transition *transition = static_cast <Transition *>(item);
        if (!m_timeline->transitionHandler->moveTransition(transition->transitionTag(), oldInfo.track, oldInfo.track, transition->transitionEndTrack(), oldInfo.startPos, oldInfo.endPos, info.startPos, info.endPos)) {
            // Cannot resize transition
            KdenliveSettings::setSnaptopoints(false);
            transition->resizeStart((int) oldInfo.startPos.frames(m_document->fps()));
            KdenliveSettings::setSnaptopoints(snap);
            emit displayMessage(i18n("Cannot resize transition"), ErrorMessage);
        } else {
            QDomElement old = transition->toXML();
            if (transition->updateKeyframes(oldInfo, info)) {
                QDomElement xml = transition->toXML();
                m_timeline->transitionHandler->updateTransition(xml.attribute("tag"), xml.attribute("tag"), xml.attribute("transition_btrack").toInt(), xml.attribute("transition_atrack").toInt(), info.startPos, info.endPos, xml);
                new EditTransitionCommand(this, transition->track(), transition->startPos(), old, xml, false, command);
            }
            updateTransitionWidget(transition, info);
            new MoveTransitionCommand(this, oldInfo, info, false, true, command);
        }
    }
    if (item->parentItem() && item->parentItem() != m_selectionGroup) {
        new RebuildGroupCommand(this, item->info().track, item->endPos() - GenTime(1, m_document->fps()), command);
    }

    if (!hasParentCommand) {
        m_commandStack->push(command);
    }
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
    if (item->type() == AVWidget) {
        if (!hasParentCommand) command->setText(i18n("Resize clip end"));
        bool success = m_timeline->track(info.track)->resize(oldInfo.startPos.seconds(), (info.endPos - oldInfo.endPos).seconds(), true);
        if (success) {
            // Check if there is an automatic transition on that clip (lower track)
            Transition *tr = getTransitionItemAtEnd(oldInfo.endPos, oldInfo.track);
            if (tr && tr->isAutomatic()) {
                ItemInfo trInfo = tr->info();
                ItemInfo newTrInfo = trInfo;
                newTrInfo.endPos = item->endPos();
                newTrInfo.cropDuration = newTrInfo.endPos - trInfo.startPos;
                if (newTrInfo.endPos > newTrInfo.startPos) {
                  QDomElement old = tr->toXML();
                  if (tr->updateKeyframes(trInfo, newTrInfo)) {
                      QDomElement xml = tr->toXML();
                      m_timeline->transitionHandler->updateTransition(xml.attribute("tag"), xml.attribute("tag"), xml.attribute("transition_btrack").toInt(), xml.attribute("transition_atrack").toInt(), newTrInfo.startPos, newTrInfo.endPos, xml);
                      new EditTransitionCommand(this, tr->track(), tr->startPos(), old, xml, false, command);
                  }
                  new MoveTransitionCommand(this, trInfo, newTrInfo, true, false, command);
                }
            }

            // Check if there is an automatic transition on that clip (upper track)
            tr = getTransitionItemAtEnd(oldInfo.endPos, oldInfo.track - 1);
            if (tr && tr->isAutomatic() && tr->transitionEndTrack() == oldInfo.track) {
                ItemInfo trInfo = tr->info();
                ItemInfo newTrInfo = trInfo;
                newTrInfo.endPos = item->endPos();
                newTrInfo.cropDuration = newTrInfo.endPos - trInfo.startPos;
                ClipItem * upperClip = getClipItemAtEnd(oldInfo.endPos, oldInfo.track + 1);
                //TODO
                if ((!upperClip /*|| !upperClip->baseClip()->isTransparent()*/) && newTrInfo.endPos > newTrInfo.startPos) {
                    QDomElement old = tr->toXML();
                    if (tr->updateKeyframes(trInfo, newTrInfo)) {
                        QDomElement xml = tr->toXML();
                        m_timeline->transitionHandler->updateTransition(xml.attribute("tag"), xml.attribute("tag"), xml.attribute("transition_btrack").toInt(), xml.attribute("transition_atrack").toInt(), newTrInfo.startPos, newTrInfo.endPos, xml);
                        new EditTransitionCommand(this, tr->track(), tr->startPos(), old, xml, false, command);
                    }
                    new MoveTransitionCommand(this, trInfo, newTrInfo, true, false, command);
                }

            }

            ClipItem *clip = static_cast < ClipItem * >(item);

            // Hack:
            // Since we must always resize clip before updating the keyframes, we
            // put a resize command before & after checking keyframes so that
            // we are sure the resize is performed before whenever we do or undo the action
            // TODO: find a way to apply adjusteffect after the resize command was done / undone
            new ResizeClipCommand(this, oldInfo, info, false, true, command);
            adjustEffects(clip, oldInfo, command);
        } else {
            KdenliveSettings::setSnaptopoints(false);
            item->resizeEnd((int) oldInfo.endPos.frames(m_document->fps()));
            KdenliveSettings::setSnaptopoints(true);
            emit displayMessage(i18n("Error when resizing clip"), ErrorMessage);
        }
    } else if (item->type() == TransitionWidget) {
        if (!hasParentCommand) command->setText(i18n("Resize transition end"));
        Transition *transition = static_cast <Transition *>(item);
        if (!m_timeline->transitionHandler->moveTransition(transition->transitionTag(), oldInfo.track, oldInfo.track, transition->transitionEndTrack(), oldInfo.startPos, oldInfo.endPos, info.startPos, info.endPos)) {
            // Cannot resize transition
            KdenliveSettings::setSnaptopoints(false);
            transition->resizeEnd((int) oldInfo.endPos.frames(m_document->fps()));
            KdenliveSettings::setSnaptopoints(true);
            emit displayMessage(i18n("Cannot resize transition"), ErrorMessage);
        } else {
            // Check transition keyframes
            QDomElement old = transition->toXML();
            ItemInfo info = transition->info();
            if (transition->updateKeyframes(oldInfo, info)) {
                QDomElement xml = transition->toXML();
                m_timeline->transitionHandler->updateTransition(xml.attribute(QStringLiteral("tag")), xml.attribute(QStringLiteral("tag")), xml.attribute(QStringLiteral("transition_btrack")).toInt(), xml.attribute(QStringLiteral("transition_atrack")).toInt(), transition->startPos(), transition->endPos(), xml);
                new EditTransitionCommand(this, transition->track(), transition->startPos(), old, xml, false, command);
            }
            updateTransitionWidget(transition, info);
            new MoveTransitionCommand(this, oldInfo, info, false, true, command);
        }
    }
    if (item->parentItem() && item->parentItem() != m_selectionGroup)
        new RebuildGroupCommand(this, item->info().track, item->startPos(), command);

    if (!hasParentCommand) {
        updateTrackDuration(oldInfo.track, command);
        m_commandStack->push(command);
    }
}

void CustomTrackView::updatePositionEffects(ClipItem* item, const ItemInfo &info, bool standalone)
{
    int effectPos = item->hasEffect(QStringLiteral("volume"), QStringLiteral("fadein"));
    if (effectPos != -1) {
        QDomElement effect = item->getEffectAtIndex(effectPos);
        int start = item->cropStart().frames(m_document->fps());
        int duration = EffectsList::parameter(effect, QStringLiteral("out")).toInt() - EffectsList::parameter(effect, QStringLiteral("in")).toInt();
        int max = item->cropDuration().frames(m_document->fps());
        if (duration > max) {
            // Make sure the fade effect is not longer than the clip
            duration = max;
        }
        duration += start;
        EffectsList::setParameter(effect, QStringLiteral("in"), QString::number(start));
        EffectsList::setParameter(effect, QStringLiteral("out"), QString::number(duration));
        if (!m_timeline->track(item->track())->editEffect(item->startPos().seconds(), EffectsController::getEffectArgs(m_document->getProfileInfo(), effect), false)) {
            emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
        }
        // if fade effect is displayed, update the effect edit widget with new clip duration
        if (standalone && item->isSelected() && effectPos == item->selectedEffectIndex()) {
            emit clipItemSelected(item);
        }
    }
    effectPos = item->hasEffect(QStringLiteral("brightness"), QStringLiteral("fade_from_black"));
    if (effectPos != -1) {
        QDomElement effect = item->getEffectAtIndex(effectPos);
        int start = item->cropStart().frames(m_document->fps());
        int duration = EffectsList::parameter(effect, QStringLiteral("out")).toInt() - EffectsList::parameter(effect, QStringLiteral("in")).toInt();
        int max = item->cropDuration().frames(m_document->fps());
        if (duration > max) {
            // Make sure the fade effect is not longer than the clip
            duration = max;
        }
        duration += start;
        EffectsList::setParameter(effect, QStringLiteral("in"), QString::number(start));
        EffectsList::setParameter(effect, QStringLiteral("out"), QString::number(duration));
        if (!m_timeline->track(item->track())->editEffect(item->startPos().seconds(), EffectsController::getEffectArgs(m_document->getProfileInfo(), effect), false)) {
            emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
        }
        // if fade effect is displayed, update the effect edit widget with new clip duration
        if (standalone && item->isSelected() && effectPos == item->selectedEffectIndex()) {
            emit clipItemSelected(item);
        }
    }

    effectPos = item->hasEffect(QStringLiteral("volume"), QStringLiteral("fadeout"));
    if (effectPos != -1) {
        // there is a fade out effect
        QDomElement effect = item->getEffectAtIndex(effectPos);
        int max = item->cropDuration().frames(m_document->fps());
        int end = max + item->cropStart().frames(m_document->fps()) - 1;
        int duration = EffectsList::parameter(effect, QStringLiteral("out")).toInt() - EffectsList::parameter(effect, QStringLiteral("in")).toInt();
        if (duration > max) {
            // Make sure the fade effect is not longer than the clip
            duration = max;
        }
        int start = end - duration;
        EffectsList::setParameter(effect, QStringLiteral("in"), QString::number(start));
        EffectsList::setParameter(effect, QStringLiteral("out"), QString::number(end));
        if (!m_timeline->track(item->track())->editEffect(item->startPos().seconds(), EffectsController::getEffectArgs(m_document->getProfileInfo(), effect), false)) {
            emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
        }
        // if fade effect is displayed, update the effect edit widget with new clip duration
        if (standalone && item->isSelected() && effectPos == item->selectedEffectIndex()) {
            emit clipItemSelected(item);
        }
    }

    effectPos = item->hasEffect(QStringLiteral("brightness"), QStringLiteral("fade_to_black"));
    if (effectPos != -1) {
        QDomElement effect = item->getEffectAtIndex(effectPos);
        int max = item->cropDuration().frames(m_document->fps());
        int end = max + item->cropStart().frames(m_document->fps()) - 1;
        int duration = EffectsList::parameter(effect, QStringLiteral("out")).toInt() - EffectsList::parameter(effect, QStringLiteral("in")).toInt();
        if (duration > max) {
            // Make sure the fade effect is not longer than the clip
            duration = max;
        }
        int start = end - duration;
        EffectsList::setParameter(effect, QStringLiteral("in"), QString::number(start));
        EffectsList::setParameter(effect, QStringLiteral("out"), QString::number(end));
        if (!m_timeline->track(item->track())->editEffect(item->startPos().seconds(), EffectsController::getEffectArgs(m_document->getProfileInfo(), effect), false)) {
            emit displayMessage(i18n("Problem editing effect"), ErrorMessage);
        }
        // if fade effect is displayed, update the effect edit widget with new clip duration
        if (standalone && item->isSelected() && effectPos == item->selectedEffectIndex()) {
            emit clipItemSelected(item);
        }
    }

    effectPos = item->hasEffect(QStringLiteral("freeze"), QStringLiteral("freeze"));
    if (effectPos != -1) {
        // Freeze effect needs to be adjusted with clip resize
        int diff = (info.startPos - item->startPos()).frames(m_document->fps());
        QDomElement eff = item->getEffectAtIndex(effectPos);
        if (!eff.isNull() && diff != 0) {
            int freeze_pos = EffectsList::parameter(eff, QStringLiteral("frame")).toInt() + diff;
            EffectsList::setParameter(eff, QStringLiteral("frame"), QString::number(freeze_pos));
            if (standalone && item->isSelected() && item->selectedEffect().attribute("id") == "freeze") {
                emit clipItemSelected(item);
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
    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i) == selected) continue;
        if (skipSelectedItems && itemList.at(i)->isSelected()) continue;
        if (itemList.at(i)->type() == AVWidget) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            if (!item) continue;
            GenTime start = item->startPos();
            GenTime end = item->endPos();
            if (!snaps.contains(start))
                snaps.append(start);
            if (!snaps.contains(end))
                snaps.append(end);
            if (!offsetList.isEmpty()) {
                for (int j = 0; j < offsetList.size(); ++j) {
                    GenTime offset = end - offsetList.at(j);
                    if (offset > GenTime()) {
                        if (!snaps.contains(offset)) {
                            snaps.append(offset);
                        }
                        offset = start - offsetList.at(j);
                        if (offset > GenTime() && !snaps.contains(offset)) {
                            snaps.append(offset);
                        }
                    }
                }
            }
            // Add clip markers
            QList<GenTime> markers;
            ClipController *controller = m_document->getClipController(item->getBinId());
            if (controller) {
                markers = item->snapMarkers(controller->snapMarkers());
            } else {
                qWarning("No controller!");
            }
            for (int j = 0; j < markers.size(); ++j) {
                GenTime t = markers.at(j);
                if (!snaps.contains(t))
                    snaps.append(t);
                if (!offsetList.isEmpty()) {
                    for (int k = 0; k < offsetList.size(); ++k) {
                        GenTime offset = t - offsetList.at(k);
                        if (offset > GenTime() && !snaps.contains(offset))
                            snaps.append(offset);
                    }
                }
            }
        } else if (itemList.at(i)->type() == TransitionWidget) {
            Transition *transition = static_cast <Transition*>(itemList.at(i));
            if (!transition) continue;
            GenTime start = transition->startPos();
            GenTime end = transition->endPos();
            if (!snaps.contains(start))
                snaps.append(start);
            if (!snaps.contains(end))
                snaps.append(end);
            if (!offsetList.isEmpty()) {
                for (int j = 0; j < offsetList.size(); ++j) {
                    GenTime offset = end - offsetList.at(j);
                    if (offset > GenTime()) {
                        if (!snaps.contains(offset)) {
                            snaps.append(offset);
                        }
                        offset = start - offsetList.at(j);
                        if (offset > GenTime() && !snaps.contains(offset)) {
                            snaps.append(offset);
                        }
                    }
                }
            }
        }
    }

    // add cursor position
    GenTime pos = GenTime(m_cursorPos, m_document->fps());
    if (!snaps.contains(pos))
        snaps.append(pos);
    if (!offsetList.isEmpty()) {
        for (int j = 0; j < offsetList.size(); ++j) {
            GenTime offset = pos - offsetList.at(j);
            if (!snaps.contains(offset))
                snaps.append(offset);
        }
    }

    // add guides
    for (int i = 0; i < m_guides.count(); ++i) {
        GenTime pos = m_guides.at(i)->position();
        if (!snaps.contains(pos))
            snaps.append(pos);
        if (!offsetList.isEmpty()) {
            for (int j = 0; j < offsetList.size(); ++j) {
                GenTime offset = pos - offsetList.at(j);
                if (!snaps.contains(offset))
                    snaps.append(offset);
            }
        }
    }

    // add render zone
    QPoint z = m_document->zone();
    pos = GenTime(z.x(), m_document->fps());
    if (!snaps.contains(pos))
        snaps.append(pos);
    pos = GenTime(z.y(), m_document->fps());
    if (!snaps.contains(pos))
        snaps.append(pos);

    qSort(snaps);
    m_scene->setSnapList(snaps);
    //for (int i = 0; i < m_snapPoints.size(); ++i)
    //    //qDebug() << "SNAP POINT: " << m_snapPoints.at(i).frames(25);
}

void CustomTrackView::slotSeekToPreviousSnap()
{
    updateSnapPoints(NULL);
    seekCursorPos((int) m_scene->previousSnapPoint(GenTime(m_cursorPos, m_document->fps())).frames(m_document->fps()));
    checkScrolling();
}

void CustomTrackView::slotSeekToNextSnap()
{
    updateSnapPoints(NULL);
    seekCursorPos((int) m_scene->nextSnapPoint(GenTime(m_cursorPos, m_document->fps())).frames(m_document->fps()));
    checkScrolling();
}

void CustomTrackView::clipStart()
{
    AbstractClipItem *item = getMainActiveClip();
    if (item == NULL) {
        item = m_dragItem;
    }
    if (item != NULL) {
        seekCursorPos((int) item->startPos().frames(m_document->fps()));
        checkScrolling();
    }
}

void CustomTrackView::clipEnd()
{
    AbstractClipItem *item = getMainActiveClip();
    if (item == NULL) {
        item = m_dragItem;
    }
    if (item != NULL) {
        seekCursorPos((int) item->endPos().frames(m_document->fps()) - 1);
        checkScrolling();
    }
}

int CustomTrackView::hasGuide(double pos, bool framePos)
{
    for (int i = 0; i < m_guides.count(); ++i) {
        double guidePos = m_guides.at(i)->position().frames(m_document->fps()) * (framePos ? 1 : matrix().m11());
        if (framePos) {
            if (guidePos == pos) return guidePos;
        } else {
            if (qAbs(guidePos - pos) <= QApplication::startDragDistance()) return guidePos;
        }
        if (guidePos > pos) return -1;
    }
    return -1;
}

void CustomTrackView::buildGuidesMenu(QMenu *goMenu) const
{
    goMenu->clear();
    double fps = m_document->fps();
    for (int i = 0; i < m_guides.count(); ++i) {
        QAction *act = goMenu->addAction(m_guides.at(i)->label() + '/' + Timecode::getStringTimecode(m_guides.at(i)->position().frames(fps), fps));
        act->setData(m_guides.at(i)->position().frames(m_document->fps()));
    }
    goMenu->setEnabled(!m_guides.isEmpty());
}

QMap <double, QString> CustomTrackView::guidesData() const
{
    QMap <double, QString> data;
    for (int i = 0; i < m_guides.count(); ++i) {
        data.insert(m_guides.at(i)->position().seconds(), m_guides.at(i)->label());
    }
    return data;
}

void CustomTrackView::editGuide(const GenTime &oldPos, const GenTime &pos, const QString &comment)
{
    if (comment.isEmpty() && pos < GenTime()) {
        // Delete guide
        bool found = false;
        for (int i = 0; i < m_guides.count(); ++i) {
            if (m_guides.at(i)->position() == oldPos) {
                delete m_guides.takeAt(i);
                found = true;
                break;
            }
        }
        if (!found) emit displayMessage(i18n("No guide at cursor time"), ErrorMessage);
    }
    else if (oldPos >= GenTime()) {
        // move guide
        for (int i = 0; i < m_guides.count(); ++i) {
            if (m_guides.at(i)->position() == oldPos) {
                Guide *item = m_guides.at(i);
                item->updateGuide(pos, comment);
                break;
            }
        }
    } else addGuide(pos, comment);
    qSort(m_guides.begin(), m_guides.end(), sortGuidesList);
    emit guidesUpdated();
}

bool CustomTrackView::addGuide(const GenTime &pos, const QString &comment, bool loadingProject)
{
    for (int i = 0; i < m_guides.count(); ++i) {
        if (m_guides.at(i)->position() == pos) {
            emit displayMessage(i18n("A guide already exists at position %1", m_document->timecode().getTimecodeFromFrames(pos.frames(m_document->fps()))), ErrorMessage);
            return false;
        }
    }
    Guide *g = new Guide(this, pos, comment, m_tracksHeight * m_timeline->visibleTracksCount() * matrix().m22());
    scene()->addItem(g);
    m_guides.append(g);
    qSort(m_guides.begin(), m_guides.end(), sortGuidesList);
    if (!loadingProject) {
        emit guidesUpdated();
    }
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

void CustomTrackView::slotEditGuide(int guidePos, const QString& newText)
{
    GenTime pos;
    if (guidePos == -1) pos = GenTime(m_cursorPos, m_document->fps());
    else pos = GenTime(guidePos, m_document->fps());
    bool found = false;
    for (int i = 0; i < m_guides.count(); ++i) {
        if (m_guides.at(i)->position() == pos) {
            CommentedTime guide = m_guides.at(i)->info();
            if (!newText.isEmpty()) {
                EditGuideCommand *command = new EditGuideCommand(this, guide.time(), guide.comment(), guide.time(), newText, true);
                m_commandStack->push(command);
            } else {
                slotEditGuide(guide);
            }
            found = true;
            break;
        }
    }
    if (!found) emit displayMessage(i18n("No guide at cursor time"), ErrorMessage);
}

void CustomTrackView::slotEditGuide(const CommentedTime &guide)
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
    for (int i = 0; i < m_guides.count(); ++i) {
        if (m_guides.at(i)->position() == pos) {
            EditGuideCommand *command = new EditGuideCommand(this, m_guides.at(i)->position(), m_guides.at(i)->label(), GenTime(-1), QString(), true);
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
    EditGuideCommand *command = new EditGuideCommand(this, m_dragGuide->position(), m_dragGuide->label(), GenTime(-1), QString(), true);
    m_commandStack->push(command);
}


void CustomTrackView::slotDeleteAllGuides()
{
    QUndoCommand *deleteAll = new QUndoCommand();
    deleteAll->setText(QStringLiteral("Delete all guides"));
    for (int i = 0; i < m_guides.count(); ++i) {
        new EditGuideCommand(this, m_guides.at(i)->position(), m_guides.at(i)->label(), GenTime(-1), QString(), true, deleteAll);
    }
    m_commandStack->push(deleteAll);
}

void CustomTrackView::setTool(ProjectTool tool)
{
    m_tool = tool;
    if (m_cutLine && tool != RazorTool) {
        delete m_cutLine;
        m_cutLine = NULL;
    }
    switch (m_tool) {
    case RazorTool:
        if (!m_cutLine) {
            m_cutLine = m_scene->addLine(0, 0, 0, m_tracksHeight * m_scene->scale().y());
            m_cutLine->setZValue(1000);
            QPen pen1 = QPen();
            pen1.setWidth(1);
            QColor line(Qt::red);
            pen1.setColor(line);
            m_cutLine->setPen(pen1);
            m_cutLine->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
            slotRefreshCutLine();
        }
        setCursor(m_razorCursor);
        break;
    case SpacerTool:
        setCursor(Qt::SplitHCursor);
        break;
    default:
        unsetCursor();
    }
}

void CustomTrackView::setScale(double scaleFactor, double verticalScale)
{

    QMatrix newmatrix;
    newmatrix = newmatrix.scale(scaleFactor, verticalScale);
    m_scene->isZooming = true;
    m_scene->setScale(scaleFactor, verticalScale);
    removeTipAnimation();
    bool adjust = verticalScale != matrix().m22();
    setMatrix(newmatrix);
    double newHeight = m_tracksHeight * m_timeline->visibleTracksCount() * matrix().m22();
    if (adjust) {
        setSceneRect(0, 0, sceneRect().width(), m_tracksHeight * m_timeline->visibleTracksCount());
        for (int i = 0; i < m_guides.count(); ++i) {
            m_guides.at(i)->setLine(0, 0, 0, newHeight - 1);
        }
    }
    if (scaleFactor < 1.5) {
        m_cursorLine->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
        QPen p = m_cursorLine->pen();
        QColor c = p.color();
        c.setAlpha(255);
        p.setColor(c);
        m_cursorLine->setPen(p);
        m_cursorOffset = 0;
        m_cursorLine->setLine(0, 0, 0, newHeight - 1);
    }
    else {
        m_cursorLine->setFlag(QGraphicsItem::ItemIgnoresTransformations, false);
        QPen p = m_cursorLine->pen();
        QColor c = p.color();
        c.setAlpha(100);
        p.setColor(c);
        m_cursorLine->setPen(p);
        m_cursorOffset = 0.5;
        m_cursorLine->setLine(0, 0, 0, m_tracksHeight * m_timeline->visibleTracksCount() - 1);
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
    slotRefreshCutLine();
    m_scene->isZooming = false;
}

void CustomTrackView::slotRefreshGuides()
{
    if (KdenliveSettings::showmarkers()) {
        for (int i = 0; i < m_guides.count(); ++i)
            m_guides.at(i)->update();
    }
}

void CustomTrackView::drawBackground(QPainter * painter, const QRectF &rect)
{
    //TODO: optimize, we currently redraw bg on every cursor move
    painter->setClipRect(rect);
    QPen pen1 = painter->pen();
    QColor lineColor = palette().text().color();
    lineColor.setAlpha(50);
    pen1.setColor(lineColor);
    painter->setPen(pen1);
    double min = rect.left();
    double max = rect.right();
    //painter->drawLine(QPointF(min, 0), QPointF(max, 0));
    int maxTrack = m_timeline->visibleTracksCount();
    QColor audioColor = palette().alternateBase().color();
    QColor activeLockColor = m_lockedTrackColor;
    activeLockColor.setAlpha(90);
    for (int i = 1; i <= maxTrack; ++i) {
        TrackInfo info = m_timeline->getTrackInfo(i);
        if (info.isLocked || info.type == AudioTrack || i == m_selectedTrack) {
            const QRectF track(min, m_tracksHeight * (maxTrack - i), max - min, m_tracksHeight - 1);
            if (i == m_selectedTrack)
                painter->fillRect(track, info.isLocked ? activeLockColor : m_selectedTrackColor);
            else
                painter->fillRect(track, info.isLocked ? m_lockedTrackColor : audioColor);
        }
        painter->drawLine(QPointF(min, m_tracksHeight * (maxTrack - i) - 1), QPointF(max, m_tracksHeight * (maxTrack - i) - 1));
    }
    painter->drawLine(QPointF(min, m_tracksHeight * (maxTrack) - 1), QPointF(max, m_tracksHeight * (maxTrack) - 1));
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
    int hor = m_document->timecode().getFrameCount(pos);
    activateMonitor();
    seekCursorPos(hor);
    slotSelectTrack(track.toInt());
    selectClip(true, false, track.toInt(), hor);
    ensureVisible(hor, verticalScrollBar()->value() + 10, 2, 2, 50, 0);
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
    for (int i = 0; i < itemList.count(); ++i) {
        // parse all clip names
        if (itemList.at(i)->type() == AVWidget) {
            ClipItem *item = static_cast <ClipItem *>(itemList.at(i));
            GenTime start = item->startPos();
            CommentedTime t(start, item->clipName());
            m_searchPoints.append(t);
            // add all clip markers
            ClipController *controller = m_document->getClipController(item->getBinId());
            QList < CommentedTime > markers = controller->commentedSnapMarkers();
            m_searchPoints += markers;
        }
    }

    // add guides
    for (int i = 0; i < m_guides.count(); ++i)
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
    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == AVWidget) {
            ClipItem *item = static_cast<ClipItem*>(itemList.at(i));
            if (item->getBinId() == clipId)
                matchingInfo << item->info();
        }
    }
    return matchingInfo;
}

void CustomTrackView::copyClip()
{
    qDeleteAll(m_copiedItems);
    m_copiedItems.clear();
    QAction *pasteAction = m_document->getAction(KStandardAction::name(KStandardAction::Paste));
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() == 0) {
        emit displayMessage(i18n("Select a clip before copying"), ErrorMessage);
        if (pasteAction)
            pasteAction->setEnabled(false);
        return;
    }
    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == AVWidget) {
            ClipItem *clip = static_cast <ClipItem *>(itemList.at(i));
            ClipItem *clone = clip->clone(clip->info());
            m_copiedItems.append(clone);
        } else if (itemList.at(i)->type() == TransitionWidget) {
            Transition *dup = static_cast <Transition *>(itemList.at(i));
            m_copiedItems.append(dup->clone());
        }
    }
    if (pasteAction)
        pasteAction->setEnabled(!m_copiedItems.isEmpty());
}

bool CustomTrackView::canBePastedTo(ItemInfo info, int type) const
{
    if (m_scene->editMode() != TimelineMode::NormalEdit) {
        // If we are in overwrite mode, always allow the move
        return true;
    }
    int height = m_tracksHeight - 2;
    int offset = 0;
    if (type == TransitionWidget) {
        height = Transition::itemHeight();
        offset = Transition::itemOffset();
    }
    else if (type == AVWidget) {
        height = ClipItem::itemHeight();
        offset = ClipItem::itemOffset();
    }
    QRectF rect((double) info.startPos.frames(m_document->fps()), (double)(getPositionFromTrack(info.track) + 1 + offset), (double)(info.endPos - info.startPos).frames(m_document->fps()), (double) height);
    QList<QGraphicsItem *> collisions = scene()->items(rect, Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisions.count(); ++i) {
        if (collisions.at(i)->type() == type) {
            return false;
        }
    }
    return true;
}

bool CustomTrackView::canBePastedTo(QList <ItemInfo> infoList, int type) const
{
    QPainterPath path;
    for (int i = 0; i < infoList.count(); ++i) {
        const QRectF rect((double) infoList.at(i).startPos.frames(m_document->fps()), (double)(getPositionFromTrack(infoList.at(i).track) + 1), (double)(infoList.at(i).endPos - infoList.at(i).startPos).frames(m_document->fps()), (double)(m_tracksHeight - 1));
        path.addRect(rect);
    }
    QList<QGraphicsItem *> collisions = scene()->items(path);
    for (int i = 0; i < collisions.count(); ++i) {
        if (collisions.at(i)->type() == type) return false;
    }
    return true;
}

bool CustomTrackView::canBePasted(QList<AbstractClipItem *> items, GenTime offset, int trackOffset) const
{
    for (int i = 0; i < items.count(); ++i) {
        ItemInfo info = items.at(i)->info();
        info.startPos += offset;
        info.endPos += offset;
        info.track += trackOffset;
        if (!canBePastedTo(info, items.at(i)->type())) return false;
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
        if (!contentsRect().contains(position)) {
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
        track = getTrackFromPos(mapToScene(position).y());

    GenTime leftPos = m_copiedItems.at(0)->startPos();
    int lowerTrack = m_copiedItems.at(0)->track();
    int upperTrack = m_copiedItems.at(0)->track();
    for (int i = 1; i < m_copiedItems.count(); ++i) {
        if (m_copiedItems.at(i)->startPos() < leftPos) leftPos = m_copiedItems.at(i)->startPos();
        if (m_copiedItems.at(i)->track() < lowerTrack) lowerTrack = m_copiedItems.at(i)->track();
        if (m_copiedItems.at(i)->track() > upperTrack) upperTrack = m_copiedItems.at(i)->track();
    }

    GenTime offset = pos - leftPos;
    int trackOffset = track - lowerTrack;
    if (upperTrack + trackOffset > m_timeline->tracksCount() - 1) trackOffset = m_timeline->tracksCount() - 1 -upperTrack;
    if (lowerTrack + trackOffset < 0 || !canBePasted(m_copiedItems, offset, trackOffset)) {
        emit displayMessage(i18n("Cannot paste selected clips"), ErrorMessage);
        return;
    }
    QUndoCommand *pasteClips = new QUndoCommand();
    pasteClips->setText(QStringLiteral("Paste clips"));
    RefreshMonitorCommand *firstRefresh = new RefreshMonitorCommand(this, ItemInfo(), false, true, pasteClips);
    QList <ItemInfo> range;
    for (int i = 0; i < m_copiedItems.count(); ++i) {
        // parse all clip names
        if (m_copiedItems.at(i) && m_copiedItems.at(i)->type() == AVWidget) {
            ClipItem *clip = static_cast <ClipItem *>(m_copiedItems.at(i));
            ItemInfo info = clip->info();
            info.startPos += offset;
            info.endPos += offset;
            info.track += trackOffset;
            if (canBePastedTo(info, AVWidget)) {
                new AddTimelineClipCommand(this, clip->getBinId(), info, clip->effectList(), clip->clipState(), true, false, false, pasteClips);
                if (clip->hasVisibleVideo()) {
                    range << info;
                }
            } else emit displayMessage(i18n("Cannot paste clip to selected place"), ErrorMessage);
        } else if (m_copiedItems.at(i) && m_copiedItems.at(i)->type() == TransitionWidget) {
            Transition *tr = static_cast <Transition *>(m_copiedItems.at(i));
            ItemInfo info;
            info.startPos = tr->startPos() + offset;
            info.endPos = tr->endPos() + offset;
            info.track = tr->track() + trackOffset;
            int transitionEndTrack;
            if (!tr->forcedTrack()) transitionEndTrack = getPreviousVideoTrack(info.track);
            else transitionEndTrack = tr->transitionEndTrack();
            if (canBePastedTo(info, TransitionWidget)) {
                if (info.startPos >= info.endPos) {
                    emit displayMessage(i18n("Invalid transition"), ErrorMessage);
                } else {
                    new AddTransitionCommand(this, info, transitionEndTrack, tr->toXML(), false, true, pasteClips);
                    range << info;
                }
            } else emit displayMessage(i18n("Cannot paste transition to selected place"), ErrorMessage);
        }
    }
    updateTrackDuration(-1, pasteClips);
    firstRefresh->updateRange(range);
    new RefreshMonitorCommand(this, range, true, false, pasteClips);
    m_commandStack->push(pasteClips);
}

void CustomTrackView::pasteClipEffects()
{
    if (m_copiedItems.count() != 1 || m_copiedItems.at(0)->type() != AVWidget) {
        emit displayMessage(i18n("You must copy exactly one clip before pasting effects"), ErrorMessage);
        return;
    }
    ClipItem *clip = static_cast < ClipItem *>(m_copiedItems.at(0));

    QUndoCommand *paste = new QUndoCommand();
    paste->setText(QStringLiteral("Paste effects"));

    QList<QGraphicsItem *> clips = scene()->selectedItems();

    // expand groups
    for (int i = 0; i < clips.count(); ++i) {
        if (clips.at(i)->type() == GroupWidget) {
            QList<QGraphicsItem *> children = clips.at(i)->childItems();
            for (int j = 0; j < children.count(); ++j) {
                if (children.at(j)->type() == AVWidget && !clips.contains(children.at(j))) {
                    clips.append(children.at(j));
                }
            }
        }
    }

    for (int i = 0; i < clips.count(); ++i) {
        if (clips.at(i)->type() == AVWidget) {
            ClipItem *item = static_cast < ClipItem *>(clips.at(i));
            for (int j = 0; j < clip->effectsCount(); ++j) {
                QDomElement eff = clip->effect(j);
                if (eff.attribute(QStringLiteral("unique"), QStringLiteral("0")) == QLatin1String("0") || item->hasEffect(eff.attribute(QStringLiteral("tag")), eff.attribute(QStringLiteral("id"))) == -1) {
                    adjustKeyfames(clip->cropStart(), item->cropStart(), item->cropDuration(), eff);
                    new AddEffectCommand(this, item->track(), item->startPos(), eff, true, paste);
                }
            }
        }
    }
    if (paste->childCount() > 0) m_commandStack->push(paste);
    else delete paste;

    //adjust effects (fades, ...)
    for (int i = 0; i < clips.count(); ++i) {
        if (clips.at(i)->type() == AVWidget) {
            ClipItem *item = static_cast < ClipItem *>(clips.at(i));
            updatePositionEffects(item, item->info());
        }
    }
}


void CustomTrackView::adjustKeyfames(GenTime oldstart, GenTime newstart, GenTime duration, QDomElement xml)
{
    // parse parameters to check if we need to adjust to the new crop start
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    QDomNodeList params = xml.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.isNull()) continue;
        if (e.attribute(QStringLiteral("type")) == QLatin1String("keyframe")
            || e.attribute(QStringLiteral("type")) == QLatin1String("simplekeyframe")) {
            // Effect has a keyframe type parameter, we need to adjust the values
            QString adjusted = EffectsController::adjustKeyframes(e.attribute(QStringLiteral("keyframes")), oldstart.frames(m_document->fps()), newstart.frames(m_document->fps()), (newstart + duration).frames(m_document->fps()) - 1, m_document->getProfileInfo());
            e.setAttribute(QStringLiteral("keyframes"), adjusted);
        } else if (e.attribute(QStringLiteral("type")) == QLatin1String("animated")) {
            if (xml.attribute(QStringLiteral("kdenlive:sync_in_out")) != QLatin1String("1")) {
                QString resizedAnim = KeyframeView::cutAnimation(e.attribute(QStringLiteral("value")), 0,  duration.frames(m_document->fps()), duration.frames(m_document->fps()));
                e.setAttribute(QStringLiteral("value"), resizedAnim);
            } else if (xml.hasAttribute(QStringLiteral("kdenlive:sync_in_out"))) {
                // Effect attached to clip in, update
                xml.setAttribute("in", QString::number(newstart.frames(m_document->fps())));
                xml.setAttribute("out", QString::number((newstart + duration).frames(m_document->fps())));
            }
        }
    }
}

ClipItem *CustomTrackView::getClipUnderCursor() const
{
    QRectF rect((double) seekPosition(), 0.0, 1.0, (double)(m_tracksHeight * m_timeline->visibleTracksCount()));
    QList<QGraphicsItem *> collisions = scene()->items(rect, Qt::IntersectsItemBoundingRect);
    if (m_dragItem && !collisions.isEmpty() && collisions.contains(m_dragItem) && m_dragItem->type() == AVWidget && !m_dragItem->isItemLocked()) {
        return static_cast < ClipItem *>(m_dragItem);
    }
    for (int i = 0; i < collisions.count(); ++i) {
        if (collisions.at(i)->type() == AVWidget) {
            ClipItem *clip = static_cast < ClipItem *>(collisions.at(i));
            if (!clip->isItemLocked()) return clip;
        }
    }
    return NULL;
}

const QString CustomTrackView::getClipUnderCursor(int *pos, QPoint *zone) const
{
    ClipItem *item = static_cast < ClipItem *>(getMainActiveClip());
    if (item == NULL) {
        if (m_dragItem && m_dragItem->type() == AVWidget) {
            item = static_cast < ClipItem *>(m_dragItem);
        }
    } else {
        *pos = seekPosition() - (item->startPos() - item->cropStart()).frames(m_document->fps());
        if (zone) {
            *zone = QPoint(item->cropStart().frames(m_document->fps()), (item->cropStart() + item->cropDuration()).frames(m_document->fps()));
        }
    }
    QString result;
    if (item) {
        result = item->getBinId();
    }
    return result;
}

AbstractClipItem *CustomTrackView::getMainActiveClip() const
{
    QList<QGraphicsItem *> clips = scene()->selectedItems();
    if (clips.isEmpty()) {
        return getClipUnderCursor();
    } else {
        AbstractClipItem *item = NULL;
        for (int i = 0; i < clips.count(); ++i) {
            if (clips.at(i)->type() == AVWidget) {
                item = static_cast < AbstractClipItem *>(clips.at(i));
                if (clips.count() > 1 && item->startPos().frames(m_document->fps()) <= seekPosition() && item->endPos().frames(m_document->fps()) >= seekPosition()) break;
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
            if (clips.at(i)->type() != AVWidget) clips.removeAt(i);
            else ++i;
        }
        if (clips.count() == 1 && allowOutsideCursor) return static_cast < ClipItem *>(clips.at(0));
        for (int i = 0; i < clips.count(); ++i) {
            if (clips.at(i)->type() == AVWidget) {
                item = static_cast < ClipItem *>(clips.at(i));
                if (item->startPos().frames(m_document->fps()) <= seekPosition() && item->endPos().frames(m_document->fps()) >= seekPosition())
                    return item;
            }
        }
    }
    return NULL;
}

void CustomTrackView::expandActiveClip()
{
    AbstractClipItem *item = getActiveClipUnderCursor(true);
    if (item == NULL || item->type() != AVWidget) {
        emit displayMessage(i18n("You must select one clip for this action"), ErrorMessage);
        return;
    }
    ClipItem *clip = static_cast < ClipItem *>(item);
    QUrl url = clip->binClip()->url();
    if (clip->clipType() != Playlist || !url.isValid()) {
        emit displayMessage(i18n("You must select a playlist clip for this action"), ErrorMessage);
        return;
    }
    // Step 1: remove playlist clip
    QUndoCommand *expandCommand = new QUndoCommand();
    expandCommand->setText(i18n("Expand Clip"));
    ItemInfo info = clip->info();
    new AddTimelineClipCommand(this, clip->getBinId(), info, clip->effectList(), clip->clipState(), true, true, true, expandCommand);
    emit importPlaylistClips(info, url, expandCommand);
}

void CustomTrackView::setInPoint()
{
    AbstractClipItem *clip = getActiveClipUnderCursor(true);
    if (clip == NULL) {
        if (m_dragItem && m_dragItem->type() == TransitionWidget) {
            clip = m_dragItem;
        } else {
            emit displayMessage(i18n("You must select one clip for this action"), ErrorMessage);
            return;
        }
    }

    AbstractGroupItem *parent = static_cast <AbstractGroupItem *>(clip->parentItem());
    if (parent) {
        // Resizing a group
        QUndoCommand *resizeCommand = new QUndoCommand();
        resizeCommand->setText(i18n("Resize group"));
        QList <QGraphicsItem *> items = parent->childItems();
        GenTime min = parent->startPos();
        GenTime max = min;
        for (int i = 0; i < items.count(); ++i) {
            AbstractClipItem *item = static_cast<AbstractClipItem *>(items.at(i));
            if (item && item->type() == AVWidget) {
                ItemInfo info = item->info();
                prepareResizeClipStart(item, info, seekPosition(), true, resizeCommand);
                ClipItem *cp = qobject_cast<ClipItem *> (clip);
                if (cp && cp->hasVisibleVideo()) {
                    min = qMin(min, item->startPos());
                    max = qMax(max, item->startPos());
                }
            }
        }
        if (resizeCommand->childCount() > 0) {
            m_commandStack->push(resizeCommand);
            if (min < max) {
                ItemInfo nfo;
                nfo.startPos = min;
                nfo.endPos = max;
                monitorRefresh(nfo, true);
            }
        } else {
            //TODO warn user of failed resize
            delete resizeCommand;
        }
    }
    else {
        ItemInfo info = clip->info();
        prepareResizeClipStart(clip, info, seekPosition(), true);
        ClipItem *cp = qobject_cast<ClipItem *> (clip);
        if (cp && cp->hasVisibleVideo()) {
            ItemInfo updated = cp->info();
            if (updated.startPos > info.startPos) {
                updated.endPos = updated.startPos;
                updated.startPos = info.startPos;
            } else {
                updated.endPos = info.startPos;
            }
            monitorRefresh(updated, true);
        }
    }
}

void CustomTrackView::setOutPoint()
{
    AbstractClipItem *clip = getActiveClipUnderCursor(true);
    if (clip == NULL) {
        if (m_dragItem && m_dragItem->type() == TransitionWidget) {
            clip = m_dragItem;
        } else {
            emit displayMessage(i18n("You must select one clip for this action"), ErrorMessage);
            return;
        }
    }
    AbstractGroupItem *parent = static_cast <AbstractGroupItem *>(clip->parentItem());
    if (parent) {
        // Resizing a group
        QUndoCommand *resizeCommand = new QUndoCommand();
        resizeCommand->setText(i18n("Resize group"));
        QList <QGraphicsItem *> items = parent->childItems();
        GenTime min = parent->startPos() + parent->duration();
        GenTime max = min;
        for (int i = 0; i < items.count(); ++i) {
            AbstractClipItem *item = static_cast<AbstractClipItem *>(items.at(i));
            if (item && item->type() == AVWidget) {
                ItemInfo info = item->info();
                prepareResizeClipEnd(item, info, seekPosition(), true, resizeCommand);
                ClipItem *cp = qobject_cast<ClipItem *> (item);
                if (cp && cp->hasVisibleVideo()) {
                    min = qMin(min, item->endPos());
                    max = qMax(max, item->endPos());
                }
            }
        }
        if (resizeCommand->childCount() > 0) {
            m_commandStack->push(resizeCommand);
            if (min < max) {
                ItemInfo nfo;
                nfo.startPos = min;
                nfo.endPos = max;
                monitorRefresh(nfo, true);
            }
        } else {
            //TODO warn user of failed resize
            delete resizeCommand;
        }
    }
    else {
        ItemInfo info = clip->info();
        prepareResizeClipEnd(clip, info, seekPosition(), true);
        ClipItem *cp = qobject_cast<ClipItem *> (clip);
        if (cp && cp->hasVisibleVideo()) {
            ItemInfo updated = cp->info();
            if (updated.endPos > info.endPos) {
                updated.startPos = info.endPos;
            } else {
                updated.startPos = updated.endPos;
                updated.endPos = info.endPos;
            }
            monitorRefresh(updated, true);
        }
    }
}

void CustomTrackView::slotUpdateAllThumbs()
{
    if (!isEnabled()) return;
    QList<QGraphicsItem *> itemList = scene()->items();
    //if (itemList.isEmpty()) return;
    ClipItem *item;
    bool ok = false;
    QDir thumbsFolder = m_document->getCacheDir(CacheThumbs, &ok);
    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == AVWidget) {
            item = static_cast <ClipItem *>(itemList.at(i));
            if (item && item->isEnabled() && item->clipType() != Color && item->clipType() != Audio) {
                // Check if we have a cached thumbnail
                if (item->clipType() == Image || item->clipType() == Text) {
                    QString thumb = thumbsFolder.absoluteFilePath(item->getBinHash() + "#0.png");
                    if (ok && QFile::exists(thumb)) {
                        QPixmap pix(thumb);
                        if (pix.isNull()) QFile::remove(thumb);
                        item->slotSetStartThumb(pix);
                    }
                } else {
                    QString startThumb = thumbsFolder.absoluteFilePath(item->getBinHash() + "#");
                    QString endThumb = startThumb;
                    startThumb.append(QString::number((int) item->speedIndependantCropStart().frames(m_document->fps())) + ".png");
                    endThumb.append(QString::number((int) (item->speedIndependantCropStart() + item->speedIndependantCropDuration()).frames(m_document->fps()) - 1) + ".png");
                    if (QFile::exists(startThumb)) {
                        QPixmap pix(startThumb);
                        if (pix.isNull()) QFile::remove(startThumb);
                        item->slotSetStartThumb(pix);
                    }
                    if (QFile::exists(endThumb)) {
                        QPixmap pix(endThumb);
                        if (pix.isNull()) QFile::remove(endThumb);
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
    QList<QGraphicsItem *> itemList = scene()->items();
    ClipItem *item;
    bool ok = false;
    QDir thumbsFolder = m_document->getCacheDir(CacheThumbs, &ok);
    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == AVWidget) {
            item = static_cast <ClipItem *>(itemList.at(i));
            if (item->clipType() != Color && item->clipType() != Audio) {
                // Check if we have a cached thumbnail
                if (item->clipType() == Image || item->clipType() == Text || item->clipType() == Audio) {
                    QString thumb = thumbsFolder.absoluteFilePath(item->getBinHash() + "#0.png");
                    if (!QFile::exists(thumb)) {
                        item->startThumb().save(thumb);
                    }
                } else {
                    QString startThumb = thumbsFolder.absoluteFilePath(item->getBinHash() + '#');
                    QString endThumb = startThumb;
                    startThumb.append(QString::number((int) item->speedIndependantCropStart().frames(m_document->fps())) + ".png");
                    endThumb.append(QString::number((int) (item->speedIndependantCropStart() + item->speedIndependantCropDuration()).frames(m_document->fps()) - 1) + ".png");
                    if (!QFile::exists(startThumb)) {
                        item->startThumb().save(startThumb);
                    }
                    if (!QFile::exists(endThumb)) {
                        item->endThumb().save(endThumb);
                    }
                }
            }
        }
    }
}


void CustomTrackView::slotInsertTrack(int ix)
{
    QPointer<TrackDialog> d = new TrackDialog(m_timeline, parentWidget());
    d->comboTracks->setCurrentIndex(m_timeline->visibleTracksCount() - ix);
    d->label->setText(i18n("Insert track"));
    QStringList existingTrackNames = m_timeline->getTrackNames();
    int i = 1;
    QString proposedName = i18n("Video %1", i);
    while (existingTrackNames.contains(proposedName)) {
        proposedName = i18n("Video %1", ++i);
    }
    d->track_name->setText(proposedName);
    d->setWindowTitle(i18n("Insert New Track"));

    if (d->exec() == QDialog::Accepted) {
        ix = m_timeline->visibleTracksCount() - d->comboTracks->currentIndex();
        if (d->before_select->currentIndex() == 0)
            ix++;
        TrackInfo info;
        info.duration = 0;
        info.isMute = false;
        info.isLocked = false;
        info.effectsList = EffectsList(true);
        if (d->video_track->isChecked()) {
            info.type = VideoTrack;
            info.isBlind = false;
        } else {
            info.type = AudioTrack;
            info.isBlind = true;
        }
        info.trackName = d->track_name->text();
        AddTrackCommand *addTrack = new AddTrackCommand(this, ix, info, true);
        m_commandStack->push(addTrack);
    }
    delete d;
}

void CustomTrackView::slotDeleteTrack(int ix)
{
    if (m_timeline->tracksCount() <= 2) {
        // TODO: warn user that at least one track is necessary
        return;
    }
    QPointer<TrackDialog> d = new TrackDialog(m_timeline, parentWidget());
    d->comboTracks->setCurrentIndex(m_timeline->visibleTracksCount() - ix);
    d->label->setText(i18n("Delete track"));
    d->track_name->setHidden(true);
    d->name_label->setHidden(true);
    d->before_select->setHidden(true);
    d->setWindowTitle(i18n("Delete Track"));
    d->video_track->setHidden(true);
    d->audio_track->setHidden(true);
    if (d->exec() == QDialog::Accepted) {
        ix = m_timeline->visibleTracksCount() - d->comboTracks->currentIndex();
        TrackInfo info = m_timeline->getTrackInfo(ix);
        deleteTimelineTrack(ix, info);
    }
    delete d;
}

void CustomTrackView::slotConfigTracks(int ix)
{
    QPointer<TracksConfigDialog> d = new TracksConfigDialog(m_timeline,
                                                            ix, parentWidget());
    if (d->exec() == QDialog::Accepted) {
        configTracks(d->tracksList());
        QList <int> toDelete = d->deletedTracks();
        while (!toDelete.isEmpty()) {
            int track = toDelete.takeLast();
            TrackInfo info = m_timeline->getTrackInfo(track);
            deleteTimelineTrack(track, info);
        }
    }
    delete d;
}

void CustomTrackView::deleteTimelineTrack(int ix, TrackInfo trackinfo)
{
    if (m_timeline->tracksCount() < 2) return;
    // Clear effect stack
    clearSelection();
    emit transitionItemSelected(NULL);
    // Make sure the selected track index is not outside range
    m_selectedTrack = qBound(1, m_selectedTrack, m_timeline->tracksCount() - 2);

    double startY = getPositionFromTrack(ix) + 1 + m_tracksHeight / 2;
    QRectF r(0, startY, sceneRect().width(), m_tracksHeight / 2 - 1);
    QList<QGraphicsItem *> selection = m_scene->items(r);
    QUndoCommand *deleteTrack = new QUndoCommand();
    deleteTrack->setText(QStringLiteral("Delete track"));

    // Remove clips on that track from groups
    QList<QGraphicsItem *> groupsToProceed;
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == GroupWidget) {
            if (!groupsToProceed.contains(selection.at(i))) {
                groupsToProceed << selection.at(i);
            }
        }
    }
    for (int i = 0; i < groupsToProceed.count(); i++) {
        AbstractGroupItem *grp = static_cast<AbstractGroupItem *>(groupsToProceed.at(i));
        QList <QGraphicsItem *> items = grp->childItems();
        QList <ItemInfo> clipGrpInfos;
        QList <ItemInfo> transitionGrpInfos;
        QList <ItemInfo> newClipGrpInfos;
        QList <ItemInfo> newTransitionGrpInfos;
        for (int j = 0; j < items.count(); ++j) {
            if (items.at(j)->type() == AVWidget) {
                AbstractClipItem *clip = static_cast <AbstractClipItem *>(items.at(j));
                clipGrpInfos.append(clip->info());
                if (clip->track() != ix) {
                    newClipGrpInfos.append(clip->info());
                }
            } else if (items.at(j)->type() == TransitionWidget) {
                AbstractClipItem *clip = static_cast <AbstractClipItem *>(items.at(j));
                transitionGrpInfos.append(clip->info());
                if (clip->track() != ix) {
                    newTransitionGrpInfos.append(clip->info());
                }
            }
        }
        if (!clipGrpInfos.isEmpty() || !transitionGrpInfos.isEmpty()) {
            // Break group
            new GroupClipsCommand(this, clipGrpInfos, transitionGrpInfos, false, deleteTrack);
            // Rebuild group without clips from track to delete
            if (!newClipGrpInfos.isEmpty() || !newTransitionGrpInfos.isEmpty()) {
                new GroupClipsCommand(this, newClipGrpInfos, newTransitionGrpInfos, true, true, deleteTrack);
            }
        }
    }
    RefreshMonitorCommand *firstRefresh = new RefreshMonitorCommand(this, ItemInfo(), false, true, deleteTrack);
    // Delete all clips in selected track
    QList <ItemInfo> ranges;
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == AVWidget) {
            ClipItem *item =  static_cast <ClipItem *>(selection.at(i));
            ranges << item->info();
            new AddTimelineClipCommand(this, item->getBinId(), item->info(), item->effectList(), item->clipState(), false, true, false, deleteTrack);
            m_scene->removeItem(item);
            delete item;
            item = NULL;
        } else if (selection.at(i)->type() == TransitionWidget) {
            Transition *item =  static_cast <Transition *>(selection.at(i));
            ranges << item->info();
            new AddTransitionCommand(this, item->info(), item->transitionEndTrack(), item->toXML(), true, false, deleteTrack);
            m_scene->removeItem(item);
            delete item;
            item = NULL;
        }
    }
    firstRefresh->updateRange(ranges);
    new AddTrackCommand(this, ix, trackinfo, false, deleteTrack);
    new RefreshMonitorCommand(this, ranges, true, false, deleteTrack);
    m_commandStack->push(deleteTrack);
}

void CustomTrackView::autoTransition()
{
    QList<QGraphicsItem *> itemList = scene()->selectedItems();
    if (itemList.count() != 1 || itemList.at(0)->type() != TransitionWidget) {
        emit displayMessage(i18n("You must select one transition for this action"), ErrorMessage);
        return;
    }
    Transition *tr = static_cast <Transition*>(itemList.at(0));
    tr->setAutomatic(!tr->isAutomatic());
    QDomElement transition = tr->toXML();
    m_timeline->transitionHandler->updateTransition(transition.attribute(QStringLiteral("tag")), transition.attribute(QStringLiteral("tag")), transition.attribute(QStringLiteral("transition_btrack")).toInt(), transition.attribute(QStringLiteral("transition_atrack")).toInt(), tr->startPos(), tr->endPos(), transition);
    setDocumentModified();
}

void CustomTrackView::clipNameChanged(const QString &id)
{
    QList<QGraphicsItem *> list = scene()->items();
    ClipItem *clip = NULL;
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i)->type() == AVWidget) {
            clip = static_cast <ClipItem *>(list.at(i));
            if (clip->getBinId() == id) {
                clip->update();
            }
        }
    }
    //viewport()->update();
}

void CustomTrackView::getClipAvailableSpace(AbstractClipItem *item, GenTime &minimum, GenTime &maximum)
{
    minimum = GenTime();
    maximum = GenTime();
    QList<QGraphicsItem *> selection;
    selection = m_scene->items(QRectF(0, getPositionFromTrack(item->track()) + m_tracksHeight / 2, sceneRect().width(), 2));
    selection.removeAll(item);
    for (int i = 0; i < selection.count(); ++i) {
        AbstractClipItem *clip = static_cast <AbstractClipItem *>(selection.at(i));
        if (clip && clip->type() == AVWidget) {
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
    selection = m_scene->items(QRectF(0, getPositionFromTrack(item->track()) + m_tracksHeight, sceneRect().width(), 2));
    selection.removeAll(item);
    for (int i = 0; i < selection.count(); ++i) {
        AbstractClipItem *clip = static_cast <AbstractClipItem *>(selection.at(i));
        if (clip && clip->type() == TransitionWidget) {
            if (clip->endPos() <= item->startPos() && clip->endPos() > minimum) minimum = clip->endPos();
            if (clip->startPos() > item->startPos() && (clip->startPos() < maximum || maximum == GenTime())) maximum = clip->startPos();
        }
    }
}

void CustomTrackView::loadGroups(const QDomNodeList &groups)
{
    m_document->clipManager()->resetGroups();
    for (int i = 0; i < groups.count(); ++i) {
        QDomNodeList children = groups.at(i).childNodes();
        scene()->clearSelection();
        QList <QGraphicsItem*>list;
        for (int nodeindex = 0; nodeindex < children.count(); ++nodeindex) {
            QDomElement elem = children.item(nodeindex).toElement();
            int track = elem.attribute(QStringLiteral("track")).toInt();
            // Ignore items removed after track deletion
            if (track == -1) continue;
            int pos = elem.attribute(QStringLiteral("position")).toInt();
            if (elem.tagName() == QLatin1String("clipitem")) {
                ClipItem *clip = getClipItemAtStart(GenTime(pos, m_document->fps()), track);
                if (clip) list.append(clip);//clip->setSelected(true);
            } else {
                Transition *clip = getTransitionItemAt(pos, track);
                if (clip) list.append(clip);//clip->setSelected(true);
            }
        }
        groupSelectedItems(list, true);
    }
}

void CustomTrackView::splitAudio(bool warn, ItemInfo info, int destTrack, QUndoCommand *masterCommand)
{
    resetSelectionGroup();
    QList<QGraphicsItem *> selection;
    bool hasMasterCommand = masterCommand != NULL;
    if (!hasMasterCommand) {
        masterCommand = new QUndoCommand();
        masterCommand->setText(i18n("Split audio"));
    }
    if (!info.isValid()) {
        // Operate on current selection
        selection = scene()->selectedItems();
        if (selection.isEmpty()) {
            emit displayMessage(i18n("You must select at least one clip for this action"), ErrorMessage);
            if (!hasMasterCommand)
                delete masterCommand;
            return;
        }
    } else {
        new SplitAudioCommand(this, info.track, destTrack, info.startPos, masterCommand);
    }
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == AVWidget) {
            ClipItem *clip = static_cast <ClipItem *>(selection.at(i));
            if (clip->clipType() == AV || clip->clipType() == Playlist) {
                if (clip->parentItem()) {
                    emit displayMessage(i18n("Cannot split audio of grouped clips"), ErrorMessage);
                } else {
                    new SplitAudioCommand(this, clip->track(), destTrack, clip->startPos(), masterCommand);
                }
            }
        }
    }
    if (masterCommand->childCount()) {
        updateTrackDuration(-1, masterCommand);
        if (!hasMasterCommand) {
            m_commandStack->push(masterCommand);
        }
    } else {
        if (warn) emit displayMessage(i18n("No clip to split"), ErrorMessage);
        if (!hasMasterCommand)
            delete masterCommand;
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
        m_audioCorrelator = NULL;
        m_audioAlignmentReference = NULL;
    }
    if (selection.at(0)->type() == AVWidget) {
        ClipItem *clip = static_cast<ClipItem*>(selection.at(0));
        if (clip->clipType() == AV || clip->clipType() == Audio) {
            m_audioAlignmentReference = clip;
            Mlt::Producer *prod = m_timeline->track(clip->track())->clipProducer(m_document->renderer()->getBinProducer(clip->getBinId()), clip->clipState());
            if (!prod) {
                qWarning() << "couldn't load producer for clip " << clip->getBinId() << " on track " << clip->track();
                return;
            }
            AudioEnvelope *envelope = new AudioEnvelope(clip->binClip()->url().path(), prod);
            m_audioCorrelator = new AudioCorrelation(envelope);
            connect(m_audioCorrelator, SIGNAL(gotAudioAlignData(int,int,int)), this, SLOT(slotAlignClip(int,int,int)));
            connect(m_audioCorrelator, SIGNAL(displayMessage(QString,MessageType)), this, SIGNAL(displayMessage(QString,MessageType)));
            emit displayMessage(i18n("Processing audio, please wait."), ProcessingJobMessage);
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

    QList<QGraphicsItem *> selection = scene()->selectedItems();
    foreach (QGraphicsItem *item, selection) {
        if (item->type() == AVWidget) {

            ClipItem *clip = static_cast<ClipItem*>(item);
            if (clip == m_audioAlignmentReference) {
                continue;
            }

            if (clip->clipType() == AV || clip->clipType() == Audio) {
                ItemInfo info = clip->info();
                Mlt::Producer *prod = m_timeline->track(clip->track())->clipProducer(m_document->renderer()->getBinProducer(clip->getBinId()), clip->clipState());
                if (!prod) {
                    qWarning() << "couldn't load producer for clip " << clip->getBinId() << " on track " << clip->track();
                    return;
                }
                AudioEnvelope *envelope = new AudioEnvelope(clip->binClip()->url().path(), prod,
                        info.cropStart.frames(m_document->fps()),
                        info.cropDuration.frames(m_document->fps()),
                        clip->track(),
                        info.startPos.frames(m_document->fps()));
                m_audioCorrelator->addChild(envelope);
            }
        }
    }
    emit displayMessage(i18n("Processing audio, please wait."), ProcessingJobMessage);
}

void CustomTrackView::slotAlignClip(int track, int pos, int shift)
{
    QUndoCommand *moveCommand = new QUndoCommand();
    ClipItem *clip = getClipItemAtStart(GenTime(pos, m_document->fps()), track);
    if (!clip) {
        emit displayMessage(i18n("Cannot find clip to align."), ErrorMessage);
        delete moveCommand;
        return;
    }
    GenTime add(shift, m_document->fps());
    ItemInfo start = clip->info();

    ItemInfo end = start;
    end.startPos = m_audioAlignmentReference->startPos() + add - m_audioAlignmentReference->cropStart();
    end.endPos = end.startPos + start.cropDuration;
    if ( end.startPos < GenTime() ) {
        // Clip would start before 0, so crop it first
        GenTime cropBy = -end.startPos;
        if (cropBy > start.cropDuration) {
            delete moveCommand;
            emit displayMessage(i18n("Unable to move clip out of timeline."), ErrorMessage);
            return;
        }
        ItemInfo resized = start;
        resized.startPos += cropBy;

        resizeClip(start, resized);
        new ResizeClipCommand(this, start, resized, false, false, moveCommand);

        start = clip->info();
        end.startPos += cropBy;
    }

    if (itemCollision(clip, end)) {
        delete moveCommand;
        emit displayMessage(i18n("Unable to move clip due to collision."), ErrorMessage);
        return;
    }
    emit displayMessage(i18n("Clip aligned."), OperationCompletedMessage);
    moveCommand->setText(i18n("Auto-align clip"));
    new MoveClipCommand(this, start, end, false, true, moveCommand);
    updateTrackDuration(clip->track(), moveCommand);
    m_commandStack->push(moveCommand);

}

bool CustomTrackView::doSplitAudio(const GenTime &pos, int track, int destTrack, bool split)
{
    ClipItem *clip = getClipItemAtStart(pos, track);
    if (clip == NULL) {
        qDebug() << "// Cannot find clip to split!!!";
        return false;
    }
    EffectsList effects;
    effects.clone(clip->effectList());
    if (split) {
        int start = pos.frames(m_document->fps());
        // do not split audio when we are on an audio track
        if (m_timeline->getTrackInfo(track).type == AudioTrack)
            return false;
        if (destTrack == -1) {
            destTrack = track - 1;
            for (; destTrack > 0; destTrack--) {
                TrackInfo info = m_timeline->getTrackInfo(destTrack);
                if (info.type == AudioTrack && !info.isLocked) {
		    if (sceneEditMode() != TimelineMode::NormalEdit)
			break;
                    int blength = m_timeline->getTrackSpaceLength(destTrack, start, false);
                    if (blength == -1 || blength >= clip->cropDuration().frames(m_document->fps())) {
                        break;
                    }
                }
            }
        } else {
	    // Expanding to target track, check it is not locked
	    TrackInfo info = m_timeline->getTrackInfo(destTrack);
	    if (info.type != AudioTrack || info.isLocked) {
		destTrack = 0;
	    } else if (sceneEditMode() == TimelineMode::NormalEdit) {
		int blength = m_timeline->getTrackSpaceLength(destTrack, start, false);
		if (blength >= 0 && blength < clip->cropDuration().frames(m_document->fps())) {
		    destTrack = 0;
		}
	    }
	}
        if (destTrack == 0) {
            emit displayMessage(i18n("No empty space to put clip audio"), ErrorMessage);
	    return false;
        } else {
            ItemInfo info = clip->info();
            info.track = destTrack;
            scene()->clearSelection();
            addClip(clip->getBinId(), info, clip->effectList(), PlaylistState::AudioOnly, false);
            clip->setSelected(true);
            ClipItem *audioClip = getClipItemAtStart(pos, info.track);
            if (audioClip) {
                if (m_timeline->track(track)->replace(pos.seconds(), m_document->renderer()->getBinVideoProducer(clip->getBinId()))) {
                    clip->setState(PlaylistState::VideoOnly);
                } else {
                    emit displayMessage(i18n("Cannot update clip (time: %1, track: %2)", pos.frames(m_document->fps()), destTrack), ErrorMessage);
                }
                audioClip->setSelected(true);

                // keep video effects, move audio effects to audio clip
                int videoIx = 0;
                int audioIx = 0;
                for (int i = 0; i < effects.count(); ++i) {
                    if (effects.at(i).attribute(QStringLiteral("type")) == QLatin1String("audio")) {
                        deleteEffect(track, pos, clip->effect(videoIx));
                        audioIx++;
                    } else {
                        deleteEffect(destTrack, pos, audioClip->effect(audioIx));
                        videoIx++;
                    }
                }
                groupSelectedItems(QList <QGraphicsItem*>()<<clip<<audioClip, true);
            }
        }
    } else {
        // unsplit clip: remove audio part and change video part to normal clip
        if (clip->parentItem() == NULL || clip->parentItem()->type() != GroupWidget) {
            //qDebug() << "//CANNOT FIND CLP GRP";
            return false;
        }
        AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(clip->parentItem());
        QList<QGraphicsItem *> children = grp->childItems();
        if (children.count() != 2) {
            //qDebug() << "//SOMETHING IS WRONG WITH CLP GRP";
            return false;
        }
        for (int i = 0; i < children.count(); ++i) {
            if (children.at(i) != clip) {
                ClipItem *clp = static_cast <ClipItem *>(children.at(i));
                ItemInfo info = clip->info();
                deleteClip(clp->info());
                if (!m_timeline->track(info.track)->replace(info.startPos.seconds(), m_document->renderer()->getBinProducer(clip->getBinId()))) {
                    emit displayMessage(i18n("Cannot update clip (time: %1, track: %2)", info.startPos.frames(m_document->fps()), info.track), ErrorMessage);
                    return false;
                }
                else {
                    clip->setState(PlaylistState::Original);
                }

                // re-add audio effects
                for (int i = 0; i < effects.count(); ++i) {
                    if (effects.at(i).attribute(QStringLiteral("type")) == QLatin1String("audio")) {
                        addEffect(track, pos, effects.at(i));
                    }
                }

                break;
            }
        }
        clip->setFlag(QGraphicsItem::ItemIsMovable, true);
        m_document->clipManager()->removeGroup(grp);
        if (grp == m_selectionGroup) m_selectionGroup = NULL;
        scene()->destroyItemGroup(grp);
    }
    return true;
}

void CustomTrackView::setClipType(PlaylistState::ClipState state)
{
    QString text = i18n("Audio and Video");
    if (state == PlaylistState::VideoOnly)
        text = i18n("Video only");
    else if (state == PlaylistState::AudioOnly)
        text = i18n("Audio only");
    else if (state == PlaylistState::Disabled)
        text = i18n("Disabled");

    resetSelectionGroup();
    QList<QGraphicsItem *> selection = scene()->selectedItems();
    if (selection.isEmpty()) {
        emit displayMessage(i18n("You must select one clip for this action"), ErrorMessage);
        return;
    }
    QUndoCommand *videoCommand = new QUndoCommand();
    videoCommand->setText(text);
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == AVWidget) {
            ClipItem *clip = static_cast <ClipItem *>(selection.at(i));
            if (clip->clipType() == AV || clip->clipType() == Playlist || clip->clipType() == Audio) {
                if (clip->parentItem()) {
                    emit displayMessage(i18n("Cannot change grouped clips"), ErrorMessage);
                } else {
                    new ChangeClipTypeCommand(this, clip->info(), state, clip->clipState(), videoCommand);
                }
            }
        }
    }
    m_commandStack->push(videoCommand);
}

void CustomTrackView::disableClip()
{
    resetSelectionGroup();
    QList<QGraphicsItem *> selection = scene()->selectedItems();
    if (selection.isEmpty()) {
        emit displayMessage(i18n("You must select one clip for this action"), ErrorMessage);
        return;
    }
    QUndoCommand *videoCommand = new QUndoCommand();
    videoCommand->setText(i18n("Disable clip"));
    QList <ItemInfo> range;
    // Expand groups
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == GroupWidget) {
            QList <QGraphicsItem *>children = selection.at(i)->childItems();
            foreach(QGraphicsItem *item, children) {
                if (!selection.contains(item))
                    selection << item;
            }
        } else if (selection.at(i)->parentItem() && !selection.contains(selection.at(i)->parentItem())) {
            QList <QGraphicsItem *>children = selection.at(i)->parentItem()->childItems();
            foreach(QGraphicsItem *item, children) {
                if (!selection.contains(item))
                    selection << item;
            }
        }
    }
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == AVWidget) {
            ClipItem *clip = static_cast <ClipItem *>(selection.at(i));
            ClipType cType = clip->clipType();
            if (cType == AV || cType == Playlist || cType == Audio) {
                PlaylistState::ClipState currentStatus = clip->clipState();
                PlaylistState::ClipState newStatus;
                if (currentStatus == PlaylistState::Disabled) {
                    newStatus = clip->originalState();
                } else {
                    newStatus = PlaylistState::Disabled;
                }
                new ChangeClipTypeCommand(this, clip->info(), newStatus, currentStatus, videoCommand);
                if (newStatus != PlaylistState::AudioOnly && currentStatus != PlaylistState::AudioOnly && cType != Audio)
                    range << clip->info();
            }
        }
    }
    m_commandStack->push(videoCommand);
    monitorRefresh(range, true);
}

void CustomTrackView::monitorRefresh(QList <ItemInfo> range, bool invalidateRange)
{
    bool refreshMonitor = false;
    for (int i = 0; i < range.count(); i++) {
        if (range.at(i).contains(GenTime(m_cursorPos, m_document->fps())))
            refreshMonitor = true;
        if (invalidateRange)
            m_timeline->invalidateRange(range.at(i));
    }
    if (refreshMonitor)
        m_document->renderer()->doRefresh();
}

void CustomTrackView::monitorRefresh(ItemInfo range, bool invalidateRange)
{
    if (range.contains(GenTime(m_cursorPos, m_document->fps())))
        m_document->renderer()->doRefresh();
    if (invalidateRange)
        m_timeline->invalidateRange(range);
}

void CustomTrackView::monitorRefresh(bool invalidateRange)
{
    m_document->renderer()->doRefresh();
    if (invalidateRange)
        m_timeline->invalidateRange();
}

void CustomTrackView::doChangeClipType(ItemInfo info, PlaylistState::ClipState state)
{
    ClipItem *clip = getClipItemAtStart(info.startPos, info.track);
    if (clip == NULL) {
        emit displayMessage(i18n("Cannot find clip to edit (time: %1, track: %2)", info.startPos.frames(m_document->fps()), m_timeline->getTrackInfo(info.track).trackName), ErrorMessage);
        return;
    }
    Mlt::Producer *prod;
    double speed = clip->speed();
    PlaylistState::ClipState previousState = clip->clipState();
    if (state == PlaylistState::VideoOnly) {
        prod = m_document->renderer()->getBinVideoProducer(clip->getBinId());
    }
    else {
        prod = m_document->renderer()->getBinProducer(clip->getBinId());
    }
    if (speed != 1) {
        QLocale locale;
        QString url = prod->get("resource");
        Track::SlowmoInfo slowmoInfo;
        slowmoInfo.speed = speed;
        slowmoInfo.strobe = 1;
        slowmoInfo.state = state;
        Mlt::Producer *copy = m_document->renderer()->getSlowmotionProducer(slowmoInfo.toString(locale) + url);
        if (copy == NULL) {
            // create mute slowmo producer
            url.prepend(locale.toString(speed) + ":");
            Mlt::Properties passProperties;
            Mlt::Properties original(prod->get_properties());
            passProperties.pass_list(original, ClipController::getPassPropertiesList(false));
            copy = m_timeline->track(info.track)->buildSlowMoProducer(passProperties, url, clip->getBinId(), slowmoInfo);
        }
        if (copy == NULL) {
            // Failed to get slowmo producer, error
            qDebug()<<"Failed to get slowmo producer, error";
            return;
        }
        prod = copy;
    }
    if (prod && prod->is_valid() && m_timeline->track(info.track)->replace(info.startPos.seconds(), prod, state, previousState)) {
        clip->setState(state);
        clip->update();
        if (clip->clipType() != Audio && state != PlaylistState::Disabled && previousState != PlaylistState::Disabled && (previousState == PlaylistState::AudioOnly || state == PlaylistState::AudioOnly)) {
            // Disabled state is handled upstream, so check if we switch to / from an audio state
            monitorRefresh(info, true);
        }
    } else {
        // Changing clip type failed
        emit displayMessage(i18n("Cannot update clip (time: %1, track: %2)", info.startPos.frames(m_document->fps()), m_timeline->getTrackInfo(info.track).trackName), ErrorMessage);
        return;
    }
}

void CustomTrackView::updateClipTypeActions(ClipItem *clip)
{
    bool hasAudio;
    bool hasAV;
    if (clip == NULL) {
        m_clipTypeGroup->setEnabled(false);
        hasAudio = false;
        hasAV = false;
    } else {
        switch (clip->clipType()) {
        case AV:
        case Playlist:
            hasAudio = true;
            hasAV = true;
            m_clipTypeGroup->setEnabled(true);
            break;
        case Audio:
            hasAudio = true;
            hasAV = false;
            m_clipTypeGroup->setEnabled(true);
            break;
        default:
            hasAudio = false;
            hasAV = false;
            m_clipTypeGroup->setEnabled(false);
        }
        QList <QAction *> actions = m_clipTypeGroup->actions();
        for (int i = 0; i < actions.count(); ++i) {
            if (actions.at(i)->data().toInt() == clip->clipState()) {
                actions.at(i)->setChecked(true);
                break;
            }
        }
    }
    for (int i = 0; i < m_audioActions.count(); ++i) {
        m_audioActions.at(i)->setEnabled(hasAudio);
    }
    for (int i = 0; i < m_avActions.count(); ++i) {
        m_avActions.at(i)->setEnabled(hasAV);
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
    QDomElement lumaTransition = MainWindow::transitions.getEffectByTag(QStringLiteral("luma"), QStringLiteral("luma"));
    QDomNodeList params = lumaTransition.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("tag")) == QLatin1String("resource")) {
            lumaNames = e.attribute(QStringLiteral("paramlistdisplay"));
            lumaFiles = e.attribute(QStringLiteral("paramlist"));
            break;
        }
    }

    QList<QGraphicsItem *> itemList = items();
    Transition *transitionitem;
    QDomElement transitionXml;
    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == TransitionWidget) {
            transitionitem = static_cast <Transition*>(itemList.at(i));
            transitionXml = transitionitem->toXML();
            if (transitionXml.attribute(QStringLiteral("id")) == QLatin1String("luma") && transitionXml.attribute(QStringLiteral("tag")) == QLatin1String("luma")) {
                QDomNodeList params = transitionXml.elementsByTagName(QStringLiteral("parameter"));
                for (int i = 0; i < params.count(); ++i) {
                    QDomElement e = params.item(i).toElement();
                    if (e.attribute(QStringLiteral("tag")) == QLatin1String("resource")) {
                        e.setAttribute(QStringLiteral("paramlistdisplay"), lumaNames);
                        e.setAttribute(QStringLiteral("paramlist"), lumaFiles);
                        break;
                    }
                }
            }
            if (transitionXml.attribute(QStringLiteral("id")) == QLatin1String("composite") && transitionXml.attribute(QStringLiteral("tag")) == QLatin1String("composite")) {
                QDomNodeList params = transitionXml.elementsByTagName(QStringLiteral("parameter"));
                for (int i = 0; i < params.count(); ++i) {
                    QDomElement e = params.item(i).toElement();
                    if (e.attribute(QStringLiteral("tag")) == QLatin1String("luma")) {
                        e.setAttribute(QStringLiteral("paramlistdisplay"), lumaNames);
                        e.setAttribute(QStringLiteral("paramlist"), lumaFiles);
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
    if (m_dragItem) m_dragItem->setMainSelectedClip(false);
    m_dragItem = NULL;
    QList<QGraphicsItem *> itemList = items();
    for (int i = 0; i < itemList.count(); ++i) {
        // remove all items and re-add them one by one
        if (itemList.at(i) != m_cursorLine && itemList.at(i)->parentItem() == NULL) m_scene->removeItem(itemList.at(i));
    }
    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->parentItem() == 0 && (itemList.at(i)->type() == AVWidget || itemList.at(i)->type() == TransitionWidget)) {
            AbstractClipItem *clip = static_cast <AbstractClipItem *>(itemList.at(i));
            clip->updateFps(m_document->fps());
            m_scene->addItem(clip);
        } else if (itemList.at(i)->type() == GroupWidget) {
            AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(itemList.at(i));
            QList<QGraphicsItem *> children = grp->childItems();
            for (int j = 0; j < children.count(); ++j) {
                if (children.at(j)->type() == AVWidget || children.at(j)->type() == TransitionWidget) {
                    AbstractClipItem *clip = static_cast <AbstractClipItem *>(children.at(j));
                    clip->setFlag(QGraphicsItem::ItemIsMovable, true);
                    clip->updateFps(m_document->fps());
                }
            }
            m_document->clipManager()->removeGroup(grp);
            m_scene->addItem(grp);
            if (grp == m_selectionGroup) m_selectionGroup = NULL;
            scene()->destroyItemGroup(grp);
            scene()->clearSelection();
            /*for (int j = 0; j < children.count(); ++j) {
                if (children.at(j)->type() == AVWidget || children.at(j)->type() == TRANSITIONWIDGET) {
                    //children.at(j)->setParentItem(0);
                    children.at(j)->setSelected(true);
                }
            }*/
            groupSelectedItems(children, true);
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
    int ix;
    if (m_selectedTrack > 1) ix = m_selectedTrack - 1;
    else ix = m_timeline->tracksCount() - 1;
    slotSelectTrack(ix);
}

void CustomTrackView::slotTrackUp()
{
    int ix;
    if (m_selectedTrack < m_timeline->tracksCount() - 1) ix = m_selectedTrack + 1;
    else ix = 1;
    slotSelectTrack(ix);
}

int CustomTrackView::selectedTrack() const
{
    return m_selectedTrack;
}

QStringList CustomTrackView::selectedClips() const
{
    QStringList clipIds;
    QList<QGraphicsItem *> selection = m_scene->selectedItems();
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == AVWidget) {
            ClipItem *item = static_cast<ClipItem*>(selection.at(i));
            clipIds << item->getBinId();
        }
    }
    return clipIds;
}

void CustomTrackView::slotSelectTrack(int ix, bool switchTarget)
{
    m_selectedTrack = qBound(1, ix, m_timeline->tracksCount() - 1);
    emit updateTrackHeaders();
    m_timeline->slotShowTrackEffects(ix);
    QRectF rect(mapToScene(QPoint(10, 0)).x(), getPositionFromTrack(ix) , 10, m_tracksHeight);
    ensureVisible(rect, 0, 0);
    viewport()->update();
    if (switchTarget)
	m_timeline->switchTrackTarget();
}

void CustomTrackView::slotSelectClipsInTrack()
{
    QRectF rect(0, getPositionFromTrack(m_selectedTrack) + m_tracksHeight / 2, sceneRect().width(), m_tracksHeight / 2 - 1);
    resetSelectionGroup();
    QList<QGraphicsItem *> selection = m_scene->items(rect);
    m_scene->clearSelection();
    QList<QGraphicsItem *> list;
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == AVWidget || selection.at(i)->type() == TransitionWidget || selection.at(i)->type() == GroupWidget) {
            list.append(selection.at(i));
        }
    }
    groupSelectedItems(list, false, true);
}

void CustomTrackView::slotSelectAllClips()
{
    m_scene->clearSelection();
    resetSelectionGroup();
    groupSelectedItems(m_scene->items(), false, true);
}

void CustomTrackView::selectClip(bool add, bool group, int track, int pos)
{
    QRectF rect;
    if (track != -1 && pos != -1)
        rect = QRectF(pos, getPositionFromTrack(track) + m_tracksHeight / 2, 1, 1);
    else
        rect = QRectF(m_cursorPos, getPositionFromTrack(m_selectedTrack) + m_tracksHeight / 2, 1, 1);
    QList<QGraphicsItem *> selection = m_scene->items(rect);
    resetSelectionGroup(group);
    if (!group) m_scene->clearSelection();
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == AVWidget) {
            selection.at(i)->setSelected(add);
            break;
        }
    }
    if (group) groupSelectedItems();
}

void CustomTrackView::selectTransition(bool add, bool group)
{
    QRectF rect(m_cursorPos, getPositionFromTrack(m_selectedTrack) + m_tracksHeight, 1, 1);
    QList<QGraphicsItem *> selection = m_scene->items(rect);
    resetSelectionGroup(group);
    if (!group) m_scene->clearSelection();
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == TransitionWidget) {
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
    for (int i = 0; i < itemList.count(); ++i) {
        if (itemList.at(i)->type() == TransitionWidget) {
            transitionitem = static_cast <Transition*>(itemList.at(i));
            transitionXml = transitionitem->toXML();
            // luma files in transitions are in "resource" property
            QString luma = EffectsList::parameter(transitionXml, QStringLiteral("resource"));
            if (!luma.isEmpty()) urls << QUrl(luma).path();
        }
    }
    urls.removeDuplicates();
    return urls;
}

void CustomTrackView::setEditMode(TimelineMode::EditMode mode)
{
    m_scene->setEditMode(mode);
}

void CustomTrackView::checkTrackSequence(int track)
{
    QList <int> times = m_document->renderer()->checkTrackSequence(track);
    QRectF rect(0, getPositionFromTrack(track) + m_tracksHeight / 2, sceneRect().width(), 2);
    QList<QGraphicsItem *> selection = m_scene->items(rect);
    QList <int> timelineList;
    timelineList.append(0);
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == AVWidget) {
            ClipItem *clip = static_cast <ClipItem *>(selection.at(i));
            int start = clip->startPos().frames(m_document->fps());
            int end = clip->endPos().frames(m_document->fps());
            if (!timelineList.contains(start)) timelineList.append(start);
            if (!timelineList.contains(end)) timelineList.append(end);
        }
    }
    qSort(timelineList);
    //qDebug() << "// COMPARE:\n" << times << '\n' << timelineList << "\n-------------------";
    if (times != timelineList) KMessageBox::sorry(this, i18n("error"), i18n("TRACTOR"));
}

void CustomTrackView::insertZone(TimelineMode::EditMode sceneMode, const QString clipId, QPoint binZone)
{
    bool extractAudio = true;
    bool extractVideo = true;
    ProjectClip *clp = m_document->getBinClip(clipId);
    if (!clp) {
        emit displayMessage(i18n("Select a Bin Clip to perform operation"), ErrorMessage);
        return;
    }
    ClipType cType = clp->clipType();
    if (KdenliveSettings::splitaudio()) {
        if (m_timeline->audioTarget == -1 || m_timeline->getTrackInfo(m_timeline->audioTarget).isLocked) 
            extractAudio = false;
        if (cType == Audio || m_timeline->videoTarget == -1 || m_timeline->getTrackInfo(m_timeline->videoTarget).isLocked)
            extractVideo = false;
    }
    else if (m_timeline->getTrackInfo(m_selectedTrack).isLocked) {
        // Cannot perform an Extract operation on a locked track
        emit displayMessage(i18n("Cannot perform operation on a locked track"), ErrorMessage);
        return;
    }
    if (binZone.isNull()) return;
    QPoint timelineZone;
    if (KdenliveSettings::useTimelineZoneToEdit()) {
        timelineZone = m_document->zone();
        timelineZone.setY(timelineZone.y() + 1);
    }
    else {
        timelineZone.setX(seekPosition());
        timelineZone.setY(-1);
    }
    ItemInfo info;
    int binLength = binZone.y() - binZone.x() + 1;
    int timelineLength = timelineZone.y() - timelineZone.x();
    if (KdenliveSettings::useTimelineZoneToEdit()) {
	if (clp->hasLimitedDuration()) {
	    //Make sure insert duration is not longer than clip length
	    timelineLength = qMin(timelineLength, (int) clp->duration().frames(m_document->fps()) - binZone.x());
	} else if (timelineLength > clp->duration().frames(m_document->fps())) {
	    // Update source clip max length
	    clp->setProducerProperty(QStringLiteral("length"), timelineLength + 1);
	}
    }
    info.startPos = GenTime(timelineZone.x(), m_document->fps());
    info.cropStart = GenTime(binZone.x(), m_document->fps());
    info.endPos = info.startPos + GenTime(KdenliveSettings::useTimelineZoneToEdit() ? timelineLength : binLength, m_document->fps());
    info.cropDuration = info.endPos - info.startPos;
    info.track = m_selectedTrack;
    QUndoCommand *addCommand = new QUndoCommand();
    addCommand->setText(i18n("Insert clip"));
    if (sceneMode == TimelineMode::OverwriteEdit) {
        // We clear the selected zone in all non locked tracks
        extractZone(QPoint(timelineZone.x(), info.endPos.frames(m_document->fps())), false, QList <ItemInfo>(), addCommand);
    } else {
        // Cut and move clips forward
        cutTimeline(timelineZone.x(), QList <ItemInfo>(), QList <ItemInfo>(), addCommand);
        new AddSpaceCommand(this, info, QList <ItemInfo>(), true, addCommand);
    }
    if (KdenliveSettings::splitaudio()) {
        if (extractVideo) {
            info.track = m_timeline->videoTarget;
            if (extractAudio) {
                new AddTimelineClipCommand(this, clipId, info, EffectsList(), PlaylistState::Original, true, false, true, addCommand);
            } else {
                new AddTimelineClipCommand(this, clipId, info, EffectsList(), PlaylistState::VideoOnly, true, false, true, addCommand);
            }
        } else if (extractAudio) {
            // Extract audio only
            info.track = m_timeline->audioTarget;
            new AddTimelineClipCommand(this, clipId, info, EffectsList(), cType == Audio ? PlaylistState::Original : PlaylistState::AudioOnly, true, false, false, addCommand);
        } else {
            emit displayMessage(i18n("No target track(s) selected"), InformationMessage);
        }
    }
    else {
        new AddTimelineClipCommand(this, clipId, info, EffectsList(), PlaylistState::Original, true, false, true, addCommand);
    }
    // Automatic audio split
    if (KdenliveSettings::splitaudio() && extractVideo && extractAudio) {
        if (!m_timeline->getTrackInfo(m_timeline->audioTarget).isLocked && clp->isSplittable())
            splitAudio(false, info, m_timeline->audioTarget, addCommand);
    }
    updateTrackDuration(info.track, addCommand);
    m_commandStack->push(addCommand);
    selectClip(true, false, m_selectedTrack, timelineZone.x());
}

void CustomTrackView::clearSelection(bool emitInfo)
{
    if (m_dragItem) m_dragItem->setSelected(false);
    resetSelectionGroup();
    scene()->clearSelection();
    if (m_dragItem) m_dragItem->setMainSelectedClip(false);
    m_dragItem = NULL;
    if (emitInfo) emit clipItemSelected(NULL);
}

void CustomTrackView::updatePalette()
{
    if (m_cursorLine) {
        QPen pen1 = m_cursorLine->pen();
        QColor line(palette().text().color());
        line.setAlpha(pen1.color().alpha());
        pen1.setColor(line);
        m_cursorLine->setPen(pen1);
    }
    QIcon razorIcon = KoIconUtils::themedIcon(QStringLiteral("edit-cut"));
    m_razorCursor = QCursor(razorIcon.pixmap(32, 32));
    KColorScheme scheme(palette().currentColorGroup(), KColorScheme::Window, KSharedConfig::openConfig(KdenliveSettings::colortheme()));
    m_selectedTrackColor = scheme.background(KColorScheme::ActiveBackground ).color();
    m_selectedTrackColor.setAlpha(150);
    m_lockedTrackColor = scheme.background(KColorScheme::NegativeBackground ).color();
    m_lockedTrackColor.setAlpha(150);
}

void CustomTrackView::removeTipAnimation()
{
    if (m_visualTip) {
        scene()->removeItem(m_visualTip);
        m_keyPropertiesTimer->stop();
        delete m_keyProperties;
        m_keyProperties = NULL;
        delete m_visualTip;
        m_visualTip = NULL;
    }
}

bool CustomTrackView::hasAudio(int track) const
{
    QRectF rect(0, (double)(getPositionFromTrack(track) + 1), (double) sceneRect().width(), (double)(m_tracksHeight - 1));
    QList<QGraphicsItem *> collisions = scene()->items(rect, Qt::IntersectsItemBoundingRect);
    for (int i = 0; i < collisions.count(); ++i) {
        QGraphicsItem *item = collisions.at(i);
        if (!item->isEnabled()) continue;
        if (item->type() == AVWidget) {
            ClipItem *clip = static_cast <ClipItem *>(item);
            if (clip->clipState() != PlaylistState::VideoOnly && (clip->clipType() == Audio || clip->clipType() == AV || clip->clipType() == Playlist)) return true;
        }
    }
    return false;
}

void CustomTrackView::slotAddTrackEffect(QDomElement effect, int ix)
{
    EffectsController::initTrackEffect(m_document->getProfileInfo(), effect);
    QUndoCommand *effectCommand = new QUndoCommand();
    QString effectName;
    if (effect.tagName() == QLatin1String("effectgroup")) {
        effectName = effect.attribute(QStringLiteral("name"));
    } else {
        QDomElement namenode = effect.firstChildElement(QStringLiteral("name"));
        if (!namenode.isNull()) effectName = i18n(namenode.text().toUtf8().data());
        else effectName = i18n("effect");
    }
    effectCommand->setText(i18n("Add %1", effectName));
    if (effect.tagName() == QLatin1String("effectgroup")) {
        QDomNodeList effectlist = effect.elementsByTagName(QStringLiteral("effect"));
        for (int j = 0; j < effectlist.count(); ++j) {
            QDomElement trackeffect = effectlist.at(j).toElement();
            if (trackeffect.attribute(QStringLiteral("unique"), QStringLiteral("0")) != QLatin1String("0") && m_timeline->hasTrackEffect(ix, trackeffect.attribute(QStringLiteral("tag")), trackeffect.attribute(QStringLiteral("id"))) != -1) {
                emit displayMessage(i18n("Effect already present in track"), ErrorMessage);
                continue;
            }
            new AddEffectCommand(this, ix, GenTime(-1), trackeffect, true, effectCommand);
        }
    }
    else {
        if (effect.attribute(QStringLiteral("unique"), QStringLiteral("0")) != QLatin1String("0") && m_timeline->hasTrackEffect(ix, effect.attribute(QStringLiteral("tag")), effect.attribute(QStringLiteral("id"))) != -1) {
            emit displayMessage(i18n("Effect already present in track"), ErrorMessage);
            delete effectCommand;
            return;
        }
        new AddEffectCommand(this, ix, GenTime(-1), effect, true, effectCommand);
    }

    if (effectCommand->childCount() > 0) {
        m_commandStack->push(effectCommand);
    }
    else delete effectCommand;
}


void CustomTrackView::updateTrackDuration(int track, QUndoCommand *command)
{
    Q_UNUSED(command)

    QList<int> tracks;
    if (track >= 0) {
        tracks << track;
    } else {
        // negative track number -> update all tracks
        for (int i = 1; i < m_timeline->tracksCount(); ++i)
            tracks << i;
    }
    /*for (int i = 0; i < tracks.count(); ++i) {
        int t = tracks.at(i);
        // t + 1 because of black background track
        int duration = m_document->renderer()->mltTrackDuration(t + 1);
        if (duration != m_document->trackDuration(t)) {
            m_document->setTrackDuration(t, duration);

            // update effects
            EffectsList effects = m_document->getTrackEffects(t);
            for (int j = 0; j < effects.count(); ++j) {
                // TODO
                 // - lookout for keyframable parameters and update them so all keyframes are in the new range (0 - duration)
                 // - update the effectstack if necessary
                 //
            }
        }
    }*/
}


void CustomTrackView::adjustEffects(ClipItem* item, ItemInfo oldInfo, QUndoCommand* command)
{
    QMap<int, QDomElement> effects = item->adjustEffectsToDuration(oldInfo);
    if (!effects.isEmpty()) {
        QMap<int, QDomElement>::const_iterator i = effects.constBegin();
        while (i != effects.constEnd()) {
            new EditEffectCommand(this, item->track(), item->startPos(), i.value(), item->effect(i.key()), i.value().attribute(QStringLiteral("kdenlive_ix")).toInt(), true, true, false, command);
            ++i;
        }
    }
    if (item == m_dragItem) {
        // clip is selected, update effect stack
        emit clipItemSelected(item);
    }
}


void CustomTrackView::slotGotFilterJobResults(const QString &/*id*/, int startPos, int track, stringMap filterParams, stringMap extra)
{
    ClipItem *clip = getClipItemAtStart(GenTime(startPos, m_document->fps()), track);
    if (clip == NULL) {
        emit displayMessage(i18n("Cannot find clip for effect update %1.", extra.value("finalfilter")), ErrorMessage);
        return;
    }
    QDomElement newEffect;
    QDomElement effect = clip->effectAtIndex(clip->selectedEffectIndex());
    if (effect.attribute(QStringLiteral("id")) == extra.value(QStringLiteral("finalfilter"))) {
        newEffect = effect.cloneNode().toElement();
        QMap<QString, QString>::const_iterator i = filterParams.constBegin();
        while (i != filterParams.constEnd()) {
            EffectsList::setParameter(newEffect, i.key(), i.value());
            ++i;
        }
        EditEffectCommand *command = new EditEffectCommand(this, clip->track(), clip->startPos(), effect, newEffect, clip->selectedEffectIndex(), true, true, true);
        m_commandStack->push(command);
        emit clipItemSelected(clip);
    }
}


void CustomTrackView::slotImportClipKeyframes(GraphicsRectItem type, ItemInfo info, QDomElement xml, QMap<QString, QString> data)
{
    ClipItem *item = NULL;
    ItemInfo srcInfo;
    if (data.isEmpty()) {
        if (type == TransitionWidget) {
            // We want to import keyframes to a transition
            if (!m_selectionGroup) {
                emit displayMessage(i18n("You need to select one clip and one transition"), ErrorMessage);
                return;
            }
            // Make sure there is no collision
            QList<QGraphicsItem *> children = m_selectionGroup->childItems();
            for (int i = 0; i < children.count(); ++i) {
                if (children.at(i)->type() == AVWidget) {
                    item = static_cast<ClipItem*>(children.at(i));
                    srcInfo = item->info();
                    break;
                }
            }
        }
        else {
            // Import keyframes from current clip to its effect
            if (m_dragItem && m_dragItem->type() == AVWidget) item = static_cast<ClipItem*> (m_dragItem);
        }
        if (!item) {
            emit displayMessage(i18n("No clip found"), ErrorMessage);
            return;
        }
        data = item->binClip()->analysisData();
    }
    if (data.isEmpty()) {
        emit displayMessage(i18n("No keyframe data found in clip"), ErrorMessage);
        return;
    }
    QPointer<KeyframeImport>import = new KeyframeImport(srcInfo, info, data, m_document->timecode(), xml, m_document->getProfileInfo(), this);
    if (import->exec() != QDialog::Accepted) {
        delete import;
        return;
    }
    QString keyframeData = import->selectedData();
    QString tag = import->selectedTarget();
    emit importKeyframes(type, tag, keyframeData);
    delete import;
}

void CustomTrackView::slotReplaceTimelineProducer(const QString &id)
{
    Mlt::Producer *prod = m_document->renderer()->getBinProducer(id);
    Mlt::Producer *videoProd = m_document->renderer()->getBinVideoProducer(id);
    QList <Track::SlowmoInfo> allSlows;
    for (int i = 1; i < m_timeline->tracksCount(); i++) {
	allSlows << m_timeline->track(i)->getSlowmotionInfos(id);
    }
    QLocale locale;
    QString url = prod->get("resource");
    // generate all required slowmo producers
    QStringList processedUrls;
    QMap <QString, Mlt::Producer *> newSlowMos;
    for (int i = 0; i < allSlows.count(); i++) {
	// find out speed and strobe values
        Track::SlowmoInfo info = allSlows.at(i);
        QString wrapUrl = QStringLiteral("timewarp:") + locale.toString(info.speed) + ':' + url;
	// build slowmo producer
	if (processedUrls.contains(wrapUrl)) continue;
        processedUrls << wrapUrl;
	Mlt::Producer *slowProd = new Mlt::Producer(*prod->profile(), 0, wrapUrl.toUtf8().constData());
	if (!slowProd->is_valid()) {
                qDebug()<<"++++ FAILED TO CREATE SLOWMO PROD";
		continue;
        }
        //if (strobe > 1) slowProd->set("strobe", strobe);
        QString producerid = "slowmotion:" + id + ':' + info.toString(locale);
        slowProd->set("id", producerid.toUtf8().constData());
	newSlowMos.insert(info.toString(locale), slowProd);
    }
    QList <ItemInfo> toUpdate;
    for (int i = 1; i < m_timeline->tracksCount(); i++) {
        toUpdate << m_timeline->track(i)->replaceAll(id,  prod, videoProd, newSlowMos);
    }

    // update slowmotion storage
    QMapIterator<QString, Mlt::Producer *> i(newSlowMos);
    while (i.hasNext()) {
	i.next();
	Mlt::Producer *sprod = i.value();
	m_document->renderer()->storeSlowmotionProducer(i.key() + url, sprod, true);
    }
    if (!toUpdate.isEmpty())
        monitorRefresh(toUpdate, true);
    m_timeline->refreshTractor();
}

void CustomTrackView::slotPrepareTimelineReplacement(const QString &id)
{
    for (int i = 1; i < m_timeline->tracksCount(); i++) {
        m_timeline->track(i)->replaceId(id);
    }
}

void CustomTrackView::slotUpdateTimelineProducer(const QString &id)
{
    Mlt::Producer *prod = m_document->renderer()->getBinProducer(id);
    for (int i = 1; i < m_timeline->tracksCount(); i++) {
        m_timeline->track(i)->updateEffects(id,  prod);
    }
}

bool CustomTrackView::hasSelection() const
{
    return (m_selectionGroup || m_dragItem);
}

void CustomTrackView::exportTimelineSelection(QString path)
{
    if (!m_selectionGroup && !m_dragItem) {
	qDebug()<<"/// ARGH, NO SELECTION GRP";
	return;
    }
    if (path.isEmpty()) {
        QString clipFolder = KRecentDirs::dir(QStringLiteral(":KdenliveClipFolder"));
        if (clipFolder.isEmpty()) {
            clipFolder = QDir::homePath();
        }
        path = QFileDialog::getSaveFileName(this, i18n("Save Zone"), clipFolder, i18n("MLT playlist (*.mlt)"));
        if (path.isEmpty()) return;
    }
    QList<QGraphicsItem *> children;
    QRectF bounding;
    if (m_selectionGroup) {
	children = m_selectionGroup->childItems();
	bounding = m_selectionGroup->sceneBoundingRect();
    }
    else {
	// only one clip selected
	children << m_dragItem;
	bounding = m_dragItem->sceneBoundingRect();
    }
    int firstTrack = getTrackFromPos(bounding.bottom());
    int lastTrack = getTrackFromPos(bounding.top());
    // Build export playlist
    Mlt::Tractor *newTractor = new Mlt::Tractor(*(m_document->renderer()->getProducer()->profile()));
    Mlt::Field *field = newTractor->field();
    for (int i = firstTrack; i <= lastTrack; i++) {
	QScopedPointer<Mlt::Playlist> newTrackPlaylist(new Mlt::Playlist(*newTractor->profile()));
	newTractor->set_track(*newTrackPlaylist, i - firstTrack);
    }
    int startOffest = m_projectDuration;
    // Find first frame of selection
    for (int i = 0; i < children.count(); ++i) {
	QGraphicsItem *item = children.at(i);
	if (item->type() != TransitionWidget && item->type() != AVWidget) {
	      continue;
	}
	AbstractClipItem *it = static_cast<AbstractClipItem *>(item);
	if (!it) continue;
	int pos = it->startPos().frames(m_document->fps());
	if (pos < startOffest) startOffest = pos;
    }
    for (int i = 0; i < children.count(); ++i) {
	QGraphicsItem *item = children.at(i);
	if (item->type() == AVWidget) {
            ClipItem *clip = static_cast<ClipItem*>(item);
	    int track = clip->track() - firstTrack;
	    m_timeline->duplicateClipOnPlaylist(clip->track(), clip->startPos().seconds(), startOffest, newTractor->track(track));
	}
	else if (item->type() == TransitionWidget) {
	    Transition *tr = static_cast<Transition*>(item);
	    int a_track = qBound(0, tr->transitionEndTrack() - firstTrack, lastTrack - firstTrack + 1);
	    int b_track = qBound(0, tr->track() - firstTrack, lastTrack - firstTrack + 1);;
	    m_timeline->transitionHandler->duplicateTransitionOnPlaylist(tr->startPos().frames(m_document->fps()) - startOffest, tr->endPos().frames(m_document->fps()) - startOffest, tr->transitionTag(), tr->toXML(), a_track, b_track, field);
	}
    }
    Mlt::Consumer xmlConsumer(*newTractor->profile(),  ("xml:" + path).toUtf8().constData());
    xmlConsumer.set("terminate_on_pause", 1);
    xmlConsumer.connect(*newTractor);
    xmlConsumer.run();
    delete newTractor;
}

int CustomTrackView::getTrackFromPos(double y) const
{
    int totalTracks = m_timeline->tracksCount() - 1;
    return qMax(1, totalTracks - ((int)(y / m_tracksHeight)));
}

int CustomTrackView::getPositionFromTrack(int track) const
{
    int totalTracks = m_timeline->tracksCount() - 1;
    return m_tracksHeight * (totalTracks - track);
}


void CustomTrackView::importPlaylist(ItemInfo info, QMap <QString, QString> idMap, QDomDocument doc, QUndoCommand *command)
{
    Mlt::Producer *import = new Mlt::Producer(*m_document->renderer()->getProducer()->profile(), "xml-string", doc.toString().toUtf8().constData());
    if (!import || !import->is_valid()) {
        delete command;
        qDebug()<<" / / /CANNOT open playlist to import ";
        return;
    }
    Mlt::Service service(import->parent().get_service());
    if (service.type() != tractor_type) {
        delete command;
        qDebug()<<" / / /CANNOT playlist file: "<<service.type();
        return;
    }

    // Parse imported file
    Mlt::Tractor tractor(service);
    int playlistTracks = tractor.count();
    int lowerTrack = info.track;
    if (lowerTrack + playlistTracks > m_timeline->tracksCount()) {
        lowerTrack = m_timeline->tracksCount() - playlistTracks;
    }
    if (lowerTrack <1) {
        qWarning()<<" / / / TOO many tracks in playlist for our timeline ";
        delete command;
        return;
    }
    // Check if there are no objects on the way
    double startY = getPositionFromTrack(lowerTrack + playlistTracks) + 1;
    QRectF r(info.startPos.frames(m_document->fps()), startY, (info.endPos - info.startPos).frames(m_document->fps()), m_tracksHeight * playlistTracks);
    QList<QGraphicsItem *> selection = m_scene->items(r);
    ClipItem *playlistToExpand = getClipItemAtStart(info.startPos, info.track);
    if (playlistToExpand) selection.removeAll(playlistToExpand);
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == TransitionWidget || selection.at(i)->type() == AVWidget) {
            qWarning()<<" / / /There are clips in timeline preventing expand actions";
            delete command;
            return;
        }
    }
    new RefreshMonitorCommand(this, info, false, true, command);
    for (int i = 0;  i < playlistTracks; i++) {
        int startPos = info.startPos.frames(m_document->fps());
        Mlt::Producer trackProducer(tractor.track(i));
        Mlt::Playlist trackPlaylist((mlt_playlist) trackProducer.get_service());
        for (int j = 0; j < trackPlaylist.count(); j++) {
            QScopedPointer<Mlt::Producer> original(trackPlaylist.get_clip(j));
            if (original == NULL || !original->is_valid()) {
                // invalid clip
                continue;
            }
            if (original->is_blank()) {
                startPos += original->get_playtime();
                continue;
            }
            // Found a producer, import it
            QString originalId = original->parent().get("id");
            // track producer
            if (originalId.contains(QLatin1Char('_'))) originalId = originalId.section(QLatin1Char('_'), 0, 0);
            // slowmotion producer
            if (originalId.contains(QLatin1Char(':'))) originalId = originalId.section(QLatin1Char(':'), 1, 1);
            // TODO: Handle speed effect!

            QString newId = idMap.value(originalId);
            qDebug() << "newId: " << newId << ", originalId: " << originalId;
            if (newId.isEmpty()) {
                qDebug()<<" / /WARNING, MISSING PRODUCER FOR: " << originalId;
                startPos += original->get_playtime();
                continue;
            }
            // Ready, insert clip
            ItemInfo insertInfo;
            insertInfo.startPos = GenTime(startPos, m_document->fps());
            int in = original->get_in();
            int out = original->get_out();
            insertInfo.cropStart = GenTime(in, m_document->fps());
            insertInfo.cropDuration = GenTime(out - in + 1, m_document->fps());
            insertInfo.endPos = insertInfo.startPos + insertInfo.cropDuration;
            insertInfo.track = lowerTrack + i;
            EffectsList effects = ClipController::xmlEffectList(original->profile(), *original);
            new AddTimelineClipCommand(this, newId, insertInfo, effects, PlaylistState::Original, true, false, false, command);
            startPos += original->get_playtime();
        }
        updateTrackDuration(lowerTrack + i, command);
    }

    // Paste transitions
    QScopedPointer<Mlt::Service> serv(tractor.field());
    while (serv && serv->is_valid()) {
        if (serv->type() == transition_type) {
            Mlt::Transition t((mlt_transition) serv->get_service());
            if (t.get_int("internal_added") > 0) {
                // This is an auto transition, skip
            }
            else {
                Mlt::Properties prop(t.get_properties());
                ItemInfo transitionInfo;
                transitionInfo.startPos = info.startPos + GenTime(t.get_in(), m_document->fps());
                transitionInfo.endPos = info.startPos + GenTime(t.get_out(), m_document->fps());
                transitionInfo.track = t.get_b_track() + lowerTrack;
                int endTrack = t.get_a_track() + lowerTrack;
                if (prop.get("kdenlive_id") == NULL && QString(prop.get("mlt_service")) == "composite" && Timeline::isSlide(prop.get("geometry")))
                    prop.set("kdenlive_id", "slide");
                QDomElement base = MainWindow::transitions.getEffectByTag(prop.get("mlt_service"), prop.get("kdenlive_id")).cloneNode().toElement();

                QDomNodeList params = base.elementsByTagName("parameter");
                for (int i = 0; i < params.count(); ++i) {
                    QDomElement e = params.item(i).toElement();
                    QString paramName = e.hasAttribute("tag") ? e.attribute("tag") : e.attribute("name");
                    QString value;
                    if (paramName == "a_track") {
                        value = QString::number(transitionInfo.track);
                    }
                    else if (paramName == "b_track") {
                        value = QString::number(endTrack);
                    }
                    else value = prop.get(paramName.toUtf8().constData());
                    //int factor = e.attribute("factor").toInt();
                    if (value.isEmpty()) continue;
                    e.setAttribute("value", value);
                }
                base.setAttribute("force_track", prop.get_int("force_track"));
                base.setAttribute("automatic", prop.get_int("automatic"));
                new AddTransitionCommand(this, transitionInfo, endTrack, base, false, true, command);
            }
        }
        serv.reset(serv->producer());
    }

    if (command->childCount() > 0) {
        new RefreshMonitorCommand(this, info, true, false, command);
        m_commandStack->push(command);
    }
    else delete command;
}

void CustomTrackView::slotRefreshCutLine()
{
    if (m_cutLine) {
        QPointF pos = mapToScene(mapFromGlobal(QCursor::pos()));
        int mappedXPos = qMax((int)(pos.x()), 0);
        m_cutLine->setPos(mappedXPos, getPositionFromTrack(getTrackFromPos(pos.y())));
    }
}

void CustomTrackView::updateTransitionWidget(Transition *tr, ItemInfo info)
{
    QPoint p;
    ClipItem *transitionClip = getClipItemAtStart(info.startPos, info.track);
    if (transitionClip && transitionClip->binClip()) {
        int frameWidth = transitionClip->binClip()->getProducerIntProperty(QStringLiteral("meta.media.width"));
        int frameHeight = transitionClip->binClip()->getProducerIntProperty(QStringLiteral("meta.media.height"));
        double factor = transitionClip->binClip()->getProducerProperty(QStringLiteral("aspect_ratio")).toDouble();
        if (factor == 0) factor = 1.0;
        p.setX((int)(frameWidth * factor + 0.5));
        p.setY(frameHeight);
    }
    emit transitionItemSelected(tr, getPreviousVideoTrack(tr->track()), p, true);
}

void CustomTrackView::dropTransitionGeometry(Transition *trans, const QString &geometry)
{
    if (m_dragItem && m_dragItem != trans) {
        clearSelection(false);
        m_dragItem = trans;
        m_dragItem->setMainSelectedClip(true);
        trans->setSelected(true);
        updateTimelineSelection();
    }
    QMap <QString, QString> data;
    data.insert(i18n("Dropped Geometry"), geometry);
    slotImportClipKeyframes(TransitionWidget, trans->info(), trans->toXML(), data);
}

void CustomTrackView::breakLockedGroups(QList<ItemInfo> clipsToMove, QList<ItemInfo> transitionsToMove, QUndoCommand *masterCommand, bool doIt)
{
    QList <AbstractGroupItem *> processedGroups;
    for (int i = 0; i < clipsToMove.count(); ++i) {
        ClipItem *clip = getClipItemAtStart(clipsToMove.at(i).startPos, clipsToMove.at(i).track);
        if (clip == NULL || !clip->parentItem()) {
            qDebug()<<" * ** Canot find clip to break: "<<clipsToMove.at(i).startPos.frames(25)<<", "<<clipsToMove.at(i).track;
            continue;
        }
        // If group has a locked item, ungroup first
        AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(clip->parentItem());
        if (grp->isItemLocked() && !processedGroups.contains(grp)) {
            processedGroups << grp;
            groupClips(false, grp->childItems(), true, masterCommand, doIt);
        }
    }
    for (int i = 0; i < transitionsToMove.count(); ++i) {
        Transition *trans = getTransitionItemAtStart(transitionsToMove.at(i).startPos, transitionsToMove.at(i).track);
        if (trans == NULL || !trans->parentItem())
            continue;
        // If group has a locked item, ungroup first
        AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(trans->parentItem());
        if (grp->isItemLocked() && !processedGroups.contains(grp)) {
            processedGroups << grp;
            groupClips(false, grp->childItems(), true, masterCommand, doIt);
        }
    }
}

void CustomTrackView::switchTrackLock()
{
    slotSwitchTrackLock(m_selectedTrack, !m_timeline->getTrackInfo(m_selectedTrack).isLocked);
}

void CustomTrackView::switchAllTrackLock()
{
    if (m_selectedTrack > 1)
        slotSwitchTrackLock(m_selectedTrack, !m_timeline->getTrackInfo(1).isLocked, true);
    else if (m_timeline->visibleTracksCount() > 1)
        slotSwitchTrackLock(m_selectedTrack, !m_timeline->getTrackInfo(2).isLocked, true);
}

void CustomTrackView::slotAcceptRipple(bool accept)
{
    m_timeline->removeSplitOverlay();
    QMetaObject::invokeMethod(this, "doRipple", Qt::QueuedConnection, Q_ARG(bool, accept));
}

void CustomTrackView::doRipple(bool accept)
{
    if (accept) {
        QUndoCommand *command = new QUndoCommand;
        command->setText(i18n("Ripple Edit"));
        ItemInfo info = m_dragItem->info();
        int resizePos = m_cursorPos;
        // Ripple clip start
        ClipItem *second = getClipItemAtEnd(info.startPos, info.track);// - GenTime(1, m_document->fps()), info.track);
        if (!second) {
            // Something is wrong
            qDebug()<<" * * ** CAMNNOT FIND SECONT CLIP";
            emit displayMessage(i18n("Cannot find clip"), InformationMessage);
            monitorRefresh();
            QTimer::singleShot(0, this, SLOT(resetScene()));
            return;
        }
        ItemInfo secondInfo = second->info();
        if (resizePos > info.startPos.frames(m_document->fps())) {
            // shortening target clip, resize selected clip first
            prepareResizeClipStart(m_dragItem, info, resizePos, true, command);
            prepareResizeClipEnd(second, secondInfo, resizePos, true, command);
        } else {
            prepareResizeClipEnd(second, secondInfo, resizePos, true, command);
            prepareResizeClipStart(m_dragItem, info, resizePos, true, command);
        }
        m_commandStack->push(command);
    }
    monitorRefresh();
    emit loadMonitorScene(MonitorSceneDefault, false);
}

void CustomTrackView::setOperationMode(OperationType mode)
{
    m_moveOpMode = mode;
}

OperationType CustomTrackView::prepareMode() const
{
    return m_operationMode;
}

OperationType CustomTrackView::operationMode() const
{
    return m_moveOpMode;
}

AbstractClipItem *CustomTrackView::dragItem()
{
    return m_dragItem;
}

AbstractGroupItem *CustomTrackView::selectionGroup()
{
    AbstractGroupItem *group = NULL;
    if (m_selectionGroup) {
        group = static_cast <AbstractGroupItem *>(m_selectionGroup);
    }
    else if (m_dragItem) {
        group = static_cast <AbstractGroupItem *>(m_dragItem->parentItem());
    }
    return group;
}

TimelineMode::EditMode CustomTrackView::sceneEditMode()
{
    return m_scene->editMode();
}

bool CustomTrackView::isLastClip(ItemInfo info)
{
    return m_timeline->isLastClip(info);
}

TrackInfo CustomTrackView::getTrackInfo(int ix)
{
    return m_timeline->getTrackInfo(ix);
}

Timecode CustomTrackView::timecode()
{
    return m_document->timecode();
}

void CustomTrackView::reloadTrack(ItemInfo info, bool includeLastFrame)
{
    m_timeline->reloadTrack(info, includeLastFrame);
}

void CustomTrackView::sortGuides()
{
    qSort(m_guides.begin(), m_guides.end(), sortGuidesList);
}
