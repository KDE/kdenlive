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

#include "trimmanager.h"
#include "kdenlivesettings.h"
#include "timeline/customtrackview.h"
#include "timeline/clipitem.h"

#include <KLocalizedString>
#include <QtConcurrent>
#include <QStandardPaths>
#include <QProcess>



TrimManager::TrimManager(CustomTrackView *view, DocUndoStack *commandStack) : AbstractToolManager(view, commandStack)
    , m_firstClip(NULL)
    , m_secondClip(NULL)
{
}

bool TrimManager::mousePress(ItemInfo info, Qt::KeyboardModifiers, QList<QGraphicsItem *>)
{
    return enterTrimMode(info, m_view->prepareMode() == ResizeStart);
}

void TrimManager::mouseMove(int pos)
{
    if (!m_firstInfo.isValid() || !m_secondInfo.isValid())
        return;
    if (pos < m_firstClip->endPos().frames(m_view->fps())) {
        m_firstClip->resizeEnd(pos, false);
        m_secondClip->resizeStart(pos, true, false);
    } else {
        m_secondClip->resizeStart(pos, true, false);
        m_firstClip->resizeEnd(pos, false);
    }
    m_view->seekCursorPos(pos);
}

void TrimManager::mouseRelease(GenTime)
{
    endRoll();
}

bool TrimManager::enterTrimMode(ItemInfo info, bool trimStart)
{
    if (trimStart) {
        m_firstClip = m_view->getClipItemAtEnd(info.startPos, info.track);
        m_secondClip = m_view->getClipItemAtStart(info.startPos, info.track);
    } else {
        m_firstClip = m_view->getClipItemAtEnd(info.endPos, info.track);
        m_secondClip = m_view->getClipItemAtStart(info.endPos, info.track);
    }
    if (!m_firstClip || !m_secondClip) {
        m_view->setOperationMode(None);
        m_firstInfo = ItemInfo();
        m_secondInfo = ItemInfo();
        return false;
    }
    AbstractClipItem *dragItem = m_view->dragItem();
    AbstractGroupItem *selectionGroup = m_view->selectionGroup();
    if (selectionGroup) {
        m_view->resetSelectionGroup(false);
        dragItem->setSelected(true);
    }
    m_view->setOperationMode(trimStart ? RollingStart : RollingEnd);
    m_firstInfo = m_firstClip->info();
    m_secondInfo = m_secondClip->info();
    m_view->rippleMode(true);
    m_view->loadMonitorScene(MonitorSceneRipple, true);
    m_view->seekCursorPos(trimStart ? info.startPos.frames(m_view->fps()) : info.endPos.frames(m_view->fps()));
    return true;
}

void TrimManager::moveRoll(bool forward)
{
    if (!m_firstInfo.isValid() || !m_secondInfo.isValid())
        return;
    int pos = m_firstClip->endPos().frames(m_view->fps());
    bool snap = KdenliveSettings::snaptopoints();
    KdenliveSettings::setSnaptopoints(false);
    if (forward) {
        pos++;
        m_secondClip->resizeStart(pos, true, false);
        m_firstClip->resizeEnd(pos, false);
    } else {
        pos--;
        m_firstClip->resizeEnd(pos, false);
        m_secondClip->resizeStart(pos, true, false);
    }
    m_view->seekCursorPos(pos);
    KdenliveSettings::setSnaptopoints(snap);
}

void TrimManager::endRoll()
{
    if (!m_firstInfo.isValid() || !m_secondInfo.isValid())
        return;
    m_view->rippleMode(false);
    QUndoCommand *command = new QUndoCommand;
    command->setText(i18n("Rolling Edit"));
    if (m_firstClip->endPos() < m_firstInfo.endPos) {
        m_view->prepareResizeClipEnd(m_firstClip, m_firstInfo, m_firstClip->startPos().frames(m_view->fps()), false, command);
        m_view->prepareResizeClipStart(m_secondClip, m_secondInfo, m_secondClip->startPos().frames(m_view->fps()), false, command);
    } else {
        m_view->prepareResizeClipStart(m_secondClip, m_secondInfo, m_secondClip->startPos().frames(m_view->fps()), false, command);
        m_view->prepareResizeClipEnd(m_firstClip, m_firstInfo, m_firstClip->startPos().frames(m_view->fps()), false, command);
    }
    m_commandStack->push(command);
    m_firstInfo = ItemInfo();
    m_secondInfo = ItemInfo();
}
