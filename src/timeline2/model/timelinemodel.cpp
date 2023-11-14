/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "timelinemodel.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "bin/model/markersortmodel.h"
#include "bin/model/subtitlemodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "clipmodel.hpp"
#include "compositionmodel.hpp"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "effects/effectsrepository.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "groupsmodel.hpp"
#include "kdenlivesettings.h"
#include "profiles/profilemodel.hpp"
#include "snapmodel.hpp"
#include "timeline2/view/previewmanager.h"
#include "timelinefunctions.hpp"

#include "monitor/monitormanager.h"

#include <KLocalizedString>
#include <QCryptographicHash>
#include <QDebug>
#include <QModelIndex>
#include <QThread>
#include <mlt++/MltConsumer.h>
#include <mlt++/MltField.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTractor.h>
#include <mlt++/MltTransition.h>
#include <queue>
#include <set>

#include "macros.hpp"
#include <localeHandling.h>

#ifdef CRASH_AUTO_TEST
#include "logger.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <rttr/registration>
#pragma GCC diagnostic pop
RTTR_REGISTRATION
{
    using namespace rttr;
    registration::class_<TimelineModel>("TimelineModel")
        .method("setTrackLockedState", &TimelineModel::setTrackLockedState)(parameter_names("trackId", "lock"))
        .method("requestClipMove", select_overload<bool(int, int, int, bool, bool, bool, bool, bool)>(&TimelineModel::requestClipMove))(
            parameter_names("clipId", "trackId", "position", "moveMirrorTracks", "updateView", "logUndo", "invalidateTimeline", "revertMove"))
        .method("requestCompositionMove", select_overload<bool(int, int, int, bool, bool)>(&TimelineModel::requestCompositionMove))(
            parameter_names("compoId", "trackId", "position", "updateView", "logUndo"))
        .method("requestClipInsertion", select_overload<bool(const QString &, int, int, int &, bool, bool, bool)>(&TimelineModel::requestClipInsertion))(
            parameter_names("binClipId", "trackId", "position", "id", "logUndo", "refreshView", "useTargets"))
        .method("requestItemDeletion", select_overload<bool(int, bool)>(&TimelineModel::requestItemDeletion))(parameter_names("clipId", "logUndo"))
        .method("requestGroupMove", select_overload<bool(int, int, int, int, bool, bool, bool, bool)>(&TimelineModel::requestGroupMove))(
            parameter_names("itemId", "groupId", "delta_track", "delta_pos", "moveMirrorTracks", "updateView", "logUndo", "revertMove"))
        .method("requestGroupDeletion", select_overload<bool(int, bool)>(&TimelineModel::requestGroupDeletion))(parameter_names("clipId", "logUndo"))
        .method("requestItemResize", select_overload<int(int, int, bool, bool, int, bool)>(&TimelineModel::requestItemResize))(
            parameter_names("itemId", "size", "right", "logUndo", "snapDistance", "allowSingleResize"))
        .method("requestClipsGroup", select_overload<int(const std::unordered_set<int> &, bool, GroupType)>(&TimelineModel::requestClipsGroup))(
            parameter_names("itemIds", "logUndo", "type"))
        .method("requestClipUngroup", select_overload<bool(int, bool)>(&TimelineModel::requestClipUngroup))(parameter_names("itemId", "logUndo"))
        .method("requestClipsUngroup", &TimelineModel::requestClipsUngroup)(parameter_names("itemIds", "logUndo"))
        .method("requestTrackInsertion", select_overload<bool(int, int &, const QString &, bool)>(&TimelineModel::requestTrackInsertion))(
            parameter_names("pos", "id", "trackName", "audioTrack"))
        .method("requestTrackDeletion", select_overload<bool(int)>(&TimelineModel::requestTrackDeletion))(parameter_names("trackId"))
        .method("requestClearSelection", select_overload<bool(bool)>(&TimelineModel::requestClearSelection))(parameter_names("onDeletion"))
        .method("requestAddToSelection", &TimelineModel::requestAddToSelection)(parameter_names("itemId", "clear"))
        .method("requestRemoveFromSelection", &TimelineModel::requestRemoveFromSelection)(parameter_names("itemId"))
        .method("requestSetSelection", select_overload<bool(const std::unordered_set<int> &)>(&TimelineModel::requestSetSelection))(parameter_names("itemIds"))
        .method("requestFakeClipMove", select_overload<bool(int, int, int, bool, bool, bool)>(&TimelineModel::requestFakeClipMove))(
            parameter_names("clipId", "trackId", "position", "updateView", "logUndo", "invalidateTimeline"))
        .method("requestFakeGroupMove", select_overload<bool(int, int, int, int, bool, bool)>(&TimelineModel::requestFakeGroupMove))(
            parameter_names("clipId", "groupId", "delta_track", "delta_pos", "updateView", "logUndo"))
        .method("suggestClipMove",
                &TimelineModel::suggestClipMove)(parameter_names("clipId", "trackId", "position", "cursorPosition", "snapDistance", "moveMirrorTracks"))
        .method("suggestCompositionMove",
                &TimelineModel::suggestCompositionMove)(parameter_names("compoId", "trackId", "position", "cursorPosition", "snapDistance"))
        // .method("addSnap", &TimelineModel::addSnap)(parameter_names("pos"))
        // .method("removeSnap", &TimelineModel::addSnap)(parameter_names("pos"))
        // .method("requestCompositionInsertion", select_overload<bool(const QString &, int, int, int, std::unique_ptr<Mlt::Properties>, int &, bool)>(
        //                                            &TimelineModel::requestCompositionInsertion))(
        //     parameter_names("transitionId", "trackId", "position", "length", "transProps", "id", "logUndo"))
        .method("requestClipTimeWarp", select_overload<bool(int, double, bool, bool)>(&TimelineModel::requestClipTimeWarp))(
            parameter_names("clipId", "speed", "pitchCompensate", "changeDuration"));
}
#else
#define TRACE_CONSTR(...)
#define TRACE_STATIC(...)
#define TRACE_RES(...)
#define TRACE(...)
#endif

int TimelineModel::seekDuration = 30000;

TimelineModel::TimelineModel(const QUuid &uuid, std::weak_ptr<DocUndoStack> undo_stack)
    : QAbstractItemModel_shared_from_this()
    , m_blockRefresh(false)
    , m_uuid(uuid)
    , m_tractor(new Mlt::Tractor(pCore->getProjectProfile()))
    , m_masterStack(nullptr)
    , m_masterService(nullptr)
    , m_timelinePreview(nullptr)
    , m_snaps(new SnapModel())
    , m_undoStack(std::move(undo_stack))
    , m_blackClip(new Mlt::Producer(pCore->getProjectProfile(), "color:black"))
    , m_lock(QReadWriteLock::Recursive)
    , m_timelineEffectsEnabled(true)
    , m_id(getNextId())
    , m_overlayTrackCount(-1)
    , m_videoTarget(-1)
    , m_editMode(TimelineMode::NormalEdit)
    , m_closing(false)
    , m_softDelete(false)
{
    // Create black background track
    m_blackClip->set("kdenlive:playlistid", "black_track");
    m_blackClip->set("mlt_type", "producer");
    m_blackClip->set("aspect_ratio", 1);
    m_blackClip->set("length", INT_MAX);
    m_blackClip->set("mlt_image_format", "rgba");
    m_blackClip->set("set.test_audio", 0);
    m_blackClip->set_in_and_out(0, TimelineModel::seekDuration);
    m_tractor->insert_track(*m_blackClip, 0);
    if (uuid != pCore->currentDoc()->uuid()) {
        // This is not the main tractor
        m_tractor->set("id", uuid.toString().toUtf8().constData());
    }

    TRACE_CONSTR(this);
}

void TimelineModel::prepareClose(bool softDelete)
{
    requestClearSelection(true);
    QWriteLocker locker(&m_lock);
    // Unlock all tracks to allow deleting clip from tracks
    m_closing = true;
    m_blockRefresh = true;
    if (softDelete) {
        m_softDelete = true;
    }
    if (!m_softDelete) {
        auto it = m_allTracks.begin();
        while (it != m_allTracks.end()) {
            (*it)->unlock();
            ++it;
        }
        m_subtitleModel.reset();
    } else {
        auto it = m_allTracks.begin();
        while (it != m_allTracks.end()) {
            (*it)->m_softDelete = true;
            ++it;
        }
    }
    // m_subtitleModel->removeAllSubtitles();
}

TimelineModel::~TimelineModel()
{
    m_closing = true;
    if (!m_softDelete) {
        qDebug() << "::::::==\n\nCLOSING TIMELINE MODEL\n\n::::::::";
        QScopedPointer<Mlt::Service> service(m_tractor->field());
        QScopedPointer<Mlt::Field> field(m_tractor->field());
        field->lock();
        // Make sure all previous track compositing is removed
        while (service != nullptr && service->is_valid()) {
            if (service->type() == mlt_service_transition_type) {
                Mlt::Transition t(mlt_transition(service->get_service()));
                service.reset(service->producer());
                // remove all compositing
                field->disconnect_service(t);
                t.disconnect_all_producers();
            } else {
                service.reset(service->producer());
            }
        }
        field->unlock();
        m_allTracks.clear();
        if (pCore->currentDoc() && !pCore->currentDoc()->closing) {
            // If we are not closing the project, unregister this timeline clips from bin
            for (const auto &clip : m_allClips) {
                clip.second->deregisterClipToBin();
            }
        }
    }
}

void TimelineModel::setMarkerModel(std::shared_ptr<MarkerListModel> markerModel)
{
    if (m_guidesModel) {
        return;
    }
    m_guidesModel = markerModel;
    m_guidesModel->registerSnapModel(std::static_pointer_cast<SnapInterface>(m_snaps));
    m_guidesFilterModel.reset(new MarkerSortModel(this));
    m_guidesFilterModel->setSourceModel(m_guidesModel.get());
    m_guidesFilterModel->setSortRole(MarkerListModel::PosRole);
    m_guidesFilterModel->sort(0, Qt::AscendingOrder);
    m_guidesModel->loadCategories(KdenliveSettings::guidesCategories());
}

int TimelineModel::getTracksCount() const
{
    READ_LOCK();
    int count = m_tractor->count();
    if (m_overlayTrackCount > -1) {
        count -= m_overlayTrackCount;
    }
    Q_ASSERT(count >= 0);
    // don't count the black background track
    Q_ASSERT(count - 1 == static_cast<int>(m_allTracks.size()));
    return count - 1;
}

QPair<int, int> TimelineModel::getAVtracksCount() const
{
    QPair<int, int> tracks{0, 0};
    auto it = m_allTracks.cbegin();
    while (it != m_allTracks.cend()) {
        if ((*it)->isAudioTrack()) {
            tracks.first++;
        } else {
            tracks.second++;
        }
        ++it;
    }
    if (m_overlayTrackCount > -1) {
        tracks.first -= m_overlayTrackCount;
    }
    return tracks;
}

QList<int> TimelineModel::getTracksIds(bool audio) const
{
    QList<int> trackIds;
    auto it = m_allTracks.cbegin();
    while (it != m_allTracks.cend()) {
        if ((*it)->isAudioTrack() == audio) {
            trackIds.insert(0, (*it)->getId());
        }
        ++it;
    }
    return trackIds;
}

int TimelineModel::getTrackIndexFromPosition(int pos) const
{
    Q_ASSERT(pos >= 0 && pos < int(m_allTracks.size()));
    READ_LOCK();
    auto it = m_allTracks.cbegin();
    while (pos > 0) {
        it++;
        pos--;
    }
    return (*it)->getId();
}

int TimelineModel::getClipsCount() const
{
    READ_LOCK();
    int size = int(m_allClips.size());
    return size;
}

int TimelineModel::getCompositionsCount() const
{
    READ_LOCK();
    int size = int(m_allCompositions.size());
    return size;
}

int TimelineModel::getClipTrackId(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    return clip->getCurrentTrackId();
}

int TimelineModel::getCompositionTrackId(int compoId) const
{
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    const auto trans = m_allCompositions.at(compoId);
    return trans->getCurrentTrackId();
}

int TimelineModel::getItemTrackId(int itemId) const
{
    READ_LOCK();
    Q_ASSERT(isItem(itemId));
    if (isClip(itemId)) {
        return getClipTrackId(itemId);
    }
    if (isComposition(itemId)) {
        return getCompositionTrackId(itemId);
    }
    if (isSubTitle(itemId)) {
        // TODO: fix when introducing multiple subtitle tracks
        return -2;
    }
    return -1;
}

bool TimelineModel::hasClipEndMix(int clipId) const
{
    if (!isClip(clipId)) return false;
    int tid = getClipTrackId(clipId);
    if (tid < 0) return false;

    return getTrackById_const(tid)->hasEndMix(clipId);
}

int TimelineModel::getClipPosition(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    int pos = clip->getPosition();
    return pos;
}

int TimelineModel::getClipEnd(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    int pos = clip->getPosition() + clip->getPlaytime();
    return pos;
}

double TimelineModel::getClipSpeed(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_allClips.at(clipId)->getSpeed();
}

int TimelineModel::getClipSplitPartner(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_groups->getSplitPartner(clipId);
}

int TimelineModel::getClipIn(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    return clip->getIn();
}

QPoint TimelineModel::getClipInDuration(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    return {clip->getIn(), clip->getPlaytime()};
}

PlaylistState::ClipState TimelineModel::getClipState(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    return clip->clipState();
}

const QString TimelineModel::getClipBinId(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    const auto clip = m_allClips.at(clipId);
    QString id = clip->binId();
    return id;
}

int TimelineModel::getClipPlaytime(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(isClip(clipId));
    const auto clip = m_allClips.at(clipId);
    int playtime = clip->getPlaytime();
    return playtime;
}

QSize TimelineModel::getClipFrameSize(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(isClip(clipId));
    const auto clip = m_allClips.at(clipId);
    return clip->getFrameSize();
}

int TimelineModel::getTrackClipsCount(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    int count = getTrackById_const(trackId)->getClipsCount();
    return count;
}

int TimelineModel::getClipByStartPosition(int trackId, int position) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    return getTrackById_const(trackId)->getClipByStartPosition(position);
}

int TimelineModel::getClipByPosition(int trackId, int position, int playlist) const
{
    READ_LOCK();
    if (isSubtitleTrack(trackId)) {
        return getSubtitleByPosition(position);
    }
    Q_ASSERT(isTrack(trackId));
    return getTrackById_const(trackId)->getClipByPosition(position, playlist);
}

int TimelineModel::getCompositionByPosition(int trackId, int position) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    return getTrackById_const(trackId)->getCompositionByPosition(position);
}

int TimelineModel::getSubtitleByStartPosition(int position) const
{
    READ_LOCK();
    GenTime startTime(position, pCore->getCurrentFps());
    auto findResult =
        std::find_if(std::begin(m_allSubtitles), std::end(m_allSubtitles), [&](const std::pair<int, GenTime> &pair) { return pair.second == startTime; });
    if (findResult != std::end(m_allSubtitles)) {
        return findResult->first;
    }
    return -1;
}

int TimelineModel::getSubtitleByPosition(int position) const
{
    READ_LOCK();
    GenTime startTime(position, pCore->getCurrentFps());
    if (m_subtitleModel) {
        std::unordered_set<int> sids = m_subtitleModel->getItemsInRange(position, position);
        if (!sids.empty()) {
            return *sids.begin();
        }
    }
    return -1;
}

int TimelineModel::getTrackPosition(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_allTracks.cbegin();
    int pos = int(std::distance(it, static_cast<decltype(it)>(m_iteratorTable.at(trackId))));
    return pos;
}

int TimelineModel::getTrackMltIndex(int trackId) const
{
    READ_LOCK();
    // Because of the black track that we insert in first position, the mlt index is the position + 1
    return getTrackPosition(trackId) + 1;
}

int TimelineModel::getTrackSortValue(int trackId, int separated) const
{
    if (separated == 1) {
        // This will be A2, A1, V1, V2
        return getTrackPosition(trackId) + 1;
    }
    if (separated == 2) {
        // This will be A1, A2, V1, V2
        // Count audio/video tracks
        auto it = m_allTracks.cbegin();
        int aCount = 0;
        int vCount = 0;
        int refPos = 0;
        bool isVideo = true;
        while (it != m_allTracks.cend()) {
            if ((*it)->isAudioTrack()) {
                if ((*it)->getId() == trackId) {
                    refPos = aCount;
                    isVideo = false;
                }
                aCount++;
            } else {
                // video track
                if ((*it)->getId() == trackId) {
                    refPos = vCount;
                }
                vCount++;
            }
            ++it;
        }
        return isVideo ? aCount + refPos + 1 : aCount - refPos;
    }
    // This will be A1, V1, A2, V2
    auto it = m_allTracks.cend();
    int aCount = 0;
    int vCount = 0;
    bool isAudio = false;
    int trackPos = 0;
    while (it != m_allTracks.begin()) {
        --it;
        bool audioTrack = (*it)->isAudioTrack();
        if (audioTrack) {
            aCount++;
        } else {
            vCount++;
        }
        if (trackId == (*it)->getId()) {
            isAudio = audioTrack;
            trackPos = audioTrack ? aCount : vCount;
        }
    }
    if (isAudio) {
        if (aCount > vCount) {
            if (trackPos - 1 > aCount - vCount) {
                // We have more audio tracks than video tracks
                return (aCount - vCount + 1) + 2 * (trackPos - (aCount - vCount + 1));
            }
            return trackPos;
        }
        return 2 * trackPos;
    }
    return 2 * (vCount + 1 - trackPos) + 1;
}

QList<int> TimelineModel::getLowerTracksId(int trackId, TrackType type) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    QList<int> results;
    auto it = m_iteratorTable.at(trackId);
    while (it != m_allTracks.cbegin()) {
        --it;
        if (type == TrackType::AnyTrack) {
            results << (*it)->getId();
            continue;
        }
        bool audioTrack = (*it)->isAudioTrack();
        if (type == TrackType::AudioTrack && audioTrack) {
            results << (*it)->getId();
        } else if (type == TrackType::VideoTrack && !audioTrack) {
            results << (*it)->getId();
        }
    }
    return results;
}

int TimelineModel::getPreviousVideoTrackIndex(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    while (it != m_allTracks.cbegin()) {
        --it;
        if (!(*it)->isAudioTrack()) {
            return (*it)->getId();
        }
    }
    return 0;
}

int TimelineModel::getPreviousVideoTrackPos(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    while (it != m_allTracks.cbegin()) {
        --it;
        if (!(*it)->isAudioTrack()) {
            return getTrackMltIndex((*it)->getId());
        }
    }
    return 0;
}

int TimelineModel::getMirrorVideoTrackId(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    if (!(*it)->isAudioTrack()) {
        // we expected an audio track...
        return -1;
    }
    int count = 0;
    while (it != m_allTracks.cend()) {
        if ((*it)->isAudioTrack()) {
            count++;
        } else {
            count--;
            if (count == 0) {
                return (*it)->getId();
            }
        }
        ++it;
    }
    return -1;
}

int TimelineModel::getMirrorTrackId(int trackId) const
{
    if (isAudioTrack(trackId)) {
        return getMirrorVideoTrackId(trackId);
    }
    return getMirrorAudioTrackId(trackId);
}

int TimelineModel::getMirrorAudioTrackId(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    if ((*it)->isAudioTrack()) {
        // we expected a video track...
        qWarning() << "requesting mirror audio track for audio track";
        return -1;
    }
    int count = 0;
    while (it != m_allTracks.cbegin()) {
        if (!(*it)->isAudioTrack()) {
            count++;
        } else {
            count--;
            if (count == 0) {
                return (*it)->getId();
            }
        }
        --it;
    }
    if ((*it)->isAudioTrack() && count == 1) {
        return (*it)->getId();
    }
    return -1;
}

void TimelineModel::setEditMode(TimelineMode::EditMode mode)
{
    m_editMode = mode;
}

TimelineMode::EditMode TimelineModel::editMode() const
{
    return m_editMode;
}

bool TimelineModel::normalEdit() const
{
    return m_editMode == TimelineMode::NormalEdit;
}

bool TimelineModel::requestFakeClipMove(int clipId, int trackId, int position, bool updateView, bool invalidateTimeline, Fun &undo, Fun &redo)
{
    Q_UNUSED(updateView);
    Q_UNUSED(invalidateTimeline);
    Q_UNUSED(undo);
    Q_UNUSED(redo);
    Q_ASSERT(isClip(clipId));
    m_allClips[clipId]->setFakePosition(position);
    bool trackChanged = false;
    if (trackId > -1) {
        if (trackId != m_allClips[clipId]->getFakeTrackId()) {
            if (getTrackById_const(trackId)->trackType() == m_allClips[clipId]->clipState()) {
                m_allClips[clipId]->setFakeTrackId(trackId);
                trackChanged = true;
            }
        }
    }
    QModelIndex modelIndex = makeClipIndexFromID(clipId);
    if (modelIndex.isValid()) {
        QVector<int> roles{FakePositionRole};
        if (trackChanged) {
            roles << FakeTrackIdRole;
        }
        notifyChange(modelIndex, modelIndex, roles);
        return true;
    }
    return false;
}

bool TimelineModel::requestClipMove(int clipId, int trackId, int position, bool moveMirrorTracks, bool updateView, bool invalidateTimeline, bool finalMove,
                                    Fun &undo, Fun &redo, bool revertMove, bool groupMove, const QMap<int, int> &moving_clips,
                                    std::pair<MixInfo, MixInfo> mixData)
{
    Q_UNUSED(moveMirrorTracks)
    if (trackId == -1) {
        qWarning() << "clip is not on a track";
        return false;
    }
    Q_ASSERT(isClip(clipId));
    if (m_allClips[clipId]->clipState() == PlaylistState::Disabled) {
        if (getTrackById_const(trackId)->trackType() == PlaylistState::AudioOnly && !m_allClips[clipId]->canBeAudio()) {
            qWarning() << "clip type mismatch 1";
            return false;
        }
        if (getTrackById_const(trackId)->trackType() == PlaylistState::VideoOnly && !m_allClips[clipId]->canBeVideo()) {
            qWarning() << "clip type mismatch 2";
            return false;
        }
    } else if (getTrackById_const(trackId)->trackType() != m_allClips[clipId]->clipState()) {
        // Move not allowed (audio / video mismatch)
        qWarning() << "clip type mismatch 3";
        return false;
    }
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    bool ok = true;
    int old_trackId = getClipTrackId(clipId);
    int previous_track = moving_clips.value(clipId, -1);
    if (old_trackId == -1) {
        // old_trackId = previous_track;
    }
    bool notifyViewOnly = false;
    Fun update_model = []() { return true; };
    if (old_trackId == trackId) {
        // Move on same track, simply inform the view
        updateView = false;
        notifyViewOnly = true;
        update_model = [clipId, this, trackId, invalidateTimeline]() {
            QModelIndex modelIndex = makeClipIndexFromID(clipId);
            notifyChange(modelIndex, modelIndex, StartRole);
            if (invalidateTimeline && !getTrackById_const(trackId)->isAudioTrack()) {
                int in = getClipPosition(clipId);
                Q_EMIT invalidateZone(in, in + getClipPlaytime(clipId));
            }
            return true;
        };
    }
    Fun sync_mix = []() { return true; };
    Fun simple_move_mix = []() { return true; };
    Fun simple_restore_mix = []() { return true; };
    QList<int> allowedClipMixes;
    if (!groupMove && old_trackId > -1) {
        mixData = getTrackById_const(old_trackId)->getMixInfo(clipId);
    }
    if (old_trackId == trackId && !finalMove && !revertMove) {
        if (mixData.first.firstClipId > -1 && !moving_clips.contains(mixData.first.firstClipId)) {
            // Mix at clip start, don't allow moving left
            if (position < (mixData.first.firstClipInOut.second - mixData.first.mixOffset) &&
                (position + m_allClips[clipId]->getPlaytime() >= mixData.first.firstClipInOut.first)) {
                qDebug() << "==== ABORTING GROUP MOVE ON START MIX";
                return false;
            }
        }
        if (mixData.second.firstClipId > -1 && !moving_clips.contains(mixData.second.secondClipId)) {
            // Mix at clip end, don't allow moving right
            if (position + getClipPlaytime(clipId) > mixData.second.secondClipInOut.first && position < mixData.second.secondClipInOut.second) {
                qDebug() << "==== ABORTING GROUP MOVE ON END MIX: " << position;
                return false;
            }
        }
    }
    bool hadMix = mixData.first.firstClipId > -1 || mixData.second.firstClipId > -1;
    if (!finalMove && !revertMove) {
        QVector<int> exceptions = {clipId};
        if (mixData.first.firstClipId > -1) {
            exceptions << mixData.first.firstClipId;
        }
        if (mixData.second.secondClipId > -1) {
            exceptions << mixData.second.secondClipId;
        }
        if (!getTrackById_const(trackId)->isAvailableWithExceptions(position, getClipPlaytime(clipId), exceptions)) {
            // No space for clip insert operation, abort
            qWarning() << "No free space for clip move";
            return false;
        }
    }
    if (old_trackId == -1 && isTrack(previous_track) && hadMix && previous_track != trackId) {
        // Clip is moved to another track
        bool mixGroupMove = false;
        if (mixData.first.firstClipId > 0) {
            allowedClipMixes << mixData.first.firstClipId;
            if (moving_clips.contains(mixData.first.firstClipId)) {
                allowedClipMixes << mixData.first.firstClipId;
            } else if (finalMove) {
                position += (mixData.first.firstClipInOut.second - mixData.first.secondClipInOut.first - mixData.first.mixOffset);
                removeMixWithUndo(clipId, local_undo, local_redo);
            }
        }
        if (mixData.second.firstClipId > 0) {
            allowedClipMixes << mixData.second.secondClipId;
            if (moving_clips.contains(mixData.second.secondClipId)) {
                allowedClipMixes << mixData.second.secondClipId;
            } else if (finalMove) {
                removeMixWithUndo(mixData.second.secondClipId, local_undo, local_redo);
            }
        }
        if (m_groups->isInGroup(clipId) && mixData.first.firstClipId > 0) {
            int parentGroup = m_groups->getRootId(clipId);
            if (parentGroup > -1) {
                std::unordered_set<int> sub = m_groups->getLeaves(parentGroup);
                if (sub.count(mixData.first.firstClipId) > 0 && sub.count(mixData.first.secondClipId) > 0) {
                    mixGroupMove = true;
                }
            }
        }
        if (mixGroupMove) {
            // We are moving a group on another track, delete and re-add
            // Get mix properties
            std::pair<int, int> tracks = getTrackById_const(previous_track)->getMixTracks(mixData.first.secondClipId);
            std::pair<QString, QVector<QPair<QString, QVariant>>> mixParams = getTrackById_const(previous_track)->getMixParams(mixData.first.secondClipId);
            simple_move_mix = [this, previous_track, trackId, finalMove, mixData, tracks, mixParams]() {
                // Insert mix on new track
                bool result = getTrackById_const(trackId)->createMix(mixData.first, mixParams, tracks, finalMove);
                // Remove mix on old track
                getTrackById_const(previous_track)->removeMix(mixData.first);
                return result;
            };
            simple_restore_mix = [this, previous_track, trackId, finalMove, mixData, tracks, mixParams]() {
                bool result = getTrackById_const(previous_track)->createMix(mixData.first, mixParams, tracks, finalMove);
                // Remove mix on old track
                getTrackById_const(trackId)->removeMix(mixData.first);

                return result;
            };
        }
    } else if (finalMove && !groupMove && isTrack(old_trackId) && hadMix) {
        // Clip has a mix
        if (mixData.first.firstClipId > -1) {
            if (old_trackId == trackId) {
                int mixCut = m_allClips[clipId]->getMixCutPosition();
                // We are moving a clip on same track
                if (position > mixData.first.secondClipInOut.first - mixCut || position < mixData.first.firstClipInOut.first) {
                    position += m_allClips[clipId]->getMixDuration() - mixCut;
                    removeMixWithUndo(clipId, local_undo, local_redo);
                }
            } else {
                // Clip moved to another track, delete mix
                position += (m_allClips[clipId]->getMixDuration() - m_allClips[clipId]->getMixCutPosition());
                removeMixWithUndo(clipId, local_undo, local_redo);
            }
        }
        if (mixData.second.firstClipId > -1) {
            // We have a mix at clip end
            if (old_trackId == trackId) {
                int mixEnd = m_allClips[mixData.second.secondClipId]->getPosition() + m_allClips[mixData.second.secondClipId]->getMixDuration();
                if (position > mixEnd || position < m_allClips[mixData.second.secondClipId]->getPosition()) {
                    // Moved outside mix zone
                    removeMixWithUndo(mixData.second.secondClipId, local_undo, local_redo);
                }
            } else {
                // Clip moved to another track, delete mix
                // Mix will be deleted by syncronizeMixes operation, only
                // re-add it on undo
                removeMixWithUndo(mixData.second.secondClipId, local_undo, local_redo);
            }
        }
    } else if (finalMove && groupMove && isTrack(old_trackId) && hadMix && old_trackId == trackId) {
        // Group move on same track with mix
        if (mixData.first.firstClipId > -1) {
            // Mix on clip start, check if mix is still in range
            if (!moving_clips.contains(mixData.first.firstClipId)) {
                int mixCut = m_allClips[clipId]->getMixCutPosition();
                // We are moving a clip on same track
                if (position > mixData.first.secondClipInOut.first - mixCut || position < mixData.first.firstClipInOut.first) {
                    // Mix will be deleted, recreate on undo
                    position += m_allClips[mixData.first.secondClipId]->getMixDuration() - m_allClips[mixData.first.secondClipId]->getMixCutPosition();
                    removeMixWithUndo(mixData.first.secondClipId, local_undo, local_redo);
                }
            } else {
                allowedClipMixes << mixData.first.firstClipId;
            }
        }
        if (mixData.second.firstClipId > -1) {
            // Mix on clip end, check if mix is still in range
            if (!moving_clips.contains(mixData.second.secondClipId)) {
                int mixEnd = m_allClips[mixData.second.secondClipId]->getPosition() + m_allClips[mixData.second.secondClipId]->getMixDuration();
                if (mixEnd > position + m_allClips[clipId]->getPlaytime() || position > mixEnd) {
                    // Mix will be deleted, recreate on undo
                    removeMixWithUndo(mixData.second.secondClipId, local_undo, local_redo);
                }
            } else {
                allowedClipMixes << mixData.second.secondClipId;
            }
        }
    } else {
    }
    if (old_trackId != -1) {
        if (notifyViewOnly) {
            PUSH_LAMBDA(update_model, local_undo);
        }
        ok = getTrackById(old_trackId)->requestClipDeletion(clipId, updateView, finalMove, local_undo, local_redo, groupMove, false, allowedClipMixes);
        if (!ok) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            qWarning() << "clip deletion failed";
            return false;
        } else {
        }
    }
    ok = ok && getTrackById(trackId)->requestClipInsertion(clipId, position, updateView, finalMove, local_undo, local_redo, groupMove, old_trackId == -1,
                                                           allowedClipMixes);

    if (!ok) {
        qWarning() << "clip insertion failed";
        bool undone = local_undo();
        Q_ASSERT(undone);
        return false;
    }

    sync_mix();
    update_model();
    simple_move_mix();

    if (finalMove) {
        PUSH_LAMBDA(simple_restore_mix, undo);
        PUSH_LAMBDA(simple_move_mix, local_redo);
        // PUSH_LAMBDA(sync_mix, local_redo);
    }
    if (notifyViewOnly) {
        PUSH_LAMBDA(update_model, local_redo);
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::mixClip(int idToMove, const QString &mixId, int delta)
{
    std::unordered_set<int> initialSelection = getCurrentSelection();
    if (idToMove == -1 && initialSelection.empty()) {
        pCore->displayMessage(i18n("Select a clip to apply the mix"), ErrorMessage, 500);
        return false;
    }
    struct mixStructure
    {
        int clipId;
        std::pair<int, int> clips;
        std::pair<int, int> spaces;
        std::pair<int, int> durations;
        int mixPosition;
        int selectedTrack;
    };
    QList<mixStructure> clipsToMixList;
    std::vector<int> clipIds;
    std::unordered_set<int> selectionToSort;
    if (idToMove != -1) {
        selectionToSort = {idToMove};
        // Check for audio / video partner
        if (isClip(idToMove) && m_groups->isInGroup(idToMove)) {
            int partnerId = getClipSplitPartner(idToMove);
            if (partnerId > -1) {
                selectionToSort.insert(partnerId);
            }
        }
    } else {
        selectionToSort = initialSelection;
    }
    // Sort selection chronologically to limit reordering
    std::vector<int> ordered(selectionToSort.begin(), selectionToSort.end());
    std::sort(ordered.begin(), ordered.end(), [this](int cidA, int cidB) {
        if (!isClip(cidA) || !isClip(cidB)) {
            return false;
        }
        return m_allClips[cidA]->getPosition() < m_allClips[cidB]->getPosition();
    });
    clipIds = ordered;

    int noSpaceInClip = 0;
    int mixDuration = pCore->getDurationFromString(KdenliveSettings::mix_duration());
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };

    auto processMix = [this, &undo, &redo, mixDuration, mixId](mixStructure mInfo) {
        if (mInfo.spaces.first > -1) {
            if (mInfo.spaces.second > -1) {
                // Both clips have limited durations
                mInfo.durations.first = qMin(mixDuration / 2, mInfo.spaces.first);
                mInfo.durations.first = qMin(mInfo.durations.first, mInfo.spaces.first);
                mInfo.durations.second = qMin(mixDuration - mixDuration / 2, mInfo.spaces.second);
                int offset = mixDuration - (mInfo.durations.first + mInfo.durations.second);
                if (offset > 0) {
                    if (mInfo.spaces.first > mInfo.durations.first) {
                        mInfo.durations.first = qMin(mInfo.spaces.first, mInfo.durations.first + offset);
                    } else if (mInfo.spaces.second > mInfo.durations.second) {
                        mInfo.durations.second = qMin(mInfo.spaces.second, mInfo.durations.second + offset);
                    }
                }
            } else {
                mInfo.durations.first = qMin(mixDuration - mixDuration / 2, mInfo.spaces.first);
                mInfo.durations.second = mixDuration - mInfo.durations.second;
            }
        } else {
            if (mInfo.spaces.second > -1) {
                mInfo.durations.second = qMin(mixDuration - mixDuration / 2, mInfo.spaces.second);
                mInfo.durations.first = mixDuration - mInfo.durations.second;
            } else {
                mInfo.durations.first = mixDuration / 2;
                mInfo.durations.second = mixDuration - mInfo.durations.first;
            }
        }
        return requestClipMix(mixId, mInfo.clips, mInfo.durations, mInfo.selectedTrack, mInfo.mixPosition, true, true, true, undo, redo, false);
    };

    for (int s : clipIds) {
        if (!isClip(s)) {
            continue;
        }
        mixStructure mixInfo;
        mixInfo.clipId = s;
        mixInfo.clips = {-1, -1};
        mixInfo.spaces = {0, 0};
        mixInfo.durations = {0, 0};
        mixInfo.mixPosition = 0;
        mixInfo.selectedTrack = getClipTrackId(s);
        if (mixInfo.selectedTrack == -1 || !isTrack(mixInfo.selectedTrack)) {
            continue;
        }
        mixInfo.mixPosition = getItemPosition(s);
        int clipDuration = getItemPlaytime(s);
        // Check if clip already has a mix
        if (delta > -1 && getTrackById_const(mixInfo.selectedTrack)->hasStartMix(s)) {
            if (getTrackById_const(mixInfo.selectedTrack)->hasEndMix(s)) {
                continue;
            }
            mixInfo.clips.second = getTrackById_const(mixInfo.selectedTrack)->getClipByPosition(mixInfo.mixPosition + clipDuration + 1);
            // Check if previous clip was selected, and not next clip. In that case we stop processing
            int previousClip = getTrackById_const(mixInfo.selectedTrack)->getClipByPosition(mixInfo.mixPosition - 1);
            if (std::find(clipIds.begin(), clipIds.end(), previousClip) != clipIds.end() &&
                std::find(clipIds.begin(), clipIds.end(), mixInfo.clips.second) == clipIds.end()) {
                continue;
            }
        } else if (delta < 1 && getTrackById_const(mixInfo.selectedTrack)->hasEndMix(s)) {
            mixInfo.clips.first = getTrackById_const(mixInfo.selectedTrack)->getClipByPosition(mixInfo.mixPosition - 1);
            // Check if next clip was selected, and not previous clip. In that case we stop processing
            int nextClip = getTrackById_const(mixInfo.selectedTrack)->getClipByPosition(mixInfo.mixPosition + clipDuration + 1);
            if (std::find(clipIds.begin(), clipIds.end(), nextClip) != clipIds.end() &&
                std::find(clipIds.begin(), clipIds.end(), mixInfo.clips.first) == clipIds.end()) {
                continue;
            }
            if (mixInfo.clips.first > -1 && getTrackById_const(mixInfo.selectedTrack)->hasEndMix(mixInfo.clips.first)) {
                // Could happen if 2 clips before are mixed to full length
                mixInfo.clips.first = -1;
            }
        } else {
            if (delta < 1) {
                mixInfo.clips.first = getTrackById_const(mixInfo.selectedTrack)->getClipByPosition(mixInfo.mixPosition - 1);
            }
            if (delta > -1) {
                mixInfo.clips.second = getTrackById_const(mixInfo.selectedTrack)->getClipByPosition(mixInfo.mixPosition + clipDuration + 1);
            }
        }
        if (mixInfo.clips.first > -1 && mixInfo.clips.second > -1) {
            // We have a clip before and a clip after, check if both are selected
            if (std::find(clipIds.begin(), clipIds.end(), mixInfo.clips.second) == clipIds.end()) {
                if (std::find(clipIds.begin(), clipIds.end(), mixInfo.clips.first) != clipIds.end()) {
                    mixInfo.clips.second = -1;
                }
            } else if (std::find(clipIds.begin(), clipIds.end(), mixInfo.clips.first) == clipIds.end()) {
                mixInfo.clips.first = -1;
            }
        }
        if (mixInfo.clips.second != -1) {
            // Mix at end of selected clip
            // Make sure we have enough space in clip to resize
            // mixInfo.spaces.first is the maximum frames we have to expand first clip on the right
            mixInfo.spaces.first = m_allClips[s]->m_endlessResize
                                       ? m_allClips[mixInfo.clips.second]->getPlaytime()
                                       : qMin(m_allClips[mixInfo.clips.second]->getPlaytime(), m_allClips[s]->getMaxDuration() - m_allClips[s]->getOut() - 1);
            // mixInfo.spaces.second is the maximum frames we have to expand second clip on the left
            mixInfo.spaces.second = m_allClips[mixInfo.clips.second]->m_endlessResize
                                        ? m_allClips[s]->getPlaytime()
                                        : qMin(m_allClips[s]->getPlaytime(), m_allClips[mixInfo.clips.second]->getIn());
            if (getTrackById_const(mixInfo.selectedTrack)->hasStartMix(s)) {
                int spaceBeforeMix = m_allClips[mixInfo.clips.second]->getPosition() - (m_allClips[s]->getPosition() + m_allClips[s]->getMixDuration());
                mixInfo.spaces.second = mixInfo.spaces.second == -1 ? spaceBeforeMix : qMin(mixInfo.spaces.second, spaceBeforeMix);
            }
            if (getTrackById_const(mixInfo.selectedTrack)->hasEndMix(mixInfo.clips.second)) {
                MixInfo mixData = getTrackById_const(mixInfo.selectedTrack)->getMixInfo(mixInfo.clips.second).second;
                if (mixData.secondClipId > -1) {
                    int spaceAfterMix = m_allClips[mixInfo.clips.second]->getPlaytime() - m_allClips[mixData.secondClipId]->getMixDuration();
                    mixInfo.spaces.first = mixInfo.spaces.first == -1 ? spaceAfterMix : qMin(mixInfo.spaces.first, spaceAfterMix);
                }
            }
            if (mixInfo.spaces.first > -1 && mixInfo.spaces.second > -1 && (mixInfo.spaces.first + mixInfo.spaces.second < 3)) {
                noSpaceInClip = 2;
                continue;
            }
            mixInfo.mixPosition += clipDuration;
            mixInfo.clips.first = s;
            mixInfo.clipId = s;
            processMix(mixInfo);
            clipsToMixList << mixInfo;
            continue;
        } else {
            if (mixInfo.clips.first == -1) {
                // No clip to mix, abort
                continue;
            }
            // Make sure we have enough space in clip to resize
            // mixInfo.spaces.first is the maximum frames we have to expand first clip on the right
            mixInfo.spaces.first =
                m_allClips[mixInfo.clips.first]->m_endlessResize
                    ? m_allClips[s]->getPlaytime()
                    : qMin(m_allClips[s]->getPlaytime(), m_allClips[mixInfo.clips.first]->getMaxDuration() - m_allClips[mixInfo.clips.first]->getOut() - 1);
            // mixInfo.spaces.second is the maximum frames we have to expand second clip on the left
            mixInfo.spaces.second = m_allClips[s]->m_endlessResize ? m_allClips[mixInfo.clips.first]->getPlaytime()
                                                                   : qMin(m_allClips[mixInfo.clips.first]->getPlaytime(), m_allClips[s]->getIn());
            if (getTrackById_const(mixInfo.selectedTrack)->hasStartMix(mixInfo.clips.first)) {
                int spaceBeforeMix =
                    m_allClips[s]->getPosition() - (m_allClips[mixInfo.clips.first]->getPosition() + m_allClips[mixInfo.clips.first]->getMixDuration());
                mixInfo.spaces.second = mixInfo.spaces.second == -1 ? spaceBeforeMix : qMin(mixInfo.spaces.second, spaceBeforeMix);
            }
            if (getTrackById_const(mixInfo.selectedTrack)->hasEndMix(s)) {
                MixInfo mixData = getTrackById_const(mixInfo.selectedTrack)->getMixInfo(s).second;
                if (mixData.secondClipId > -1) {
                    int spaceAfterMix = m_allClips[s]->getPlaytime() - m_allClips[mixData.secondClipId]->getMixDuration();
                    mixInfo.spaces.first = mixInfo.spaces.first == -1 ? spaceAfterMix : qMin(mixInfo.spaces.first, spaceAfterMix);
                }
            }
            if (mixInfo.spaces.first > -1 && mixInfo.spaces.second > -1 && (mixInfo.spaces.first + mixInfo.spaces.second < 3)) {
                noSpaceInClip = 1;
                continue;
            }
            // Create Mix at start of selected clip
            mixInfo.clips.second = s;
            mixInfo.clipId = s;
            processMix(mixInfo);
            clipsToMixList << mixInfo;
            continue;
        }
    }
    if (clipsToMixList.isEmpty()) {
        if (noSpaceInClip > 0) {
            pCore->displayMessage(i18n("Not enough frames at clip %1 to apply the mix", noSpaceInClip == 1 ? i18n("start") : i18n("end")), ErrorMessage, 500);
        } else {
            pCore->displayMessage(i18n("Select a clip to apply the mix"), ErrorMessage, 500);
        }
        return false;
    }
    pCore->pushUndo(undo, redo, i18n("Create mix"));
    // Reselect clips
    if (!initialSelection.empty()) {
        requestSetSelection(initialSelection);
    }
    return true;
}

