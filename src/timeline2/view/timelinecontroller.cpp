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
#include "../model/timelinefunctions.hpp"
#include "bin/bin.h"
#include "bin/model/markerlistmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "dialogs/spacerdialog.h"
#include "doc/kdenlivedoc.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "kdenlivesettings.h"
#include "previewmanager.h"
#include "project/projectmanager.h"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/model/compositionmodel.hpp"
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/trackmodel.hpp"
#include "timeline2/view/dialogs/clipdurationdialog.h"
#include "timeline2/view/dialogs/trackdialog.h"
#include "timelinewidget.h"
#include "transitions/transitionsrepository.hpp"
#include "lib/audio/audioEnvelope.h"

#include <KActionCollection>
#include <QApplication>
#include <QInputDialog>
#include <QQuickItem>

int TimelineController::m_duration = 0;

TimelineController::TimelineController(QObject *parent)
    : QObject(parent)
    , m_root(nullptr)
    , m_usePreview(false)
    , m_position(0)
    , m_seekPosition(-1)
    , m_activeTrack(0)
    , m_audioRef(-1)
    , m_zone(-1, -1)
    , m_scale(3.0)
    , m_timelinePreview(nullptr)
{
    m_disablePreview = pCore->currentDoc()->getAction(QStringLiteral("disable_preview"));
    connect(m_disablePreview, &QAction::triggered, this, &TimelineController::disablePreview);
    connect(this, &TimelineController::selectionChanged, this, &TimelineController::updateClipActions);
    m_disablePreview->setEnabled(false);
}

TimelineController::~TimelineController()
{
    delete m_timelinePreview;
    m_timelinePreview = nullptr;
}

void TimelineController::setModel(std::shared_ptr<TimelineItemModel> model)
{
    delete m_timelinePreview;
    m_zone = QPoint(-1, -1);
    m_timelinePreview = nullptr;
    m_model = std::move(model);
    m_selection.selectedItems.clear();
    m_selection.selectedTrack = -1;
    connect(m_model.get(), &TimelineItemModel::requestClearAssetView, [&](int id) { pCore->clearAssetPanel(id); });
    connect(m_model.get(), &TimelineItemModel::requestMonitorRefresh, [&]() { pCore->requestMonitorRefresh(); });
    connect(m_model.get(), &TimelineModel::invalidateZone, this, &TimelineController::invalidateZone, Qt::DirectConnection);
    connect(m_model.get(), &TimelineModel::durationUpdated, this, &TimelineController::checkDuration);
    connect(m_model.get(), &TimelineModel::removeFromSelection, this, &TimelineController::slotUpdateSelection);
}

void TimelineController::setTargetTracks(QPair<int, int> targets)
{
    setVideoTarget(targets.first >= 0 ? m_model->getTrackIndexFromPosition(targets.first) : -1);
    setAudioTarget(targets.second >= 0 ? m_model->getTrackIndexFromPosition(targets.second) : -1);
}

std::shared_ptr<TimelineItemModel> TimelineController::getModel() const
{
    return m_model;
}

void TimelineController::setRoot(QQuickItem *root)
{
    m_root = root;
}

Mlt::Tractor *TimelineController::tractor()
{
    return m_model->tractor();
}

void TimelineController::removeSelection(int newSelection)
{
    if (!m_selection.selectedItems.contains(newSelection)) {
        return;
    }
    m_selection.selectedItems.removeAll(newSelection);
    std::unordered_set<int> ids;
    ids.insert(m_selection.selectedItems.cbegin(), m_selection.selectedItems.cend());
    m_model->m_temporarySelectionGroup = m_model->requestClipsGroup(ids, true, GroupType::Selection);

    std::unordered_set<int> newIds;
    if (m_model->m_temporarySelectionGroup >= 0) {
        // new items were selected, inform model to prepare for group drag
        newIds = m_model->getGroupElements(m_selection.selectedItems.constFirst());
    }
    emit selectionChanged();
    if (!m_selection.selectedItems.isEmpty())
        emitSelectedFromSelection();
    else
        emit selected(nullptr);
}

void TimelineController::addSelection(int newSelection, bool clear)
{
    if (m_selection.selectedItems.contains(newSelection)) {
        return;
    }
    if (clear) {
        if (m_model->m_temporarySelectionGroup >= 0) {
            m_model->m_groups->destructGroupItem(m_model->m_temporarySelectionGroup);
            m_model->m_temporarySelectionGroup = -1;
        }
        m_selection.selectedItems.clear();
    }
    m_selection.selectedItems << newSelection;
    std::unordered_set<int> ids;
    ids.insert(m_selection.selectedItems.cbegin(), m_selection.selectedItems.cend());
    m_model->m_temporarySelectionGroup = m_model->requestClipsGroup(ids, true, GroupType::Selection);

    std::unordered_set<int> newIds;
    if (m_model->m_temporarySelectionGroup >= 0) {
        // new items were selected, inform model to prepare for group drag
        newIds = m_model->getGroupElements(m_selection.selectedItems.constFirst());
    }
    emit selectionChanged();
    if (!m_selection.selectedItems.isEmpty())
        emitSelectedFromSelection();
    else
        emit selected(nullptr);
}

int TimelineController::getCurrentItem()
{
    // TODO: if selection is empty, return topmost clip under timeline cursor
    if (m_selection.selectedItems.isEmpty()) {
        return -1;
    }
    // TODO: if selection contains more than 1 clip, return topmost clip under timeline cursor in selection
    return m_selection.selectedItems.constFirst();
}

double TimelineController::scaleFactor() const
{
    return m_scale;
}

const QString TimelineController::getTrackNameFromMltIndex(int trackPos)
{
    if (trackPos == -1) {
        return i18n("unknown");
    }
    if (trackPos == 0) {
        return i18n("Black");
    }
    return m_model->getTrackTagById(m_model->getTrackIndexFromPosition(trackPos - 1));
}

const QString TimelineController::getTrackNameFromIndex(int trackIndex)
{
    QString trackName = m_model->getTrackFullName(trackIndex);
    return trackName.isEmpty() ? m_model->getTrackTagById(trackIndex) : trackName;
}

QMap<int, QString> TimelineController::getTrackNames(bool videoOnly)
{
    QMap<int, QString> names;
    for (const auto &track : m_model->m_iteratorTable) {
        if (videoOnly && m_model->getTrackById_const(track.first)->isAudioTrack()) {
            continue;
        }
        QString trackName = m_model->getTrackFullName(track.first);
        names[m_model->getTrackMltIndex(track.first)] = trackName;
    }
    return names;
}

void TimelineController::setScaleFactorOnMouse(double scale, bool zoomOnMouse)
{
    /*if (m_duration * scale < width() - 160) {
        // Don't allow scaling less than full project's width
        scale = (width() - 160.0) / m_duration;
    }*/
    if (m_root) {
        m_root->setProperty("zoomOnMouse", zoomOnMouse ? qMin(getMousePos(), duration()) : -1);
        m_scale = scale;
        emit scaleFactorChanged();
    } else {
        qWarning("Timeline root not created, impossible to zoom in");
    }
}

void TimelineController::setScaleFactor(double scale)
{
    m_scale = scale;
    // Update mainwindow's zoom slider
    emit updateZoom(scale);
    // inform qml
    emit scaleFactorChanged();
}

int TimelineController::duration() const
{
    return m_duration;
}

int TimelineController::fullDuration() const
{
    return m_duration + TimelineModel::seekDuration;
}

void TimelineController::checkDuration()
{
    int currentLength = m_model->duration();
    if (currentLength != m_duration) {
        m_duration = currentLength;
        emit durationChanged();
    }
}

std::unordered_set<int> TimelineController::getCurrentSelectionIds() const
{
    std::unordered_set<int> selection;
    if (m_model->m_temporarySelectionGroup >= 0 ||
        (!m_selection.selectedItems.isEmpty() && m_model->m_groups->isInGroup(m_selection.selectedItems.constFirst()))) {
        selection = m_model->getGroupElements(m_selection.selectedItems.constFirst());
    } else {
        for (int i : m_selection.selectedItems) {
            selection.insert(i);
        }
    }
    return selection;
}

void TimelineController::selectCurrentItem(ObjectType type, bool select, bool addToCurrent)
{
    QList<int> toSelect;
    int currentClip = type == ObjectType::TimelineClip ? m_model->getClipByPosition(m_activeTrack, timelinePosition())
                                                       : m_model->getCompositionByPosition(m_activeTrack, timelinePosition());
    if (currentClip == -1) {
        pCore->displayMessage(i18n("No item under timeline cursor in active track"), InformationMessage, 500);
        return;
    }
    if (addToCurrent || !select) {
        toSelect = m_selection.selectedItems;
    }
    if (select) {
        if (!toSelect.contains(currentClip)) {
            toSelect << currentClip;
            setSelection(toSelect);
        }
    } else if (toSelect.contains(currentClip)) {
        toSelect.removeAll(currentClip);
        setSelection(toSelect);
    }
}

