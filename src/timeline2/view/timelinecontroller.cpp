/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "timelinecontroller.h"
#include "../model/timelinefunctions.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "audiomixer/mixermanager.hpp"
#include "bin/bin.h"
#include "bin/clipcreator.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "bin/model/markersortmodel.h"
#include "bin/model/subtitlemodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "dialogs/importsubtitle.h"
#include "dialogs/managesubtitles.h"
#include "dialogs/spacerdialog.h"
#include "dialogs/speechdialog.h"
#include "dialogs/speeddialog.h"
#include "dialogs/timeremap.h"
#include "doc/kdenlivedoc.h"
#include "effects/effectsrepository.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "glaxnimatelauncher.h"
#include "kdenlivesettings.h"
#include "lib/audio/audioEnvelope.h"
#include "mainwindow.h"
#include "monitor/monitormanager.h"
#include "previewmanager.h"
#include "project/projectmanager.h"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/model/compositionmodel.hpp"
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/model/snapmodel.hpp"
#include "timeline2/model/trackmodel.hpp"
#include "timeline2/view/dialogs/clipdurationdialog.h"
#include "timeline2/view/dialogs/trackdialog.h"
#include "timeline2/view/timelinewidget.h"
#include "transitions/transitionsrepository.hpp"

#include <KColorScheme>
#include <KMessageBox>
#include <KRecentDirs>
#include <KUrlRequesterDialog>
#include <QClipboard>
#include <QFontDatabase>
#include <QQuickItem>
#include <kio_version.h>

#include <QtMath>

#include <memory>
#include <unistd.h>

TimelineController::TimelineController(QObject *parent)
    : QObject(parent)
    , multicamIn(-1)
    , m_duration(0)
    , m_root(nullptr)
    , m_usePreview(false)
    , m_audioRef(-1)
    , m_zone(-1, -1)
    , m_activeTrack(-1)
    , m_scale(QFontMetrics(QApplication::font()).maxWidth() / 250)
    , m_ready(false)
    , m_snapStackIndex(-1)
    , m_effectZone({0, 0})
    , m_autotrackHeight(KdenliveSettings::autotrackheight())
{
    m_disablePreview = pCore->currentDoc()->getAction(QStringLiteral("disable_preview"));
    connect(m_disablePreview, &QAction::triggered, this, &TimelineController::disablePreview);
    m_disablePreview->setEnabled(false);
    connect(pCore.get(), &Core::autoScrollChanged, this, &TimelineController::autoScrollChanged);
    connect(pCore.get(), &Core::refreshActiveGuides, this, [this]() { m_activeSnaps.clear(); });
    connect(pCore.get(), &Core::autoTrackHeight, this, [this](bool enable) {
        m_autotrackHeight = enable;
        Q_EMIT autotrackHeightChanged();
    });
}

TimelineController::~TimelineController() {}

void TimelineController::prepareClose()
{
    // Clear root so we don't call its methods anymore
    QObject::disconnect(m_deleteConnection);
    disconnect(this, &TimelineController::selectionChanged, this, &TimelineController::updateClipActions);
    disconnect(m_model.get(), &TimelineModel::selectionChanged, this, &TimelineController::selectionChanged);
    disconnect(this, &TimelineController::videoTargetChanged, this, &TimelineController::updateVideoTarget);
    disconnect(this, &TimelineController::audioTargetChanged, this, &TimelineController::updateAudioTarget);
    disconnect(m_model.get(), &TimelineModel::selectedMixChanged, this, &TimelineController::showMixModel);
    disconnect(m_model.get(), &TimelineModel::selectedMixChanged, this, &TimelineController::selectedMixChanged);
    m_ready = false;
    m_root = nullptr;
    // Delete timeline preview before resetting model so that removing clips from timeline doesn't invalidate
    m_model->resetPreviewManager();
    m_model.reset();
}

void TimelineController::setModel(std::shared_ptr<TimelineItemModel> model)
{
    m_zone = QPoint(-1, -1);
    m_hasAudioTarget = 0;
    m_lastVideoTarget = -1;
    m_lastAudioTarget.clear();
    m_usePreview = false;
    m_model = model;
    m_activeSnaps.clear();
    connect(m_model.get(), &TimelineItemModel::requestClearAssetView, pCore.get(), &Core::clearAssetPanel);
    m_deleteConnection = connect(m_model.get(), &TimelineItemModel::checkItemDeletion, this, [this](int id) {
        if (m_ready) {
            QMetaObject::invokeMethod(m_root, "checkDeletion", Qt::QueuedConnection, Q_ARG(QVariant, id));
        }
    });
    connect(m_model.get(), &TimelineItemModel::showTrackEffectStack, this, [&](int tid) {
        if (tid > -1) {
            showTrackAsset(tid);
        } else {
            showMasterEffects();
        }
    });
    if (m_model->hasTimelinePreview()) {
        // this timeline model already contains a timeline preview, connect it
        connectPreviewManager();
    }
    connect(m_model.get(), &TimelineModel::connectPreviewManager, this, &TimelineController::connectPreviewManager);
    connect(m_model.get(), &TimelineModel::selectionModeChanged, this, &TimelineController::colorsChanged);
    connect(this, &TimelineController::selectionChanged, this, &TimelineController::updateClipActions);
    connect(this, &TimelineController::selectionChanged, this, &TimelineController::updateTrimmingMode);
    connect(this, &TimelineController::videoTargetChanged, this, &TimelineController::updateVideoTarget);
    connect(this, &TimelineController::audioTargetChanged, this, &TimelineController::updateAudioTarget);
    connect(m_model.get(), &TimelineItemModel::requestMonitorRefresh, [&]() { pCore->refreshProjectMonitorOnce(); });
    connect(m_model.get(), &TimelineModel::durationUpdated, this, &TimelineController::checkDuration);
    connect(m_model.get(), &TimelineModel::selectionChanged, this, &TimelineController::selectionChanged);
    connect(m_model.get(), &TimelineModel::selectedMixChanged, this, &TimelineController::showMixModel);
    connect(m_model.get(), &TimelineModel::selectedMixChanged, this, &TimelineController::selectedMixChanged);
    connect(m_model.get(), &TimelineModel::dataChanged, this, &TimelineController::checkClipPosition);
    connect(m_model.get(), &TimelineModel::checkTrackDeletion, this, &TimelineController::checkTrackDeletion, Qt::DirectConnection);
    connect(m_model.get(), &TimelineModel::flashLock, this, &TimelineController::slotFlashLock);
    connect(m_model.get(), &TimelineModel::refreshClipActions, this, &TimelineController::updateClipActions);
    connect(m_model.get(), &TimelineModel::highlightSub, this,
            [this](int index) { QMetaObject::invokeMethod(m_root, "highlightSub", Qt::QueuedConnection, Q_ARG(QVariant, index)); });
    if (m_model->hasSubtitleModel()) {
        loadSubtitleIndex();
    }
    connect(m_model.get(), &TimelineItemModel::subtitleModelInitialized, this, &TimelineController::loadSubtitleIndex);
    connect(m_model.get(), &TimelineItemModel::subtitlesListChanged, this, &TimelineController::subtitlesListChanged);
}

void TimelineController::loadSubtitleIndex()
{
    int currentIx = pCore->currentDoc()->getSequenceProperty(m_model->uuid(), QStringLiteral("kdenlive:activeSubtitleIndex"), QStringLiteral("0")).toInt();
    auto subtitleModel = m_model->getSubtitleModel();
    QMap<std::pair<int, QString>, QString> currentSubs = subtitleModel->getSubtitlesList();
    QMapIterator<std::pair<int, QString>, QString> i(currentSubs);
    int counter = 0;
    while (i.hasNext()) {
        i.next();
        if (i.key().first == currentIx) {
            m_activeSubPosition = counter;
            break;
        }
        counter++;
    }
    Q_EMIT activeSubtitlePositionChanged();
}

void TimelineController::restoreTargetTracks()
{
    setTargetTracks(m_hasVideoTarget, m_model->m_binAudioTargets);
}

void TimelineController::setTargetTracks(bool hasVideo, const QMap<int, QString> &audioTargets)
{
    if (m_model->isLoading) {
        // Timeline is still being build
        return;
    }
    int videoTrack = -1;
    m_model->m_binAudioTargets = audioTargets;
    QMap<int, int> audioTracks;
    m_hasVideoTarget = hasVideo;
    m_hasAudioTarget = audioTargets.size();
    if (m_hasVideoTarget) {
        videoTrack = m_model->getFirstVideoTrackIndex();
    }
    if (m_hasAudioTarget > 0) {
        if (m_lastAudioTarget.count() == audioTargets.count()) {
            // Use existing track targets
            QList<int> audioStreams = audioTargets.keys();
            QMapIterator<int, int> st(m_lastAudioTarget);
            while (st.hasNext()) {
                st.next();
                audioTracks.insert(st.key(), audioStreams.takeLast());
            }
        } else {
            // Use audio tracks from the first
            QVector<int> tracks;
            auto it = m_model->m_allTracks.cbegin();
            while (it != m_model->m_allTracks.cend()) {
                if ((*it)->isAudioTrack()) {
                    tracks << (*it)->getId();
                }
                ++it;
            }
            if (KdenliveSettings::multistream_checktrack() && audioTargets.count() > tracks.count()) {
                pCore->bin()->checkProjectAudioTracks(QString(), audioTargets.count());
            }
            QMapIterator<int, QString> st(audioTargets);
            while (st.hasNext()) {
                st.next();
                if (tracks.isEmpty()) {
                    break;
                }
                audioTracks.insert(tracks.takeLast(), st.key());
            }
        }
    }
    Q_EMIT hasAudioTargetChanged();
    Q_EMIT hasVideoTargetChanged();
    setVideoTarget(m_hasVideoTarget && (m_lastVideoTarget > -1) ? m_lastVideoTarget : videoTrack);
    setAudioTarget(audioTracks);
}

std::shared_ptr<TimelineItemModel> TimelineController::getModel() const
{
    return m_model;
}

void TimelineController::setRoot(QQuickItem *root)
{
    m_root = root;
    m_ready = true;
}

Mlt::Tractor *TimelineController::tractor()
{
    return m_model->tractor();
}

Mlt::Producer TimelineController::trackProducer(int tid)
{
    return *(m_model->getTrackById(tid).get());
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
    if (m_root) {
        m_root->setProperty("zoomOnMouse", zoomOnMouse ? qMax(0, getMousePos()) : -1);
        m_scale = scale;
        Q_EMIT scaleFactorChanged();
    } else {
        qWarning() << "Timeline root not created, impossible to zoom in";
    }
}

void TimelineController::setScaleFactor(double scale)
{
    m_scale = scale;
    // Update mainwindow's zoom slider
    Q_EMIT updateZoom(scale);
    // inform qml
    Q_EMIT scaleFactorChanged();
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
        Q_EMIT durationChanged(m_duration);
    }
}

void TimelineController::hideTrack(int trackId, bool hide)
{
    bool isAudio = m_model->isAudioTrack(trackId);
    QString state = hide ? (isAudio ? "1" : "2") : "3";
    QString previousState = m_model->getTrackProperty(trackId, QStringLiteral("hide")).toString();
    Fun undo_lambda = [this, trackId, previousState]() {
        m_model->setTrackProperty(trackId, QStringLiteral("hide"), previousState);
        m_model->updateDuration();
        return true;
    };
    Fun redo_lambda = [this, trackId, state]() {
        m_model->setTrackProperty(trackId, QStringLiteral("hide"), state);
        m_model->updateDuration();
        return true;
    };
    redo_lambda();
    pCore->pushUndo(undo_lambda, redo_lambda, state == QLatin1String("3") ? i18n("Hide Track") : i18n("Enable Track"));
}

int TimelineController::selectedTrack() const
{
    std::unordered_set<int> sel = m_model->getCurrentSelection();
    if (sel.empty()) return -1;
    std::vector<std::pair<int, int>> selected_tracks; // contains pairs of (track position, track id) for each selected item
    for (int s : sel) {
        int tid = m_model->getItemTrackId(s);
        selected_tracks.emplace_back(m_model->getTrackPosition(tid), tid);
    }
    // sort by track position
    std::sort(selected_tracks.begin(), selected_tracks.begin(), [](const auto &a, const auto &b) { return a.first < b.first; });
    return selected_tracks.front().second;
}

bool TimelineController::selectCurrentItem(KdenliveObjectType type, bool select, bool addToCurrent, bool showErrorMsg)
{
    int currentClip = -1;
    if (m_activeTrack == -1 || (m_model->isSubtitleTrack(m_activeTrack) && type != KdenliveObjectType::TimelineClip)) {
        // Cannot select item
    } else if (type == KdenliveObjectType::TimelineClip) {
        currentClip = m_model->isSubtitleTrack(m_activeTrack) ? m_model->getSubtitleByPosition(pCore->getMonitorPosition())
                                                              : m_model->getClipByPosition(m_activeTrack, pCore->getMonitorPosition());
    } else if (type == KdenliveObjectType::TimelineComposition) {
        currentClip = m_model->getCompositionByPosition(m_activeTrack, pCore->getMonitorPosition());
    } else if (type == KdenliveObjectType::TimelineMix) {
        if (m_activeTrack >= 0) {
            currentClip = m_model->getClipByPosition(m_activeTrack, pCore->getMonitorPosition());
        }
        if (currentClip > -1) {
            if (m_model->hasClipEndMix(currentClip)) {
                int mixPartner = m_model->getTrackById_const(m_activeTrack)->getSecondMixPartner(currentClip);
                int clipEnd = m_model->getClipPosition(currentClip) + m_model->getClipPlaytime(currentClip);
                int mixStart = clipEnd - m_model->getMixDuration(mixPartner);
                if (mixStart < pCore->getMonitorPosition() && pCore->getMonitorPosition() < clipEnd) {
                    if (select) {
                        m_model->requestMixSelection(mixPartner);
                        return true;
                    } else if (selectedMix() == mixPartner) {
                        m_model->requestClearSelection();
                        return true;
                    }
                }
            }
            int delta = pCore->getMonitorPosition() - m_model->getClipPosition(currentClip);
            if (m_model->getMixDuration(currentClip) >= delta) {
                if (select) {
                    m_model->requestMixSelection(currentClip);
                    return true;
                } else if (selectedMix() == currentClip) {
                    m_model->requestClearSelection();
                    return true;
                }
                return true;
            } else {
                currentClip = -1;
            }
        }
    }

    if (currentClip == -1) {
        if (showErrorMsg) {
            pCore->displayMessage(i18n("No item under timeline cursor in active track"), ErrorMessage, 500);
        }
        return false;
    }
    if (!select) {
        m_model->requestRemoveFromSelection(currentClip);
    } else {
        bool grouped = m_model->m_groups->isInGroup(currentClip);
        m_model->requestAddToSelection(currentClip, !addToCurrent);
        if (grouped) {
            // If part of a group, ensure the effect/composition stack displays the selected item's properties
            showAsset(currentClip);
        }
    }
    return true;
}

QList<int> TimelineController::selection() const
{
    if (!m_root) return QList<int>();
    std::unordered_set<int> sel = m_model->getCurrentSelection();
    QList<int> items;
    for (int id : sel) {
        items << id;
    }
    return items;
}

int TimelineController::selectedMix() const
{
    return m_model->m_selectedMix;
}

void TimelineController::selectItems(const QList<int> &ids)
{
    std::unordered_set<int> ids_s(ids.begin(), ids.end());
    m_model->requestSetSelection(ids_s);
}

void TimelineController::setScrollPos(int pos)
{
    if (pos > 0 && m_root) {
        QMetaObject::invokeMethod(m_root, "setScrollPos", Qt::QueuedConnection, Q_ARG(QVariant, pos));
    }
}

void TimelineController::resetView()
{
    m_model->_resetView();
    if (m_root) {
        QMetaObject::invokeMethod(m_root, "updatePalette");
    }
    Q_EMIT colorsChanged();
}

bool TimelineController::snap()
{
    return KdenliveSettings::snaptopoints();
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
        position = pCore->getMonitorPosition();
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
        position = pCore->getMonitorPosition();
    }
    TimelineFunctions::requestMultipleClipsInsertion(m_model, binIds, tid, position, clipIds, logUndo, refreshView);
    // we don't need to check the return value of the above function, in case of failure it will return an empty list of ids.
    return clipIds;
}

void TimelineController::insertNewMix(int tid, int position, const QString &transitionId)
{
    int clipId = m_model->getTrackById_const(tid)->getClipByPosition(position);
    if (clipId > 0) {
        m_model->mixClip(clipId, transitionId, -1);
    }
}

int TimelineController::insertNewCompositionAtPos(int tid, int position, const QString &transitionId)
{
    // TODO: adjust position and duration to existing clips ?
    return insertComposition(tid, position, transitionId, true);
}

int TimelineController::insertNewComposition(int tid, int clipId, int offset, const QString &transitionId, bool logUndo)
{
    int id;
    int minimumPos = clipId > -1 ? m_model->getClipPosition(clipId) : offset;
    int clip_duration = clipId > -1 ? m_model->getClipPlaytime(clipId) : pCore->getDurationFromString(KdenliveSettings::transition_duration());
    int endPos = minimumPos + clip_duration;
    int position = minimumPos;
    int duration = qMin(clip_duration, pCore->getDurationFromString(KdenliveSettings::transition_duration()));
    int lowerVideoTrackId = m_model->getPreviousVideoTrackIndex(tid);
    bool revert = offset > clip_duration / 2;
    int bottomId = 0;
    if (lowerVideoTrackId > 0) {
        bottomId = m_model->getTrackById_const(lowerVideoTrackId)->getClipByPosition(position + offset);
    }
    if (bottomId <= 0) {
        // No video track underneath
        if (offset < duration && duration < 2 * clip_duration) {
            // Composition dropped close to start, keep default composition duration
        } else if (clip_duration - offset < duration * 1.2 && duration < 2 * clip_duration) {
            // Composition dropped close to end, keep default composition duration
            position = endPos - duration;
        } else {
            // Use full clip length for duration
            duration = m_model->getTrackById_const(tid)->suggestCompositionLength(position);
        }
    } else {
        duration = qMin(duration, m_model->getTrackById_const(tid)->suggestCompositionLength(position));
        QPair<int, int> bottom(m_model->m_allClips[bottomId]->getPosition(), m_model->m_allClips[bottomId]->getPlaytime());
        if (bottom.first > minimumPos) {
            // Lower clip is after top clip
            if (position + offset > bottom.first) {
                int test_duration = m_model->getTrackById_const(tid)->suggestCompositionLength(bottom.first);
                if (test_duration > 0) {
                    offset -= (bottom.first - position);
                    position = bottom.first;
                    duration = test_duration;
                    revert = position > minimumPos;
                }
            }
        } else if (position >= bottom.first) {
            // Lower clip is before or at same pos as top clip
            int test_duration = m_model->getTrackById_const(lowerVideoTrackId)->suggestCompositionLength(position);
            if (test_duration > 0) {
                duration = qMin(test_duration, clip_duration);
            }
        }
    }
    int defaultLength = pCore->getDurationFromString(KdenliveSettings::transition_duration());
    bool isShortComposition = TransitionsRepository::get()->getType(transitionId) == AssetListType::AssetType::VideoShortComposition;
    if (duration < 0 || (isShortComposition && duration > 1.5 * defaultLength)) {
        duration = defaultLength;
    } else if (duration <= 1) {
        // if suggested composition duration is lower than 4 frames, use default
        duration = pCore->getDurationFromString(KdenliveSettings::transition_duration());
        if (minimumPos + clip_duration - position < 3) {
            position = minimumPos + clip_duration - duration;
        }
    }
    QPair<int, int> finalPos = m_model->getTrackById_const(tid)->validateCompositionLength(position, offset, duration, endPos);
    position = finalPos.first;
    duration = finalPos.second;

    std::unique_ptr<Mlt::Properties> props(nullptr);
    if (revert) {
        props = std::make_unique<Mlt::Properties>();
        if (transitionId == QLatin1String("dissolve")) {
            props->set("reverse", 1);
        } else if (transitionId == QLatin1String("composite")) {
            props->set("invert", 1);
        } else if (transitionId == QLatin1String("wipe")) {
            props->set("geometry", "0=0% 0% 100% 100% 100%;-1=0% 0% 100% 100% 0%");
        } else if (transitionId == QLatin1String("slide")) {
            props->set("rect", "0=0% 0% 100% 100% 100%;-1=100% 0% 100% 100% 100%");
        }
    }
    if (!m_model->requestCompositionInsertion(transitionId, tid, position, duration, std::move(props), id, logUndo)) {
        id = -1;
        pCore->displayMessage(i18n("Could not add composition at selected position"), ErrorMessage, 500);
    }
    return id;
}

int TimelineController::isOnCut(int cid) const
{
    Q_ASSERT(m_model->isComposition(cid));
    int tid = m_model->getItemTrackId(cid);
    return m_model->getTrackById_const(tid)->isOnCut(cid);
}

int TimelineController::insertComposition(int tid, int position, const QString &transitionId, bool logUndo)
{
    int id;
    int duration = pCore->getDurationFromString(KdenliveSettings::transition_duration());
    // Check if composition should be reversed (top clip at beginning, bottom at end)
    int a_track = m_model->getPreviousVideoTrackPos(tid);
    int topClip = m_model->getTrackById_const(tid)->getClipByPosition(position);
    int bottomClip = -1;
    if (a_track > 0) {
        // There is a video track below, check its clip
        int bottomTid = m_model->getTrackIndexFromPosition(a_track - 1);
        if (bottomTid > -1) {
            bottomClip = m_model->getTrackById_const(bottomTid)->getClipByPosition(position);
        }
    }
    bool reverse = false;
    if (topClip > -1 && bottomClip > -1) {
        if (m_model->getClipPosition(topClip) + m_model->getClipPlaytime(topClip) <
            m_model->getClipPosition(bottomClip) + m_model->getClipPlaytime(bottomClip)) {
            reverse = true;
        }
    }
    std::unique_ptr<Mlt::Properties> props(nullptr);
    if (reverse) {
        props = std::make_unique<Mlt::Properties>();
        if (transitionId == QLatin1String("dissolve")) {
            props->set("reverse", 1);
        } else if (transitionId == QLatin1String("composite")) {
            props->set("invert", 1);
        } else if (transitionId == QLatin1String("wipe")) {
            props->set("geometry", "0=0% 0% 100% 100% 100%;-1=0% 0% 100% 100% 0%");
        } else if (transitionId == QLatin1String("slide")) {
            props->set("rect", "0=0% 0% 100% 100% 100%;-1=100% 0% 100% 100% 100%");
        }
    }
    if (!m_model->requestCompositionInsertion(transitionId, tid, position, duration, std::move(props), id, logUndo)) {
        id = -1;
    }
    return id;
}

void TimelineController::slotFlashLock(int trackId)
{
    QMetaObject::invokeMethod(m_root, "animateLockButton", Qt::QueuedConnection, Q_ARG(QVariant, trackId));
}