bool TimelineModel::requestClipMix(const QString &mixId, std::pair<int, int> clipIds, std::pair<int, int> mixDurations, int trackId, int position,
                                   bool updateView, bool invalidateTimeline, bool finalMove, Fun &undo, Fun &redo, bool groupMove)
{
    if (trackId == -1) {
        return false;
    }
    Q_ASSERT(isClip(clipIds.first));
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    bool ok = true;
    Fun update_model = []() { return true; };
    // Move on same track, simply inform the view
    updateView = false;
    bool notifyViewOnly = true;
    update_model = [clipIds, this, trackId, position, invalidateTimeline, mixDurations]() {
        QModelIndex modelIndex = makeClipIndexFromID(clipIds.second);
        notifyChange(modelIndex, modelIndex, {StartRole, DurationRole});
        QModelIndex modelIndex2 = makeClipIndexFromID(clipIds.first);
        notifyChange(modelIndex2, modelIndex2, DurationRole);
        if (invalidateTimeline && !getTrackById_const(trackId)->isAudioTrack()) {
            Q_EMIT invalidateZone(position - mixDurations.second, position + mixDurations.first);
        }
        return true;
    };
    if (notifyViewOnly) {
        PUSH_LAMBDA(update_model, local_undo);
    }
    ok = getTrackById(trackId)->requestClipMix(mixId, clipIds, mixDurations, updateView, finalMove, local_undo, local_redo, groupMove);
    if (!ok) {
        qWarning() << "mix failed, reverting";
        bool undone = local_undo();
        Q_ASSERT(undone);
        return false;
    }
    update_model();
    if (notifyViewOnly) {
        PUSH_LAMBDA(update_model, local_redo);
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return ok;
}

bool TimelineModel::requestFakeClipMove(int clipId, int trackId, int position, bool updateView, bool logUndo, bool invalidateTimeline)
{
    QWriteLocker locker(&m_lock);
    TRACE(clipId, trackId, position, updateView, logUndo, invalidateTimeline)
    Q_ASSERT(m_allClips.count(clipId) > 0);
    if (m_groups->isInGroup(clipId)) {
        // element is in a group.
        int groupId = m_groups->getRootId(clipId);
        int current_trackId = getClipTrackId(clipId);
        int track_pos1 = getTrackPosition(trackId);
        int track_pos2 = getTrackPosition(current_trackId);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_allClips[clipId]->getPosition();
        bool res = requestFakeGroupMove(clipId, groupId, delta_track, delta_pos, updateView, logUndo);
        TRACE_RES(res);
        return res;
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = requestFakeClipMove(clipId, trackId, position, updateView, invalidateTimeline, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move clip"));
    }
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestClipMove(int clipId, int trackId, int position, bool moveMirrorTracks, bool updateView, bool logUndo, bool invalidateTimeline,
                                    bool revertMove)
{
    QWriteLocker locker(&m_lock);
    TRACE(clipId, trackId, position, updateView, logUndo, invalidateTimeline);
    Q_ASSERT(m_allClips.count(clipId) > 0);
    if (m_allClips[clipId]->getPosition() == position && getClipTrackId(clipId) == trackId) {
        TRACE_RES(true);
        return true;
    }
    if (m_groups->isInGroup(clipId)) {
        // element is in a group.
        int groupId = m_groups->getRootId(clipId);
        int current_trackId = getClipTrackId(clipId);
        int track_pos1 = getTrackPosition(trackId);
        int track_pos2 = getTrackPosition(current_trackId);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_allClips[clipId]->getPosition();
        return requestGroupMove(clipId, groupId, delta_track, delta_pos, moveMirrorTracks, updateView, logUndo, revertMove);
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = requestClipMove(clipId, trackId, position, moveMirrorTracks, updateView, invalidateTimeline, logUndo, undo, redo, revertMove);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move clip"));
    }
    TRACE_RES(res);
    return res;
}

std::shared_ptr<SubtitleModel> TimelineModel::getSubtitleModel()
{
    return m_subtitleModel;
}

int TimelineModel::cutSubtitle(int position, Fun &undo, Fun &redo)
{
    if (m_subtitleModel) {
        return m_subtitleModel->cutSubtitle(position, undo, redo);
    }
    return -1;
}

bool TimelineModel::requestSubtitleMove(int clipId, int position, bool updateView, bool logUndo, bool finalMove)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allSubtitles.count(clipId) > 0);
    GenTime oldPos = m_allSubtitles.at(clipId);
    GenTime newPos(position, pCore->getCurrentFps());
    if (oldPos == newPos) {
        return true;
    }
    if (m_groups->isInGroup(clipId)) {
        // element is in a group.
        int groupId = m_groups->getRootId(clipId);
        int delta_pos = position - oldPos.frames(pCore->getCurrentFps());
        return requestGroupMove(clipId, groupId, 0, delta_pos, false, updateView, logUndo);
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = requestSubtitleMove(clipId, position, updateView, logUndo, logUndo, finalMove, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move subtitle"));
    }
    return res;
}

bool TimelineModel::requestSubtitleMove(int clipId, int position, bool updateView, bool first, bool last, bool finalMove, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    GenTime oldPos = m_allSubtitles.at(clipId);
    GenTime newPos(position, pCore->getCurrentFps());
    Fun local_redo = [this, clipId, newPos, reloadSubFile = last && finalMove, updateView]() {
        return m_subtitleModel->moveSubtitle(clipId, newPos, reloadSubFile, updateView);
    };
    Fun local_undo = [this, oldPos, clipId, reloadSubFile = first && finalMove, updateView]() {
        return m_subtitleModel->moveSubtitle(clipId, oldPos, reloadSubFile, updateView);
    };
    bool res = local_redo();
    if (res) {
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    } else {
        local_undo();
    }
    return res;
}

bool TimelineModel::requestClipMoveAttempt(int clipId, int trackId, int position)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allClips.count(clipId) > 0);
    if (m_allClips[clipId]->getPosition() == position && getClipTrackId(clipId) == trackId) {
        return true;
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = true;
    if (m_groups->isInGroup(clipId)) {
        // element is in a group.
        int groupId = m_groups->getRootId(clipId);
        int current_trackId = getClipTrackId(clipId);
        int track_pos1 = getTrackPosition(trackId);
        int track_pos2 = getTrackPosition(current_trackId);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_allClips[clipId]->getPosition();
        res = requestGroupMove(clipId, groupId, delta_track, delta_pos, false, false, undo, redo, false, false);
    } else {
        res = requestClipMove(clipId, trackId, position, true, false, false, false, undo, redo);
    }
    if (res) {
        undo();
    }
    return res;
}

QVariantList TimelineModel::suggestItemMove(int itemId, int trackId, int position, int cursorPosition, int snapDistance)
{
    if (isClip(itemId)) {
        return suggestClipMove(itemId, trackId, position, cursorPosition, snapDistance);
    }
    if (isComposition(itemId)) {
        return suggestCompositionMove(itemId, trackId, position, cursorPosition, snapDistance);
    }
    if (isSubTitle(itemId)) {
        return {suggestSubtitleMove(itemId, position, cursorPosition, snapDistance), -1};
    }
    return QVariantList();
}

int TimelineModel::adjustFrame(int frame, int trackId)
{
    if (m_editMode == TimelineMode::InsertEdit && isTrack(trackId)) {
        frame = qMin(frame, getTrackById_const(trackId)->trackDuration());
    }
    return frame;
}

int TimelineModel::suggestSubtitleMove(int subId, int position, int cursorPosition, int snapDistance)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isSubTitle(subId));
    int currentPos = getSubtitlePosition(subId);
    if (currentPos == position || m_subtitleModel->isLocked()) {
        return position;
    }
    int newPos = position;
    if (snapDistance > 0) {
        int offset = 0;
        std::vector<int> ignored_pts;
        // For snapping, we must ignore all in/outs of the clips of the group being moved
        std::unordered_set<int> all_items = {subId};
        if (m_groups->isInGroup(subId)) {
            int groupId = m_groups->getRootId(subId);
            all_items = m_groups->getLeaves(groupId);
        }
        for (int current_clipId : all_items) {
            if (isClip(current_clipId)) {
                m_allClips[current_clipId]->allSnaps(ignored_pts, offset);
            } else if (isComposition(current_clipId) || isSubTitle(current_clipId)) {
                // Composition or subtitle
                int in = getItemPosition(current_clipId) - offset;
                ignored_pts.push_back(in);
                ignored_pts.push_back(in + getItemPlaytime(current_clipId));
            }
        }
        int snapped = getBestSnapPos(currentPos, position - currentPos, ignored_pts, cursorPosition, snapDistance);
        if (snapped >= 0) {
            newPos = snapped;
        }
    }
    // m_subtitleModel->moveSubtitle(GenTime(currentPos, pCore->getCurrentFps()), GenTime(position, pCore->getCurrentFps()));
    if (requestSubtitleMove(subId, newPos, true, false)) {
        return newPos;
    }
    return position;
}

QVariantList TimelineModel::suggestClipMove(int clipId, int trackId, int position, int cursorPosition, int snapDistance, bool moveMirrorTracks)
{
    QWriteLocker locker(&m_lock);
    TRACE(clipId, trackId, position, cursorPosition, snapDistance);
    Q_ASSERT(isClip(clipId));
    Q_ASSERT(isTrack(trackId));
    int currentPos = m_editMode == TimelineMode::NormalEdit ? getClipPosition(clipId) : m_allClips[clipId]->getFakePosition();
    int offset = m_editMode == TimelineMode::NormalEdit ? 0 : getClipPosition(clipId) - currentPos;
    int sourceTrackId = (m_editMode != TimelineMode::NormalEdit) ? m_allClips[clipId]->getFakeTrackId() : getClipTrackId(clipId);
    if (sourceTrackId > -1 && getTrackById_const(trackId)->isAudioTrack() != getTrackById_const(sourceTrackId)->isAudioTrack()) {
        // Trying move on incompatible track type, stay on same track
        trackId = sourceTrackId;
    }
    if (currentPos == position && sourceTrackId == trackId) {
        TRACE_RES(position);
        return {position, trackId};
    }
    if (m_editMode == TimelineMode::InsertEdit) {
        int maxPos = getTrackById_const(trackId)->trackDuration();
        if (m_allClips[clipId]->getCurrentTrackId() == trackId) {
            maxPos -= m_allClips[clipId]->getPlaytime();
        }
        position = qMin(position, maxPos);
    }
    bool after = position > currentPos;
    if (snapDistance > 0) {
        std::vector<int> ignored_pts;
        // For snapping, we must ignore all in/outs of the clips of the group being moved
        std::unordered_set<int> all_items = {clipId};
        if (m_groups->isInGroup(clipId)) {
            int groupId = m_groups->getRootId(clipId);
            all_items = m_groups->getLeaves(groupId);
        }
        for (int current_clipId : all_items) {
            if (isClip(current_clipId)) {
                m_allClips[current_clipId]->allSnaps(ignored_pts, offset);
            } else if (isComposition(current_clipId) || isSubTitle(current_clipId)) {
                // Composition
                int in = getItemPosition(current_clipId) - offset;
                ignored_pts.push_back(in);
                ignored_pts.push_back(in + getItemPlaytime(current_clipId));
            }
        }
        int snapped = getBestSnapPos(currentPos, position - currentPos, ignored_pts, cursorPosition, snapDistance);
        if (snapped >= 0) {
            position = snapped;
        }
    }
    position = qMax(0, position);
    if (currentPos == position && sourceTrackId == trackId) {
        TRACE_RES(position);
        return {position, trackId};
    }
    bool isInGroup = m_groups->isInGroup(clipId);
    if (sourceTrackId == trackId) {
        // Same track move, check if there is a mix and limit move
        std::pair<MixInfo, MixInfo> mixData = getTrackById_const(trackId)->getMixInfo(clipId);
        if (mixData.first.firstClipId > -1) {
            // Clip has start mix
            int clipDuration = m_allClips[clipId]->getPlaytime();
            // ensure we don't move into clip
            bool allowMove = false;
            if (isInGroup) {
                // Check if in same group as clip mix
                int groupId = m_groups->getRootId(clipId);
                if (groupId == m_groups->getRootId(mixData.first.firstClipId)) {
                    allowMove = true;
                }
            }
            if (!allowMove && position + clipDuration > mixData.first.firstClipInOut.first && position < mixData.first.firstClipInOut.second) {
                // Abort move
                return {currentPos, sourceTrackId};
            }
        }
        if (mixData.second.firstClipId > -1) {
            // Clip has end mix
            int clipDuration = m_allClips[clipId]->getPlaytime();
            bool allowMove = false;
            if (isInGroup) {
                // Check if in same group as clip mix
                int groupId = m_groups->getRootId(clipId);
                if (groupId == m_groups->getRootId(mixData.second.secondClipId)) {
                    allowMove = true;
                }
            }
            if (!allowMove && position + clipDuration > mixData.second.secondClipInOut.first && position < mixData.second.secondClipInOut.second) {
                // Abort move
                return {currentPos, sourceTrackId};
            }
        }
    }
    // we check if move is possible
    bool possible = (m_editMode == TimelineMode::NormalEdit) ? requestClipMove(clipId, trackId, position, moveMirrorTracks, true, false, false)
                                                             : requestFakeClipMove(clipId, trackId, position, true, false, false);

    if (possible) {
        TRACE_RES(position);
        if (m_editMode != TimelineMode::NormalEdit) {
            trackId = m_allClips[clipId]->getFakeTrackId();
        }
        return {position, trackId};
    }
    if (sourceTrackId == -1) {
        // not clear what to do here, if the current move doesn't work. We could try to find empty space, but it might end up being far away...
        TRACE_RES(currentPos);
        return {currentPos, -1};
    }
    // Find best possible move
    if (!isInGroup) {
        // Try same track move
        if (trackId != sourceTrackId && sourceTrackId != -1) {
            trackId = sourceTrackId;
            possible = requestClipMove(clipId, trackId, position, moveMirrorTracks, true, false, false);
            if (!possible) {
                qWarning() << "can't move clip" << clipId << "on track" << trackId << "at" << position;
            } else {
                TRACE_RES(position);
                return {position, trackId};
            }
        }

        int blank_length = getTrackById_const(trackId)->getBlankSizeNearClip(clipId, after);
        if (blank_length < INT_MAX) {
            if (after) {
                position = currentPos + blank_length;
            } else {
                position = currentPos - blank_length;
            }
        } else {
            TRACE_RES(currentPos);
            return {currentPos, sourceTrackId};
        }
        possible = requestClipMove(clipId, trackId, position, moveMirrorTracks, true, false, false);
        TRACE_RES(possible ? position : currentPos);
        if (possible) {
            return {position, trackId};
        }
        return {currentPos, sourceTrackId};
    }
    if (trackId != sourceTrackId) {
        // Try same track move
        possible = requestClipMove(clipId, sourceTrackId, position, moveMirrorTracks, true, false, false);
        if (possible) {
            return {position, sourceTrackId};
        }
        return {currentPos, sourceTrackId};
    }
    // find best pos for groups
    int groupId = m_groups->getRootId(clipId);
    std::unordered_set<int> all_items = m_groups->getLeaves(groupId);
    QMap<int, int> trackPosition;

    // First pass, sort clips by track and keep only the first / last depending on move direction
    for (int current_clipId : all_items) {
        int clipTrack = getItemTrackId(current_clipId);
        if (clipTrack == -1) {
            continue;
        }
        int in = getItemPosition(current_clipId);
        if (trackPosition.contains(clipTrack)) {
            if (after) {
                // keep only last clip position for track
                int out = in + getItemPlaytime(current_clipId);
                if (trackPosition.value(clipTrack) < out) {
                    trackPosition.insert(clipTrack, out);
                }
            } else {
                // keep only first clip position for track
                if (trackPosition.value(clipTrack) > in) {
                    trackPosition.insert(clipTrack, in);
                }
            }
        } else {
            trackPosition.insert(clipTrack, after ? in + getItemPlaytime(current_clipId) - 1 : in);
        }
    }

    // Now check space on each track
    QMapIterator<int, int> i(trackPosition);
    int blank_length = 0;
    while (i.hasNext()) {
        i.next();
        int track_space;
        if (!after) {
            // Check space before the position
            if (isSubtitleTrack(i.key())) {
                track_space = i.value();
            } else {
                track_space = i.value() - getTrackById_const(i.key())->getBlankStart(i.value() - 1);
            }
            if (blank_length == 0 || blank_length > track_space) {
                blank_length = track_space;
            }
        } else {
            // Check space after the position
            if (isSubtitleTrack(i.key())) {
                continue;
            }
            track_space = getTrackById(i.key())->getBlankEnd(i.value() + 1) - i.value();
            if (blank_length == 0 || blank_length > track_space) {
                blank_length = track_space;
            }
        }
    }
    if (snapDistance > 0) {
        if (blank_length > 10 * snapDistance) {
            blank_length = 0;
        }
    } else if (blank_length / pCore->getProjectProfile().fps() > 5) {
        blank_length = 0;
    }
    if (blank_length != 0) {
        int updatedPos = currentPos + (after ? blank_length : -blank_length);
        possible = requestClipMove(clipId, trackId, updatedPos, moveMirrorTracks, true, false, false);
        if (possible) {
            TRACE_RES(updatedPos);
            return {updatedPos, trackId};
        }
    }
    TRACE_RES(currentPos);
    return {currentPos, sourceTrackId};
}

QVariantList TimelineModel::suggestCompositionMove(int compoId, int trackId, int position, int cursorPosition, int snapDistance)
{
    QWriteLocker locker(&m_lock);
    TRACE(compoId, trackId, position, cursorPosition, snapDistance);
    Q_ASSERT(isComposition(compoId));
    Q_ASSERT(isTrack(trackId));
    int currentPos = getCompositionPosition(compoId);
    int currentTrack = getCompositionTrackId(compoId);
    if (getTrackById_const(trackId)->isAudioTrack()) {
        // Trying move on incompatible track type, stay on same track
        trackId = currentTrack;
    }
    if (currentPos == position && currentTrack == trackId) {
        TRACE_RES(position);
        return {position, trackId};
    }

    if (snapDistance > 0) {
        // For snapping, we must ignore all in/outs of the clips of the group being moved
        std::vector<int> ignored_pts;
        if (m_groups->isInGroup(compoId)) {
            int groupId = m_groups->getRootId(compoId);
            auto all_items = m_groups->getLeaves(groupId);
            for (int current_compoId : all_items) {
                // TODO: fix for composition
                int in = getItemPosition(current_compoId);
                ignored_pts.push_back(in);
                ignored_pts.push_back(in + getItemPlaytime(current_compoId));
            }
        } else {
            int in = currentPos;
            int out = in + getCompositionPlaytime(compoId);
            ignored_pts.push_back(in);
            ignored_pts.push_back(out);
        }
        int snapped = getBestSnapPos(currentPos, position - currentPos, ignored_pts, cursorPosition, snapDistance);
        if (snapped >= 0) {
            position = snapped;
        }
    }
    position = qMax(0, position);
    if (currentPos == position && currentTrack == trackId) {
        TRACE_RES(position);
        return {position, trackId};
    }
    // we check if move is possible
    bool possible = requestCompositionMove(compoId, trackId, position, true, false);
    if (possible) {
        TRACE_RES(position);
        return {position, trackId};
    }
    TRACE_RES(currentPos);
    return {currentPos, currentTrack};
}