void TimelineController::setSelection(const QList<int> &newSelection, int trackIndex, bool isMultitrack)
{
    qDebug() << "Changing selection to" << newSelection << " trackIndex" << trackIndex << "isMultitrack" << isMultitrack;
    if (newSelection != selection() || trackIndex != m_selection.selectedTrack || isMultitrack != m_selection.isMultitrackSelected) {
        std::unordered_set<int> previousSelection = getCurrentSelectionIds();
        m_selection.selectedItems = newSelection;
        m_selection.selectedTrack = trackIndex;
        m_selection.isMultitrackSelected = isMultitrack;
        if (m_model->m_temporarySelectionGroup > -1) {
            // Clear current selection
            m_model->requestClipUngroup(m_model->m_temporarySelectionGroup, false);
        }
        std::unordered_set<int> newIds;
        if (m_selection.selectedItems.size() > 0) {
            std::unordered_set<int> ids;
            ids.insert(m_selection.selectedItems.cbegin(), m_selection.selectedItems.cend());
            m_model->m_temporarySelectionGroup = m_model->requestClipsGroup(ids, true, GroupType::Selection);
            if (m_model->m_temporarySelectionGroup >= 0 ||
                (!m_selection.selectedItems.isEmpty() && m_model->m_groups->isInGroup(m_selection.selectedItems.constFirst()))) {
                newIds = m_model->getGroupElements(m_selection.selectedItems.constFirst());
            } else {
                qDebug() << "// NON GROUPED SELCTUIIN: " << m_selection.selectedItems << " !!!!!!";
            }
            emitSelectedFromSelection();
        } else {
            // Empty selection
            emit selected(nullptr);
            emit showItemEffectStack(QString(), nullptr, QSize(), false);
        }
        emit selectionChanged();
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
    return m_selection.selectedItems;
}

void TimelineController::selectMultitrack()
{
    setSelection(QList<int>(), -1, true);
    QMetaObject::invokeMethod(m_root, "selectMultitrack");
    // emit selected(m_model.tractor());
}

void TimelineController::resetView()
{
    m_model->_resetView();
    if (m_root) {
        QMetaObject::invokeMethod(m_root, "updatePalette");
    }
}

bool TimelineController::snap()
{
    return KdenliveSettings::snaptopoints();
}

void TimelineController::snapChanged(bool snap)
{
    m_root->setProperty("snapping", snap ? 10 / std::sqrt(m_scale) : -1);
}

bool TimelineController::ripple()
{
    return false;
}

bool TimelineController::scrub()
{
    return false;
}

int TimelineController::insertClip(int tid, int position, const QString &data_str, bool logUndo, bool refreshView, bool useTargets)
{
    int id;
    if (tid == -1) {
        tid = m_activeTrack;
    }
    if (position == -1) {
        position = timelinePosition();
    }
    if (!m_model->requestClipInsertion(data_str, tid, position, id, logUndo, refreshView, useTargets)) {
        id = -1;
    }
    return id;
}

QList<int> TimelineController::insertClips(int tid, int position, const QStringList &binIds, bool logUndo, bool refreshView)
{
    QList<int> clipIds;
    if (tid == -1) {
        tid = m_activeTrack;
    }
    if (position == -1) {
        position = timelinePosition();
    }
    TimelineFunctions::requestMultipleClipsInsertion(m_model, binIds, tid, position, clipIds, logUndo, refreshView);
    // we don't need to check the return value of the above function, in case of failure it will return an empty list of ids.
    return clipIds;
}

int TimelineController::insertNewComposition(int tid, int position, const QString &transitionId, bool logUndo)
{
    int id;
    //int duration = pCore->currentDoc()->getFramePos(KdenliveSettings::transition_duration());
    int duration = m_model->getTrackById_const(tid)->suggestCompositionLength(position);
    int lowerVideoTrackId = m_model->getPreviousVideoTrackIndex(tid);
    if (lowerVideoTrackId > 0) {
        int duration2 = m_model->getTrackById_const(lowerVideoTrackId)->suggestCompositionLength(position);
        if (duration2 > 0) {
            duration = (duration > 0) ? qMin(duration, duration2) : duration2;
        }
    }
    if (duration <= 4) {
        // if suggested composition duration is lower than 4 frames, use default
        duration = pCore->currentDoc()->getFramePos(KdenliveSettings::transition_duration());
    }
    if (!m_model->requestCompositionInsertion(transitionId, tid, position, duration, nullptr, id, logUndo)) {
        id = -1;
    }
    return id;
}

int TimelineController::insertComposition(int tid, int position, const QString &transitionId, bool logUndo)
{
    int id;
    int duration = pCore->currentDoc()->getFramePos(KdenliveSettings::transition_duration());
    if (!m_model->requestCompositionInsertion(transitionId, tid, position, duration, nullptr, id, logUndo)) {
        id = -1;
    }
    return id;
}

void TimelineController::deleteSelectedClips()
{
    if (m_selection.selectedItems.isEmpty()) {
        return;
    }
    if (m_model->m_temporarySelectionGroup != -1) {
        // selection is grouped, delete group only
        m_model->requestGroupDeletion(m_model->m_temporarySelectionGroup);
    } else {
        for (int cid : m_selection.selectedItems) {
            m_model->requestItemDeletion(cid);
        }
    }
    m_selection.selectedItems.clear();
    emit selectionChanged();
}

void TimelineController::slotUpdateSelection(int itemId)
{
    if (m_selection.selectedItems.contains(itemId)) {
        m_selection.selectedItems.removeAll(itemId);
        emit selectionChanged();
    }
}

void TimelineController::copyItem()
{
    int clipId = -1;
    if (!m_selection.selectedItems.isEmpty()) {
        clipId = m_selection.selectedItems.first();
    } else {
        return;
    }
    m_root->setProperty("copiedClip", clipId);
}

bool TimelineController::pasteItem(int clipId, int tid, int position)
{
    // TODO: copy multiple clips / groups
    if (clipId == -1) {
        clipId = m_root->property("copiedClip").toInt();
        if (clipId == -1) {
            return -1;
        }
    }
    if (tid == -1 && position == -1) {
        tid = getMouseTrack();
        position = getMousePos();
    }
    if (tid == -1) {
        tid = m_activeTrack;
    }
    if (position == -1) {
        position = timelinePosition();
    }
    qDebug() << "PASTING CLIP: " << clipId << ", " << tid << ", " << position;
    return TimelineFunctions::requestItemCopy(m_model, clipId, tid, position);
    return false;
}

void TimelineController::triggerAction(const QString &name)
{
    pCore->triggerAction(name);
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
    if (tid == -1) {
        tid = m_activeTrack;
    }
    QPointer<TrackDialog> d = new TrackDialog(m_model, tid, qApp->activeWindow());
    if (d->exec() == QDialog::Accepted) {
        int newTid;
        m_model->requestTrackInsertion(d->selectedTrackPosition(), newTid, d->trackName(), d->addAudioTrack());
        m_model->buildTrackCompositing(true);
    }
}

void TimelineController::deleteTrack(int tid)
{
    if (tid == -1) {
        tid = m_activeTrack;
    }
    QPointer<TrackDialog> d = new TrackDialog(m_model, tid, qApp->activeWindow(), true);
    if (d->exec() == QDialog::Accepted) {
        int selectedTrackIx = d->selectedTrackId();
        m_model->requestTrackDeletion(selectedTrackIx);
        m_model->buildTrackCompositing(true);
        if (m_activeTrack == selectedTrackIx) {
            setActiveTrack(m_model->getTrackIndexFromPosition(m_model->getTracksCount() - 1));
        }
    }
}

void TimelineController::gotoNextSnap()
{
    setPosition(m_model->requestNextSnapPos(timelinePosition()));
}

void TimelineController::gotoPreviousSnap()
{
    setPosition(m_model->requestPreviousSnapPos(timelinePosition()));
}

void TimelineController::groupSelection()
{
    if (m_selection.selectedItems.size() < 2) {
        pCore->displayMessage(i18n("Select at least 2 items to group"), InformationMessage, 500);
        return;
    }
    std::unordered_set<int> clips;
    for (int id : m_selection.selectedItems) {
        clips.insert(id);
    }
    m_model->requestClipsGroup(clips);
    emit selectionChanged();
}

void TimelineController::unGroupSelection(int cid)
{
    if (cid == -1 && m_selection.selectedItems.isEmpty()) {
        pCore->displayMessage(i18n("Select at least 1 item to ungroup"), InformationMessage, 500);
        return;
    }
    if (cid == -1) {
        if (m_model->m_temporarySelectionGroup >= 0) {
            cid = m_model->m_temporarySelectionGroup;
        } else {
            for (int id : m_selection.selectedItems) {
                if (m_model->m_groups->getRootId(id)) {
                    cid = id;
                    break;
                }
            }
        }
    }
    if (cid > -1) {
        if (m_model->m_groups->isInGroup(cid)) {
            cid = m_model->m_groups->getRootId(cid);
        }
        m_model->requestClipUngroup(cid);
    }
    if (m_model->m_temporarySelectionGroup >= 0) {
        m_model->requestClipUngroup(m_model->m_temporarySelectionGroup, false);
    }
    m_selection.selectedItems.clear();
    emit selectionChanged();
}

void TimelineController::setInPoint()
{
    int cursorPos = timelinePosition();
    if (!m_selection.selectedItems.isEmpty()) {
        for (int id : m_selection.selectedItems) {
            int start = m_model->getItemPosition(id);
            if (start == cursorPos) {
                continue;
            }
            int size = start + m_model->getItemPlaytime(id) - cursorPos;
            m_model->requestItemResize(id, size, false, true, 0, false);
        }
    }
}

int TimelineController::timelinePosition() const
{
    return m_seekPosition >= 0 ? m_seekPosition : m_position;
}

void TimelineController::setOutPoint()
{
    int cursorPos = timelinePosition();
    if (!m_selection.selectedItems.isEmpty()) {
        for (int id : m_selection.selectedItems) {
            int start = m_model->getItemPosition(id);
            if (start + m_model->getItemPlaytime(id) == cursorPos) {
                continue;
            }
            int size = cursorPos - start;
            m_model->requestItemResize(id, size, true, true, 0, false);
        }
    }
}

void TimelineController::editMarker(const QString &cid, int frame)
{
    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(cid);
    GenTime pos(frame, pCore->getCurrentFps());
    clip->getMarkerModel()->editMarkerGui(pos, qApp->activeWindow(), false, clip.get());
}

void TimelineController::editGuide(int frame)
{
    if (frame == -1) {
        frame = timelinePosition();
    }
    auto guideModel = pCore->projectManager()->current()->getGuideModel();
    GenTime pos(frame, pCore->getCurrentFps());
    guideModel->editMarkerGui(pos, qApp->activeWindow(), false);
}

void TimelineController::moveGuide(int frame, int newFrame)
{
    auto guideModel = pCore->projectManager()->current()->getGuideModel();
    GenTime pos(frame, pCore->getCurrentFps());
    GenTime newPos(newFrame, pCore->getCurrentFps());
    guideModel->editMarker(pos, newPos);
}

void TimelineController::switchGuide(int frame, bool deleteOnly)
{
    bool markerFound = false;
    if (frame == -1) {
        frame = timelinePosition();
    }
    CommentedTime marker = pCore->projectManager()->current()->getGuideModel()->getMarker(GenTime(frame, pCore->getCurrentFps()), &markerFound);
    if (!markerFound) {
        if (deleteOnly) {
            pCore->displayMessage(i18n("No guide found at current position"), InformationMessage, 500);
            return;
        }
        GenTime pos(frame, pCore->getCurrentFps());
        pCore->projectManager()->current()->getGuideModel()->addMarker(pos, i18n("guide"));
    } else {
        pCore->projectManager()->current()->getGuideModel()->removeMarker(marker.time());
    }
}

void TimelineController::addAsset(const QVariantMap data)
{
    QString effect = data.value(QStringLiteral("kdenlive/effect")).toString();
    if (!m_selection.selectedItems.isEmpty()) {
        QList<int> effectSelection;
        for (int id : m_selection.selectedItems) {
            if (m_model->isClip(id)) {
                effectSelection << id;
                int partner = m_model->getClipSplitPartner(id);
                if (partner > -1 && !effectSelection.contains(partner)) {
                    effectSelection << partner;
                }
            }
        }
        bool foundMatch = false;
        for (int id : effectSelection) {
            if (m_model->addClipEffect(id, effect, false)) {
                foundMatch = true;
            }
        }
        if (!foundMatch) {
            QString effectName = EffectsRepository::get()->getName(effect);
            pCore->displayMessage(i18n("Cannot add effect %1 to selected clip", effectName), InformationMessage, 500);
        }
    } else {
        pCore->displayMessage(i18n("Select a clip to apply an effect"), InformationMessage, 500);
    }
}

void TimelineController::requestRefresh()
{
    pCore->requestMonitorRefresh();
}

void TimelineController::showAsset(int id)
{
    if (m_model->isComposition(id)) {
        emit showTransitionModel(id, m_model->getCompositionParameterModel(id));
    } else if (m_model->isClip(id)) {
        QModelIndex clipIx = m_model->makeClipIndexFromID(id);
        QString clipName = m_model->data(clipIx, Qt::DisplayRole).toString();
        bool showKeyframes = m_model->data(clipIx, TimelineModel::ShowKeyframesRole).toInt();
        qDebug() << "-----\n// SHOW KEYFRAMES: " << showKeyframes;
        emit showItemEffectStack(clipName, m_model->getClipEffectStackModel(id), m_model->getClipFrameSize(id), showKeyframes);
    }
}

void TimelineController::showTrackAsset(int trackId)
{
    emit showItemEffectStack(getTrackNameFromIndex(trackId), m_model->getTrackEffectStackModel(trackId), pCore->getCurrentFrameSize(), false);
}

void TimelineController::setPosition(int position)
{
    setSeekPosition(position);
    emit seeked(position);
}

void TimelineController::setAudioTarget(int track)
{
    m_model->m_audioTarget = track;
    emit audioTargetChanged();
}

void TimelineController::setVideoTarget(int track)
{
    m_model->m_videoTarget = track;
    emit videoTargetChanged();
}

void TimelineController::setActiveTrack(int track)
{
    m_activeTrack = track;
    emit activeTrackChanged();
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
    if (m_zone.x() > 0) {
        m_model->removeSnap(m_zone.x());
    }
    if (m_zone.y() > 0) {
        m_model->removeSnap(m_zone.y() - 1);
    }
    if (zone.x() > 0) {
        m_model->addSnap(zone.x());
    }
    if (zone.y() > 0) {
        m_model->addSnap(zone.y() - 1);
    }
    m_zone = zone;
    emit zoneChanged();
}

void TimelineController::setZoneIn(int inPoint)
{
    if (m_zone.x() > 0) {
        m_model->removeSnap(m_zone.x());
    }
    if (inPoint > 0) {
        m_model->addSnap(inPoint);
    }
    m_zone.setX(inPoint);
    emit zoneMoved(m_zone);
}

void TimelineController::setZoneOut(int outPoint)
{
    if (m_zone.y() > 0) {
        m_model->removeSnap(m_zone.y() - 1);
    }
    if (outPoint > 0) {
        m_model->addSnap(outPoint - 1);
    }
    m_zone.setY(outPoint);
    emit zoneMoved(m_zone);
}

void TimelineController::selectItems(QVariantList arg, int startFrame, int endFrame, bool addToSelect)
{
    std::unordered_set<int> previousSelection = getCurrentSelectionIds();
    std::unordered_set<int> itemsToSelect;
    if (addToSelect) {
        for (int cid : m_selection.selectedItems) {
            itemsToSelect.insert(cid);
        }
    }
    m_selection.selectedItems.clear();
    for (int i = 0; i < arg.count(); i++) {
        auto currentClips = m_model->getItemsInRange(arg.at(i).toInt(), startFrame, endFrame, true);
        itemsToSelect.insert(currentClips.begin(), currentClips.end());
    }
    if (itemsToSelect.size() > 0) {
        for (int x : itemsToSelect) {
            m_selection.selectedItems << x;
        }
        qDebug() << "// GROUPING ITEMS: " << m_selection.selectedItems;
        m_model->m_temporarySelectionGroup = m_model->requestClipsGroup(itemsToSelect, true, GroupType::Selection);
        qDebug() << "// GROUPING ITEMS DONE";
    } else if (m_model->m_temporarySelectionGroup > -1) {
        m_model->requestClipUngroup(m_model->m_temporarySelectionGroup, false);
    }
    std::unordered_set<int> newIds;
    if (m_model->m_temporarySelectionGroup >= 0) {
        newIds = m_model->getGroupElements(m_selection.selectedItems.constFirst());
        for (int child : newIds) {
            QModelIndex ix;
            if (m_model->isClip(child)) {
                ix = m_model->makeClipIndexFromID(child);
            } else if (m_model->isComposition(child)) {
                ix = m_model->makeCompositionIndexFromID(child);
            }
            if (ix.isValid()) {
                m_model->dataChanged(ix, ix, {TimelineModel::GroupedRole});
            }
        }
    }
    emit selectionChanged();
}

void TimelineController::requestClipCut(int clipId, int position)
{
    if (position == -1) {
        position = timelinePosition();
    }
    TimelineFunctions::requestClipCut(m_model, clipId, position);
}

void TimelineController::cutClipUnderCursor(int position, int track)
{
    if (position == -1) {
        position = timelinePosition();
    }
    bool foundClip = false;
    for (int cid : m_selection.selectedItems) {
        if (m_model->isClip(cid)) {
            if (TimelineFunctions::requestClipCut(m_model, cid, position)) {
                foundClip = true;
            }
        } else {
            qDebug() << "//// TODO: COMPOSITION CUT!!!";
        }
    }
    if (!foundClip) {
        if (track == -1) {
            track = m_activeTrack;
        }
        if (track >= 0) {
            int cid = m_model->getClipByPosition(track, position);
            if (cid >= 0 && TimelineFunctions::requestClipCut(m_model, cid, position)) {
                foundClip = true;
            }
        }
    }
    if (!foundClip) {
        pCore->displayMessage(i18n("No clip to cut"), InformationMessage, 500);
    }
}

int TimelineController::requestSpacerStartOperation(int trackId, int position)
{
    return TimelineFunctions::requestSpacerStartOperation(m_model, trackId, position);
}

bool TimelineController::requestSpacerEndOperation(int clipId, int startPosition, int endPosition)
{
    return TimelineFunctions::requestSpacerEndOperation(m_model, clipId, startPosition, endPosition);
}

void TimelineController::seekCurrentClip(bool seekToEnd)
{
    for (int cid : m_selection.selectedItems) {
        int start = m_model->getItemPosition(cid);
        if (seekToEnd) {
            start += m_model->getItemPlaytime(cid);
        }
        setPosition(start);
        break;
    }
}

void TimelineController::seekToClip(int cid, bool seekToEnd)
{
    int start = m_model->getItemPosition(cid);
    if (seekToEnd) {
        start += m_model->getItemPlaytime(cid);
    }
    setPosition(start);
}

void TimelineController::seekToMouse()
{
    QVariant returnedValue;
    QMetaObject::invokeMethod(m_root, "getMousePos", Q_RETURN_ARG(QVariant, returnedValue));
    int mousePos = returnedValue.toInt();
    setPosition(mousePos);
}

int TimelineController::getMousePos()
{
    QVariant returnedValue;
    QMetaObject::invokeMethod(m_root, "getMousePos", Q_RETURN_ARG(QVariant, returnedValue));
    return returnedValue.toInt();
}

int TimelineController::getMouseTrack()
{
    QVariant returnedValue;
    QMetaObject::invokeMethod(m_root, "getMouseTrack", Q_RETURN_ARG(QVariant, returnedValue));
    return returnedValue.toInt();
}

void TimelineController::refreshItem(int id)
{
    int in = m_model->getItemPosition(id);
    if (in > m_position) {
        return;
    }
    if (m_position <= in + m_model->getItemPlaytime(id)) {
        pCore->requestMonitorRefresh();
    }
}

QPoint TimelineController::getTracksCount() const
{
    QVariant returnedValue;
    QMetaObject::invokeMethod(m_root, "getTracksCount", Q_RETURN_ARG(QVariant, returnedValue));
    QVariantList tracks = returnedValue.toList();
    QPoint p(tracks.at(0).toInt(), tracks.at(1).toInt());
    return p;
}

QStringList TimelineController::extractCompositionLumas() const
{
    return m_model->extractCompositionLumas();
}

void TimelineController::addEffectToCurrentClip(const QStringList &effectData)
{
    QList<int> activeClips;
    for (int track = m_model->getTracksCount() - 1; track >= 0; track--) {
        int trackIx = m_model->getTrackIndexFromPosition(track);
        int cid = m_model->getClipByPosition(trackIx, timelinePosition());
        if (cid > -1) {
            activeClips << cid;
        }
    }
    if (!activeClips.isEmpty()) {
        if (effectData.count() == 4) {
            QString effectString = effectData.at(1) + QStringLiteral("-") + effectData.at(2) + QStringLiteral("-") + effectData.at(3);
            m_model->copyClipEffect(activeClips.first(), effectString);
        } else {
            m_model->addClipEffect(activeClips.first(), effectData.constFirst());
        }
    }
}

void TimelineController::adjustFade(int cid, const QString &effectId, int duration, int initialDuration)
{
    if (duration <= 0) {
        // remove fade
        m_model->removeFade(cid, effectId == QLatin1String("fadein"));
    } else {
        m_model->adjustEffectLength(cid, effectId, duration, initialDuration);
    }
}

QPair<int, int> TimelineController::getCompositionATrack(int cid) const
{
    QPair<int, int> result;
    std::shared_ptr<CompositionModel> compo = m_model->getCompositionPtr(cid);
    if (compo) {
        result = QPair<int, int>(compo->getATrack(), m_model->getTrackMltIndex(compo->getCurrentTrackId()));
    }
    return result;
}

void TimelineController::setCompositionATrack(int cid, int aTrack)
{
    TimelineFunctions::setCompositionATrack(m_model, cid, aTrack);
}

bool TimelineController::compositionAutoTrack(int cid) const
{
    std::shared_ptr<CompositionModel> compo = m_model->getCompositionPtr(cid);
    return compo && compo->getForcedTrack() == -1;
}

const QString TimelineController::getClipBinId(int clipId) const
{
    return m_model->getClipBinId(clipId);
}

void TimelineController::focusItem(int itemId)
{
    int start = m_model->getItemPosition(itemId);
    setPosition(start);
}

int TimelineController::headerWidth() const
{
    return qMax(10, KdenliveSettings::headerwidth());
}

void TimelineController::setHeaderWidth(int width)
{
    KdenliveSettings::setHeaderwidth(width);
}

bool TimelineController::createSplitOverlay(Mlt::Filter *filter)
{
    if (m_timelinePreview && m_timelinePreview->hasOverlayTrack()) {
        return true;
    }
    int clipId = getCurrentItem();
    if (clipId == -1) {
        pCore->displayMessage(i18n("Select a clip to compare effect"), InformationMessage, 500);
        return false;
    }
    std::shared_ptr<ClipModel> clip = m_model->getClipPtr(clipId);
    const QString binId = clip->binId();

    // Get clean bin copy of the clip
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(binId);
    std::shared_ptr<Mlt::Producer> binProd(binClip->masterProducer()->cut(clip->getIn(), clip->getOut()));

    // Get copy of timeline producer
    Mlt::Producer *clipProducer = new Mlt::Producer(*clip);

    // Built tractor and compositing
    Mlt::Tractor trac(*m_model->m_tractor->profile());
    Mlt::Playlist play(*m_model->m_tractor->profile());
    Mlt::Playlist play2(*m_model->m_tractor->profile());
    play.append(*clipProducer);
    play2.append(*binProd);
    trac.set_track(play, 0);
    trac.set_track(play2, 1);
    play2.attach(*filter);
    QString splitTransition = TransitionsRepository::get()->getCompositingTransition();
    Mlt::Transition t(*m_model->m_tractor->profile(), splitTransition.toUtf8().constData());
    t.set("always_active", 1);
    trac.plant_transition(t, 0, 1);
    int startPos = m_model->getClipPosition(clipId);

    // plug in overlay playlist
    Mlt::Playlist *overlay = new Mlt::Playlist(*m_model->m_tractor->profile());
    overlay->insert_blank(0, startPos);
    Mlt::Producer split(trac.get_producer());
    overlay->insert_at(startPos, &split, 1);

    // insert in tractor
    if (!m_timelinePreview) {
        initializePreview();
    }
    m_timelinePreview->setOverlayTrack(overlay);
    m_model->m_overlayTrackCount = m_timelinePreview->addedTracks();
    return true;
}

void TimelineController::removeSplitOverlay()
{
    if (m_timelinePreview && !m_timelinePreview->hasOverlayTrack()) {
        return;
    }
    // disconnect
    m_timelinePreview->removeOverlayTrack();
    m_model->m_overlayTrackCount = m_timelinePreview->addedTracks();
}

void TimelineController::addPreviewRange(bool add)
{
    if (m_timelinePreview && !m_zone.isNull()) {
        m_timelinePreview->addPreviewRange(m_zone, add);
    }
}

void TimelineController::clearPreviewRange()
{
    if (m_timelinePreview) {
        m_timelinePreview->clearPreviewRange();
    }
}

void TimelineController::startPreviewRender()
{
    // Timeline preview stuff
    if (!m_timelinePreview) {
        initializePreview();
    } else if (m_disablePreview->isChecked()) {
        m_disablePreview->setChecked(false);
        disablePreview(false);
    }
    if (m_timelinePreview) {
        if (!m_usePreview) {
            m_timelinePreview->buildPreviewTrack();
            m_usePreview = true;
            m_model->m_overlayTrackCount = m_timelinePreview->addedTracks();
        }
        m_timelinePreview->startPreviewRender();
    }
}

void TimelineController::stopPreviewRender()
{
    if (m_timelinePreview) {
        m_timelinePreview->abortRendering();
    }
}

void TimelineController::initializePreview()
{
    if (m_timelinePreview) {
        // Update parameters
        if (!m_timelinePreview->loadParams()) {
            if (m_usePreview) {
                // Disconnect preview track
                m_timelinePreview->disconnectTrack();
                m_usePreview = false;
            }
            delete m_timelinePreview;
            m_timelinePreview = nullptr;
        }
    } else {
        m_timelinePreview = new PreviewManager(this, m_model->m_tractor.get());
        if (!m_timelinePreview->initialize()) {
            // TODO warn user
            delete m_timelinePreview;
            m_timelinePreview = nullptr;
        } else {
        }
    }
    QAction *previewRender = pCore->currentDoc()->getAction(QStringLiteral("prerender_timeline_zone"));
    if (previewRender) {
        previewRender->setEnabled(m_timelinePreview != nullptr);
    }
    m_disablePreview->setEnabled(m_timelinePreview != nullptr);
    m_disablePreview->blockSignals(true);
    m_disablePreview->setChecked(false);
    m_disablePreview->blockSignals(false);
}

void TimelineController::disablePreview(bool disable)
{
    if (disable) {
        m_timelinePreview->deletePreviewTrack();
        m_usePreview = false;
        m_model->m_overlayTrackCount = m_timelinePreview->addedTracks();
    } else {
        if (!m_usePreview) {
            if (!m_timelinePreview->buildPreviewTrack()) {
                // preview track already exists, reconnect
                m_model->m_tractor->lock();
                m_timelinePreview->reconnectTrack();
                m_model->m_tractor->unlock();
            }
            m_timelinePreview->loadChunks(QVariantList(), QVariantList(), QDateTime());
            m_usePreview = true;
        }
    }
    m_model->m_overlayTrackCount = m_timelinePreview->addedTracks();
}

QVariantList TimelineController::dirtyChunks() const
{
    return m_timelinePreview ? m_timelinePreview->m_dirtyChunks : QVariantList();
}

QVariantList TimelineController::renderedChunks() const
{
    return m_timelinePreview ? m_timelinePreview->m_renderedChunks : QVariantList();
}

int TimelineController::workingPreview() const
{
    return m_timelinePreview ? m_timelinePreview->workingPreview : -1;
}

bool TimelineController::useRuler() const
{
    return pCore->currentDoc()->getDocumentProperty(QStringLiteral("enableTimelineZone")).toInt() == 1;
}

void TimelineController::resetPreview()
{
    if (m_timelinePreview) {
        m_timelinePreview->clearPreviewRange();
        initializePreview();
    }
}

void TimelineController::loadPreview(QString chunks, QString dirty, const QDateTime &documentDate, int enable)
{
    if (chunks.isEmpty() && dirty.isEmpty()) {
        return;
    }
    if (!m_timelinePreview) {
        initializePreview();
    }
    QVariantList renderedChunks;
    QVariantList dirtyChunks;
    QStringList chunksList = chunks.split(QLatin1Char(','), QString::SkipEmptyParts);
    QStringList dirtyList = dirty.split(QLatin1Char(','), QString::SkipEmptyParts);
    for (const QString &frame : chunksList) {
        renderedChunks << frame.toInt();
    }
    for (const QString &frame : dirtyList) {
        dirtyChunks << frame.toInt();
    }
    m_disablePreview->blockSignals(true);
    m_disablePreview->setChecked(enable);
    m_disablePreview->blockSignals(false);
    if (!enable) {
        m_timelinePreview->buildPreviewTrack();
        m_usePreview = true;
        m_model->m_overlayTrackCount = m_timelinePreview->addedTracks();
    }
    m_timelinePreview->loadChunks(renderedChunks, dirtyChunks, documentDate);
}

QMap<QString, QString> TimelineController::documentProperties()
{
    QMap<QString, QString> props = pCore->currentDoc()->documentProperties();
    int audioTarget = m_model->m_audioTarget == -1 ? -1 : m_model->getTrackPosition(m_model->m_audioTarget);
    int videoTarget = m_model->m_videoTarget == -1 ? -1 : m_model->getTrackPosition(m_model->m_videoTarget);
    int activeTrack = m_activeTrack == -1 ? -1 : m_model->getTrackPosition(m_activeTrack);
    props.insert(QStringLiteral("audioTarget"), QString::number(audioTarget));
    props.insert(QStringLiteral("videoTarget"), QString::number(videoTarget));
    props.insert(QStringLiteral("activeTrack"), QString::number(activeTrack));
    props.insert(QStringLiteral("position"), QString::number(timelinePosition()));
    props.insert(QStringLiteral("zonein"), QString::number(m_zone.x()));
    props.insert(QStringLiteral("zoneout"), QString::number(m_zone.y()));
    if (m_timelinePreview) {
        QPair<QStringList, QStringList> chunks = m_timelinePreview->previewChunks();
        props.insert(QStringLiteral("previewchunks"), chunks.first.join(QLatin1Char(',')));
        props.insert(QStringLiteral("dirtypreviewchunks"), chunks.second.join(QLatin1Char(',')));
    }
    props.insert(QStringLiteral("disablepreview"), QString::number((int)m_disablePreview->isChecked()));
    return props;
}

void TimelineController::insertSpace(int trackId, int frame)
{
    if (frame == -1) {
        frame = timelinePosition();
    }
    if (trackId == -1) {
        trackId = m_activeTrack;
    }
    QPointer<SpacerDialog> d = new SpacerDialog(GenTime(65, pCore->getCurrentFps()), pCore->currentDoc()->timecode(), qApp->activeWindow());
    if (d->exec() != QDialog::Accepted) {
        delete d;
        return;
    }
    int cid = requestSpacerStartOperation(d->affectAllTracks() ? -1 : trackId, frame);
    int spaceDuration = d->selectedDuration().frames(pCore->getCurrentFps());
    delete d;
    if (cid == -1) {
        pCore->displayMessage(i18n("No clips found to insert space"), InformationMessage, 500);
        return;
    }
    int start = m_model->getItemPosition(cid);
    requestSpacerEndOperation(cid, start, start + spaceDuration);
}

void TimelineController::removeSpace(int trackId, int frame, bool affectAllTracks)
{
    if (frame == -1) {
        frame = timelinePosition();
    }
    if (trackId == -1) {
        trackId = m_activeTrack;
    }
    // find blank duration
    int spaceDuration = m_model->getTrackById_const(trackId)->getBlankSizeAtPos(frame);
    int cid = requestSpacerStartOperation(affectAllTracks ? -1 : trackId, frame);
    if (cid == -1) {
        pCore->displayMessage(i18n("No clips found to insert space"), InformationMessage, 500);
        return;
    }
    int start = m_model->getItemPosition(cid);
    requestSpacerEndOperation(cid, start, start - spaceDuration);
}

void TimelineController::invalidateClip(int cid)
{
    if (!m_timelinePreview) {
        return;
    }
    int start = m_model->getItemPosition(cid);
    int end = start + m_model->getItemPlaytime(cid);
    m_timelinePreview->invalidatePreview(start, end);
}

void TimelineController::invalidateZone(int in, int out)
{
    if (!m_timelinePreview) {
        return;
    }
    m_timelinePreview->invalidatePreview(in, out);
}

void TimelineController::changeItemSpeed(int clipId, double speed)
{
    if (qFuzzyCompare(speed, -1)) {
        speed = 100 * m_model->getClipSpeed(clipId);
        bool ok = false;
        double duration = m_model->getItemPlaytime(clipId);
        // this is the max speed so that the clip is at least one frame long
        double maxSpeed = 100. * duration * qAbs(m_model->getClipSpeed(clipId));
        // this is the min speed so that the clip doesn't bump into the next one on track
        double minSpeed = 100. * duration * qAbs(m_model->getClipSpeed(clipId)) / (duration + double(m_model->getBlankSizeNearClip(clipId, true)) - 1);

        // if there is a split partner, we must also take it into account
        int partner = m_model->getClipSplitPartner(clipId);
        if (partner != -1) {
            double duration2 = m_model->getItemPlaytime(partner);
            double maxSpeed2 = 100. * duration2 * qAbs(m_model->getClipSpeed(partner));
            double minSpeed2 = 100. * duration2 * qAbs(m_model->getClipSpeed(partner)) / (duration2 + double(m_model->getBlankSizeNearClip(partner, true)) - 1);
            minSpeed = std::max(minSpeed, minSpeed2);
            maxSpeed = std::min(maxSpeed, maxSpeed2);
        }
        speed = QInputDialog::getDouble(QApplication::activeWindow(), i18n("Clip Speed"), i18n("Percentage"), speed, minSpeed, maxSpeed, 2, &ok);
        if (!ok) {
            return;
        }
    }
    m_model->requestClipTimeWarp(clipId, speed);
}

void TimelineController::switchCompositing(int mode)
{
    // m_model->m_tractor->lock();
    QScopedPointer<Mlt::Service> service(m_model->m_tractor->field());
    Mlt::Field *field = m_model->m_tractor->field();
    field->lock();
    while ((service != nullptr) && service->is_valid()) {
        if (service->type() == transition_type) {
            Mlt::Transition t((mlt_transition)service->get_service());
            QString serviceName = t.get("mlt_service");
            if (t.get_int("internal_added") == 237 && serviceName != QLatin1String("mix")) {
                // remove all compositing transitions
                field->disconnect_service(t);
            }
        }
        service.reset(service->producer());
    }
    if (mode > 0) {
        const QString compositeGeometry =
            QStringLiteral("0=0/0:%1x%2").arg(m_model->m_tractor->profile()->width()).arg(m_model->m_tractor->profile()->height());

        // Loop through tracks
        for (int track = 1; track < m_model->getTracksCount(); track++) {
            if (m_model->getTrackById(m_model->getTrackIndexFromPosition(track))->getProperty("kdenlive:audio_track").toInt() == 0) {
                // This is a video track
                Mlt::Transition t(*m_model->m_tractor->profile(),
                                  mode == 1 ? "composite" : TransitionsRepository::get()->getCompositingTransition().toUtf8().constData());
                t.set("always_active", 1);
                t.set("a_track", 0);
                t.set("b_track", track + 1);
                if (mode == 1) {
                    t.set("valign", "middle");
                    t.set("halign", "centre");
                    t.set("fill", 1);
                    t.set("geometry", compositeGeometry.toUtf8().constData());
                }
                t.set("internal_added", 237);
                field->plant_transition(t, 0, track + 1);
            }
        }
    }
    field->unlock();
    delete field;
    pCore->requestMonitorRefresh();
}

void TimelineController::extractZone(QPoint zone, bool liftOnly)
{
    QVector<int> tracks;
    if (audioTarget() >= 0) {
        tracks << audioTarget();
    }
    if (videoTarget() >= 0) {
        tracks << videoTarget();
    }
    if (tracks.isEmpty()) {
        tracks << m_activeTrack;
    }
    if (m_zone == QPoint()) {
        // Use current timeline position and clip zone length
        zone.setY(timelinePosition() + zone.y() - zone.x());
        zone.setX(timelinePosition());
    }
    TimelineFunctions::extractZone(m_model, tracks, m_zone == QPoint() ? zone : m_zone, liftOnly);
}

void TimelineController::extract(int clipId)
{
    // TODO: grouped clips?
    int in = m_model->getClipPosition(clipId);
    QPoint zone(in, in + m_model->getClipPlaytime(clipId));
    int track = m_model->getClipTrackId(clipId);
    TimelineFunctions::extractZone(m_model, QVector<int>() << track, zone, false);
}

int TimelineController::insertZone(const QString &binId, QPoint zone, bool overwrite)
{
    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(binId);
    int aTrack = -1;
    int vTrack = -1;
    if (clip->hasAudio()) {
        aTrack = audioTarget();
    }
    if (clip->hasVideo()) {
        vTrack = videoTarget();
    }
    if (aTrack == -1 && vTrack == -1) {
        // No target tracks defined, use active track
        if (m_model->getTrackById_const(m_activeTrack)->isAudioTrack()) {
            aTrack = m_activeTrack;
            vTrack = m_model->getMirrorVideoTrackId(aTrack);
        } else {
            vTrack = m_activeTrack;
            aTrack = m_model->getMirrorAudioTrackId(vTrack);
        }
    }
    int insertPoint;
    QPoint sourceZone;
    if (useRuler() && m_zone != QPoint()) {
        // We want to use timeline zone for in/out insert points
        insertPoint = m_zone.x();
        sourceZone = QPoint(zone.x(), zone.x() + m_zone.y() - m_zone.x());
    } else {
        // Use curent timeline pos and clip zone for in/out
        insertPoint = timelinePosition();
        sourceZone = zone;
    }
    QList <int> target_tracks;
    if (vTrack > -1) {
        target_tracks << vTrack;
    }
    if (aTrack > -1) {
        target_tracks << aTrack;
    }
    return TimelineFunctions::insertZone(m_model, target_tracks, binId, insertPoint, sourceZone, overwrite) ? insertPoint + (sourceZone.y() - sourceZone.x()) : -1;
}

void TimelineController::updateClip(int clipId, QVector<int> roles)
{
    QModelIndex ix = m_model->makeClipIndexFromID(clipId);
    if (ix.isValid()) {
        m_model->dataChanged(ix, ix, roles);
    }
}

void TimelineController::showClipKeyframes(int clipId, bool value)
{
    TimelineFunctions::showClipKeyframes(m_model, clipId, value);
}

void TimelineController::showCompositionKeyframes(int clipId, bool value)
{
    TimelineFunctions::showCompositionKeyframes(m_model, clipId, value);
}

void TimelineController::switchEnableState(int clipId)
{
    TimelineFunctions::switchEnableState(m_model, clipId);
}

void TimelineController::addCompositionToClip(const QString &assetId, int clipId)
{
    int position = m_model->getClipPosition(clipId);
    int track = m_model->getClipTrackId(clipId);
    insertNewComposition(track, position, assetId, true);
}

void TimelineController::addEffectToClip(const QString &assetId, int clipId)
{
    m_model->addClipEffect(clipId, assetId);
}

bool TimelineController::splitAV()
{
    int cid = m_selection.selectedItems.first();
    if (m_model->isClip(cid)) {
        std::shared_ptr<ClipModel> clip = m_model->getClipPtr(cid);
        if (clip->clipState() == PlaylistState::AudioOnly) {
            return TimelineFunctions::requestSplitVideo(m_model, cid, videoTarget());
        } else {
            return TimelineFunctions::requestSplitAudio(m_model, cid, audioTarget());
        }
    }
    pCore->displayMessage(i18n("No clip found to perform AV split operation"), InformationMessage, 500);
    return false;
}

void TimelineController::splitAudio(int clipId)
{
    TimelineFunctions::requestSplitAudio(m_model, clipId, audioTarget());
}

void TimelineController::splitVideo(int clipId)
{
    TimelineFunctions::requestSplitVideo(m_model, clipId, videoTarget());
}

void TimelineController::setAudioRef(int clipId)
{
    m_audioRef = clipId;
    std::unique_ptr<AudioEnvelope> envelope(new AudioEnvelope(getClipBinId(clipId), clipId));
    m_audioCorrelator.reset(new AudioCorrelation(std::move(envelope)));
    connect(m_audioCorrelator.get(), &AudioCorrelation::gotAudioAlignData, [&] (int cid, int shift) {
        int pos = m_model->getClipPosition(m_audioRef) + shift + m_model->getClipIn(m_audioRef);
        bool result = m_model->requestClipMove(cid, m_model->getClipTrackId(cid), pos, true, true);
        if (!result) {
            pCore->displayMessage(i18n("Cannot move clip to frame %1.", (pos + shift)), InformationMessage, 500);
        }
    });
    connect(m_audioCorrelator.get(), &AudioCorrelation::displayMessage, pCore.get(), &Core::displayMessage);
}

void TimelineController::alignAudio(int clipId)
{
    // find other clip
    if (m_audioRef == -1 || m_audioRef == clipId) {
        pCore->displayMessage(i18n("Set audio reference before attempting to align"), InformationMessage, 500);
        return;
    }
    const QString masterBinClipId = getClipBinId(m_audioRef);
    if (m_model->m_groups->isInGroup(clipId)) {
        std::unordered_set<int> groupIds = m_model->getGroupElements(clipId);
        // Check that no item is grouped with our audioRef item
        // TODO
        clearSelection();
    }
    const QString otherBinId = getClipBinId(clipId);
    if (otherBinId == masterBinClipId) {
        // easy, same clip.
        int newPos = m_model->getClipPosition(m_audioRef) - m_model->getClipIn(m_audioRef) + m_model->getClipIn(clipId);
        if (newPos) {
            bool result = m_model->requestClipMove(clipId, m_model->getClipTrackId(clipId), newPos, true, true);
            if (!result) {
                pCore->displayMessage(i18n("Cannot move clip to frame %1.", newPos), InformationMessage, 500);
            }
            return;
        }
    }
    // Perform audio calculation
    AudioEnvelope *envelope = new AudioEnvelope(getClipBinId(clipId), clipId, m_model->getClipIn(clipId),  m_model->getClipPlaytime(clipId), m_model->getClipPosition(clipId));
    m_audioCorrelator->addChild(envelope);
}

void TimelineController::switchTrackLock(bool applyToAll)
{
    if (!applyToAll) {
        // apply to active track only
        bool locked = m_model->getTrackById_const(m_activeTrack)->getProperty("kdenlive:locked_track").toInt() == 1;
        m_model->setTrackProperty(m_activeTrack, QStringLiteral("kdenlive:locked_track"), locked ? QStringLiteral("0") : QStringLiteral("1"));
    } else {
        // Invert track lock
        // Get track states first
        QMap<int, bool> trackLockState;
        int unlockedTracksCount = 0;
        int tracksCount = m_model->getTracksCount();
        for (int track = tracksCount - 1; track >= 0; track--) {
            int trackIx = m_model->getTrackIndexFromPosition(track);
            bool isLocked = m_model->getTrackById_const(trackIx)->getProperty("kdenlive:locked_track").toInt() == 1;
            if (!isLocked) {
                unlockedTracksCount++;
            }
            trackLockState.insert(trackIx, isLocked);
        }
        if (unlockedTracksCount == tracksCount) {
            // do not lock all tracks, leave active track unlocked
            trackLockState.insert(m_activeTrack, true);
        }
        QMapIterator<int, bool> i(trackLockState);
        while (i.hasNext()) {
            i.next();
            m_model->setTrackProperty(i.key(), QStringLiteral("kdenlive:locked_track"), i.value() ? QStringLiteral("0") : QStringLiteral("1"));
        }
    }
}

void TimelineController::switchTargetTrack()
{
    bool isAudio = m_model->getTrackById_const(m_activeTrack)->getProperty("kdenlive:audio_track").toInt() == 1;
    if (isAudio) {
        setAudioTarget(audioTarget() == m_activeTrack ? -1 : m_activeTrack);
    } else {
        setVideoTarget(videoTarget() == m_activeTrack ? -1 : m_activeTrack);
    }
}

int TimelineController::audioTarget() const
{
    return m_model->m_audioTarget;
}

int TimelineController::videoTarget() const
{
    return m_model->m_videoTarget;
}

void TimelineController::resetTrackHeight()
{
    int tracksCount = m_model->getTracksCount();
    for (int track = tracksCount - 1; track >= 0; track--) {
        int trackIx = m_model->getTrackIndexFromPosition(track);
        m_model->getTrackById(trackIx)->setProperty(QStringLiteral("kdenlive:trackheight"), QString::number(KdenliveSettings::trackheight()));
    }
    QModelIndex modelStart = m_model->makeTrackIndexFromID(m_model->getTrackIndexFromPosition(0));
    QModelIndex modelEnd = m_model->makeTrackIndexFromID(m_model->getTrackIndexFromPosition(tracksCount - 1));
    m_model->dataChanged(modelStart, modelEnd, {TimelineModel::HeightRole});
}

int TimelineController::groupClips(const QList<int> &clipIds)
{
    std::unordered_set<int> theSet(clipIds.begin(), clipIds.end());
    return m_model->requestClipsGroup(theSet, false, GroupType::Selection);
}

bool TimelineController::ungroupClips(int clipId)
{
    return m_model->requestClipUngroup(clipId);
}

void TimelineController::clearSelection()
{
    if (m_model->m_temporarySelectionGroup >= 0) {
        m_model->m_groups->destructGroupItem(m_model->m_temporarySelectionGroup);
        m_model->m_temporarySelectionGroup = -1;
    }
    m_selection.selectedItems.clear();
    emit selectionChanged();
}

void TimelineController::selectAll()
{
    QList<int> ids;
    for (auto clp : m_model->m_allClips) {
        ids << clp.first;
    }
    for (auto clp : m_model->m_allCompositions) {
        ids << clp.first;
    }
    setSelection(ids);
}

void TimelineController::selectCurrentTrack()
{
    QList<int> ids;
    for (auto clp : m_model->getTrackById_const(m_activeTrack)->m_allClips) {
        ids << clp.first;
    }
    for (auto clp : m_model->getTrackById_const(m_activeTrack)->m_allCompositions) {
        ids << clp.first;
    }
    setSelection(ids);
}

void TimelineController::pasteEffects(int targetId)
{
    QVariant returnedValue;
    QMetaObject::invokeMethod(m_root, "getCopiedItemId", Q_RETURN_ARG(QVariant, returnedValue));
    int sourceId = returnedValue.toInt();
    if (targetId == -1 && !m_selection.selectedItems.isEmpty()) {
        targetId = m_selection.selectedItems.constFirst();
    }
    if (!m_model->isClip(targetId) || !m_model->isClip(sourceId)) {
        return;
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    std::shared_ptr<EffectStackModel> sourceStack = m_model->getClipEffectStackModel(sourceId);
    std::shared_ptr<EffectStackModel> destStack = m_model->getClipEffectStackModel(targetId);
    bool result = destStack->importEffects(sourceStack, m_model->m_allClips[targetId]->clipState(), undo, redo);
    if (result) {
        pCore->pushUndo(undo, redo, i18n("Paste effects"));
    } else {
        pCore->displayMessage(i18n("Cannot paste effect on selected clip"), InformationMessage, 500);
        undo();
    }
}

double TimelineController::fps() const
{
    return pCore->getCurrentFps();
}

void TimelineController::editItemDuration(int id)
{
    int start = m_model->getItemPosition(id);
    int in = 0;
    int duration = m_model->getItemPlaytime(id);
    int maxLength = -1;
    bool isComposition = false;
    if (m_model->isClip(id)) {
        in = m_model->getClipIn(id);
        std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(getClipBinId(id));
        if (clip && clip->hasLimitedDuration()) {
            maxLength = clip->getProducerDuration();
        }
    } else if (m_model->isComposition(id)) {
        // nothing to do
        isComposition = true;
    } else {
        pCore->displayMessage(i18n("No item to edit"), InformationMessage, 500);
        return;
    }
    int trackId = m_model->getItemTrackId(id);
    int maxFrame = qMax(0, start + duration + (isComposition ? m_model->getTrackById(trackId)->getBlankSizeNearComposition(id, true) : m_model->getTrackById(trackId)->getBlankSizeNearClip(id, true)));
    int minFrame = qMax(0, in - (isComposition ? m_model->getTrackById(trackId)->getBlankSizeNearComposition(id, false) : m_model->getTrackById(trackId)->getBlankSizeNearClip(id, false)));
    int partner = isComposition ? -1 : m_model->getClipSplitPartner(id);
    QPointer<ClipDurationDialog> dialog =
        new ClipDurationDialog(id, pCore->currentDoc()->timecode(), start, minFrame, in, in + duration, maxLength, maxFrame, qApp->activeWindow());
    if (dialog->exec() == QDialog::Accepted) {
        std::function<bool(void)> undo = []() { return true; };
        std::function<bool(void)> redo = []() { return true; };
        int newPos = dialog->startPos().frames(pCore->getCurrentFps());
        int newIn = dialog->cropStart().frames(pCore->getCurrentFps());
        int newDuration = dialog->duration().frames(pCore->getCurrentFps());
        bool result = true;
        if (newPos < start) {
            if (!isComposition) {
                result = m_model->requestClipMove(id, trackId, newPos, true, true, undo, redo);
                if (result && partner > -1) {
                    result = m_model->requestClipMove(partner, m_model->getItemTrackId(partner), newPos, true, true, undo, redo);
                }
            } else {
                result = m_model->requestCompositionMove(id, trackId, newPos, m_model->m_allCompositions[id]->getForcedTrack(), true, undo, redo);
            }
            if (result && newIn != in) {
                m_model->requestItemResize(id, duration + (in - newIn), false, true, undo, redo);
                if (result && partner > -1) {
                    result = m_model->requestItemResize(partner, duration + (in - newIn), false, true, undo, redo);
                }
            }
            if (newDuration != duration + (in - newIn)) {
                result = result && m_model->requestItemResize(id, newDuration, true, true, undo, redo);
                if (result && partner > -1) {
                    result = m_model->requestItemResize(partner, newDuration, false, true, undo, redo);
                }
            }
        } else {
            // perform resize first
            if (newIn != in) {
                result = m_model->requestItemResize(id, duration + (in - newIn), false, true, undo, redo);
                if (result && partner > -1) {
                    result = m_model->requestItemResize(partner, duration + (in - newIn), false, true, undo, redo);
                }
            }
            if (newDuration != duration + (in - newIn)) {
                result = result && m_model->requestItemResize(id, newDuration, start == newPos, true, undo, redo);
                if (result && partner > -1) {
                    result = m_model->requestItemResize(partner, newDuration, start == newPos, true, undo, redo);
                }
            }
            if (start != newPos || newIn != in) {
                if (!isComposition) {
                    result = result && m_model->requestClipMove(id, trackId, newPos, true, true, undo, redo);
                    if (result && partner > -1) {
                        result = m_model->requestClipMove(partner, m_model->getItemTrackId(partner), newPos, true, true, undo, redo);
                    }
                } else {
                    result = result && m_model->requestCompositionMove(id, trackId, newPos, m_model->m_allCompositions[id]->getForcedTrack(), true, undo, redo);
                }
            }
        }
        if (result) {
            pCore->pushUndo(undo, redo, i18n("Edit item"));
        } else {
            undo();
        }
    }
}

void TimelineController::updateClipActions()
{
    if (m_selection.selectedItems.isEmpty()) {
        for(QAction *act : clipActions) {
            act->setEnabled(false);
        }
        return;
    }
    std::shared_ptr<ClipModel> clip(nullptr);
    int item = m_selection.selectedItems.first();
    if (m_model->isClip(item)) {
        clip = m_model->getClipPtr(item);
    }
    for(QAction *act : clipActions) {
        bool enableAction = true;
        const QChar actionData = act->data().toChar();
        if (actionData == QLatin1Char('G')) {
            enableAction = m_model->isInMultiSelection(item);
        } else if (actionData == QLatin1Char('U')) {
            enableAction = m_model->m_groups->isInGroup(item) && !m_model->isInMultiSelection(item);
        } else if (actionData == QLatin1Char('A')) {
            enableAction = clip && clip->clipState() == PlaylistState::AudioOnly;
        } else if (actionData == QLatin1Char('V')) {
            enableAction = clip && clip->clipState() == PlaylistState::VideoOnly;
        } else if (actionData == QLatin1Char('D')) {
            enableAction = clip && clip->clipState() == PlaylistState::Disabled;
        } else if (actionData == QLatin1Char('E')) {
            enableAction = clip && clip->clipState() != PlaylistState::Disabled;
        } else if (actionData == QLatin1Char('X') || actionData == QLatin1Char('S')) {
            enableAction = clip && clip->canBeVideo() && clip->canBeAudio();
            if (enableAction && actionData == QLatin1Char('S')) {
                act->setText(clip->clipState() == PlaylistState::AudioOnly ? i18n("Split video") : i18n("Split audio"));
            }
        } else if (actionData == QLatin1Char('C') && clip == nullptr) {
            enableAction = false;
        }
        act->setEnabled(enableAction);
    }
}

const QString TimelineController::getAssetName(const QString &assetId, bool isTransition)
{
    return isTransition ? TransitionsRepository::get()->getName(assetId) : EffectsRepository::get()->getName(assetId);
}

void TimelineController::grabCurrent()
{
    if (m_selection.selectedItems.isEmpty()) {
        //TODO: error displayMessage
        return;
    }
    int id = m_selection.selectedItems.constFirst();
    if (m_model->isClip(id)) {
        std::shared_ptr<ClipModel> clip = m_model->getClipPtr(id);
        clip->setGrab(!clip->isGrabbed());
        QModelIndex ix = m_model->makeClipIndexFromID(id);
        if (ix.isValid()) {
            m_model->dataChanged(ix, ix, {TimelineItemModel::GrabbedRole});
        }
    } else if (m_model->isComposition(id)) {
        std::shared_ptr<CompositionModel> clip = m_model->getCompositionPtr(id);
        clip->setGrab(!clip->isGrabbed());
        QModelIndex ix = m_model->makeCompositionIndexFromID(id);
        if (ix.isValid()) {
            m_model->dataChanged(ix, ix, {TimelineItemModel::GrabbedRole});
        }
    }
}

bool TimelineController::endFakeMove(int clipId, int position, bool updateView, bool logUndo, bool invalidateTimeline)
{
    Q_ASSERT(m_model->m_allClips.count(clipId) > 0);
    int trackId = m_model->m_allClips[clipId]->getFakeTrackId();
    if (m_model->getClipPosition(clipId) == position && m_model->getClipTrackId(clipId) == trackId) {
        qDebug()<<"* * ** END FAKE; NO MOVE RQSTED";
        return true;
    }
    if (m_model->m_groups->isInGroup(clipId)) {
        // element is in a group.
        int old_trackId = m_model->getItemTrackId(clipId);
        int groupId = m_model->m_groups->getRootId(clipId);
        int current_trackId = m_model->getClipTrackId(clipId);
        int track_pos1 = m_model->getTrackPosition(trackId);
        int track_pos2 = m_model->getTrackPosition(current_trackId);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_model->m_allClips[clipId]->getPosition();
        return endFakeGroupMove(clipId, groupId, delta_track, delta_pos, updateView, logUndo);
    }
    qDebug()<<"//////\n//////\nENDING FAKE MNOVE: "<<trackId<<", POS: "<<position;
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    int duration = m_model->getClipPlaytime(clipId);
    int currentTrack = m_model->m_allClips[clipId]->getCurrentTrackId();
    bool res = true;
    if (currentTrack > -1) {
        res = res & m_model->getTrackById(currentTrack)->requestClipDeletion(clipId, updateView, invalidateTimeline, undo, redo);
    }
    if (m_model->m_editMode == TimelineMode::OverwriteEdit) {
        res = res & TimelineFunctions::liftZone(m_model, trackId, QPoint(position, position + duration), undo, redo);
    } else if (m_model->m_editMode == TimelineMode::InsertEdit) {
        int startClipId = m_model->getClipByPosition(trackId, position);
        if (startClipId > -1) {
            // There is a clip, cut
            res = res & TimelineFunctions::requestClipCut(m_model, startClipId, position, undo, redo);
        }
        res = res & TimelineFunctions::insertSpace(m_model, trackId, QPoint(position, position + duration), undo, redo);
    }
    res = res & m_model->getTrackById(trackId)->requestClipInsertion(clipId, position, updateView, invalidateTimeline, undo, redo);
    if (res) {
        if (logUndo) {
            pCore->pushUndo(undo, redo, i18n("Move item"));
        }
    } else {
        qDebug()<<"//// FAKE FAILED";
        undo();
    }
    return res;
}

bool TimelineController::endFakeGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, bool logUndo)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = endFakeGroupMove(clipId, groupId, delta_track, delta_pos, updateView, logUndo, undo, redo);
    if (res && logUndo) {
        pCore->pushUndo(undo, redo, i18n("Move group"));
    }
    return res;
}

