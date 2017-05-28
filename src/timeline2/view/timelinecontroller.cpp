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

#include "timelinecontroller.h"
#include "bin/bin.h"
#include "bin/model/markerlistmodel.hpp"
#include "../model/timelinefunctions.hpp"
#include "bin/projectclip.h"
#include "core.h"
#include "dialogs/markerdialog.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "project/projectmanager.h"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timelinewidget.h"
#include "utils/KoIconUtils.h"

#include <KActionCollection>
#include <QApplication>
#include <QQuickItem>

int TimelineController::m_duration = 0;

TimelineController::TimelineController(KActionCollection *actionCollection, QObject *parent)
    : QObject(parent)
    , m_root(nullptr)
    , m_actionCollection(actionCollection)
    , m_position(0)
    , m_seekPosition(-1)
    , m_scale(3.0)
{
}

void TimelineController::setModel(std::shared_ptr<TimelineItemModel> model)
{
    m_model = std::move(model);
}

void TimelineController::setRoot(QQuickItem *root)
{
    m_root = root;
}

Mlt::Tractor *TimelineController::tractor()
{
    return m_model->tractor();
}

void TimelineController::addSelection(int newSelection)
{
    if (m_selection.selectedClips.contains(newSelection)) {
        return;
    }
    m_selection.selectedClips << newSelection;
    emit selectionChanged();

    if (!m_selection.selectedClips.isEmpty())
        emitSelectedFromSelection();
    else
        emit selected(nullptr);
}

double TimelineController::scaleFactor() const
{
    return m_scale;
}

void TimelineController::setScaleFactor(double scale)
{
    /*if (m_duration * scale < width() - 160) {
        // Don't allow scaling less than full project's width
        scale = (width() - 160.0) / m_duration;
    }*/
    m_scale = scale;
    emit scaleFactorChanged();
}

int TimelineController::duration() const
{
    return m_duration;
}

void TimelineController::checkDuration()
{
    int currentLength = m_model->duration();
    if (currentLength != m_duration) {
        m_duration = currentLength;
        emit durationChanged();
    }
}

void TimelineController::setSelection(const QList<int> &newSelection, int trackIndex, bool isMultitrack)
{
    if (newSelection != selection() || trackIndex != m_selection.selectedTrack || isMultitrack != m_selection.isMultitrackSelected) {
        qDebug() << "Changing selection to" << newSelection << " trackIndex" << trackIndex << "isMultitrack" << isMultitrack;
        m_selection.selectedClips = newSelection;
        m_selection.selectedTrack = trackIndex;
        m_selection.isMultitrackSelected = isMultitrack;
        emit selectionChanged();

        if (!m_selection.selectedClips.isEmpty())
            emitSelectedFromSelection();
        else
            emit selected(nullptr);
    }
}

void TimelineController::emitSelectedFromSelection()
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

QList<int> TimelineController::selection() const
{
    if (!m_root) return QList<int>();
    return m_selection.selectedClips;
}

void TimelineController::selectMultitrack()
{
    setSelection(QList<int>(), -1, true);
    QMetaObject::invokeMethod(m_root, "selectMultitrack");
    // emit selected(m_model.tractor());
}

bool TimelineController::snap()
{
    return true;
}

bool TimelineController::ripple()
{
    return false;
}

bool TimelineController::scrub()
{
    return false;
}

int TimelineController::insertClip(int tid, int position, const QString &data_str, bool logUndo)
{
    int id;
    if (!m_model->requestClipInsertion(data_str, tid, position, id, logUndo)) {
        id = -1;
    }
    return id;
}

int TimelineController::insertComposition(int tid, int position, const QString &transitionId, bool logUndo)
{
    int id;
    if (!m_model->requestCompositionInsertion(transitionId, tid, position, 100, id, logUndo)) {
        id = -1;
    }
    return id;
}

void TimelineController::deleteSelectedClips()
{
    if (m_selection.selectedClips.isEmpty()) {
        qDebug() << " * * *NO selection, aborting";
        return;
    }
    for (int cid : m_selection.selectedClips) {
        m_model->requestItemDeletion(cid);
    }
}