bool TimelineModel::requestClipCreation(const QString &binClipId, int &id, PlaylistState::ClipState state, int audioStream, double speed, bool warp_pitch,
                                        Fun &undo, Fun &redo)
{
    QString bid = binClipId;
    if (binClipId.contains(QLatin1Char('/'))) {
        bid = binClipId.section(QLatin1Char('/'), 0, 0);
    }
    if (!pCore->projectItemModel()->hasClip(bid)) {
        qWarning() << "master clip not found";
        return false;
    }
    std::shared_ptr<ProjectClip> master = pCore->projectItemModel()->getClipByBinID(bid);
    if (!master->statusReady() || !master->isCompatible(state)) {
        qWarning() << "clip not ready or not compatible" << state << master->statusReady();
        return false;
    }
    int clipId = TimelineModel::getNextId();
    id = clipId;
    qDebug() << "======\nCREATING TIMELINE OBJECT: " << clipId << "\n========================";
    Fun local_undo = deregisterClip_lambda(clipId);
    ClipModel::construct(shared_from_this(), bid, clipId, state, audioStream, speed, warp_pitch);
    auto clip = m_allClips[clipId];
    Fun local_redo = [clip, this, state, audioStream, speed, warp_pitch]() {
        // We capture a shared_ptr to the clip, which means that as long as this undo object lives, the clip object is not deleted. To insert it back it is
        // sufficient to register it.
        registerClip(clip, true);
        clip->refreshProducerFromBin(-1, state, audioStream, speed, warp_pitch);
        return true;
    };

    if (binClipId.contains(QLatin1Char('/'))) {
        int in = binClipId.section(QLatin1Char('/'), 1, 1).toInt();
        int out = binClipId.section(QLatin1Char('/'), 2, 2).toInt();
        int initLength = m_allClips[clipId]->getPlaytime();
        bool res = true;
        if (in != 0) {
            initLength -= in;
            res = requestItemResize(clipId, initLength, false, true, local_undo, local_redo);
        }
        int updatedDuration = out - in + 1; // +1: e.g. in=100, out=101 is 2 frames long
        res = res && requestItemResize(clipId, updatedDuration, true, true, local_undo, local_redo);
        if (!res) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestClipInsertion(const QString &binClipId, int trackId, int position, int &id, bool logUndo, bool refreshView, bool useTargets)
{
    QWriteLocker locker(&m_lock);
    TRACE(binClipId, trackId, position, id, logUndo, refreshView, useTargets);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    QVector<int> allowedTracks;
    if (useTargets) {
        auto it = m_allTracks.cbegin();
        while (it != m_allTracks.cend()) {
            int target_track = (*it)->getId();
            if (getTrackById_const(target_track)->shouldReceiveTimelineOp()) {
                allowedTracks << target_track;
            }
            ++it;
        }
    }
    if (useTargets && allowedTracks.isEmpty()) {
        pCore->displayMessage(i18n("No available track for insert operation"), ErrorMessage, 500);
        return false;
    }
    bool result = requestClipInsertion(binClipId, trackId, position, id, logUndo, refreshView, useTargets, undo, redo, allowedTracks);
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Insert Clip"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestClipInsertion(const QString &binClipId, int trackId, int position, int &id, bool logUndo, bool refreshView, bool useTargets,
                                         Fun &undo, Fun &redo, const QVector<int> &allowedTracks)
{
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    bool res = false;
    ClipType::ProducerType type = ClipType::Unknown;
    // binClipId id is in the form: A2/10/50
    // A2 means audio only insertion for bin clip with id 2
    // 10 is in point
    // 50 is out point
    QString binIdWithInOut = binClipId;
    // bid is the A2 part
    QString bid = binClipId.section(QLatin1Char('/'), 0, 0);
    // dropType indicates if we want a normal drop (disabled), audio only or video only drop
    PlaylistState::ClipState dropType = PlaylistState::Disabled;
    if (bid.startsWith(QLatin1Char('A'))) {
        dropType = PlaylistState::AudioOnly;
        bid.remove(0, 1);
        binIdWithInOut.remove(0, 1);
    } else if (bid.startsWith(QLatin1Char('V'))) {
        dropType = PlaylistState::VideoOnly;
        bid.remove(0, 1);
        binIdWithInOut.remove(0, 1);
    }
    if (!pCore->projectItemModel()->hasClip(bid)) {
        qWarning() << "no clip found in bin for" << bid;
        return false;
    }

    bool audioDrop = false;
    if (!useTargets) {
        audioDrop = getTrackById_const(trackId)->isAudioTrack();
        if (audioDrop) {
            if (dropType == PlaylistState::VideoOnly) {
                return false;
            }
        } else if (dropType == PlaylistState::AudioOnly) {
            return false;
        }
    }

    std::shared_ptr<ProjectClip> master = pCore->projectItemModel()->getClipByBinID(bid);
    type = master->clipType();
    // Ensure we don't insert a timeline clip onto itself
    if (type == ClipType::Timeline && !master->canBeDropped(m_uuid)) {
        // Abort insert
        pCore->displayMessage(i18n("You cannot insert a sequence containing itself"), ErrorMessage);
        return false;
    }
    if (useTargets && m_audioTarget.isEmpty() && m_videoTarget == -1) {
        useTargets = false;
    }
    if ((dropType == PlaylistState::Disabled || dropType == PlaylistState::AudioOnly) &&
        (type == ClipType::AV || type == ClipType::Playlist || type == ClipType::Timeline)) {
        bool useAudioTarget = false;
        if (useTargets && !m_audioTarget.isEmpty() && m_videoTarget == -1) {
            // If audio target is set but no video target, only insert audio
            useAudioTarget = true;
        } else if (useTargets && (getTrackById_const(trackId)->isLocked() || !allowedTracks.contains(trackId))) {
            // Video target set but locked
            useAudioTarget = true;
        }
        if (useAudioTarget) {
            // Find first possible audio target
            QList<int> audioTargetTracks = m_audioTarget.keys();
            trackId = -1;
            for (int tid : qAsConst(audioTargetTracks)) {
                if (tid > -1 && !getTrackById_const(tid)->isLocked() && allowedTracks.contains(tid)) {
                    trackId = tid;
                    break;
                }
            }
        }
        if (trackId == -1) {
            if (!allowedTracks.isEmpty()) {
                // No active tracks, aborting
                return true;
            }
            pCore->displayMessage(i18n("No available track for insert operation"), ErrorMessage);
            return false;
        }
        int audioStream = -1;
        QList<int> keys = m_binAudioTargets.keys();
        if (!useTargets) {
            // Drag and drop, calculate target tracks
            if (audioDrop) {
                if (keys.count() > 1) {
                    // Dropping a clip with several audio streams
                    int tracksBelow = getLowerTracksId(trackId, TrackType::AudioTrack).count();
                    if (tracksBelow < keys.count() - 1) {
                        // We don't have enough audio tracks below, check above
                        QList<int> audioTrackIds = getTracksIds(true);
                        if (audioTrackIds.count() < keys.count()) {
                            // Not enough audio tracks
                            pCore->displayMessage(i18n("Not enough audio tracks for all streams (%1)", keys.count()), ErrorMessage);
                            return false;
                        }
                        trackId = audioTrackIds.at(audioTrackIds.count() - keys.count());
                    }
                }
                if (keys.isEmpty()) {
                    pCore->displayMessage(i18n("No available track for insert operation"), ErrorMessage);
                    return false;
                }
                audioStream = keys.first();
            } else {
                // Dropping video, ensure we have enough audio tracks for its streams
                int mirror = getMirrorTrackId(trackId);
                QList<int> audioTids = {};
                if (mirror > -1) {
                    audioTids = getLowerTracksId(mirror, TrackType::AudioTrack);
                }
                if (audioTids.count() < keys.count() - 1 || (mirror == -1 && !keys.isEmpty())) {
                    // Check if project has enough audio tracks
                    if (keys.count() > getTracksIds(true).count()) {
                        // Not enough audio tracks in the project
                        pCore->displayMessage(i18n("Not enough audio tracks for all streams (%1)", keys.count()), ErrorMessage);
                        return false;
                    } else {
                        // Check if all audio tracks are locked. In that case allow inserting video only
                        QList<int> audioTracks = getTracksIds(true);
                        auto is_unlocked = [&](int tid) { return !getTrackById_const(tid)->isLocked(); };
                        bool hasUnlockedAudio = std::any_of(audioTracks.begin(), audioTracks.end(), is_unlocked);
                        if (hasUnlockedAudio) {
                            pCore->displayMessage(i18n("No available track for insert operation"), ErrorMessage);
                            return false;
                        } else {
                            keys.clear();
                        }
                    }
                }
            }
        } else if (audioDrop) {
            // Drag & drop, use our first audio target
            audioStream = m_audioTarget.first();
        } else {
            // Using target tracks
            if (m_audioTarget.contains(trackId)) {
                audioStream = m_audioTarget.value(trackId);
            }
        }

        res = requestClipCreation(binIdWithInOut, id, getTrackById_const(trackId)->trackType(), audioStream, 1.0, false, local_undo, local_redo);
        res = res && requestClipMove(id, trackId, position, true, refreshView, logUndo, logUndo, local_undo, local_redo);
        // Get mirror track
        int mirror = dropType == PlaylistState::Disabled ? getMirrorTrackId(trackId) : -1;
        if (mirror > -1 && getTrackById_const(mirror)->isLocked() && !useTargets) {
            mirror = -1;
        }
        QList<int> target_track;
        if (audioDrop) {
            if (m_videoTarget > -1 && !getTrackById_const(m_videoTarget)->isLocked() && dropType != PlaylistState::AudioOnly) {
                target_track << m_videoTarget;
            }
        } else if (useTargets) {
            QList<int> targetIds = m_audioTarget.keys();
            targetIds.removeAll(trackId);
            for (int &ix : targetIds) {
                if (!getTrackById_const(ix)->isLocked() && allowedTracks.contains(ix)) {
                    target_track << ix;
                }
            }
        }

        bool canMirrorDrop = !useTargets && ((mirror > -1 && (audioDrop || !keys.isEmpty())) || keys.count() > 1);
        QMap<int, int> dropTargets;
        if (res && (canMirrorDrop || !target_track.isEmpty()) && master->hasAudioAndVideo()) {
            if (!useTargets) {
                int streamsCount = 0;
                target_track.clear();
                QList<int> audioTids;
                if (!audioDrop) {
                    // insert audio mirror track
                    if (mirror > -1) {
                        target_track << mirror;
                        audioTids = getLowerTracksId(mirror, TrackType::AudioTrack);
                    }
                } else {
                    audioTids = getLowerTracksId(trackId, TrackType::AudioTrack);
                }
                // First audio stream already inserted in target_track or in timeline
                streamsCount = m_binAudioTargets.count() - 1;
                while (streamsCount > 0 && !audioTids.isEmpty()) {
                    target_track << audioTids.takeFirst();
                    streamsCount--;
                }
                QList<int> aTargets = m_binAudioTargets.keys();
                if (audioDrop) {
                    aTargets.removeAll(audioStream);
                }
                std::sort(aTargets.begin(), aTargets.end());
                for (int i = 0; i < target_track.count() && i < aTargets.count(); ++i) {
                    dropTargets.insert(target_track.at(i), aTargets.at(i));
                }
                if (audioDrop && mirror > -1) {
                    target_track << mirror;
                }
            }
            if (target_track.isEmpty() && useTargets) {
                // No available track for splitting, abort
                pCore->displayMessage(i18n("No available track for split operation"), ErrorMessage);
                res = false;
            }
            if (!res) {
                bool undone = local_undo();
                Q_ASSERT(undone);
                id = -1;
                return false;
            }
            // Process all mirror insertions
            std::function<bool(void)> audio_undo = []() { return true; };
            std::function<bool(void)> audio_redo = []() { return true; };
            std::unordered_set<int> createdMirrors = {id};
            int mirrorAudioStream = -1;
            for (int &target_ix : target_track) {
                bool currentDropIsAudio = !audioDrop;
                if (!useTargets && m_binAudioTargets.count() > 1 && dropTargets.contains(target_ix)) {
                    // Audio clip dropped first but has other streams
                    currentDropIsAudio = true;
                    mirrorAudioStream = dropTargets.value(target_ix);
                    if (mirrorAudioStream == audioStream) {
                        continue;
                    }
                } else if (currentDropIsAudio) {
                    if (!useTargets) {
                        mirrorAudioStream = dropTargets.value(target_ix);
                    } else {
                        mirrorAudioStream = m_audioTarget.value(target_ix);
                    }
                }
                int newId;
                res = requestClipCreation(binIdWithInOut, newId, currentDropIsAudio ? PlaylistState::AudioOnly : PlaylistState::VideoOnly,
                                          currentDropIsAudio ? mirrorAudioStream : -1, 1.0, false, audio_undo, audio_redo);
                if (res) {
                    res = requestClipMove(newId, target_ix, position, true, true, true, true, audio_undo, audio_redo);
                    // use lazy evaluation to group only if move was successful
                    if (!res) {
                        pCore->displayMessage(i18n("Audio split failed: no viable track"), ErrorMessage);
                        bool undone = audio_undo();
                        Q_ASSERT(undone);
                        break;
                    } else {
                        createdMirrors.insert(newId);
                    }
                } else {
                    pCore->displayMessage(i18n("Audio split failed: impossible to create audio clip"), ErrorMessage);
                    bool undone = audio_undo();
                    Q_ASSERT(undone);
                    break;
                }
            }
            if (res) {
                requestClipsGroup(createdMirrors, audio_undo, audio_redo, GroupType::AVSplit);
                UPDATE_UNDO_REDO(audio_redo, audio_undo, local_undo, local_redo);
            }
        }
    } else {
        std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(bid);
        if (dropType == PlaylistState::Disabled) {
            dropType = getTrackById_const(trackId)->trackType();
        } else if (dropType != getTrackById_const(trackId)->trackType()) {
            return false;
        }
        QString normalisedBinId = binClipId;
        if (normalisedBinId.startsWith(QLatin1Char('A')) || normalisedBinId.startsWith(QLatin1Char('V'))) {
            normalisedBinId.remove(0, 1);
        }
        res = requestClipCreation(normalisedBinId, id, dropType, binClip->getProducerIntProperty(QStringLiteral("audio_index")), 1.0, false, local_undo,
                                  local_redo);
        res = res && requestClipMove(id, trackId, position, true, refreshView, logUndo, logUndo, local_undo, local_redo);
    }
    if (!res) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        id = -1;
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestItemDeletion(int itemId, Fun &undo, Fun &redo, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    if (m_groups->isInGroup(itemId)) {
        return requestGroupDeletion(itemId, undo, redo);
    }
    if (isClip(itemId)) {
        return requestClipDeletion(itemId, undo, redo, logUndo);
    }
    if (isComposition(itemId)) {
        return requestCompositionDeletion(itemId, undo, redo);
    }
    if (isSubTitle(itemId)) {
        return requestSubtitleDeletion(itemId, undo, redo, true, true);
    }
    Q_ASSERT(false);
    return false;
}

bool TimelineModel::requestItemDeletion(int itemId, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId, logUndo);
    Q_ASSERT(isItem(itemId));
    QString actionLabel;
    if (m_groups->isInGroup(itemId)) {
        actionLabel = i18n("Remove group");
    } else {
        if (isClip(itemId)) {
            actionLabel = i18n("Delete Clip");
        } else if (isComposition(itemId)) {
            actionLabel = i18n("Delete Composition");
        } else if (isSubTitle(itemId)) {
            actionLabel = i18n("Delete Subtitle");
        }
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = requestItemDeletion(itemId, undo, redo, logUndo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, actionLabel);
    }
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestClipDeletion(int clipId, Fun &undo, Fun &redo, bool logUndo)
{
    int trackId = getClipTrackId(clipId);
    if (trackId != -1) {
        bool res = true;
        if (getTrackById_const(trackId)->hasStartMix(clipId)) {
            MixInfo mixData = getTrackById_const(trackId)->getMixInfo(clipId).first;
            if (isClip(mixData.firstClipId)) {
                res = getTrackById(trackId)->requestRemoveMix({mixData.firstClipId, clipId}, undo, redo);
            }
        }
        if (getTrackById_const(trackId)->hasEndMix(clipId)) {
            MixInfo mixData = getTrackById_const(trackId)->getMixInfo(clipId).second;
            if (isClip(mixData.secondClipId)) {
                res = getTrackById(trackId)->requestRemoveMix({clipId, mixData.secondClipId}, undo, redo);
            }
        }
        res = res && getTrackById(trackId)->requestClipDeletion(clipId, true, logUndo && !m_closing, undo, redo, false, true);
        if (!res) {
            undo();
            return false;
        }
    }
    auto operation = deregisterClip_lambda(clipId);
    auto clip = m_allClips[clipId];
    Fun reverse = [this, clip]() {
        // We capture a shared_ptr to the clip, which means that as long as this undo object lives,
        // the clip object is not deleted. To insert it back it is sufficient to register it.
        registerClip(clip, true);
        return true;
    };
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    undo();
    return false;
}

bool TimelineModel::requestSubtitleDeletion(int clipId, Fun &undo, Fun &redo, bool first, bool last)
{
    GenTime startTime = m_allSubtitles.at(clipId);
    SubtitledTime sub = m_subtitleModel->getSubtitle(startTime);
    Fun operation = [this, clipId, last]() { return m_subtitleModel->removeSubtitle(clipId, false, last); };
    GenTime start = sub.start();
    GenTime end = sub.end();
    QString text = sub.subtitle();
    Fun reverse = [this, clipId, start, end, text, first]() { return m_subtitleModel->addSubtitle(clipId, start, end, text, false, first); };
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

bool TimelineModel::requestCompositionDeletion(int compositionId, Fun &undo, Fun &redo)
{
    int trackId = getCompositionTrackId(compositionId);
    if (trackId != -1) {
        bool res = getTrackById(trackId)->requestCompositionDeletion(compositionId, true, true, undo, redo, true);
        if (!res) {
            undo();
            return false;
        } else {
            Fun unplant_op = [this, compositionId]() {
                unplantComposition(compositionId);
                return true;
            };
            unplant_op();
            PUSH_LAMBDA(unplant_op, redo);
        }
    }
    Fun operation = deregisterComposition_lambda(compositionId);
    auto composition = m_allCompositions[compositionId];
    int new_in = composition->getPosition();
    int new_out = new_in + composition->getPlaytime();
    Fun reverse = [this, composition, compositionId, trackId, new_in, new_out]() {
        // We capture a shared_ptr to the composition, which means that as long as this undo object lives,
        // the composition object is not deleted. To insert it back it is sufficient to register it.
        registerComposition(composition);
        composition->setCurrentTrackId(trackId, true);
        replantCompositions(compositionId, false);
        checkRefresh(new_in, new_out);
        return true;
    };
    if (operation()) {
        Fun update_monitor = [this, new_in, new_out]() {
            checkRefresh(new_in, new_out);
            return true;
        };
        update_monitor();
        PUSH_LAMBDA(update_monitor, operation);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    undo();
    return false;
}

std::unordered_set<int> TimelineModel::getItemsInRange(int trackId, int start, int end, bool listCompositions)
{
    Q_UNUSED(listCompositions)

    std::unordered_set<int> allClips;
    if (isSubtitleTrack(trackId) || trackId == -1) {
        // Subtitles
        if (m_subtitleModel) {
            std::unordered_set<int> subs = m_subtitleModel->getItemsInRange(start, end);
            allClips.insert(subs.begin(), subs.end());
        }
    }
    if (trackId == -1) {
        for (const auto &track : m_allTracks) {
            if (track->isLocked()) {
                continue;
            }
            std::unordered_set<int> clipTracks = getItemsInRange(track->getId(), start, end, listCompositions);
            allClips.insert(clipTracks.begin(), clipTracks.end());
        }
    } else if (trackId >= 0) {
        std::unordered_set<int> clipTracks = getTrackById(trackId)->getClipsInRange(start, end);
        allClips.insert(clipTracks.begin(), clipTracks.end());
        if (listCompositions) {
            std::unordered_set<int> compoTracks = getTrackById(trackId)->getCompositionsInRange(start, end);
            allClips.insert(compoTracks.begin(), compoTracks.end());
        }
    }
    return allClips;
}

bool TimelineModel::requestFakeGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, bool logUndo)
{
    TRACE(clipId, groupId, delta_track, delta_pos, updateView, logUndo);
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = requestFakeGroupMove(clipId, groupId, delta_track, delta_pos, updateView, logUndo, undo, redo);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move group"));
    }
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestFakeGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, bool finalMove, Fun &undo, Fun &redo,
                                         bool allowViewRefresh)
{
    Q_UNUSED(updateView);
    Q_UNUSED(finalMove);
    Q_UNUSED(undo);
    Q_UNUSED(redo);
    Q_UNUSED(allowViewRefresh);
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allGroups.count(groupId) > 0);
    bool ok = true;
    auto all_items = m_groups->getLeaves(groupId);
    Q_ASSERT(all_items.size() > 1);
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };

    // Moving groups is a two stage process: first we remove the clips from the tracks, and then try to insert them back at their calculated new positions.
    // This way, we ensure that no conflict will arise with clips inside the group being moved

    // Check if there is a track move

    // First, remove clips
    bool hasAudio = false;
    bool hasVideo = false;
    std::unordered_map<int, int> old_track_ids, old_position, old_forced_track;
    for (int item : all_items) {
        int old_trackId = getItemTrackId(item);
        old_track_ids[item] = old_trackId;
        if (old_trackId != -1) {
            if (isClip(item)) {
                old_position[item] = m_allClips[item]->getPosition();
                if (!hasAudio && getTrackById_const(old_trackId)->isAudioTrack()) {
                    hasAudio = true;
                } else if (!hasVideo && !getTrackById_const(old_trackId)->isAudioTrack()) {
                    hasVideo = true;
                }
            } else {
                hasVideo = true;
                old_position[item] = m_allCompositions[item]->getPosition();
                old_forced_track[item] = m_allCompositions[item]->getForcedTrack();
            }
        }
    }

    // Second step, calculate delta
    int audio_delta, video_delta;
    audio_delta = video_delta = delta_track;

    if (getTrackById(old_track_ids[clipId])->isAudioTrack()) {
        // Master clip is audio, so reverse delta for video clips
        if (hasAudio) {
            video_delta = -delta_track;
        } else {
            video_delta = 0;
        }
    } else {
        if (hasVideo) {
            audio_delta = -delta_track;
        } else {
            audio_delta = 0;
        }
    }
    bool trackChanged = false;
    if (delta_track != 0) {
        // Ensure the track move is possible (not outside our current tracks)
        for (int item : all_items) {
            int current_track_id = old_track_ids[item];
            int current_track_position = getTrackPosition(current_track_id);
            bool audioTrack = getTrackById_const(current_track_id)->isAudioTrack();
            int d = audioTrack ? audio_delta : video_delta;
            int target_track_position = current_track_position + d;
            bool brokenMove = target_track_position < 0 || target_track_position >= getTracksCount();
            if (!brokenMove) {
                int target_id = getTrackIndexFromPosition(target_track_position);
                brokenMove = audioTrack != getTrackById_const(target_id)->isAudioTrack();
            }
            if (brokenMove) {
                if (isClip(item)) {
                    int lastTid = m_allClips[item]->getFakeTrackId();
                    int originalTid = m_allClips[item]->getCurrentTrackId();
                    int last_position = getTrackPosition(lastTid);
                    int original_position = getTrackPosition(originalTid);
                    int lastDelta = last_position - original_position;
                    if (audioTrack) {
                        if (qAbs(audio_delta) > qAbs(lastDelta)) {
                            audio_delta = lastDelta;
                        }
                        if (video_delta != 0) {
                            video_delta = -lastDelta;
                        }
                    } else {
                        if (qAbs(video_delta) > qAbs(lastDelta)) {
                            video_delta = lastDelta;
                        }
                        if (audio_delta != 0) {
                            audio_delta = -lastDelta;
                        }
                    }
                };
            }
        }
    }

    // Reverse sort. We need to insert from left to right to avoid confusing the view
    for (int item : all_items) {
        int current_track_id = old_track_ids[item];
        int current_track_position = getTrackPosition(current_track_id);
        int d = getTrackById_const(current_track_id)->isAudioTrack() ? audio_delta : video_delta;
        int target_track_position = current_track_position + d;
        if (target_track_position >= 0 && target_track_position < getTracksCount()) {
            auto it = m_allTracks.cbegin();
            std::advance(it, target_track_position);
            int target_track = (*it)->getId();
            int target_position = old_position[item] + delta_pos;
            if (isClip(item)) {
                m_allClips[item]->setFakePosition(target_position);
                if (m_allClips[item]->getFakeTrackId() != target_track) {
                    trackChanged = true;
                }
                m_allClips[item]->setFakeTrackId(target_track);
            } else {
            }
        } else {
            ok = false;
        }
        if (!ok) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    QModelIndex modelIndex;
    QVector<int> roles{FakePositionRole};
    if (trackChanged) {
        roles << FakeTrackIdRole;
    }
    for (int item : all_items) {
        if (isClip(item)) {
            modelIndex = makeClipIndexFromID(item);
        } else {
            modelIndex = makeCompositionIndexFromID(item);
        }
        notifyChange(modelIndex, modelIndex, roles);
    }
    return true;
}

bool TimelineModel::requestGroupMove(int itemId, int groupId, int delta_track, int delta_pos, bool moveMirrorTracks, bool updateView, bool logUndo,
                                     bool revertMove)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId, groupId, delta_track, delta_pos, updateView, logUndo);
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    bool res = requestGroupMove(itemId, groupId, delta_track, delta_pos, updateView, logUndo, undo, redo, revertMove, moveMirrorTracks);
    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move group"));
    }
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestGroupMove(int itemId, int groupId, int delta_track, int delta_pos, bool updateView, bool finalMove, Fun &undo, Fun &redo,
                                     bool revertMove, bool moveMirrorTracks, bool allowViewRefresh, const QVector<int> &allowedTracks)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(m_allGroups.count(groupId) > 0);
    Q_ASSERT(isItem(itemId));
    if (getGroupElements(groupId).count(itemId) == 0) {
        // this group doesn't contain the clip, abort
        return false;
    }
    bool ok = true;
    auto all_items = m_groups->getLeaves(groupId);
    Q_ASSERT(all_items.size() > 1);
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    std::vector<std::pair<int, int>> sorted_clips;
    QVector<int> sorted_clips_ids;
    std::vector<std::pair<int, std::pair<int, int>>> sorted_compositions;
    std::vector<std::pair<int, GenTime>> sorted_subtitles;
    int lowerTrack = -1;
    int upperTrack = -1;
    QVector<int> tracksWithMix;

    // Separate clips from compositions to sort and check source tracks
    QMap<std::pair<int, int>, int> mixesToDelete;
    // Mixes might be deleted while moving clips to another track, so store them before attempting a move
    QMap<int, std::pair<MixInfo, MixInfo>> mixDataArray;
    for (int affectedItemId : all_items) {
        if (delta_track != 0 && !isSubTitle(affectedItemId)) {
            // Check if an upper / lower move is possible
            const int trackPos = getTrackPosition(getItemTrackId(affectedItemId));
            if (lowerTrack == -1 || lowerTrack > trackPos) {
                lowerTrack = trackPos;
            }
            if (upperTrack == -1 || upperTrack < trackPos) {
                upperTrack = trackPos;
            }
        }
        if (isClip(affectedItemId)) {
            sorted_clips.emplace_back(affectedItemId, m_allClips[affectedItemId]->getPosition());
            sorted_clips_ids.push_back(affectedItemId);
            int current_track_id = getClipTrackId(affectedItemId);
            // Check if we have a mix in the group
            if (getTrackById_const(current_track_id)->hasMix(affectedItemId)) {
                std::pair<MixInfo, MixInfo> mixData = getTrackById_const(current_track_id)->getMixInfo(affectedItemId);
                mixDataArray.insert(affectedItemId, mixData);
                if (delta_track != 0) {
                    if (mixData.first.firstClipId > -1 && all_items.find(mixData.first.firstClipId) == all_items.end()) {
                        // First part of the mix is not moving, delete start mix
                        mixesToDelete.insert({mixData.first.firstClipId, affectedItemId}, current_track_id);
                    }
                    if (mixData.second.firstClipId > -1 && all_items.find(mixData.second.secondClipId) == all_items.end()) {
                        // First part of the mix is not moving, delete start mix
                        mixesToDelete.insert({affectedItemId, mixData.second.secondClipId}, current_track_id);
                    }
                } else if (!tracksWithMix.contains(current_track_id)) {
                    // There is a mix, prepare for update
                    tracksWithMix << current_track_id;
                }
            }
        } else if (isComposition(affectedItemId)) {
            sorted_compositions.push_back(
                {affectedItemId, {m_allCompositions[affectedItemId]->getPosition(), getTrackMltIndex(m_allCompositions[affectedItemId]->getCurrentTrackId())}});
        } else if (isSubTitle(affectedItemId)) {
            sorted_subtitles.emplace_back(affectedItemId, m_allSubtitles.at(affectedItemId));
        }
    }

    if (!sorted_subtitles.empty() && m_subtitleModel->isLocked()) {
        // Group with a locked subtitle, abort
        return false;
    }

    // Sort clips first
    std::sort(sorted_clips.begin(), sorted_clips.end(), [delta_pos](const std::pair<int, int> &clipId1, const std::pair<int, int> &clipId2) {
        return delta_pos > 0 ? clipId2.second < clipId1.second : clipId1.second < clipId2.second;
    });

    // Sort subtitles
    std::sort(sorted_subtitles.begin(), sorted_subtitles.end(), [delta_pos](const std::pair<int, GenTime> &clipId1, const std::pair<int, GenTime> &clipId2) {
        return delta_pos > 0 ? clipId2.second < clipId1.second : clipId1.second < clipId2.second;
    });

    // Sort compositions. We need to delete in the move direction from top to bottom
    std::sort(sorted_compositions.begin(), sorted_compositions.end(),
              [delta_track, delta_pos](const std::pair<int, std::pair<int, int>> &clipId1, const std::pair<int, std::pair<int, int>> &clipId2) {
                  const int p1 = delta_track < 0 ? clipId1.second.second : delta_track > 0 ? -clipId1.second.second : clipId1.second.first;
                  const int p2 = delta_track < 0 ? clipId2.second.second : delta_track > 0 ? -clipId2.second.second : clipId2.second.first;
                  return delta_track == 0 ? (delta_pos > 0 ? p2 < p1 : p1 < p2) : p1 < p2;
              });

    // Moving groups is a two stage process: first we remove the clips from the tracks, and then try to insert them back at their calculated new positions.
    // This way, we ensure that no conflict will arise with clips inside the group being moved

    Fun update_model = [this, finalMove]() {
        if (finalMove) {
            updateDuration();
        }
        return true;
    };
    // Check that we don't move subtitles before 0
    if (!sorted_subtitles.empty() && sorted_subtitles.front().second.frames(pCore->getCurrentFps()) + delta_pos < 0) {
        delta_pos = -sorted_subtitles.front().second.frames(pCore->getCurrentFps());
        if (delta_pos == 0) {
            return false;
        }
    }

    // Check if there is a track move
    // Second step, reinsert clips at correct positions
    int audio_delta, video_delta;
    audio_delta = video_delta = delta_track;
    bool masterIsAudio = delta_track != 0 ? getTrackById_const(getItemTrackId(itemId))->isAudioTrack() : false;
    if (delta_track < 0) {
        if (!masterIsAudio) {
            // Case 1, dragging a video clip down
            bool lowerTrackIsAudio = getTrackById_const(getTrackIndexFromPosition(lowerTrack))->isAudioTrack();
            int lowerPos = lowerTrackIsAudio ? lowerTrack - delta_track : lowerTrack + delta_track;
            if (lowerPos < 0) {
                // No space below
                delta_track = 0;
            } else if (!lowerTrackIsAudio) {
                // Moving a group of video clips
                if (getTrackById_const(getTrackIndexFromPosition(lowerPos))->isAudioTrack()) {
                    // Moving to a non matching track (video on audio track)
                    delta_track = 0;
                }
            }
        } else if (lowerTrack + delta_track < 0) {
            // Case 2, dragging an audio clip down
            delta_track = 0;
        }
    } else if (delta_track > 0) {
        if (!masterIsAudio) {
            // Case 1, dragging a video clip up
            int upperPos = upperTrack + delta_track;
            if (upperPos >= getTracksCount()) {
                // Moving above top track, not allowed
                delta_track = 0;
            } else if (getTrackById_const(getTrackIndexFromPosition(upperPos))->isAudioTrack()) {
                // Trying to move to a non matching track (video clip on audio track)
                delta_track = 0;
            }
        } else {
            bool upperTrackIsAudio = getTrackById_const(getTrackIndexFromPosition(upperTrack))->isAudioTrack();
            if (!upperTrackIsAudio) {
                // Dragging an audio clip up, check that upper video clip has an available video track
                int targetPos = upperTrack - delta_track;
                if (moveMirrorTracks && (targetPos < 0 || getTrackById_const(getTrackIndexFromPosition(targetPos))->isAudioTrack())) {
                    delta_track = 0;
                }
            } else {
                int targetPos = upperTrack + delta_track;
                if (targetPos >= getTracksCount() || !getTrackById_const(getTrackIndexFromPosition(targetPos))->isAudioTrack()) {
                    // Trying to drag audio above topmost track or on video track
                    delta_track = 0;
                }
            }
        }
    }
    if (delta_track != 0 && !revertMove) {
        // Ensure destination tracks are empty
        for (const std::pair<int, int> &item : sorted_clips) {
            int currentTrack = getClipTrackId(item.first);
            int trackOffset = delta_track;
            // Adjust delta_track depending on master
            if (getTrackById_const(currentTrack)->isAudioTrack()) {
                if (!masterIsAudio) {
                    trackOffset = -delta_track;
                }
            } else if (masterIsAudio) {
                trackOffset = -delta_track;
            }
            int newTrackPosition = getTrackPosition(currentTrack) + trackOffset;
            if (newTrackPosition < 0 || newTrackPosition >= int(m_allTracks.size())) {
                if (!moveMirrorTracks && item.first != itemId) {
                    continue;
                }
                delta_track = 0;
                break;
            }
            int newItemTrackId = getTrackIndexFromPosition(newTrackPosition);
            int newIn = item.second + delta_pos;
            if (!getTrackById_const(newItemTrackId)->isAvailableWithExceptions(newIn, getClipPlaytime(item.first) - 1, sorted_clips_ids)) {
                if (!moveMirrorTracks && item.first != itemId) {
                    continue;
                }
                delta_track = 0;
                break;
            }
        }
    }
    if (delta_track == 0 && delta_pos == 0) {
        return false;
    }
    bool updateSubtitles = updateView;
    if (delta_track == 0 && updateView) {
        updateView = false;
        allowViewRefresh = false;
        update_model = [sorted_clips, sorted_compositions, finalMove, this]() {
            QModelIndex modelIndex;
            QVector<int> roles{StartRole};
            for (const std::pair<int, int> &item : sorted_clips) {
                modelIndex = makeClipIndexFromID(item.first);
                notifyChange(modelIndex, modelIndex, roles);
            }
            for (const std::pair<int, std::pair<int, int>> &item : sorted_compositions) {
                modelIndex = makeCompositionIndexFromID(item.first);
                notifyChange(modelIndex, modelIndex, roles);
            }
            if (finalMove) {
                updateDuration();
            }
            return true;
        };
    }

    std::unordered_map<int, int> old_track_ids, old_position, old_forced_track;
    QMap<int, int> oldTrackIds;
    // Check for mixes
    for (const std::pair<int, int> &item : sorted_clips) {
        // Keep track of old track for mixes
        oldTrackIds.insert(item.first, getClipTrackId(item.first));
    }
    // First delete mixes that have to
    if (finalMove && !mixesToDelete.isEmpty()) {
        QMapIterator<std::pair<int, int>, int> i(mixesToDelete);
        while (i.hasNext()) {
            i.next();
            // Delete mix
            getTrackById(i.value())->requestRemoveMix(i.key(), local_undo, local_redo);
        }
    }

    // First, remove clips
    if (delta_track != 0) {
        // We delete our clips only if changing track
        for (const std::pair<int, int> &item : sorted_clips) {
            int old_trackId = getClipTrackId(item.first);
            old_track_ids[item.first] = old_trackId;
            if (old_trackId != -1) {
                bool updateThisView = allowViewRefresh;
                ok = ok && getTrackById(old_trackId)->requestClipDeletion(item.first, updateThisView, finalMove, local_undo, local_redo, true, false);
                old_position[item.first] = item.second;
                if (!ok) {
                    bool undone = local_undo();
                    Q_ASSERT(undone);
                    return false;
                }
            }
        }
        for (const std::pair<int, std::pair<int, int>> &item : sorted_compositions) {
            int old_trackId = getCompositionTrackId(item.first);
            if (old_trackId != -1) {
                old_track_ids[item.first] = old_trackId;
                old_position[item.first] = item.second.first;
                old_forced_track[item.first] = m_allCompositions[item.first]->getForcedTrack();
            }
        }
        if (masterIsAudio) {
            // Master clip is audio, so reverse delta for video clips
            video_delta = -delta_track;
        } else {
            audio_delta = -delta_track;
        }
    }

    Fun sync_mix = [this, tracksWithMix, finalMove]() {
        if (!finalMove) {
            return true;
        }
        for (int tid : tracksWithMix) {
            getTrackById_const(tid)->syncronizeMixes(finalMove);
        }
        return true;
    };
    // We need to insert depending on the move direction to avoid confusing the view
    // std::reverse(std::begin(sorted_clips), std::end(sorted_clips));
    bool updateThisView = allowViewRefresh;
    if (delta_track == 0) {
        // Special case, we are moving on same track, avoid too many calculations
        // First pass, check for collisions and suggest better delta
        QVector<int> processedTracks;
        for (const std::pair<int, int> &item : sorted_clips) {
            int current_track_id = getClipTrackId(item.first);
            if (processedTracks.contains(current_track_id)) {
                // We only check the first clip for each track since they are sorted depending on the move direction
                continue;
            }
            processedTracks << current_track_id;
            if (!allowedTracks.isEmpty() && !allowedTracks.contains(current_track_id)) {
                continue;
            }
            int current_in = item.second;
            int playtime = getClipPlaytime(item.first);
            int target_position = current_in + delta_pos;
            int subPlaylist = -1;
            if (delta_pos < 0) {
                if (getTrackById_const(current_track_id)->hasStartMix(item.first)) {
                    subPlaylist = m_allClips[item.first]->getSubPlaylistIndex();
                }
                if (!getTrackById_const(current_track_id)->isAvailable(target_position, qMin(qAbs(delta_pos), playtime), subPlaylist)) {
                    if (!getTrackById_const(current_track_id)->isBlankAt(current_in - 1)) {
                        // No move possible, abort
                        bool undone = local_undo();
                        Q_ASSERT(undone);
                        return false;
                    }
                    int newStart = getTrackById_const(current_track_id)->getBlankStart(current_in - 1, subPlaylist);
                    delta_pos = qMax(delta_pos, newStart - current_in);
                }
            } else {
                int moveEnd = target_position + playtime;
                int moveStart = qMax(current_in + playtime, target_position);
                if (getTrackById_const(current_track_id)->hasEndMix(item.first)) {
                    subPlaylist = m_allClips[item.first]->getSubPlaylistIndex();
                }
                if (!getTrackById_const(current_track_id)->isAvailable(moveStart, moveEnd - moveStart, subPlaylist)) {
                    int newStart = getTrackById_const(current_track_id)->getBlankEnd(current_in + playtime, subPlaylist);
                    if (newStart == current_in + playtime) {
                        // No move possible, abort
                        bool undone = local_undo();
                        Q_ASSERT(undone);
                        return false;
                    }
                    delta_pos = qMin(delta_pos, newStart - (current_in + playtime));
                }
            }
        }
        PUSH_LAMBDA(sync_mix, local_undo);
        for (const std::pair<int, int> &item : sorted_clips) {
            int current_track_id = getClipTrackId(item.first);
            if (!allowedTracks.isEmpty() && !allowedTracks.contains(current_track_id)) {
                continue;
            }
            int current_in = item.second;
            int target_position = current_in + delta_pos;
            ok = requestClipMove(item.first, current_track_id, target_position, moveMirrorTracks, updateThisView, finalMove, finalMove, local_undo, local_redo,
                                 revertMove, true, oldTrackIds,
                                 mixDataArray.contains(item.first) ? mixDataArray.value(item.first) : std::pair<MixInfo, MixInfo>());
            if (!ok) {
                qWarning() << "failed moving clip on track " << current_track_id;
                break;
            }
        }
        if (ok) {
            sync_mix();
            PUSH_LAMBDA(sync_mix, local_redo);
            // Move compositions
            for (const std::pair<int, std::pair<int, int>> &item : sorted_compositions) {
                int current_track_id = getItemTrackId(item.first);
                if (!allowedTracks.isEmpty() && !allowedTracks.contains(current_track_id)) {
                    continue;
                }
                int current_in = item.second.first;
                int target_position = current_in + delta_pos;
                ok = requestCompositionMove(item.first, current_track_id, m_allCompositions[item.first]->getForcedTrack(), target_position, updateThisView,
                                            finalMove, local_undo, local_redo);
                if (!ok) {
                    break;
                }
            }
        }
        if (!ok) {
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    } else {
        // Track changed
        PUSH_LAMBDA(sync_mix, local_undo);
        for (const std::pair<int, int> &item : sorted_clips) {
            int current_track_id = old_track_ids[item.first];
            int current_track_position = getTrackPosition(current_track_id);
            int d = getTrackById(current_track_id)->isAudioTrack() ? audio_delta : video_delta;
            if (!moveMirrorTracks && item.first != itemId) {
                d = 0;
            }
            int target_track_position = current_track_position + d;
            if (target_track_position >= 0 && target_track_position < getTracksCount()) {
                auto it = m_allTracks.cbegin();
                std::advance(it, target_track_position);
                int target_track = (*it)->getId();
                int target_position = old_position[item.first] + delta_pos;
                ok = ok && requestClipMove(item.first, target_track, target_position, moveMirrorTracks, updateThisView, finalMove, finalMove, local_undo,
                                           local_redo, revertMove, true, oldTrackIds,
                                           mixDataArray.contains(item.first) ? mixDataArray.value(item.first) : std::pair<MixInfo, MixInfo>());
            } else {
                ok = false;
            }
            if (!ok) {
                bool undone = local_undo();
                Q_ASSERT(undone);
                return false;
            }
        }
        sync_mix();
        PUSH_LAMBDA(sync_mix, local_redo);
        for (const std::pair<int, std::pair<int, int>> &item : sorted_compositions) {
            int current_track_id = old_track_ids[item.first];
            int current_track_position = getTrackPosition(current_track_id);
            int d = getTrackById(current_track_id)->isAudioTrack() ? audio_delta : video_delta;
            int target_track_position = current_track_position + d;

            if (target_track_position >= 0 && target_track_position < getTracksCount()) {
                auto it = m_allTracks.cbegin();
                std::advance(it, target_track_position);
                int target_track = (*it)->getId();
                int target_position = old_position[item.first] + delta_pos;
                ok = ok && requestCompositionMove(item.first, target_track, old_forced_track[item.first], target_position, updateThisView, finalMove,
                                                  local_undo, local_redo);
            } else {
                qWarning() << "aborting move tried on track" << target_track_position;
                ok = false;
            }
            if (!ok) {
                bool undone = local_undo();
                Q_ASSERT(undone);
                return false;
            }
        }
    }
    // Move subtitles
    if (!sorted_subtitles.empty()) {
        std::vector<std::pair<int, GenTime>>::iterator ptr;
        auto last = std::prev(sorted_subtitles.end());

        for (ptr = sorted_subtitles.begin(); ptr < sorted_subtitles.end(); ptr++) {
            ok = requestSubtitleMove((*ptr).first, (*ptr).second.frames(pCore->getCurrentFps()) + delta_pos, updateSubtitles, ptr == sorted_subtitles.begin(),
                                     ptr == last, finalMove, local_undo, local_redo);
            if (!ok) {
                bool undone = local_undo();
                Q_ASSERT(undone);
                return false;
            }
        }
    }
    update_model();
    PUSH_LAMBDA(update_model, local_redo);
    PUSH_LAMBDA(update_model, local_undo);
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestGroupDeletion(int clipId, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(clipId, logUndo);
    if (!m_groups->isInGroup(clipId)) {
        TRACE_RES(false);
        return false;
    }
    bool res = requestItemDeletion(clipId, logUndo);
    TRACE_RES(res);
    return res;
}

bool TimelineModel::requestGroupDeletion(int clipId, Fun &undo, Fun &redo)
{
    // we do a breadth first exploration of the group tree, ungroup (delete) every inner node, and then delete all the leaves.
    std::queue<int> group_queue;
    group_queue.push(m_groups->getRootId(clipId));
    std::unordered_set<int> all_items;
    std::unordered_set<int> all_compositions;
    // Subtitles MUST BE SORTED BY REVERSE timeline id to preserve the qml model index on undo!!
    std::set<int> all_subtitles;
    while (!group_queue.empty()) {
        int current_group = group_queue.front();
        bool isSelection = m_currentSelection == current_group;

        group_queue.pop();
        Q_ASSERT(isGroup(current_group));
        auto children = m_groups->getDirectChildren(current_group);
        int one_child = -1; // we need the id on any of the indices of the elements of the group
        for (int c : children) {
            if (isClip(c)) {
                all_items.insert(c);
                one_child = c;
            } else if (isComposition(c)) {
                all_compositions.insert(c);
                one_child = c;
            } else if (isSubTitle(c)) {
                all_subtitles.insert(c);
                one_child = c;
            } else {
                Q_ASSERT(isGroup(c));
                one_child = c;
                group_queue.push(c);
            }
        }
        if (one_child != -1) {
            if (m_groups->getType(current_group) == GroupType::Selection) {
                Q_ASSERT(isSelection);
                // in the case of a selection group, we delete the group but don't log it in the undo object
                Fun tmp_undo = []() { return true; };
                Fun tmp_redo = []() { return true; };
                m_groups->ungroupItem(one_child, tmp_undo, tmp_redo);
            } else {
                bool res = m_groups->ungroupItem(one_child, undo, redo);
                if (!res) {
                    undo();
                    return false;
                }
            }
        }
        if (isSelection) {
            requestClearSelection(true);
        }
    }
    for (int clip : all_items) {
        bool res = requestClipDeletion(clip, undo, redo);
        if (!res) {
            // Undo is processed in requestClipDeletion
            return false;
        }
    }
    for (int compo : all_compositions) {
        bool res = requestCompositionDeletion(compo, undo, redo);
        if (!res) {
            undo();
            return false;
        }
    }
    std::set<int>::reverse_iterator rit;
    for (rit = all_subtitles.rbegin(); rit != all_subtitles.rend(); ++rit) {
        bool res = requestSubtitleDeletion(*rit, undo, redo, rit == all_subtitles.rbegin(), rit == std::prev(all_subtitles.rend()));
        if (!res) {
            undo();
            return false;
        }
    }
    return true;
}

const QVariantList TimelineModel::getGroupData(int itemId)
{
    QWriteLocker locker(&m_lock);
    if (!m_groups->isInGroup(itemId)) {
        return {itemId, getItemPosition(itemId), getItemPlaytime(itemId)};
    }
    int groupId = m_groups->getRootId(itemId);
    QVariantList result;
    std::unordered_set<int> items = m_groups->getLeaves(groupId);
    for (int id : items) {
        result << id << getItemPosition(id) << getItemPlaytime(id);
    }
    return result;
}

void TimelineModel::processGroupResize(QVariantList startPosList, QVariantList endPosList, bool right)
{
    Q_ASSERT(startPosList.size() == endPosList.size());
    QMap<int, QPair<int, int>> startData;
    QMap<int, QPair<int, int>> endData;
    while (!startPosList.isEmpty()) {
        int id = startPosList.takeFirst().toInt();
        int in = startPosList.takeFirst().toInt();
        int duration = startPosList.takeFirst().toInt();
        startData.insert(id, {in, duration});
        id = endPosList.takeFirst().toInt();
        in = endPosList.takeFirst().toInt();
        duration = endPosList.takeFirst().toInt();
        endData.insert(id, {in, duration});
    }
    QMapIterator<int, QPair<int, int>> i(startData);
    QList<int> changedItems;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = true;
    QVector<int> mixTracks;
    QList<int> ids = startData.keys();
    for (auto &id : ids) {
        if (isClip(id)) {
            int tid = m_allClips[id]->getCurrentTrackId();
            if (tid > -1 && getTrackById(tid)->hasMix(id)) {
                if (!mixTracks.contains(tid)) {
                    mixTracks << tid;
                }
                if (right) {
                    if (getTrackById_const(tid)->hasEndMix(id)) {
                        std::pair<MixInfo, MixInfo> mixData = getTrackById_const(tid)->getMixInfo(id);
                        QPair<int, int> endPos = endData.value(id);
                        if (endPos.first + endPos.second <= mixData.second.secondClipInOut.first) {
                            Fun sync_mix_undo = [this, tid, mixData]() {
                                getTrackById_const(tid)->createMix(mixData.second, getTrackById_const(tid)->isAudioTrack());
                                return true;
                            };
                            PUSH_LAMBDA(sync_mix_undo, undo);
                        }
                    }
                } else if (getTrackById_const(tid)->hasStartMix(id)) {
                    std::pair<MixInfo, MixInfo> mixData = getTrackById_const(tid)->getMixInfo(id);
                    QPair<int, int> endPos = endData.value(id);
                    if (endPos.first >= mixData.first.firstClipInOut.second) {
                        Fun sync_mix_undo = [this, tid, mixData]() {
                            getTrackById_const(tid)->createMix(mixData.first, getTrackById_const(tid)->isAudioTrack());
                            return true;
                        };
                        PUSH_LAMBDA(sync_mix_undo, undo);
                    }
                }
            }
        }
    }
    Fun sync_mix = []() { return true; };
    if (!mixTracks.isEmpty()) {
        sync_mix = [this, mixTracks]() {
            for (auto &tid : mixTracks) {
                getTrackById_const(tid)->syncronizeMixes(true);
            }
            return true;
        };
        PUSH_LAMBDA(sync_mix, undo);
    }
    while (i.hasNext()) {
        i.next();
        QPair<int, int> startItemPos = i.value();
        QPair<int, int> endItemPos = endData.value(i.key());
        if (startItemPos.first != endItemPos.first || startItemPos.second != endItemPos.second) {
            // Revert individual items to original position
            requestItemResize(i.key(), startItemPos.second, right, false, 0, true);
            changedItems << i.key();
        }
    }
    for (int id : qAsConst(changedItems)) {
        QPair<int, int> endItemPos = endData.value(id);
        int duration = endItemPos.second;
        result = result & requestItemResize(id, duration, right, true, undo, redo, false);
        if (!result) {
            break;
        }
    }
    if (result) {
        sync_mix();
        PUSH_LAMBDA(sync_mix, redo);
        PUSH_UNDO(undo, redo, i18n("Resize group"));
    } else {
        undo();
    }
}

const std::vector<int> TimelineModel::getBoundaries(int itemId)
{
    std::vector<int> boundaries;
    std::unordered_set<int> items;
    if (m_groups->isInGroup(itemId)) {
        int groupId = m_groups->getRootId(itemId);
        items = m_groups->getLeaves(groupId);
    } else {
        items.insert(itemId);
    }
    for (int id : items) {
        if (isItem(id)) {
            int pos = getItemPosition(id);
            boundaries.push_back(pos);
            pos += getItemPlaytime(id);
            boundaries.push_back(pos);
        }
    }
    return boundaries;
}

int TimelineModel::requestClipResizeAndTimeWarp(int itemId, int size, bool right, int snapDistance, bool allowSingleResize, double speed)
{
    Q_UNUSED(snapDistance)
    QWriteLocker locker(&m_lock);
    TRACE(itemId, size, right, true, snapDistance, allowSingleResize);
    Q_ASSERT(isClip(itemId));
    if (size <= 0) {
        TRACE_RES(-1);
        return -1;
    }
    int in = getItemPosition(itemId);
    int out = in + getItemPlaytime(itemId);
    // size = requestItemResizeInfo(itemId, in, out, size, right, snapDistance);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    std::unordered_set<int> all_items;
    if (!allowSingleResize && m_groups->isInGroup(itemId)) {
        int groupId = m_groups->getRootId(itemId);
        std::unordered_set<int> items;
        if (m_groups->getType(groupId) == GroupType::AVSplit) {
            // Only resize group elements if it is an avsplit
            items = m_groups->getLeaves(groupId);
        } else {
            all_items.insert(itemId);
        }
        for (int id : items) {
            if (id == itemId) {
                all_items.insert(id);
                continue;
            }
            int start = getItemPosition(id);
            int end = in + getItemPlaytime(id);
            if (right) {
                if (out == end) {
                    all_items.insert(id);
                }
            } else if (start == in) {
                all_items.insert(id);
            }
        }
    } else {
        all_items.insert(itemId);
    }
    bool result = true;
    for (int id : all_items) {
        int tid = getItemTrackId(id);
        if (tid > -1 && getTrackById_const(tid)->isLocked()) {
            continue;
        }
        // First delete clip, then timewarp, resize and reinsert
        int pos = getItemPosition(id);
        int invalidateIn = pos;
        int invalidateOut = invalidateIn + getClipPlaytime(id);
        if (!right) {
            pos += getItemPlaytime(id) - size;
        }
        bool hasVideo = false;
        if (tid != -1 && !getTrackById_const(tid)->isAudioTrack()) {
            hasVideo = true;
        }
        int trackDuration = getTrackById_const(tid)->trackDuration();
        result = getTrackById(tid)->requestClipDeletion(id, true, false, undo, redo, false, false);
        bool pitchCompensate = m_allClips[id]->getIntProperty(QStringLiteral("warp_pitch"));
        result = result && requestClipTimeWarp(id, speed, pitchCompensate, true, undo, redo);
        result = result && requestItemResize(id, size, true, true, undo, redo);
        result = result && getTrackById(tid)->requestClipInsertion(id, pos, true, false, undo, redo, false, false);
        if (!result) {
            break;
        }
        bool durationChanged = false;
        if (trackDuration != getTrackById_const(tid)->trackDuration()) {
            durationChanged = true;
            getTrackById(tid)->adjustStackLength(trackDuration, getTrackById_const(tid)->trackDuration(), undo, redo);
        }
        if (right) {
            invalidateOut = qMax(invalidateOut, invalidateIn + getClipPlaytime(id));
        } else {
            invalidateIn = qMin(invalidateIn, invalidateOut - getClipPlaytime(id));
        }
        Fun view_redo = [this, invalidateIn, invalidateOut, hasVideo, durationChanged]() {
            if (hasVideo) {
                Q_EMIT invalidateZone(invalidateIn, invalidateOut);
            }
            if (durationChanged) {
                // last clip in playlist updated
                updateDuration();
            }
            return true;
        };
        view_redo();
        PUSH_LAMBDA(view_redo, redo);
        PUSH_LAMBDA(view_redo, undo);
    }
    if (!result) {
        bool undone = undo();
        Q_ASSERT(undone);
    } else {
        PUSH_UNDO(undo, redo, i18n("Resize clip speed"));
    }
    int res = result ? size : -1;
    TRACE_RES(res);
    return res;
}

int TimelineModel::requestItemResizeInfo(int itemId, int in, int out, int size, bool right, int snapDistance)
{
    int trackId = getItemTrackId(itemId);
    bool checkMix = trackId != -1;
    Fun temp_undo = []() { return true; };
    Fun temp_redo = []() { return true; };
    bool skipSnaps = snapDistance <= 0;
    bool sizeUpdated = false;
    if (checkMix && right && size > out - in && isClip(itemId)) {
        int playlist = -1;
        if (getTrackById_const(trackId)->hasEndMix(itemId)) {
            playlist = m_allClips[itemId]->getSubPlaylistIndex();
        }
        int targetPos = in + size - 1;
        if (!getTrackById_const(trackId)->isBlankAt(targetPos, playlist)) {
            int updatedSize = getTrackById_const(trackId)->getBlankEnd(out, playlist) - in + 1;
            if (!skipSnaps && size - updatedSize > snapDistance) {
                skipSnaps = true;
            }
            size = updatedSize;
            sizeUpdated = true;
        }
    } else if (checkMix && !right && size > (out - in) && isClip(itemId)) {
        int targetPos = out - size;
        int playlist = -1;
        if (getTrackById_const(trackId)->hasStartMix(itemId)) {
            playlist = m_allClips[itemId]->getSubPlaylistIndex();
        }
        if (!getTrackById_const(trackId)->isBlankAt(targetPos, playlist)) {
            int updatedSize = out - getTrackById_const(trackId)->getBlankStart(in - 1, playlist);
            if (!skipSnaps && size - updatedSize > snapDistance) {
                skipSnaps = true;
            }
            size = updatedSize;
            sizeUpdated = true;
        }
    }
    int proposed_size = size;
    if (!skipSnaps) {
        int timelinePos = pCore->getMonitorPosition();
        m_snaps->addPoint(timelinePos);
        proposed_size = m_snaps->proposeSize(in, out, getBoundaries(itemId), size, right, snapDistance);
        m_snaps->removePoint(timelinePos);
    }
    if (proposed_size > 0 && (!skipSnaps || sizeUpdated)) {
        // only test move if proposed_size is valid
        bool success = false;
        if (isClip(itemId)) {
            bool hasMix = getTrackById_const(trackId)->hasMix(itemId);
            success = m_allClips[itemId]->requestResize(proposed_size, right, temp_undo, temp_redo, false, hasMix);
        } else if (isComposition(itemId)) {
            success = m_allCompositions[itemId]->requestResize(proposed_size, right, temp_undo, temp_redo, false);
        } else if (isSubTitle(itemId)) {
            // TODO: don't allow subtitle overlap?
            success = true;
        }
        // undo temp move
        temp_undo();
        if (success) {
            size = proposed_size;
        }
    }
    return size;
}

bool TimelineModel::trackIsBlankAt(int tid, int pos, int playlist) const
{
    if (pos > getTrackById_const(tid)->trackDuration() - 1) {
        return true;
    }
    return getTrackById_const(tid)->isBlankAt(pos, playlist);
}

bool TimelineModel::trackIsAvailable(int tid, int pos, int duration, int playlist) const
{
    return getTrackById_const(tid)->isAvailable(pos, duration, playlist);
}

int TimelineModel::getClipStartAt(int tid, int pos, int playlist) const
{
    return getTrackById_const(tid)->getClipStart(pos, playlist);
}

int TimelineModel::getClipEndAt(int tid, int pos, int playlist) const
{
    return getTrackById_const(tid)->getClipEnd(pos, playlist);
}

int TimelineModel::requestItemSpeedChange(int itemId, int size, bool right, int snapDistance)
{
    Q_ASSERT(isClip(itemId));
    QWriteLocker locker(&m_lock);
    TRACE(itemId, size, right, snapDistance);
    Q_ASSERT(isItem(itemId));
    if (size <= 0) {
        TRACE_RES(-1);
        return -1;
    }
    int in = getItemPosition(itemId);
    int out = in + getItemPlaytime(itemId);

    if (right && size > out - in) {
        int targetPos = in + size - 1;
        int trackId = getItemTrackId(itemId);
        if (!getTrackById_const(trackId)->isBlankAt(targetPos) || !getItemsInRange(trackId, out + 1, targetPos, false).empty()) {
            size = getTrackById_const(trackId)->getBlankEnd(out + 1) - in;
        }
    } else if (!right && size > (out - in)) {
        int targetPos = out - size;
        int trackId = getItemTrackId(itemId);
        if (!getTrackById_const(trackId)->isBlankAt(targetPos) || !getItemsInRange(trackId, targetPos, in - 1, false).empty()) {
            size = out - getTrackById_const(trackId)->getBlankStart(in - 1);
        }
    }
    int timelinePos = pCore->getMonitorPosition();
    m_snaps->addPoint(timelinePos);
    int proposed_size = m_snaps->proposeSize(in, out, getBoundaries(itemId), size, right, snapDistance);
    m_snaps->removePoint(timelinePos);
    return proposed_size > 0 ? proposed_size : size;
}

bool TimelineModel::removeMixWithUndo(int cid, Fun &undo, Fun &redo)
{
    int tid = getItemTrackId(cid);
    if (isTrack(tid)) {
        MixInfo mixData = getTrackById_const(tid)->getMixInfo(cid).first;
        if (mixData.firstClipId > -1 && mixData.secondClipId > -1) {
            bool res = getTrackById(tid)->requestRemoveMix({mixData.firstClipId, mixData.secondClipId}, undo, redo);
            return res;
        }
    }
    return true;
}

bool TimelineModel::removeMix(int cid)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = removeMixWithUndo(cid, undo, redo);
    if (res) {
        PUSH_UNDO(undo, redo, i18n("Remove mix"));
    } else {
        pCore->displayMessage(i18n("Removing mix failed"), ErrorMessage, 500);
    }
    return res;
}