void TimelineController::deleteSelectedClips()
{
    if (dragOperationRunning()) {
        // Don't allow timeline operation while drag in progress
        pCore->displayMessage(i18n("Cannot perform operation while dragging in timeline"), ErrorMessage);
        return;
    }
    auto sel = m_model->getCurrentSelection();
    // Check if we are operating on a locked track
    std::unordered_set<int> trackIds;
    for (auto &id : sel) {
        if (m_model->isItem(id)) {
            trackIds.insert(m_model->getItemTrackId(id));
        }
    }
    for (auto &tid : trackIds) {
        if (m_model->trackIsLocked(tid)) {
            m_model->flashLock(tid);
            return;
        }
    }
    if (sel.empty()) {
        // Check if a mix is selected
        if (m_model->m_selectedMix > -1 && m_model->isClip(m_model->m_selectedMix)) {
            m_model->removeMix(m_model->m_selectedMix);
            m_model->requestClearAssetView(m_model->m_selectedMix);
            m_model->requestClearSelection(true);
        }
        return;
    }
    // only need to delete the first item, the others will be deleted in cascade
    if (m_model->m_editMode == TimelineMode::InsertEdit) {
        // In insert mode, perform an extract operation (don't leave gaps)
        if (m_model->singleSelectionMode()) {
            // TODO only create 1 undo operation
            m_model->requestClearSelection();
            std::function<bool(void)> undo = []() { return true; };
            std::function<bool(void)> redo = []() { return true; };
            for (auto &s : sel) {
                // Remove item from group
                int clipToUngroup = s;
                std::unordered_set<int> clipsToRegroup = m_model->m_groups->getLeaves(m_model->m_groups->getRootId(s));
                clipsToRegroup.erase(clipToUngroup);
                int in = m_model->getClipPosition(s);
                int out = in + m_model->getClipPlaytime(s);
                int tid = m_model->getClipTrackId(s);
                std::pair<MixInfo, MixInfo> mixData = m_model->getTrackById_const(tid)->getMixInfo(s);
                if (mixData.first.firstClipId > -1) {
                    // Clip has a start mix, adjust in point
                    in += (mixData.first.firstClipInOut.second - mixData.first.secondClipInOut.first - mixData.first.mixOffset);
                }
                if (mixData.second.firstClipId > -1) {
                    // Clip has end mix, adjust out point
                    out -= mixData.second.mixOffset;
                }
                QVector<int> tracks = {tid};
                TimelineFunctions::extractZoneWithUndo(m_model, tracks, QPoint(in, out), false, clipToUngroup, clipsToRegroup, undo, redo);
            }
            pCore->pushUndo(undo, redo, i18n("Extract zone"));
        } else {
            extract(*sel.begin());
        }
    } else {
        m_model->requestItemDeletion(*sel.begin());
    }
}

int TimelineController::getMainSelectedItem(bool restrictToCurrentPos, bool allowComposition)
{
    auto sel = m_model->getCurrentSelection();
    if (sel.empty() || sel.size() > 2) {
        return -1;
    }
    int itemId = *(sel.begin());
    if (sel.size() == 2) {
        int parentGroup = m_model->m_groups->getRootId(itemId);
        if (parentGroup == -1 || m_model->m_groups->getType(parentGroup) != GroupType::AVSplit) {
            return -1;
        }
    }
    if (!restrictToCurrentPos) {
        if (m_model->isClip(itemId) || (allowComposition && m_model->isComposition(itemId))) {
            return itemId;
        }
    }
    if (m_model->isClip(itemId)) {
        int position = pCore->getMonitorPosition();
        int start = m_model->getClipPosition(itemId);
        int end = start + m_model->getClipPlaytime(itemId);
        if (position >= start && position <= end) {
            return itemId;
        }
    }
    return -1;
}

std::pair<int, int> TimelineController::selectionPosition(int *aTracks, int *vTracks)
{
    std::unordered_set<int> selectedIds = m_model->getCurrentSelection();
    if (selectedIds.empty()) {
        return {-1, -1};
    }
    int position = -1;
    int targetTrackId = -1;
    std::pair<int, int> audioTracks = {-1, -1};
    std::pair<int, int> videoTracks = {-1, -1};
    int topVideoWithSplit = -1;
    for (auto &id : selectedIds) {
        int tid = m_model->getItemTrackId(id);
        if (m_model->isSubtitleTrack(tid)) {
            // Subtitle track not supported
            continue;
        }
        if (position == -1 || position > m_model->getItemPosition(id)) {
            position = m_model->getItemPosition(id);
        }
        int trackPos = m_model->getTrackPosition(tid);
        if (m_model->isAudioTrack(tid)) {
            // Find audio track range
            if (audioTracks.first < 0 || trackPos < audioTracks.first) {
                audioTracks.first = trackPos;
            }
            if (audioTracks.second < 0 || trackPos > audioTracks.second) {
                audioTracks.second = trackPos;
            }
        } else {
            // Find video track range
            int splitId = m_model->m_groups->getSplitPartner(id);
            if (splitId > -1 && (topVideoWithSplit == -1 || trackPos > topVideoWithSplit)) {
                topVideoWithSplit = trackPos;
            }
            if (videoTracks.first < 0 || trackPos < videoTracks.first) {
                videoTracks.first = trackPos;
            }
            if (videoTracks.second < 0 || trackPos > videoTracks.second) {
                videoTracks.second = trackPos;
            }
        }
    }
    int minimumMirrorTracks = 0;
    if (topVideoWithSplit > -1) {
        // Ensure we have enough audio tracks for audio partners
        minimumMirrorTracks = topVideoWithSplit - videoTracks.first + 1;
    }

    if (videoTracks.first > -1) {
        *vTracks = videoTracks.second - videoTracks.first + 1;
        targetTrackId = m_model->getTrackIndexFromPosition(videoTracks.first);
    } else {
        *vTracks = 0;
    }
    if (audioTracks.first > -1) {
        *aTracks = qMax(audioTracks.second - audioTracks.first + 1, minimumMirrorTracks);
        if (targetTrackId == -1) {
            targetTrackId = m_model->getTrackIndexFromPosition(audioTracks.second);
        }
    } else {
        *aTracks = qMax(0, minimumMirrorTracks);
    }
    return {position, targetTrackId};
}

int TimelineController::copyItem()
{
    std::unordered_set<int> selectedIds = m_model->getCurrentSelection();
    if (selectedIds.empty()) {
        return -1;
    }
    int clipId = *(selectedIds.begin());
    QString copyString = TimelineFunctions::copyClips(m_model, selectedIds);
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(copyString);
    m_root->setProperty("copiedClip", clipId);
    return clipId;
}

std::pair<int, QString> TimelineController::getCopyItemData()
{
    std::unordered_set<int> selectedIds = m_model->getCurrentSelection();
    if (selectedIds.empty()) {
        return {-1, QString()};
    }
    int clipId = *(selectedIds.begin());
    QString copyString = TimelineFunctions::copyClips(m_model, selectedIds);
    return {clipId, copyString};
}

bool TimelineController::pasteItem(int position, int tid)
{
    QClipboard *clipboard = QApplication::clipboard();
    QString txt = clipboard->text();
    if (tid == -1) {
        tid = m_activeTrack;
    }
    if (position == -1) {
        position = getMenuOrTimelinePos();
    }
    return TimelineFunctions::pasteClips(m_model, txt, tid, position);
}

void TimelineController::triggerAction(const QString &name)
{
    pCore->triggerAction(name);
}

const QString TimelineController::actionText(const QString &name)
{
    return pCore->actionText(name);
}

QString TimelineController::timecode(int frames) const
{
    return KdenliveSettings::frametimecode() ? QString::number(frames) : m_model->tractor()->frames_to_time(frames, mlt_time_smpte_df);
}

QString TimelineController::framesToClock(int frames) const
{
    return m_model->tractor()->frames_to_time(frames, mlt_time_clock);
}

QString TimelineController::simplifiedTC(int frames) const
{
    if (KdenliveSettings::frametimecode()) {
        return QString::number(frames);
    }
    QString s = m_model->tractor()->frames_to_time(frames, mlt_time_smpte_df);
    return s.startsWith(QLatin1String("00:")) ? s.remove(0, 3) : s;
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

bool TimelineController::audioThumbNormalize() const
{
    return KdenliveSettings::normalizechannels();
}

bool TimelineController::showWaveforms() const
{
    return KdenliveSettings::audiothumbnails();
}

void TimelineController::beginAddTrack(int tid)
{
    if (tid == -1) {
        tid = m_activeTrack;
    }
    QScopedPointer<TrackDialog> d(new TrackDialog(m_model, tid, qApp->activeWindow()));
    if (d->exec() == QDialog::Accepted) {
        auto trackName = d->trackName();
        bool result =
            m_model->addTracksAtPosition(d->selectedTrackPosition(), d->tracksCount(), trackName, d->addAudioTrack(), d->addAVTrack(), d->addRecTrack());
        if (!result) {
            pCore->displayMessage(i18n("Could not insert track"), ErrorMessage, 500);
        }
    }
}

void TimelineController::deleteMultipleTracks(int tid)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    QScopedPointer<TrackDialog> d(new TrackDialog(m_model, tid, qApp->activeWindow(), true, m_activeTrack));
    if (d->exec() == QDialog::Accepted) {
        bool result = true;
        QList<int> allIds = d->toDeleteTrackIds();
        for (int selectedTrackIx : qAsConst(allIds)) {
            result = m_model->requestTrackDeletion(selectedTrackIx, undo, redo);
            if (!result) {
                break;
            }
            if (m_activeTrack == -1) {
                setActiveTrack(m_model->getTrackIndexFromPosition(m_model->getTracksCount() - 1));
            }
        }
        if (result) {
            pCore->pushUndo(undo, redo, allIds.count() > 1 ? i18n("Delete Tracks") : i18n("Delete Track"));
        } else {
            undo();
        }
    }
}

void TimelineController::switchTrackRecord(int tid, bool monitor)
{
    if (tid == -1) {
        tid = m_activeTrack;
    }
    if (!m_model->getTrackById_const(tid)->isAudioTrack()) {
        pCore->displayMessage(i18n("Select an audio track to display record controls"), ErrorMessage, 500);
    }
    int recDisplayed = m_model->getTrackProperty(tid, QStringLiteral("kdenlive:audio_rec")).toInt();
    if (monitor == false) {
        // Disable rec controls
        if (recDisplayed == 0) {
            // Already hidden
            return;
        }
        m_model->setTrackProperty(tid, QStringLiteral("kdenlive:audio_rec"), QStringLiteral("0"));
    } else {
        // Enable rec controls
        if (recDisplayed == 1) {
            // Already displayed
            return;
        }
        m_model->setTrackProperty(tid, QStringLiteral("kdenlive:audio_rec"), QStringLiteral("1"));
    }
    QModelIndex ix = m_model->makeTrackIndexFromID(tid);
    if (ix.isValid()) {
        Q_EMIT m_model->dataChanged(ix, ix, {TimelineModel::AudioRecordRole});
    }
}

void TimelineController::checkTrackDeletion(int selectedTrackIx)
{
    if (m_activeTrack == selectedTrackIx) {
        // Make sure we don't keep an index on a deleted track
        m_activeTrack = -1;
        Q_EMIT activeTrackChanged();
    }
    if (m_model->m_audioTarget.contains(selectedTrackIx)) {
        QMap<int, int> selection = m_model->m_audioTarget;
        selection.remove(selectedTrackIx);
        setAudioTarget(selection);
    }
    if (m_model->m_videoTarget == selectedTrackIx) {
        setVideoTarget(-1);
    }
    if (m_lastAudioTarget.contains(selectedTrackIx)) {
        m_lastAudioTarget.remove(selectedTrackIx);
        Q_EMIT lastAudioTargetChanged();
    }
    if (m_lastVideoTarget == selectedTrackIx) {
        m_lastVideoTarget = -1;
        Q_EMIT lastVideoTargetChanged();
    }
}

void TimelineController::showConfig(int page, int tab)
{
    Q_EMIT pCore->showConfigDialog((Kdenlive::ConfigPage)page, tab);
}

void TimelineController::gotoNextSnap()
{
    if (m_activeSnaps.empty() || pCore->undoIndex() != m_snapStackIndex) {
        m_snapStackIndex = pCore->undoIndex();
        m_activeSnaps.clear();
        m_activeSnaps = m_model->getGuideModel()->getSnapPoints();
        m_activeSnaps.push_back(m_zone.x());
        m_activeSnaps.push_back(m_zone.y() - 1);
    }
    std::vector<int> canceled = m_model->getFilteredGuideModel()->getIgnoredSnapPoints();
    int nextSnap = m_model->getNextSnapPos(pCore->getMonitorPosition(), m_activeSnaps, canceled);
    if (nextSnap > pCore->getMonitorPosition()) {
        setPosition(nextSnap);
    }
}

void TimelineController::gotoPreviousSnap()
{
    if (pCore->getMonitorPosition() > 0) {
        if (m_activeSnaps.empty() || pCore->undoIndex() != m_snapStackIndex) {
            m_snapStackIndex = pCore->undoIndex();
            m_activeSnaps.clear();
            m_activeSnaps = m_model->getGuideModel()->getSnapPoints();
            m_activeSnaps.push_back(m_zone.x());
            m_activeSnaps.push_back(m_zone.y() - 1);
        }
        std::vector<int> canceled = m_model->getFilteredGuideModel()->getIgnoredSnapPoints();
        setPosition(m_model->getPreviousSnapPos(pCore->getMonitorPosition(), m_activeSnaps, canceled));
    }
}

void TimelineController::gotoNextGuide()
{
    QList<CommentedTime> guides = m_model->getGuideModel()->getAllMarkers();
    std::vector<int> canceled = m_model->getFilteredGuideModel()->getIgnoredSnapPoints();
    int pos = pCore->getMonitorPosition();
    double fps = pCore->getCurrentFps();
    int guidePos = 0;
    for (auto &guide : guides) {
        guidePos = guide.time().frames(fps);
        if (std::find(canceled.begin(), canceled.end(), guidePos) != canceled.end()) {
            continue;
        }
        if (guidePos > pos) {
            setPosition(guidePos);
            return;
        }
    }
    setPosition(m_duration - 1);
}

void TimelineController::gotoPreviousGuide()
{
    if (pCore->getMonitorPosition() > 0) {
        QList<CommentedTime> guides = m_model->getGuideModel()->getAllMarkers();
        std::vector<int> canceled = m_model->getFilteredGuideModel()->getIgnoredSnapPoints();
        int pos = pCore->getMonitorPosition();
        double fps = pCore->getCurrentFps();
        int lastGuidePos = 0;
        int guidePos = 0;
        for (auto &guide : guides) {
            guidePos = guide.time().frames(fps);
            if (std::find(canceled.begin(), canceled.end(), guidePos) != canceled.end()) {
                continue;
            }
            if (guidePos >= pos) {
                setPosition(lastGuidePos);
                return;
            }
            lastGuidePos = guidePos;
        }
        setPosition(lastGuidePos);
    }
}

void TimelineController::groupSelection()
{
    if (dragOperationRunning()) {
        // Don't allow timeline operation while drag in progress
        pCore->displayMessage(i18n("Cannot perform operation while dragging in timeline"), ErrorMessage);
        return;
    }
    const auto selection = m_model->getCurrentSelection();
    if (selection.size() < 2) {
        pCore->displayMessage(i18n("Select at least 2 items to group"), ErrorMessage, 500);
        return;
    }
    m_model->requestClearSelection();
    m_model->requestClipsGroup(selection);
    m_model->requestSetSelection(selection);
}

void TimelineController::unGroupSelection(int cid)
{
    if (dragOperationRunning()) {
        // Don't allow timeline operation while drag in progress
        pCore->displayMessage(i18n("Cannot perform operation while dragging in timeline"), ErrorMessage);
        return;
    }
    auto ids = m_model->getCurrentSelection();
    // ask to unselect if needed
    m_model->requestClearSelection();
    if (cid > -1) {
        ids.insert(cid);
    }
    if (!ids.empty()) {
        m_model->requestClipsUngroup(ids);
    }
}

