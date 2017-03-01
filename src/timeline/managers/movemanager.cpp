/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "movemanager.h"
#include "kdenlivesettings.h"
#include "timeline/customtrackview.h"
#include "timeline/clipitem.h"
#include "timeline/transition.h"
#include "timeline/timelinecommands.h"
#include "timeline/abstractgroupitem.h"
#include "timeline/transitionhandler.h"

#include <KLocalizedString>
#include <QScrollBar>
#include <QFontMetrics>
#include <QApplication>

MoveManager::MoveManager(TransitionHandler *handler, CustomTrackView *view, DocUndoStack *commandStack) : AbstractToolManager(MoveType, view, commandStack)
    , m_transitionHandler(handler)
    , m_scrollOffset(0)
    , m_scrollTrigger(QFontMetrics(view->font()).averageCharWidth() * 3)
    , m_dragMoved(false)
{
    connect(&m_scrollTimer, &QTimer::timeout, this, &MoveManager::slotCheckMouseScrolling);
    m_scrollTimer.setInterval(100);
    m_scrollTimer.setSingleShot(true);
}

bool MoveManager::mousePress(QMouseEvent *event, const ItemInfo &info, const QList<QGraphicsItem *> &)
{
    m_view->setCursor(Qt::ClosedHandCursor);
    m_dragMoved = false;
    m_clickPoint = event->pos();
    m_dragItemInfo = info;
    m_view->setOperationMode(MoveOperation);
    return true;
}

bool MoveManager::mouseMove(QMouseEvent *event, int, int)
{
    if (!m_dragMoved && event->buttons() & Qt::LeftButton) {
        if ((m_clickPoint - event->pos()).manhattanLength() < QApplication::startDragDistance()) {
            event->ignore();
            return true;
        }
        m_dragMoved = true;
    }
    if (m_dragItemInfo.isValid()) {
        if (event->pos().x() < m_scrollTrigger) {
            m_scrollOffset = -30;
            m_scrollTimer.start();
        } else if (m_view->viewport()->width() - event->pos().x() < m_scrollTrigger) {
            m_scrollOffset = 30;
            m_scrollTimer.start();
        } else if (m_scrollTimer.isActive()) {
            m_scrollTimer.stop();
        }
        event->accept();
        return false;
    }
    return true;
}