int TimelineModel::requestItemResize(int itemId, int size, bool right, bool logUndo, int snapDistance, bool allowSingleResize)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId, size, right, logUndo, snapDistance, allowSingleResize)
    Q_ASSERT(isItem(itemId));
    if (size <= 0) {
        TRACE_RES(-1)
        return -1;
    }
    int in = 0;
    int offset = getItemPlaytime(itemId);
    int tid = getItemTrackId(itemId);
    int out = offset;
    qDebug() << "======= REQUESTING NEW CLIP SIZE: " << size << ", ON TID: " << tid;
    if (tid != -1 || !isClip(itemId)) {
        in = qMax(0, getItemPosition(itemId));
        out += in;
        size = requestItemResizeInfo(itemId, in, out, size, right, snapDistance);
    }
    qDebug() << "======= ADJUSTED NEW CLIP SIZE: " << size << " FROM " << offset;
    offset -= size;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    Fun adjust_mix = []() { return true; };
    Fun sync_end_mix = []() { return true; };
    Fun sync_end_mix_undo = []() { return true; };
    std::unordered_set<int> all_items;
    QList<int> tracksWithMixes;
    all_items.insert(itemId);
    int resizedCount = 0;
    if (logUndo && isClip(itemId)) {
        if (tid > -1) {
            if (right) {
                if (getTrackById_const(tid)->hasEndMix(itemId)) {
                    tracksWithMixes << tid;
                    std::pair<MixInfo, MixInfo> mixData = getTrackById_const(tid)->getMixInfo(itemId);
                    int resizeOut = in + size;
                    int clipMixCut = mixData.second.secondClipInOut.first + m_allClips[mixData.second.secondClipId]->getMixDuration() -
                                     m_allClips[mixData.second.secondClipId]->getMixCutPosition();
                    int clipMixStart = mixData.second.secondClipInOut.first;
                    if (resizeOut < clipMixCut || resizeOut <= clipMixStart) {
                        // Clip resized outside of mix zone, mix will be deleted
                        bool res = removeMixWithUndo(mixData.second.secondClipId, undo, redo);
                        if (res) {
                            size = m_allClips[itemId]->getPlaytime();
                            resizedCount++;
                        } else {
                            return -1;
                        }
                    } else {
                        // Mix was resized, update cut position
                        int currentMixDuration = m_allClips[mixData.second.secondClipId]->getMixDuration();
                        int currentMixCut = m_allClips[mixData.second.secondClipId]->getMixCutPosition();
                        adjust_mix = [this, tid, mixData, currentMixCut, itemId]() {
                            MixInfo secondMixData = getTrackById_const(tid)->getMixInfo(itemId).second;
                            int mixOffset = mixData.second.firstClipInOut.second - secondMixData.firstClipInOut.second;
                            getTrackById_const(tid)->setMixDuration(secondMixData.secondClipId,
                                                                    secondMixData.firstClipInOut.second - secondMixData.secondClipInOut.first,
                                                                    currentMixCut - mixOffset);
                            QModelIndex ix = makeClipIndexFromID(secondMixData.secondClipId);
                            Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                            return true;
                        };
                        Fun adjust_mix_undo = [this, tid, mixData, currentMixCut, currentMixDuration]() {
                            getTrackById_const(tid)->setMixDuration(mixData.second.secondClipId, currentMixDuration, currentMixCut);
                            QModelIndex ix = makeClipIndexFromID(mixData.second.secondClipId);
                            Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                            return true;
                        };
                        PUSH_LAMBDA(adjust_mix_undo, undo);
                    }
                }
                if (getTrackById_const(tid)->hasStartMix(itemId)) {
                    // Resize mix if necessary
                    std::pair<MixInfo, MixInfo> mixData = getTrackById_const(tid)->getMixInfo(itemId);
                    if (in + size <= mixData.first.firstClipInOut.second) {
                        // Resized smaller than mix, adjust
                        int updatedSize = in + size - mixData.first.firstClipInOut.first;
                        // Mix was resized, update cut position
                        int currentMixDuration = m_allClips[itemId]->getMixDuration();
                        int currentMixCut = m_allClips[itemId]->getMixCutPosition();
                        Fun adjust_mix1 = [this, tid, currentMixDuration, currentMixCut, itemId,
                                           mixOffset = mixData.first.firstClipInOut.second - (in + size)]() {
                            getTrackById_const(tid)->setMixDuration(itemId, currentMixDuration - mixOffset, currentMixCut - mixOffset);
                            QModelIndex ix = makeClipIndexFromID(itemId);
                            Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                            return true;
                        };
                        Fun adjust_mix_undo = [this, tid, itemId, currentMixCut, currentMixDuration]() {
                            getTrackById_const(tid)->setMixDuration(itemId, currentMixDuration, currentMixCut);
                            QModelIndex ix = makeClipIndexFromID(itemId);
                            Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                            return true;
                        };
                        PUSH_LAMBDA(adjust_mix1, adjust_mix);
                        PUSH_LAMBDA(adjust_mix_undo, undo);
                        requestItemResize(mixData.first.firstClipId, updatedSize, true, logUndo, undo, redo);
                        resizedCount++;
                    }
                }
            } else {
                // Resized left side
                if (getTrackById_const(tid)->hasStartMix(itemId)) {
                    tracksWithMixes << tid;
                    std::pair<MixInfo, MixInfo> mixData = getTrackById_const(tid)->getMixInfo(itemId);
                    if (out - size >= mixData.first.firstClipInOut.second) {
                        // Moved outside mix, delete
                        bool res = removeMixWithUndo(mixData.first.secondClipId, undo, redo);
                        if (res) {
                            size = m_allClips[itemId]->getPlaytime();
                            resizedCount++;
                        } else {
                            return -1;
                        }
                    } else {
                        // Mix was resized, update cut position
                        int currentMixDuration = m_allClips[mixData.first.secondClipId]->getMixDuration();
                        int currentMixCut = m_allClips[mixData.first.secondClipId]->getMixCutPosition();
                        adjust_mix = [this, tid, currentMixCut, itemId]() {
                            MixInfo firstMixData = getTrackById_const(tid)->getMixInfo(itemId).first;
                            if (firstMixData.firstClipId > -1 && firstMixData.secondClipId > -1) {
                                getTrackById_const(tid)->setMixDuration(firstMixData.secondClipId,
                                                                        firstMixData.firstClipInOut.second - firstMixData.secondClipInOut.first, currentMixCut);
                                QModelIndex ix = makeClipIndexFromID(firstMixData.secondClipId);
                                Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                            }
                            return true;
                        };
                        Fun adjust_mix_undo = [this, tid, mixData, currentMixCut, currentMixDuration]() {
                            getTrackById_const(tid)->setMixDuration(mixData.first.secondClipId, currentMixDuration, currentMixCut);
                            QModelIndex ix = makeClipIndexFromID(mixData.first.secondClipId);
                            Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                            return true;
                        };
                        PUSH_LAMBDA(adjust_mix_undo, undo);
                    }
                }
                if (getTrackById_const(tid)->hasEndMix(itemId)) {
                    // Resize mix if necessary
                    std::pair<MixInfo, MixInfo> mixData = getTrackById_const(tid)->getMixInfo(itemId);
                    if (out - size >= mixData.second.secondClipInOut.first) {
                        // Resized smaller than mix, adjust
                        int updatedClipSize = mixData.second.secondClipInOut.second - (out - size);
                        int updatedMixDuration = mixData.second.firstClipInOut.second - (out - size);
                        // Mix was resized, update cut position
                        int currentMixDuration = m_allClips[mixData.second.secondClipId]->getMixDuration();
                        int currentMixCut = m_allClips[mixData.second.secondClipId]->getMixCutPosition();
                        Fun adjust_mix1 = [this, tid, currentMixCut, secondId = mixData.second.secondClipId, updatedMixDuration]() {
                            getTrackById_const(tid)->setMixDuration(secondId, updatedMixDuration, currentMixCut);
                            QModelIndex ix = makeClipIndexFromID(secondId);
                            Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                            return true;
                        };
                        Fun adjust_mix_undo = [this, tid, secondId = mixData.second.secondClipId, currentMixCut, currentMixDuration]() {
                            getTrackById_const(tid)->setMixDuration(secondId, currentMixDuration, currentMixCut);
                            QModelIndex ix = makeClipIndexFromID(secondId);
                            Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                            return true;
                        };
                        PUSH_LAMBDA(adjust_mix1, adjust_mix);
                        PUSH_LAMBDA(adjust_mix_undo, undo);
                        requestItemResize(mixData.second.secondClipId, updatedClipSize, false, logUndo, undo, redo);
                        resizedCount++;
                    }
                }
            }
        }
    }
    if (!allowSingleResize && m_groups->isInGroup(itemId)) {
        int groupId = m_groups->getRootId(itemId);
        std::unordered_set<int> items = m_groups->getLeaves(groupId);
        /*if (m_groups->getType(groupId) == GroupType::AVSplit) {
            // Only resize group elements if it is an avsplit
            items = m_groups->getLeaves(groupId);
        }*/
        for (int id : items) {
            if (id == itemId) {
                continue;
            }
            int start = getItemPosition(id);
            int end = start + getItemPlaytime(id);
            bool resizeMix = false;
            if (right) {
                if (out == end) {
                    all_items.insert(id);
                    resizeMix = true;
                }
            } else if (start == in) {
                all_items.insert(id);
                resizeMix = true;
            }
            if (logUndo && resizeMix && isClip(id)) {
                int trackId = getItemTrackId(id);
                if (trackId > -1) {
                    if (right) {
                        if (getTrackById_const(trackId)->hasEndMix(id)) {
                            if (!tracksWithMixes.contains(trackId)) {
                                tracksWithMixes << trackId;
                            }
                            std::pair<MixInfo, MixInfo> mixData = getTrackById_const(trackId)->getMixInfo(id);
                            if (end - offset <= mixData.second.secondClipInOut.first + m_allClips[mixData.second.secondClipId]->getMixDuration() -
                                                    m_allClips[mixData.second.secondClipId]->getMixCutPosition()) {
                                // Resized outside mix
                                removeMixWithUndo(mixData.second.secondClipId, undo, redo);
                                resizedCount++;
                            } else {
                                // Mix was resized, update cut position
                                int currentMixDuration = m_allClips[mixData.second.secondClipId]->getMixDuration();
                                int currentMixCut = m_allClips[mixData.second.secondClipId]->getMixCutPosition();
                                Fun adjust_mix2 = [this, trackId, mixData, currentMixCut, id]() {
                                    MixInfo secondMixData = getTrackById_const(trackId)->getMixInfo(id).second;
                                    int offset = mixData.second.firstClipInOut.second - secondMixData.firstClipInOut.second;
                                    getTrackById_const(trackId)->setMixDuration(secondMixData.secondClipId,
                                                                                secondMixData.firstClipInOut.second - secondMixData.secondClipInOut.first,
                                                                                currentMixCut - offset);
                                    QModelIndex ix = makeClipIndexFromID(secondMixData.secondClipId);
                                    Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                                    return true;
                                };
                                Fun adjust_mix_undo = [this, trackId, mixData, currentMixCut, currentMixDuration]() {
                                    getTrackById_const(trackId)->setMixDuration(mixData.second.secondClipId, currentMixDuration, currentMixCut);
                                    QModelIndex ix = makeClipIndexFromID(mixData.second.secondClipId);
                                    Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                                    return true;
                                };
                                PUSH_LAMBDA(adjust_mix2, adjust_mix);
                                PUSH_LAMBDA(adjust_mix_undo, undo);
                            }
                        }
                    } else if (getTrackById_const(trackId)->hasStartMix(id)) {
                        if (!tracksWithMixes.contains(trackId)) {
                            tracksWithMixes << trackId;
                        }
                        std::pair<MixInfo, MixInfo> mixData = getTrackById_const(trackId)->getMixInfo(id);
                        if (start + offset >= mixData.first.firstClipInOut.second) {
                            // Moved outside mix, remove
                            removeMixWithUndo(mixData.first.secondClipId, undo, redo);
                            resizedCount++;
                        }
                    }
                }
            }
        }
    }
    bool result = true;
    int finalPos = right ? in + size : out - size;
    int finalSize;
    for (int id : all_items) {
        int trackId = getItemTrackId(id);
        if (trackId > -1 && getTrackById_const(trackId)->isLocked()) {
            continue;
        }
        if (isSubtitleTrack(trackId) && m_subtitleModel && m_subtitleModel->isLocked()) {
            continue;
        }
        if (right) {
            finalSize = finalPos - qMax(0, getItemPosition(id));
        } else {
            int currentDuration = getItemPlaytime(id);
            if (trackId == -1) {
                finalSize = qMax(0, getItemPosition(id)) + currentDuration - finalPos;
            } else {
                finalSize = qMax(0, getItemPosition(id)) + currentDuration - qMax(0, finalPos);
                if (finalSize == currentDuration) {
                    continue;
                }
            }
        }
        result = result && requestItemResize(id, finalSize, right, logUndo, undo, redo);
        if (!result) {
            break;
        }
        if (id == itemId) {
            size = finalSize;
        }
        resizedCount++;
    }
    result = result && resizedCount != 0;
    if (!result) {
        qDebug() << "resize aborted" << result;
        bool undone = undo();
        Q_ASSERT(undone);
    } else if (logUndo) {
        if (isClip(itemId)) {
            adjust_mix();
            PUSH_LAMBDA(adjust_mix, redo);
            PUSH_UNDO(undo, redo, i18n("Resize clip"))
        } else if (isComposition(itemId)) {
            PUSH_UNDO(undo, redo, i18n("Resize composition"))
        } else if (isSubTitle(itemId)) {
            PUSH_UNDO(undo, redo, i18n("Resize subtitle"))
        }
    }
    int res = result ? size : -1;
    TRACE_RES(res)
    return res;
}

