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
#include "renderer.h"

#include <KLocalizedString>
#include <QStandardPaths>
#include <mlt++/MltPlaylist.h>

TrimManager::TrimManager(CustomTrackView *view, DocUndoStack *commandStack) : AbstractToolManager(TrimType, view, commandStack)
    , m_firstClip(nullptr)
    , m_secondClip(nullptr)
    , m_trimMode(NormalTrim)
    , m_rippleIndex(0)
    , m_trimPlaylist(nullptr)
    , trimChanged(false)
    , m_render(nullptr)
{
}

bool TrimManager::mousePress(QMouseEvent *, const ItemInfo &info, const QList<QGraphicsItem *> &)
{
    return enterTrimMode(info, m_view->prepareMode() == ResizeStart);
}

bool TrimManager::mouseMove(QMouseEvent *event, int pos, int)
{
    if (event->buttons() & Qt::LeftButton) {
        if (!m_firstInfo.isValid() || !m_secondInfo.isValid()) {
            return false;
        }
        double snappedPos = m_view->getSnapPointForPos(pos);
        if (snappedPos < m_firstClip->endPos().frames(m_view->fps())) {
            m_firstClip->resizeEnd(snappedPos, false);
            m_secondClip->resizeStart(snappedPos, true, false);
        } else {
            m_secondClip->resizeStart(snappedPos, true, false);
            m_firstClip->resizeEnd(snappedPos, false);
        }
        m_view->seekCursorPos(snappedPos);
        return true;
    }
    return false;
}

void TrimManager::mouseRelease(QMouseEvent *, GenTime)
{
    endTrim();
}

