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

#include "selectmanager.h"
#include "timeline/customtrackview.h"
#include "timeline/clipitem.h"
#include "timeline/abstractgroupitem.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "timeline/timelinecommands.h"

#include <KLocalizedString>
#include <QGraphicsItem>

#include <QMouseEvent>

#include "klocalizedstring.h"

SelectManager::SelectManager(CustomTrackView *view, DocUndoStack *commandStack) : AbstractToolManager(SelectType, view, commandStack)
    , m_dragMoved(false)
{
}

bool SelectManager::mousePress(QMouseEvent *event, const ItemInfo &info, const QList<QGraphicsItem *> &)
{
    Q_UNUSED(info);
    m_view->activateMonitor();
    m_modifiers = event->modifiers();
    m_dragMoved = false;
    m_clickPoint = event->pos();
    if (m_modifiers & Qt::ShiftModifier) {
        m_view->createRectangleSelection(event->modifiers());
        return true;
    }
    // No item under click
    AbstractClipItem *dragItem = m_view->dragItem();
    if (dragItem == nullptr) {
        m_view->clearSelection(true);
        if (event->button() == Qt::LeftButton) {
            m_view->setOperationMode(Seek);
            m_view->seekCursorPos((int) m_view->mapToScene(event->pos()).x());
            event->setAccepted(true);
            return true;
        }
    } else {
        m_view->setOperationMode(m_view->prepareMode());
    }
    switch (m_view->operationMode()) {
    case FadeIn:
    case FadeOut:
        //m_view->setCursor(Qt::PointingHandCursor);
        break;
    case KeyFrame:
        //m_view->setCursor(Qt::PointingHandCursor);
        dragItem->prepareKeyframeMove();
        break;
    default:
        //m_view->setCursor(Qt::ArrowCursor);
        break;
    }
    return false;
}

bool SelectManager::mouseMove(QMouseEvent *event, int, int)
{
    OperationType mode = m_view->operationMode();
    if (mode == Seek) {
        return false;
    }
    if (!m_dragMoved && event->buttons() & Qt::LeftButton) {
        if ((m_clickPoint - event->pos()).manhattanLength() < QApplication::startDragDistance()) {
            event->ignore();
            return true;
        }
        m_dragMoved = true;
    }
    if (mode == FadeIn) {
        AbstractClipItem *dragItem = m_view->dragItem();
        double mappedXPos = m_view->mapToScene(event->pos()).x();
        static_cast<ClipItem *>(dragItem)->setFadeIn(static_cast<int>(mappedXPos - dragItem->startPos().frames(m_view->fps())));
        event->accept();
        return true;
    } else if (mode == FadeOut) {
        AbstractClipItem *dragItem = m_view->dragItem();
        double mappedXPos = m_view->mapToScene(event->pos()).x();
        static_cast<ClipItem *>(dragItem)->setFadeOut(static_cast<int>(dragItem->endPos().frames(m_view->fps()) - mappedXPos));
        event->accept();
        return true;
    } else if (mode == KeyFrame) {
        AbstractClipItem *dragItem = m_view->dragItem();
        double mappedXPos = m_view->mapToScene(event->pos()).x();
        GenTime keyFramePos = GenTime(mappedXPos, m_view->fps()) - dragItem->startPos();
        double value = dragItem->mapFromScene(m_view->mapToScene(event->pos()).toPoint()).y();
        dragItem->updateKeyFramePos(keyFramePos.frames(m_view->fps()), value);
        QString position = m_view->timecode().getDisplayTimecodeFromFrames(dragItem->selectedKeyFramePos(), KdenliveSettings::frametimecode());
        m_view->displayMessage(position + QStringLiteral(" : ") + QString::number(dragItem->editedKeyFrameValue()), InformationMessage);
        event->accept();
        return true;
    }
    return false;
}