bool TimelineModel::requestItemResize(int itemId, int &size, bool right, bool logUndo, Fun &undo, Fun &redo, bool blockUndo)
{
    Q_UNUSED(blockUndo)
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    bool result = false;
    if (isClip(itemId)) {
        bool hasMix = false;
        int tid = m_allClips[itemId]->getCurrentTrackId();
        if (tid > -1) {
            std::pair<MixInfo, MixInfo> mixData = getTrackById_const(tid)->getMixInfo(itemId);
            if (right && mixData.second.firstClipId > -1) {
                hasMix = true;
                if (mixData.second.firstClipInOut.first + size < mixData.second.secondClipInOut.first) {
                    // Resize is outside mix zone, remove mix
                    removeMixWithUndo(mixData.second.secondClipId, undo, redo);
                } else {
                    size = qMin(size, mixData.second.secondClipInOut.second - mixData.second.firstClipInOut.first);
                }
            } else if (!right && mixData.first.firstClipId > -1) {
                hasMix = true;
                // We have a mix at clip start, limit size to previous clip start
                size = qMin(size, mixData.first.secondClipInOut.second - mixData.first.firstClipInOut.first);
                int currentMixDuration = mixData.first.firstClipInOut.second - mixData.first.secondClipInOut.first;
                int mixDuration = mixData.first.firstClipInOut.second - (mixData.first.secondClipInOut.second - size);
                Fun local_update = [this, itemId, tid, mixData, mixDuration] {
                    getTrackById_const(tid)->setMixDuration(itemId, qMax(1, mixDuration), mixData.first.mixOffset);
                    QModelIndex ix = makeClipIndexFromID(itemId);
                    Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                    return true;
                };
                Fun local_update_undo = [this, itemId, tid, mixData, currentMixDuration] {
                    if (getTrackById_const(tid)->hasStartMix(itemId)) {
                        getTrackById_const(tid)->setMixDuration(itemId, currentMixDuration, mixData.first.mixOffset);
                        QModelIndex ix = makeClipIndexFromID(itemId);
                        Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                    }
                    return true;
                };
                local_update();
                if (logUndo) {
                    UPDATE_UNDO_REDO(local_update, local_update_undo, local_undo, local_redo);
                }
            } else {
                hasMix = mixData.second.firstClipId > -1 || mixData.first.firstClipId > -1;
            }
        }
        result = m_allClips[itemId]->requestResize(size, right, local_undo, local_redo, logUndo, hasMix);
    } else if (isComposition(itemId)) {
        result = m_allCompositions[itemId]->requestResize(size, right, local_undo, local_redo, logUndo);
    } else if (isSubTitle(itemId)) {
        result = m_subtitleModel->requestResize(itemId, size, right, local_undo, local_redo, logUndo);
    }
    if (result) {
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    }
    return result;
}

int TimelineModel::requestItemRippleResize(const std::shared_ptr<TimelineItemModel> &timeline, int itemId, int size, bool right, bool logUndo, bool moveGuides,
                                           int snapDistance, bool allowSingleResize)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId, size, right, logUndo, snapDistance, allowSingleResize)
    Q_ASSERT(isItem(itemId));
    if (size <= 0) {
        TRACE_RES(-1)
        return -1;
    }
    int in = 0;
    int offset = getItemPlaytime(itemId);
    int tid = getItemTrackId(itemId);
    int out = offset;
    qDebug() << "======= REQUESTING NEW CLIP SIZE (RIPPLE): " << size;
    if (tid != -1 || !isClip(itemId)) {
        in = qMax(0, getItemPosition(itemId));
        out += in;
        // m_snaps->addPoint(cursorPos);
        int proposed_size = m_snaps->proposeSize(in, out, getBoundaries(itemId), size, true, snapDistance);
        // m_snaps->removePoint(cursorPos);
        if (proposed_size > -1) {
            size = proposed_size;
        }
    }
    qDebug() << "======= ADJUSTED NEW CLIP SIZE (RIPPLE): " << size;
    offset -= size;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    Fun sync_mix = []() { return true; };
    Fun adjust_mix = []() { return true; };
    Fun sync_end_mix = []() { return true; };
    Fun sync_end_mix_undo = []() { return true; };
    PUSH_LAMBDA(sync_mix, undo);
    std::unordered_set<int> all_items;
    QList<int> tracksWithMixes;
    all_items.insert(itemId);
    if (logUndo && isClip(itemId)) {
        /*if (tid > -1) {
            if (right) {
                if (getTrackById_const(tid)->hasEndMix(itemId)) {
                    tracksWithMixes << tid;
                    std::pair<MixInfo, MixInfo> mixData = getTrackById_const(tid)->getMixInfo(itemId);
                    if (in + size <= mixData.second.secondClipInOut.first + m_allClips[mixData.second.secondClipId]->getMixDuration() -
        m_allClips[mixData.second.secondClipId]->getMixCutPosition()) {
                        // Clip resized outside of mix zone, mix will be deleted
                        bool res = removeMixWithUndo(mixData.second.secondClipId, undo, redo);
                        if (res) {
                            size = m_allClips[itemId]->getPlaytime();
                        } else {
                            return -1;
                        }
                    } else {
                        // Mix was resized, update cut position
                        int currentMixDuration = m_allClips[mixData.second.secondClipId]->getMixDuration();
                        int currentMixCut = m_allClips[mixData.second.secondClipId]->getMixCutPosition();
                        adjust_mix = [this, tid, mixData, currentMixCut, itemId]() {
                            MixInfo secondMixData = getTrackById_const(tid)->getMixInfo(itemId).second;
                            int offset = mixData.second.firstClipInOut.second - secondMixData.firstClipInOut.second;
                            getTrackById_const(tid)->setMixDuration(secondMixData.secondClipId, secondMixData.firstClipInOut.second -
        secondMixData.secondClipInOut.first, currentMixCut - offset); QModelIndex ix = makeClipIndexFromID(secondMixData.secondClipId); Q_EMIT dataChanged(ix,
        ix, {TimelineModel::MixRole,TimelineModel::MixCutRole}); return true;
                        };
                        Fun adjust_mix_undo = [this, tid, mixData, currentMixCut, currentMixDuration]() {
                            getTrackById_const(tid)->setMixDuration(mixData.second.secondClipId, currentMixDuration, currentMixCut);
                            QModelIndex ix = makeClipIndexFromID(mixData.second.secondClipId);
                            Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole,TimelineModel::MixCutRole});
                            return true;
                        };
                        PUSH_LAMBDA(adjust_mix_undo, undo);
                    }
                }
            } else if (getTrackById_const(tid)->hasStartMix(itemId)) {
                tracksWithMixes << tid;
                std::pair<MixInfo, MixInfo> mixData = getTrackById_const(tid)->getMixInfo(itemId);
                if (out - size >= mixData.first.firstClipInOut.second) {
                    // Moved outside mix, delete
                    Fun sync_mix_undo = [this, tid, mixData]() {
                        getTrackById_const(tid)->createMix(mixData.first, getTrackById_const(tid)->isAudioTrack());
                        getTrackById_const(tid)->syncronizeMixes(true);
                        return true;
                    };
                    bool switchPlaylist = getTrackById_const(tid)->hasEndMix(itemId) == false && m_allClips[itemId]->getSubPlaylistIndex() == 1;
                    if (switchPlaylist) {
                        sync_end_mix = [this, tid, mixData]() {
                            return getTrackById_const(tid)->switchPlaylist(mixData.first.secondClipId, m_allClips[mixData.first.secondClipId]->getPosition(), 1,
        0);
                        };
                        sync_end_mix_undo = [this, tid, mixData]() {
                            return getTrackById_const(tid)->switchPlaylist(mixData.first.secondClipId, m_allClips[mixData.first.secondClipId]->getPosition(), 0,
        1);
                        };
                    }
                    PUSH_LAMBDA(sync_mix_undo, undo);

                }
            }
        }*/
    }
    if (!allowSingleResize && m_groups->isInGroup(itemId)) {
        int groupId = m_groups->getRootId(itemId);
        std::unordered_set<int> items = m_groups->getLeaves(groupId);
        /*if (m_groups->getType(groupId) == GroupType::AVSplit) {
            // Only resize group elements if it is an avsplit
            items = m_groups->getLeaves(groupId);
        }*/
        for (int id : items) {
            if (id == itemId) {
                continue;
            }
            int start = getItemPosition(id);
            int end = start + getItemPlaytime(id);
            bool resizeMix = false;
            if (right) {
                if (out == end) {
                    all_items.insert(id);
                    resizeMix = true;
                }
            } else if (start == in) {
                all_items.insert(id);
                resizeMix = true;
            }
            if (logUndo && resizeMix && isClip(id)) {
                int trackId = getItemTrackId(id);
                if (trackId > -1) {
                    if (right) {
                        if (getTrackById_const(trackId)->hasEndMix(id)) {
                            if (!tracksWithMixes.contains(trackId)) {
                                tracksWithMixes << trackId;
                            }
                            std::pair<MixInfo, MixInfo> mixData = getTrackById_const(trackId)->getMixInfo(id);
                            if (end - offset <= mixData.second.secondClipInOut.first + m_allClips[mixData.second.secondClipId]->getMixDuration() -
                                                    m_allClips[mixData.second.secondClipId]->getMixCutPosition()) {
                                // Resized outside mix
                                removeMixWithUndo(mixData.second.secondClipId, undo, redo);
                                Fun sync_mix_undo = [this, trackId, mixData]() {
                                    getTrackById_const(trackId)->createMix(mixData.second, getTrackById_const(trackId)->isAudioTrack());
                                    getTrackById_const(trackId)->syncronizeMixes(true);
                                    return true;
                                };
                                bool switchPlaylist = getTrackById_const(trackId)->hasEndMix(mixData.second.secondClipId) == false &&
                                                      m_allClips[mixData.second.secondClipId]->getSubPlaylistIndex() == 1;
                                if (switchPlaylist) {
                                    Fun sync_end_mix2 = [this, trackId, mixData]() {
                                        return getTrackById_const(trackId)->switchPlaylist(mixData.second.secondClipId, mixData.second.secondClipInOut.first, 1,
                                                                                           0);
                                    };
                                    Fun sync_end_mix_undo2 = [this, trackId, mixData]() {
                                        return getTrackById_const(trackId)->switchPlaylist(mixData.second.secondClipId,
                                                                                           m_allClips[mixData.second.secondClipId]->getPosition(), 0, 1);
                                    };
                                    PUSH_LAMBDA(sync_end_mix2, sync_end_mix);
                                    PUSH_LAMBDA(sync_end_mix_undo2, sync_end_mix_undo);
                                }
                                PUSH_LAMBDA(sync_mix_undo, undo);
                            } else {
                                // Mix was resized, update cut position
                                int currentMixDuration = m_allClips[mixData.second.secondClipId]->getMixDuration();
                                int currentMixCut = m_allClips[mixData.second.secondClipId]->getMixCutPosition();
                                Fun adjust_mix2 = [this, trackId, mixData, currentMixCut, id]() {
                                    MixInfo secondMixData = getTrackById_const(trackId)->getMixInfo(id).second;
                                    int offset = mixData.second.firstClipInOut.second - secondMixData.firstClipInOut.second;
                                    getTrackById_const(trackId)->setMixDuration(secondMixData.secondClipId,
                                                                                secondMixData.firstClipInOut.second - secondMixData.secondClipInOut.first,
                                                                                currentMixCut - offset);
                                    QModelIndex ix = makeClipIndexFromID(secondMixData.secondClipId);
                                    Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                                    return true;
                                };
                                Fun adjust_mix_undo = [this, trackId, mixData, currentMixCut, currentMixDuration]() {
                                    getTrackById_const(trackId)->setMixDuration(mixData.second.secondClipId, currentMixDuration, currentMixCut);
                                    QModelIndex ix = makeClipIndexFromID(mixData.second.secondClipId);
                                    Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                                    return true;
                                };
                                PUSH_LAMBDA(adjust_mix2, adjust_mix);
                                PUSH_LAMBDA(adjust_mix_undo, undo);
                            }
                        }
                    } else if (getTrackById_const(trackId)->hasStartMix(id)) {
                        if (!tracksWithMixes.contains(trackId)) {
                            tracksWithMixes << trackId;
                        }
                        std::pair<MixInfo, MixInfo> mixData = getTrackById_const(trackId)->getMixInfo(id);
                        if (start + offset >= mixData.first.firstClipInOut.second) {
                            // Moved outside mix, remove
                            Fun sync_mix_undo = [this, trackId, mixData]() {
                                getTrackById_const(trackId)->createMix(mixData.first, getTrackById_const(trackId)->isAudioTrack());
                                getTrackById_const(trackId)->syncronizeMixes(true);
                                return true;
                            };
                            bool switchPlaylist = getTrackById_const(trackId)->hasEndMix(id) == false && m_allClips[id]->getSubPlaylistIndex() == 1;
                            if (switchPlaylist) {
                                Fun sync_end_mix2 = [this, trackId, mixData]() {
                                    return getTrackById_const(trackId)->switchPlaylist(mixData.first.secondClipId,
                                                                                       m_allClips[mixData.first.secondClipId]->getPosition(), 1, 0);
                                };
                                Fun sync_end_mix_undo2 = [this, trackId, mixData]() {
                                    return getTrackById_const(trackId)->switchPlaylist(mixData.first.secondClipId,
                                                                                       m_allClips[mixData.first.secondClipId]->getPosition(), 0, 1);
                                };
                                PUSH_LAMBDA(sync_end_mix2, sync_end_mix);
                                PUSH_LAMBDA(sync_end_mix_undo2, sync_end_mix_undo);
                            }
                            PUSH_LAMBDA(sync_mix_undo, undo);
                        }
                    }
                }
            }
        }
    }
    if (logUndo && !tracksWithMixes.isEmpty()) {
        sync_mix = [this, tracksWithMixes]() {
            for (auto &t : tracksWithMixes) {
                getTrackById_const(t)->syncronizeMixes(true);
            }
            return true;
        };
    }
    bool result = true;
    int finalPos = right ? in + size : out - size;
    int finalSize;
    int resizedCount = 0;
    for (int id : all_items) {
        int trackId = getItemTrackId(id);
        if (trackId > -1 && getTrackById_const(trackId)->isLocked()) {
            continue;
        }
        if (isSubtitleTrack(trackId) && m_subtitleModel && m_subtitleModel->isLocked()) {
            continue;
        }
        if (right) {
            finalSize = finalPos - qMax(0, getItemPosition(id));
        } else {
            finalSize = qMax(0, getItemPosition(id)) + getItemPlaytime(id) - finalPos;
        }
        result = result && requestItemRippleResize(timeline, id, finalSize, right, logUndo, moveGuides, undo, redo);
        resizedCount++;
    }
    result = result && resizedCount != 0;
    if (!result) {
        qDebug() << "resize aborted" << result;
        bool undone = undo();
        Q_ASSERT(undone);
    } else if (logUndo) {
        if (isClip(itemId)) {
            sync_end_mix();
            sync_mix();
            adjust_mix();
            PUSH_LAMBDA(sync_end_mix, redo);
            PUSH_LAMBDA(adjust_mix, redo);
            PUSH_LAMBDA(sync_mix, redo);
            PUSH_LAMBDA(undo, sync_end_mix_undo);
            PUSH_UNDO(sync_end_mix_undo, redo, i18n("Ripple resize clip"))
        } else if (isComposition(itemId)) {
            PUSH_UNDO(undo, redo, i18n("Ripple resize composition"))
        } else if (isSubTitle(itemId)) {
            PUSH_UNDO(undo, redo, i18n("Ripple resize subtitle"))
        }
    }
    int res = result ? size : -1;
    TRACE_RES(res)
    return res;
}

bool TimelineModel::requestItemRippleResize(const std::shared_ptr<TimelineItemModel> &timeline, int itemId, int size, bool right, bool logUndo, bool moveGuides,
                                            Fun &undo, Fun &redo, bool blockUndo)
{
    Q_UNUSED(blockUndo)
    Q_UNUSED(moveGuides)
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    bool result = false;
    if (isClip(itemId)) {
        bool hasMix = false;
        int tid = m_allClips[itemId]->getCurrentTrackId();
        if (tid > -1) {
            /*std::pair<MixInfo, MixInfo> mixData = getTrackById_const(tid)->getMixInfo(itemId);
            if (right && mixData.second.firstClipId > -1) {
                hasMix = true;
                size = qMin(size, mixData.second.secondClipInOut.second - mixData.second.firstClipInOut.first);
            } else if (!right && mixData.first.firstClipId > -1) {
                hasMix = true;
                // We have a mix at clip start, limit size to previous clip start
                size = qMin(size, mixData.first.secondClipInOut.second - mixData.first.firstClipInOut.first);
                int currentMixDuration = mixData.first.firstClipInOut.second - mixData.first.secondClipInOut.first;
                int mixDuration = mixData.first.firstClipInOut.second - (mixData.first.secondClipInOut.second - size);
                Fun local_update = [this, itemId, tid, mixData, mixDuration] {
                    getTrackById_const(tid)->setMixDuration(itemId, qMax(1, mixDuration), mixData.first.mixOffset);
                    QModelIndex ix = makeClipIndexFromID(itemId);
                    Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole,TimelineModel::MixCutRole});
                    return true;
                };
                Fun local_update_undo = [this, itemId, tid, mixData, currentMixDuration] {
                    getTrackById_const(tid)->setMixDuration(itemId, currentMixDuration, mixData.first.mixOffset);
                    QModelIndex ix = makeClipIndexFromID(itemId);
                    Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole,TimelineModel::MixCutRole});
                    return true;
                };
                local_update();
                if (logUndo) {
                    UPDATE_UNDO_REDO(local_update, local_update_undo, local_undo, local_redo);
                }
            } else {
                hasMix = mixData.second.firstClipId > -1 || mixData.first.firstClipId > -1;
            }*/
        }
        bool affectAllTracks = false;
        size = m_allClips[itemId]->getMaxDuration() > 0 ? qBound(1, size, m_allClips[itemId]->getMaxDuration()) : qMax(1, size);
        int delta = size - m_allClips[itemId]->getPlaytime();
        qDebug() << "requestItemRippleResize logUndo: " << logUndo << " size: " << size << " playtime: " << m_allClips[itemId]->getPlaytime()
                 << " delta: " << delta;
        auto spacerOperation = [this, itemId, affectAllTracks, &local_undo, &local_redo, delta, right, timeline](int position) {
            int trackId = getItemTrackId(itemId);
            if (right && getTrackById_const(trackId)->isLastClip(getItemPosition(itemId))) {
                return true;
            }
            std::pair<int, int> spacerOp = TimelineFunctions::requestSpacerStartOperation(timeline, affectAllTracks ? -1 : trackId, position + 1, true, true);
            int cid = spacerOp.first;
            if (cid == -1) {
                return false;
            }
            int endPos = getItemPosition(cid) + delta;
            // Start undoable command
            TimelineFunctions::requestSpacerEndOperation(timeline, cid, getItemPosition(cid), endPos, affectAllTracks ? -1 : trackId,
                                                         KdenliveSettings::lockedGuides() ? -1 : position, local_undo, local_redo, false);
            return true;
        };
        if (delta > 0) {
            if (right) {
                int position = getItemPosition(itemId) + getItemPlaytime(itemId);
                if (!spacerOperation(position)) {
                    return false;
                }
            } else {
                int position = getItemPosition(itemId);
                if (!spacerOperation(position)) {
                    return false;
                }
            }
        }

        result = m_allClips[itemId]->requestResize(size, right, local_undo, local_redo, logUndo, hasMix);
        if (!result && delta > 0) {
            local_undo();
        }
        if (result && delta < 0) {
            if (right) {
                int position = getItemPosition(itemId) + getItemPlaytime(itemId) - delta;
                if (!spacerOperation(position)) {
                    return false;
                }
            } else {
                int position = getItemPosition(itemId) + delta;
                if (!spacerOperation(position)) {
                    return false;
                }
            }
        }

    } else if (isComposition(itemId)) {
        return false;
        // TODO? Does it make sense?
        // result = m_allCompositions[itemId]->requestResize(size, right, local_undo, local_redo, logUndo);
    } else if (isSubTitle(itemId)) {
        return false;
        // TODO?
        // result = m_subtitleModel->requestResize(itemId, size, right, local_undo, local_redo, logUndo);
    }
    if (result) {
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    }
    return result;
}

int TimelineModel::requestSlipSelection(int offset, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(offset, logUndo)

    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = true;
    int slipCount = 0;
    for (auto id : getCurrentSelection()) {
        int tid = getItemTrackId(id);
        if (tid > -1 && getTrackById_const(tid)->isLocked()) {
            continue;
        }
        if (!isClip(id)) {
            continue;
        }
        result = result && requestClipSlip(id, offset, logUndo, undo, redo);
        slipCount++;
    }
    result = result && slipCount != 0;
    if (!result) {
        bool undone = undo();
        Q_ASSERT(undone);
    } else if (logUndo) {
        PUSH_UNDO(undo, redo, i18ncp("Undo/Redo menu text", "Slip clip", "Slip clips", slipCount));
    }
    int res = result ? offset : 0;
    TRACE_RES(res)
    return res;
}

int TimelineModel::requestClipSlip(int itemId, int offset, bool logUndo, bool allowSingleResize)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId, offset, logUndo, allowSingleResize)
    Q_ASSERT(isClip(itemId));
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    std::unordered_set<int> all_items;
    all_items.insert(itemId);
    if (!allowSingleResize && m_groups->isInGroup(itemId)) {
        int groupId = m_groups->getRootId(itemId);
        all_items = m_groups->getLeaves(groupId);
    }
    bool result = true;
    int slipCount = 0;
    for (int id : all_items) {
        int tid = getItemTrackId(id);
        if (tid > -1 && getTrackById_const(tid)->isLocked()) {
            continue;
        }
        result = result && requestClipSlip(id, offset, logUndo, undo, redo);
        slipCount++;
    }
    result = result && slipCount != 0;
    if (!result) {
        bool undone = undo();
        Q_ASSERT(undone);
    } else if (logUndo) {
        PUSH_UNDO(undo, redo, i18n("Slip clip"))
    }
    int res = result ? offset : 0;
    TRACE_RES(res)
    return res;
}

bool TimelineModel::requestClipSlip(int itemId, int offset, bool logUndo, Fun &undo, Fun &redo, bool blockUndo)
{
    Q_UNUSED(blockUndo)
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    bool result = false;
    if (isClip(itemId)) {
        result = m_allClips[itemId]->requestSlip(offset, local_undo, local_redo, logUndo);
    }
    if (result) {
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    }
    return result;
}

int TimelineModel::requestClipsGroup(const std::unordered_set<int> &ids, bool logUndo, GroupType type)
{
    QWriteLocker locker(&m_lock);
    TRACE(ids, logUndo, type);
    if (type == GroupType::Selection || type == GroupType::Leaf) {
        // Selections shouldn't be done here. Call requestSetSelection instead
        TRACE_RES(-1);
        return -1;
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    int result = requestClipsGroup(ids, undo, redo, type);
    if (result > -1 && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Group clips"));
    }
    TRACE_RES(result);
    return result;
}

int TimelineModel::requestClipsGroup(const std::unordered_set<int> &ids, Fun &undo, Fun &redo, GroupType type)
{
    QWriteLocker locker(&m_lock);
    if (type != GroupType::Selection) {
        requestClearSelection();
    }
    int clipsCount = 0;
    QList<int> tracks;
    for (int id : ids) {
        if (isClip(id)) {
            int trackId = getClipTrackId(id);
            if (trackId == -1) {
                return -1;
            }
            tracks << trackId;
            clipsCount++;
        } else if (isComposition(id)) {
            if (getCompositionTrackId(id) == -1) {
                return -1;
            }
        } else if (isSubTitle(id)) {
        } else if (!isGroup(id)) {
            return -1;
        }
    }
    if (type == GroupType::Selection && ids.size() == 1) {
        // only one element selected, no group created
        return -1;
    }
    if (ids.size() == 2 && clipsCount == 2 && type == GroupType::Normal) {
        // Check if we are grouping an AVSplit
        auto it = ids.begin();
        int firstId = *it;
        std::advance(it, 1);
        int secondId = *it;
        bool isAVGroup = false;
        if (getClipBinId(firstId) == getClipBinId(secondId)) {
            if (getClipState(firstId) == PlaylistState::AudioOnly) {
                if (getClipState(secondId) == PlaylistState::VideoOnly) {
                    isAVGroup = true;
                }
            } else if (getClipState(secondId) == PlaylistState::AudioOnly) {
                isAVGroup = true;
            }
        }
        if (isAVGroup) {
            type = GroupType::AVSplit;
        }
    }
    int groupId = m_groups->groupItems(ids, undo, redo, type);
    if (type != GroupType::Selection) {
        // we make sure that the undo and the redo are going to unselect before doing anything else
        Fun unselect = [this]() { return requestClearSelection(); };
        PUSH_FRONT_LAMBDA(unselect, undo);
        PUSH_FRONT_LAMBDA(unselect, redo);
    }
    return groupId;
}

bool TimelineModel::requestClipsUngroup(const std::unordered_set<int> &itemIds, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemIds, logUndo);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = true;
    requestClearSelection();
    std::unordered_set<int> roots;
    std::transform(itemIds.begin(), itemIds.end(), std::inserter(roots, roots.begin()), [&](int id) { return m_groups->getRootId(id); });
    for (int root : roots) {
        if (isGroup(root)) {
            result = result && requestClipUngroup(root, undo, redo);
        }
    }
    if (!result) {
        bool undone = undo();
        Q_ASSERT(undone);
    }
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Ungroup clips"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestClipUngroup(int itemId, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId, logUndo);
    requestClearSelection();
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = true;
    result = requestClipUngroup(itemId, undo, redo);
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Ungroup clips"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestClipUngroup(int itemId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    bool isSelection = m_groups->getType(m_groups->getRootId(itemId)) == GroupType::Selection;
    if (!isSelection) {
        requestClearSelection();
    }
    bool res = m_groups->ungroupItem(itemId, undo, redo);
    if (res && !isSelection) {
        // we make sure that the undo and the redo are going to unselect before doing anything else
        Fun unselect = [this]() { return requestClearSelection(); };
        PUSH_FRONT_LAMBDA(unselect, undo);
        PUSH_FRONT_LAMBDA(unselect, redo);
    }
    return res;
}