bool TimelineController::endFakeGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, bool finalMove, Fun &undo, Fun &redo)
{
    Q_ASSERT(m_model->m_allGroups.count(groupId) > 0);
    bool ok = true;
    auto all_items = m_model->m_groups->getLeaves(groupId);
    Q_ASSERT(all_items.size() > 1);
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };

    // Sort clips. We need to delete from right to left to avoid confusing the view
    std::vector<int> sorted_clips(all_items.begin(), all_items.end());
    std::sort(sorted_clips.begin(), sorted_clips.end(), [this](int clipId1, int clipId2) {
        int p1 = m_model->isClip(clipId1) ? m_model->m_allClips[clipId1]->getPosition() : m_model->m_allCompositions[clipId1]->getPosition();
        int p2 = m_model->isClip(clipId2) ? m_model->m_allClips[clipId2]->getPosition() : m_model->m_allCompositions[clipId2]->getPosition();
        return p2 <= p1;
    });

    // Moving groups is a two stage process: first we remove the clips from the tracks, and then try to insert them back at their calculated new positions.
    // This way, we ensure that no conflict will arise with clips inside the group being moved

    // First, remove clips
    int audio_delta, video_delta;
    audio_delta = video_delta = delta_track;
    int master_trackId = m_model->getItemTrackId(clipId);
    if (m_model->getTrackById_const(master_trackId)->isAudioTrack()) {
        // Master clip is audio, so reverse delta for video clips
        video_delta = -delta_track;
    } else {
        audio_delta = -delta_track;
    }
    int min = -1;
    int max = -1;
    std::unordered_map<int, int> old_track_ids, old_position, old_forced_track, new_track_ids;
    for (int item : sorted_clips) {
        int old_trackId = m_model->getItemTrackId(item);
        old_track_ids[item] = old_trackId;
        if (old_trackId != -1) {
            bool updateThisView = true;
            if (m_model->isClip(item)) {
                int current_track_position = m_model->getTrackPosition(old_trackId);
                int d = m_model->getTrackById_const(old_trackId)->isAudioTrack() ? audio_delta : video_delta;
                int target_track_position = current_track_position + d;
                auto it = m_model->m_allTracks.cbegin();
                std::advance(it, target_track_position);
                int target_track = (*it)->getId();
                new_track_ids[item] = target_track;
                old_position[item] = m_model->m_allClips[item]->getPosition();
                int duration = m_model->m_allClips[item]->getPlaytime();
                min = min < 0 ? old_position[item] + delta_pos : qMin(min, old_position[item] + delta_pos);
                max = max < 0 ? old_position[item] + delta_pos + duration : qMax(max, old_position[item] + delta_pos + duration);
                ok = ok && m_model->getTrackById(old_trackId)->requestClipDeletion(item, updateThisView, finalMove, undo, redo);
            } else {
                //ok = ok && getTrackById(old_trackId)->requestCompositionDeletion(item, updateThisView, local_undo, local_redo);
                old_position[item] = m_model->m_allCompositions[item]->getPosition();
                old_forced_track[item] = m_model->m_allCompositions[item]->getForcedTrack();
            }
            if (!ok) {
                bool undone = undo();
                Q_ASSERT(undone);
                return false;
            }
        }
    }
    bool res = true;
    if (m_model->m_editMode == TimelineMode::OverwriteEdit) {
        for (int item : sorted_clips) {
            if (m_model->isClip(item) && new_track_ids.count(item) > 0) {
                int target_track = new_track_ids[item];
                int target_position = old_position[item] + delta_pos;
                int duration = m_model->m_allClips[item]->getPlaytime();
                res = res & TimelineFunctions::liftZone(m_model, target_track, QPoint(target_position, target_position + duration), undo, redo);
            }
        }
    } else if (m_model->m_editMode == TimelineMode::InsertEdit) {
        QList <int> processedTracks;
        for (int item : sorted_clips) {
            int target_track = new_track_ids[item];
            if (processedTracks.contains(target_track)) {
                // already processed
                continue;
            }
            processedTracks << target_track;
            int target_position = min;
            int startClipId = m_model->getClipByPosition(target_track, target_position);
            if (startClipId > -1) {
                // There is a clip, cut
                res = res & TimelineFunctions::requestClipCut(m_model, startClipId, target_position, undo, redo);
            }
        }
        res = res & TimelineFunctions::insertSpace(m_model, -1, QPoint(min, max), undo, redo);
    }
    for (int item : sorted_clips) {
        if (m_model->isClip(item)) {
            int target_track = new_track_ids[item];
            int target_position = old_position[item] + delta_pos;
            ok = ok && m_model->requestClipMove(item, target_track, target_position, updateView, finalMove, undo, redo);
        } else {
            //ok = ok && requestCompositionMove(item, target_track, old_forced_track[item], target_position, updateThisView, local_undo, local_redo);
        }
        if (!ok) {
            bool undone = undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    return true;
}

QStringList TimelineController::getThumbKeys()
{
    QStringList result;
    for (auto clp : m_model->m_allClips) {
        const QString binId = getClipBinId(clp.first);
        std::shared_ptr<ProjectClip> binClip = pCore->bin()->getBinClip(binId);
        result << binClip->hash() + QLatin1Char('#') + QString::number(clp.second->getIn()) + QStringLiteral(".png");
        result << binClip->hash() + QLatin1Char('#') + QString::number(clp.second->getOut()) + QStringLiteral(".png");
    }
    result.removeDuplicates();
    return result;
}

bool TimelineController::isInSelection(int itemId)
{
    return m_model->isInMultiSelection(itemId);
}

void TimelineController::slotMultitrackView(bool enable)
{
    TimelineFunctions::enableMultitrackView(m_model, enable);
}