bool TimelineController::dragOperationRunning()
{
    QVariant returnedValue;
    QMetaObject::invokeMethod(m_root, "isDragging", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    return returnedValue.toBool();
}

bool TimelineController::trimmingActive()
{
    ToolType::ProjectTool tool = pCore->window()->getCurrentTimeline()->activeTool();
    return tool == ToolType::SlideTool || tool == ToolType::SlipTool || tool == ToolType::RippleTool || tool == ToolType::RollTool;
}

void TimelineController::setInPoint(bool ripple)
{
    if (dragOperationRunning()) {
        // Don't allow timeline operation while drag in progress
        pCore->displayMessage(i18n("Cannot perform operation while dragging in timeline"), ErrorMessage);
        qDebug() << "Cannot operate while dragging";
        return;
    }
    auto requestResize = [this, ripple](int id, int size) {
        if (ripple) {
            m_model->requestItemRippleResize(m_model, id, size, false, true, !KdenliveSettings::lockedGuides(), 0, false);
            setPosition(m_model->getItemPosition(id));
        } else {
            m_model->requestItemResize(id, size, false, true, 0, false);
        }
    };
    int cursorPos = pCore->getMonitorPosition();
    const auto selection = m_model->getCurrentSelection();
    bool selectionFound = false;
    if (!selection.empty()) {
        for (int id : selection) {
            int start = m_model->getItemPosition(id);
            if (start == cursorPos) {
                continue;
            }
            int size = start + m_model->getItemPlaytime(id) - cursorPos;
            requestResize(id, size);
            selectionFound = true;
        }
    }
    if (!selectionFound) {
        if (m_activeTrack >= 0) {
            int cid = m_model->getClipByPosition(m_activeTrack, cursorPos);
            if (cid < 0) {
                // Check first item after timeline position
                int maximumSpace = m_model->getTrackById_const(m_activeTrack)->getBlankEnd(cursorPos);
                if (maximumSpace < INT_MAX) {
                    cid = m_model->getClipByPosition(m_activeTrack, maximumSpace + 1);
                }
            }
            if (cid >= 0) {
                int start = m_model->getItemPosition(cid);
                if (start != cursorPos) {
                    int size = start + m_model->getItemPlaytime(cid) - cursorPos;
                    requestResize(cid, size);
                    selectionFound = true;
                }
            }
        } else if (m_model->isSubtitleTrack(m_activeTrack)) {
            // Subtitle track
            auto subtitleModel = m_model->getSubtitleModel();
            if (subtitleModel) {
                int sid = -1;
                std::unordered_set<int> sids = subtitleModel->getItemsInRange(cursorPos, cursorPos);
                if (sids.empty()) {
                    sids = subtitleModel->getItemsInRange(cursorPos, -1);
                    for (int s : sids) {
                        if (sid == -1 || subtitleModel->getStartPosForId(s) < subtitleModel->getStartPosForId(sid)) {
                            sid = s;
                        }
                    }
                } else {
                    sid = *sids.begin();
                }
                if (sid > -1) {
                    int start = m_model->getItemPosition(sid);
                    if (start != cursorPos) {
                        int size = start + m_model->getItemPlaytime(sid) - cursorPos;
                        requestResize(sid, size);
                        selectionFound = true;
                    }
                }
            }
        }
        if (!selectionFound) {
            pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
        }
    }
}

void TimelineController::setOutPoint(bool ripple)
{
    if (dragOperationRunning()) {
        // Don't allow timeline operation while drag in progress
        pCore->displayMessage(i18n("Cannot perform operation while dragging in timeline"), ErrorMessage);
        qDebug() << "Cannot operate while dragging";
        return;
    }
    auto requestResize = [this, ripple](int id, int size) {
        if (ripple) {
            m_model->requestItemRippleResize(m_model, id, size, true, true, !KdenliveSettings::lockedGuides(), 0, false);
        } else {
            m_model->requestItemResize(id, size, true, true, 0, false);
        }
    };
    int cursorPos = pCore->getMonitorPosition();
    const auto selection = m_model->getCurrentSelection();
    bool selectionFound = false;
    if (!selection.empty()) {
        for (int id : selection) {
            int start = m_model->getItemPosition(id);
            if (start + m_model->getItemPlaytime(id) == cursorPos) {
                continue;
            }
            int size = cursorPos - start;
            requestResize(id, size);
            selectionFound = true;
        }
    }
    if (!selectionFound) {
        if (m_activeTrack >= 0) {
            int cid = m_model->getClipByPosition(m_activeTrack, cursorPos);
            if (cid < 0 || cursorPos == m_model->getItemPosition(cid)) {
                // If no clip found at cursor pos or we are at the first frame of a clip, try to find previous clip
                // Check first item before timeline position
                // If we are at a clip start, check space before this clip
                int offset = cid >= 0 ? 1 : 0;
                int previousPos = m_model->getTrackById_const(m_activeTrack)->getBlankStart(cursorPos - offset);
                cid = m_model->getClipByPosition(m_activeTrack, qMax(0, previousPos - 1));
            }
            if (cid >= 0) {
                int start = m_model->getItemPosition(cid);
                if (start + m_model->getItemPlaytime(cid) != cursorPos) {
                    int size = cursorPos - start;
                    requestResize(cid, size);
                    selectionFound = true;
                }
            }
        } else if (m_model->isSubtitleTrack(m_activeTrack)) {
            // Subtitle track
            auto subtitleModel = m_model->getSubtitleModel();
            if (subtitleModel) {
                int sid = -1;
                std::unordered_set<int> sids = subtitleModel->getItemsInRange(cursorPos, cursorPos);
                if (sids.empty()) {
                    sids = subtitleModel->getItemsInRange(0, cursorPos);
                    for (int s : sids) {
                        if (sid == -1 || subtitleModel->getSubtitleEnd(s) > subtitleModel->getSubtitleEnd(sid)) {
                            sid = s;
                        }
                    }
                } else {
                    sid = *sids.begin();
                }
                if (sid > -1) {
                    int start = m_model->getItemPosition(sid);
                    if (start + m_model->getItemPlaytime(sid) != cursorPos) {
                        int size = cursorPos - start;
                        requestResize(sid, size);
                        selectionFound = true;
                    }
                }
            }
        }
        if (!selectionFound) {
            pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
        }
    }
}

void TimelineController::editMarker(int cid, int position)
{
    if (cid == -1) {
        cid = getMainSelectedClip();
        if (cid == -1) {
            pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
            return;
        }
    }
    Q_ASSERT(m_model->isClip(cid));
    double speed = m_model->getClipSpeed(cid);
    if (position == -1) {
        // Calculate marker position relative to timeline cursor
        position = pCore->getMonitorPosition() - m_model->getClipPosition(cid) + m_model->getClipIn(cid);
        position = int(position * speed);
    }
    if (position < (m_model->getClipIn(cid) * speed) || position > (m_model->getClipIn(cid) * speed + m_model->getClipPlaytime(cid))) {
        pCore->displayMessage(i18n("Cannot find clip to edit marker"), ErrorMessage, 500);
        return;
    }
    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(getClipBinId(cid));
    if (clip->getMarkerModel()->hasMarker(position)) {
        GenTime pos(position, pCore->getCurrentFps());
        clip->getMarkerModel()->editMarkerGui(pos, qApp->activeWindow(), false, clip.get());
    } else {
        pCore->displayMessage(i18n("Cannot find clip to edit marker"), ErrorMessage, 500);
    }
}

void TimelineController::addMarker(int cid, int position)
{
    if (cid == -1) {
        cid = getMainSelectedClip();
        if (cid == -1) {
            pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
            return;
        }
    }
    Q_ASSERT(m_model->isClip(cid));
    double speed = m_model->getClipSpeed(cid);
    if (position == -1) {
        // Calculate marker position relative to timeline cursor
        position = pCore->getMonitorPosition() - m_model->getClipPosition(cid) + m_model->getClipIn(cid);
        position = int(position * speed);
    }
    if (position < (m_model->getClipIn(cid) * speed) || position > (m_model->getClipIn(cid) * speed + m_model->getClipPlaytime(cid))) {
        pCore->displayMessage(i18n("Cannot find clip to edit marker"), ErrorMessage, 500);
        return;
    }
    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(getClipBinId(cid));
    GenTime pos(position, pCore->getCurrentFps());
    clip->getMarkerModel()->editMarkerGui(pos, qApp->activeWindow(), true, clip.get());
}

int TimelineController::getMainSelectedClip()
{
    int clipId = m_root->property("mainItemId").toInt();
    if (clipId == -1 || !isInSelection(clipId)) {
        std::unordered_set<int> sel = m_model->getCurrentSelection();
        for (int i : sel) {
            if (m_model->isClip(i)) {
                clipId = i;
                break;
            }
        }
    }
    return m_model->isClip(clipId) ? clipId : -1;
}

void TimelineController::addQuickMarker(int cid, int position)
{
    if (cid == -1) {
        cid = getMainSelectedClip();
        if (cid == -1) {
            pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
            return;
        }
    }
    Q_ASSERT(m_model->isClip(cid));
    double speed = m_model->getClipSpeed(cid);
    if (position == -1) {
        // Calculate marker position relative to timeline cursor
        position = pCore->getMonitorPosition() - m_model->getClipPosition(cid);
        position = int(position * speed);
    }
    if (position < (m_model->getClipIn(cid) * speed) || position > ((m_model->getClipIn(cid) + m_model->getClipPlaytime(cid) * speed))) {
        pCore->displayMessage(i18n("Cannot find clip to edit marker"), ErrorMessage, 500);
        return;
    }
    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(getClipBinId(cid));
    GenTime pos(position, pCore->getCurrentFps());
    CommentedTime marker(pos, pCore->currentDoc()->timecode().getDisplayTimecode(pos, false), KdenliveSettings::default_marker_type());
    clip->getMarkerModel()->addMarker(marker.time(), marker.comment(), marker.markerType());
}

void TimelineController::deleteMarker(int cid, int position)
{
    if (cid == -1) {
        cid = getMainSelectedClip();
        if (cid == -1) {
            pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
            return;
        }
    }
    Q_ASSERT(m_model->isClip(cid));
    double speed = m_model->getClipSpeed(cid);
    if (position == -1) {
        // Calculate marker position relative to timeline cursor
        position = pCore->getMonitorPosition() - m_model->getClipPosition(cid) + m_model->getClipIn(cid);
        position = int(position * speed);
    }
    if (position < (m_model->getClipIn(cid) * speed) || position > (m_model->getClipIn(cid) * speed + m_model->getClipPlaytime(cid))) {
        pCore->displayMessage(i18n("Cannot find clip to edit marker"), ErrorMessage, 500);
        return;
    }
    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(getClipBinId(cid));
    GenTime pos(position, pCore->getCurrentFps());
    clip->getMarkerModel()->removeMarker(pos);
}

void TimelineController::deleteAllMarkers(int cid)
{
    if (cid == -1) {
        cid = getMainSelectedClip();
        if (cid == -1) {
            pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
            return;
        }
    }
    Q_ASSERT(m_model->isClip(cid));
    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(getClipBinId(cid));
    clip->getMarkerModel()->removeAllMarkers();
}

void TimelineController::editGuide(int frame)
{
    if (frame == -1) {
        frame = pCore->getMonitorPosition();
    }
    auto guideModel = m_model->getGuideModel();
    GenTime pos(frame, pCore->getCurrentFps());
    guideModel->editMarkerGui(pos, qApp->activeWindow(), false);
}

void TimelineController::moveGuideById(int id, int newFrame)
{
    if (newFrame < 0) {
        return;
    }
    auto guideModel = m_model->getGuideModel();
    GenTime newPos(newFrame, pCore->getCurrentFps());
    GenTime oldPos = guideModel->markerById(id).time();
    guideModel->editMarker(oldPos, newPos);
}

int TimelineController::moveGuideWithoutUndo(int mid, int newFrame)
{
    if (newFrame < 0) {
        return -1;
    }
    auto guideModel = m_model->getGuideModel();
    GenTime newPos(newFrame, pCore->getCurrentFps());
    if (guideModel->moveMarker(mid, newPos)) {
        return newFrame;
    }
    return -1;
}

bool TimelineController::moveGuidesInRange(int start, int end, int offset)
{
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool final = false;
    final = moveGuidesInRange(start, end, offset, undo, redo);
    if (final) {
        if (offset > 0) {
            pCore->pushUndo(undo, redo, i18n("Insert space"));
        } else {
            pCore->pushUndo(undo, redo, i18n("Remove space"));
        }
        return true;
    } else {
        undo();
    }
    return false;
}

bool TimelineController::moveGuidesInRange(int start, int end, int offset, Fun &undo, Fun &redo)
{
    GenTime fromPos(start, pCore->getCurrentFps());
    GenTime toPos(start + offset, pCore->getCurrentFps());
    QList<CommentedTime> guides = m_model->getGuideModel()->getMarkersInRange(start, end);
    return m_model->getGuideModel()->moveMarkers(guides, fromPos, toPos, undo, redo);
}

void TimelineController::switchGuide(int frame, bool deleteOnly, bool showGui)
{
    bool markerFound = false;
    if (frame == -1) {
        frame = pCore->getMonitorPosition();
    }
    qDebug() << "::: ADDING GUIDE TO MODEL: " << m_model->uuid();
    CommentedTime marker = m_model->getGuideModel()->getMarker(frame, &markerFound);
    if (!markerFound) {
        if (deleteOnly) {
            pCore->displayMessage(i18n("No guide found at current position"), ErrorMessage, 500);
            return;
        }
        GenTime pos(frame, pCore->getCurrentFps());

        if (showGui) {
            m_model->getGuideModel()->editMarkerGui(pos, qApp->activeWindow(), true);
        } else {
            m_model->getGuideModel()->addMarker(pos, i18n("guide"));
        }
    } else {
        m_model->getGuideModel()->removeMarker(marker.time());
    }
}

void TimelineController::addAsset(const QVariantMap &data)
{
    const auto selection = m_model->getCurrentSelection();
    if (!selection.empty()) {
        QString effect = data.value(QStringLiteral("kdenlive/effect")).toString();
        QVariantList effectSelection = m_model->addClipEffect(*selection.begin(), effect, false);
        if (effectSelection.isEmpty()) {
            QString effectName = EffectsRepository::get()->getName(effect);
            pCore->displayMessage(i18n("Cannot add effect %1 to selected clip", effectName), ErrorMessage, 500);
        } else if (KdenliveSettings::seekonaddeffect() && effectSelection.count() == 1) {
            // Move timeline cursor inside clip if it is not
            int cid = effectSelection.first().toInt();
            int in = m_model->getClipPosition(cid);
            int out = in + m_model->getClipPlaytime(cid);
            int position = pCore->getMonitorPosition();
            if (position < in || position > out) {
                Q_EMIT seeked(in);
            }
        }
    } else {
        pCore->displayMessage(i18n("Select a clip to apply an effect"), ErrorMessage, 500);
    }
}

void TimelineController::requestRefresh()
{
    pCore->refreshProjectMonitorOnce();
}

void TimelineController::showAsset(int id)
{
    if (m_model->isComposition(id)) {
        Q_EMIT showTransitionModel(id, m_model->getCompositionParameterModel(id));
    } else if (m_model->isClip(id)) {
        QModelIndex clipIx = m_model->makeClipIndexFromID(id);
        QString clipName = m_model->data(clipIx, Qt::DisplayRole).toString();
        bool showKeyframes = m_model->data(clipIx, TimelineModel::ShowKeyframesRole).toInt();
        qDebug() << "-----\n// SHOW KEYFRAMES: " << showKeyframes;
        Q_EMIT showItemEffectStack(clipName, m_model->getClipEffectStackModel(id), m_model->getClipFrameSize(id), showKeyframes);
    } else if (m_model->isSubTitle(id)) {
        qDebug() << "::: SHOWING SUBTITLE: " << id;
        Q_EMIT showSubtitle(id);
    }
}

void TimelineController::showTrackAsset(int trackId)
{
    Q_EMIT showItemEffectStack(getTrackNameFromIndex(trackId), m_model->getTrackEffectStackModel(trackId), pCore->getCurrentFrameSize(), false);
}

void TimelineController::adjustTrackHeight(int trackId, int height)
{
    if (trackId > -1) {
        m_model->getTrackById(trackId)->setProperty(QStringLiteral("kdenlive:trackheight"), QString::number(height));
        m_model->setTrackProperty(trackId, "kdenlive:collapsed", QStringLiteral("0"));
        QModelIndex modelStart = m_model->makeTrackIndexFromID(trackId);
        Q_EMIT m_model->dataChanged(modelStart, modelStart, {TimelineModel::HeightRole});
        return;
    }
}

void TimelineController::adjustAllTrackHeight(int trackId, int height)
{
    bool isAudio = m_model->getTrackById_const(trackId)->isAudioTrack();
    auto it = m_model->m_allTracks.cbegin();
    while (it != m_model->m_allTracks.cend()) {
        int target_track = (*it)->getId();
        if (target_track != trackId && m_model->getTrackById_const(target_track)->isAudioTrack() == isAudio) {
            m_model->getTrackById(target_track)->setProperty(QStringLiteral("kdenlive:trackheight"), QString::number(height));
        }
        ++it;
    }
    int tracksCount = m_model->getTracksCount();
    QModelIndex modelStart = m_model->makeTrackIndexFromID(m_model->getTrackIndexFromPosition(0));
    QModelIndex modelEnd = m_model->makeTrackIndexFromID(m_model->getTrackIndexFromPosition(tracksCount - 1));
    Q_EMIT m_model->dataChanged(modelStart, modelEnd, {TimelineModel::HeightRole});
}

void TimelineController::collapseAllTrackHeight(int trackId, bool collapse, int collapsedHeight)
{
    bool isAudio = m_model->getTrackById_const(trackId)->isAudioTrack();
    auto it = m_model->m_allTracks.cbegin();
    while (it != m_model->m_allTracks.cend()) {
        int target_track = (*it)->getId();
        if (m_model->getTrackById_const(target_track)->isAudioTrack() == isAudio) {
            if (collapse) {
                m_model->setTrackProperty(target_track, "kdenlive:collapsed", QString::number(collapsedHeight));
            } else {
                m_model->setTrackProperty(target_track, "kdenlive:collapsed", QStringLiteral("0"));
            }
        }
        ++it;
    }
    int tracksCount = m_model->getTracksCount();
    QModelIndex modelStart = m_model->makeTrackIndexFromID(m_model->getTrackIndexFromPosition(0));
    QModelIndex modelEnd = m_model->makeTrackIndexFromID(m_model->getTrackIndexFromPosition(tracksCount - 1));
    Q_EMIT m_model->dataChanged(modelStart, modelEnd, {TimelineModel::HeightRole});
}

void TimelineController::defaultTrackHeight(int trackId)
{
    if (trackId > -1) {
        m_model->getTrackById(trackId)->setProperty(QStringLiteral("kdenlive:trackheight"), QString::number(KdenliveSettings::trackheight()));
        QModelIndex modelStart = m_model->makeTrackIndexFromID(trackId);
        Q_EMIT m_model->dataChanged(modelStart, modelStart, {TimelineModel::HeightRole});
        return;
    }
    auto it = m_model->m_allTracks.cbegin();
    while (it != m_model->m_allTracks.cend()) {
        int target_track = (*it)->getId();
        m_model->getTrackById(target_track)->setProperty(QStringLiteral("kdenlive:trackheight"), QString::number(KdenliveSettings::trackheight()));
        ++it;
    }
    int tracksCount = m_model->getTracksCount();
    QModelIndex modelStart = m_model->makeTrackIndexFromID(m_model->getTrackIndexFromPosition(0));
    QModelIndex modelEnd = m_model->makeTrackIndexFromID(m_model->getTrackIndexFromPosition(tracksCount - 1));
    Q_EMIT m_model->dataChanged(modelStart, modelEnd, {TimelineModel::HeightRole});
}

void TimelineController::setPosition(int position)
{
    // Process seek request
    Q_EMIT seeked(position);
}

void TimelineController::setAudioTarget(const QMap<int, int> &tracks)
{
    // Clear targets before re-adding to trigger qml refresh
    m_model->m_audioTarget.clear();
    Q_EMIT audioTargetChanged();

    if ((!tracks.isEmpty() && !m_model->isTrack(tracks.firstKey())) || m_hasAudioTarget == 0) {
        return;
    }

    m_model->m_audioTarget = tracks;
    Q_EMIT audioTargetChanged();
}

void TimelineController::switchAudioTarget(int trackId)
{
    if (m_model->m_audioTarget.contains(trackId)) {
        m_model->m_audioTarget.remove(trackId);
    } else {
        // TODO: use track description
        if (m_model->m_binAudioTargets.count() == 1) {
            // Only one audio stream, remove previous and switch
            m_model->m_audioTarget.clear();
        }
        int ix = getFirstUnassignedStream();
        if (ix > -1) {
            m_model->m_audioTarget.insert(trackId, ix);
        }
    }
    Q_EMIT audioTargetChanged();
}

void TimelineController::assignCurrentTarget(int index)
{
    if (m_activeTrack == -1 || !m_model->isTrack(m_activeTrack)) {
        pCore->displayMessage(i18n("No active track"), ErrorMessage, 500);
        return;
    }
    bool isAudio = m_model->isAudioTrack(m_activeTrack);
    if (isAudio) {
        // Select audio target stream
        if (index >= 0 && index < m_model->m_binAudioTargets.size()) {
            // activate requested stream
            int stream = m_model->m_binAudioTargets.keys().at(index);
            assignAudioTarget(m_activeTrack, stream);
        } else {
            // Remove audio target
            m_model->m_audioTarget.remove(m_activeTrack);
            Q_EMIT audioTargetChanged();
        }
    } else {
        // Select video target stream
        setVideoTarget(m_activeTrack);
    }
}

void TimelineController::assignAudioTarget(int trackId, int stream)
{
    QList<int> assignedStreams = m_model->m_audioTarget.values();
    if (assignedStreams.contains(stream)) {
        // This stream was assigned to another track, remove
        m_model->m_audioTarget.remove(m_model->m_audioTarget.key(stream));
    }
    // Remove and re-add target track to trigger a refresh in qml track headers
    m_model->m_audioTarget.remove(trackId);
    Q_EMIT audioTargetChanged();

    m_model->m_audioTarget.insert(trackId, stream);
    Q_EMIT audioTargetChanged();
}

int TimelineController::getFirstUnassignedStream() const
{
    QList<int> keys = m_model->m_binAudioTargets.keys();
    QList<int> assigned = m_model->m_audioTarget.values();
    for (int k : qAsConst(keys)) {
        if (!assigned.contains(k)) {
            return k;
        }
    }
    return -1;
}

void TimelineController::setVideoTarget(int track)
{
    if ((track > -1 && !m_model->isTrack(track)) || !m_hasVideoTarget) {
        m_model->m_videoTarget = -1;
        return;
    }
    m_model->m_videoTarget = track;
    Q_EMIT videoTargetChanged();
}

void TimelineController::setActiveTrack(int track)
{
    if (track > -1 && !m_model->isTrack(track)) {
        return;
    }
    m_activeTrack = track;
    Q_EMIT activeTrackChanged();
}

void TimelineController::setZoneToSelection()
{
    std::unordered_set<int> selection = m_model->getCurrentSelection();
    QPoint zone(-1, -1);
    for (int cid : selection) {
        int inPos = m_model->getItemPosition(cid);
        int outPos = inPos + m_model->getItemPlaytime(cid);
        if (zone.x() == -1 || inPos < zone.x()) {
            zone.setX(inPos);
        }
        if (outPos > zone.y()) {
            zone.setY(outPos);
        }
    }
    if (zone.x() > -1 && zone.y() > -1) {
        updateZone(m_zone, zone, true);
    } else {
        pCore->displayMessage(i18n("No item selected in timeline"), ErrorMessage, 500);
    }
}

void TimelineController::setZone(const QPoint &zone, bool withUndo)
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
    updateZone(m_zone, zone, withUndo);
}

void TimelineController::updateZone(const QPoint oldZone, const QPoint newZone, bool withUndo)
{
    if (!withUndo) {
        m_zone = newZone;
        Q_EMIT zoneChanged();
        // Update monitor zone
        Q_EMIT zoneMoved(m_zone);
        return;
    }
    Fun undo_zone = [this, oldZone]() {
        setZone(oldZone, false);
        return true;
    };
    Fun redo_zone = [this, newZone]() {
        setZone(newZone, false);
        return true;
    };
    redo_zone();
    pCore->pushUndo(undo_zone, redo_zone, i18n("Set Zone"));
}

void TimelineController::updateEffectZone(const QPoint oldZone, const QPoint newZone, bool withUndo)
{
    Q_UNUSED(oldZone)
    Q_EMIT pCore->updateEffectZone(newZone, withUndo);
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
    Q_EMIT zoneChanged();
    // Update monitor zone
    Q_EMIT zoneMoved(m_zone);
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
    Q_EMIT zoneChanged();
    Q_EMIT zoneMoved(m_zone);
}

void TimelineController::selectItems(const QVariantList &tracks, int startFrame, int endFrame, bool addToSelect, bool selectBottomCompositions,
                                     bool selectSubTitles)
{
    std::unordered_set<int> itemsToSelect;
    if (addToSelect) {
        itemsToSelect = m_model->getCurrentSelection();
    }
    for (int i = 0; i < tracks.count(); i++) {
        if (m_model->getTrackById_const(tracks.at(i).toInt())->isLocked()) {
            continue;
        }
        auto currentClips = m_model->getItemsInRange(tracks.at(i).toInt(), startFrame, endFrame, i < tracks.count() - 1 ? true : selectBottomCompositions);
        itemsToSelect.insert(currentClips.begin(), currentClips.end());
    }
    if (selectSubTitles && m_model->hasSubtitleModel()) {
        auto currentSubs = m_model->getSubtitleModel()->getItemsInRange(startFrame, endFrame);
        itemsToSelect.insert(currentSubs.begin(), currentSubs.end());
    }
    m_model->requestSetSelection(itemsToSelect);
}

void TimelineController::requestClipCut(int clipId, int position)
{
    if (position == -1) {
        position = pCore->getMonitorPosition();
    }
    TimelineFunctions::requestClipCut(m_model, clipId, position);
}

void TimelineController::cutClipUnderCursor(int position, int track)
{
    if (position == -1) {
        position = pCore->getMonitorPosition();
    }
    QMutexLocker lk(&m_metaMutex);
    bool foundClip = false;
    const auto selection = m_model->getCurrentSelection();
    if (track == -1) {
        for (int cid : selection) {
            if ((m_model->isClip(cid) || m_model->isSubTitle(cid)) && positionIsInItem(cid)) {
                if (TimelineFunctions::requestClipCut(m_model, cid, position)) {
                    foundClip = true;
                    // Cutting clips in the selection group is handled in TimelineFunctions
                }
                break;
            }
        }
    }
    if (!foundClip) {
        if (track == -1) {
            track = m_activeTrack;
        }
        if (track != -1) {
            int cid = m_model->getClipByPosition(track, position);
            if (cid >= 0 && TimelineFunctions::requestClipCut(m_model, cid, position)) {
                foundClip = true;
            }
        }
    }
    if (!foundClip) {
        pCore->displayMessage(i18n("No clip to cut"), ErrorMessage, 500);
    }
}

void TimelineController::cutAllClipsUnderCursor(int position)
{
    if (position == -1) {
        position = pCore->getMonitorPosition();
    }
    QMutexLocker lk(&m_metaMutex);
    TimelineFunctions::requestClipCutAll(m_model, position);
}

int TimelineController::requestSpacerStartOperation(int trackId, int position)
{
    QMutexLocker lk(&m_metaMutex);
    std::pair<int, int> spacerOp = TimelineFunctions::requestSpacerStartOperation(m_model, trackId, position);
    int itemId = spacerOp.first;
    return itemId;
}

int TimelineController::spacerMinPos() const
{
    return TimelineFunctions::spacerMinPos();
}

void TimelineController::spacerMoveGuides(const QVector<int> &ids, int offset)
{
    m_model->getGuideModel()->moveMarkersWithoutUndo(ids, offset);
}

QVector<int> TimelineController::spacerSelection(int startFrame)
{
    return m_model->getGuideModel()->getMarkersIdInRange(startFrame, -1);
}

int TimelineController::getGuidePosition(int id)
{
    return m_model->getGuideModel()->getMarkerPos(id);
}

bool TimelineController::requestSpacerEndOperation(int clipId, int startPosition, int endPosition, int affectedTrack, const QVector<int> &selectedGuides,
                                                   int guideStart)
{
    QMutexLocker lk(&m_metaMutex);
    // Start undoable command
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    if (guideStart > -1) {
        // Move guides back to original position
        m_model->getGuideModel()->moveMarkersWithoutUndo(selectedGuides, startPosition - endPosition, false);
    }
    bool result = TimelineFunctions::requestSpacerEndOperation(m_model, clipId, startPosition, endPosition, affectedTrack, guideStart, undo, redo);
    return result;
}

void TimelineController::seekCurrentClip(bool seekToEnd)
{
    const auto selection = m_model->getCurrentSelection();
    int cid = -1;
    if (!selection.empty()) {
        cid = *selection.begin();
    } else {
        int cursorPos = pCore->getMonitorPosition();
        cid = m_model->getClipByPosition(m_activeTrack, cursorPos);
        if (cid < 0) {
            /* If the cursor is at the clip end it is one frame after the clip,
             * make it possible to jump to the clip start in that situation too
             */
            cid = m_model->getClipByPosition(m_activeTrack, cursorPos - 1);
        }
    }
    if (cid > -1) {
        seekToClip(cid, seekToEnd);
    }
}

void TimelineController::seekToClip(int cid, bool seekToEnd)
{
    int start = m_model->getItemPosition(cid);
    if (seekToEnd) {
        // -1 because to go to the end of a 10-frame clip,
        // need to go from frame 0 to frame 9 (10th frame)
        start += m_model->getItemPlaytime(cid) - 1;
    }
    setPosition(start);
}

void TimelineController::seekToMouse()
{
    int mousePos = getMousePos();
    if (mousePos > -1) {
        setPosition(mousePos);
    }
}

const QPoint TimelineController::getMousePosInTimeline() const
{
    QPoint mousPosInWidget = pCore->window()->getCurrentTimeline()->mapFromGlobal(QCursor::pos());
    return mousPosInWidget;
}

int TimelineController::getMousePos()
{
    QVariant returnedValue;
    int posInWidget = getMousePosInTimeline().x();
    QMetaObject::invokeMethod(m_root, "getMouseOffset", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    posInWidget += returnedValue.toInt();
    return posInWidget / m_scale;
}

int TimelineController::getMouseTrack()
{
    QVariant returnedValue;
    int posInWidget = getMousePosInTimeline().y();
    QMetaObject::invokeMethod(m_root, "getMouseTrackFromPos", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, posInWidget));
    return returnedValue.toInt();
}

bool TimelineController::positionIsInItem(int id)
{
    int in = m_model->getItemPosition(id);
    int position = pCore->getMonitorPosition();
    if (in > position) {
        return false;
    }
    if (position <= in + m_model->getItemPlaytime(id)) {
        return true;
    }
    return false;
}

void TimelineController::refreshItem(int id)
{
    if (m_model->isClip(id) && m_model->m_allClips[id]->isAudioOnly()) {
        return;
    }
    if (positionIsInItem(id)) {
        pCore->refreshProjectMonitorOnce();
    }
}

QPair<int, int> TimelineController::getAvTracksCount() const
{
    return m_model->getAVtracksCount();
}

