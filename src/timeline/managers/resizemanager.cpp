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

#include "resizemanager.h"
#include "kdenlivesettings.h"
#include "timeline/customtrackview.h"
#include "timeline/clipitem.h"
#include "timeline/transition.h"
#include "timeline/timelinecommands.h"
#include "timeline/abstractgroupitem.h"

#include <KLocalizedString>

ResizeManager::ResizeManager(CustomTrackView *view, DocUndoStack *commandStack) : AbstractToolManager(ResizeType, view, commandStack)
{
}

bool ResizeManager::mousePress(QMouseEvent *event, const ItemInfo &info, const QList<QGraphicsItem *> &)
{
    m_dragItemInfo = info;
    m_controlModifier = event->modifiers();
    AbstractClipItem *dragItem = m_view->dragItem();
    AbstractGroupItem *selectionGroup = m_view->selectionGroup();
    if (selectionGroup) {
        m_view->resetSelectionGroup(false);
        dragItem->setSelected(true);
    }
    m_view->setOperationMode(m_view->prepareMode());
    m_startInfos.clear();
    if (dragItem->type() == AVWidget && dragItem->parentItem()) {
        // Store start infos
        AbstractGroupItem *parent = static_cast <AbstractGroupItem *>(dragItem->parentItem());
        if (parent) {
            QList<QGraphicsItem *> items = parent->childItems();
            for (int i = 0; i < items.count(); ++i) {
                if (items.at(i)->type() == AVWidget) {
                    AbstractClipItem *item = static_cast<AbstractClipItem *>(items.at(i));
                    m_startInfos << item->info();
                }
            }
        }
    }
    return true;
}

bool ResizeManager::mouseMove(QMouseEvent *event, int pos, int)
{
    double snappedPos = m_view->getSnapPointForPos(pos);
    if (event->buttons() & Qt::LeftButton) {
        AbstractClipItem *dragItem = m_view->dragItem();
        if (!(m_controlModifier & Qt::ControlModifier) && dragItem->type() == AVWidget && dragItem->parentItem()) {
            AbstractGroupItem *parent = static_cast <AbstractGroupItem *>(dragItem->parentItem());
            if (parent) {
                if (m_view->operationMode() == ResizeStart) {
                    parent->resizeStart(snappedPos);
                } else {
                    parent->resizeEnd(snappedPos);
                }
            }
        } else {
            if (m_view->operationMode() == ResizeStart) {
                dragItem->resizeStart(snappedPos, true, false);
            } else {
                dragItem->resizeEnd(snappedPos, false);
            }
        }
        QString crop = m_view->timecode().getDisplayTimecode(dragItem->cropStart(), KdenliveSettings::frametimecode());
        QString duration = m_view->timecode().getDisplayTimecode(dragItem->cropDuration(), KdenliveSettings::frametimecode());
        QString offset = m_view->timecode().getDisplayTimecode(dragItem->cropStart() - m_dragItemInfo.cropStart, KdenliveSettings::frametimecode());
        m_view->displayMessage(i18n("Crop from start: %1 Duration: %2 Offset: %3", crop, duration, offset), InformationMessage);
        event->accept();
    }
    return true;
}