bool TimelineModel::requestTrackInsertion(int position, int &id, const QString &trackName, bool audioTrack)
{
    QWriteLocker locker(&m_lock);
    TRACE(position, id, trackName, audioTrack);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestTrackInsertion(position, id, trackName, audioTrack, undo, redo);
    if (result) {
        PUSH_UNDO(undo, redo, i18nc("@action", "Insert Track"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestTrackInsertion(int position, int &id, const QString &trackName, bool audioTrack, Fun &undo, Fun &redo, bool addCompositing)
{
    // TODO: make sure we disable overlayTrack before inserting a track
    if (position == -1) {
        position = int(m_allTracks.size());
    }
    if (position < 0 || position > int(m_allTracks.size())) {
        return false;
    }
    int previousId = -1;
    if (position < int(m_allTracks.size())) {
        previousId = getTrackIndexFromPosition(position);
    }
    int trackId = TimelineModel::getNextId();
    id = trackId;
    Fun local_undo = deregisterTrack_lambda(trackId);
    TrackModel::construct(shared_from_this(), trackId, position, trackName, audioTrack, addCompositing);
    // Adjust compositions that were affecting track at previous pos
    QList<std::shared_ptr<CompositionModel>> updatedCompositions;
    if (previousId > -1) {
        for (auto &compo : m_allCompositions) {
            if (position > 0 && compo.second->getATrack() == position && compo.second->getForcedTrack() == -1) {
                updatedCompositions << compo.second;
            }
        }
    }
    Fun local_update = [position, updatedCompositions]() {
        for (auto &compo : updatedCompositions) {
            compo->setATrack(position + 1, -1);
        }
        return true;
    };
    Fun local_update_undo = [position, updatedCompositions]() {
        for (auto &compo : updatedCompositions) {
            compo->setATrack(position, -1);
        }
        return true;
    };

    Fun local_name_update = [position, audioTrack, this]() {
        if (KdenliveSettings::audiotracksbelow() == 0) {
            _resetView();
        } else {
            if (audioTrack) {
                for (int i = 0; i <= position && i < int(m_allTracks.size()); i++) {
                    QModelIndex ix = makeTrackIndexFromID(getTrackIndexFromPosition(i));
                    Q_EMIT dataChanged(ix, ix, {TimelineModel::TrackTagRole});
                }
            } else {
                for (int i = position; i < int(m_allTracks.size()); i++) {
                    QModelIndex ix = makeTrackIndexFromID(getTrackIndexFromPosition(i));
                    Q_EMIT dataChanged(ix, ix, {TimelineModel::TrackTagRole});
                }
            }
        }
        return true;
    };

    local_update();
    local_name_update();
    Fun rebuild_compositing = [this]() {
        buildTrackCompositing(true);
        return true;
    };
    if (addCompositing) {
        buildTrackCompositing(true);
    }
    auto track = getTrackById(trackId);
    Fun local_redo = [track, position, local_update, addCompositing, this]() {
        // We capture a shared_ptr to the track, which means that as long as this undo object lives, the track object is not deleted. To insert it back it is
        // sufficient to register it.
        registerTrack(track, position, true);
        local_update();
        if (addCompositing) {
            buildTrackCompositing(true);
        }
        return true;
    };
    if (addCompositing) {
        PUSH_LAMBDA(local_update_undo, local_undo);
        PUSH_LAMBDA(rebuild_compositing, local_undo);
    }
    PUSH_LAMBDA(local_name_update, local_undo);
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    PUSH_LAMBDA(local_name_update, redo);
    return true;
}

bool TimelineModel::requestTrackDeletion(int trackId)
{
    // TODO: make sure we disable overlayTrack before deleting a track
    QWriteLocker locker(&m_lock);
    TRACE(trackId);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestTrackDeletion(trackId, undo, redo);
    if (result) {
        if (m_videoTarget == trackId) {
            m_videoTarget = -1;
        }
        if (m_audioTarget.contains(trackId)) {
            m_audioTarget.remove(trackId);
        }
        PUSH_UNDO(undo, redo, i18n("Delete Track"));
    }
    TRACE_RES(result);
    return result;
}

bool TimelineModel::requestTrackDeletion(int trackId, Fun &undo, Fun &redo)
{
    Q_ASSERT(isTrack(trackId));
    if (m_allTracks.size() < 2) {
        pCore->displayMessage(i18n("Cannot delete last track in timeline"), ErrorMessage, 500);
        return false;
    }
    // Discard running jobs
    pCore->taskManager.discardJobs(ObjectId(KdenliveObjectType::TimelineTrack, trackId, m_uuid));

    std::vector<int> clips_to_delete;
    for (const auto &it : getTrackById(trackId)->m_allClips) {
        clips_to_delete.push_back(it.first);
    }
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    for (int clip : clips_to_delete) {
        bool res = true;
        while (res && m_groups->isInGroup(clip)) {
            res = requestClipUngroup(clip, local_undo, local_redo);
        }
        if (res) {
            res = requestClipDeletion(clip, local_undo, local_redo);
        }
        if (!res) {
            bool u = local_undo();
            Q_ASSERT(u);
            return false;
        }
    }
    std::vector<int> compositions_to_delete;
    for (const auto &it : getTrackById(trackId)->m_allCompositions) {
        compositions_to_delete.push_back(it.first);
    }
    for (int compo : compositions_to_delete) {
        bool res = true;
        while (res && m_groups->isInGroup(compo)) {
            res = requestClipUngroup(compo, local_undo, local_redo);
        }
        if (res) {
            res = requestCompositionDeletion(compo, local_undo, local_redo);
        }
        if (!res) {
            bool u = local_undo();
            Q_ASSERT(u);
            return false;
        }
    }
    int old_position = getTrackPosition(trackId);
    int previousTrack = getPreviousVideoTrackPos(trackId);
    auto operation = deregisterTrack_lambda(trackId);
    std::shared_ptr<TrackModel> track = getTrackById(trackId);
    bool audioTrack = track->isAudioTrack();
    QList<std::shared_ptr<CompositionModel>> updatedCompositions;
    for (auto &compo : m_allCompositions) {
        if (compo.second->getATrack() == old_position + 1 && compo.second->getForcedTrack() == -1) {
            updatedCompositions << compo.second;
        }
    }
    Fun reverse = [this, track, old_position, updatedCompositions]() {
        // We capture a shared_ptr to the track, which means that as long as this undo object lives, the track object is not deleted. To insert it back it is
        // sufficient to register it.
        registerTrack(track, old_position);
        for (auto &compo : updatedCompositions) {
            compo->setATrack(old_position + 1, -1);
        }
        return true;
    };
    Fun local_update = [previousTrack, updatedCompositions]() {
        for (auto &compo : updatedCompositions) {
            compo->setATrack(previousTrack, -1);
        }
        return true;
    };
    Fun rebuild_compositing = [this]() {
        buildTrackCompositing(true);
        return true;
    };
    Fun local_name_update = [old_position, audioTrack, this]() {
        if (audioTrack) {
            for (int i = 0; i < qMin(old_position + 1, getTracksCount()); i++) {
                QModelIndex ix = makeTrackIndexFromID(getTrackIndexFromPosition(i));
                Q_EMIT dataChanged(ix, ix, {TimelineModel::TrackTagRole});
            }
        } else {
            for (int i = old_position; i < getTracksCount(); i++) {
                QModelIndex ix = makeTrackIndexFromID(getTrackIndexFromPosition(i));
                Q_EMIT dataChanged(ix, ix, {TimelineModel::TrackTagRole});
            }
        }
        return true;
    };
    if (operation()) {
        local_update();
        rebuild_compositing();
        local_name_update();
        PUSH_LAMBDA(rebuild_compositing, local_undo);
        PUSH_LAMBDA(local_name_update, local_undo);
        UPDATE_UNDO_REDO(operation, reverse, local_undo, local_redo);
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
        PUSH_LAMBDA(local_update, redo);
        PUSH_LAMBDA(rebuild_compositing, redo);
        PUSH_LAMBDA(local_name_update, redo);
        return true;
    }
    local_undo();
    return false;
}

void TimelineModel::registerTrack(std::shared_ptr<TrackModel> track, int pos, bool doInsert, bool singleOperation)
{
    int id = track->getId();
    if (pos == -1) {
        pos = static_cast<int>(m_allTracks.size());
    }
    Q_ASSERT(pos >= 0);
    Q_ASSERT(pos <= static_cast<int>(m_allTracks.size()));

    // effective insertion (MLT operation), add 1 to account for black background track
    if (doInsert) {
        if (!singleOperation) {
            m_tractor->block();
        }
        int error = m_tractor->insert_track(*track, pos + 1);
        if (!singleOperation) {
            m_tractor->unblock();
        }
        Q_ASSERT(error == 0); // we might need better error handling...
    }

    // we now insert in the list
    auto posIt = m_allTracks.begin();
    std::advance(posIt, pos);
    beginInsertRows(QModelIndex(), pos, pos);
    auto it = m_allTracks.insert(posIt, std::move(track));
    // it now contains the iterator to the inserted element, we store it
    Q_ASSERT(m_iteratorTable.count(id) == 0); // check that id is not used (shouldn't happen)
    m_iteratorTable[id] = it;
    endInsertRows();
    int cache = int(QThread::idealThreadCount()) + int(m_allTracks.size() + 1) * 2;
    mlt_service_cache_set_size(nullptr, "producer_avformat", qMax(4, cache));
}

void TimelineModel::registerClip(const std::shared_ptr<ClipModel> &clip, bool registerProducer)
{
    int id = clip->getId();
    Q_ASSERT(m_allClips.count(id) == 0);
    m_allClips[id] = clip;
    clip->registerClipToBin(clip->getProducer(), registerProducer);
    m_groups->createGroupItem(id);
    clip->setTimelineEffectsEnabled(m_timelineEffectsEnabled);
}

void TimelineModel::registerSubtitle(int id, GenTime startTime, bool temporary)
{
    Q_ASSERT(m_allSubtitles.count(id) == 0);
    m_allSubtitles.emplace(id, startTime);
    if (!temporary) {
        m_groups->createGroupItem(id);
    }
}

int TimelineModel::positionForIndex(int id)
{
    return int(std::distance(m_allSubtitles.begin(), m_allSubtitles.find(id)));
}

void TimelineModel::deregisterSubtitle(int id, bool temporary)
{
    Q_ASSERT(m_allSubtitles.count(id) > 0);
    if (!temporary && m_subtitleModel->isSelected(id)) {
        requestClearSelection(true);
    }
    m_allSubtitles.erase(id);
    if (!temporary) {
        m_groups->destructGroupItem(id);
    }
}

void TimelineModel::registerGroup(int groupId)
{
    Q_ASSERT(m_allGroups.count(groupId) == 0);
    m_allGroups.insert(groupId);
}

Fun TimelineModel::deregisterTrack_lambda(int id)
{
    return [this, id]() {
        if (!m_closing) {
            Q_EMIT checkTrackDeletion(id);
        }
        auto it = m_iteratorTable[id];    // iterator to the element
        int index = getTrackPosition(id); // compute index in list
        if (!m_closing) {
            // send update to the model
            beginRemoveRows(QModelIndex(), index, index);
        }
        // melt operation, add 1 to account for black background track
        m_tractor->remove_track(static_cast<int>(index + 1));
        // actual deletion of object
        m_allTracks.erase(it);
        // clean table
        m_iteratorTable.erase(id);
        if (!m_closing) {
            // Finish operation
            endRemoveRows();
            int cache = int(QThread::idealThreadCount()) + int(m_allTracks.size() + 1) * 2;
            mlt_service_cache_set_size(nullptr, "producer_avformat", qMax(4, cache));
        }
        return true;
    };
}

Fun TimelineModel::deregisterClip_lambda(int clipId)
{
    return [this, clipId]() {
        // Clear effect stack
        Q_EMIT requestClearAssetView(clipId);
        if (!m_closing) {
            Q_EMIT checkItemDeletion(clipId);
        }
        Q_ASSERT(m_allClips.count(clipId) > 0);
        Q_ASSERT(getClipTrackId(clipId) == -1); // clip must be deleted from its track at this point
        Q_ASSERT(!m_groups->isInGroup(clipId)); // clip must be ungrouped at this point
        auto clip = m_allClips[clipId];
        m_allClips.erase(clipId);
        clip->deregisterClipToBin();
        m_groups->destructGroupItem(clipId);
        return true;
    };
}

void TimelineModel::deregisterGroup(int id)
{
    Q_ASSERT(m_allGroups.count(id) > 0);
    m_allGroups.erase(id);
}

std::shared_ptr<TrackModel> TimelineModel::getTrackById(int trackId)
{
    Q_ASSERT(m_iteratorTable.count(trackId) > 0);
    return *m_iteratorTable[trackId];
}

const std::shared_ptr<TrackModel> TimelineModel::getTrackById_const(int trackId) const
{
    Q_ASSERT(m_iteratorTable.count(trackId) > 0);
    return *m_iteratorTable.at(trackId);
}

bool TimelineModel::addTrackEffect(int trackId, const QString &effectId)
{
    if (trackId == -1) {
        if (m_masterStack == nullptr || m_masterStack->appendEffect(effectId) == false) {
            QString effectName = EffectsRepository::get()->getName(effectId);
            pCore->displayMessage(i18n("Cannot add effect %1 to master track", effectName), InformationMessage, 500);
            return false;
        }
    } else {
        Q_ASSERT(m_iteratorTable.count(trackId) > 0);
        if ((*m_iteratorTable.at(trackId))->addEffect(effectId) == false) {
            QString effectName = EffectsRepository::get()->getName(effectId);
            pCore->displayMessage(i18n("Cannot add effect %1 to selected track", effectName), InformationMessage, 500);
            return false;
        }
    }
    return true;
}

bool TimelineModel::copyTrackEffect(int trackId, const QString &sourceId)
{
    QStringList source = sourceId.split(QLatin1Char(','));
    Q_ASSERT(source.count() == 4);
    int itemType = source.at(0).toInt();
    int itemId = source.at(1).toInt();
    int itemRow = source.at(2).toInt();
    const QUuid uuid(source.at(3));
    std::shared_ptr<EffectStackModel> effectStack = pCore->getItemEffectStack(uuid, itemType, itemId);

    if (trackId == -1) {
        QWriteLocker locker(&m_lock);
        if (m_masterStack == nullptr || m_masterStack->copyEffect(effectStack->getEffectStackRow(itemRow), PlaylistState::Disabled) ==
                                            false) { // We use "disabled" in a hacky way to accept video and audio on master
            pCore->displayMessage(i18n("Cannot paste effect to master track"), InformationMessage, 500);
            return false;
        }
    } else {
        Q_ASSERT(m_iteratorTable.count(trackId) > 0);
        if ((*m_iteratorTable.at(trackId))->copyEffect(effectStack, itemRow) == false) {
            pCore->displayMessage(i18n("Cannot paste effect to selected track"), InformationMessage, 500);
            return false;
        }
    }
    return true;
}

std::shared_ptr<ClipModel> TimelineModel::getClipPtr(int clipId) const
{
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_allClips.at(clipId);
}

bool TimelineModel::addClipEffect(int clipId, const QString &effectId, bool notify)
{
    Q_ASSERT(m_allClips.count(clipId) > 0);
    // Check if we are applying an audio effect on an audio clip
    bool isAudio = EffectsRepository::get()->isAudioEffect(effectId);
    bool audioClip = m_allClips.at(clipId)->isAudioOnly();
    if (isAudio != audioClip) {
        // Check if we have a split partner
        clipId = getClipSplitPartner(clipId);
    }
    bool result = clipId > -1 && m_allClips.at(clipId)->addEffect(effectId);
    if (!result && notify) {
        QString effectName = EffectsRepository::get()->getName(effectId);
        pCore->displayMessage(i18n("Cannot add effect %1 to selected clip", effectName), ErrorMessage, 500);
    }
    return result;
}

bool TimelineModel::removeFade(int clipId, bool fromStart)
{
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_allClips.at(clipId)->removeFade(fromStart);
}

std::shared_ptr<EffectStackModel> TimelineModel::getClipEffectStack(int itemId)
{
    Q_ASSERT(m_allClips.count(itemId));
    return m_allClips.at(itemId)->m_effectStack;
}

bool TimelineModel::adjustEffectLength(int clipId, const QString &effectId, int duration, int initialDuration)
{
    Q_ASSERT(m_allClips.count(clipId));
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool res = m_allClips.at(clipId)->adjustEffectLength(effectId, duration, initialDuration, undo, redo);
    if (res && initialDuration > 0) {
        PUSH_UNDO(undo, redo, i18n("Adjust Fade"));
    }
    return res;
}

std::shared_ptr<CompositionModel> TimelineModel::getCompositionPtr(int compoId) const
{
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    return m_allCompositions.at(compoId);
}

int TimelineModel::getNextId()
{
    return KdenliveDoc::next_id++;
}

bool TimelineModel::isClip(int id) const
{
    return m_allClips.count(id) > 0;
}

bool TimelineModel::isComposition(int id) const
{
    return m_allCompositions.count(id) > 0;
}

bool TimelineModel::isSubTitle(int id) const
{
    return m_allSubtitles.count(id) > 0;
}

bool TimelineModel::isItem(int id) const
{
    return isClip(id) || isComposition(id) || isSubTitle(id);
}

bool TimelineModel::isTrack(int id) const
{
    return m_iteratorTable.count(id) > 0;
}

bool TimelineModel::isGroup(int id) const
{
    return m_allGroups.count(id) > 0;
}

void TimelineModel::updateDuration()
{
    if (m_closing) {
        return;
    }
    int current = m_blackClip->get_playtime() - TimelineModel::seekDuration - 1;
    int duration = 0;
    for (const auto &tck : m_iteratorTable) {
        auto track = (*tck.second);
        if (track->isAudioTrack()) {
            if (track->isMute()) {
                continue;
            }
        } else if (track->isHidden()) {
            continue;
        }
        duration = qMax(duration, track->trackDuration());
    }
    if (m_subtitleModel) {
        duration = qMax(duration, m_subtitleModel->trackDuration());
    }
    if (duration != current) {
        // update black track length
        std::unique_ptr<Mlt::Field> field(m_tractor->field());
        field->lock();
        m_blackClip->set("out", duration + TimelineModel::seekDuration);
        field->unlock();
        Q_EMIT durationUpdated(m_uuid);
        if (m_masterStack) {
            Q_EMIT m_masterStack->dataChanged(QModelIndex(), QModelIndex(), {});
        }
    }
}

int TimelineModel::duration() const
{
    int duration = 0;
    auto it = m_allTracks.cbegin();
    while (it != m_allTracks.cend()) {
        if ((*it)->isAudioTrack()) {
            if ((*it)->isMute()) {
                // Muted audio track
                ++it;
                continue;
            }
        } else if ((*it)->isHidden()) {
            // Hidden video track
            ++it;
            continue;
        }
        int trackDuration = (*it)->getTrackService()->get_playtime();
        duration = qMax(duration, trackDuration);
        ++it;
    }
    if (m_subtitleModel && !m_subtitleModel->isDisabled()) {
        duration = qMax(duration, m_subtitleModel->trackDuration());
    }
    return duration;
}

std::unordered_set<int> TimelineModel::getGroupElements(int clipId)
{
    int groupId = m_groups->getRootId(clipId);
    return m_groups->getLeaves(groupId);
}

bool TimelineModel::requestReset(Fun &undo, Fun &redo)
{
    std::vector<int> all_ids;
    for (const auto &track : m_iteratorTable) {
        all_ids.push_back(track.first);
    }
    bool ok = true;
    for (int trackId : all_ids) {
        ok = ok && requestTrackDeletion(trackId, undo, redo);
    }
    return ok;
}

void TimelineModel::setUndoStack(std::weak_ptr<DocUndoStack> undo_stack)
{
    m_undoStack = std::move(undo_stack);
}

int TimelineModel::suggestSnapPoint(int pos, int snapDistance)
{
    int cursorPosition = pCore->getMonitorPosition();
    m_snaps->addPoint(cursorPosition);
    int snapped = m_snaps->getClosestPoint(pos);
    m_snaps->removePoint(cursorPosition);
    return (qAbs(snapped - pos) < snapDistance ? snapped : pos);
}

int TimelineModel::getBestSnapPos(int referencePos, int diff, std::vector<int> pts, int cursorPosition, int snapDistance)
{
    if (!pts.empty()) {
        if (m_editMode == TimelineMode::NormalEdit) {
            m_snaps->ignore(pts);
        }
    } else {
        return -1;
    }
    // Sort and remove duplicates
    std::sort(pts.begin(), pts.end());
    pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
    m_snaps->addPoint(cursorPosition);
    int closest = -1;
    int lowestDiff = snapDistance + 1;
    for (int point : pts) {
        int snapped = m_snaps->getClosestPoint(point + diff);
        int currentDiff = qAbs(point + diff - snapped);
        if (currentDiff < lowestDiff) {
            lowestDiff = currentDiff;
            closest = snapped - (point - referencePos);
            if (lowestDiff < 2) {
                break;
            }
        }
    }
    if (m_editMode == TimelineMode::NormalEdit) {
        m_snaps->unIgnore();
    }
    m_snaps->removePoint(cursorPosition);
    return closest;
}

int TimelineModel::getNextSnapPos(int pos, std::vector<int> &snaps, std::vector<int> &ignored)
{
    QVector<int> tracks;
    // Get active tracks
    auto it = m_allTracks.cbegin();
    while (it != m_allTracks.cend()) {
        if ((*it)->shouldReceiveTimelineOp()) {
            tracks << (*it)->getId();
        }
        ++it;
    }
    bool hasSubtitles = m_subtitleModel && !m_allSubtitles.empty();
    bool filterOutSubtitles = false;
    if (hasSubtitles) {
        // If subtitle track is locked or hidden, don't snap to it
        if (m_subtitleModel->isLocked() || !KdenliveSettings::showSubtitles()) {
            filterOutSubtitles = true;
        }
    }
    if ((tracks.isEmpty() || tracks.count() == int(m_allTracks.size())) && !filterOutSubtitles) {
        // No active track, use all possible snap points
        m_snaps->ignore(ignored);
        int next = m_snaps->getNextPoint(pos);
        m_snaps->unIgnore();
        return next;
    }
    for (auto num : ignored) {
        snaps.erase(std::remove(snaps.begin(), snaps.end(), num), snaps.end());
    }
    // Build snap points for selected tracks
    for (const auto &cp : m_allClips) {
        // Check if clip is on a target track
        if (tracks.contains(cp.second->getCurrentTrackId())) {
            auto clip = (cp.second);
            clip->allSnaps(snaps);
        }
    }
    // Subtitle snaps
    if (hasSubtitles && !filterOutSubtitles) {
        // Add subtitle snaps
        m_subtitleModel->allSnaps(snaps);
    }
    // sort snaps
    std::sort(snaps.begin(), snaps.end());
    for (auto i : snaps) {
        if (int(i) > pos) {
            return int(i);
        }
    }
    return pos;
}

int TimelineModel::getPreviousSnapPos(int pos, std::vector<int> &snaps, std::vector<int> &ignored)
{
    QVector<int> tracks;
    // Get active tracks
    auto it = m_allTracks.cbegin();
    while (it != m_allTracks.cend()) {
        if ((*it)->shouldReceiveTimelineOp()) {
            tracks << (*it)->getId();
        }
        ++it;
    }
    bool hasSubtitles = m_subtitleModel && !m_allSubtitles.empty();
    bool filterOutSubtitles = false;
    if (hasSubtitles) {
        // If subtitle track is locked or hidden, don't snap to it
        if (m_subtitleModel->isLocked() || !KdenliveSettings::showSubtitles()) {
            filterOutSubtitles = true;
        }
    }
    if ((tracks.isEmpty() || tracks.count() == int(m_allTracks.size())) && !filterOutSubtitles) {
        // No active track, use all possible snap points
        m_snaps->ignore(ignored);
        int previous = m_snaps->getPreviousPoint(pos);
        m_snaps->unIgnore();
        return previous;
    }
    // Build snap points for selected tracks
    for (auto num : ignored) {
        snaps.erase(std::remove(snaps.begin(), snaps.end(), num), snaps.end());
    }
    for (const auto &cp : m_allClips) {
        // Check if clip is on a target track
        if (tracks.contains(cp.second->getCurrentTrackId())) {
            auto clip = (cp.second);
            clip->allSnaps(snaps);
        }
    }
    // Subtitle snaps
    if (hasSubtitles && !filterOutSubtitles) {
        // Add subtitle snaps
        m_subtitleModel->allSnaps(snaps);
    }
    // sort snaps
    std::sort(snaps.begin(), snaps.end());
    // sort descending
    std::reverse(snaps.begin(), snaps.end());
    for (auto i : snaps) {
        if (int(i) < pos) {
            return int(i);
        }
    }
    return 0;
}

void TimelineModel::addSnap(int pos)
{
    TRACE(pos);
    return m_snaps->addPoint(pos);
}

void TimelineModel::removeSnap(int pos)
{
    TRACE(pos);
    return m_snaps->removePoint(pos);
}

void TimelineModel::registerComposition(const std::shared_ptr<CompositionModel> &composition)
{
    int id = composition->getId();
    Q_ASSERT(m_allCompositions.count(id) == 0);
    m_allCompositions[id] = composition;
    m_groups->createGroupItem(id);
}

bool TimelineModel::requestCompositionInsertion(const QString &transitionId, int trackId, int position, int length, std::unique_ptr<Mlt::Properties> transProps,
                                                int &id, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    // TRACE(transitionId, trackId, position, length, transProps.get(), id, logUndo);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = requestCompositionInsertion(transitionId, trackId, -1, position, length, std::move(transProps), id, undo, redo, logUndo);
    if (result && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Insert Composition"));
    }
    // TRACE_RES(result);
    return result;
}

bool TimelineModel::requestCompositionCreation(const QString &transitionId, int length, std::unique_ptr<Mlt::Properties> transProps, int &id, Fun &undo,
                                               Fun &redo, bool finalMove, const QString &originalDecimalPoint)
{
    Q_UNUSED(finalMove)
    int compositionId = TimelineModel::getNextId();
    id = compositionId;
    Fun local_undo = deregisterComposition_lambda(compositionId);
    CompositionModel::construct(shared_from_this(), transitionId, originalDecimalPoint, length, compositionId, std::move(transProps));
    auto composition = m_allCompositions[compositionId];
    Fun local_redo = [composition, this]() {
        // We capture a shared_ptr to the composition, which means that as long as this undo object lives, the composition object is not deleted. To insert it
        // back it is sufficient to register it.
        registerComposition(composition);
        return true;
    };
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::requestCompositionInsertion(const QString &transitionId, int trackId, int compositionTrack, int position, int length,
                                                std::unique_ptr<Mlt::Properties> transProps, int &id, Fun &undo, Fun &redo, bool finalMove,
                                                const QString &originalDecimalPoint)
{
    int compositionId = TimelineModel::getNextId();
    id = compositionId;
    Fun local_undo = deregisterComposition_lambda(compositionId);
    CompositionModel::construct(shared_from_this(), transitionId, originalDecimalPoint, length, compositionId, std::move(transProps));
    auto composition = m_allCompositions[compositionId];
    Fun local_redo = [composition, this]() {
        // We capture a shared_ptr to the composition, which means that as long as this undo object lives, the composition object is not deleted. To insert it
        // back it is sufficient to register it.
        registerComposition(composition);
        return true;
    };
    bool res = requestCompositionMove(compositionId, trackId, compositionTrack, position, true, finalMove, local_undo, local_redo);
    if (res) {
        res = requestItemResize(compositionId, length, true, true, local_undo, local_redo, true);
    }
    if (!res) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        id = -1;
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

Fun TimelineModel::deregisterComposition_lambda(int compoId)
{
    return [this, compoId]() {
        Q_ASSERT(m_allCompositions.count(compoId) > 0);
        Q_ASSERT(!m_groups->isInGroup(compoId)); // composition must be ungrouped at this point
        requestClearSelection(true);
        Q_EMIT requestClearAssetView(compoId);
        m_allCompositions.erase(compoId);
        m_groups->destructGroupItem(compoId);
        return true;
    };
}

int TimelineModel::getSubtitlePosition(int subId) const
{
    Q_ASSERT(m_allSubtitles.count(subId) > 0);
    return m_allSubtitles.at(subId).frames(pCore->getCurrentFps());
}

int TimelineModel::getCompositionPosition(int compoId) const
{
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    const auto trans = m_allCompositions.at(compoId);
    return trans->getPosition();
}

int TimelineModel::getCompositionEnd(int compoId) const
{
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    const auto trans = m_allCompositions.at(compoId);
    return trans->getPosition() + trans->getPlaytime();
}

int TimelineModel::getCompositionPlaytime(int compoId) const
{
    READ_LOCK();
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    const auto trans = m_allCompositions.at(compoId);
    int playtime = trans->getPlaytime();
    return playtime;
}

int TimelineModel::getItemPosition(int itemId) const
{
    if (isClip(itemId)) {
        return getClipPosition(itemId);
    }
    if (isComposition(itemId)) {
        return getCompositionPosition(itemId);
    }
    if (isSubTitle(itemId)) {
        return getSubtitlePosition(itemId);
    }
    return -1;
}

int TimelineModel::getClipSubPlaylistIndex(int cid) const
{
    Q_ASSERT(isClip(cid));
    return m_allClips.at(cid)->getSubPlaylistIndex();
}

const QString TimelineModel::getClipName(int cid) const
{
    Q_ASSERT(isClip(cid));
    return m_allClips.at(cid)->clipName();
}

int TimelineModel::getItemEnd(int itemId) const
{
    if (isClip(itemId)) {
        return getClipEnd(itemId);
    }
    if (isComposition(itemId)) {
        return getCompositionEnd(itemId);
    }
    if (isSubTitle(itemId)) {
        return m_subtitleModel->getSubtitleEnd(itemId);
    }
    return -1;
}

int TimelineModel::getItemPlaytime(int itemId) const
{
    if (isClip(itemId)) {
        return getClipPlaytime(itemId);
    }
    if (isComposition(itemId)) {
        return getCompositionPlaytime(itemId);
    }
    if (isSubTitle(itemId)) {
        return m_subtitleModel->getSubtitlePlaytime(itemId);
    }
    return -1;
}

int TimelineModel::getTrackCompositionsCount(int trackId) const
{
    Q_ASSERT(isTrack(trackId));
    return getTrackById_const(trackId)->getCompositionsCount();
}

bool TimelineModel::requestCompositionMove(int compoId, int trackId, int position, bool updateView, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isComposition(compoId));
    if (m_allCompositions[compoId]->getPosition() == position && getCompositionTrackId(compoId) == trackId) {
        return true;
    }
    if (m_groups->isInGroup(compoId)) {
        // element is in a group.
        int groupId = m_groups->getRootId(compoId);
        int current_trackId = getCompositionTrackId(compoId);
        int track_pos1 = getTrackPosition(trackId);
        int track_pos2 = getTrackPosition(current_trackId);
        int delta_track = track_pos1 - track_pos2;
        int delta_pos = position - m_allCompositions[compoId]->getPosition();
        return requestGroupMove(compoId, groupId, delta_track, delta_pos, true, updateView, logUndo);
    }
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    int min = getCompositionPosition(compoId);
    int max = min + getCompositionPlaytime(compoId);
    int tk = getCompositionTrackId(compoId);
    bool res = requestCompositionMove(compoId, trackId, m_allCompositions[compoId]->getForcedTrack(), position, updateView, logUndo, undo, redo);
    if (tk > -1) {
        min = qMin(min, getCompositionPosition(compoId));
        max = qMax(max, getCompositionPosition(compoId));
    } else {
        min = getCompositionPosition(compoId);
        max = min + getCompositionPlaytime(compoId);
    }

    if (res && logUndo) {
        PUSH_UNDO(undo, redo, i18n("Move composition"));
        checkRefresh(min, max);
    }
    return res;
}

bool TimelineModel::isAudioTrack(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    return (*it)->isAudioTrack();
}

bool TimelineModel::isSubtitleTrack(int trackId) const
{
    return trackId == -2;
}

bool TimelineModel::requestCompositionMove(int compoId, int trackId, int compositionTrack, int position, bool updateView, bool finalMove, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Q_ASSERT(isComposition(compoId));
    Q_ASSERT(isTrack(trackId));
    if (compositionTrack == -1 || (compositionTrack > 0 && trackId == getTrackIndexFromPosition(compositionTrack - 1))) {
        compositionTrack = getPreviousVideoTrackPos(trackId);
    }
    if (compositionTrack == -1) {
        // it doesn't make sense to insert a composition on the last track
        qDebug() << "Move failed because of last track";
        return false;
    }

    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    bool ok = true;
    int old_trackId = getCompositionTrackId(compoId);
    bool notifyViewOnly = false;
    Fun update_model = []() { return true; };
    if (updateView && old_trackId == trackId) {
        // Move on same track, only send view update
        updateView = false;
        notifyViewOnly = true;
        update_model = [compoId, this]() {
            QModelIndex modelIndex = makeCompositionIndexFromID(compoId);
            notifyChange(modelIndex, modelIndex, StartRole);
            return true;
        };
    }
    if (old_trackId != -1) {
        Fun delete_operation = []() { return true; };
        Fun delete_reverse = []() { return true; };
        if (old_trackId != trackId) {
            delete_operation = [this, compoId]() {
                bool res = unplantComposition(compoId);
                if (res) m_allCompositions[compoId]->setATrack(-1, -1);
                return res;
            };
            int oldAtrack = m_allCompositions[compoId]->getATrack();
            delete_reverse = [this, compoId, oldAtrack, updateView]() {
                m_allCompositions[compoId]->setATrack(oldAtrack, oldAtrack < 1 ? -1 : getTrackIndexFromPosition(oldAtrack - 1));
                return replantCompositions(compoId, updateView);
            };
        }
        ok = delete_operation();
        if (!ok) qDebug() << "Move failed because of first delete operation";

        if (ok) {
            if (notifyViewOnly) {
                PUSH_LAMBDA(update_model, local_undo);
            }
            UPDATE_UNDO_REDO(delete_operation, delete_reverse, local_undo, local_redo);
            ok = getTrackById(old_trackId)->requestCompositionDeletion(compoId, updateView, finalMove, local_undo, local_redo, false);
        }
        if (!ok) {
            qDebug() << "Move failed because of first deletion request";
            bool undone = local_undo();
            Q_ASSERT(undone);
            return false;
        }
    }
    ok = getTrackById(trackId)->requestCompositionInsertion(compoId, position, updateView, finalMove, local_undo, local_redo);
    if (!ok) qDebug() << "Move failed because of second insertion request";
    if (ok) {
        Fun insert_operation = []() { return true; };
        Fun insert_reverse = []() { return true; };
        if (old_trackId != trackId) {
            insert_operation = [this, compoId, compositionTrack, updateView]() {
                m_allCompositions[compoId]->setATrack(compositionTrack, compositionTrack < 1 ? -1 : getTrackIndexFromPosition(compositionTrack - 1));
                return replantCompositions(compoId, updateView);
            };
            insert_reverse = [this, compoId]() {
                bool res = unplantComposition(compoId);
                if (res) m_allCompositions[compoId]->setATrack(-1, -1);
                return res;
            };
        }
        ok = insert_operation();
        if (!ok) qDebug() << "Move failed because of second insert operation";
        if (ok) {
            if (notifyViewOnly) {
                PUSH_LAMBDA(update_model, local_redo);
            }
            UPDATE_UNDO_REDO(insert_operation, insert_reverse, local_undo, local_redo);
        }
    }
    if (!ok) {
        bool undone = local_undo();
        Q_ASSERT(undone);
        return false;
    }
    update_model();
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return true;
}

bool TimelineModel::replantCompositions(int currentCompo, bool updateView)
{
    // We ensure that the compositions are planted in a decreasing order of a_track, and increasing order of b_track.
    // For that, there is no better option than to disconnect every composition and then reinsert everything in the correct order.
    std::vector<std::pair<int, int>> compos;
    for (const auto &compo : m_allCompositions) {
        int trackId = compo.second->getCurrentTrackId();
        if (trackId == -1 || compo.second->getATrack() == -1) {
            continue;
        }
        // Note: we need to retrieve the position of the track, that is its melt index.
        int trackPos = getTrackMltIndex(trackId);
        compos.emplace_back(trackPos, compo.first);
        if (compo.first != currentCompo) {
            unplantComposition(compo.first);
        }
    }
    // sort by decreasing b_track
    std::sort(compos.begin(), compos.end(), [&](const std::pair<int, int> &a, const std::pair<int, int> &b) {
        if (m_allCompositions[a.second]->getATrack() == m_allCompositions[b.second]->getATrack()) {
            return a.first < b.first;
        }
        return m_allCompositions[a.second]->getATrack() > m_allCompositions[b.second]->getATrack();
    });
    // replant
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();

    // Unplant track compositing
    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString resource = mlt_properties_get(properties, "mlt_service");

    mlt_service_type mlt_type = mlt_service_identify(nextservice);
    QList<Mlt::Transition *> trackCompositions;
    while (mlt_type == mlt_service_transition_type) {
        Mlt::Transition transition(reinterpret_cast<mlt_transition>(nextservice));
        nextservice = mlt_service_producer(nextservice);
        int internal = transition.get_int("internal_added");
        if (internal > 0 && resource != QLatin1String("mix")) {
            trackCompositions << new Mlt::Transition(transition);
            field->disconnect_service(transition);
            transition.disconnect_all_producers();
        }
        if (nextservice == nullptr) {
            break;
        }
        mlt_type = mlt_service_identify(nextservice);
        properties = MLT_SERVICE_PROPERTIES(nextservice);
        resource = mlt_properties_get(properties, "mlt_service");
    }
    // Sort track compositing
    std::sort(trackCompositions.begin(), trackCompositions.end(), [](Mlt::Transition *a, Mlt::Transition *b) { return a->get_b_track() < b->get_b_track(); });

    for (const auto &compo : compos) {
        int aTrack = m_allCompositions[compo.second]->getATrack();
        Q_ASSERT(aTrack != -1 && aTrack < m_tractor->count());

        Mlt::Transition &transition = *m_allCompositions[compo.second].get();
        transition.set_tracks(aTrack, compo.first);
        int ret = field->plant_transition(transition, aTrack, compo.first);

        mlt_service consumer = mlt_service_consumer(transition.get_service());
        Q_ASSERT(consumer != nullptr);
        if (ret != 0) {
            field->unlock();
            return false;
        }
    }
    // Replant last tracks compositing
    while (!trackCompositions.isEmpty()) {
        Mlt::Transition *firstTr = trackCompositions.takeFirst();
        field->plant_transition(*firstTr, firstTr->get_a_track(), firstTr->get_b_track());
    }
    field->unlock();
    if (updateView) {
        QModelIndex modelIndex = makeCompositionIndexFromID(currentCompo);
        notifyChange(modelIndex, modelIndex, ItemATrack);
    }
    return true;
}

bool TimelineModel::unplantComposition(int compoId)
{
    Mlt::Transition &transition = *m_allCompositions[compoId].get();
    mlt_service consumer = mlt_service_consumer(transition.get_service());
    Q_ASSERT(consumer != nullptr);
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();
    field->disconnect_service(transition);
    int ret = transition.disconnect_all_producers();

    mlt_service nextservice = mlt_service_get_producer(transition.get_service());
    // mlt_service consumer = mlt_service_consumer(transition.get_service());
    Q_ASSERT(nextservice == nullptr);
    // Q_ASSERT(consumer == nullptr);
    field->unlock();
    return ret != 0;
}

bool TimelineModel::checkConsistency(const std::vector<int> &guideSnaps)
{
    // We store all in/outs of clips to check snap points
    std::map<int, int> snaps;

    for (const auto &tck : m_iteratorTable) {
        auto track = (*tck.second);
        // Check parent/children link for tracks
        if (auto ptr = track->m_parent.lock()) {
            if (ptr.get() != this) {
                qWarning() << "Wrong parent for track" << tck.first;
                return false;
            }
        } else {
            qWarning() << "NULL parent for track" << tck.first;
            return false;
        }
        // check consistency of track
        if (!track->checkConsistency()) {
            qWarning() << "Consistency check failed for track" << tck.first;
            return false;
        }
    }

    // Check parent/children link for clips
    for (const auto &cp : m_allClips) {
        auto clip = (cp.second);
        // Check parent/children link for tracks
        if (auto ptr = clip->m_parent.lock()) {
            if (ptr.get() != this) {
                qWarning() << "Wrong parent for clip" << cp.first;
                return false;
            }
        } else {
            qWarning() << "NULL parent for clip" << cp.first;
            return false;
        }
        if (getClipTrackId(cp.first) != -1) {
            snaps[clip->getPosition()] += 1;
            snaps[clip->getPosition() + clip->getPlaytime()] += 1;
            if (clip->getMixDuration() > 0) {
                snaps[clip->getPosition() + clip->getMixDuration() - clip->getMixCutPosition()] += 1;
            }
        }
        if (!clip->checkConsistency()) {
            qWarning() << "Consistency check failed for clip" << cp.first;
            return false;
        }
    }
    for (const auto &cp : m_allCompositions) {
        auto clip = (cp.second);
        // Check parent/children link for tracks
        if (auto ptr = clip->m_parent.lock()) {
            if (ptr.get() != this) {
                qWarning() << "Wrong parent for compo" << cp.first;
                return false;
            }
        } else {
            qWarning() << "NULL parent for compo" << cp.first;
            return false;
        }
        if (getCompositionTrackId(cp.first) != -1) {
            snaps[clip->getPosition()] += 1;
            snaps[clip->getPosition() + clip->getPlaytime()] += 1;
        }
    }

    for (auto p : guideSnaps) {
        snaps[p] += 1;
    }

    // Check snaps
    auto stored_snaps = m_snaps->_snaps();
    if (snaps.size() != stored_snaps.size()) {
        qWarning() << "Wrong number of snaps" << snaps.size() << stored_snaps.size();
        return false;
    }
    for (auto i = snaps.begin(), j = stored_snaps.begin(); i != snaps.end(); ++i, ++j) {
        if (*i != *j) {
            qWarning() << "Wrong snap info at point" << (*i).first;
            return false;
        }
    }

    // We check consistency with bin model
    auto binClips = pCore->projectItemModel()->getAllClipIds();
    // First step: all clips referenced by the bin model exist and are inserted
    for (const auto &binClip : binClips) {
        auto projClip = pCore->projectItemModel()->getClipByBinID(binClip);
        for (const auto &insertedClip : projClip->m_registeredClips) {
            if (auto ptr = insertedClip.second.lock()) {
                if (ptr.get() == this) { // check we are talking of this timeline
                    if (!isClip(insertedClip.first)) {
                        qWarning() << "Bin model registers a bad clip ID" << insertedClip.first;
                        return false;
                    }
                }
            } else {
                qWarning() << "Bin model registers a clip in a NULL timeline" << insertedClip.first;
                return false;
            }
        }
    }

    // Second step: all clips are referenced
    for (const auto &clip : m_allClips) {
        auto binId = clip.second->m_binClipId;
        auto projClip = pCore->projectItemModel()->getClipByBinID(binId);
        if (projClip->m_registeredClips.count(clip.first) == 0) {
            qWarning() << "Clip " << clip.first << "not registered in bin";
            return false;
        }
    }

    // We now check consistency of the compositions. For that, we list all compositions of the tractor, and see if we have a matching one in our
    // m_allCompositions
    std::unordered_set<int> remaining_compo;
    for (const auto &compo : m_allCompositions) {
        if (getCompositionTrackId(compo.first) != -1 && m_allCompositions[compo.first]->getATrack() != -1) {
            remaining_compo.insert(compo.first);

            // check validity of the consumer
            Mlt::Transition &transition = *m_allCompositions[compo.first].get();
            mlt_service consumer = mlt_service_consumer(transition.get_service());
            Q_ASSERT(consumer != nullptr);
        }
    }
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();

    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_service_type mlt_type = mlt_service_identify(nextservice);
    while (nextservice != nullptr) {
        if (mlt_type == mlt_service_transition_type) {
            auto tr = mlt_transition(nextservice);
            if (mlt_properties_get_int(MLT_TRANSITION_PROPERTIES(tr), "internal_added") > 0) {
                // Skip track compositing
                nextservice = mlt_service_producer(nextservice);
                continue;
            }
            int currentTrack = mlt_transition_get_b_track(tr);
            int currentATrack = mlt_transition_get_a_track(tr);
            if (currentTrack == currentATrack) {
                // Skip invalid transitions created by MLT on track deletion
                nextservice = mlt_service_producer(nextservice);
                continue;
            }

            int currentIn = mlt_transition_get_in(tr);
            int currentOut = mlt_transition_get_out(tr);

            int foundId = -1;
            // we iterate to try to find a matching compo
            for (int compoId : remaining_compo) {
                if (getTrackMltIndex(getCompositionTrackId(compoId)) == currentTrack && m_allCompositions[compoId]->getATrack() == currentATrack &&
                    m_allCompositions[compoId]->getIn() == currentIn && m_allCompositions[compoId]->getOut() == currentOut) {
                    foundId = compoId;
                    break;
                }
            }
            if (foundId == -1) {
                qWarning() << "No matching composition IN: " << currentIn << ", OUT: " << currentOut << ", TRACK: " << currentTrack << " / " << currentATrack
                           << ", SERVICE: " << mlt_properties_get(MLT_TRANSITION_PROPERTIES(tr), "mlt_service")
                           << "\nID: " << mlt_properties_get(MLT_TRANSITION_PROPERTIES(tr), "id");
                field->unlock();
                return false;
            }
            remaining_compo.erase(foundId);
        }
        nextservice = mlt_service_producer(nextservice);
        if (nextservice == nullptr) {
            break;
        }
        mlt_type = mlt_service_identify(nextservice);
    }
    field->unlock();

    if (!remaining_compo.empty()) {
        qWarning() << "Compositions have not been found:";
        for (int compoId : remaining_compo) {
            qWarning() << compoId;
        }
        return false;
    }

    // We check consistency of groups
    if (!m_groups->checkConsistency(true, true)) {
        qWarning() << "error in group consistency";
        return false;
    }

    // Check that the selection is in a valid state:
    if (m_currentSelection != -1 && !isClip(m_currentSelection) && !isComposition(m_currentSelection) && !isSubTitle(m_currentSelection) &&
        !isGroup(m_currentSelection)) {
        qWarning() << "Selection is in inconsistent state";
        return false;
    }
    return true;
}

void TimelineModel::setTimelineEffectsEnabled(bool enabled)
{
    m_timelineEffectsEnabled = enabled;
    // propagate info to clips
    for (const auto &clip : m_allClips) {
        clip.second->setTimelineEffectsEnabled(enabled);
    }

    // TODO if we support track effects, they should be disabled here too
}

std::shared_ptr<Mlt::Producer> TimelineModel::producer()
{
    return std::make_shared<Mlt::Producer>(tractor());
}

const QString TimelineModel::sceneList(const QString &root, const QString &fullPath, const QString &filterData)
{
    LocaleHandling::resetLocale();
    QString playlist;
    Mlt::Consumer xmlConsumer(pCore->getProjectProfile(), "xml", fullPath.isEmpty() ? "kdenlive_playlist" : fullPath.toUtf8().constData());
    if (!root.isEmpty()) {
        xmlConsumer.set("root", root.toUtf8().constData());
    }
    if (!xmlConsumer.is_valid()) {
        return QString();
    }
    xmlConsumer.set("store", "kdenlive");
    xmlConsumer.set("time_format", "clock");
    // Disabling meta creates cleaner files, but then we don't have access to metadata on the fly (meta channels, etc)
    // And we must use "avformat" instead of "avformat-novalidate" on project loading which causes a big delay on project opening
    // xmlConsumer.set("no_meta", 1);
    Mlt::Service s(m_tractor->get_service());
    std::unique_ptr<Mlt::Filter> filter = nullptr;
    if (!filterData.isEmpty()) {
        filter = std::make_unique<Mlt::Filter>(pCore->getProjectProfile().get_profile(), QString("dynamictext:%1").arg(filterData).toUtf8().constData());
        filter->set("fgcolour", "#ffffff");
        filter->set("bgcolour", "#bb333333");
        s.attach(*filter.get());
    }
    xmlConsumer.connect(s);
    xmlConsumer.run();
    if (filter) {
        s.detach(*filter.get());
    }
    playlist = fullPath.isEmpty() ? QString::fromUtf8(xmlConsumer.get("kdenlive_playlist")) : fullPath;
    return playlist;
}

void TimelineModel::checkRefresh(int start, int end)
{
    if (m_blockRefresh) {
        return;
    }
    int currentPos = tractor()->position();
    if (currentPos >= start && currentPos < end) {
        Q_EMIT requestMonitorRefresh();
    }
}

std::shared_ptr<AssetParameterModel> TimelineModel::getCompositionParameterModel(int compoId) const
{
    READ_LOCK();
    Q_ASSERT(isComposition(compoId));
    return std::static_pointer_cast<AssetParameterModel>(m_allCompositions.at(compoId));
}

std::shared_ptr<EffectStackModel> TimelineModel::getClipEffectStackModel(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(isClip(clipId));
    return std::static_pointer_cast<EffectStackModel>(m_allClips.at(clipId)->m_effectStack);
}

std::shared_ptr<EffectStackModel> TimelineModel::getClipMixStackModel(int clipId) const
{
    READ_LOCK();
    Q_ASSERT(isClip(clipId));
    return std::static_pointer_cast<EffectStackModel>(m_allClips.at(clipId)->m_effectStack);
}

std::shared_ptr<EffectStackModel> TimelineModel::getTrackEffectStackModel(int trackId)
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    return getTrackById(trackId)->m_effectStack;
}

std::shared_ptr<EffectStackModel> TimelineModel::getMasterEffectStackModel()
{
    READ_LOCK();
    if (m_masterStack == nullptr) {
        m_masterService.reset(new Mlt::Service(*m_tractor.get()));
        m_masterStack = EffectStackModel::construct(m_masterService, ObjectId(KdenliveObjectType::Master, 0, m_uuid), m_undoStack);
        connect(m_masterStack.get(), &EffectStackModel::updateMasterZones, pCore.get(), &Core::updateMasterZones);
    }
    return m_masterStack;
}

void TimelineModel::importMasterEffects(std::weak_ptr<Mlt::Service> service)
{
    READ_LOCK();
    if (m_masterStack == nullptr) {
        getMasterEffectStackModel();
    }
    m_masterStack->importEffects(std::move(service), PlaylistState::Disabled, false, QString(), m_uuid);
}

QStringList TimelineModel::extractCompositionLumas() const
{
    QStringList urls;
    for (const auto &compo : m_allCompositions) {
        QString luma = compo.second->getProperty(QStringLiteral("resource"));
        if (luma.isEmpty()) {
            luma = compo.second->getProperty(QStringLiteral("luma"));
        }
        if (!luma.isEmpty()) {
            urls << QUrl::fromLocalFile(luma).toLocalFile();
        }
    }
    urls.removeDuplicates();
    return urls;
}

QStringList TimelineModel::extractExternalEffectFiles() const
{
    QStringList urls;
    for (const auto &clip : m_allClips) {
        urls << clip.second->externalFiles();
    }
    return urls;
}

void TimelineModel::adjustAssetRange(int clipId, int in, int out)
{
    Q_UNUSED(clipId)
    Q_UNUSED(in)
    Q_UNUSED(out)
    // pCore->adjustAssetRange(clipId, in, out);
}

bool TimelineModel::requestClipReload(int clipId, int forceDuration, Fun &local_undo, Fun &local_redo)
{
    if (m_closing) {
        return false;
    }
    // in order to make the producer change effective, we need to unplant / replant the clip in its track
    int old_trackId = getClipTrackId(clipId);
    int oldPos = getClipPosition(clipId);
    int oldOut = getClipIn(clipId) + getClipPlaytime(clipId);
    int currentSubplaylist = m_allClips[clipId]->getSubPlaylistIndex();
    int maxDuration = m_allClips[clipId]->getMaxDuration();
    bool hasPitch = false;
    double speed = m_allClips[clipId]->getSpeed();
    PlaylistState::ClipState state = m_allClips[clipId]->clipState();
    if (!qFuzzyCompare(speed, 1.)) {
        hasPitch = m_allClips[clipId]->getIntProperty(QStringLiteral("warp_pitch"));
    }
    int audioStream = m_allClips[clipId]->getIntProperty(QStringLiteral("audio_index"));
    bool timeremap = m_allClips[clipId]->hasTimeRemap();
    // Check if clip out is longer than actual producer duration (if user forced duration)
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(getClipBinId(clipId));
    bool clipIsShorter = oldOut > int(binClip->frameDuration());
    bool refreshView = clipIsShorter || forceDuration > -1;
    if (old_trackId != -1) {
        if (clipIsShorter && forceDuration == -1 && binClip->hasLimitedDuration()) {
            // replacement clip is shorter, resize first
            int resizeDuration = int(binClip->frameDuration());
            requestItemResize(clipId, resizeDuration, true, true, local_undo, local_redo);
        }
        getTrackById(old_trackId)->requestClipDeletion(clipId, refreshView, true, local_undo, local_redo, false, false);
        m_allClips[clipId]->refreshProducerFromBin(old_trackId, state, audioStream, 0, hasPitch, currentSubplaylist == 1, timeremap);
        if (forceDuration > -1) {
            m_allClips[clipId]->requestResize(forceDuration, true, local_undo, local_redo);
        }
        getTrackById(old_trackId)->requestClipInsertion(clipId, oldPos, refreshView, true, local_undo, local_redo, false, false);
        if (maxDuration != m_allClips[clipId]->getMaxDuration()) {
            QModelIndex ix = makeClipIndexFromID(clipId);
            Q_EMIT dataChanged(ix, ix, {TimelineModel::MaxDurationRole});
        }
    }
    return clipIsShorter;
}

void TimelineModel::replugClip(int clipId)
{
    int old_trackId = getClipTrackId(clipId);
    if (old_trackId != -1) {
        getTrackById(old_trackId)->replugClip(clipId);
    }
}

void TimelineModel::requestClipUpdate(int clipId, const QVector<int> &roles)
{
    QModelIndex modelIndex = makeClipIndexFromID(clipId);
    if (roles.contains(TimelineModel::ReloadAudioThumbRole)) {
        m_allClips[clipId]->forceThumbReload = !m_allClips[clipId]->forceThumbReload;
    }
    if (roles.contains(TimelineModel::ResourceRole)) {
        int in = getClipPosition(clipId);
        Q_EMIT invalidateZone(in, in + getClipPlaytime(clipId));
    }
    notifyChange(modelIndex, modelIndex, roles);
}

bool TimelineModel::requestClipTimeWarp(int clipId, double speed, bool pitchCompensate, bool changeDuration, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Fun local_undo = []() { return true; };
    Fun local_redo = []() { return true; };
    int oldPos = getClipPosition(clipId);
    // in order to make the producer change effective, we need to unplant / replant the clip in its track
    bool success = true;
    int trackId = getClipTrackId(clipId);
    if (trackId != -1) {
        success = success && getTrackById(trackId)->requestClipDeletion(clipId, true, true, local_undo, local_redo, false, false);
    }
    if (success) {
        success = m_allClips[clipId]->useTimewarpProducer(speed, pitchCompensate, changeDuration, local_undo, local_redo);
    }
    if (trackId != -1) {
        success = success && getTrackById(trackId)->requestClipInsertion(clipId, oldPos, true, true, local_undo, local_redo, false, false);
    }
    if (!success) {
        local_undo();
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return success;
}

bool TimelineModel::requestClipTimeRemap(int clipId, bool enable)
{
    if (!enable || !m_allClips[clipId]->hasTimeRemap()) {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        int splitId = m_groups->getSplitPartner(clipId);
        bool result = true;
        if (splitId > -1) {
            result = requestClipTimeRemap(splitId, enable, undo, redo);
        }
        result = result && requestClipTimeRemap(clipId, enable, undo, redo);
        if (result) {
            PUSH_UNDO(undo, redo, i18n("Enable time remap"));
            Q_EMIT refreshClipActions();
            return true;
        } else {
            return false;
        }
    }
    return true;
}

std::shared_ptr<Mlt::Producer> TimelineModel::getClipProducer(int clipId)
{
    Q_ASSERT(m_allClips.count(clipId) > 0);
    return m_allClips[clipId]->getProducer();
}

bool TimelineModel::requestClipTimeRemap(int clipId, bool enable, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    int oldPos = getClipPosition(clipId);
    // in order to make the producer change effective, we need to unplant / replant the clip in int track
    bool success = true;
    int trackId = getClipTrackId(clipId);
    int previousDuration = 0;
    qDebug() << "=== REQUEST REMAP: " << enable << "\n\nWWWWWWWWWWWWWWWWWWWWWWWWWWWW";
    if (!enable && m_allClips[clipId]->hasTimeRemap()) {
        previousDuration = m_allClips[clipId]->getRemapInputDuration();
        qDebug() << "==== CALCULATED INPIUT DURATION: " << previousDuration << "\n\nHHHHHHHHHHHHHH";
    }
    if (trackId != -1) {
        success = success && getTrackById(trackId)->requestClipDeletion(clipId, true, true, local_undo, local_redo, false, false);
    }
    if (success) {
        success = m_allClips[clipId]->useTimeRemapProducer(enable, local_undo, local_redo);
    }
    if (trackId != -1) {
        success = success && getTrackById(trackId)->requestClipInsertion(clipId, oldPos, true, true, local_undo, local_redo, false, false);
        if (success && !enable && previousDuration > 0) {
            // Restore input duration
            requestItemResize(clipId, previousDuration, true, true, local_undo, local_redo);
        }
    }
    if (!success) {
        local_undo();
        return false;
    }
    UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
    return success;
}

bool TimelineModel::requestClipTimeWarp(int clipId, double speed, bool pitchCompensate, bool changeDuration)
{
    QWriteLocker locker(&m_lock);
    if (qFuzzyCompare(speed, m_allClips[clipId]->getSpeed()) && pitchCompensate == bool(m_allClips[clipId]->getIntProperty("warp_pitch"))) {
        return true;
    }
    TRACE(clipId, speed);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    // Get main clip info
    int trackId = getClipTrackId(clipId);
    bool result = true;
    if (trackId != -1) {
        // Check if clip has a split partner
        int splitId = m_groups->getSplitPartner(clipId);
        if (splitId > -1) {
            result = requestClipTimeWarp(splitId, speed / 100.0, pitchCompensate, changeDuration, undo, redo);
        }
        if (result) {
            result = requestClipTimeWarp(clipId, speed / 100.0, pitchCompensate, changeDuration, undo, redo);
        }
        if (!result) {
            pCore->displayMessage(i18n("Change speed failed"), ErrorMessage);
            undo();
            TRACE_RES(false);
            return false;
        }
    } else {
        // If clip is not inserted on a track, we just change the producer
        result = m_allClips[clipId]->useTimewarpProducer(speed, pitchCompensate, changeDuration, undo, redo);
    }
    if (result) {
        PUSH_UNDO(undo, redo, i18n("Change clip speed"));
    }
    TRACE_RES(result);
    return result;
}

const QString TimelineModel::getTrackTagById(int trackId) const
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    bool isAudio = getTrackById_const(trackId)->isAudioTrack();
    int count = 1;
    int totalAudio = 2;
    auto it = m_allTracks.cbegin();
    bool found = false;
    while ((isAudio || !found) && it != m_allTracks.cend()) {
        if ((*it)->isAudioTrack()) {
            totalAudio++;
            if (isAudio && !found) {
                count++;
            }
        } else if (!isAudio) {
            count++;
        }
        if ((*it)->getId() == trackId) {
            found = true;
        }
        it++;
    }
    return isAudio ? QStringLiteral("A%1").arg(totalAudio - count) : QStringLiteral("V%1").arg(count - 1);
}

/*void TimelineModel::updateProfile(Mlt::Profile profile)
{
    m_profile = profile;
    m_tractor->set_profile(m_profile.get_profile());
    for (int i = 0; i < m_tractor->count(); i++) {
        std::shared_ptr<Mlt::Producer> tk(m_tractor->track(i));
        tk->set_profile(m_profile.get_profile());
        if (tk->type() == mlt_service_tractor_type) {
            Mlt::Tractor sub(*tk.get());
            for (int j = 0; j < sub.count(); j++) {
                std::shared_ptr<Mlt::Producer> subtk(sub.track(j));
                subtk->set_profile(m_profile.get_profile());
            }
        }
    }
    m_blackClip->set_profile(m_profile.get_profile());
    // Rebuild compositions since profile has changed
    buildTrackCompositing(true);
}*/

void TimelineModel::updateFieldOrderFilter(std::unique_ptr<ProfileModel> &ptr)
{
    std::shared_ptr<Mlt::Filter> foFilter = nullptr;
    for (int i = 0; i < m_tractor->filter_count(); i++) {
        std::shared_ptr<Mlt::Filter> fl(m_tractor->filter(i));
        if (!fl->is_valid()) {
            continue;
        }
        const QString filterService = fl->get("mlt_service");
        int foundCount = 0;
        if (filterService == QLatin1String("avfilter.fieldorder")) {
            foundCount++;
            if ((ptr->progressive() || foundCount > 1) && fl->get_int("internal_added") == 237) {
                // If the profile is progressive, field order is redundant: remove
                // Also we only need one field order filter
                m_tractor->detach(*fl.get());
                pCore->currentDoc()->setModified(true);
            } else {
                foFilter = fl;
                foFilter->set("internal_added", 237);
                QString value = ptr->bottom_field_first() ? "bff" : "tff";
                if (foFilter->get("av.order") != value) {
                    pCore->currentDoc()->setModified(true);
                }
                foFilter->set("av.order", value.toUtf8().constData());
            }
        }
    }
    // Build default filter if not found
    if (!ptr->progressive() && foFilter == nullptr) {
        foFilter.reset(new Mlt::Filter(m_tractor->get_profile(), "avfilter.fieldorder"));
        if (foFilter->is_valid()) {
            foFilter->set("internal_added", 237);
            foFilter->set("av.order", ptr->bottom_field_first() ? "bff" : "tff");
            m_tractor->attach(*foFilter.get());
            pCore->currentDoc()->setModified(true);
        }
    }
}

int TimelineModel::getBlankSizeNearClip(int clipId, bool after) const
{
    READ_LOCK();
    Q_ASSERT(m_allClips.count(clipId) > 0);
    int trackId = getClipTrackId(clipId);
    if (trackId != -1) {
        return getTrackById_const(trackId)->getBlankSizeNearClip(clipId, after);
    }
    return 0;
}

int TimelineModel::getPreviousTrackId(int trackId)
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    bool audioWanted = (*it)->isAudioTrack();
    while (it != m_allTracks.cbegin()) {
        --it;
        if ((*it)->isAudioTrack() == audioWanted) {
            return (*it)->getId();
        }
    }
    return trackId;
}

int TimelineModel::getNextTrackId(int trackId)
{
    READ_LOCK();
    Q_ASSERT(isTrack(trackId));
    auto it = m_iteratorTable.at(trackId);
    bool audioWanted = (*it)->isAudioTrack();
    while (it != m_allTracks.cend()) {
        ++it;
        if (it != m_allTracks.cend() && (*it)->isAudioTrack() == audioWanted) {
            break;
        }
    }
    return it == m_allTracks.cend() ? trackId : (*it)->getId();
}

bool TimelineModel::requestClearSelection(bool onDeletion)
{
    QWriteLocker locker(&m_lock);
    TRACE();
    if (m_selectedMix > -1) {
        m_selectedMix = -1;
        Q_EMIT selectedMixChanged(-1, nullptr);
    }
    if (m_currentSelection == -1) {
        TRACE_RES(true);
        return true;
    }
    if (isGroup(m_currentSelection)) {
        // Reset offset display on clips
        std::unordered_set<int> items = m_groups->getLeaves(m_currentSelection);
        for (auto &id : items) {
            if (isGroup(id)) {
                std::unordered_set<int> children = m_groups->getLeaves(id);
                items.insert(children.begin(), children.end());
            } else if (isClip(id)) {
                m_allClips[id]->clearOffset();
                m_allClips[id]->setGrab(false);
                m_allClips[id]->setSelected(false);
            } else if (isComposition(id)) {
                m_allCompositions[id]->setGrab(false);
                m_allCompositions[id]->setSelected(false);
            } else if (isSubTitle(id)) {
                m_subtitleModel->setSelected(id, false);
            }
            if (m_groups->getType(m_currentSelection) == GroupType::Selection) {
                m_groups->destructGroupItem(m_currentSelection);
            }
        }
    } else {
        if (isClip(m_currentSelection)) {
            m_allClips[m_currentSelection]->setGrab(false);
            m_allClips[m_currentSelection]->setSelected(false);
        } else if (isComposition(m_currentSelection)) {
            m_allCompositions[m_currentSelection]->setGrab(false);
            m_allCompositions[m_currentSelection]->setSelected(false);
        } else if (isSubTitle(m_currentSelection)) {
            m_subtitleModel->setSelected(m_currentSelection, false);
        }
        Q_ASSERT(onDeletion || isClip(m_currentSelection) || isComposition(m_currentSelection) || isSubTitle(m_currentSelection));
    }
    m_currentSelection = -1;
    if (m_subtitleModel) {
        m_subtitleModel->clearGrab();
    }
    Q_EMIT selectionChanged();
    TRACE_RES(true);
    return true;
}

void TimelineModel::requestMixSelection(int cid)
{
    requestClearSelection();
    int tid = getItemTrackId(cid);
    if (tid > -1) {
        m_selectedMix = cid;
        Q_EMIT selectedMixChanged(cid, getTrackById_const(tid)->mixModel(cid));
    }
}

void TimelineModel::requestClearSelection(bool onDeletion, Fun &undo, Fun &redo)
{
    Fun operation = [this, onDeletion]() {
        requestClearSelection(onDeletion);
        return true;
    };
    Fun reverse = [this, clips = getCurrentSelection()]() { return requestSetSelection(clips); };
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
    }
}

void TimelineModel::clearGroupSelectionOnDelete(std::vector<int> groups)
{
    READ_LOCK();
    if (std::find(groups.begin(), groups.end(), m_currentSelection) != groups.end()) {
        requestClearSelection(true);
    }
}

std::unordered_set<int> TimelineModel::getCurrentSelection() const
{
    READ_LOCK();
    if (m_currentSelection == -1) {
        return {};
    }
    if (isGroup(m_currentSelection)) {
        return m_groups->getLeaves(m_currentSelection);
    } else {
        Q_ASSERT(isClip(m_currentSelection) || isComposition(m_currentSelection) || isSubTitle(m_currentSelection));
        return {m_currentSelection};
    }
}

void TimelineModel::requestAddToSelection(int itemId, bool clear)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId, clear);
    if (clear) {
        requestClearSelection();
    }
    std::unordered_set<int> selection = getCurrentSelection();
    if (selection.insert(itemId).second) {
        requestSetSelection(selection);
    }
}

