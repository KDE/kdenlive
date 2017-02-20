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

TimelineWidget::TimelineWidget(BinController *binController, std::weak_ptr<DocUndoStack> undoStack, QWidget *parent)
    : QQuickWidget(parent)
    , m_model(TimelineItemModel::construct(undoStack, true))
    , m_binController(binController)
    , m_position(0)
{
    registerTimelineItems();
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    rootContext()->setContextProperty("multitrack", &*m_model);
    rootContext()->setContextProperty("timeline", this);
    setSource(QUrl(QStringLiteral("qrc:/qml/timeline.qml")));
    //m_model->tractor()->multitrack()->listen("producer-changed", this, (mlt_listener) TimelineWidget::onTractorChange);
    m_model->tractor()->refresh();
    Mlt::Producer service(m_model->tractor()->parent().get_producer());
    qDebug()<<"TL SERVIE: "<<service.get("mlt_type")<<" / "<<service.get("mlt_service");
    qDebug()<<"*** BUILDING TIMELINE LENGTH: "<<service.get("length");
    //connect(&*m_model, SIGNAL(seeked(int)), this, SLOT(onSeeked(int)));
}

void TimelineWidget::onTractorChange(mlt_multitrack, TimelineWidget *self)
{
    qDebug()<<" * * ** * *DURATUION CHANGED";
    self->updateDuration();
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

void TimelineWidget::updateDuration()
{
    rootObject()->setProperty("duration", m_model->duration());
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

bool TimelineWidget::moveClip(int toTrack, int clipIndex, int position, bool logUndo)
{
    return m_model->requestClipMove(clipIndex, toTrack, position, logUndo, logUndo);
}

bool TimelineWidget::allowMoveClip(int toTrack, int clipIndex, int position)
{
    return m_model->requestClipMove(clipIndex, toTrack, position, false, false);
}

int TimelineWidget::suggestClipMove(int toTrack, int clipIndex, int position)
{
    return m_model->suggestClipMove(clipIndex,toTrack,  position);
}
bool TimelineWidget::trimClip(int clipIndex, int delta, bool right, bool logUndo)
{
    return m_model->requestClipTrim(clipIndex, delta, right, false, logUndo);
}

bool TimelineWidget::resizeClip(int clipIndex, int duration, bool right, bool logUndo)
{
    return m_model->requestClipResize(clipIndex, duration, right, logUndo, true);
}

void TimelineWidget::insertClip(int track, int position, QString data_str)
{
    std::shared_ptr<Mlt::Producer> prod = std::make_shared<Mlt::Producer>(m_binController->getBinProducer(data_str));
    int id;
    m_model->requestClipInsertion(prod, track, position, id);
}


QString TimelineWidget::timecode(int frames)
{
    return m_model->tractor()->frames_to_time(frames, mlt_time_smpte_df);
}

void TimelineWidget::setPosition(int position)
{
    emit seeked(position);
}

void TimelineWidget::onSeeked(int position)
{
    m_position = position;
    emit positionChanged();
}

Mlt::Producer *TimelineWidget::producer()
{
    Mlt::Producer *prod = new Mlt::Producer(m_model->tractor()->get_producer());
    qDebug()<<"*** TIMELINE LENGTH: "<<prod->get_playtime()<<" / "<<m_model->tractor()->get_length();
    return prod;
}