QStringList TimelineController::extractCompositionLumas() const
{
    return m_model->extractCompositionLumas();
}

QStringList TimelineController::extractExternalEffectFiles() const
{
    return m_model->extractExternalEffectFiles();
}

void TimelineController::addEffectToCurrentClip(const QStringList &effectData)
{
    QList<int> activeClips;
    for (int track = m_model->getTracksCount() - 1; track >= 0; track--) {
        int trackIx = m_model->getTrackIndexFromPosition(track);
        int cid = m_model->getClipByPosition(trackIx, pCore->getMonitorPosition());
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
    if (initialDuration == -2) {
        // Add default fade
        duration = pCore->getDurationFromString(KdenliveSettings::fade_duration());
        initialDuration = 0;
    }
    if (duration <= 0) {
        // remove fade
        if (initialDuration > 0) {
            // Restore original fade duration
            m_model->adjustEffectLength(cid, effectId, initialDuration, -1);
        }
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
    int tid = m_model->getItemTrackId(itemId);
    setPosition(start);
    setActiveTrack(tid);
    Q_EMIT centerView();
}

int TimelineController::headerWidth() const
{
    return qMax(10, KdenliveSettings::headerwidth());
}

void TimelineController::setHeaderWidth(int width)
{
    KdenliveSettings::setHeaderwidth(width);
}

bool TimelineController::createSplitOverlay(int clipId, std::shared_ptr<Mlt::Filter> filter)
{
    if (m_model->hasTimelinePreview() && m_model->previewManager()->hasOverlayTrack()) {
        return true;
    }
    if (clipId == -1) {
        pCore->displayMessage(i18n("Select a clip to compare effect"), ErrorMessage, 500);
        return false;
    }
    std::shared_ptr<ClipModel> clip = m_model->getClipPtr(clipId);
    const QString binId = clip->binId();

    // Get clean bin copy of the clip
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(binId);
    std::shared_ptr<Mlt::Producer> binProd(binClip->masterProducer()->cut(clip->getIn(), clip->getOut()));

    // Get copy of timeline producer
    std::shared_ptr<Mlt::Producer> clipProducer(new Mlt::Producer(*clip));

    // Built tractor and compositing
    Mlt::Tractor trac(pCore->getProjectProfile());
    Mlt::Playlist play(pCore->getProjectProfile());
    Mlt::Playlist play2(pCore->getProjectProfile());
    play.append(*clipProducer.get());
    play2.append(*binProd);
    trac.set_track(play, 0);
    trac.set_track(play2, 1);
    play2.attach(*filter.get());
    QString splitTransition = TransitionsRepository::get()->getCompositingTransition();
    Mlt::Transition t(pCore->getProjectProfile(), splitTransition.toUtf8().constData());
    t.set("always_active", 1);
    trac.plant_transition(t, 0, 1);
    int startPos = m_model->getClipPosition(clipId);

    // plug in overlay playlist
    auto *overlay = new Mlt::Playlist(pCore->getProjectProfile());
    overlay->insert_blank(0, startPos);
    Mlt::Producer split(trac.get_producer());
    overlay->insert_at(startPos, &split, 1);

    // insert in tractor
    if (!m_model->hasTimelinePreview()) {
        initializePreview();
    }
    m_model->setOverlayTrack(overlay);
    return true;
}

void TimelineController::removeSplitOverlay()
{
    if (!m_model->hasTimelinePreview() || !m_model->previewManager()->hasOverlayTrack()) {
        return;
    }
    // disconnect
    m_model->removeOverlayTrack();
}

int TimelineController::requestItemRippleResize(int itemId, int size, bool right, bool logUndo, int snapDistance, bool allowSingleResize)
{
    return m_model->requestItemRippleResize(m_model, itemId, size, right, logUndo, !KdenliveSettings::lockedGuides(), snapDistance, allowSingleResize);
}

void TimelineController::updateTrimmingMode()
{
    if (trimmingActive()) {
        requestStartTrimmingMode();
    } else {
        requestEndTrimmingMode();
    }
}

int TimelineController::trimmingBoundOffset(int offset)
{
    std::shared_ptr<ClipModel> mainClip = m_model->getClipPtr(m_trimmingMainClip);
    return qBound(mainClip->getOut() - mainClip->getMaxDuration() + 1, offset, mainClip->getIn());
}

void TimelineController::slipPosChanged(int offset)
{
    if (!m_model->isClip(m_trimmingMainClip) || !pCore->monitorManager()->isTrimming()) {
        return;
    }
    std::shared_ptr<ClipModel> mainClip = m_model->getClipPtr(m_trimmingMainClip);
    offset = qBound(mainClip->getOut() - mainClip->getMaxDuration() + 1, offset, mainClip->getIn());
    int outPoint = mainClip->getOut() - offset;
    int inPoint = mainClip->getIn() - offset;

    pCore->monitorManager()->projectMonitor()->slotTrimmingPos(inPoint, offset, inPoint, outPoint);
    showToolTip(i18n("In:%1, Out:%2 (%3%4)", simplifiedTC(inPoint), simplifiedTC(outPoint), (offset < 0 ? "-" : "+"), simplifiedTC(qFabs(offset))));
}

void TimelineController::ripplePosChanged(int size, bool right)
{
    if (!m_model->isClip(m_trimmingMainClip) || !pCore->monitorManager()->isTrimming()) {
        return;
    }
    if (size < 0) {
        return;
    }
    qDebug() << "ripplePosChanged" << size << right;
    std::shared_ptr<ClipModel> mainClip = m_model->getClipPtr(m_trimmingMainClip);
    int delta = size - mainClip->getPlaytime();
    if (!right) {
        delta *= -1;
    }
    int pos = right ? mainClip->getOut() : mainClip->getIn();
    pos += delta;
    if (mainClip->getMaxDuration() > -1) {
        pos = qBound(0, pos, mainClip->getMaxDuration());
    } else {
        pos = qMax(0, pos);
    }
    pCore->monitorManager()->projectMonitor()->slotTrimmingPos(pos + 1, delta, right ? mainClip->getIn() : pos, right ? pos : mainClip->getOut());
}

bool TimelineController::slipProcessSelection(int mainClipId, bool addToSelection)
{
    std::unordered_set<int> sel = m_model->getCurrentSelection();
    std::unordered_set<int> newSel;

    for (int i : sel) {
        if (m_model->isClip(i) && m_model->getClipPtr(i)->getMaxDuration() != -1) {
            newSel.insert(i);
        }
    }

    if (mainClipId != -1 && !m_model->isClip(mainClipId) && m_model->getClipPtr(mainClipId)->getMaxDuration() == -1) {
        mainClipId = -1;
    }

    if ((newSel.empty() || !isInSelection(mainClipId)) && mainClipId != -1) {
        m_trimmingMainClip = mainClipId;
        Q_EMIT trimmingMainClipChanged();
        if (!addToSelection) {
            newSel.clear();
        }
        newSel.insert(mainClipId);
    }

    if (newSel != sel) {
        m_model->requestSetSelection(newSel);
        return false;
    }

    if (sel.empty()) {
        return false;
    }

    Q_ASSERT(!sel.empty());

    if (mainClipId == -1) {
        mainClipId = getMainSelectedClip();
    }

    if (m_model->getTrackById(m_model->getClipTrackId(mainClipId))->isLocked()) {
        int partnerId = m_model->m_groups->getSplitPartner(mainClipId);
        if (partnerId == -1 || m_model->getTrackById(m_model->getClipTrackId(partnerId))->isLocked()) {
            mainClipId = -1;
            for (int i : sel) {
                if (i != mainClipId && !m_model->getTrackById(m_model->getClipTrackId(i))->isLocked()) {
                    mainClipId = i;
                    break;
                }
            }
        } else {
            mainClipId = partnerId;
        }
    }

    if (mainClipId == -1) {
        pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
        return false;
    }

    std::shared_ptr<ClipModel> mainClip = m_model->getClipPtr(mainClipId);

    if (mainClip->getMaxDuration() == -1) {
        return false;
    }

    int partnerId = m_model->m_groups->getSplitPartner(mainClipId);

    if (mainClip->isAudioOnly() && partnerId != -1 && !m_model->getTrackById(m_model->getClipTrackId(partnerId))->isLocked()) {
        mainClip = m_model->getClipPtr(partnerId);
    }

    m_trimmingMainClip = mainClip->getId();
    Q_EMIT trimmingMainClipChanged();
    return true;
}

bool TimelineController::requestStartTrimmingMode(int mainClipId, bool addToSelection, bool right)
{

    if (pCore->monitorManager()->isTrimming() && m_trimmingMainClip == mainClipId) {
        return true;
    }

    if (pCore->activeTool() == ToolType::SlipTool && !slipProcessSelection(mainClipId, addToSelection)) {
        return false;
    }

    if (pCore->activeTool() == ToolType::RippleTool) {
        if (m_model.get()->isClip(mainClipId)) {
            m_trimmingMainClip = mainClipId;
            Q_EMIT trimmingMainClipChanged();
        } else {
            return false;
        }
    }

    std::shared_ptr<ClipModel> mainClip = m_model->getClipPtr(m_trimmingMainClip);

    const int previousClipId = m_model->getTrackById_const(mainClip->getCurrentTrackId())->getClipByPosition(mainClip->getPosition() - 1);
    std::shared_ptr<Mlt::Producer> previousFrame;
    if (pCore->activeTool() == ToolType::SlipTool && previousClipId > -1) {
        std::shared_ptr<ClipModel> previousClip = m_model->getClipPtr(previousClipId);
        previousFrame = std::shared_ptr<Mlt::Producer>(previousClip->getProducer()->cut(0));
        Mlt::Filter filter(pCore->getProjectProfile(), "freeze");
        filter.set("mlt_service", "freeze");
        filter.set("frame", previousClip->getOut());
        previousFrame->attach(filter);
    } else {
        previousFrame = std::shared_ptr<Mlt::Producer>(new Mlt::Producer(pCore->getProjectProfile(), "color:black"));
    }

    const int nextClipId = m_model->getTrackById_const(mainClip->getCurrentTrackId())->getClipByPosition(mainClip->getPosition() + mainClip->getPlaytime());
    std::shared_ptr<Mlt::Producer> nextFrame;
    if (pCore->activeTool() == ToolType::SlipTool && nextClipId > -1) {
        std::shared_ptr<ClipModel> nextClip = m_model->getClipPtr(nextClipId);
        nextFrame = std::shared_ptr<Mlt::Producer>(nextClip->getProducer()->cut(0));
        Mlt::Filter filter(pCore->getProjectProfile(), "freeze");
        filter.set("mlt_service", "freeze");
        filter.set("frame", nextClip->getIn());
        nextFrame->attach(filter);
    } else {
        nextFrame = std::shared_ptr<Mlt::Producer>(new Mlt::Producer(pCore->getProjectProfile(), "color:black"));
    }

    std::shared_ptr<Mlt::Producer> inOutFrame;
    if (pCore->activeTool() == ToolType::RippleTool) {
        inOutFrame = std::shared_ptr<Mlt::Producer>(mainClip->getProducer()->cut(0));
        Mlt::Filter filter(pCore->getProjectProfile(), "freeze");
        filter.set("mlt_service", "freeze");
        filter.set("frame", right ? mainClip->getIn() : mainClip->getOut());
        inOutFrame->attach(filter);
    }

    std::vector<std::shared_ptr<Mlt::Producer>> producers;
    int previewLength = 0;
    switch (pCore->activeTool()) {
    case ToolType::SlipTool:
        producers.push_back(std::shared_ptr<Mlt::Producer>(previousFrame));
        producers.push_back(std::shared_ptr<Mlt::Producer>(mainClip->getProducer()->cut(0)));
        producers.push_back(std::shared_ptr<Mlt::Producer>(mainClip->getProducer()->cut(mainClip->getOut() - mainClip->getIn())));
        producers.push_back(nextFrame);
        previewLength = producers[1]->get_length();
        break;
    case ToolType::SlideTool:
        break;
    case ToolType::RollTool:
        break;
    case ToolType::RippleTool:
        if (right) {
            producers.push_back(std::shared_ptr<Mlt::Producer>(inOutFrame));
        }
        producers.push_back(std::shared_ptr<Mlt::Producer>(mainClip->getProducer()->cut(0)));
        if (!right) {
            producers.push_back(std::shared_ptr<Mlt::Producer>(inOutFrame));
            previewLength = producers[0]->get_length();
        } else {
            previewLength = producers[1]->get_length();
        }
        break;
    default:
        return false;
    }

    // Built tractor
    Mlt::Tractor trac(pCore->getProjectProfile());

    // Now that we know the length of the preview create and add black background producer
    std::shared_ptr<Mlt::Producer> black(new Mlt::Producer(pCore->getProjectProfile(), "color:black"));
    black->set("length", previewLength);
    black->set_in_and_out(0, previewLength);
    black->set("mlt_image_format", "rgba");
    trac.set_track(*black.get(), 0);
    // trac.set_track( 1);

    if (!mainClip->isAudioOnly()) {
        int count = 1; // 0 is background track so we start at 1
        for (auto const &producer : producers) {
            trac.set_track(*producer.get(), count);
            count++;
        }

        // Add "composite" transitions for multi clip view
        for (int i = 0; i < int(producers.size()); i++) {
            // Construct transition
            Mlt::Transition transition(pCore->getProjectProfile(), "composite");
            transition.set("mlt_service", "composite");
            transition.set("a_track", 0);
            transition.set("b_track", i + 1);
            transition.set("distort", 0);
            transition.set("aligned", 0);
            // 200 is an arbitrary number so we can easily remove these transition later
            // transition.set("internal_added", 200);

            QString geometry;
            switch (pCore->activeTool()) {
            case ToolType::RollTool:
            case ToolType::RippleTool:
                switch (i) {
                case 0:
                    geometry = QStringLiteral("0 0 50% 100%");
                    break;
                case 1:
                    geometry = QStringLiteral("50% 0 50% 100%");
                    break;
                }
                break;
            case ToolType::SlipTool:
                switch (i) {
                case 0:
                    geometry = QStringLiteral("0 0 25% 25%");
                    break;
                case 1:
                    geometry = QStringLiteral("0 25% 50% 50%");
                    break;
                case 2:
                    geometry = QStringLiteral("50% 25% 50% 50%");
                    break;
                case 3:
                    geometry = QStringLiteral("75% 75% 25% 25%");
                    break;
                }
                break;
            case ToolType::SlideTool:
                switch (i) {
                case 0:
                    geometry = QStringLiteral("0 0 25% 25%");
                    break;
                case 1:
                    geometry = QStringLiteral("50% 25% 50% 50%");
                    break;
                case 2:
                    geometry = QStringLiteral("0 25% 50% 50%");
                    break;
                case 3:
                    geometry = QStringLiteral("50% 75% 25% 25%");
                    break;
                }
                break;
            default:
                break;
            }

            // Add transition to track:
            transition.set("geometry", geometry.toUtf8().constData());
            transition.set("always_active", 1);
            trac.plant_transition(transition, 0, i + 1);
        }
    }

    pCore->monitorManager()->projectMonitor()->setProducer(std::make_shared<Mlt::Producer>(trac), -2);
    pCore->monitorManager()->projectMonitor()->slotSwitchTrimming(true);

    switch (pCore->activeTool()) {
    case ToolType::RollTool:
    case ToolType::RippleTool:
        ripplePosChanged(mainClip->getPlaytime(), right);
        break;
    case ToolType::SlipTool:
        slipPosChanged(0);
        break;
    case ToolType::SlideTool:
        break;
    default:
        break;
    }

    return true;
}

void TimelineController::requestEndTrimmingMode()
{
    if (pCore->monitorManager()->isTrimming()) {
        pCore->monitorManager()->projectMonitor()->setProducer(pCore->window()->getCurrentTimeline()->model()->producer(), 0);
        pCore->monitorManager()->projectMonitor()->slotSwitchTrimming(false);
    }
}

void TimelineController::addPreviewRange(bool add)
{
    if (m_zone.isNull()) {
        return;
    }
    if (!m_model->hasTimelinePreview()) {
        initializePreview();
    }
    if (m_model->hasTimelinePreview()) {
        m_model->previewManager()->addPreviewRange(m_zone, add);
    }
}

void TimelineController::clearPreviewRange(bool resetZones)
{
    if (m_model->hasTimelinePreview()) {
        m_model->previewManager()->clearPreviewRange(resetZones);
    }
}

void TimelineController::startPreviewRender()
{
    // Timeline preview stuff
    if (!m_model->hasTimelinePreview()) {
        initializePreview();
    } else if (m_disablePreview->isChecked()) {
        m_disablePreview->setChecked(false);
        disablePreview(false);
    }
    if (m_model->hasTimelinePreview()) {
        if (!m_usePreview) {
            m_model->buildPreviewTrack();
            m_usePreview = true;
        }
        if (!m_model->previewManager()->hasDefinedRange()) {
            addPreviewRange(true);
        }
        m_model->previewManager()->startPreviewRender();
    }
}

void TimelineController::stopPreviewRender()
{
    if (m_model->hasTimelinePreview()) {
        m_model->previewManager()->abortRendering();
    }
}

void TimelineController::initializePreview()
{
    if (m_model->hasTimelinePreview()) {
        // Update parameters
        if (!m_model->previewManager()->loadParams()) {
            if (m_usePreview) {
                // Disconnect preview track
                m_model->previewManager()->disconnectTrack();
                m_usePreview = false;
            }
            m_model->resetPreviewManager();
        }
    } else {
        m_model->initializePreviewManager();
    }
}

void TimelineController::connectPreviewManager()
{
    if (m_model->hasTimelinePreview()) {
        connect(m_model->previewManager().get(), &PreviewManager::dirtyChunksChanged, this, &TimelineController::dirtyChunksChanged,
                static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
        connect(m_model->previewManager().get(), &PreviewManager::renderedChunksChanged, this, &TimelineController::renderedChunksChanged,
                static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
        connect(m_model->previewManager().get(), &PreviewManager::workingPreviewChanged, this, &TimelineController::workingPreviewChanged,
                static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
    }
}

bool TimelineController::hasPreviewTrack() const
{
    return (m_model->hasTimelinePreview() && (m_model->previewManager()->hasOverlayTrack() || m_model->previewManager()->hasPreviewTrack()));
}

void TimelineController::disablePreview(bool disable)
{
    if (disable) {
        m_model->deletePreviewTrack();
        m_usePreview = false;
    } else {
        if (!m_usePreview) {
            if (!m_model->buildPreviewTrack()) {
                // preview track already exists, reconnect
                m_model->m_tractor->lock();
                m_model->previewManager()->reconnectTrack();
                m_model->m_tractor->unlock();
            }
            Mlt::Playlist playlist;
            m_model->previewManager()->loadChunks(QVariantList(), QVariantList(), playlist);
            m_usePreview = true;
        }
    }
}

QVariantList TimelineController::dirtyChunks() const
{
    return m_model->hasTimelinePreview() ? m_model->previewManager()->m_dirtyChunks : QVariantList();
}

QVariantList TimelineController::renderedChunks() const
{
    return m_model->hasTimelinePreview() ? m_model->previewManager()->m_renderedChunks : QVariantList();
}

int TimelineController::workingPreview() const
{
    return m_model->hasTimelinePreview() ? m_model->previewManager()->workingPreview : -1;
}

bool TimelineController::useRuler() const
{
    return pCore->currentDoc()->getDocumentProperty(QStringLiteral("enableTimelineZone")).toInt() == 1;
}

bool TimelineController::scrollVertically() const
{
    return KdenliveSettings::scrollvertically() == 1;
}

void TimelineController::resetPreview()
{
    if (m_model->hasTimelinePreview()) {
        m_model->previewManager()->clearPreviewRange(true);
        initializePreview();
    }
}

void TimelineController::getSequenceProperties(QMap<QString, QString> &seqProps)
{
    int activeTrack = m_activeTrack < 0 ? m_activeTrack : m_model->getTrackPosition(m_activeTrack);
    QVariant returnedValue;
    QMetaObject::invokeMethod(m_root, "getScrollPos", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    int scrollPos = returnedValue.toInt();
    seqProps.insert(QStringLiteral("position"), QString::number(m_model->tractor()->position()));
    seqProps.insert(QStringLiteral("activeTrack"), QString::number(activeTrack));
    seqProps.insert(QStringLiteral("scrollPos"), QString::number(scrollPos));
    seqProps.insert(QStringLiteral("zonein"), QString::number(m_zone.x()));
    seqProps.insert(QStringLiteral("zoneout"), QString::number(m_zone.y()));
    seqProps.insert(QStringLiteral("disablepreview"), QString::number(m_disablePreview->isChecked()));

    if (m_model->hasSubtitleModel()) {
        const QString subtitlesData = m_model->getSubtitleModel()->subtitlesFilesToJson();
        seqProps.insert(QStringLiteral("subtitlesList"), subtitlesData);
    }
}

int TimelineController::getMenuOrTimelinePos() const
{
    int frame = m_root->property("clickFrame").toInt();
    if (frame == -1) {
        frame = pCore->getMonitorPosition();
    }
    return frame;
}

void TimelineController::insertSpace(int trackId, int frame)
{
    if (frame == -1) {
        frame = getMenuOrTimelinePos();
    }
    if (trackId == -1) {
        trackId = m_activeTrack;
    }
    QPointer<SpacerDialog> d = new SpacerDialog(GenTime(65, pCore->getCurrentFps()), pCore->currentDoc()->timecode(), qApp->activeWindow());
    if (d->exec() != QDialog::Accepted) {
        delete d;
        return;
    }
    bool affectAllTracks = d->affectAllTracks();
    int cid = requestSpacerStartOperation(affectAllTracks ? -1 : trackId, frame);
    int spaceDuration = d->selectedDuration().frames(pCore->getCurrentFps());
    delete d;
    if (cid == -1) {
        pCore->displayMessage(i18n("No clips found to insert space"), ErrorMessage, 500);
        return;
    }
    int start = m_model->getItemPosition(cid);
    requestSpacerEndOperation(cid, start, start + spaceDuration, affectAllTracks ? -1 : trackId);
}

void TimelineController::removeSpace(int trackId, int frame, bool affectAllTracks)
{
    if (frame == -1) {
        frame = getMenuOrTimelinePos();
    }
    if (trackId == -1) {
        trackId = m_activeTrack;
    }
    bool res = TimelineFunctions::requestDeleteBlankAt(m_model, trackId, frame, affectAllTracks);
    if (!res) {
        pCore->displayMessage(i18n("Cannot remove space at given position"), ErrorMessage, 500);
    }
}

void TimelineController::removeTrackSpaces(int trackId, int frame)
{
    if (frame == -1) {
        frame = getMenuOrTimelinePos();
    }
    if (trackId == -1) {
        trackId = m_activeTrack;
    }
    bool res = TimelineFunctions::requestDeleteAllBlanksFrom(m_model, trackId, frame);
    if (!res) {
        pCore->displayMessage(i18n("Cannot remove all spaces"), ErrorMessage, 500);
    }
}

void TimelineController::removeTrackClips(int trackId, int frame)
{
    if (frame == -1) {
        frame = getMenuOrTimelinePos();
    }
    if (trackId == -1) {
        trackId = m_activeTrack;
    }
    bool res = TimelineFunctions::requestDeleteAllClipsFrom(m_model, trackId, frame);
    if (!res) {
        pCore->displayMessage(i18n("Cannot remove all clips"), ErrorMessage, 500);
    }
}

void TimelineController::invalidateItem(int cid)
{
    if (!m_model->hasTimelinePreview() || !m_model->isItem(cid)) {
        return;
    }
    const int tid = m_model->getItemTrackId(cid);
    if (tid == -1 || m_model->getTrackById_const(tid)->isAudioTrack()) {
        return;
    }
    int start = m_model->getItemPosition(cid);
    int end = start + m_model->getItemPlaytime(cid);
    m_model->previewManager()->invalidatePreview(start, end);
}

void TimelineController::invalidateTrack(int tid)
{
    if (!m_model->hasTimelinePreview() || !m_model->isTrack(tid) || m_model->getTrackById_const(tid)->isAudioTrack()) {
        return;
    }
    for (const auto &clp : m_model->getTrackById_const(tid)->m_allClips) {
        invalidateItem(clp.first);
    }
}

void TimelineController::remapItemTime(int clipId)
{
    if (clipId == -1) {
        clipId = getMainSelectedClip();
    }
    // Don't allow remaping a clip with speed effect
    if (clipId == -1 || !m_model->isClip(clipId) || !qFuzzyCompare(1., m_model->m_allClips[clipId]->getSpeed())) {
        pCore->displayMessage(i18n("No item to edit"), ErrorMessage, 500);
        return;
    }
    ClipType::ProducerType type = m_model->m_allClips[clipId]->clipType();
    if (type == ClipType::Color || type == ClipType::Image) {
        pCore->displayMessage(i18n("No item to edit"), ErrorMessage, 500);
        return;
    }
    if (m_model->m_allClips[clipId]->hasTimeRemap()) {
        // Remove remap effect
        m_model->requestClipTimeRemap(clipId, false);
        Q_EMIT pCore->remapClip(-1);
    } else {
        // Add remap effect
        Q_EMIT pCore->remapClip(clipId);
    }
}

void TimelineController::changeItemSpeed(int clipId, double speed)
{
    /*if (clipId == -1) {
        clipId = getMainSelectedItem(false, true);
    }*/
    if (clipId == -1) {
        clipId = getMainSelectedClip();
    }
    if (clipId == -1) {
        pCore->displayMessage(i18n("No item to edit"), ErrorMessage, 500);
        return;
    }
    bool pitchCompensate = m_model->m_allClips[clipId]->getIntProperty(QStringLiteral("warp_pitch"));
    if (qFuzzyCompare(speed, -1)) {
        speed = 100 * m_model->getClipSpeed(clipId);
        int duration = m_model->getItemPlaytime(clipId);
        // this is the max speed so that the clip is at least one frame long
        double maxSpeed = duration * qAbs(speed);
        // this is the min speed so that the clip doesn't bump into the next one on track
        double minSpeed = duration * qAbs(speed) / (duration + double(m_model->getBlankSizeNearClip(clipId, true)));

        // if there is a split partner, we must also take it into account
        int partner = m_model->getClipSplitPartner(clipId);
        if (partner != -1) {
            double duration2 = m_model->getItemPlaytime(partner);
            double maxSpeed2 = 100. * duration2 * qAbs(m_model->getClipSpeed(partner));
            double minSpeed2 = 100. * duration2 * qAbs(m_model->getClipSpeed(partner)) / (duration2 + double(m_model->getBlankSizeNearClip(partner, true)));
            minSpeed = std::max(minSpeed, minSpeed2);
            maxSpeed = std::min(maxSpeed, maxSpeed2);
        }
        std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(getClipBinId(clipId));
        QScopedPointer<SpeedDialog> d(
            new SpeedDialog(QApplication::activeWindow(), std::abs(speed), duration, minSpeed, maxSpeed, speed < 0, pitchCompensate, binClip->clipType()));
        if (d->exec() != QDialog::Accepted) {
            Q_EMIT regainFocus();
            return;
        }
        Q_EMIT regainFocus();
        speed = d->getValue();
        pitchCompensate = d->getPitchCompensate();
        qDebug() << "requesting speed " << speed;
    }
    bool res = m_model->requestClipTimeWarp(clipId, speed, pitchCompensate, true);
    if (res) {
        updateClipActions();
    }
}

void TimelineController::switchCompositing(bool enable)
{
    // m_model->m_tractor->lock();
    pCore->currentDoc()->setDocumentProperty(QStringLiteral("compositing"), QString::number(enable));
    QScopedPointer<Mlt::Service> service(m_model->m_tractor->field());
    QScopedPointer<Mlt::Field> field(m_model->m_tractor->field());
    field->lock();
    while ((service != nullptr) && service->is_valid()) {
        if (service->type() == mlt_service_transition_type) {
            Mlt::Transition t(mlt_transition(service->get_service()));
            service.reset(service->producer());
            QString serviceName = t.get("mlt_service");
            if (t.get_int("internal_added") == 237 && serviceName != QLatin1String("mix")) {
                // remove all compositing transitions
                field->disconnect_service(t);
                t.disconnect_all_producers();
            }
        } else {
            service.reset(service->producer());
        }
    }
    if (enable) {
        // Loop through tracks
        for (int track = 0; track < m_model->getTracksCount(); track++) {
            if (m_model->getTrackById(m_model->getTrackIndexFromPosition(track))->getProperty("kdenlive:audio_track").toInt() == 0) {
                // This is a video track
                Mlt::Transition t(pCore->getProjectProfile(), TransitionsRepository::get()->getCompositingTransition().toUtf8().constData());
                t.set("always_active", 1);
                t.set_tracks(0, track + 1);
                t.set("internal_added", 237);
                field->plant_transition(t, 0, track + 1);
            }
        }
    }
    field->unlock();
    pCore->refreshProjectMonitorOnce();
}

void TimelineController::extractZone(QPoint zone, bool liftOnly)
{
    QVector<int> tracks;
    auto it = m_model->m_allTracks.cbegin();
    while (it != m_model->m_allTracks.cend()) {
        int target_track = (*it)->getId();
        if (m_model->getTrackById_const(target_track)->shouldReceiveTimelineOp()) {
            tracks << target_track;
        }
        ++it;
    }
    if (tracks.isEmpty()) {
        pCore->displayMessage(i18n("Please activate a track for this operation by clicking on its label"), ErrorMessage);
    }
    if (m_zone.isNull()) {
        // Use current timeline position and clip zone length
        zone.setY(pCore->getMonitorPosition() + zone.y() - zone.x());
        zone.setX(pCore->getMonitorPosition());
    }
    TimelineFunctions::extractZone(m_model, tracks, m_zone == QPoint() ? zone : m_zone, liftOnly);
    if (!liftOnly && !m_zone.isNull()) {
        setPosition(m_zone.x());
    }
}

void TimelineController::extract(int clipId, bool singleSelectionMode)
{
    if (clipId == -1) {
        std::unordered_set<int> sel = m_model->getCurrentSelection();
        for (int i : sel) {
            if (m_model->isClip(i)) {
                clipId = i;
                break;
            }
        }
        if (clipId == -1) {
            pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
            return;
        }
    }
    int in = m_model->getClipPosition(clipId);
    int out = in + m_model->getClipPlaytime(clipId);
    int tid = m_model->getClipTrackId(clipId);
    std::pair<MixInfo, MixInfo> mixData = m_model->getTrackById_const(tid)->getMixInfo(clipId);
    if (mixData.first.firstClipId > -1) {
        // Clip has a start mix, adjust in point
        in += (mixData.first.firstClipInOut.second - mixData.first.secondClipInOut.first - mixData.first.mixOffset);
    }
    if (mixData.second.firstClipId > -1) {
        // Clip has end mix, adjust out point
        out -= mixData.second.mixOffset;
    }
    QVector<int> tracks = {tid};
    int clipToUngroup = -1;
    std::unordered_set<int> clipsToRegroup;
    if (m_model->m_groups->isInGroup(clipId)) {
        if (singleSelectionMode) {
            // Remove item from group
            clipsToRegroup = m_model->m_groups->getLeaves(m_model->m_groups->getRootId(clipId));
            clipToUngroup = clipId;
            clipsToRegroup.erase(clipToUngroup);
            m_model->requestClearSelection();
        } else {
            int targetRoot = m_model->m_groups->getRootId(clipId);
            if (m_model->isGroup(targetRoot)) {
                std::unordered_set<int> sub = m_model->m_groups->getLeaves(targetRoot);
                for (int current_id : sub) {
                    if (current_id == clipId) {
                        continue;
                    }
                    if (m_model->isClip(current_id)) {
                        int newIn = m_model->getClipPosition(current_id);
                        int newOut = newIn + m_model->getClipPlaytime(current_id);
                        int tk = m_model->getClipTrackId(current_id);
                        std::pair<MixInfo, MixInfo> cMixData = m_model->getTrackById_const(tk)->getMixInfo(current_id);
                        if (cMixData.first.firstClipId > -1) {
                            // Clip has a start mix, adjust in point
                            newIn += (cMixData.first.firstClipInOut.second - cMixData.first.secondClipInOut.first - cMixData.first.mixOffset);
                        }
                        if (cMixData.second.firstClipId > -1) {
                            // Clip has end mix, adjust out point
                            newOut -= cMixData.second.mixOffset;
                        }
                        in = qMin(in, newIn);
                        out = qMax(out, newOut);
                        if (!tracks.contains(tk)) {
                            tracks << tk;
                        }
                    }
                }
            }
        }
    }
    TimelineFunctions::extractZone(m_model, tracks, QPoint(in, out), false, clipToUngroup, clipsToRegroup);
}

void TimelineController::saveZone(int clipId)
{
    if (clipId == -1) {
        clipId = getMainSelectedClip();
        if (clipId == -1) {
            pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
            return;
        }
    }
    int in = m_model->getClipIn(clipId);
    int out = in + m_model->getClipPlaytime(clipId) - 1;
    QString id;
    pCore->projectItemModel()->requestAddBinSubClip(id, in, out, {}, m_model->m_allClips[clipId]->binId());
}

bool TimelineController::insertClipZone(const QString &binId, int tid, int position)
{
    QStringList binIdData = binId.split(QLatin1Char('/'));
    int in = 0;
    int out = -1;
    if (binIdData.size() >= 3) {
        in = binIdData.at(1).toInt();
        out = binIdData.at(2).toInt();
    }

    QString bid = binIdData.first();
    // dropType indicates if we want a normal drop (disabled), audio only or video only drop
    PlaylistState::ClipState dropType = PlaylistState::Disabled;
    if (bid.startsWith(QLatin1Char('A'))) {
        dropType = PlaylistState::AudioOnly;
        bid = bid.remove(0, 1);
    } else if (bid.startsWith(QLatin1Char('V'))) {
        dropType = PlaylistState::VideoOnly;
        bid = bid.remove(0, 1);
    }
    QList<int> audioTracks;
    int vTrack = -1;
    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(bid);
    if (out <= in) {
        out = int(clip->frameDuration() - 1);
    }
    QList<int> audioStreams = m_model->m_binAudioTargets.keys();
    if (dropType == PlaylistState::VideoOnly) {
        vTrack = tid;
    } else if (dropType == PlaylistState::AudioOnly) {
        audioTracks << tid;
        if (audioStreams.size() > 1) {
            // insert the other audio streams
            QList<int> lower = m_model->getLowerTracksId(tid, TrackType::AudioTrack);
            while (audioStreams.size() > 1 && !lower.isEmpty()) {
                audioTracks << lower.takeFirst();
                audioStreams.takeFirst();
            }
        }
    } else {
        if (m_model->getTrackById_const(tid)->isAudioTrack()) {
            audioTracks << tid;
            if (audioStreams.size() > 1) {
                // insert the other audio streams
                QList<int> lower = m_model->getLowerTracksId(tid, TrackType::AudioTrack);
                while (audioStreams.size() > 1 && !lower.isEmpty()) {
                    audioTracks << lower.takeFirst();
                    audioStreams.takeFirst();
                }
            }
            vTrack = clip->hasAudioAndVideo() ? m_model->getMirrorVideoTrackId(tid) : -1;
        } else {
            vTrack = tid;
            if (clip->hasAudioAndVideo()) {
                int firstAudio = m_model->getMirrorAudioTrackId(vTrack);
                audioTracks << firstAudio;
                if (audioStreams.size() > 1) {
                    // insert the other audio streams
                    QList<int> lower = m_model->getLowerTracksId(firstAudio, TrackType::AudioTrack);
                    while (audioStreams.size() > 1 && !lower.isEmpty()) {
                        audioTracks << lower.takeFirst();
                        audioStreams.takeFirst();
                    }
                }
            }
        }
    }
    QList<int> target_tracks;
    if (vTrack > -1) {
        target_tracks << vTrack;
    }
    if (!audioTracks.isEmpty()) {
        target_tracks << audioTracks;
    }
    qDebug() << "=====================\n\nREADY TO INSERT IN TRACKS: " << audioTracks << " / VIDEO: " << vTrack << "\n\n=========";
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool overwrite = m_model->m_editMode == TimelineMode::OverwriteEdit;
    QPoint zone(in, out + 1);
    bool res = TimelineFunctions::insertZone(m_model, target_tracks, binId, position, zone, overwrite, false, undo, redo);
    if (res) {
        int newPos = position + (zone.y() - zone.x());
        int currentPos = pCore->getMonitorPosition();
        Fun redoPos = [this, newPos]() {
            Kdenlive::MonitorId activeMonitor = pCore->monitorManager()->activeMonitor()->id();
            pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
            pCore->monitorManager()->refreshProjectMonitor();
            setPosition(newPos);
            pCore->monitorManager()->activateMonitor(activeMonitor);
            return true;
        };
        Fun undoPos = [this, currentPos]() {
            Kdenlive::MonitorId activeMonitor = pCore->monitorManager()->activeMonitor()->id();
            pCore->monitorManager()->activateMonitor(Kdenlive::ProjectMonitor);
            pCore->monitorManager()->refreshProjectMonitor();
            setPosition(currentPos);
            pCore->monitorManager()->activateMonitor(activeMonitor);
            return true;
        };
        redoPos();
        UPDATE_UNDO_REDO_NOLOCK(redoPos, undoPos, undo, redo);
        pCore->pushUndo(undo, redo, overwrite ? i18n("Overwrite zone") : i18n("Insert zone"));
    } else {
        pCore->displayMessage(i18n("Could not insert zone"), ErrorMessage);
        undo();
    }
    return res;
}

int TimelineController::insertZone(const QString &binId, QPoint zone, bool overwrite, Fun &undo, Fun &redo)
{
    std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(binId);
    int aTrack = -1;
    int vTrack = -1;
    if (clip->hasAudio() && !m_model->m_audioTarget.isEmpty()) {
        QList<int> audioTracks = m_model->m_audioTarget.keys();
        for (int tid : qAsConst(audioTracks)) {
            if (m_model->getTrackById_const(tid)->shouldReceiveTimelineOp()) {
                aTrack = tid;
                break;
            }
        }
    }
    if (clip->hasVideo()) {
        vTrack = videoTarget();
    }

    int insertPoint;
    QPoint sourceZone;
    if (useRuler() && m_zone != QPoint()) {
        // We want to use timeline zone for in/out insert points
        insertPoint = m_zone.x();
        sourceZone = QPoint(zone.x(), zone.x() + m_zone.y() - m_zone.x());
    } else {
        // Use current timeline pos and clip zone for in/out
        insertPoint = pCore->getMonitorPosition();
        sourceZone = zone;
    }
    QList<int> target_tracks;
    if (vTrack > -1) {
        target_tracks << vTrack;
    }
    if (aTrack > -1) {
        target_tracks << aTrack;
    }
    if (target_tracks.isEmpty()) {
        pCore->displayMessage(i18n("Please select a target track by clicking on a track's target zone"), ErrorMessage);
        return -1;
    }
    bool res = TimelineFunctions::insertZone(m_model, target_tracks, binId, insertPoint, sourceZone, overwrite, true, undo, redo);
    if (res) {
        int newPos = insertPoint + (sourceZone.y() - sourceZone.x());
        int currentPos = pCore->getMonitorPosition();
        Fun redoPos = [this, newPos]() {
            setPosition(newPos);
            pCore->getMonitor(Kdenlive::ProjectMonitor)->refreshMonitorIfActive();
            return true;
        };
        Fun undoPos = [this, currentPos]() {
            setPosition(currentPos);
            pCore->getMonitor(Kdenlive::ProjectMonitor)->refreshMonitorIfActive();
            return true;
        };
        redoPos();
        UPDATE_UNDO_REDO_NOLOCK(redoPos, undoPos, undo, redo);
    }
    return res;
}

void TimelineController::updateClip(int clipId, const QVector<int> &roles)
{
    QModelIndex ix = m_model->makeClipIndexFromID(clipId);
    if (ix.isValid()) {
        Q_EMIT m_model->dataChanged(ix, ix, roles);
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

void TimelineController::switchEnableState(std::unordered_set<int> selection)
{
    if (selection.empty()) {
        selection = m_model->getCurrentSelection();
        // clipId = getMainSelectedItem(false, false);
    }
    if (selection.empty()) {
        return;
    }
    TimelineFunctions::switchEnableState(m_model, selection);
}

void TimelineController::addCompositionToClip(const QString &assetId, int clipId, int offset)
{
    if (clipId == -1) {
        clipId = getMainSelectedClip();
        if (clipId == -1) {
            pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
            return;
        }
    }
    if (offset == -1) {
        offset = m_root->property("clickFrame").toInt();
    }
    int track = clipId > -1 ? m_model->getClipTrackId(clipId) : m_activeTrack;
    int compoId = -1;
    if (assetId.isEmpty()) {
        QStringList compositions = KdenliveSettings::favorite_transitions();
        if (compositions.isEmpty()) {
            pCore->displayMessage(i18n("Select a favorite composition"), ErrorMessage, 500);
            return;
        }
        compoId = insertNewComposition(track, clipId, offset, compositions.first(), true);
    } else {
        compoId = insertNewComposition(track, clipId, offset, assetId, true);
    }
    if (compoId > 0) {
        m_model->requestSetSelection({compoId});
    }
}

void TimelineController::setEffectsEnabled(int clipId, bool enabled)
{
    std::shared_ptr<ClipModel> clip = m_model->getClipPtr(clipId);
    clip->setTimelineEffectsEnabled(enabled);
}

void TimelineController::addEffectToClip(const QString &assetId, int clipId)
{
    if (clipId == -1) {
        clipId = getMainSelectedClip();
        if (clipId == -1) {
            pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
            return;
        }
    }
    if (m_model->addClipEffect(clipId, assetId).size() > 0 && KdenliveSettings::seekonaddeffect()) {
        // Move timeline cursor inside clip if it is not
        int in = m_model->getClipPosition(clipId);
        int out = in + m_model->getClipPlaytime(clipId);
        int position = pCore->getMonitorPosition();
        if (position < in || position > out) {
            Q_EMIT seeked(in);
        }
    }
}

bool TimelineController::splitAV()
{
    int cid = *m_model->getCurrentSelection().begin();
    if (m_model->isClip(cid)) {
        std::shared_ptr<ClipModel> clip = m_model->getClipPtr(cid);
        if (clip->clipState() == PlaylistState::AudioOnly) {
            return TimelineFunctions::requestSplitVideo(m_model, cid, videoTarget());
        } else {
            QVariantList aTargets = audioTarget();
            int targetTrack = aTargets.isEmpty() ? -1 : aTargets.first().toInt();
            return TimelineFunctions::requestSplitAudio(m_model, cid, targetTrack);
        }
    }
    pCore->displayMessage(i18n("No clip found to perform AV split operation"), ErrorMessage, 500);
    return false;
}

void TimelineController::splitAudio(int clipId)
{
    QVariantList aTargets = audioTarget();
    int targetTrack = aTargets.isEmpty() ? -1 : aTargets.first().toInt();
    TimelineFunctions::requestSplitAudio(m_model, clipId, targetTrack);
}

void TimelineController::splitVideo(int clipId)
{
    TimelineFunctions::requestSplitVideo(m_model, clipId, videoTarget());
}

void TimelineController::setAudioRef(int clipId)
{
    if (clipId == -1) {
        clipId = getMainSelectedClip();
        if (clipId == -1) {
            pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
            return;
        }
    }
    m_audioRef = clipId;
    std::unique_ptr<AudioEnvelope> envelope(new AudioEnvelope(getClipBinId(clipId), clipId));
    m_audioCorrelator.reset(new AudioCorrelation(std::move(envelope)));
    connect(m_audioCorrelator.get(), &AudioCorrelation::gotAudioAlignData, this, [&](int cid, int shift) {
        // Ensure the clip was not deleted while processing calculations
        if (m_model->isClip(cid)) {
            int pos = m_model->getClipPosition(m_audioRef) + shift - m_model->getClipIn(m_audioRef);
            bool result = m_model->requestClipMove(cid, m_model->getClipTrackId(cid), pos, true, true, true);
            if (!result) {
                pCore->displayMessage(i18n("Cannot move clip to frame %1.", (pos + shift)), ErrorMessage, 500);
            }
        } else {
            // Clip was deleted, discard audio reference
            m_audioRef = -1;
        }
    });
    connect(m_audioCorrelator.get(), &AudioCorrelation::displayMessage, pCore.get(), &Core::displayMessage);
}

void TimelineController::alignAudio(int clipId)
{
    // find other clip
    if (clipId == -1) {
        clipId = getMainSelectedClip();
        if (clipId == -1) {
            pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
            return;
        }
    }
    if (m_audioRef == -1 || m_audioRef == clipId || !m_model->isClip(m_audioRef)) {
        pCore->displayMessage(i18n("Set audio reference before attempting to align"), InformationMessage, 500);
        return;
    }
    const QString masterBinClipId = getClipBinId(m_audioRef);
    std::unordered_set<int> clipsToAnalyse;
    if (m_model->m_groups->isInGroup(clipId)) {
        clipsToAnalyse = m_model->getGroupElements(clipId);
        m_model->requestClearSelection();
    } else {
        clipsToAnalyse.insert(clipId);
    }
    QList<int> processedGroups;
    int processed = 0;
    for (int cid : clipsToAnalyse) {
        if (!m_model->isClip(cid) || cid == m_audioRef) {
            continue;
        }
        const QString otherBinId = getClipBinId(cid);
        if (m_model->m_groups->isInGroup(cid)) {
            int parentGroup = m_model->m_groups->getRootId(cid);
            if (processedGroups.contains(parentGroup)) {
                continue;
            }
            // Only process one clip from the group
            std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(otherBinId);
            if (clip->hasAudio()) {
                processedGroups << parentGroup;
            } else {
                continue;
            }
        }
        if (!pCore->bin()->getBinClip(otherBinId)->hasAudio()) {
            // Cannot process non audi clips
            continue;
        }
        if (otherBinId == masterBinClipId) {
            // easy, same clip.
            int newPos = m_model->getClipPosition(m_audioRef) - m_model->getClipIn(m_audioRef) + m_model->getClipIn(cid);
            if (newPos) {
                bool result = m_model->requestClipMove(cid, m_model->getClipTrackId(cid), newPos, true, true, true);
                processed++;
                if (!result) {
                    pCore->displayMessage(i18n("Cannot move clip to frame %1.", newPos), ErrorMessage, 500);
                }
                continue;
            }
        }
        processed++;
        // Perform audio calculation
        auto *envelope =
            new AudioEnvelope(otherBinId, cid, size_t(m_model->getClipIn(cid)), size_t(m_model->getClipPlaytime(cid)), size_t(m_model->getClipPosition(cid)));
        m_audioCorrelator->addChild(envelope);
    }
    if (processed == 0) {
        // TODO: improve feedback message after freeze
        pCore->displayMessage(i18n("Select a clip to apply an effect"), ErrorMessage, 500);
    }
}

void TimelineController::switchTrackActive(int trackId)
{
    if (trackId == -1) {
        trackId = m_activeTrack;
    }
    if (trackId < 0) {
        return;
    }
    bool active = m_model->getTrackById_const(trackId)->isTimelineActive();
    m_model->setTrackProperty(trackId, QStringLiteral("kdenlive:timeline_active"), active ? QStringLiteral("0") : QStringLiteral("1"));
    m_activeSnaps.clear();
}

void TimelineController::switchAllTrackActive()
{
    auto it = m_model->m_allTracks.cbegin();
    while (it != m_model->m_allTracks.cend()) {
        bool active = (*it)->isTimelineActive();
        int target_track = (*it)->getId();
        m_model->setTrackProperty(target_track, QStringLiteral("kdenlive:timeline_active"), active ? QStringLiteral("0") : QStringLiteral("1"));
        ++it;
    }
    m_activeSnaps.clear();
}

void TimelineController::makeAllTrackActive()
{
    // Check current status
    auto it = m_model->m_allTracks.cbegin();
    bool makeActive = false;
    while (it != m_model->m_allTracks.cend()) {
        if (!(*it)->isTimelineActive()) {
            // There is an inactive track, activate all
            makeActive = true;
            break;
        }
        ++it;
    }
    it = m_model->m_allTracks.cbegin();
    while (it != m_model->m_allTracks.cend()) {
        int target_track = (*it)->getId();
        m_model->setTrackProperty(target_track, QStringLiteral("kdenlive:timeline_active"), makeActive ? QStringLiteral("1") : QStringLiteral("0"));
        ++it;
    }
    m_activeSnaps.clear();
}

void TimelineController::switchTrackDisabled()
{

    if (m_model->isSubtitleTrack(m_activeTrack)) {
        // Subtitle track
        switchSubtitleDisable();
    } else {
        bool isAudio = m_model->getTrackById_const(m_activeTrack)->isAudioTrack();
        bool enabled = isAudio ? m_model->getTrackById_const(m_activeTrack)->isMute() : m_model->getTrackById_const(m_activeTrack)->isHidden();
        hideTrack(m_activeTrack, enabled);
    }
}

void TimelineController::switchTrackLock(bool applyToAll)
{
    if (!applyToAll) {
        // apply to active track only
        if (m_model->isSubtitleTrack(m_activeTrack)) {
            // Subtitle track
            switchSubtitleLock();
        } else {
            bool locked = m_model->getTrackById_const(m_activeTrack)->isLocked();
            m_model->setTrackLockedState(m_activeTrack, !locked);
        }
    } else {
        // Invert track lock
        const auto ids = m_model->getAllTracksIds();
        // count the number of tracks to be locked
        for (const int id : ids) {
            bool isLocked = m_model->getTrackById_const(id)->isLocked();
            m_model->setTrackLockedState(id, !isLocked);
        }
        if (m_model->hasSubtitleModel()) {
            switchSubtitleLock();
        }
    }
}

void TimelineController::switchTargetTrack()
{
    if (m_activeTrack < 0) {
        return;
    }
    bool isAudio = m_model->isAudioTrack(m_activeTrack);
    if (isAudio) {
        QMap<int, int> current = m_model->m_audioTarget;
        if (current.contains(m_activeTrack)) {
            current.remove(m_activeTrack);
        } else {
            int ix = getFirstUnassignedStream();
            if (ix > -1) {
                current.insert(m_activeTrack, ix);
            } else if (current.size() == 1) {
                // If we only have one video stream, directly reassign it
                int stream = current.first();
                current.clear();
                current.insert(m_activeTrack, stream);
            } else {
                pCore->displayMessage(i18n("All streams already assigned, deselect another audio target first"), InformationMessage, 500);
                return;
            }
        }
        setAudioTarget(current);
    } else {
        setVideoTarget(videoTarget() == m_activeTrack ? -1 : m_activeTrack);
    }
}

QVariantList TimelineController::audioTarget() const
{
    QVariantList audioTracks;
    QMapIterator<int, int> i(m_model->m_audioTarget);
    while (i.hasNext()) {
        i.next();
        audioTracks << i.key();
    }
    return audioTracks;
}

QVariantList TimelineController::lastAudioTarget() const
{
    QVariantList audioTracks;
    QMapIterator<int, int> i(m_lastAudioTarget);
    while (i.hasNext()) {
        i.next();
        audioTracks << i.key();
    }
    return audioTracks;
}

const QString TimelineController::audioTargetName(int tid) const
{
    if (m_model->m_audioTarget.contains(tid) && m_model->m_binAudioTargets.size() > 1) {
        int streamIndex = m_model->m_audioTarget.value(tid);
        if (m_model->m_binAudioTargets.contains(streamIndex)) {
            QString targetName = m_model->m_binAudioTargets.value(streamIndex);
            return targetName.isEmpty() ? QChar('x') : targetName.section(QLatin1Char('|'), 0, 0);
        } else {
            qDebug() << "STREAM INDEX NOT IN TARGET : " << streamIndex << " = " << m_model->m_binAudioTargets;
        }
    } else {
        qDebug() << "TRACK NOT IN TARGET : " << tid << " = " << m_model->m_audioTarget.keys();
    }
    return QString();
}

int TimelineController::videoTarget() const
{
    return m_model->m_videoTarget;
}

int TimelineController::hasAudioTarget() const
{
    return m_hasAudioTarget;
}

int TimelineController::clipTargets() const
{
    return m_model->m_binAudioTargets.size();
}

bool TimelineController::hasVideoTarget() const
{
    return m_hasVideoTarget;
}

bool TimelineController::autoScroll() const
{
    return !pCore->monitorManager()->projectMonitor()->isPlaying() || KdenliveSettings::autoscroll();
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
    Q_EMIT m_model->dataChanged(modelStart, modelEnd, {TimelineModel::HeightRole});
}

void TimelineController::selectAll()
{
    std::unordered_set<int> ids;
    for (const auto &clp : m_model->m_allClips) {
        ids.insert(clp.first);
    }
    for (const auto &clp : m_model->m_allCompositions) {
        ids.insert(clp.first);
    }
    // Subtitles
    for (const auto &sub : m_model->m_allSubtitles) {
        ids.insert(sub.first);
    }
    m_model->requestSetSelection(ids);
}

void TimelineController::selectCurrentTrack()
{
    if (m_activeTrack == -1) {
        return;
    }
    std::unordered_set<int> ids;
    if (m_model->isSubtitleTrack(m_activeTrack)) {
        for (const auto &sub : m_model->m_allSubtitles) {
            ids.insert(sub.first);
        }
    } else {
        for (const auto &clp : m_model->getTrackById_const(m_activeTrack)->m_allClips) {
            ids.insert(clp.first);
        }
        for (const auto &clp : m_model->getTrackById_const(m_activeTrack)->m_allCompositions) {
            ids.insert(clp.first);
        }
    }
    m_model->requestSetSelection(ids);
}

void TimelineController::deleteEffects(int targetId)
{
    std::unordered_set<int> targetIds;
    std::unordered_set<int> sel;
    if (targetId == -1) {
        sel = m_model->getCurrentSelection();
    } else {
        if (m_model->m_groups->isInGroup(targetId)) {
            sel = {m_model->m_groups->getRootId(targetId)};
        } else {
            sel = {targetId};
        }
    }
    if (sel.empty()) {
        pCore->displayMessage(i18n("No clip selected"), InformationMessage, 500);
    }
    for (int s : sel) {
        if (m_model->isGroup(s)) {
            std::unordered_set<int> sub = m_model->m_groups->getLeaves(s);
            for (int current_id : sub) {
                if (m_model->isClip(current_id)) {
                    targetIds.insert(current_id);
                }
            }
        } else if (m_model->isClip(s)) {
            targetIds.insert(s);
        }
    }
    if (targetIds.empty()) {
        pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    for (int target : targetIds) {
        std::shared_ptr<EffectStackModel> destStack = m_model->getClipEffectStackModel(target);
        destStack->removeAllEffects(undo, redo);
    }
    pCore->pushUndo(undo, redo, i18n("Delete effects"));
}

void TimelineController::pasteEffects(int targetId)
{
    std::unordered_set<int> targetIds;
    std::unordered_set<int> sel;
    if (targetId == -1) {
        sel = m_model->getCurrentSelection();
    } else {
        if (m_model->m_groups->isInGroup(targetId)) {
            sel = {m_model->m_groups->getRootId(targetId)};
        } else {
            sel = {targetId};
        }
    }
    if (sel.empty()) {
        pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
    }
    for (int s : sel) {
        if (m_model->isGroup(s)) {
            std::unordered_set<int> sub = m_model->m_groups->getLeaves(s);
            for (int current_id : sub) {
                if (m_model->isClip(current_id)) {
                    targetIds.insert(current_id);
                }
            }
        } else if (m_model->isClip(s)) {
            targetIds.insert(s);
        }
    }
    if (targetIds.empty()) {
        pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
    }

    QClipboard *clipboard = QApplication::clipboard();
    QString txt = clipboard->text();
    if (txt.isEmpty()) {
        pCore->displayMessage(i18n("No information in clipboard"), ErrorMessage, 500);
        return;
    }
    QDomDocument copiedItems;
    copiedItems.setContent(txt);
    if (copiedItems.documentElement().tagName() != QLatin1String("kdenlive-scene")) {
        pCore->displayMessage(i18n("No information in clipboard"), ErrorMessage, 500);
        return;
    }
    QDomNodeList clips = copiedItems.documentElement().elementsByTagName(QStringLiteral("clip"));
    if (clips.isEmpty()) {
        pCore->displayMessage(i18n("No information in clipboard"), ErrorMessage, 500);
        return;
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    QDomElement effects = clips.at(0).firstChildElement(QStringLiteral("effects"));
    effects.setAttribute(QStringLiteral("parentIn"), clips.at(0).toElement().attribute(QStringLiteral("in")));
    for (int i = 1; i < clips.size(); i++) {
        QDomElement subeffects = clips.at(i).firstChildElement(QStringLiteral("effects"));
        QDomNodeList subs = subeffects.childNodes();
        while (!subs.isEmpty()) {
            subs.at(0).toElement().setAttribute(QStringLiteral("parentIn"), clips.at(i).toElement().attribute(QStringLiteral("in")));
            effects.appendChild(subs.at(0));
        }
    }
    int insertedEffects = 0;
    for (int target : targetIds) {
        std::shared_ptr<EffectStackModel> destStack = m_model->getClipEffectStackModel(target);
        if (destStack->fromXml(effects, undo, redo)) {
            insertedEffects++;
        }
    }
    if (insertedEffects > 0) {
        pCore->pushUndo(undo, redo, i18n("Paste effects"));
    } else {
        pCore->displayMessage(i18n("Cannot paste effect on selected clip"), ErrorMessage, 500);
        undo();
    }
}

double TimelineController::fps() const
{
    return pCore->getCurrentFps();
}

void TimelineController::editItemDuration(int id)
{
    if (id == -1) {
        id = m_root->property("mainItemId").toInt();
        if (id == -1) {
            std::unordered_set<int> sel = m_model->getCurrentSelection();
            if (!sel.empty()) {
                id = *sel.begin();
            }
            if (id == -1) {
                pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
                return;
            }
        }
    }
    if (id == -1 || !m_model->isItem(id)) {
        pCore->displayMessage(i18n("No item to edit"), ErrorMessage, 500);
        return;
    }
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
        pCore->displayMessage(i18n("No item to edit"), ErrorMessage, 500);
        return;
    }
    int trackId = m_model->getItemTrackId(id);
    int maxFrame = qMax(0, start + duration +
                               (isComposition ? m_model->getTrackById(trackId)->getBlankSizeNearComposition(id, true)
                                              : m_model->getTrackById(trackId)->getBlankSizeNearClip(id, true)));
    int minFrame = qMax(0, in - (isComposition ? m_model->getTrackById(trackId)->getBlankSizeNearComposition(id, false)
                                               : m_model->getTrackById(trackId)->getBlankSizeNearClip(id, false)));
    int partner = isComposition ? -1 : m_model->getClipSplitPartner(id);
    QScopedPointer<ClipDurationDialog> dialog(new ClipDurationDialog(id, start, minFrame, in, in + duration, maxLength, maxFrame, qApp->activeWindow()));
    if (dialog->exec() == QDialog::Accepted) {
        std::function<bool(void)> undo = []() { return true; };
        std::function<bool(void)> redo = []() { return true; };
        int newPos = dialog->startPos().frames(pCore->getCurrentFps());
        int newIn = dialog->cropStart().frames(pCore->getCurrentFps());
        int newDuration = dialog->duration().frames(pCore->getCurrentFps());
        bool result = true;
        if (newPos < start) {
            if (!isComposition) {
                result = m_model->requestClipMove(id, trackId, newPos, true, true, true, true, undo, redo);
                if (result && partner > -1) {
                    result = m_model->requestClipMove(partner, m_model->getItemTrackId(partner), newPos, true, true, true, true, undo, redo);
                }
            } else {
                result = m_model->requestCompositionMove(id, trackId, m_model->m_allCompositions[id]->getForcedTrack(), newPos, true, true, undo, redo);
            }
            if (result && newIn != in) {
                int updatedDuration = duration + (in - newIn);
                result = m_model->requestItemResize(id, updatedDuration, false, true, undo, redo);
                if (result && partner > -1) {
                    result = m_model->requestItemResize(partner, updatedDuration, false, true, undo, redo);
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
                int updatedDuration = duration + (in - newIn);
                result = m_model->requestItemResize(id, updatedDuration, false, true, undo, redo);
                if (result && partner > -1) {
                    result = m_model->requestItemResize(partner, updatedDuration, false, true, undo, redo);
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
                    result = result && m_model->requestClipMove(id, trackId, newPos, true, true, true, true, undo, redo);
                    if (result && partner > -1) {
                        result = m_model->requestClipMove(partner, m_model->getItemTrackId(partner), newPos, true, true, true, true, undo, redo);
                    }
                } else {
                    result = result &&
                             m_model->requestCompositionMove(id, trackId, m_model->m_allCompositions[id]->getForcedTrack(), newPos, true, true, undo, redo);
                }
            }
        }
        if (result) {
            pCore->pushUndo(undo, redo, i18n("Edit item"));
        } else {
            undo();
        }
    }
    Q_EMIT regainFocus();
}

void TimelineController::focusTimelineSequence(int id)
{
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(getClipBinId(id));
    if (binClip) {
        const QUuid uuid = binClip->getSequenceUuid();
        int sequencePos = pCore->getMonitorPosition();
        sequencePos -= m_model->getClipPosition(id);
        if (sequencePos < 0 || sequencePos > m_model->getClipPlaytime(id)) {
            sequencePos = -1;
        } else {
            sequencePos += m_model->getClipIn(id);
        }
        Fun local_redo = [uuid, binId = binClip->binId(), sequencePos]() { return pCore->projectManager()->openTimeline(binId, uuid, sequencePos); };
        if (local_redo()) {
            Fun local_undo = [uuid]() {
                if (pCore->projectManager()->closeTimeline(uuid)) {
                    pCore->window()->closeTimelineTab(uuid);
                }
                return true;
            };
            pCore->pushUndo(local_undo, local_redo, i18n("Open sequence"));
        }
    }
}

void TimelineController::editTitleClip(int id)
{
    if (id == -1) {
        id = m_root->property("mainItemId").toInt();
        if (id == -1) {
            std::unordered_set<int> sel = m_model->getCurrentSelection();
            if (!sel.empty()) {
                id = *sel.begin();
            }
            if (id == -1 || !m_model->isItem(id) || !m_model->isClip(id)) {
                pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
                return;
            }
        }
    }
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(getClipBinId(id));
    if (binClip->clipType() != ClipType::Text && binClip->clipType() != ClipType::TextTemplate) {
        pCore->displayMessage(i18n("Item is not a title clip"), ErrorMessage, 500);
        return;
    }
    seekToMouse();
    pCore->bin()->showTitleWidget(binClip);
}

void TimelineController::editAnimationClip(int id)
{
    if (id == -1) {
        id = m_root->property("mainItemId").toInt();
        if (id == -1) {
            std::unordered_set<int> sel = m_model->getCurrentSelection();
            if (!sel.empty()) {
                id = *sel.begin();
            }
            if (id == -1 || !m_model->isItem(id) || !m_model->isClip(id)) {
                pCore->displayMessage(i18n("No clip selected"), ErrorMessage, 500);
                return;
            }
        }
    }
    GlaxnimateLauncher::instance().openClip(id);
}

QPoint TimelineController::selectionInOut() const
{
    std::unordered_set<int> ids = m_model->getCurrentSelection();
    std::unordered_set<int> items_list;
    for (int i : ids) {
        if (m_model->isGroup(i)) {
            std::unordered_set<int> children = m_model->m_groups->getLeaves(i);
            items_list.insert(children.begin(), children.end());
        } else {
            items_list.insert(i);
        }
    }
    int in = -1;
    int out = -1;
    for (int id : items_list) {
        if (m_model->isClip(id) || m_model->isComposition(id)) {
            int itemIn = m_model->getItemPosition(id);
            int itemOut = itemIn + m_model->getItemPlaytime(id) - 1;
            if (in < 0 || itemIn < in) {
                in = itemIn;
            }
            if (itemOut > out) {
                out = itemOut;
            }
        }
    }
    return QPoint(in, out);
}

void TimelineController::updateClipActions()
{
    if (m_model->getCurrentSelection().empty()) {
        for (QAction *act : qAsConst(clipActions)) {
            const QChar actionData = act->data().toChar();
            if (actionData == QLatin1Char('P')) {
                // Position actions should stay enabled in clip monitor
                act->setEnabled(true);
            } else {
                act->setEnabled(false);
            }
        }
        Q_EMIT timelineClipSelected(false);
        // nothing selected
        Q_EMIT showItemEffectStack(QString(), nullptr, QSize(), false);
        pCore->timeRemapWidget()->selectedClip(-1, QUuid());
        Q_EMIT showSubtitle(-1);
        pCore->displaySelectionMessage(QString());
        return;
    }
    std::shared_ptr<ClipModel> clip(nullptr);
    std::unordered_set<int> selectedItems = m_model->getCurrentSelection();
    int item = *selectedItems.begin();
    int selectionSize = selectedItems.size();
    if (selectionSize == 1) {
        if (m_model->isClip(item) || m_model->isComposition(item)) {
            showAsset(item);
            Q_EMIT showSubtitle(-1);
        } else if (m_model->isSubTitle(item)) {
            Q_EMIT showSubtitle(item);
        }
        pCore->displaySelectionMessage(QString());
    } else {
        int min = -1;
        int max = -1;
        for (const auto &id : selectedItems) {
            int itemPos = m_model->getItemPosition(id);
            int itemOut = itemPos + m_model->getItemPlaytime(id);
            if (min == -1 || itemPos < min) {
                min = itemPos;
            }
            if (max == -1 || itemOut > max) {
                max = itemOut;
            }
        }
        pCore->displaySelectionMessage(i18n("%1 items selected (%2) |", selectionSize, simplifiedTC(max - min)));
    }
    if (m_model->isClip(item)) {
        clip = m_model->getClipPtr(item);
        if (clip->hasTimeRemap()) {
            Q_EMIT pCore->remapClip(item);
        }
    }
    bool isInGroup = m_model->m_groups->isInGroup(item);
    PlaylistState::ClipState state = PlaylistState::ClipState::Unknown;
    ClipType::ProducerType type = ClipType::Unknown;
    if (clip) {
        state = clip->clipState();
        type = clip->clipType();
    }
    for (QAction *act : qAsConst(clipActions)) {
        bool enableAction = true;
        const QChar actionData = act->data().toChar();
        if (actionData == QLatin1Char('G')) {
            enableAction = isInSelection(item) && selectionSize > 1;
        } else if (actionData == QLatin1Char('U')) {
            enableAction = isInGroup;
        } else if (actionData == QLatin1Char('A')) {
            if (isInGroup && m_model->m_groups->getType(m_model->m_groups->getRootId(item)) == GroupType::AVSplit) {
                enableAction = true;
            } else {
                enableAction = state == PlaylistState::AudioOnly;
            }
        } else if (actionData == QLatin1Char('V')) {
            enableAction = state == PlaylistState::VideoOnly;
        } else if (actionData == QLatin1Char('D')) {
            enableAction = state == PlaylistState::Disabled;
        } else if (actionData == QLatin1Char('E')) {
            enableAction = state != PlaylistState::Disabled && state != PlaylistState::Unknown;
        } else if (actionData == QLatin1Char('X') || actionData == QLatin1Char('S')) {
            enableAction = clip && clip->canBeVideo() && clip->canBeAudio();
            if (enableAction && actionData == QLatin1Char('S')) {
                if (isInGroup) {
                    // Check if all clips in the group have have same state (audio or video)
                    int targetRoot = m_model->m_groups->getRootId(item);
                    if (m_model->isGroup(targetRoot)) {
                        std::unordered_set<int> sub = m_model->m_groups->getLeaves(targetRoot);
                        for (int current_id : sub) {
                            if (current_id == item) {
                                continue;
                            }
                            if (m_model->isClip(current_id) && m_model->getClipPtr(current_id)->clipState() != state) {
                                // Group with audio and video clips, disable split action
                                enableAction = false;
                                break;
                            }
                        }
                    }
                }
                act->setText(state == PlaylistState::AudioOnly ? i18n("Restore video") : i18n("Restore audio"));
            }
        } else if (actionData == QLatin1Char('W')) {
            enableAction = clip != nullptr;
            if (enableAction) {
                act->setText(clip->clipState() == PlaylistState::Disabled ? i18n("Enable clip") : i18n("Disable clip"));
            }
        } else if (actionData == QLatin1Char('C') && clip == nullptr) {
            enableAction = false;
        } else if (actionData == QLatin1Char('P')) {
            // Position actions should stay enabled in clip monitor
            enableAction = true;
        } else if (actionData == QLatin1Char('R')) {
            // Time remap action
            enableAction = clip != nullptr && type != ClipType::Color && type != ClipType::Image && qFuzzyCompare(1., m_model->m_allClips[item]->getSpeed());
            if (enableAction) {
                act->setChecked(clip->hasTimeRemap());
            }
        } else if (actionData == QLatin1Char('Q')) {
            // Speed change action
            enableAction = clip != nullptr && (clip->getSpeed() != 1. || (type != ClipType::Timeline && type != ClipType::Playlist && type != ClipType::Color &&
                                                                          type != ClipType::Image && !clip->hasTimeRemap()));
        }
        act->setEnabled(enableAction);
    }
    Q_EMIT timelineClipSelected(clip != nullptr);
}

const QString TimelineController::getAssetName(const QString &assetId, bool isTransition)
{
    return isTransition ? TransitionsRepository::get()->getName(assetId) : EffectsRepository::get()->getName(assetId);
}

void TimelineController::grabCurrent()
{
    if (trimmingActive()) {
        return;
    }
    std::unordered_set<int> ids = m_model->getCurrentSelection();
    std::unordered_set<int> items_list;
    int mainId = -1;
    for (int i : ids) {
        if (m_model->isGroup(i)) {
            std::unordered_set<int> children = m_model->m_groups->getLeaves(i);
            items_list.insert(children.begin(), children.end());
        } else {
            items_list.insert(i);
        }
    }
    for (int id : items_list) {
        if (mainId == -1 && m_model->getItemTrackId(id) == m_activeTrack) {
            mainId = id;
            continue;
        }
        if (m_model->isClip(id)) {
            std::shared_ptr<ClipModel> clip = m_model->getClipPtr(id);
            clip->setGrab(!clip->isGrabbed());
        } else if (m_model->isComposition(id)) {
            std::shared_ptr<CompositionModel> clip = m_model->getCompositionPtr(id);
            clip->setGrab(!clip->isGrabbed());
        } else if (m_model->isSubTitle(id)) {
            m_model->getSubtitleModel()->switchGrab(id);
        }
    }
    if (mainId > -1) {
        if (m_model->isClip(mainId)) {
            std::shared_ptr<ClipModel> clip = m_model->getClipPtr(mainId);
            clip->setGrab(!clip->isGrabbed());
        } else if (m_model->isComposition(mainId)) {
            std::shared_ptr<CompositionModel> clip = m_model->getCompositionPtr(mainId);
            clip->setGrab(!clip->isGrabbed());
        } else if (m_model->isSubTitle(mainId)) {
            m_model->getSubtitleModel()->switchGrab(mainId);
        }
    }
}

int TimelineController::getItemMovingTrack(int itemId) const
{
    if (m_model->isClip(itemId)) {
        int trackId = -1;
        if (m_model->m_editMode != TimelineMode::NormalEdit) {
            trackId = m_model->m_allClips[itemId]->getFakeTrackId();
        }
        return trackId < 0 ? m_model->m_allClips[itemId]->getCurrentTrackId() : trackId;
    }
    return m_model->m_allCompositions[itemId]->getCurrentTrackId();
}

bool TimelineController::endFakeMove(int clipId, int position, bool updateView, bool logUndo, bool invalidateTimeline)
{
    Q_ASSERT(m_model->m_allClips.count(clipId) > 0);
    int trackId = m_model->m_allClips[clipId]->getFakeTrackId();
    if (m_model->getClipPosition(clipId) == position && m_model->getClipTrackId(clipId) == trackId) {
        qDebug() << "* * ** END FAKE; NO MOVE RQSTED";
        // Ensure clip height binds again with parent track height
        if (m_model->m_groups->isInGroup(clipId)) {
            int groupId = m_model->m_groups->getRootId(clipId);
            auto all_items = m_model->m_groups->getLeaves(groupId);
            for (int item : all_items) {
                if (m_model->isClip(item)) {
                    m_model->m_allClips[item]->setFakeTrackId(-1);
                    QModelIndex modelIndex = m_model->makeClipIndexFromID(item);
                    if (modelIndex.isValid()) {
                        m_model->notifyChange(modelIndex, modelIndex, TimelineModel::FakeTrackIdRole);
                    }
                }
            }
        } else {
            m_model->m_allClips[clipId]->setFakeTrackId(-1);
            QModelIndex modelIndex = m_model->makeClipIndexFromID(clipId);
            if (modelIndex.isValid()) {
                m_model->notifyChange(modelIndex, modelIndex, TimelineModel::FakeTrackIdRole);
            }
        }
        return true;
    }
    if (m_model->m_groups->isInGroup(clipId)) {
        // element is in a group.
        int groupId = m_model->m_groups->getRootId(clipId);
        int current_trackId = m_model->getClipTrackId(clipId);
        int track_pos1 = m_model->getTrackPosition(trackId);
        int track_pos2 = m_model->getTrackPosition(current_trackId);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_model->m_allClips[clipId]->getPosition();
        return endFakeGroupMove(clipId, groupId, delta_track, delta_pos, updateView, logUndo);
    }
    qDebug() << "//////\n//////\nENDING FAKE MOVE: " << trackId << ", POS: " << position;
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    int startPos = m_model->getClipPosition(clipId);
    int duration = m_model->getClipPlaytime(clipId);
    int currentTrack = m_model->m_allClips[clipId]->getCurrentTrackId();
    bool res = true;
    if (currentTrack > -1) {
        std::pair<MixInfo, MixInfo> mixData = m_model->getTrackById_const(currentTrack)->getMixInfo(clipId);
        if (mixData.first.firstClipId > -1) {
            m_model->removeMixWithUndo(mixData.first.secondClipId, undo, redo);
        }
        if (mixData.second.firstClipId > -1) {
            m_model->removeMixWithUndo(mixData.second.secondClipId, undo, redo);
        }
        res = m_model->getTrackById(currentTrack)->requestClipDeletion(clipId, updateView, invalidateTimeline, undo, redo, false, false);
    }
    if (m_model->m_editMode == TimelineMode::OverwriteEdit) {
        res = res && TimelineFunctions::liftZone(m_model, trackId, QPoint(position, position + duration), undo, redo);
    } else if (m_model->m_editMode == TimelineMode::InsertEdit) {
        // Remove space from previous location
        if (currentTrack > -1) {
            res = res && TimelineFunctions::removeSpace(m_model, {startPos, startPos + duration}, undo, redo, {currentTrack}, false);
        }
        int startClipId = m_model->getClipByPosition(trackId, position);
        if (startClipId > -1) {
            // There is a clip at insert pos
            if (m_model->getClipPosition(startClipId) != position) {
                // If position is in the middle of the clip, cut it
                res = res && TimelineFunctions::requestClipCut(m_model, startClipId, position, undo, redo);
            }
        }
        res = res && TimelineFunctions::requestInsertSpace(m_model, QPoint(position, position + duration), undo, redo, {trackId});
    }
    res = res && m_model->getTrackById(trackId)->requestClipInsertion(clipId, position, updateView, invalidateTimeline, undo, redo, false, false);
    if (res) {
        // Terminate fake move
        if (m_model->isClip(clipId)) {
            m_model->m_allClips[clipId]->setFakeTrackId(-1);
        }
        if (logUndo) {
            pCore->pushUndo(undo, redo, i18n("Move item"));
        }
    } else {
        qDebug() << "//// FAKE FAILED";
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
        // Terminate fake move
        if (m_model->isClip(clipId)) {
            m_model->m_allClips[clipId]->setFakeTrackId(-1);
        }
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
    std::vector<int> sorted_clips{std::make_move_iterator(std::begin(all_items)), std::make_move_iterator(std::end(all_items))};
    std::sort(sorted_clips.begin(), sorted_clips.end(), [this](const int &clipId1, const int &clipId2) {
        int p1 = m_model->isClip(clipId1) ? m_model->m_allClips[clipId1]->getPosition() : m_model->m_allCompositions[clipId1]->getPosition();
        int p2 = m_model->isClip(clipId2) ? m_model->m_allClips[clipId2]->getPosition() : m_model->m_allCompositions[clipId2]->getPosition();
        return p2 < p1;
    });

    // Moving groups is a two stage process: first we remove the clips from the tracks, and then try to insert them back at their calculated new positions.
    // This way, we ensure that no conflict will arise with clips inside the group being moved
    // We are moving a group on another track, delete and re-add

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
    std::vector<int> affected_trackIds;
    std::unordered_map<int, std::pair<QString, QVector<QPair<QString, QVariant>>>> mixToMove;
    std::unordered_map<int, MixInfo> mixInfoToMove;
    std::unordered_map<int, std::pair<int, int>> mixTracksToMove;
    // Remove mixes not part of the group move
    for (int item : sorted_clips) {
        if (m_model->isClip(item)) {
            int tid = m_model->getItemTrackId(item);
            affected_trackIds.emplace_back(tid);
            std::pair<MixInfo, MixInfo> mixData = m_model->getTrackById_const(tid)->getMixInfo(item);
            if (mixData.first.firstClipId > -1) {
                if (std::find(sorted_clips.begin(), sorted_clips.end(), mixData.first.firstClipId) == sorted_clips.end()) {
                    // Clip has startMix
                    m_model->removeMixWithUndo(mixData.first.secondClipId, undo, redo);
                } else {
                    // Get mix properties
                    std::pair<QString, QVector<QPair<QString, QVariant>>> mixParams =
                        m_model->getTrackById_const(tid)->getMixParams(mixData.first.secondClipId);
                    mixToMove[item] = mixParams;
                    mixTracksToMove[item] = m_model->getTrackById_const(tid)->getMixTracks(mixData.first.secondClipId);
                    mixInfoToMove[item] = mixData.first;
                }
            }
            if (mixData.second.firstClipId > -1 && std::find(sorted_clips.begin(), sorted_clips.end(), mixData.second.secondClipId) == sorted_clips.end()) {
                m_model->removeMixWithUndo(mixData.second.secondClipId, undo, redo);
            }
        }
    }
    for (int item : sorted_clips) {
        int old_trackId = m_model->getItemTrackId(item);
        old_track_ids[item] = old_trackId;
        if (old_trackId != -1) {
            if (m_model->isClip(item)) {
                int current_track_position = m_model->getTrackPosition(old_trackId);
                int d = m_model->getTrackById_const(old_trackId)->isAudioTrack() ? audio_delta : video_delta;
                int target_track_position = current_track_position + d;
                auto it = m_model->m_allTracks.cbegin();
                std::advance(it, target_track_position);
                int target_track = (*it)->getId();
                new_track_ids[item] = target_track;
                affected_trackIds.emplace_back(target_track);
                old_position[item] = m_model->m_allClips[item]->getPosition();
                int duration = m_model->m_allClips[item]->getPlaytime();
                min = min < 0 ? old_position[item] + delta_pos : qMin(min, old_position[item] + delta_pos);
                max = max < 0 ? old_position[item] + delta_pos + duration : qMax(max, old_position[item] + delta_pos + duration);
                ok = ok && m_model->getTrackById(old_trackId)->requestClipDeletion(item, true, finalMove, undo, redo, false, false);
                if (m_model->m_editMode == TimelineMode::InsertEdit) {
                    // Lift space left by removed clip
                    ok = ok && TimelineFunctions::removeSpace(m_model, {old_position[item], old_position[item] + duration}, undo, redo, {old_trackId}, false);
                }
            } else {
                // ok = ok && getTrackById(old_trackId)->requestCompositionDeletion(item, updateThisView, local_undo, local_redo);
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
                res = res && TimelineFunctions::liftZone(m_model, target_track, QPoint(target_position, target_position + duration), undo, redo);
            }
        }
    } else if (m_model->m_editMode == TimelineMode::InsertEdit) {
        QVector<int> processedTracks;
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
                res = res && TimelineFunctions::requestClipCut(m_model, startClipId, target_position, undo, redo);
            }
        }
        res = res && TimelineFunctions::requestInsertSpace(m_model, QPoint(min, max), undo, redo, processedTracks);
    }
    for (int item : sorted_clips) {
        if (m_model->isClip(item)) {
            int target_track = new_track_ids[item];
            int target_position = old_position[item] + delta_pos;
            ok = ok && m_model->requestClipMove(item, target_track, target_position, true, updateView, finalMove, finalMove, undo, redo);
        } else {
            // ok = ok && requestCompositionMove(item, target_track, old_forced_track[item], target_position, updateThisView, local_undo, local_redo);
        }
        if (!ok) {
            bool undone = undo();
            Q_ASSERT(undone);
            return false;
        }
    }

    if (delta_track == 0) {
        Fun sync_mix = [this, affected_trackIds, finalMove]() {
            for (int t : affected_trackIds) {
                m_model->getTrackById_const(t)->syncronizeMixes(finalMove);
            }
            return true;
        };
        sync_mix();
        PUSH_LAMBDA(sync_mix, redo);
        return true;
    }
    for (int item : sorted_clips) {
        if (mixToMove.find(item) == mixToMove.end()) {
            continue;
        }
        int trackId = new_track_ids[item];
        int previous_track = old_track_ids[item];
        MixInfo mixData = mixInfoToMove[item];
        std::pair<int, int> mixTracks = mixTracksToMove[item];
        std::pair<QString, QVector<QPair<QString, QVariant>>> mixParams = mixToMove[item];
        Fun simple_move_mix = [this, previous_track, trackId, finalMove, mixData, mixTracks, mixParams]() {
            // Insert mix on new track
            bool result = m_model->getTrackById_const(trackId)->createMix(mixData, mixParams, mixTracks, finalMove);
            // Remove mix on old track
            m_model->getTrackById_const(previous_track)->removeMix(mixData);
            return result;
        };
        Fun simple_restore_mix = [this, previous_track, trackId, finalMove, mixData, mixTracks, mixParams]() {
            bool result = m_model->getTrackById_const(previous_track)->createMix(mixData, mixParams, mixTracks, finalMove);
            // Remove mix on old track
            m_model->getTrackById_const(trackId)->removeMix(mixData);
            return result;
        };
        simple_move_mix();
        if (finalMove) {
            PUSH_LAMBDA(simple_restore_mix, undo);
            PUSH_LAMBDA(simple_move_mix, redo);
        }
    }
    return true;
}

const std::unordered_map<QString, std::vector<int>> TimelineController::getThumbKeys()
{
    std::unordered_map<QString, std::vector<int>> framesToStore;
    for (const auto &clp : m_model->m_allClips) {
        if (clp.second->isAudioOnly()) {
            // Don't process audio clips
            continue;
        }
        const QString binId = getClipBinId(clp.first);
        framesToStore[binId].push_back(clp.second->getIn());
        framesToStore[binId].push_back(clp.second->getOut());
    }
    return framesToStore;
}

bool TimelineController::isInSelection(int itemId)
{
    return m_model->getCurrentSelection().count(itemId) > 0;
}

bool TimelineController::exists(int itemId)
{
    return m_model->isClip(itemId) || m_model->isComposition(itemId);
}

void TimelineController::slotMultitrackView(bool enable, bool refresh)
{
    QStringList trackNames = TimelineFunctions::enableMultitrackView(m_model, enable, refresh);
    if (!refresh) {
        // This is just a temporary state (disable multitrack view for playlist save, don't change scene
        return;
    }
    pCore->monitorManager()->projectMonitor()->slotShowEffectScene(enable ? MonitorSplitTrack : MonitorSceneNone, false, QVariant(trackNames));
    QObject::disconnect(m_connection);
    if (enable) {
        connect(m_model.get(), &TimelineItemModel::trackVisibilityChanged, this, &TimelineController::updateMultiTrack, Qt::UniqueConnection);
        m_connection = connect(this, &TimelineController::activeTrackChanged, [this]() {
            int ix = 0;
            auto it = m_model->m_allTracks.cbegin();
            while (it != m_model->m_allTracks.cend()) {
                int target_track = (*it)->getId();
                ++it;
                if (target_track == m_activeTrack) {
                    break;
                }
                if (m_model->getTrackById_const(target_track)->isAudioTrack() || m_model->getTrackById_const(target_track)->isHidden()) {
                    continue;
                }
                ++ix;
            }
            pCore->monitorManager()->projectMonitor()->updateMultiTrackView(ix);
        });
        int ix = 0;
        auto it = m_model->m_allTracks.cbegin();
        while (it != m_model->m_allTracks.cend()) {
            int target_track = (*it)->getId();
            ++it;
            if (target_track == m_activeTrack) {
                break;
            }
            if (m_model->getTrackById_const(target_track)->isAudioTrack() || m_model->getTrackById_const(target_track)->isHidden()) {
                continue;
            }
            ++ix;
        }
        pCore->monitorManager()->projectMonitor()->updateMultiTrackView(ix);
    } else {
        disconnect(m_model.get(), &TimelineItemModel::trackVisibilityChanged, this, &TimelineController::updateMultiTrack);
    }
}

void TimelineController::updateMultiTrack()
{
    QStringList trackNames = TimelineFunctions::enableMultitrackView(m_model, true, true);
    pCore->monitorManager()->projectMonitor()->slotShowEffectScene(MonitorSplitTrack, false, QVariant(trackNames));
}

void TimelineController::activateTrackAndSelect(int trackPosition, bool notesMode)
{
    int tid = -1;
    int ix = 0;
    if (notesMode && trackPosition == -2) {
        m_activeTrack = -2;
        Q_EMIT activeTrackChanged();
        return;
    }
    auto it = m_model->m_allTracks.cbegin();
    while (it != m_model->m_allTracks.cend()) {
        tid = (*it)->getId();
        ++it;
        if (!notesMode && (m_model->getTrackById_const(tid)->isAudioTrack() || m_model->getTrackById_const(tid)->isHidden())) {
            continue;
        }
        if (trackPosition == ix) {
            break;
        }
        ++ix;
    }
    if (tid > -1) {
        m_activeTrack = tid;
        Q_EMIT activeTrackChanged();
        if (!notesMode && pCore->window()->getCurrentTimeline()->activeTool() != ToolType::MulticamTool) {
            selectCurrentItem(KdenliveObjectType::TimelineClip, true);
        }
    }
}

void TimelineController::saveTimelineSelection(const QDir &targetDir)
{
    std::unordered_set<int> ids = m_model->getCurrentSelection();
    std::unordered_set<int> items_list;
    for (int i : ids) {
        if (m_model->isGroup(i)) {
            std::unordered_set<int> children = m_model->m_groups->getLeaves(i);
            items_list.insert(children.begin(), children.end());
        } else {
            items_list.insert(i);
        }
    }
    int startPos = 0;
    int endPos = 0;
    for (int id : items_list) {
        int start = m_model->getItemPosition(id);
        int end = start + m_model->getItemPlaytime(id);
        if (startPos == 0 || start < startPos) {
            startPos = start;
        }
        if (end > endPos) {
            endPos = end;
        }
    }
    TimelineFunctions::saveTimelineSelection(m_model, m_model->getCurrentSelection(), targetDir, endPos - startPos - 1);
}

void TimelineController::addEffectKeyframe(int cid, int frame, double val)
{
    if (m_model->isClip(cid)) {
        std::shared_ptr<EffectStackModel> destStack = m_model->getClipEffectStackModel(cid);
        destStack->addEffectKeyFrame(frame, val);
    } else if (m_model->isComposition(cid)) {
        std::shared_ptr<KeyframeModelList> listModel = m_model->m_allCompositions[cid]->getKeyframeModel();
        listModel->addKeyframe(frame, val);
    }
}

void TimelineController::removeEffectKeyframe(int cid, int frame)
{
    if (m_model->isClip(cid)) {
        std::shared_ptr<EffectStackModel> destStack = m_model->getClipEffectStackModel(cid);
        destStack->removeKeyFrame(frame);
    } else if (m_model->isComposition(cid)) {
        std::shared_ptr<KeyframeModelList> listModel = m_model->m_allCompositions[cid]->getKeyframeModel();
        listModel->removeKeyframe(GenTime(frame, pCore->getCurrentFps()));
    }
}

void TimelineController::updateEffectKeyframe(int cid, int oldFrame, int newFrame, const QVariant &normalizedValue)
{
    if (m_model->isClip(cid)) {
        std::shared_ptr<EffectStackModel> destStack = m_model->getClipEffectStackModel(cid);
        destStack->updateKeyFrame(oldFrame, newFrame, normalizedValue);
    } else if (m_model->isComposition(cid)) {
        std::shared_ptr<KeyframeModelList> listModel = m_model->m_allCompositions[cid]->getKeyframeModel();
        listModel->updateKeyframe(GenTime(oldFrame, pCore->getCurrentFps()), GenTime(newFrame, pCore->getCurrentFps()), normalizedValue);
    }
}

bool TimelineController::hasKeyframeAt(int cid, int frame)
{
    if (m_model->isClip(cid)) {
        std::shared_ptr<EffectStackModel> destStack = m_model->getClipEffectStackModel(cid);
        return destStack->hasKeyFrame(frame);
    } else if (m_model->isComposition(cid)) {
        std::shared_ptr<KeyframeModelList> listModel = m_model->m_allCompositions[cid]->getKeyframeModel();
        return listModel->hasKeyframe(frame);
    }
    return false;
}

QColor TimelineController::videoColor() const
{
    KColorScheme scheme(QApplication::palette().currentColorGroup());
    return scheme.foreground(KColorScheme::LinkText).color();
}

QColor TimelineController::targetColor() const
{
    KColorScheme scheme(QApplication::palette().currentColorGroup());
    QColor base = scheme.foreground(KColorScheme::PositiveText).color();
    QColor high = QApplication::palette().highlightedText().color();
    double factor = 0.3;
    QColor res = QColor(qBound(0, base.red() + int(factor * (high.red() - 128)), 255), qBound(0, base.green() + int(factor * (high.green() - 128)), 255),
                        qBound(0, base.blue() + int(factor * (high.blue() - 128)), 255), 255);
    return res;
}

QColor TimelineController::targetTextColor() const
{
    KColorScheme scheme(QApplication::palette().currentColorGroup());
    return scheme.background(KColorScheme::PositiveBackground).color();
}

QColor TimelineController::audioColor() const
{
    KColorScheme scheme(QApplication::palette().currentColorGroup());
    return scheme.foreground(KColorScheme::PositiveText).color().darker(200);
}

QColor TimelineController::titleColor() const
{
    KColorScheme scheme(QApplication::palette().currentColorGroup());
    QColor base = scheme.foreground(KColorScheme::LinkText).color();
    QColor high = scheme.foreground(KColorScheme::NegativeText).color();
    QColor title = QColor(qBound(0, base.red() + int(high.red() - 128), 255), qBound(0, base.green() + int(high.green() - 128), 255),
                          qBound(0, base.blue() + int(high.blue() - 128), 255), 255);
    return title;
}

QColor TimelineController::imageColor() const
{
    KColorScheme scheme(QApplication::palette().currentColorGroup());
    return scheme.foreground(KColorScheme::NeutralText).color();
}

QColor TimelineController::thumbColor1() const
{
    return KdenliveSettings::thumbColor1();
}

QColor TimelineController::thumbColor2() const
{
    return KdenliveSettings::thumbColor2();
}

QColor TimelineController::slideshowColor() const
{
    KColorScheme scheme(QApplication::palette().currentColorGroup());
    QColor base = scheme.foreground(KColorScheme::LinkText).color();
    QColor high = scheme.foreground(KColorScheme::NeutralText).color();
    QColor slide = QColor(qBound(0, base.red() + int(high.red() - 128), 255), qBound(0, base.green() + int(high.green() - 128), 255),
                          qBound(0, base.blue() + int(high.blue() - 128), 255), 255);
    return slide;
}

QColor TimelineController::lockedColor() const
{
    KColorScheme scheme(QApplication::palette().currentColorGroup());
    return scheme.foreground(KColorScheme::NegativeText).color();
}

QColor TimelineController::groupColor() const
{
    KColorScheme scheme(QApplication::palette().currentColorGroup());
    return scheme.foreground(KColorScheme::ActiveText).color();
}

QColor TimelineController::selectionColor() const
{
    KColorScheme scheme(QApplication::palette().currentColorGroup(), KColorScheme::Complementary);
    if (m_model && m_model->singleSelectionMode()) {
        return Qt::red;
    }
    return scheme.foreground(KColorScheme::NeutralText).color();
}

void TimelineController::switchRecording(int trackId, bool record)
{
    if (trackId == -1) {
        trackId = pCore->mixer()->recordTrack();
    }
    if (record) {
        if (pCore->isMediaCapturing()) {
            // Already recording, abort
            return;
        }
        qDebug() << "start recording" << trackId;
        if (!m_model->isTrack(trackId)) {
            qDebug() << "ERROR: Starting to capture on invalid track " << trackId;
        }
        if (m_model->getTrackById_const(trackId)->isLocked()) {
            pCore->displayMessage(i18n("Impossible to capture on a locked track"), ErrorMessage, 500);
            return;
        }
        m_recordStart.first = pCore->getMonitorPosition();
        m_recordTrack = trackId;
        int maximumSpace = m_model->getTrackById_const(trackId)->getBlankEnd(m_recordStart.first);
        if (maximumSpace == INT_MAX) {
            m_recordStart.second = 0;
        } else {
            m_recordStart.second = maximumSpace - m_recordStart.first;
            if (m_recordStart.second < 8) {
                pCore->displayMessage(i18n("Impossible to capture here: the capture could override clips. Please remove clips after the current position or "
                                           "choose a different track"),
                                      ErrorMessage, 500);
                return;
            }
        }
        pCore->monitorManager()->slotSwitchMonitors(false);
        pCore->startMediaCapture(trackId, true, false);
        if (KdenliveSettings::disablereccountdown()) {
            pCore->startRecording();
        } else {
            pCore->getMonitor(Kdenlive::ProjectMonitor)->startCountDown();
        }

    } else {
        pCore->getMonitor(Kdenlive::ProjectMonitor)->stopCountDown();
        pCore->stopMediaCapture(trackId, true, false);
        Q_EMIT stopAudioRecord();
        pCore->monitorManager()->slotPause();
    }
}

void TimelineController::urlDropped(QStringList droppedFile, int frame, int tid)
{
    if (droppedFile.isEmpty()) {
        // Empty url passed, abort
        return;
    }
    m_recordTrack = tid;
    m_recordStart = {frame, -1};
    qDebug() << "=== GOT DROPPED FILED: " << droppedFile << "\n======";
    if (droppedFile.first().endsWith(QLatin1String(".ass")) || droppedFile.first().endsWith(QLatin1String(".srt"))) {
        // Subtitle dropped, import
        pCore->window()->showSubtitleTrack();
        importSubtitle(QUrl(droppedFile.first()).toLocalFile());
    } else {
        addAndInsertFile(QUrl(droppedFile.first()).toLocalFile(), false, true);
    }
}

void TimelineController::finishRecording(const QString &recordedFile)
{
    addAndInsertFile(recordedFile, true, false);
}

void TimelineController::addAndInsertFile(const QString &recordedFile, const bool isAudioClip, const bool highlightClip)
{
    if (recordedFile.isEmpty()) {
        return;
    }

    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    std::function<void(const QString &)> callBack = [this, highlightClip](const QString &binId) {
        int id = -1;
        if (m_recordTrack == -1) {
            return;
        }
        std::shared_ptr<ProjectClip> clip = pCore->bin()->getBinClip(binId);
        if (!clip) {
            return;
        }
        if (highlightClip) {
            pCore->activeBin()->selectClipById(binId);
        }
        qDebug() << "callback " << binId << " " << m_recordTrack << ", MAXIMUM SPACE: " << m_recordStart.second;
        if (m_recordStart.second > 0) {
            // Limited space on track
            m_recordStart.second = qMin(int(clip->frameDuration() - 1), m_recordStart.second);
            QString binClipId = QString("%1/%2/%3").arg(binId).arg(0).arg(m_recordStart.second);
            m_model->requestClipInsertion(binClipId, m_recordTrack, m_recordStart.first, id, true, true, false);
            m_recordStart.second++;
        } else {
            m_recordStart.second = clip->frameDuration();
            m_model->requestClipInsertion(binId, m_recordTrack, m_recordStart.first, id, true, true, false);
        }
        setPosition(m_recordStart.first + m_recordStart.second);
    };
    std::shared_ptr<ProjectItemModel> itemModel = pCore->projectItemModel();
    std::shared_ptr<ProjectFolder> targetFolder = itemModel->getRootFolder();
    if (isAudioClip && itemModel->defaultAudioCaptureFolder() > -1) {
        const QString audioCaptureFolder = QString::number(itemModel->defaultAudioCaptureFolder());
        std::shared_ptr<ProjectFolder> folderItem = itemModel->getFolderByBinId(audioCaptureFolder);
        if (folderItem) {
            targetFolder = folderItem;
        }
    }

    QString binId =
        ClipCreator::createClipFromFile(recordedFile, targetFolder->clipId(), pCore->projectItemModel(), undo, redo, callBack);

    if (highlightClip) {
        pCore->window()->raiseBin();
    }

    if (binId != QStringLiteral("-1")) {
        pCore->pushUndo(undo, redo, i18n("Record audio"));
    }
}

void TimelineController::updateVideoTarget()
{
    if (videoTarget() > -1) {
        m_lastVideoTarget = videoTarget();
        m_videoTargetActive = true;
        Q_EMIT lastVideoTargetChanged();
    } else {
        m_videoTargetActive = false;
    }
}

void TimelineController::updateAudioTarget()
{
    if (!audioTarget().isEmpty()) {
        m_lastAudioTarget = m_model->m_audioTarget;
        m_audioTargetActive = true;
        Q_EMIT lastAudioTargetChanged();
    } else {
        m_audioTargetActive = false;
    }
}

bool TimelineController::hasActiveTracks() const
{
    auto it = m_model->m_allTracks.cbegin();
    while (it != m_model->m_allTracks.cend()) {
        int target_track = (*it)->getId();
        if (m_model->getTrackById_const(target_track)->shouldReceiveTimelineOp()) {
            return true;
        }
        ++it;
    }
    return false;
}

void TimelineController::showMasterEffects()
{
    Q_EMIT showItemEffectStack(i18n("Master effects"), m_model->getMasterEffectStackModel(), pCore->getCurrentFrameSize(), false);
}

bool TimelineController::refreshIfVisible(int cid)
{
    auto it = m_model->m_allTracks.cbegin();
    while (it != m_model->m_allTracks.cend()) {
        int target_track = (*it)->getId();
        if (m_model->getTrackById_const(target_track)->isAudioTrack() || m_model->getTrackById_const(target_track)->isHidden()) {
            ++it;
            continue;
        }
        int child = m_model->getClipByPosition(target_track, pCore->getMonitorPosition());
        if (child > 0) {
            if (m_model->m_allClips[child]->binId().toInt() == cid) {
                return true;
            }
        }
        ++it;
    }
    return false;
}

void TimelineController::collapseActiveTrack()
{
    if (m_activeTrack == -1) {
        return;
    }
    if (m_model->isSubtitleTrack(m_activeTrack)) {
        // Subtitle track
        QMetaObject::invokeMethod(m_root, "switchSubtitleTrack", Qt::QueuedConnection);
        return;
    }
    int collapsed = m_model->getTrackProperty(m_activeTrack, QStringLiteral("kdenlive:collapsed")).toInt();
    // Default unit for timeline.qml objects size
    int baseUnit = qMax(28, qRound(QFontInfo(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont)).pixelSize() * 1.8));
    m_model->setTrackProperty(m_activeTrack, QStringLiteral("kdenlive:collapsed"), collapsed > 0 ? QStringLiteral("0") : QString::number(baseUnit));
}

void TimelineController::setActiveTrackProperty(const QString &name, const QString &value)
{
    if (m_activeTrack > -1) {
        m_model->setTrackProperty(m_activeTrack, name, value);
    }
}

bool TimelineController::isActiveTrackAudio() const
{
    if (m_activeTrack > -1) {
        if (m_model->getTrackById_const(m_activeTrack)->isAudioTrack()) {
            return true;
        }
    }
    return false;
}

const QVariant TimelineController::getActiveTrackProperty(const QString &name) const
{
    if (m_activeTrack > -1) {
        return m_model->getTrackProperty(m_activeTrack, name);
    }
    return QVariant();
}

void TimelineController::expandActiveClip()
{
    std::unordered_set<int> ids = m_model->getCurrentSelection();
    std::unordered_set<int> items_list;
    for (int i : ids) {
        if (m_model->isGroup(i)) {
            std::unordered_set<int> children = m_model->m_groups->getLeaves(i);
            items_list.insert(children.begin(), children.end());
        } else {
            items_list.insert(i);
        }
    }
    m_model->requestClearSelection();
    bool result = true;
    for (int id : items_list) {
        if (result && m_model->isClip(id)) {
            std::shared_ptr<ClipModel> clip = m_model->getClipPtr(id);
            if (clip->clipType() == ClipType::Playlist || clip->clipType() == ClipType::Timeline) {
                std::function<bool(void)> undo = []() { return true; };
                std::function<bool(void)> redo = []() { return true; };
                if (m_model->m_groups->isInGroup(id)) {
                    int targetRoot = m_model->m_groups->getRootId(id);
                    if (m_model->isGroup(targetRoot)) {
                        m_model->requestClipUngroup(targetRoot, undo, redo);
                    }
                }
                int pos = clip->getPosition();
                int inPos = m_model->getClipIn(id);
                int duration = m_model->getClipPlaytime(id);
                QDomDocument doc = TimelineFunctions::extractClip(m_model, id, getClipBinId(id));
                m_model->requestClipDeletion(id, undo, redo);
                result = TimelineFunctions::pasteClips(m_model, doc.toString(), m_activeTrack, pos, undo, redo, inPos, duration);
                if (result) {
                    pCore->pushUndo(undo, redo, i18n("Expand clip"));
                } else {
                    undo();
                    pCore->displayMessage(i18n("Could not expand clip"), ErrorMessage, 500);
                }
            }
        }
    }
}

QMap<int, QString> TimelineController::getCurrentTargets(int trackId, int &activeTargetStream)
{
    if (m_model->m_binAudioTargets.size() < 2) {
        activeTargetStream = -1;
        return QMap<int, QString>();
    }
    if (m_model->m_audioTarget.contains(trackId)) {
        activeTargetStream = m_model->m_audioTarget.value(trackId);
    } else {
        activeTargetStream = -1;
    }
    return m_model->m_binAudioTargets;
}

void TimelineController::addTracks(int videoTracks, int audioTracks)
{
    bool result = false;
    int total = videoTracks + audioTracks;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    while (videoTracks + audioTracks > 0) {
        int newTid;
        if (audioTracks > 0) {
            result = m_model->requestTrackInsertion(0, newTid, QString(), true, undo, redo);
            audioTracks--;
        } else {
            result = m_model->requestTrackInsertion(-1, newTid, QString(), false, undo, redo);
            videoTracks--;
        }
        if (!result) {
            break;
        }
    }
    if (result) {
        pCore->pushUndo(undo, redo, i18np("Insert Track", "Insert Tracks", total));
    } else {
        pCore->displayMessage(i18n("Could not insert track"), ErrorMessage, 500);
        undo();
    }
}

void TimelineController::mixClip(int cid, int delta)
{
    if (cid == -1) {
        std::unordered_set<int> selectedIds = m_model->getCurrentSelection();
        if (selectedIds.empty() && m_model->isTrack(m_activeTrack)) {
            // Check if timeline playhead is on a cut
            int timelinePos = pCore->getMonitorPosition();
            int nextClip = m_model->getTrackById_const(m_activeTrack)->getClipByPosition(timelinePos);
            int prevClip = m_model->getTrackById_const(m_activeTrack)->getClipByPosition(timelinePos - 1);
            if (m_model->isClip(prevClip) && m_model->isClip(nextClip) && prevClip != nextClip) {
                cid = nextClip;
            }
        }
    }
    m_model->mixClip(cid, QStringLiteral("luma"), delta);
}

void TimelineController::temporaryUnplug(const QList<int> &clipIds, bool hide)
{
    for (auto &cid : clipIds) {
        int tid = m_model->getItemTrackId(cid);
        if (tid == -1) {
            continue;
        }
        if (hide) {
            m_model->getTrackById_const(tid)->temporaryUnplugClip(cid);
        } else {
            m_model->getTrackById_const(tid)->temporaryReplugClip(cid);
        }
    }
}

void TimelineController::importSubtitle(const QString &path)
{
    QScopedPointer<ImportSubtitle> d(new ImportSubtitle(path, QApplication::activeWindow()));
    if (d->exec() == QDialog::Accepted && !d->subtitle_url->url().isEmpty()) {
        auto subtitleModel = m_model->getSubtitleModel();
        if (d->create_track->isChecked()) {
            // Create a new subtitle entry
            int ix = subtitleModel->createNewSubtitle(d->track_name->text());
            Q_EMIT subtitlesListChanged();
            // Activate the newly created subtitle track
            subtitlesMenuActivated(ix - 1);
        }
        int offset = 0, startFramerate = 30.00, targetFramerate = 30.00;
        if (d->cursor_pos->isChecked()) {
            offset = pCore->getMonitorPosition();
        }
        if (d->transform_framerate_check_box->isChecked()) {
            startFramerate = d->caption_original_framerate->value();
            targetFramerate = d->caption_target_framerate->value();
        }
        subtitleModel->importSubtitle(d->subtitle_url->url().toLocalFile(), offset, true, startFramerate, targetFramerate,
                                      d->codecs_list->currentText().toUtf8());
    }
    Q_EMIT regainFocus();
}

void TimelineController::exportSubtitle()
{
    if (!m_model->hasSubtitleModel()) {
        return;
    }
    QString currentSub = m_model->getSubtitleModel()->getUrl();
    if (currentSub.isEmpty()) {
        pCore->displayMessage(i18n("No subtitles in current project"), ErrorMessage);
        return;
    }
    QString url = QFileDialog::getSaveFileName(qApp->activeWindow(), i18n("Export subtitle file"), pCore->currentDoc()->url().toLocalFile(),
                                               i18n("Subtitle File (*.srt)"));
    if (url.isEmpty()) {
        return;
    }
    if (!url.endsWith(QStringLiteral(".srt"))) {
        url.append(QStringLiteral(".srt"));
    }
    QFile srcFile(url);
    if (srcFile.exists()) {
        srcFile.remove();
    }
    QFile src(currentSub);
    if (!src.copy(srcFile.fileName())) {
        KMessageBox::error(qApp->activeWindow(), i18n("Cannot write to file %1", srcFile.fileName()));
    }
}

void TimelineController::subtitleSpeechRecognition()
{
    SpeechDialog d(m_model, m_zone, m_activeTrack, false, false, qApp->activeWindow());
    d.exec();
}

bool TimelineController::subtitlesWarning() const
{
    return !EffectsRepository::get()->hasInternalEffect("avfilter.subtitles");
}

void TimelineController::subtitlesWarningDetails()
{
    KMessageBox::error(nullptr, i18n("The avfilter.subtitles filter is required, but was not found."
                                     " The subtitles feature will probably not work as expected."));
    Q_EMIT regainFocus();
}

void TimelineController::switchSubtitleDisable()
{
    if (m_model->hasSubtitleModel()) {
        auto subtitleModel = m_model->getSubtitleModel();
        bool disabled = subtitleModel->isDisabled();
        Fun local_switch = [this, subtitleModel]() {
            subtitleModel->switchDisabled();
            Q_EMIT subtitlesDisabledChanged();
            pCore->refreshProjectMonitorOnce();
            return true;
        };
        local_switch();
        pCore->pushUndo(local_switch, local_switch, disabled ? i18n("Show subtitle track") : i18n("Hide subtitle track"));
    }
}

bool TimelineController::subtitlesDisabled() const
{
    if (m_model->hasSubtitleModel()) {
        return m_model->getSubtitleModel()->isDisabled();
    }
    return false;
}

void TimelineController::switchSubtitleLock()
{
    if (m_model->hasSubtitleModel()) {
        auto subtitleModel = m_model->getSubtitleModel();
        bool locked = subtitleModel->isLocked();
        Fun local_switch = [this, subtitleModel]() {
            subtitleModel->switchLocked();
            Q_EMIT subtitlesLockedChanged();
            return true;
        };
        local_switch();
        pCore->pushUndo(local_switch, local_switch, locked ? i18n("Unlock subtitle track") : i18n("Lock subtitle track"));
    }
}
bool TimelineController::subtitlesLocked() const
{
    if (m_model->hasSubtitleModel()) {
        return m_model->getSubtitleModel()->isLocked();
    }
    return false;
}

bool TimelineController::guidesLocked() const
{
    return KdenliveSettings::lockedGuides();
}

void TimelineController::showToolTip(const QString &info) const
{
    pCore->displayMessage(info, TooltipMessage);
}

void TimelineController::showKeyBinding(const QString &info) const
{
    pCore->window()->showKeyBinding(info);
}

void TimelineController::showTimelineToolInfo(bool show) const
{
    if (show) {
        pCore->window()->showToolMessage();
    } else {
        pCore->window()->setWidgetKeyBinding();
    }
}

void TimelineController::showRulerEffectZone(QPair<int, int> inOut, bool checked)
{
    m_effectZone = checked ? QPoint(inOut.first, inOut.second) : QPoint();
    Q_EMIT effectZoneChanged();
}

void TimelineController::updateMasterZones(const QVariantList &zones)
{
    m_masterEffectZones = zones;
    Q_EMIT masterZonesChanged();
}

int TimelineController::clipMaxDuration(int cid)
{
    if (!m_model->isClip(cid)) {
        return -1;
    }
    return m_model->m_allClips[cid]->getMaxDuration();
}

void TimelineController::resizeMix(int cid, int duration, MixAlignment align, int leftFrames)
{
    if (cid > -1) {
        m_model->requestResizeMix(cid, duration, align, leftFrames);
    }
}

int TimelineController::getMixCutPos(int cid) const
{
    return m_model->getMixCutPos(cid);
}

MixAlignment TimelineController::getMixAlign(int cid) const
{
    return m_model->getMixAlign(cid);
}

void TimelineController::processMultitrackOperation(int tid, int in)
{
    int out = pCore->getMonitorPosition();
    if (out == in) {
        // Simply change the reference track, nothing to do here
        return;
    }
    QVector<int> tracks;
    auto it = m_model->m_allTracks.cbegin();
    // Lift all tracks except tid
    while (it != m_model->m_allTracks.cend()) {
        int target_track = (*it)->getId();
        if (target_track != tid && !(*it)->isAudioTrack() && m_model->getTrackById_const(target_track)->shouldReceiveTimelineOp()) {
            tracks << target_track;
        }
        ++it;
    }
    if (tracks.isEmpty()) {
        pCore->displayMessage(i18n("Please activate a track for this operation by clicking on its label"), ErrorMessage);
    }
    TimelineFunctions::extractZone(m_model, tracks, QPoint(in, out), true);
}

void TimelineController::setMulticamIn(int pos)
{
    if (multicamIn != -1) {
        // remove previous snap
        m_model->removeSnap(multicamIn);
    }
    multicamIn = pos;
    m_model->addSnap(multicamIn);
    Q_EMIT multicamInChanged();
}

void TimelineController::checkClipPosition(const QModelIndex &topLeft, const QModelIndex &, const QVector<int> &roles)
{
    if (roles.contains(TimelineModel::StartRole)) {
        int id = int(topLeft.internalId());
        if (m_model->isComposition(id) || m_model->isClip(id)) {
            Q_EMIT updateAssetPosition(id, m_model->uuid());
        }
    }
    if (roles.contains(TimelineModel::ResourceRole)) {
        int id = int(topLeft.internalId());
        if (m_model->isComposition(id) || m_model->isClip(id)) {
            int in = m_model->getItemPosition(id);
            int out = in + m_model->getItemPlaytime(id);
            pCore->refreshProjectRange({in, out});
        }
    }
}

void TimelineController::autofitTrackHeight(int timelineHeight, int collapsedHeight)
{
    int tracksCount = m_model->getTracksCount();
    if (tracksCount < 1) {
        return;
    }
    // Check how many collapsed tracks we have
    int collapsed = 0;
    auto it = m_model->m_allTracks.cbegin();
    while (it != m_model->m_allTracks.cend()) {
        if ((*it)->getProperty(QStringLiteral("kdenlive:collapsed")).toInt() > 0) {
            collapsed++;
        }
        ++it;
    }
    if (collapsed == tracksCount) {
        // All tracks are collapsed, do nothing
        return;
    }
    int trackHeight = qMax(collapsedHeight, (timelineHeight - (collapsed * collapsedHeight)) / (tracksCount - collapsed));
    it = m_model->m_allTracks.cbegin();
    while (it != m_model->m_allTracks.cend()) {
        if ((*it)->getProperty(QStringLiteral("kdenlive:collapsed")).toInt() == 0) {
            (*it)->setProperty(QStringLiteral("kdenlive:trackheight"), QString::number(trackHeight));
        }
        ++it;
    }
    // m_model->setTrackProperty(trackId, "kdenlive:collapsed", QStringLiteral("0"));
    QModelIndex modelStart = m_model->makeTrackIndexFromID(m_model->getTrackIndexFromPosition(0));
    QModelIndex modelEnd = m_model->makeTrackIndexFromID(m_model->getTrackIndexFromPosition(tracksCount - 1));
    Q_EMIT m_model->dataChanged(modelStart, modelEnd, {TimelineModel::HeightRole});
}

QVariantList TimelineController::subtitlesList() const
{
    QVariantList result;
    auto subtitleModel = m_model->getSubtitleModel();
    if (subtitleModel) {
        QMap<std::pair<int, QString>, QString> currentSubs = subtitleModel->getSubtitlesList();
        if (currentSubs.isEmpty()) {
            result << i18nc("@item:inlistbox name for subtitle track", "Subtitles");
        } else {
            QMapIterator<std::pair<int, QString>, QString> i(currentSubs);
            while (i.hasNext()) {
                i.next();
                result << i.key().second;
            }
        }
    } else {
        result << i18nc("@item:inlistbox name for subtitle track", "Subtitles");
    }
    result << i18nc("@item:inlistbox", "Manage Subtitles");
    return result;
}

void TimelineController::subtitlesMenuActivatedAsync(int ix)
{
    // This method needs a timer otherwise the qml combobox crashes because we try to chenge its index while it is processing an activated event
    QTimer::singleShot(100, this, [&, ix]() { subtitlesMenuActivated(ix); });
}

void TimelineController::refreshSubtitlesComboIndex()
{
    int ix = m_activeSubPosition;
    m_activeSubPosition = 0;
    Q_EMIT activeSubtitlePositionChanged();
    m_activeSubPosition = ix;
    Q_EMIT activeSubtitlePositionChanged();
}

void TimelineController::subtitlesMenuActivated(int ix)
{
    auto subtitleModel = m_model->getSubtitleModel();
    QMap<std::pair<int, QString>, QString> currentSubs = subtitleModel->getSubtitlesList();
    if (subtitleModel) {
        if (ix != -1 && ix < currentSubs.size()) {
            // Clear selection if a subtitle item is selected
            std::unordered_set<int> selectedIds = m_model->getCurrentSelection();
            for (auto &id : selectedIds) {
                int tid = m_model->getItemTrackId(id);
                if (m_model->isSubtitleTrack(tid)) {
                    m_model->requestClearSelection();
                    break;
                }
            }
            QMapIterator<std::pair<int, QString>, QString> i(currentSubs);
            m_activeSubPosition = 0;
            int counter = 0;
            while (i.hasNext()) {
                i.next();
                ix--;
                if (ix < 0) {
                    // Match, switch to another subtitle
                    int index = i.key().first;
                    m_activeSubPosition = counter;
                    subtitleModel->activateSubtitle(index);
                    break;
                }
                counter++;
            }
            Q_EMIT activeSubtitlePositionChanged();
            return;
        }
    }
    int currentIx = pCore->currentDoc()->getSequenceProperty(m_model->uuid(), QStringLiteral("kdenlive:activeSubtitleIndex"), QStringLiteral("0")).toInt();
    if (ix > -1) {
        m_activeSubPosition = currentSubs.size();
        Q_EMIT activeSubtitlePositionChanged();
        // Reselect last active subtitle in combobox
        QMapIterator<std::pair<int, QString>, QString> i(currentSubs);
        int counter = 0;
        while (i.hasNext()) {
            i.next();
            if (i.key().first == currentIx) {
                m_activeSubPosition = counter;
                break;
            }
            counter++;
        }
        Q_EMIT activeSubtitlePositionChanged();
    }

    // Show manage dialog
    ManageSubtitles *d = new ManageSubtitles(subtitleModel, this, currentIx, qApp->activeWindow());
    d->exec();
}