void TimelineModel::requestRemoveFromSelection(int itemId)
{
    QWriteLocker locker(&m_lock);
    TRACE(itemId);
    std::unordered_set<int> all_items = {itemId};
    int parentGroup = m_groups->getDirectAncestor(itemId);
    if (parentGroup > -1 && m_groups->getType(parentGroup) != GroupType::Selection) {
        all_items = m_groups->getLeaves(parentGroup);
    }
    std::unordered_set<int> selection = getCurrentSelection();
    for (int current_itemId : all_items) {
        if (selection.count(current_itemId) > 0) {
            selection.erase(current_itemId);
        }
    }
    requestSetSelection(selection);
}

bool TimelineModel::requestSetSelection(const std::unordered_set<int> &ids)
{
    QWriteLocker locker(&m_lock);
    TRACE(ids);
    requestClearSelection();
    // if the items are in groups, we must retrieve their topmost containing groups
    std::unordered_set<int> roots;
    std::transform(ids.begin(), ids.end(), std::inserter(roots, roots.begin()), [&](int id) { return m_groups->getRootId(id); });

    bool result = true;
    if (roots.size() == 0) {
        m_currentSelection = -1;
    } else if (roots.size() == 1) {
        m_currentSelection = *(roots.begin());
        setSelected(m_currentSelection, true);
    } else {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        if (ids.size() == 2) {
            // Check if we selected 2 clips from the same master
            QList<int> pairIds;
            for (auto &id : roots) {
                if (isClip(id)) {
                    pairIds << id;
                }
            }
            if (pairIds.size() == 2 && getClipBinId(pairIds.at(0)) == getClipBinId(pairIds.at(1))) {
                // Check if they have same bin id
                ClipType::ProducerType type = m_allClips[pairIds.at(0)]->clipType();
                if (type == ClipType::AV || type == ClipType::Audio || type == ClipType::Video) {
                    // Both clips have same bin ID, display offset
                    int pos1 = getClipPosition(pairIds.at(0));
                    int pos2 = getClipPosition(pairIds.at(1));
                    if (pos2 > pos1) {
                        int offset = pos2 - getClipIn(pairIds.at(1)) - (pos1 - getClipIn(pairIds.at(0)));
                        if (offset != 0) {
                            m_allClips[pairIds.at(1)]->setOffset(offset);
                            m_allClips[pairIds.at(0)]->setOffset(-offset);
                        }
                    } else {
                        int offset = pos1 - getClipIn(pairIds.at(0)) - (pos2 - getClipIn(pairIds.at(1)));
                        if (offset != 0) {
                            m_allClips[pairIds.at(0)]->setOffset(offset);
                            m_allClips[pairIds.at(1)]->setOffset(-offset);
                        }
                    }
                }
            }
        }
        result = (m_currentSelection = m_groups->groupItems(ids, undo, redo, GroupType::Selection)) >= 0;
        Q_ASSERT(m_currentSelection >= 0);
    }
    if (m_subtitleModel) {
        m_subtitleModel->clearGrab();
    }
    Q_EMIT selectionChanged();
    return result;
}