void MoveManager::mouseRelease(QMouseEvent *, GenTime pos)
{
    Q_UNUSED(pos);
    if (m_scrollTimer.isActive()) {
        m_scrollTimer.stop();
    }
    m_view->setCursor(Qt::OpenHandCursor);
    if (!m_dragMoved) {
        return;
    }
    AbstractClipItem *dragItem = m_view->dragItem();
    if (!dragItem || !m_dragItemInfo.isValid() || m_view->operationMode() == WaitingForConfirm) {
        // No move performed
        m_view->setOperationMode(None);
        return;
    }
    ItemInfo info = dragItem->info();
    if (dragItem->parentItem() == nullptr) {
        // we are moving one clip, easy
        if (dragItem->type() == AVWidget && (m_dragItemInfo.startPos != info.startPos || m_dragItemInfo.track != info.track)) {
            ClipItem *item = static_cast <ClipItem *>(dragItem);
            bool success = true;
            if (success) {
                QUndoCommand *moveCommand = new QUndoCommand();
                moveCommand->setText(i18n("Move clip"));
                if (item->hasVisibleVideo()) {
                    new RefreshMonitorCommand(m_view, QList<ItemInfo>() << info << m_dragItemInfo, false, true, moveCommand);
                }
                QList<ItemInfo> excluded;
                excluded << info;
                item->setItemLocked(true);
                //item->setEnabled(false);
                ItemInfo initialClip = m_dragItemInfo;
                if (m_view->sceneEditMode() == TimelineMode::InsertEdit) {
                    m_view->cutTimeline(info.startPos.frames(m_view->fps()), excluded, QList<ItemInfo>(), moveCommand, info.track);
                    new AddSpaceCommand(m_view, info, excluded, true, moveCommand, true);
                    bool isLastClip = m_view->isLastClip(info);
                    if (!isLastClip && info.track == m_dragItemInfo.track && info.startPos < m_dragItemInfo.startPos) {
                        //remove offset to allow finding correct clip to move
                        initialClip.startPos += m_dragItemInfo.cropDuration;
                        initialClip.endPos += m_dragItemInfo.cropDuration;
                    }
                } else if (m_view->sceneEditMode() == TimelineMode::OverwriteEdit) {
                    m_view->extractZone(QPoint(info.startPos.frames(m_view->fps()), info.endPos.frames(m_view->fps())), false, excluded, moveCommand, info.track);
                }
                bool isLocked = m_view->getTrackInfo(item->track()).isLocked;
                new MoveClipCommand(m_view, initialClip, info, true, true, moveCommand);

                // Also move automatic transitions (on lower track)
                Transition *startTransition = m_view->getTransitionItemAtStart(m_dragItemInfo.startPos, m_dragItemInfo.track);
                ItemInfo startTrInfo;
                ItemInfo newStartTrInfo;
                bool moveStartTrans = false;
                bool moveEndTrans = false;
                if (startTransition && startTransition->isAutomatic()) {
                    startTrInfo = startTransition->info();
                    newStartTrInfo = startTrInfo;
                    newStartTrInfo.track = info.track;
                    newStartTrInfo.startPos = info.startPos;
                    //newStartTrInfo.cropDuration = newStartTrInfo.endPos - info.startPos;
                    if (m_dragItemInfo.track == info.track /*&& !item->baseClip()->isTransparent()*/ && m_view->getClipItemAtEnd(newStartTrInfo.endPos, startTransition->transitionEndTrack())) {
                        // transition matches clip end on lower track, resize it
                        newStartTrInfo.cropDuration = newStartTrInfo.endPos - newStartTrInfo.startPos;
                    } else {
                        // move transition with clip
                        newStartTrInfo.endPos = newStartTrInfo.endPos + (newStartTrInfo.startPos - startTrInfo.startPos);
                    }
                    if (newStartTrInfo.startPos < newStartTrInfo.endPos) {
                        moveStartTrans = true;
                    }
                }
                if (startTransition == nullptr || startTransition->endPos() < m_dragItemInfo.endPos) {
                    // Check if there is a transition at clip end
                    Transition *tr = m_view->getTransitionItemAtEnd(m_dragItemInfo.endPos, m_dragItemInfo.track);
                    if (tr && tr->isAutomatic()) {
                        ItemInfo trInfo = tr->info();
                        ItemInfo newTrInfo = trInfo;
                        newTrInfo.track = info.track;
                        newTrInfo.endPos = dragItem->endPos();
                        //TODO
                        if (m_dragItemInfo.track == info.track /*&& !item->baseClip()->isTransparent()*/ && m_view->getClipItemAtStart(trInfo.startPos, tr->transitionEndTrack())) {
                            // transition start should stay the same, duration changes
                            newTrInfo.cropDuration = newTrInfo.endPos - newTrInfo.startPos;
                        } else {
                            // transition start should be moved
                            newTrInfo.startPos = newTrInfo.startPos + (newTrInfo.endPos - trInfo.endPos);
                        }
                        if (newTrInfo.startPos < newTrInfo.endPos) {
                            moveEndTrans = true;
                            if (moveStartTrans) {
                                // we have to move both transitions, remove the start one so that there is no collision
                                new AddTransitionCommand(m_view, startTrInfo, startTransition->transitionEndTrack(), startTransition->toXML(), true, true, moveCommand);
                            }
                            m_view->adjustTimelineTransitions(m_view->sceneEditMode(), tr, moveCommand);
                            if (tr->updateKeyframes(trInfo, newTrInfo)) {
                                QDomElement old = tr->toXML();
                                QDomElement xml = old.cloneNode().toElement();
                                m_transitionHandler->updateTransition(xml.attribute(QStringLiteral("tag")), xml.attribute(QStringLiteral("tag")), xml.attribute(QStringLiteral("transition_btrack")).toInt(),  xml.attribute(QStringLiteral("transition_atrack")).toInt(), newTrInfo.startPos, newTrInfo.endPos, xml);
                                new EditTransitionCommand(m_view, tr->track(), tr->startPos(), old, xml, false, moveCommand);
                            }
                            new MoveTransitionCommand(m_view, trInfo, newTrInfo, true, false, moveCommand);
                            if (moveStartTrans) {
                                // re-add transition in correct place
                                int transTrack = startTransition->transitionEndTrack();
                                if (m_dragItemInfo.track != info.track && !startTransition->forcedTrack()) {
                                    transTrack = m_view->getPreviousVideoTrack(info.track);
                                }
                                m_view->adjustTimelineTransitions(m_view->sceneEditMode(), startTransition, moveCommand);
                                if (startTransition->updateKeyframes(startTrInfo, newStartTrInfo)) {
                                    QDomElement old = startTransition->toXML();
                                    QDomElement xml = startTransition->toXML();
                                    m_transitionHandler->updateTransition(xml.attribute(QStringLiteral("tag")), xml.attribute(QStringLiteral("tag")), xml.attribute(QStringLiteral("transition_btrack")).toInt(),  xml.attribute(QStringLiteral("transition_atrack")).toInt(), newStartTrInfo.startPos, newStartTrInfo.endPos, xml);
                                }
                                new AddTransitionCommand(m_view, newStartTrInfo, transTrack, startTransition->toXML(), false, true, moveCommand);
                            }
                        }
                    }
                }

                if (moveStartTrans && !moveEndTrans) {
                    m_view->adjustTimelineTransitions(m_view->sceneEditMode(), startTransition, moveCommand);
                    if (startTransition->updateKeyframes(startTrInfo, newStartTrInfo)) {
                        QDomElement old = startTransition->toXML();
                        QDomElement xml = old.cloneNode().toElement();
                        m_transitionHandler->updateTransition(xml.attribute(QStringLiteral("tag")), xml.attribute(QStringLiteral("tag")), xml.attribute(QStringLiteral("transition_btrack")).toInt(),  xml.attribute(QStringLiteral("transition_atrack")).toInt(), newStartTrInfo.startPos, newStartTrInfo.endPos, xml);
                        new EditTransitionCommand(m_view, startTransition->track(), startTransition->startPos(), old, xml, false, moveCommand);
                    }
                    new MoveTransitionCommand(m_view, startTrInfo, newStartTrInfo, true, false, moveCommand);
                }
                // Also move automatic transitions (on upper track)
                Transition *tr = m_view->getTransitionItemAtStart(m_dragItemInfo.startPos, m_dragItemInfo.track + 1);
                if (m_dragItemInfo.track == info.track && tr && tr->isAutomatic() && tr->transitionEndTrack() == m_dragItemInfo.track) {
                    ItemInfo trInfo = tr->info();
                    ItemInfo newTrInfo = trInfo;
                    newTrInfo.startPos = dragItem->startPos();
                    newTrInfo.cropDuration = newTrInfo.endPos - dragItem->startPos();
                    ClipItem *upperClip = m_view->getClipItemAtStart(m_dragItemInfo.startPos, m_dragItemInfo.track - 1);
                    if (!upperClip /*|| !upperClip->baseClip()->isTransparent()*/) {
                        if (!m_view->getClipItemAtEnd(newTrInfo.endPos, tr->track())) {
                            // transition end should be adjusted to clip on upper track
                            newTrInfo.endPos = newTrInfo.endPos + (newTrInfo.startPos - trInfo.startPos);
                        }
                        if (newTrInfo.startPos < newTrInfo.endPos) {
                            m_view->adjustTimelineTransitions(m_view->sceneEditMode(), tr, moveCommand);
                            QDomElement old = tr->toXML();
                            if (tr->updateKeyframes(trInfo, newTrInfo)) {
                                QDomElement xml = old.cloneNode().toElement();
                                m_transitionHandler->updateTransition(xml.attribute(QStringLiteral("tag")), xml.attribute(QStringLiteral("tag")), xml.attribute(QStringLiteral("transition_btrack")).toInt(),  xml.attribute(QStringLiteral("transition_atrack")).toInt(), newTrInfo.startPos, newTrInfo.endPos, xml);
                                new EditTransitionCommand(m_view, tr->track(), tr->startPos(), old, xml, false, moveCommand);
                            }
                            new MoveTransitionCommand(m_view, trInfo, newTrInfo, true, false, moveCommand);
                        }
                    }
                }
                if (m_dragItemInfo.track == info.track && (tr == nullptr || tr->endPos() < m_dragItemInfo.endPos)) {
                    // Check if there is a transition at clip end
                    tr = m_view->getTransitionItemAtEnd(m_dragItemInfo.endPos, m_dragItemInfo.track + 1);
                    if (tr && tr->isAutomatic() && tr->transitionEndTrack() == m_dragItemInfo.track) {
                        ItemInfo trInfo = tr->info();
                        ItemInfo newTrInfo = trInfo;
                        newTrInfo.endPos = dragItem->endPos();
                        ClipItem *upperClip = m_view->getClipItemAtStart(m_dragItemInfo.startPos, m_dragItemInfo.track + 1);
                        if (!upperClip /*|| !upperClip->baseClip()->isTransparent()*/) {
                            if (!m_view->getClipItemAtStart(trInfo.startPos, tr->track())) {
                                // transition moved, update start
                                newTrInfo.startPos = dragItem->endPos() - newTrInfo.cropDuration;
                            } else {
                                // transition start should be resized
                                newTrInfo.cropDuration = dragItem->endPos() - newTrInfo.startPos;
                            }
                            if (newTrInfo.startPos < newTrInfo.endPos) {
                                m_view->adjustTimelineTransitions(m_view->sceneEditMode(), tr, moveCommand);
                                if (tr->updateKeyframes(trInfo, newTrInfo)) {
                                    QDomElement old = tr->toXML();
                                    QDomElement xml = old.cloneNode().toElement();
                                    m_transitionHandler->updateTransition(xml.attribute(QStringLiteral("tag")), xml.attribute(QStringLiteral("tag")), xml.attribute(QStringLiteral("transition_btrack")).toInt(),  xml.attribute(QStringLiteral("transition_atrack")).toInt(), newTrInfo.startPos, newTrInfo.endPos, xml);
                                    new EditTransitionCommand(m_view, tr->track(), tr->startPos(), old, xml, false, moveCommand);
                                }
                                new MoveTransitionCommand(m_view, trInfo, newTrInfo, true, false, moveCommand);
                            }
                        }
                    }
                }
                m_view->updateTrackDuration(info.track, moveCommand);
                if (m_dragItemInfo.track != info.track) {
                    m_view->updateTrackDuration(m_dragItemInfo.track, moveCommand);
                }
                bool refresh = item->hasVisibleVideo();
                if (refresh) {
                    new RefreshMonitorCommand(m_view, QList<ItemInfo>() << info << m_dragItemInfo, false, false, moveCommand);
                }
                m_commandStack->push(moveCommand);
                if (refresh) {
                    m_view->monitorRefresh(QList<ItemInfo>() << info << m_dragItemInfo, true);
                }
                item = m_view->getClipItemAtStart(info.startPos, info.track, info.endPos);
                if (item) {
                    item->setItemLocked(isLocked);
                    item->setSelected(true);
                }
            } else {
                // undo last move and emit error message
                bool snap = KdenliveSettings::snaptopoints();
                KdenliveSettings::setSnaptopoints(false);
                item->setPos((int) m_dragItemInfo.startPos.frames(m_view->fps()), m_view->getPositionFromTrack(m_dragItemInfo.track) + 1);
                KdenliveSettings::setSnaptopoints(snap);
                m_view->displayMessage(i18n("Cannot move clip to position %1", m_view->timecode().getTimecodeFromFrames(info.startPos.frames(m_view->fps()))), ErrorMessage);
            }
        } else if (dragItem->type() == TransitionWidget && (m_dragItemInfo.startPos != info.startPos || m_dragItemInfo.track != info.track)) {
            Transition *transition = static_cast <Transition *>(dragItem);
            transition->updateTransitionEndTrack(m_view->getPreviousVideoTrack(dragItem->track()));
            if (!m_transitionHandler->moveTransition(transition->transitionTag(), m_dragItemInfo.track, dragItem->track(), transition->transitionEndTrack(), m_dragItemInfo.startPos, m_dragItemInfo.endPos, info.startPos, info.endPos)) {
                // Moving transition failed, revert to previous position
                m_view->displayMessage(i18n("Cannot move transition"), ErrorMessage);
                transition->setPos((int) m_dragItemInfo.startPos.frames(m_view->fps()), m_view->getPositionFromTrack(m_dragItemInfo.track) + 1);
            } else {
                QUndoCommand *moveCommand = new QUndoCommand();
                moveCommand->setText(i18n("Move transition"));
                m_view->adjustTimelineTransitions(m_view->sceneEditMode(), transition, moveCommand);
                new MoveTransitionCommand(m_view, m_dragItemInfo, info, false, true, moveCommand);
                m_view->updateTrackDuration(info.track, moveCommand);
                if (m_dragItemInfo.track != info.track) {
                    m_view->updateTrackDuration(m_dragItemInfo.track, moveCommand);
                }
                m_commandStack->push(moveCommand);
                m_view->monitorRefresh(QList<ItemInfo>() << info << m_dragItemInfo, true);
                m_view->updateTransitionWidget(transition, info);
            }
        }
    } else {
        // Moving several clips. We need to delete them and readd them to new position,
        // or they might overlap each other during the move
        AbstractGroupItem *group = m_view->selectionGroup();
        ItemInfo cutInfo;
        cutInfo.startPos = GenTime(dragItem->scenePos().x(), m_view->fps());
        cutInfo.cropDuration = group->duration();
        cutInfo.endPos = cutInfo.startPos + cutInfo.cropDuration;

        QList<QGraphicsItem *> items = group->childItems();
        QList<ItemInfo> clipsToMove;
        QList<ItemInfo> transitionsToMove;

        GenTime timeOffset = GenTime(dragItem->scenePos().x(), m_view->fps()) - m_dragItemInfo.startPos;
        const int trackOffset = m_view->getTrackFromPos(dragItem->scenePos().y()) - m_dragItemInfo.track;

        QUndoCommand *moveGroup = new QUndoCommand();
        moveGroup->setText(i18n("Move group"));

        // Expand groups
        int max = items.count();
        for (int i = 0; i < max; ++i) {
            if (items.at(i)->type() == GroupWidget) {
                items += items.at(i)->childItems();
            }
        }
        QList<ItemInfo> updatedClipsToMove;
        QList<ItemInfo> updatedTransitionsToMove;
        for (int i = 0; i < items.count(); ++i) {
            if (items.at(i)->type() != AVWidget && items.at(i)->type() != TransitionWidget) {
                continue;
            }
            AbstractClipItem *item = static_cast <AbstractClipItem *>(items.at(i));
            if (item->type() == AVWidget) {
                clipsToMove.append(item->info());
                updatedClipsToMove << item->info();
            } else {
                transitionsToMove.append(item->info());
                updatedTransitionsToMove << item->info();
            }
        }
        if (m_view->sceneEditMode() == TimelineMode::InsertEdit) {
            m_view->cutTimeline(cutInfo.startPos.frames(m_view->fps()), clipsToMove, transitionsToMove, moveGroup, -1);
            new AddSpaceCommand(m_view, cutInfo, clipsToMove, true, moveGroup, false);
            bool isLastClip = m_view->isLastClip(cutInfo);
            if (!isLastClip && cutInfo.track == m_dragItemInfo.track && cutInfo.startPos < m_dragItemInfo.startPos) {
                //TODO: remove offset to allow finding correct clip to move
                //initialClip.startPos += m_dragItemInfo.cropDuration;
                //initialClip.endPos += m_dragItemInfo.cropDuration;
            }
        } else if (m_view->sceneEditMode() == TimelineMode::OverwriteEdit) {
            m_view->extractZone(QPoint(cutInfo.startPos.frames(m_view->fps()), cutInfo.endPos.frames(m_view->fps())), false, updatedClipsToMove, moveGroup, -1);
        }
        new MoveGroupCommand(m_view, clipsToMove, transitionsToMove, timeOffset, trackOffset, true, true, moveGroup);
        m_commandStack->push(moveGroup);
    }
    m_dragItemInfo = ItemInfo();
    m_view->setOperationMode(None);
}

void MoveManager::slotCheckMouseScrolling()
{
    if (m_scrollOffset == 0) {
        m_scrollTimer.stop();
        return;
    }
    m_view->horizontalScrollBar()->setValue(m_view->horizontalScrollBar()->value() + m_scrollOffset);
    m_scrollTimer.start();
}
