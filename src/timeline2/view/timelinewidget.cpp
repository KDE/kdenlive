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
#include "../model/builders/meltBuilder.hpp"
#include "qml/timelineitems.h"
#include "doc/docundostack.hpp"
#include "kdenlivesettings.h"
#include "core.h"
#include "qmltypes/thumbnailprovider.h"
#include "bin/bin.h"
#include "transitions/transitionlist/model/transitiontreemodel.hpp"

#include <KActionCollection>
#include <KDeclarative/KDeclarative>
// #include <QUrl>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlEngine>
#include <QAction>
#include <QSortFilterProxyModel>

const int TimelineWidget::comboScale[] = { 1, 2, 5, 10, 25, 50, 125, 250, 500, 750, 1500, 3000, 6000, 12000};

int TimelineWidget::m_duration = 0;

TimelineWidget::TimelineWidget(KActionCollection *actionCollection, BinController *binController, std::weak_ptr<DocUndoStack> undoStack, QWidget *parent)
    : QQuickWidget(parent)
    , m_model(TimelineItemModel::construct(binController->profile(), undoStack, false))
    , m_actionCollection(actionCollection)
    , m_binController(binController)
    , m_position(0)
    , m_scale(3.0)
{
    registerTimelineItems();
    QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(m_model.get());
    proxyModel->setSortRole(TimelineItemModel::ItemIdRole);
    proxyModel->sort(0, Qt::DescendingOrder);

    m_transitionModel.reset(new TransitionTreeModel(true, this));

    m_transitionProxyModel.reset(new AssetFilter(this));
    m_transitionProxyModel->setSourceModel(m_transitionModel.get());
    m_transitionProxyModel->setSortRole(AssetTreeModel::NameRole);
    m_transitionProxyModel->sort(0, Qt::AscendingOrder);

    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.initialize();
    kdeclarative.setupBindings();
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    rootContext()->setContextProperty("multitrack", proxyModel);
    rootContext()->setContextProperty("controller", m_model.get());
    rootContext()->setContextProperty("timeline", this);
    rootContext()->setContextProperty("transitionModel", m_transitionProxyModel.get());
    setSource(QUrl(QStringLiteral("qrc:/qml/timeline.qml")));

    m_thumbnailer = new ThumbnailProvider;
    engine()->addImageProvider(QStringLiteral("thumbnail"), m_thumbnailer);
    setFocusPolicy(Qt::StrongFocus);
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

void TimelineWidget::addSelection(int newSelection)
{
    if (m_selection.selectedClips.contains(newSelection)) {
        return;
    }
    m_selection.selectedClips << newSelection;
    emit selectionChanged();

    if (!m_selection.selectedClips.isEmpty())
        emitSelectedFromSelection();
    else
        emit selected(0);
}

double TimelineWidget::scaleFactor() const
{
    return m_scale;
}

void TimelineWidget::setScaleFactor(double scale)
{
    m_scale = scale;
    emit scaleFactorChanged();
}

int TimelineWidget::duration() const
{
    return m_duration;
}

void TimelineWidget::checkDuration()
{
    int currentLength = m_model->duration();
    if (currentLength != m_duration) {
        m_duration = currentLength;
        emit durationChanged();
    }
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


int TimelineWidget::insertClip(int tid, int position, QString data_str, bool logUndo)
{
    std::shared_ptr<Mlt::Producer> prod = std::make_shared<Mlt::Producer>(m_binController->getBinProducer(data_str));
    int id;
    if (!m_model->requestClipInsertion(prod, tid, position, id, logUndo)) {
        id = -1;
    }
    return id;
}

int TimelineWidget::insertComposition(int tid, int position, const QString& transitionId, bool logUndo)
{
    int id;
    if (!m_model->requestCompositionInsertion(transitionId, tid, position, 100, id, logUndo)) {
        id = -1;
    }
    return id;
}

void TimelineWidget::deleteSelectedClips()
{
    if (m_selection.selectedClips.isEmpty()) {
        qDebug()<<" * * *NO selection, aborting";
        return;
    }
    foreach(int cid, m_selection.selectedClips) {
        m_model->requestItemDeletion(cid);
    }
}

void TimelineWidget::triggerAction(const QString &name)
{
    QAction *action = m_actionCollection->action(name);
    if (action) {
        action->trigger();
    }
}

QString TimelineWidget::timecode(int frames)
{
    return KdenliveSettings::frametimecode() ? QString::number(frames) : m_model->tractor()->frames_to_time(frames, mlt_time_smpte_df);
}

void TimelineWidget::seek(int position)
{
    rootObject()->setProperty("seekPos", position);
    emit seeked(position);
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
    Mlt::Producer *prod = new Mlt::Producer(m_model->tractor());
    qDebug()<<"*** TIMELINE LENGTH: "<<prod->get_playtime()<<" / "<<m_model->tractor()->get_length();
    return prod;
}

void TimelineWidget::mousePressEvent(QMouseEvent *event)
{
    emit focusProjectMonitor();
    QQuickWidget::mousePressEvent(event);
}

void TimelineWidget::buildFromMelt(Mlt::Tractor tractor)
{
    qDebug() << "REQUESTING BUILD FROM MELT";
    m_thumbnailer->resetProject();
    constructTimelineFromMelt(m_model, tractor);
    checkDuration();
}

void TimelineWidget::setUndoStack(std::weak_ptr<DocUndoStack> undo_stack)
{
    m_model->setUndoStack(undo_stack);
}

void TimelineWidget::slotChangeZoom(int value, bool zoomOnMouse)
{
    setScaleFactor(100.0 / comboScale[value]);
}

void TimelineWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->delta() > 0) {
                emit zoomIn(true);
            } else {
                emit zoomOut(true);
            }
    } else {
        QQuickWidget::wheelEvent(event);
    }
}