void TimelineController::triggerAction(const QString &name)
{
    QAction *action = m_actionCollection->action(name);
    if (action) {
        action->trigger();
    }
}

QString TimelineController::timecode(int frames)
{
    return KdenliveSettings::frametimecode() ? QString::number(frames) : m_model->tractor()->frames_to_time(frames, mlt_time_smpte_df);
}
bool TimelineController::showThumbnails() const
{
    return KdenliveSettings::videothumbnails();
}

bool TimelineController::showAudioThumbnails() const
{
    return KdenliveSettings::audiothumbnails();
}

bool TimelineController::showMarkers() const
{
    return KdenliveSettings::showmarkers();
}

bool TimelineController::audioThumbFormat() const
{
    return KdenliveSettings::displayallchannels();
}

bool TimelineController::showWaveforms() const
{
    return KdenliveSettings::audiothumbnails();
}

void TimelineController::addTrack(int tid)
{
    qDebug() << "Adding track: " << tid;
}

void TimelineController::deleteTrack(int tid)
{
    qDebug() << "Deleting track: " << tid;
}

int TimelineController::requestBestSnapPos(int pos, int duration)
{
    return m_model->requestBestSnapPos(pos, duration);
}

void TimelineController::gotoNextSnap()
{
    seek(m_model->requestNextSnapPos(m_position));
}

void TimelineController::gotoPreviousSnap()
{
    seek(m_model->requestPreviousSnapPos(m_position));
}

void TimelineController::groupSelection()
{
    std::unordered_set<int> clips;
    for (int id : m_selection.selectedClips) {
        clips.insert(id);
    }
    m_model->requestClipsGroup(clips);
}

void TimelineController::unGroupSelection(int cid)
{
    m_model->requestClipUngroup(cid);
}

void TimelineController::setInPoint()
{
    int cursorPos = m_root->property("seekPos").toInt();
    if (cursorPos < 0) {
        cursorPos = m_position;
    }
    if (!m_selection.selectedClips.isEmpty()) {
        for (int id : m_selection.selectedClips) {
            m_model->requestItemResizeToPos(id, cursorPos, false);
        }
    }
}

void TimelineController::setOutPoint()
{
    int cursorPos = m_root->property("seekPos").toInt();
    if (cursorPos < 0) {
        cursorPos = m_position;
    }
    if (!m_selection.selectedClips.isEmpty()) {
        for (int id : m_selection.selectedClips) {
            m_model->requestItemResizeToPos(id, cursorPos, true);
        }
    }
}

void TimelineController::editMarker(const QString &cid, int frame)
{
    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(cid);
    bool markerFound = false;
    CommentedTime marker = clip->getMarker(GenTime(frame, pCore->getCurrentFps()), &markerFound);
    Q_ASSERT(markerFound);
    QPointer<MarkerDialog> d = new MarkerDialog(clip.get(), marker, pCore->bin()->projectTimecode(), i18n("Edit Marker"), qApp->activeWindow());
    if (d->exec() == QDialog::Accepted) {
        QList<CommentedTime> markers;
        CommentedTime newMarker = d->newMarker();
        if (newMarker.time() != marker.time()) {
            // marker was moved
            marker.setMarkerType(-1);
            markers << marker;
        }
        markers << newMarker;
        clip->addMarkers(markers);
    }
}

void TimelineController::editGuide(int frame)
{
    bool markerFound = false;
    CommentedTime marker = pCore->projectManager()->current()->getGuideModel().get()->getMarker(GenTime(frame, pCore->getCurrentFps()), &markerFound);
    Q_ASSERT(markerFound);
    QPointer<MarkerDialog> d = new MarkerDialog(nullptr, marker, pCore->bin()->projectTimecode(), i18n("Edit Marker"), qApp->activeWindow());
    if (d->exec() == QDialog::Accepted) {
        QList<CommentedTime> markers;
        CommentedTime newMarker = d->newMarker();
        if (newMarker.time() != marker.time()) {
            // marker was moved
            marker.setMarkerType(-1);
            markers << marker;
        }
        markers << newMarker;
        pCore->projectManager()->current()->addGuides(markers);
    }
}