void ResizeManager::mouseRelease(QMouseEvent *, GenTime pos)
{
    Q_UNUSED(pos);
    AbstractClipItem *dragItem = m_view->dragItem();
    if (dragItem) {
        if (m_view->operationMode() == ResizeStart) {
            if (dragItem->startPos() != m_dragItemInfo.startPos) {
                // resize start
                if (/*!(m_controlModifier & Qt::ControlModifier) &&*/ dragItem->type() == AVWidget && dragItem->parentItem()) {
                    AbstractGroupItem *parent = static_cast <AbstractGroupItem *>(dragItem->parentItem());
                    if (parent) {
                        QUndoCommand *resizeCommand = new QUndoCommand();
                        resizeCommand->setText(i18n("Resize group"));
                        QList<QGraphicsItem *> items = parent->childItems();
                        GenTime min = parent->startPos();
                        GenTime max = min;
                        int itemcount = 0;
                        for (int i = 0; i < items.count(); ++i) {
                            AbstractClipItem *item = static_cast<AbstractClipItem *>(items.at(i));
                            if (item && item->type() == AVWidget) {
                                ItemInfo info = m_startInfos.at(itemcount);
                                m_view->prepareResizeClipStart(item, info, item->startPos().frames(m_view->fps()), false, resizeCommand);
                                ClipItem *cp = qobject_cast<ClipItem *>(item);
                                if (cp && cp->hasVisibleVideo()) {
                                    min = qMin(min, item->startPos());
                                    max = qMax(max, item->startPos());
                                }
                                ++itemcount;
                            }
                        }
                        m_commandStack->push(resizeCommand);
                        if (min < max) {
                            ItemInfo nfo;
                            nfo.startPos = min;
                            nfo.endPos = max;
                            m_view->monitorRefresh(nfo, true);
                        }
                    } else {
                        qCDebug(KDENLIVE_LOG) << " * * * * *PARENT GRP NOT FOUND";
                    }
                } else {
                    m_view->prepareResizeClipStart(dragItem, m_dragItemInfo, dragItem->startPos().frames(m_view->fps()));
                    ItemInfo range;
                    range.track = m_dragItemInfo.track;
                    if (dragItem->startPos() < m_dragItemInfo.startPos) {
                        range.startPos = dragItem->startPos();
                        range.endPos = m_dragItemInfo.startPos;
                    } else {
                        range.endPos = dragItem->startPos();
                        range.startPos = m_dragItemInfo.startPos;
                    }
                    if (dragItem->type() == AVWidget) {
                        ClipItem *cp = qobject_cast<ClipItem *>(dragItem);
                        cp->slotUpdateRange();
                        if (cp->hasVisibleVideo()) {
                            m_view->monitorRefresh(range, true);
                        }
                        m_view->clearSelection();
                        delete dragItem;
                        m_view->reloadTrack(range, true);
                        dragItem = m_view->getClipItemAtEnd(m_dragItemInfo.endPos, m_dragItemInfo.track);
                        if (dragItem) {
                            m_view->slotSelectItem(dragItem);
                        }
                    } else {
                        // Resized transition
                        m_view->monitorRefresh(QList<ItemInfo>() << m_dragItemInfo << dragItem->info(), true);
                    }
                    if (!dragItem) {
                        qCDebug(KDENLIVE_LOG) << " * * ** SOMETHING WRONG HERE: " << m_dragItemInfo.endPos.frames(m_view->fps());
                    }
                }
            }
        } else if (m_view->operationMode() == ResizeEnd) {
            dragItem->setProperty("resizingEnd", QVariant());
            if (dragItem->endPos() != m_dragItemInfo.endPos) {
                if (!(m_controlModifier & Qt::ControlModifier)  && dragItem->type() == AVWidget && dragItem->parentItem()) {
                    AbstractGroupItem *parent = static_cast <AbstractGroupItem *>(dragItem->parentItem());
                    if (parent) {
                        QUndoCommand *resizeCommand = new QUndoCommand();
                        resizeCommand->setText(i18n("Resize group"));
                        QList<QGraphicsItem *> items = parent->childItems();
                        GenTime min = parent->startPos() + parent->duration();
                        GenTime max = min;
                        int itemcount = 0;
                        for (int i = 0; i < items.count(); ++i) {
                            AbstractClipItem *item = static_cast<AbstractClipItem *>(items.at(i));
                            if (item && item->type() == AVWidget) {
                                ItemInfo info = m_startInfos.at(itemcount);
                                m_view->prepareResizeClipEnd(item, info, item->endPos().frames(m_view->fps()), false, resizeCommand);
                                ClipItem *cp = qobject_cast<ClipItem *>(item);
                                if (cp && cp->hasVisibleVideo()) {
                                    min = qMin(min, item->endPos());
                                    max = qMax(max, item->endPos());
                                }
                                ++itemcount;
                            }
                        }
                        m_view->updateTrackDuration(-1, resizeCommand);
                        m_commandStack->push(resizeCommand);
                        if (min < max) {
                            ItemInfo nfo;
                            nfo.startPos = min;
                            nfo.endPos = max;
                            m_view->monitorRefresh(nfo, true);
                        }
                    }
                } else {
                    m_view->prepareResizeClipEnd(dragItem, m_dragItemInfo, dragItem->endPos().frames(m_view->fps()), false);
                    ItemInfo range;
                    if (dragItem->endPos() < m_dragItemInfo.endPos) {
                        range.startPos = dragItem->endPos();
                        range.endPos = m_dragItemInfo.endPos;
                    } else {
                        range.endPos = dragItem->endPos();
                        range.startPos = m_dragItemInfo.endPos;
                    }
                    if (dragItem->type() == AVWidget) {
                        ClipItem *cp = qobject_cast<ClipItem *>(dragItem);
                        cp->slotUpdateRange();
                        if (cp->hasVisibleVideo()) {
                            m_view->monitorRefresh(range, true);
                        }
                    } else {
                        m_view->monitorRefresh(QList<ItemInfo>() << m_dragItemInfo << dragItem->info(), true);
                    }
                }
            }
        }
    }
    m_startInfos.clear();
    m_view->setCursor(Qt::OpenHandCursor);
    m_view->setOperationMode(None);
}