void SelectManager::mouseRelease(QMouseEvent *event, GenTime pos)
{
    Q_UNUSED(pos);
    AbstractClipItem *dragItem = m_view->dragItem();
    OperationType moveType = m_view->operationMode();
    if (moveType == RubberSelection) {
        //setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
        if (event->modifiers() != Qt::ControlModifier) {
            if (dragItem) {
                dragItem->setMainSelectedClip(false);
            }
            dragItem = nullptr;
        }
        event->accept();
        m_view->resetSelectionGroup();
        m_view->groupSelectedItems();
        if (m_view->selectionGroup() == nullptr && dragItem) {
            // Only 1 item selected
            if (dragItem->type() == AVWidget) {
                dragItem->setMainSelectedClip(true);
                m_view->clipItemSelected(static_cast<ClipItem *>(dragItem), false);
            }
        }
        return;
    }
    if (!m_dragMoved) {
        return;
    }
    if (moveType == Seek || moveType == WaitingForConfirm || moveType == None || (!m_dragMoved && moveType == MoveOperation)) {
        if (!(m_modifiers & Qt::ControlModifier)) {
            if (dragItem) {
                dragItem->setMainSelectedClip(false);
            }
            dragItem = nullptr;
        }
        m_view->resetSelectionGroup();
        m_view->groupSelectedItems();
        AbstractGroupItem *selectionGroup = m_view->selectionGroup();
        if (selectionGroup == nullptr && dragItem) {
            // Only 1 item selected
            if (dragItem->type() == AVWidget) {
                dragItem->setMainSelectedClip(true);
                m_view->clipItemSelected(static_cast<ClipItem *>(dragItem), false);
            }
        }
    }
    if (moveType == FadeIn && dragItem) {
        ClipItem *item = static_cast <ClipItem *>(dragItem);
        // find existing video fade, if none then audio fade

        int fadeIndex = item->hasEffect(QString(), QStringLiteral("fade_from_black"));
        int fadeIndex2 = item->hasEffect(QStringLiteral("volume"), QStringLiteral("fadein"));
        if (fadeIndex >= 0 && fadeIndex2 >= 0) {
            // We have 2 fadin effects, use currently selected or first one
            int current = item->selectedEffectIndex();
            if (fadeIndex != current) {
                if (fadeIndex2 == current) {
                    fadeIndex = current;
                } else {
                    fadeIndex = qMin(fadeIndex, fadeIndex2);
                }
            }
        } else {
            fadeIndex = qMax(fadeIndex, fadeIndex2);
        }
        // resize fade in effect
        if (fadeIndex >= 0) {
            QDomElement oldeffect = item->effectAtIndex(fadeIndex);
            int end = item->fadeIn();
            if (end == 0) {
                m_view->slotDeleteEffect(item, -1, oldeffect, false);
            } else {
                int start = item->cropStart().frames(m_view->fps());
                end += start;
                QDomElement effect = oldeffect.cloneNode().toElement();
                EffectsList::setParameter(oldeffect, QStringLiteral("in"), QString::number(start));
                EffectsList::setParameter(oldeffect, QStringLiteral("out"), QString::number(end));
                m_view->slotUpdateClipEffect(item, -1, effect, oldeffect, fadeIndex);
                m_view->clipItemSelected(item);
            }
            // new fade in
        } else if (item->fadeIn() != 0) {
            QDomElement effect;
            if (item->clipState() == PlaylistState::VideoOnly || (item->clipType() != Audio && item->clipState() != PlaylistState::AudioOnly && item->clipType() != Playlist)) {
                effect = MainWindow::videoEffects.getEffectByTag(QString(), QStringLiteral("fade_from_black")).cloneNode().toElement();
            } else {
                effect = MainWindow::audioEffects.getEffectByTag(QStringLiteral("volume"), QStringLiteral("fadein")).cloneNode().toElement();
            }
            EffectsList::setParameter(effect, QStringLiteral("out"), QString::number(item->fadeIn()));
            m_view->slotAddEffect(effect, dragItem->startPos(), dragItem->track());
        }

    } else if (moveType == FadeOut && dragItem) {
        ClipItem *item = static_cast <ClipItem *>(dragItem);
        // find existing video fade, if none then audio fade

        int fadeIndex = item->hasEffect(QString(), QStringLiteral("fade_to_black"));
        int fadeIndex2 = item->hasEffect(QStringLiteral("volume"), QStringLiteral("fadeout"));
        if (fadeIndex >= 0 && fadeIndex2 >= 0) {
            // We have 2 fadin effects, use currently selected or first one
            int current = item->selectedEffectIndex();
            if (fadeIndex != current) {
                if (fadeIndex2 == current) {
                    fadeIndex = current;
                } else {
                    fadeIndex = qMin(fadeIndex, fadeIndex2);
                }
            }
        } else {
            fadeIndex = qMax(fadeIndex, fadeIndex2);
        }
        // resize fade out effect
        if (fadeIndex >= 0) {
            QDomElement oldeffect = item->effectAtIndex(fadeIndex);
            int start = item->fadeOut();
            if (start == 0) {
                m_view->slotDeleteEffect(item, -1, oldeffect, false);
            } else {
                int end = (item->cropDuration() + item->cropStart()).frames(m_view->fps());
                start = end - start;
                QDomElement effect = oldeffect.cloneNode().toElement();
                EffectsList::setParameter(oldeffect, QStringLiteral("in"), QString::number(start));
                EffectsList::setParameter(oldeffect, QStringLiteral("out"), QString::number(end));
                m_view->slotUpdateClipEffect(item, -1, effect, oldeffect, fadeIndex);
                m_view->clipItemSelected(item);
            }
            // new fade out
        } else if (item->fadeOut() != 0) {
            QDomElement effect;
            if (item->clipState() == PlaylistState::VideoOnly || (item->clipType() != Audio && item->clipState() != PlaylistState::AudioOnly && item->clipType() != Playlist)) {
                effect = MainWindow::videoEffects.getEffectByTag(QString(), QStringLiteral("fade_to_black")).cloneNode().toElement();
            } else {
                effect = MainWindow::audioEffects.getEffectByTag(QStringLiteral("volume"), QStringLiteral("fadeout")).cloneNode().toElement();
            }
            int end = (item->cropDuration() + item->cropStart()).frames(m_view->fps());
            int start = end - item->fadeOut();
            EffectsList::setParameter(effect, QStringLiteral("in"), QString::number(start));
            EffectsList::setParameter(effect, QStringLiteral("out"), QString::number(end));
            m_view->slotAddEffect(effect, dragItem->startPos(), dragItem->track());
        }

    } else if (moveType == KeyFrame && dragItem && m_dragMoved) {
        // update the MLT effect
        ClipItem *item = static_cast <ClipItem *>(dragItem);
        QDomElement oldEffect = item->selectedEffect().cloneNode().toElement();

        // check if we want to remove keyframe
        double val = m_view->mapToScene(event->pos()).toPoint().y();
        QRectF br = item->sceneBoundingRect();
        double maxh = 100.0 / br.height();
        val = (br.bottom() - val) * maxh;
        int start = item->cropStart().frames(m_view->fps());
        int end = (item->cropStart() + item->cropDuration()).frames(m_view->fps()) - 1;

        if ((val < -50 || val > 150) && item->selectedKeyFramePos() != start && item->selectedKeyFramePos() != end && item->keyframesCount() > 1) {
            //delete keyframe
            item->removeKeyframe(item->getEffectAtIndex(item->selectedEffectIndex()), item->selectedKeyFramePos());
        } else {
            item->movedKeyframe(item->getEffectAtIndex(item->selectedEffectIndex()), item->selectedKeyFramePos(), item->originalKeyFramePos());
        }

        QDomElement newEffect = item->selectedEffect().cloneNode().toElement();

        EditEffectCommand *command = new EditEffectCommand(m_view, item->track(), item->startPos(), oldEffect, newEffect, item->selectedEffectIndex(), false, false, true);
        m_commandStack->push(command);
        m_view->updateEffect(item->track(), item->startPos(), item->selectedEffect());
        m_view->clipItemSelected(item);
    } else if (moveType == KeyFrame && dragItem) {
        m_view->setActiveKeyframe(dragItem->selectedKeyFramePos());
    }
}