void TimelineModel::setSelected(int itemId, bool sel)
{
    if (isClip(itemId)) {
        m_allClips[itemId]->setSelected(sel);
    } else if (isComposition(itemId)) {
        m_allCompositions[itemId]->setSelected(sel);
    } else if (isSubTitle(itemId)) {
        m_subtitleModel->setSelected(itemId, sel);
    } else if (isGroup(itemId)) {
        auto leaves = m_groups->getLeaves(itemId);
        for (auto &id : leaves) {
            setSelected(id, true);
        }
    }
}

bool TimelineModel::requestSetSelection(const std::unordered_set<int> &ids, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    Fun reverse = [this]() {
        requestClearSelection(false);
        return true;
    };
    Fun operation = [this, ids]() { return requestSetSelection(ids); };
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

void TimelineModel::lockTrack(int trackId, bool lock)
{
    if (lock) {
        getTrackById(trackId)->lock();
    } else {
        getTrackById(trackId)->unlock();
    }
}

void TimelineModel::setTrackLockedState(int trackId, bool lock)
{
    QWriteLocker locker(&m_lock);
    TRACE(trackId, lock);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };

    Fun lock_lambda = [this, trackId]() {
        lockTrack(trackId, true);
        return true;
    };
    Fun unlock_lambda = [this, trackId]() {
        lockTrack(trackId, false);
        return true;
    };
    if (lock) {
        if (lock_lambda()) {
            UPDATE_UNDO_REDO(lock_lambda, unlock_lambda, undo, redo);
            PUSH_UNDO(undo, redo, i18n("Lock track"));
        }
    } else {
        if (unlock_lambda()) {
            UPDATE_UNDO_REDO(unlock_lambda, lock_lambda, undo, redo);
            PUSH_UNDO(undo, redo, i18n("Unlock track"));
        }
    }
}

std::unordered_set<int> TimelineModel::getAllTracksIds() const
{
    READ_LOCK();
    std::unordered_set<int> result;
    std::transform(m_iteratorTable.begin(), m_iteratorTable.end(), std::inserter(result, result.begin()), [&](const auto &track) { return track.first; });
    return result;
}

void TimelineModel::switchComposition(int cid, const QString &compoId)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    if (isClip(cid)) {
        // We are working on a mix
        requestClearSelection(true);
        int tid = getClipTrackId(cid);
        MixInfo mixData = getTrackById_const(tid)->getMixInfo(cid).first;
        getTrackById(tid)->switchMix(cid, compoId, undo, redo);
        Fun local_update = [cid, mixData, this]() {
            requestMixSelection(cid);
            int in = mixData.secondClipInOut.first;
            int out = mixData.firstClipInOut.second;
            Q_EMIT invalidateZone(in, out);
            checkRefresh(in, out);
            return true;
        };
        PUSH_LAMBDA(local_update, redo);
        PUSH_FRONT_LAMBDA(local_update, undo);
        if (redo()) {
            pCore->pushUndo(undo, redo, i18n("Change composition"));
        }
        return;
    }
    Q_ASSERT(isComposition(cid));
    std::shared_ptr<CompositionModel> compo = m_allCompositions.at(cid);
    int currentPos = compo->getPosition();
    int duration = compo->getPlaytime();
    int currentTrack = compo->getCurrentTrackId();
    int a_track = compo->getATrack();
    int forcedTrack = compo->getForcedTrack();
    // Clear selection
    requestClearSelection(true);
    if (m_groups->isInGroup(cid)) {
        pCore->displayMessage(i18n("Cannot operate on grouped composition, please ungroup"), ErrorMessage);
        return;
    }

    bool res = requestCompositionDeletion(cid, undo, redo);
    int newId = -1;
    // Check if composition should be reversed (top clip at beginning, bottom at end)
    int topClip = getTrackById_const(currentTrack)->getClipByPosition(currentPos);
    int bottomTid = a_track < 1 ? -1 : getTrackIndexFromPosition(a_track - 1);
    int bottomClip = -1;
    if (bottomTid > -1) {
        bottomClip = getTrackById_const(bottomTid)->getClipByPosition(currentPos);
    }
    bool reverse = false;
    if (topClip > -1 && bottomClip > -1) {
        if (getClipPosition(topClip) + getClipPlaytime(topClip) < getClipPosition(bottomClip) + getClipPlaytime(bottomClip)) {
            reverse = true;
        }
    }
    std::unique_ptr<Mlt::Properties> props(nullptr);
    if (reverse) {
        props = std::make_unique<Mlt::Properties>();
        if (compoId == QLatin1String("dissolve")) {
            props->set("reverse", 1);
        } else if (compoId == QLatin1String("composite")) {
            props->set("invert", 1);
        } else if (compoId == QLatin1String("wipe")) {
            props->set("geometry", "0=0% 0% 100% 100% 100%;-1=0% 0% 100% 100% 0%");
        } else if (compoId == QLatin1String("slide")) {
            props->set("rect", "0=0% 0% 100% 100% 100%;-1=100% 0% 100% 100% 100%");
        }
    }
    res = res && requestCompositionInsertion(compoId, currentTrack, a_track, currentPos, duration, std::move(props), newId, undo, redo);
    if (res) {
        if (forcedTrack > -1 && isComposition(newId)) {
            m_allCompositions[newId]->setForceTrack(true);
        }
        Fun local_redo = [newId, this]() {
            requestSetSelection({newId});
            return true;
        };
        Fun local_undo = [cid, this]() {
            requestSetSelection({cid});
            return true;
        };
        local_redo();
        PUSH_LAMBDA(local_redo, redo);
        PUSH_LAMBDA(local_undo, undo);
        PUSH_UNDO(undo, redo, i18n("Change composition"));
    } else {
        undo();
    }
}

bool TimelineModel::plantMix(int tid, Mlt::Transition *t)
{
    if (getTrackById_const(tid)->hasClipStart(t->get_in())) {
        int a_track = t->get_a_track();
        int b_track = t->get_b_track();
        getTrackById_const(tid)->getTrackService()->plant_transition(*t, a_track, b_track);
        return getTrackById_const(tid)->loadMix(t);
    } else {
        qDebug() << "=== INVALID MIX FOUND AT: " << t->get_in() << " - " << t->get("mlt_service");
        return false;
    }
}

bool TimelineModel::resizeStartMix(int cid, int duration, bool singleResize)
{
    Q_ASSERT(isClip(cid));
    int tid = m_allClips.at(cid)->getCurrentTrackId();
    if (tid > -1) {
        std::pair<MixInfo, MixInfo> mixData = getTrackById_const(tid)->getMixInfo(cid);
        if (mixData.first.firstClipId > -1) {
            int clipToResize = mixData.first.firstClipId;
            Q_ASSERT(isClip(clipToResize));
            duration = qMin(duration, m_allClips.at(cid)->getPlaytime());
            int updatedDuration = m_allClips.at(cid)->getPosition() + duration - m_allClips[clipToResize]->getPosition();
            int result = requestItemResize(clipToResize, updatedDuration, true, true, 0, singleResize);
            return result > -1;
        }
    }
    return false;
}

int TimelineModel::getMixDuration(int cid) const
{
    Q_ASSERT(isClip(cid));
    int tid = m_allClips.at(cid)->getCurrentTrackId();
    if (tid > -1) {
        if (getTrackById_const(tid)->hasStartMix(cid)) {
            return getTrackById_const(tid)->getMixDuration(cid);
        } else {
            // Mix is not yet inserted in timeline
            std::pair<int, int> mixInOut = getMixInOut(cid);
            return mixInOut.second - mixInOut.first;
        }
    }
    return 0;
}

std::pair<int, int> TimelineModel::getMixInOut(int cid) const
{
    Q_ASSERT(isClip(cid));
    int tid = m_allClips.at(cid)->getCurrentTrackId();
    if (tid > -1) {
        MixInfo mixData = getTrackById_const(tid)->getMixInfo(cid).first;
        if (mixData.firstClipId > -1) {
            return {mixData.secondClipInOut.first, mixData.firstClipInOut.second};
        }
    }
    return {-1, -1};
}

int TimelineModel::getMixCutPos(int cid) const
{
    Q_ASSERT(isClip(cid));
    return m_allClips.at(cid)->getMixCutPosition();
}

MixAlignment TimelineModel::getMixAlign(int cid) const
{
    Q_ASSERT(isClip(cid));
    int tid = m_allClips.at(cid)->getCurrentTrackId();
    if (tid > -1) {
        int mixDuration = m_allClips.at(cid)->getMixDuration();
        int mixCutPos = m_allClips.at(cid)->getMixCutPosition();
        if (mixCutPos == 0) {
            return MixAlignment::AlignRight;
        } else if (mixCutPos == mixDuration) {
            return MixAlignment::AlignLeft;
        } else if (mixCutPos == mixDuration - mixDuration / 2) {
            return MixAlignment::AlignCenter;
        }
    }
    return MixAlignment::AlignNone;
}

void TimelineModel::requestResizeMix(int cid, int duration, MixAlignment align, int leftFrames)
{
    Q_ASSERT(isClip(cid));
    int tid = m_allClips.at(cid)->getCurrentTrackId();
    if (tid > -1) {
        MixInfo mixData = getTrackById_const(tid)->getMixInfo(cid).first;
        int clipToResize = mixData.firstClipId;
        if (clipToResize > -1) {
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            // The mix cut position shoud never change through a resize operation
            int cutPos = m_allClips.at(clipToResize)->getPosition() + m_allClips.at(clipToResize)->getPlaytime() - m_allClips.at(cid)->getMixCutPosition();
            int maxLengthLeft = m_allClips.at(clipToResize)->getMaxDuration();
            // Maximum space for expanding the right clip part
            int leftMax = maxLengthLeft > -1 ? (maxLengthLeft - 1 - m_allClips.at(clipToResize)->getOut()) : -1;
            // Maximum space available on the right clip
            int availableLeft = m_allClips.at(cid)->getPosition() + m_allClips.at(cid)->getPlaytime() -
                                (m_allClips.at(clipToResize)->getPosition() + m_allClips.at(clipToResize)->getPlaytime());
            if (leftMax == -1) {
                leftMax = availableLeft;
            } else {
                leftMax = qMin(leftMax, availableLeft);
            }

            int maxLengthRight = m_allClips.at(cid)->getMaxDuration();
            // maximum space to resize clip on the left
            int availableRight = m_allClips.at(cid)->getPosition() - m_allClips.at(clipToResize)->getPosition();
            int rightMax = maxLengthRight > -1 ? (m_allClips.at(cid)->getIn()) : -1;
            if (rightMax == -1) {
                rightMax = availableRight;
            } else {
                rightMax = qMin(rightMax, availableRight);
            }
            Fun adjust_mix_undo = [this, tid, cid, prevCut = m_allClips.at(cid)->getMixCutPosition(), prevDuration = m_allClips.at(cid)->getMixDuration()]() {
                getTrackById_const(tid)->setMixDuration(cid, prevDuration, prevCut);
                QModelIndex ix = makeClipIndexFromID(cid);
                Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                return true;
            };
            if (align == MixAlignment::AlignLeft) {
                // Adjust left clip
                int updatedDurationLeft = cutPos + duration - m_allClips.at(clipToResize)->getPosition();
                if (leftMax > -1) {
                    updatedDurationLeft = qMin(updatedDurationLeft, m_allClips.at(clipToResize)->getPlaytime() + leftMax);
                }
                // Adjust right clip
                int updatedDurationRight = m_allClips.at(cid)->getPlaytime();
                if (cutPos != m_allClips.at(cid)->getPosition()) {
                    updatedDurationRight = m_allClips.at(cid)->getPosition() + m_allClips.at(cid)->getPlaytime() - cutPos;
                    if (rightMax > -1) {
                        updatedDurationRight = qMin(updatedDurationRight, m_allClips.at(cid)->getPlaytime() + rightMax);
                    }
                }
                int updatedDuration = m_allClips.at(clipToResize)->getPosition() + updatedDurationLeft -
                                      (m_allClips.at(cid)->getPosition() + m_allClips.at(cid)->getPlaytime() - updatedDurationRight);
                if (updatedDuration < 1) {
                    //
                    pCore->displayMessage(i18n("Cannot resize mix to less than 1 frame"), ErrorMessage, 500);
                    // update mix widget
                    Q_EMIT selectedMixChanged(cid, getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                requestItemResize(clipToResize, updatedDurationLeft, true, true, undo, redo);
                if (m_allClips.at(cid)->getPlaytime() != updatedDurationRight) {
                    requestItemResize(cid, updatedDurationRight, false, true, undo, redo);
                }
                int updatedCutPosition = m_allClips.at(cid)->getPosition();
                if (updatedCutPosition != cutPos) {
                    pCore->displayMessage(i18n("Cannot resize mix"), ErrorMessage, 500);
                    undo();
                    Q_EMIT selectedMixChanged(cid, getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                Fun adjust_mix = [this, tid, cid, updatedDuration]() {
                    getTrackById_const(tid)->setMixDuration(cid, updatedDuration, updatedDuration);
                    QModelIndex ix = makeClipIndexFromID(cid);
                    Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                    return true;
                };
                adjust_mix();
                UPDATE_UNDO_REDO(adjust_mix, adjust_mix_undo, undo, redo);
            } else if (align == MixAlignment::AlignRight) {
                int updatedDurationRight = m_allClips.at(cid)->getPosition() + m_allClips.at(cid)->getPlaytime() - cutPos + duration;
                if (rightMax > -1) {
                    updatedDurationRight = qMin(updatedDurationRight, m_allClips.at(cid)->getPlaytime() + rightMax);
                }
                int updatedDurationLeft = cutPos - m_allClips.at(clipToResize)->getPosition();
                if (leftMax > -1) {
                    updatedDurationLeft = qMin(updatedDurationLeft, m_allClips.at(clipToResize)->getPlaytime() + leftMax);
                }
                int updatedDuration = m_allClips.at(clipToResize)->getPosition() + updatedDurationLeft -
                                      (m_allClips.at(cid)->getPosition() + m_allClips.at(cid)->getPlaytime() - updatedDurationRight);
                if (updatedDuration < 1) {
                    //
                    pCore->displayMessage(i18n("Cannot resize mix to less than 1 frame"), ErrorMessage, 500);
                    Q_EMIT selectedMixChanged(cid, getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                requestItemResize(cid, updatedDurationRight, false, true, undo, redo);
                requestItemResize(clipToResize, updatedDurationLeft, true, true, undo, redo);
                int updatedCutPosition = m_allClips.at(clipToResize)->getPosition() + m_allClips.at(clipToResize)->getPlaytime();
                if (updatedCutPosition != cutPos) {
                    pCore->displayMessage(i18n("Cannot resize mix"), ErrorMessage, 500);
                    undo();
                    Q_EMIT selectedMixChanged(cid, getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                Fun adjust_mix = [this, tid, cid, updatedDuration]() {
                    getTrackById_const(tid)->setMixDuration(cid, updatedDuration, 0);
                    QModelIndex ix = makeClipIndexFromID(cid);
                    Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                    return true;
                };
                adjust_mix();
                UPDATE_UNDO_REDO(adjust_mix, adjust_mix_undo, undo, redo);
            } else if (align == MixAlignment::AlignCenter) {
                int updatedDurationRight = m_allClips.at(cid)->getPosition() + m_allClips.at(cid)->getPlaytime() - cutPos + duration / 2;
                if (rightMax > -1) {
                    updatedDurationRight = qMin(updatedDurationRight, m_allClips.at(cid)->getPlaytime() + rightMax);
                }
                int updatedDurationLeft = cutPos + (duration - duration / 2) - m_allClips.at(clipToResize)->getPosition();
                if (leftMax > -1) {
                    updatedDurationLeft = qMin(updatedDurationLeft, m_allClips.at(clipToResize)->getPlaytime() + leftMax);
                }
                int updatedDuration = m_allClips.at(clipToResize)->getPosition() + updatedDurationLeft -
                                      (m_allClips.at(cid)->getPosition() + m_allClips.at(cid)->getPlaytime() - updatedDurationRight);
                if (updatedDuration < 1) {
                    pCore->displayMessage(i18n("Cannot resize mix to less than 1 frame"), ErrorMessage, 500);
                    Q_EMIT selectedMixChanged(cid, getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                int deltaLeft = m_allClips.at(clipToResize)->getPosition() + updatedDurationLeft - cutPos;
                int deltaRight = cutPos - (m_allClips.at(cid)->getPosition() + m_allClips.at(cid)->getPlaytime() - updatedDurationRight);

                if (deltaRight) {
                    if (!requestItemResize(cid, updatedDurationRight, false, true, undo, redo)) {
                        qDebug() << ":::: ERROR RESIZING CID1\n\nAAAAAAAAAAAAAAAAAAAA";
                    }
                }
                if (deltaLeft > 0) {
                    if (!requestItemResize(clipToResize, updatedDurationLeft, true, true, undo, redo)) {
                        qDebug() << ":::: ERROR RESIZING clipToResize\n\nAAAAAAAAAAAAAAAAAAAA";
                    }
                }
                int mixCutPos = m_allClips.at(clipToResize)->getPosition() + m_allClips.at(clipToResize)->getPlaytime() - cutPos;
                if (mixCutPos > updatedDuration) {
                    pCore->displayMessage(i18n("Cannot resize mix"), ErrorMessage, 500);
                    undo();
                    Q_EMIT selectedMixChanged(cid, getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                if (qAbs(deltaLeft - deltaRight) > 2) {
                    // Mix not exactly centered
                    Q_EMIT selectedMixChanged(cid, getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                Fun adjust_mix = [this, tid, cid, updatedDuration, mixCutPos]() {
                    getTrackById_const(tid)->setMixDuration(cid, updatedDuration, mixCutPos);
                    QModelIndex ix = makeClipIndexFromID(cid);
                    Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                    return true;
                };
                adjust_mix();
                UPDATE_UNDO_REDO(adjust_mix, adjust_mix_undo, undo, redo);
            } else {
                // No alignment specified
                int updatedDurationRight;
                int updatedDurationLeft;
                if (leftFrames > -1) {
                    // A right frame offset was specified
                    updatedDurationLeft = qBound(0, leftFrames, duration);
                    updatedDurationRight = duration - updatedDurationLeft;
                } else {
                    updatedDurationRight = m_allClips.at(cid)->getMixCutPosition();
                    updatedDurationLeft = m_allClips.at(cid)->getMixDuration() - updatedDurationRight;
                    int currentDuration = m_allClips.at(cid)->getMixDuration();
                    if (qAbs(duration - currentDuration) == 1) {
                        if (duration < currentDuration) {
                            // We are reducing the duration
                            if (currentDuration % 2 == 0) {
                                updatedDurationRight--;
                                if (updatedDurationRight < 0) {
                                    updatedDurationRight = 0;
                                    updatedDurationLeft--;
                                }
                            } else {
                                updatedDurationLeft--;
                                if (updatedDurationLeft < 0) {
                                    updatedDurationLeft = 0;
                                    updatedDurationRight--;
                                }
                            }
                        } else {
                            // Increasing duration
                            if (currentDuration % 2 == 0) {
                                updatedDurationRight++;
                            } else {
                                updatedDurationLeft++;
                            }
                        }
                    } else {
                        double ratio = double(duration) / currentDuration;
                        updatedDurationRight *= ratio;
                        updatedDurationLeft = duration - updatedDurationRight;
                    }
                }
                if (updatedDurationLeft + updatedDurationRight < 1) {
                    //
                    pCore->displayMessage(i18n("Cannot resize mix to less than 1 frame"), ErrorMessage, 500);
                    Q_EMIT selectedMixChanged(cid, getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                updatedDurationLeft -= (m_allClips.at(cid)->getMixDuration() - m_allClips.at(cid)->getMixCutPosition());
                updatedDurationRight -= m_allClips.at(cid)->getMixCutPosition();
                if (leftMax > -1) {
                    updatedDurationLeft = qMin(updatedDurationLeft, m_allClips.at(clipToResize)->getPlaytime() + leftMax);
                }
                if (rightMax > -1) {
                    updatedDurationRight = qMin(updatedDurationRight, m_allClips.at(cid)->getPlaytime() + rightMax);
                }
                if (updatedDurationLeft != 0) {
                    int updatedDurL = m_allClips.at(cid)->getPlaytime() + updatedDurationLeft;
                    requestItemResize(cid, updatedDurL, false, true, undo, redo);
                }
                if (updatedDurationRight != 0) {
                    int updatedDurR = m_allClips.at(clipToResize)->getPlaytime() + updatedDurationRight;
                    requestItemResize(clipToResize, updatedDurR, true, true, undo, redo);
                }
                int mixCutPos = m_allClips.at(clipToResize)->getPosition() + m_allClips.at(clipToResize)->getPlaytime() - cutPos;
                int updatedDuration =
                    m_allClips.at(clipToResize)->getPosition() + m_allClips.at(clipToResize)->getPlaytime() - m_allClips.at(cid)->getPosition();
                if (mixCutPos > updatedDuration) {
                    pCore->displayMessage(i18n("Cannot resize mix"), ErrorMessage, 500);
                    undo();
                    Q_EMIT selectedMixChanged(cid, getTrackById_const(tid)->mixModel(cid), true);
                    return;
                }
                Fun adjust_mix = [this, tid, cid, updatedDuration, mixCutPos]() {
                    getTrackById_const(tid)->setMixDuration(cid, updatedDuration, mixCutPos);
                    QModelIndex ix = makeClipIndexFromID(cid);
                    Q_EMIT dataChanged(ix, ix, {TimelineModel::MixRole, TimelineModel::MixCutRole});
                    return true;
                };
                adjust_mix();
                UPDATE_UNDO_REDO(adjust_mix, adjust_mix_undo, undo, redo);
            }
            pCore->pushUndo(undo, redo, i18n("Resize mix"));
        }
    }
}

int TimelineModel::getSubtitleIndex(int subId) const
{
    if (m_allSubtitles.count(subId) == 0) {
        return -1;
    }
    auto it = m_allSubtitles.find(subId);
    return int(std::distance(m_allSubtitles.begin(), it));
}

std::pair<int, GenTime> TimelineModel::getSubtitleIdFromIndex(int index) const
{
    if (index >= static_cast<int>(m_allSubtitles.size())) {
        return {-1, GenTime()};
    }
    auto it = m_allSubtitles.begin();
    std::advance(it, index);
    return {it->first, it->second};
}

QVariantList TimelineModel::getMasterEffectZones() const
{
    if (m_masterStack) {
        return m_masterStack->getEffectZones();
    }
    return {};
}

const QSize TimelineModel::getCompositionSizeOnTrack(const ObjectId &id)
{
    int pos = getCompositionPosition(id.itemId);
    int tid = getCompositionTrackId(id.itemId);
    int cid = getTrackById_const(tid)->getClipByPosition(pos);
    if (cid > -1) {
        return getClipFrameSize(cid);
    }
    return QSize();
}

QStringList TimelineModel::getProxiesAt(int position)
{
    QStringList done;
    QStringList proxied;
    auto it = m_allTracks.begin();
    while (it != m_allTracks.end()) {
        if ((*it)->isAudioTrack()) {
            ++it;
            continue;
        }
        int clip1 = (*it)->getClipByPosition(position, 0);
        int clip2 = (*it)->getClipByPosition(position, 1);
        if (clip1 > -1) {
            // Check if proxied
            const QString binId = m_allClips[clip1]->binId();
            if (!done.contains(binId)) {
                done << binId;
                std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(binId);
                if (binClip->hasProxy()) {
                    proxied << binId;
                }
            }
        }
        if (clip2 > -1) {
            // Check if proxied
            const QString binId = m_allClips[clip2]->binId();
            if (!done.contains(binId)) {
                done << binId;
                std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(binId);
                if (binClip->hasProxy()) {
                    proxied << binId;
                }
            }
        }
        ++it;
    }
    return proxied;
}

QByteArray TimelineModel::timelineHash()
{
    QByteArray fileData;
    // Get track hashes
    auto it = m_allTracks.begin();
    while (it != m_allTracks.end()) {
        fileData.append((*it)->trackHash());
        ++it;
    }
    // Compositions hash
    for (auto &compo : m_allCompositions) {
        int track = getTrackPosition(compo.second->getCurrentTrackId());
        QString compoData = QString("%1 %2 %3 %4")
                                .arg(QString::number(compo.second->getATrack()), QString::number(track), QString::number(compo.second->getPosition()),
                                     QString::number(compo.second->getPlaytime()));
        compoData.append(compo.second->getAssetId());
        fileData.append(compoData.toLatin1());
    }
    // Guides
    if (m_guidesModel) {
        QString guidesData = m_guidesModel->toJson();
        fileData.append(guidesData.toUtf8().constData());
    }
    QByteArray fileHash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
    return fileHash;
}

std::shared_ptr<MarkerSortModel> TimelineModel::getFilteredGuideModel()
{
    return m_guidesFilterModel;
}

std::shared_ptr<MarkerListModel> TimelineModel::getGuideModel()
{
    return m_guidesModel;
}

bool TimelineModel::trackIsLocked(int trackId) const
{
    if (isSubtitleTrack(trackId)) {
        return m_subtitleModel->isLocked();
    }
    Q_ASSERT(isTrack(trackId));
    return getTrackById_const(trackId)->isLocked();
}

const QUuid TimelineModel::uuid() const
{
    return m_uuid;
}

void TimelineModel::initializePreviewManager()
{
    if (m_timelinePreview == nullptr) {
        m_timelinePreview = std::shared_ptr<PreviewManager>(new PreviewManager(m_tractor.get(), m_uuid, this));
        bool initialized = m_timelinePreview->initialize();
        if (!initialized) {
            pCore->displayMessage(i18n("Error initializing timeline preview"), ErrorMessage);
            m_timelinePreview.reset();
            return;
        }
        Q_EMIT connectPreviewManager();
        connect(this, &TimelineModel::invalidateZone, m_timelinePreview.get(), &PreviewManager::invalidatePreview, Qt::DirectConnection);
    }
}

std::shared_ptr<PreviewManager> TimelineModel::previewManager()
{
    return m_timelinePreview;
}

void TimelineModel::resetPreviewManager()
{
    if (m_timelinePreview) {
        disconnect(this, &TimelineModel::invalidateZone, m_timelinePreview.get(), &PreviewManager::invalidatePreview);
        m_timelinePreview.reset();
    }
}

bool TimelineModel::hasTimelinePreview() const
{
    return m_timelinePreview != nullptr;
}

void TimelineModel::updatePreviewConnection(bool enable)
{
    if (hasTimelinePreview()) {
        if (enable) {
            m_timelinePreview->enable();
        } else {
            m_timelinePreview->disable();
        }
    }
}

bool TimelineModel::buildPreviewTrack()
{
    bool res = false;
    if (m_timelinePreview) {
        res = m_timelinePreview->buildPreviewTrack();
        m_overlayTrackCount = m_timelinePreview->addedTracks();
    }
    return res;
}

void TimelineModel::setOverlayTrack(Mlt::Playlist *overlay)
{
    if (m_timelinePreview) {
        m_timelinePreview->setOverlayTrack(overlay);
        m_overlayTrackCount = m_timelinePreview->addedTracks();
    }
}

void TimelineModel::removeOverlayTrack()
{
    if (m_timelinePreview) {
        m_timelinePreview->removeOverlayTrack();
        m_overlayTrackCount = m_timelinePreview->addedTracks();
    }
}

void TimelineModel::deletePreviewTrack()
{
    if (m_timelinePreview) {
        m_timelinePreview->deletePreviewTrack();
        m_overlayTrackCount = m_timelinePreview->addedTracks();
    }
}

bool TimelineModel::hasSubtitleModel()
{
    return m_subtitleModel != nullptr;
}

void TimelineModel::makeTransparentBg(bool transparent)
{
    m_blackClip->lock();
    if (transparent) {
        m_blackClip->set("resource", 0);
    } else {
        m_blackClip->set("resource", "black");
    }
    m_blackClip->unlock();
}

void TimelineModel::prepareShutDown()
{
    m_softDelete = true;
}

void TimelineModel::updateVisibleSequenceName(const QString displayName)
{
    m_visibleSequenceName = displayName;
    Q_EMIT visibleSequenceNameChanged();
}

void TimelineModel::registerTimeline()
{
    qDebug() << "::: CLIPS IN THIS MODDEL: " << m_allClips.size();
    for (auto clip : m_allClips) {
        clip.second->registerClipToBin(clip.second->getProducer(), false);
    }
}

void TimelineModel::loadPreview(const QString &chunks, const QString &dirty, bool enable, Mlt::Playlist &playlist)
{
    if (chunks.isEmpty() && dirty.isEmpty()) {
        return;
    }
    if (!hasTimelinePreview()) {
        initializePreviewManager();
    }
    QVariantList renderedChunks;
    QVariantList dirtyChunks;
    QStringList chunksList = chunks.split(QLatin1Char(','), Qt::SkipEmptyParts);
    QStringList dirtyList = dirty.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &frame : qAsConst(chunksList)) {
        if (frame.contains(QLatin1Char('-'))) {
            // Range, process
            int start = frame.section(QLatin1Char('-'), 0, 0).toInt();
            int end = frame.section(QLatin1Char('-'), 1, 1).toInt();
            for (int i = start; i <= end; i += 25) {
                renderedChunks << i;
            }
        } else {
            renderedChunks << frame.toInt();
        }
    }
    for (const QString &frame : qAsConst(dirtyList)) {
        if (frame.contains(QLatin1Char('-'))) {
            // Range, process
            int start = frame.section(QLatin1Char('-'), 0, 0).toInt();
            int end = frame.section(QLatin1Char('-'), 1, 1).toInt();
            for (int i = start; i <= end; i += 25) {
                dirtyChunks << i;
            }
        } else {
            dirtyChunks << frame.toInt();
        }
    }

    if (hasTimelinePreview()) {
        if (!enable) {
            buildPreviewTrack();
        }
        previewManager()->loadChunks(renderedChunks, dirtyChunks, playlist);
    }
}

bool TimelineModel::clipIsAudio(int cid) const
{
    if (isClip(cid)) {
        int tid = getClipTrackId(cid);
        if (tid > -1) {
            return getTrackById_const(tid)->isAudioTrack();
        }
    }
    return false;
}