bool TimelineWidget::showThumbnails() const
{
    return KdenliveSettings::videothumbnails();
}

bool TimelineWidget::showAudioThumbnails() const
{
    return KdenliveSettings::audiothumbnails();
}

bool TimelineWidget::showWaveforms() const
{
    return KdenliveSettings::audiothumbnails();
}

void TimelineWidget::addTrack(int tid)
{
    qDebug()<<"Adding track: "<<tid;
}

void TimelineWidget::deleteTrack(int tid)
{
    qDebug()<<"Deleting track: "<<tid;
}

int TimelineWidget::requestBestSnapPos(int pos, int duration)
{
    return m_model->requestBestSnapPos(pos, duration);
}

void TimelineWidget::gotoNextSnap()
{
    seek(m_model->requestNextSnapPos(m_position));
}

void TimelineWidget::gotoPreviousSnap()
{
    seek(m_model->requestPreviousSnapPos(m_position));
}

void TimelineWidget::groupSelection()
{
    std::unordered_set<int> clips;
    for(int id : m_selection.selectedClips) {
        clips.insert(id);
    }
    m_model->requestClipsGroup(clips);
}

void TimelineWidget::unGroupSelection(int cid)
{
    m_model->requestClipUngroup(cid);
}

void TimelineWidget::setInPoint()
{
    int cursorPos = rootObject()->property("seekPos").toInt();
    if (cursorPos < 0) {
        cursorPos = m_position;
    }
    if (!m_selection.selectedClips.isEmpty()) {
        for(int id : m_selection.selectedClips) {
            m_model->requestItemResizeToPos(id, cursorPos, false);
        }
    }
}

void TimelineWidget::setOutPoint()
{
    int cursorPos = rootObject()->property("seekPos").toInt();
    if (cursorPos < 0) {
        cursorPos = m_position;
    }
    if (!m_selection.selectedClips.isEmpty()) {
        for(int id : m_selection.selectedClips) {
            m_model->requestItemResizeToPos(id, cursorPos, true);
        }
    }
}