void SelectManager::initTool(double)
{
    m_view->unsetCursor();
}

void SelectManager::checkOperation(QGraphicsItem *item, CustomTrackView *view, QMouseEvent *event, AbstractGroupItem *group, OperationType &operationMode, OperationType moveOperation)
{
    OperationType currentMode = operationMode;
    if (item && event->buttons() == Qt::NoButton && operationMode != ZoomTimeline) {
        AbstractClipItem *clip = static_cast <AbstractClipItem *>(item);
        QString tooltipMessage;

        if (group && clip->parentItem() == group) {
            // all other modes break the selection, so the user probably wants to move it
            operationMode = MoveOperation;
        } else {
            if (clip->rect().width() * view->transform().m11() < 15) {
                // If the item is very small, only allow move
                operationMode = MoveOperation;
            } else {
                operationMode = clip->operationMode(clip->mapFromScene(view->mapToScene(event->pos())), event->modifiers());
            }
        }

        if (operationMode == moveOperation) {
            view->graphicsViewMouseEvent(event);
            if (currentMode != operationMode) {
                view->setToolTip(tooltipMessage);
            }
            return;
        }
        ClipItem *ci = nullptr;
        if (item->type() == AVWidget) {
            ci = static_cast <ClipItem *>(item);
        }
        QString message;
        if (operationMode == MoveOperation) {
            view->setCursor(Qt::OpenHandCursor);
            if (ci) {
                message = ci->clipName() + i18n(":");
                message.append(i18n(" Position:") + view->getDisplayTimecode(ci->info().startPos));
                message.append(i18n(" Duration:") + view->getDisplayTimecode(ci->cropDuration()));
                if (clip->parentItem() && clip->parentItem()->type() == GroupWidget) {
                    AbstractGroupItem *parent = static_cast <AbstractGroupItem *>(clip->parentItem());
                    if (clip->parentItem() == group) {
                        message.append(i18n(" Selection duration:"));
                    } else {
                        message.append(i18n(" Group duration:"));
                    }
                    message.append(view->getDisplayTimecode(parent->duration()));
                    if (parent->parentItem() && parent->parentItem()->type() == GroupWidget) {
                        AbstractGroupItem *parent2 = static_cast <AbstractGroupItem *>(parent->parentItem());
                        message.append(i18n(" Selection duration:") + view->getDisplayTimecode(parent2->duration()));
                    }
                }
            }
        } else if (operationMode == ResizeStart) {
            view->setCursor(QCursor(Qt::SizeHorCursor));
            if (ci) {
                message = i18n("Crop from start: ") + view->getDisplayTimecode(ci->cropStart());
            }
            if (item->type() == AVWidget && item->parentItem() && item->parentItem() != group) {
                message.append(i18n("Use Ctrl to resize only current item, otherwise all items in this group will be resized at once."));
            }
        } else if (operationMode == ResizeEnd) {
            view->setCursor(QCursor(Qt::SizeHorCursor));
            if (ci) {
                message = i18n("Duration: ") + view->getDisplayTimecode(ci->cropDuration());
            }
            if (item->type() == AVWidget && item->parentItem() && item->parentItem() != group) {
                message.append(i18n("Use Ctrl to resize only current item, otherwise all items in this group will be resized at once."));
            }
        } else if (operationMode == FadeIn || operationMode == FadeOut) {
            view->setCursor(Qt::PointingHandCursor);
            if (ci && operationMode == FadeIn && ci->fadeIn()) {
                message = i18n("Fade in duration: ");
                message.append(view->getDisplayTimecodeFromFrames(ci->fadeIn()));
            } else if (ci && operationMode == FadeOut && ci->fadeOut()) {
                message = i18n("Fade out duration: ");
                message.append(view->getDisplayTimecodeFromFrames(ci->fadeOut()));
            } else {
                message = i18n("Drag to add or resize a fade effect.");
                tooltipMessage = message;
            }
        } else if (operationMode == TransitionStart || operationMode == TransitionEnd) {
            view->setCursor(Qt::PointingHandCursor);
            message = i18n("Click to add a transition.");
            tooltipMessage = message;
        } else if (operationMode == KeyFrame) {
            view->setCursor(Qt::PointingHandCursor);
            QMetaObject::invokeMethod(view, "displayMessage", Qt::QueuedConnection, Q_ARG(QString, i18n("Move keyframe above or below clip to remove it, double click to add a new one.")), Q_ARG(MessageType, InformationMessage));
        }
        if (currentMode != operationMode) {
            view->setToolTip(tooltipMessage);
        }
        if (!message.isEmpty()) {
            QMetaObject::invokeMethod(view, "displayMessage", Qt::QueuedConnection, Q_ARG(QString, message), Q_ARG(MessageType, InformationMessage));
        }
    } else if (event->buttons() == Qt::NoButton && (moveOperation == None || moveOperation == WaitingForConfirm)) {
        operationMode = None;
        if (currentMode != operationMode) {
            view->setToolTip(QString());
        }
        view->setCursor(Qt::ArrowCursor);
    }
}

