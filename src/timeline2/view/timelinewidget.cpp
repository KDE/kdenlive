/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "timelinewidget.h"
#include "qml/timelineitems.h"
#include "doc/docundostack.hpp"

#include <QUrl>
#include <QQuickItem>
#include <QQmlContext>

TimelineWidget::TimelineWidget(std::weak_ptr<DocUndoStack> undoStack, QWidget *parent)
    : QQuickWidget(parent)
    , m_model(TimelineModel::construct(undoStack, true))
    , m_position(0)
{
    registerTimelineItems();
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    rootContext()->setContextProperty("multitrack", &*m_model);
    rootContext()->setContextProperty("timeline", this);
    setSource(QUrl(QStringLiteral("qrc:/qml/timeline.qml")));
    //connect(&*m_model, SIGNAL(seeked(int)), this, SLOT(onSeeked(int)));
}

void TimelineWidget::setSelection(QList<int> newSelection, int trackIndex, bool isMultitrack)
{
    if (newSelection != selection()
            || trackIndex != m_selection.selectedTrack
            || isMultitrack != m_selection.isMultitrackSelected) {
        qDebug() << "Changing selection to" << newSelection << " trackIndex" << trackIndex << "isMultitrack" << isMultitrack;
        m_selection.selectedClips = newSelection;
        m_selection.selectedTrack = trackIndex;
        m_selection.isMultitrackSelected = isMultitrack;
        emit selectionChanged();

        if (!m_selection.selectedClips.isEmpty())
            emitSelectedFromSelection();
        else
            emit selected(0);
    }
}

double TimelineWidget::scaleFactor() const
{
    //TODO
    return 3.0;
}

void TimelineWidget::setScaleFactor(double scale)
{
    //TODO
}

int TimelineWidget::duration() const
{
    return m_model->duration();
}


QList<int> TimelineWidget::selection() const
{
    if (!rootObject())
        return QList<int>();
    return m_selection.selectedClips;
}

void TimelineWidget::emitSelectedFromSelection()
{
    /*if (!m_model.trackList().count()) {
        if (m_model.tractor())
            selectMultitrack();
        else
            emit selected(0);
        return;
    }

    int trackIndex = currentTrack();
    int clipIndex = selection().isEmpty()? 0 : selection().first();
    Mlt::ClipInfo* info = getClipInfo(trackIndex, clipIndex);
    if (info && info->producer && info->producer->is_valid()) {
        delete m_updateCommand;
        m_updateCommand = new Timeline::UpdateCommand(*this, trackIndex, clipIndex, info->start);
        // We need to set these special properties so time-based filters
        // can get information about the cut while still applying filters
        // to the cut parent.
        info->producer->set(kFilterInProperty, info->frame_in);
        info->producer->set(kFilterOutProperty, info->frame_out);
        if (MLT.isImageProducer(info->producer))
            info->producer->set("out", info->cut->get_int("out"));
        info->producer->set(kMultitrackItemProperty, 1);
        m_ignoreNextPositionChange = true;
        emit selected(info->producer);
        delete info;
    }*/
}

void TimelineWidget::selectMultitrack()
{
    setSelection(QList<int>(), -1, true);
    QMetaObject::invokeMethod(rootObject(), "selectMultitrack");
    //emit selected(m_model.tractor());
}

bool TimelineWidget::snap()
{
    return true;
}

bool TimelineWidget::ripple()
{
    return false;
}

bool TimelineWidget::scrub()
{
    return false;
}

bool TimelineWidget::moveClip(int /*fromTrack*/, int toTrack, int clipIndex, int position, bool logUndo)
{
    return m_model->requestClipMove(clipIndex, toTrack, position, logUndo);
}

bool TimelineWidget::allowMoveClip(int /*fromTrack*/, int toTrack, int clipIndex, int position)
{
    return m_model->allowClipMove(clipIndex, toTrack, position);
}

QString TimelineWidget::timecode(int frames)
{
    return m_model->tractor()->frames_to_time(frames, mlt_time_smpte_df);
}

void TimelineWidget::setPosition(int position)
{
    if (!m_model->tractor()) return;
    // Testing puspose only
    m_position = position;
    emit positionChanged();
    return;

    if (position <= m_model->tractor()->get_length()) {
        //emit seeked(position);
    } else {
        m_position = m_model->tractor()->get_length();
        emit positionChanged();
    }
}

void TimelineWidget::onSeeked(int position)
{
    m_position = position;
    emit positionChanged();
}