bool TrimManager::enterTrimMode(const ItemInfo &info, bool trimStart)
{
    m_view->loadMonitorScene(MonitorSceneRipple, true);
    m_view->setQmlProperty(QStringLiteral("trimmode"), (int) m_trimMode);
    if (m_trimMode == RollingTrim || m_trimMode == RippleTrim) {
        if (trimStart) {
            m_firstClip = m_view->getClipItemAtEnd(info.startPos, info.track);
            m_secondClip = m_view->getClipItemAtStart(info.startPos, info.track);
        } else {
            m_firstClip = m_view->getClipItemAtEnd(info.endPos, info.track);
            m_secondClip = m_view->getClipItemAtStart(info.endPos, info.track);
        }
        if (!m_firstClip || !m_secondClip) {
            m_view->displayMessage(i18n("Could not find necessary clips to perform rolling trim"), InformationMessage);
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
        m_firstInfo = m_firstClip->info();
        m_secondInfo = m_secondClip->info();
        if (m_trimMode == RollingTrim)  {
            m_view->setOperationMode(trimStart ? RollingStart : RollingEnd);
        } else if (m_trimMode == RippleTrim)  {
            m_view->setOperationMode(trimStart ? RippleStart : RippleEnd);
        }
        m_view->trimMode(true, m_secondInfo.startPos.frames(m_view->fps()));
        m_view->seekCursorPos(trimStart ? info.startPos.frames(m_view->fps()) : info.endPos.frames(m_view->fps()));
    }
    if (m_trimMode == RippleTrim) {
        m_render->byPassSeek = true;
        connect(m_render, &Render::renderSeek, this, &TrimManager::renderSeekRequest, Qt::UniqueConnection);
    } else if (m_render->byPassSeek) {
        m_render->byPassSeek = false;
        disconnect(m_render, &Render::renderSeek, this, &TrimManager::renderSeekRequest);
    }
    return true;
}

void TrimManager::initRipple(Mlt::Playlist *playlist, int pos, Render *renderer)
{
    m_render = renderer;
    connect(renderer, &Render::renderSeek, this, &TrimManager::renderSeekRequest);
    m_trimPlaylist = playlist;
    m_rippleIndex = playlist->get_clip_index_at(pos);
}

void TrimManager::renderSeekRequest(int diff)
{
    qCDebug(KDENLIVE_LOG) << " + + +RIPPLE DIFF: " << diff;
    Mlt::ClipInfo *cInfo = m_trimPlaylist->clip_info(m_rippleIndex);
    int in = cInfo->frame_in;
    int out = cInfo->frame_out;
    qCDebug(KDENLIVE_LOG) << "* * *RESITE CLIP FIRST IN: " << in << "-" << out << ", " << cInfo->start << ", " << cInfo->length;
    delete cInfo;
    ClipItem *clipToRipple = nullptr;
    if (m_view->operationMode() == RippleStart) {
        in -= diff;
        clipToRipple = m_secondClip;
    } else {
        out += diff;
        clipToRipple = m_firstClip;
        m_render->seekToFrame(m_firstClip->endPos().frames(m_view->fps()) + diff);
    }
    qCDebug(KDENLIVE_LOG) << "* * *RESITE CLIP IN: " << in;
    m_trimPlaylist->resize_clip(m_rippleIndex, in, out);
    m_render->doRefresh();
    m_view->rippleClip(clipToRipple, diff);
    trimChanged = true;
}

void TrimManager::moveRoll(bool forward, int pos)
{
    if (!m_firstInfo.isValid() || !m_secondInfo.isValid()) {
        return;
    }
    if (pos == -1) {
        pos = m_firstClip->endPos().frames(m_view->fps());
        if (forward) {
            pos++;
        } else {
            pos--;
        }
    }
    bool snap = KdenliveSettings::snaptopoints();
    KdenliveSettings::setSnaptopoints(false);
    if (forward) {
        m_secondClip->resizeStart(pos, true, false);
        m_firstClip->resizeEnd(pos, false);
    } else {
        m_firstClip->resizeEnd(pos, false);
        m_secondClip->resizeStart(pos, true, false);
    }
    //m_view->seekCursorPos(pos);
    KdenliveSettings::setSnaptopoints(snap);
    trimChanged = true;
}

void TrimManager::endTrim()
{
    m_view->trimMode(false);
    if (!m_firstInfo.isValid() || !m_secondInfo.isValid()) {
        return;
    }
    if (m_render->byPassSeek) {
        m_render->byPassSeek = false;
        disconnect(m_render, &Render::renderSeek, this, &TrimManager::renderSeekRequest);
    }
    if (m_view->operationMode() == RippleStart || m_view->operationMode() == RippleEnd) {
        delete m_trimPlaylist;
        m_trimPlaylist = nullptr;
        if (m_view->operationMode() == RippleStart) {
            m_view->finishRipple(m_secondClip, m_secondInfo, (m_secondInfo.endPos - m_secondClip->endPos()).frames(m_view->fps()), true);
        } else {
            m_view->finishRipple(m_firstClip, m_firstInfo, (m_firstClip->endPos() - m_firstInfo.endPos).frames(m_view->fps()), false);
        }
        //TODO: integrate in undo/redo framework
        return;
    }
    if (m_view->operationMode() == RollingStart || m_view->operationMode() == RollingEnd) {
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
}

TrimMode TrimManager::trimMode() const
{
    return m_trimMode;
}

void TrimManager::setTrimMode(TrimMode mode, const ItemInfo &info, bool fromStart)
{
    m_trimMode = mode;
    if (trimChanged && mode != NormalTrim) {
        endTrim();
    }
    QString modeLabel;
    switch (m_trimMode) {
    case RippleTrim:
        modeLabel = i18n(" Ripple ");
        break;
    case RollingTrim:
        modeLabel = i18n(" Rolling ");
        break;
    case SlideTrim:
        modeLabel = i18n(" Slide ");
        break;
    case SlipTrim:
        modeLabel = i18n(" Slip ");
        break;
    default:
        emit updateTrimMode(modeLabel);
        endTrim();
        return;
        break;
    }
    emit updateTrimMode(modeLabel);
    enterTrimMode(info, fromStart);
}