void TimelineController::switchGuide(int frame)
{
    bool markerFound = false;
    if (frame == -1) {
        frame = m_position;
    }
    CommentedTime marker = pCore->projectManager()->current()->getGuideModel().get()->getMarker(GenTime(frame, pCore->getCurrentFps()), &markerFound);
    if (!markerFound) {
        marker = CommentedTime(GenTime(frame, pCore->getCurrentFps()), i18n("guide"));
    } else {
        marker.setMarkerType(-1);
    }
    QList<CommentedTime> markers;
    markers << marker;
    pCore->projectManager()->current()->addGuides(markers);
}

void TimelineController::addAsset(const QVariantMap data)
{
    QString effect = data.value(QStringLiteral("kdenlive/effect")).toString();
    if (!m_selection.selectedClips.isEmpty()) {
        for (int id : m_selection.selectedClips) {
            // TODO: manage effects in model
            // m_model->addEffect(id, effect);
        }
    }
}

void TimelineController::requestRefresh()
{
    pCore->requestMonitorRefresh();
}

void TimelineController::showAsset(int id)
{
    qDebug() << "show asset" << id;
    if (m_model->isComposition(id)) {
        emit showTransitionModel(m_model->getCompositionParameterModel(id));
    }
}

void TimelineController::seek(int position)
{
    m_root->setProperty("seekPos", position);
    emit seeked(position);
}

void TimelineController::setPosition(int position)
{
    emit seeked(position);
}

void TimelineController::setSeekPosition(int position)
{
    m_seekPosition = position;
    emit seekPositionChanged();
}

void TimelineController::onSeeked(int position)
{
    m_position = position;
    emit positionChanged();
    if (m_seekPosition > -1 && position == m_seekPosition) {
        m_seekPosition = -1;
        emit seekPositionChanged();
    }
}

void TimelineController::setZone(const QPoint &zone)
{
    m_zone = zone;
    emit zoneChanged();
}

void TimelineController::setZoneIn(int inPoint)
{
    m_zone.setX(inPoint);
    emit zoneMoved(m_zone);
}

void TimelineController::setZoneOut(int outPoint)
{
    m_zone.setY(outPoint);
    emit zoneMoved(m_zone);
}

void TimelineController::cutClipUnderCursor(int position, int track)
{
    if (position == -1) {
        position = m_position;
    }
    bool foundClip = false;
    for (int cid : m_selection.selectedClips) {
        if (TimelineFunction::requestClipCut(m_model, cid, position)) {
            foundClip = true;
        }
    }
    if (!foundClip) {
        if (track == -1) {
            QVariant returnedValue;
            QMetaObject::invokeMethod(m_root, "currentTrackId",
            Q_RETURN_ARG(QVariant, returnedValue));
            track = returnedValue.toInt();
        }
        if (track >= 0) {
            int cid = m_model->getClipByPosition(track, position);
            if (cid >= 0) {
                TimelineFunction::requestClipCut(m_model, cid, position);
                foundClip = true;
            }
        }
    }
    if (!foundClip) {
        //TODO: display warning, no clip found
    }
}

int TimelineController::requestSpacerStartOperation(int trackId, int position)
{
    return TimelineFunction::requestSpacerStartOperation(m_model, trackId, position);
}

bool TimelineController::requestSpacerEndOperation(int clipId, int startPosition, int endPosition)
{
    return TimelineFunction::requestSpacerEndOperation(m_model, clipId, startPosition, endPosition);
}

void TimelineController::seekCurrentClip(bool seekToEnd)
{
    bool foundClip = false;
    for (int cid : m_selection.selectedClips) {
        int start = m_model->getClipPosition(cid);
        if (seekToEnd) {
            start += m_model->getClipPlaytime(cid);
        }
        seek(start);
        foundClip = true;
        break;
    }
}

QPoint TimelineController::getTracksCount() const
{
    int audioTracks = 0;
    int videoTracks = 0;
    QVariant returnedValue;
    QMetaObject::invokeMethod(m_root, "getTracksCount",
    Q_RETURN_ARG(QVariant, returnedValue));
    QVariantList tracks = returnedValue.toList();
    QPoint p(tracks.at(0).toInt(), tracks.at(1).toInt());
    return p;
}
