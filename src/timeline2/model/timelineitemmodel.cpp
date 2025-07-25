/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "timelineitemmodel.hpp"
#include "assets/keyframes/model/keyframemodel.hpp"
#include "audiomixer/mixermanager.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "bin/model/subtitlemodel.hpp"
#include "clipmodel.hpp"
#include "compositionmodel.hpp"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "effects/effectstack/model/abstracteffectitem.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "groupsmodel.hpp"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "snapmodel.hpp"
#include "timeline2/view/previewmanager.h"
#include "trackmodel.hpp"
#include "transitions/transitionsrepository.hpp"
#include <QDebug>
#include <QFileInfo>
#include <mlt++/MltField.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTractor.h>
#include <mlt++/MltTransition.h>

#ifdef CRASH_AUTO_TEST
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include <rttr/registration>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

RTTR_REGISTRATION
{
    using namespace rttr;
    registration::class_<TimelineItemModel>("TimelineItemModel");
}
#endif

TimelineItemModel::TimelineItemModel(const QUuid &uuid, std::weak_ptr<DocUndoStack> undo_stack)
    : TimelineModel(uuid, std::move(undo_stack))
{
}

void TimelineItemModel::finishConstruct(const std::shared_ptr<TimelineItemModel> &ptr)
{
    ptr->weak_this_ = ptr;
    ptr->m_groups = std::make_unique<GroupsModel>(ptr);
}

std::shared_ptr<TimelineItemModel> TimelineItemModel::construct(const QUuid &uuid, std::weak_ptr<DocUndoStack> undo_stack)
{
    std::shared_ptr<TimelineItemModel> ptr(new TimelineItemModel(uuid, std::move(undo_stack)));
    finishConstruct(ptr);
    return ptr;
}

TimelineItemModel::~TimelineItemModel() = default;

QModelIndex TimelineItemModel::index(int row, int column, const QModelIndex &parent) const
{
    READ_LOCK();
    QModelIndex result;
    if (parent.isValid()) {
        auto trackId = int(parent.internalId());
        Q_ASSERT(isTrack(trackId));
        int clipId = getTrackById_const(trackId)->getClipByRow(row);
        if (clipId != -1) {
            result = createIndex(row, 0, quintptr(clipId));
        } else if (row < getTrackClipsCount(trackId) + getTrackCompositionsCount(trackId)) {
            int compoId = getTrackById_const(trackId)->getCompositionByRow(row);
            if (compoId != -1) {
                result = createIndex(row, 0, quintptr(compoId));
            }
        } else {
            // Invalid index requested
            Q_ASSERT(false);
        }
    } else if (row < int(m_allTracks.size()) && row >= 0) {
        // Get sort order
        // row = getTracksCount() - 1 - row;
        auto it = m_allTracks.cbegin();
        std::advance(it, row);
        int trackId = (*it)->getId();
        result = createIndex(row, column, quintptr(trackId));
    }
    return result;
}

/*QModelIndex TimelineItemModel::makeIndex(int trackIndex, int clipIndex) const
{
    return index(clipIndex, 0, index(trackIndex));
}*/

bool TimelineItemModel::addTracksAtPosition(int position, int tracksCount, QString &trackName, bool addAudioTrack, bool addAVTrack, bool addRecTrack)
{
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    bool result = true;

    int insertionIndex = position;
    Fun clean_compositing = [this]() {
        removeTrackCompositing();
        return true;
    };
    clean_compositing();
    int newTid;
    for (int ix = 0; ix < tracksCount; ++ix) {
        result = requestTrackInsertion(insertionIndex, newTid, trackName, addAudioTrack, undo, redo, false);
        // bump up insertion index so that the next new track goes after this one
        insertionIndex++;
        if (result) {
            if (addAVTrack) {
                int newTid2;
                int mirrorPos = 0;
                int mirrorId = getMirrorAudioTrackId(newTid);
                if (mirrorId > -1) {
                    mirrorPos = getTrackMltIndex(mirrorId);
                }
                result = requestTrackInsertion(mirrorPos, newTid2, trackName, true, undo, redo, false);
                // because we also added an audio track, we need to put the next
                // new track's index is 1 further
                insertionIndex++;
            }
        } else {
            break;
        }
    }
    if (result) {
        Fun local_redo = [this, addRecTrack, newTid]() {
            buildTrackCompositing(true);
            if (addRecTrack) {
                pCore->mixer()->monitorAudio(newTid, true);
            }
            return true;
        };
        local_redo();
        PUSH_FRONT_LAMBDA(clean_compositing, redo);
        PUSH_FRONT_LAMBDA(clean_compositing, undo);
        PUSH_LAMBDA(local_redo, redo);
        PUSH_LAMBDA(local_redo, undo);
        pCore->pushUndo(undo, redo, addAVTrack || tracksCount > 1 ? i18nc("@action", "Insert Tracks") : i18nc("@action", "Insert Track"));
        return true;
    } else {
        undo();
        buildTrackCompositing(true);
        return false;
    }
}

QModelIndex TimelineItemModel::makeClipIndexFromID(int clipId) const
{
    Q_ASSERT(m_allClips.count(clipId) > 0);
    int trackId = m_allClips.at(clipId)->getCurrentTrackId();
    if (trackId == -1) {
        // Clip is not inserted in a track
        qDebug() << "/// WARNING; INVALID CLIP INDEX REQUESTED: " << clipId << "\n________________";
        return {};
    }
    int row = getTrackById_const(trackId)->getRowfromClip(clipId);
    return index(row, 0, makeTrackIndexFromID(trackId));
}

QModelIndex TimelineItemModel::makeCompositionIndexFromID(int compoId) const
{
    Q_ASSERT(m_allCompositions.count(compoId) > 0);
    int trackId = m_allCompositions.at(compoId)->getCurrentTrackId();
    return index(getTrackById_const(trackId)->getRowfromComposition(compoId), 0, makeTrackIndexFromID(trackId));
}

void TimelineItemModel::subtitleChanged(int subId, const QVector<int> &roles)
{
    if (m_closing) {
        return;
    }
    Q_ASSERT(m_subtitleModel != nullptr);
    Q_ASSERT(m_subtitleModel->hasSubtitle(subId));
    m_subtitleModel->updateSub(subId, roles);
}

QModelIndex TimelineItemModel::makeTrackIndexFromID(int trackId) const
{
    // we retrieve iterator
    Q_ASSERT(m_iteratorTable.count(trackId) > 0);
    auto it = m_iteratorTable.at(trackId);
    int ind = int(std::distance<decltype(m_allTracks.cbegin())>(m_allTracks.begin(), it));
    // Get sort order
    // ind = getTracksCount() - 1 - ind;
    return index(ind);
}

QModelIndex TimelineItemModel::parent(const QModelIndex &index) const
{
    READ_LOCK();
    if (index == QModelIndex()) {
        return index;
    }
    const int id = static_cast<int>(index.internalId());
    if (!index.isValid() || isTrack(id)) {
        return QModelIndex();
    }
    if (isClip(id)) {
        const int trackId = getClipTrackId(id);
        return makeTrackIndexFromID(trackId);
    }
    if (isComposition(id)) {
        const int trackId = getCompositionTrackId(id);
        return makeTrackIndexFromID(trackId);
    }
    return {};
}

int TimelineItemModel::rowCount(const QModelIndex &parent) const
{
    READ_LOCK();
    if (parent.isValid()) {
        const int id = int(parent.internalId());
        if (!isTrack(id)) {
            // clips don't have children
            // if it is not a track, it is something invalid
            return 0;
        }
        return getTrackClipsCount(id) + getTrackCompositionsCount(id);
    }
    return int(m_allTracks.size());
}

int TimelineItemModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QHash<int, QByteArray> TimelineItemModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[ResourceRole] = "resource";
    roles[ServiceRole] = "mlt_service";
    roles[BinIdRole] = "binId";
    roles[TrackIdRole] = "trackId";
    roles[TagRole] = "tag";
    roles[FakeTrackIdRole] = "fakeTrackId";
    roles[FakePositionRole] = "fakePosition";
    roles[StartRole] = "start";
    roles[MixRole] = "mixDuration";
    roles[MixCutRole] = "mixCut";
    roles[DurationRole] = "duration";
    roles[MaxDurationRole] = "maxDuration";
    roles[MarkersRole] = "markers";
    roles[KeyframesRole] = "keyframeModel";
    roles[ShowKeyframesRole] = "showKeyframes";
    roles[PlaylistStateRole] = "clipState";
    roles[StatusRole] = "clipStatus";
    roles[TypeRole] = "clipType";
    roles[InPointRole] = "in";
    roles[OutPointRole] = "out";
    roles[FramerateRole] = "fps";
    roles[GroupedRole] = "grouped";
    roles[IsDisabledRole] = "disabled";
    roles[IsAudioRole] = "audio";
    roles[AudioChannelsRole] = "audioChannels";
    roles[AudioStreamRole] = "audioStream";
    roles[AudioMultiStreamRole] = "multiStream";
    roles[AudioStreamIndexRole] = "audioStreamIndex";
    roles[IsCompositeRole] = "composite";
    roles[IsLockedRole] = "locked";
    roles[FadeInRole] = "fadeIn";
    roles[FadeOutRole] = "fadeOut";
    roles[FadeInMethodRole] = "fadeInMethod";
    roles[FadeOutMethodRole] = "fadeOutMethod";
    roles[FileHashRole] = "hash";
    roles[SpeedRole] = "speed";
    roles[TimeRemapRole] = "timeremap";
    roles[HeightRole] = "trackHeight";
    roles[TrackTagRole] = "trackTag";
    roles[ItemIdRole] = "item";
    roles[ItemATrack] = "a_track";
    roles[HasAudio] = "hasAudio";
    roles[CanBeAudioRole] = "canBeAudio";
    roles[CanBeVideoRole] = "canBeVideo";
    roles[ClipThumbRole] = "clipThumbId";
    roles[ReloadAudioThumbRole] = "reloadAudioThumb";
    roles[PositionOffsetRole] = "positionOffset";
    roles[ThumbsFormatRole] = "thumbsFormat";
    roles[AudioRecordRole] = "audioRecord";
    roles[TrackActiveRole] = "trackActive";
    roles[EffectNamesRole] = "effectNames";
    roles[EffectsEnabledRole] = "isStackEnabled";
    roles[EffectZonesRole] = "effectZones";
    roles[GrabbedRole] = "isGrabbed";
    roles[SelectedRole] = "selected";
    return roles;
}

QVariant TimelineItemModel::data(const QModelIndex &index, int role) const
{
    READ_LOCK();
    if (!m_tractor || !index.isValid()) {
        return QVariant();
    }
    const int id = int(index.internalId());
    if (role == ItemIdRole) {
        return id;
    }
    if (role == SortRole) {
        if (isTrack(id)) {
            return getTrackSortValue(id, KdenliveSettings::audiotracksbelow());
        }
        return QVariant();
    }
    if (isClip(id)) {
        std::shared_ptr<ClipModel> clip = m_allClips.at(id);
        // Get data for a clip
        switch (role) {
        // TODO
        case NameRole:
        case Qt::DisplayRole: {
            return clip->clipName();
        }
        case ResourceRole: {
            QString result = clip->getProperty("resource");
            if (result == QLatin1String("<producer>")) {
                result = clip->getProperty("mlt_service");
            }
            return result;
        }
        case StatusRole: {
            return clip->clipStatus();
        }
        case FakeTrackIdRole:
            return clip->getFakeTrackId();
        case FakePositionRole:
            return clip->getFakePosition();
        case BinIdRole:
            return clip->binId();
        case TrackIdRole:
            return clip->getCurrentTrackId();
        case ServiceRole:
            return clip->getProperty("mlt_service");
            break;
        case AudioChannelsRole:
            return clip->audioChannels();
        case AudioStreamRole:
            return clip->audioStream();
        case AudioMultiStreamRole:
            return clip->audioMultiStream();
        case AudioStreamIndexRole:
            return clip->audioStreamIndex();
        case HasAudio:
            return clip->audioEnabled();
        case IsAudioRole:
            return clip->isAudioOnly();
        case CanBeAudioRole:
            return clip->canBeAudio();
        case CanBeVideoRole:
            return clip->canBeVideo();
        case MarkersRole: {
            return QVariant::fromValue<MarkerListModel *>(clip->getMarkerModel().get());
        }
        case KeyframesRole: {
            return QVariant::fromValue<KeyframeModel *>(clip->getKeyframeModel());
        }
        case PlaylistStateRole:
            return QVariant::fromValue(clip->clipState());
        case TypeRole:
            return QVariant::fromValue(clip->clipType());
        case StartRole:
            return clip->getPosition();
        case DurationRole:
            return clip->getPlaytime();
        case MaxDurationRole:
            return clip->getMaxDuration();
        case GroupedRole:
            return m_groups->isInGroup(id);
        case EffectNamesRole:
            return clip->effectNames();
        case EffectsEnabledRole:
            return clip->stackEnabled();
        case InPointRole:
            return clip->getIn();
        case OutPointRole:
            return clip->getOut();
        case ShowKeyframesRole:
            return clip->showKeyframes();
        case FadeInRole:
            return clip->fadeIn();
        case FadeOutRole:
            return clip->fadeOut();
        case FadeInMethodRole:
            return clip->fadeMethod(true);
        case FadeOutMethodRole:
            return clip->fadeMethod(false);
        case MixRole:
            return clip->getMixDuration();
        case MixCutRole:
            return clip->getMixCutPosition();
        case ClipThumbRole:
            return clip->clipThumbPath();
        case ReloadAudioThumbRole:
            return clip->forceThumbReload;
        case PositionOffsetRole:
            return clip->getOffset();
        case SpeedRole:
            return clip->getSpeed();
        case GrabbedRole:
            return clip->isGrabbed();
        case SelectedRole:
            return clip->selected;
        case TagRole:
            return clip->clipTag();
        case TimeRemapRole:
            return clip->hasTimeRemap();
        default:
            break;
        }
    } else if (isTrack(id)) {
        switch (role) {
        case NameRole:
        case Qt::DisplayRole: {
            return getTrackById_const(id)->getProperty("kdenlive:track_name").toString();
        }
        case TypeRole:
            return QVariant::fromValue(ClipType::ProducerType::Track);
        case DurationRole:
            return getTrackById_const(id)->trackDuration();
        case IsDisabledRole:
            return getTrackById_const(id)->isAudioTrack() ? getTrackById_const(id)->isMute() : getTrackById_const(id)->isHidden();
        case IsAudioRole:
            return getTrackById_const(id)->isAudioTrack();
        case TrackTagRole:
            return getTrackTagById(id);
        case IsLockedRole:
            return getTrackById_const(id)->isLocked();
        case HeightRole: {
            int collapsed = getTrackById_const(id)->getProperty("kdenlive:collapsed").toInt();
            if (collapsed > 0) {
                return collapsed;
            }
            int height = getTrackById_const(id)->getProperty("kdenlive:trackheight").toInt();
            return (height > 0 ? height : KdenliveSettings::trackheight());
        }
        case ThumbsFormatRole:
            return getTrackById_const(id)->getProperty("kdenlive:thumbs_format").toInt();
        case IsCompositeRole: {
        case AudioRecordRole:
            return getTrackById_const(id)->getProperty("kdenlive:audio_rec").toInt();
        }
        case TrackActiveRole: {
            return getTrackById_const(id)->isTimelineActive();
        }
        case EffectNamesRole: {
            return getTrackById_const(id)->effectNames();
        }
        case EffectsEnabledRole: {
            return getTrackById_const(id)->stackEnabled();
        }
        case EffectZonesRole: {
            return getTrackById_const(id)->stackZones();
        }
        default:
            break;
        }
    } else if (isComposition(id)) {
        std::shared_ptr<CompositionModel> compo = m_allCompositions.at(id);
        switch (role) {
        case NameRole:
        case Qt::DisplayRole:
        case ResourceRole:
        case ServiceRole:
            return compo->displayName();
            break;
        case TypeRole:
            return QVariant::fromValue(ClipType::ProducerType::Composition);
        case StartRole:
            return compo->getPosition();
        case TrackIdRole:
            return compo->getCurrentTrackId();
        case DurationRole:
            return compo->getPlaytime();
        case GroupedRole:
            return m_groups->isInGroup(id);
        case InPointRole:
            return 0;
        case OutPointRole:
            return 100;
        case BinIdRole:
            return 5;
        case KeyframesRole: {
            return QVariant::fromValue<KeyframeModel *>(compo->getEffectKeyframeModel());
        }
        case FakeTrackIdRole:
            return compo->getFakeTrackId();
        case FakePositionRole:
            return compo->getFakePosition();
        case ShowKeyframesRole:
            return compo->showKeyframes();
        case ItemATrack:
            return compo->getForcedTrack();
        case MarkersRole: {
            QVariantList markersList;
            return markersList;
        }
        case GrabbedRole:
            return compo->isGrabbed();
        case SelectedRole:
            return compo->selected;
        default:
            break;
        }
    } else {
        qDebug() << "UNKNOWN DATA requested " << index << roleNames().value(role);
    }
    return QVariant();
}

void TimelineItemModel::setTrackName(int trackId, const QString &text)
{
    QWriteLocker locker(&m_lock);
    const QString &currentName = getTrackProperty(trackId, QStringLiteral("kdenlive:track_name")).toString();
    if (text == currentName) {
        return;
    }
    Fun undo_lambda = [this, trackId, currentName]() {
        setTrackProperty(trackId, QStringLiteral("kdenlive:track_name"), currentName);
        return true;
    };
    Fun redo_lambda = [this, trackId, text]() {
        setTrackProperty(trackId, QStringLiteral("kdenlive:track_name"), text);
        return true;
    };
    redo_lambda();
    PUSH_UNDO(undo_lambda, redo_lambda, i18n("Rename Track"));
}

void TimelineItemModel::setTrackProperty(int trackId, const QString &name, const QString &value)
{
    std::shared_ptr<TrackModel> track = getTrackById(trackId);
    track->setProperty(name, value);
    QVector<int> roles;
    bool updateMultiTrack = false;
    if (name == QLatin1String("kdenlive:track_name")) {
        roles.push_back(NameRole);
        if (!track->isAudioTrack()) {
            updateMultiTrack = true;
        }
    } else if (name == QLatin1String("kdenlive:locked_track")) {
        roles.push_back(IsLockedRole);
    } else if (name == QLatin1String("hide")) {
        roles.push_back(IsDisabledRole);
        if (!track->isAudioTrack() && !isLoading) {
            pCore->invalidateItem(ObjectId(KdenliveObjectType::TimelineTrack, trackId, m_uuid));
            pCore->refreshProjectMonitorOnce();
            updateMultiTrack = true;
        }
    } else if (name == QLatin1String("kdenlive:timeline_active")) {
        roles.push_back(TrackActiveRole);
    } else if (name == QLatin1String("kdenlive:thumbs_format")) {
        roles.push_back(ThumbsFormatRole);
    } else if (name == QLatin1String("kdenlive:collapsed")) {
        roles.push_back(HeightRole);
    } else if (name == QLatin1String("kdenlive:audio_rec")) {
        roles.push_back(AudioRecordRole);
    }
    if (!roles.isEmpty()) {
        QModelIndex ix = makeTrackIndexFromID(trackId);
        Q_EMIT dataChanged(ix, ix, roles);
        if (updateMultiTrack) {
            Q_EMIT trackVisibilityChanged();
        }
    }
}

void TimelineItemModel::setTrackStackEnabled(int tid, bool enable)
{
    Fun redo = [this, tid, enable]() {
        std::shared_ptr<TrackModel> track = getTrackById(tid);
        if (!track) {
            return false;
        }
        track->setEffectStackEnabled(enable);
        QModelIndex ix = makeTrackIndexFromID(tid);
        Q_EMIT dataChanged(ix, ix, {TimelineModel::EffectsEnabledRole});
        return true;
    };
    Fun undo = [this, tid, enable]() {
        std::shared_ptr<TrackModel> track = getTrackById(tid);
        if (!track) {
            return false;
        }
        track->setEffectStackEnabled(!enable);
        QModelIndex ix = makeTrackIndexFromID(tid);
        Q_EMIT dataChanged(ix, ix, {TimelineModel::EffectsEnabledRole});
        return true;
    };
    redo();
    pCore->pushUndo(undo, redo, enable ? i18n("Enable Track Effect Stack") : i18n("Disable Track Effect Stack"));
}

void TimelineItemModel::importTrackEffects(int tid, std::weak_ptr<Mlt::Service> service)
{
    std::shared_ptr<TrackModel> track = getTrackById(tid);
    Mlt::Tractor *destination = track->getTrackService();
    // Audio mixer effects are attached to the Tractor service, while track effects are attached to first playlist service
    if (auto ptr = service.lock()) {
        for (int i = 0; i < ptr->filter_count(); i++) {
            std::unique_ptr<Mlt::Filter> filter(ptr->filter(i));
            if (filter->get_int("internal_added") > 0) {
                destination->attach(*filter.get());
            }
        }
    }

    track->importEffects(std::move(service));
}

QVariant TimelineItemModel::getTrackProperty(int tid, const QString &name) const
{
    return getTrackById_const(tid)->getProperty(name);
}

QList<int> TimelineItemModel::getActiveAudioTrackIndexes() const
{
    QList<int> trackIds;
    std::list<std::shared_ptr<TrackModel>>::const_reverse_iterator it;
    for (it = m_allTracks.rbegin(); it != m_allTracks.rend(); it++) {
        if ((*it)->isAudioTrack() && (*it)->shouldReceiveTimelineOp()) {
            trackIds << (*it)->getId();
        }
    }
    return trackIds;
}

std::shared_ptr<SubtitleModel> TimelineItemModel::createSubtitleModel()
{
    if (m_subtitleModel == nullptr) {
        // Initialize the subtitle model and load file if any
        m_subtitleModel.reset(
            new SubtitleModel(std::static_pointer_cast<TimelineItemModel>(shared_from_this()), std::static_pointer_cast<SnapInterface>(m_snaps), this));
        Q_EMIT subtitleModelInitialized();
    }
    return m_subtitleModel;
}

const QString TimelineItemModel::getTrackFullName(int tid) const
{
    QString tag = getTrackTagById(tid);
    QString trackName = getTrackById_const(tid)->getProperty(QStringLiteral("kdenlive:track_name")).toString();
    return trackName.isEmpty() ? tag : tag + QStringLiteral(" - ") + trackName;
}

const QString TimelineItemModel::groupsData()
{
    return m_groups->toJson();
}

bool TimelineItemModel::loadGroups(const QString &groupsData)
{
    return m_groups->fromJson(groupsData);
}

void TimelineItemModel::notifyChange(const QModelIndex &topleft, const QModelIndex &bottomright, bool start, bool duration, bool updateThumb)
{
    QVector<int> roles;
    if (start) {
        roles.push_back(TimelineModel::StartRole);
        if (updateThumb) {
            roles.push_back(TimelineModel::InPointRole);
        }
    }
    if (duration) {
        roles.push_back(TimelineModel::DurationRole);
        if (updateThumb) {
            roles.push_back(TimelineModel::OutPointRole);
        }
    }
    Q_EMIT dataChanged(topleft, bottomright, roles);
}

void TimelineItemModel::notifyChange(const QModelIndex &topleft, const QModelIndex &bottomright, const QVector<int> &roles)
{
    Q_EMIT dataChanged(topleft, bottomright, roles);
}

void TimelineItemModel::rebuildMixer()
{
    if (pCore->mixer() == nullptr) {
        return;
    }
    pCore->mixer()->cleanup();
    pCore->mixer()->setModel(std::static_pointer_cast<TimelineItemModel>(shared_from_this()));
    auto it = m_allTracks.cbegin();
    int recTrack = -1;
    while (it != m_allTracks.cend()) {
        if ((*it)->isAudioTrack()) {
            pCore->mixer()->registerTrack((*it)->getId(), (*it)->getTrackService(), getTrackTagById((*it)->getId()),
                                          (*it)->getProperty(QStringLiteral("kdenlive:track_name")).toString());
            if ((*it)->getProperty("kdenlive:audio_rec").toInt() == 1) {
                recTrack = (*it)->getId();
            }
        }
        ++it;
    }
    if (recTrack > -1) {
        QMetaObject::invokeMethod(pCore->mixer(), "monitorAudio", Q_ARG(int, recTrack), Q_ARG(bool, true));
    }
}

bool TimelineItemModel::copyClipEffect(int clipId, const QString sourceId)
{
    QStringList source = sourceId.split(QLatin1Char(','));
    Q_ASSERT(m_allClips.count(clipId) && source.count() == 5);
    int itemType = source.at(0).toInt();
    int itemId = source.at(1).toInt();
    int itemRow = source.at(2).toInt();
    const QUuid uuid(source.at(3));
    bool singleTarget = source.at(4).toInt() == 1;
    std::shared_ptr<EffectStackModel> effectStack = pCore->getItemEffectStack(uuid, itemType, itemId);
    if (!singleTarget && m_singleSelectionMode && m_currentSelection.count(clipId)) {
        // only operate on the selected item
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        for (auto &s : m_currentSelection) {
            if (isClip(s)) {
                m_allClips.at(s)->copyEffectWithUndo(effectStack, itemRow, undo, redo);
            }
        }
        pCore->pushUndo(undo, redo, i18n("Copy effect"));
        return true;
    } else if (!singleTarget && m_groups->isInGroup(clipId)) {
        int parentGroup = m_groups->getRootId(clipId);
        if (parentGroup > -1) {
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            std::unordered_set<int> sub = m_groups->getLeaves(parentGroup);
            for (auto &s : sub) {
                if (isClip(s)) {
                    m_allClips.at(s)->copyEffectWithUndo(effectStack, itemRow, undo, redo);
                }
            }
            pCore->pushUndo(undo, redo, i18n("Copy effect"));
            return true;
        }
    }
    return m_allClips.at(clipId)->copyEffect(effectStack, itemRow);
}

void TimelineItemModel::removeTrackCompositing()
{
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    QScopedPointer<Mlt::Service> service(m_tractor->field());
    field->block();
    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_service_type mlt_type = mlt_service_identify(nextservice);
    while (mlt_type == mlt_service_transition_type) {
        Mlt::Transition transition(reinterpret_cast<mlt_transition>(nextservice));
        nextservice = mlt_service_producer(nextservice);
        if (transition.get_int("internal_added") == 237) {
            // remove all compositing transitions
            qDebug() << "!!!!!!!!!!!!! REMOVING COMPO: " << transition.get_a_track() << " / " << transition.get_b_track();
            field->disconnect_service(transition);
            transition.disconnect_all_producers();
        }
        if (nextservice == nullptr) {
            break;
        }
        mlt_type = mlt_service_identify(nextservice);
    }
    field->unblock();
}

void TimelineItemModel::buildTrackCompositing(bool rebuild)
{
    READ_LOCK();
    bool isMultiTrack = pCore->enableMultiTrack(false);
    if (rebuild) {
        removeTrackCompositing();
    }
    if (m_closing) {
        return;
    }
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    QScopedPointer<Mlt::Service> service(m_tractor->field());
    field->block();
    QString composite;
    if (pCore->currentDoc()->getSequenceProperty(m_uuid, QStringLiteral("compositing"), QStringLiteral("1")).toInt() > 0) {
        composite = TransitionsRepository::get()->getCompositingTransition();
    }
    int videoTracks = 0;
    int audioTracks = 0;
    auto it = m_allTracks.cbegin();
    while (it != m_allTracks.cend()) {
        int trackPos = getTrackMltIndex((*it)->getId());
        if (!composite.isEmpty() && !(*it)->isAudioTrack()) {
            // video track, add composition
            std::unique_ptr<Mlt::Transition> transition = TransitionsRepository::get()->getTransition(composite);
            transition->set("internal_added", 237);
            transition->set("always_active", 1);
            transition->set_tracks(0, trackPos);
            field->plant_transition(*transition.get(), 0, trackPos);
            videoTracks++;
        } else if ((*it)->isAudioTrack()) {
            // audio mix
            std::unique_ptr<Mlt::Transition> transition = TransitionsRepository::get()->getTransition(QStringLiteral("mix"));
            transition->set("internal_added", 237);
            transition->set("always_active", 1);
            transition->set("accepts_blanks", 1);
            transition->set("sum", 1);
            transition->set_tracks(0, trackPos);
            field->plant_transition(*transition.get(), 0, trackPos);
            audioTracks++;
        }
        ++it;
    }
    field->unblock();
    if (rebuild) {
        rebuildMixer();
    }
    // Update sequence clip's AV status
    int currentClipType = m_tractor->get_int("kdenlive:clip_type");
    int newClipType = audioTracks > 0 ? (videoTracks > 0 ? 0 : 1) : 2;
    if (currentClipType != newClipType) {
        m_tractor->set("kdenlive:sequenceproperties.hasAudio", audioTracks > 0 ? 1 : 0);
        m_tractor->set("kdenlive:sequenceproperties.hasVideo", videoTracks > 0 ? 1 : 0);
        m_tractor->set("kdenlive:clip_type", newClipType);
    }
    pCore->updateSequenceAVType(m_uuid, audioTracks + videoTracks);
    if (isMultiTrack) {
        pCore->enableMultiTrack(true);
    }
    if (composite.isEmpty()) {
        pCore->displayMessage(i18n("Could not setup track compositing, check your install"), MessageType::ErrorMessage);
    }
}

void TimelineItemModel::notifyChange(const QModelIndex &topleft, const QModelIndex &bottomright, int role)
{
    Q_EMIT dataChanged(topleft, bottomright, {role});
}

void TimelineItemModel::_beginRemoveRows(const QModelIndex &i, int j, int k)
{
    beginRemoveRows(i, j, k);
}
void TimelineItemModel::_beginInsertRows(const QModelIndex &i, int j, int k)
{
    beginInsertRows(i, j, k);
}
void TimelineItemModel::_endRemoveRows()
{
    endRemoveRows();
}
void TimelineItemModel::_endInsertRows()
{
    endInsertRows();
}

void TimelineItemModel::_resetView()
{
    beginResetModel();
    endResetModel();
}

void TimelineItemModel::passSequenceProperties(const QMap<QString, QString> baseProperties)
{
    QMapIterator<QString, QString> i(baseProperties);
    while (i.hasNext()) {
        i.next();
        tractor()->set(QStringLiteral("kdenlive:sequenceproperties.%1").arg(i.key()).toUtf8().constData(), i.value().toUtf8().constData());
    }
    // Store groups data
    tractor()->set("kdenlive:sequenceproperties.groups", groupsData().toUtf8().constData());
    tractor()->set("kdenlive:sequenceproperties.documentuuid", pCore->currentDoc()->uuid().toString().toUtf8().constData());
    // Save timeline guides
    const QString guidesData = getGuideModel()->toJson();
    tractor()->set("kdenlive:sequenceproperties.guides", guidesData.toUtf8().constData());
    QPair<int, int> tracks = getAVtracksCount();
    tractor()->set("kdenlive:sequenceproperties.hasAudio", tracks.first > 0 ? 1 : 0);
    tractor()->set("kdenlive:sequenceproperties.hasVideo", tracks.second > 0 ? 1 : 0);
    tractor()->set("kdenlive:sequenceproperties.tracksCount", tracks.first + tracks.second);

    if (hasTimelinePreview()) {
        QPair<QStringList, QStringList> chunks = previewManager()->previewChunks();
        tractor()->set("kdenlive:sequenceproperties.previewchunks", chunks.first.join(QLatin1Char(',')).toUtf8().constData());
        tractor()->set("kdenlive:sequenceproperties.dirtypreviewchunks", chunks.second.join(QLatin1Char(',')).toUtf8().constData());
    }
}

void TimelineItemModel::processTimelineReplacement(QList<int> instances, const QString &originalId, const QString &replacementId, int maxDuration,
                                                   bool replaceAudio, bool replaceVideo)
{
    // Check for locked tracks
    QList<int> lockedTracks;
    for (const auto &track : m_allTracks) {
        if (track->isLocked()) {
            lockedTracks << track->getId();
        }
    }
    // Check if some clips are longer than our replacement clip
    QList<int> notReplacedIds;
    for (auto &id : instances) {
        Q_ASSERT(m_allClips.count(id) > 0);
        if ((replaceAudio && m_allClips.at(id)->isAudioOnly()) || (replaceVideo && m_allClips.at(id)->clipState() == PlaylistState::VideoOnly)) {
            // Match, replace
            std::shared_ptr<ClipModel> clip = m_allClips.at(id);
            if (clip->getOut() > maxDuration || lockedTracks.contains(clip->getCurrentTrackId())) {
                notReplacedIds << id;
            }
        }
    }
    Fun local_redo = [this, instances, replacementId, replaceAudio, replaceVideo, notReplacedIds]() {
        int replaced = 0;
        for (auto &id : instances) {
            if (notReplacedIds.contains(id)) {
                continue;
            }
            Q_ASSERT(m_allClips.count(id) > 0);
            if ((replaceAudio && m_allClips.at(id)->isAudioOnly()) || (replaceVideo && m_allClips.at(id)->clipState() == PlaylistState::VideoOnly)) {
                // Match, replace
                std::shared_ptr<ClipModel> clip = m_allClips.at(id);
                clip->switchBinReference(replacementId, m_uuid);
                replaced++;
                QModelIndex ix = makeClipIndexFromID(id);
                clip->forceThumbReload = !clip->forceThumbReload;
                Q_EMIT dataChanged(
                    ix, ix,
                    {BinIdRole, ClipThumbRole, AudioChannelsRole, AudioStreamRole, AudioMultiStreamRole, AudioStreamIndexRole, NameRole, ReloadAudioThumbRole});
            }
        }
        if (!notReplacedIds.isEmpty()) {
            pCore->displayMessage(i18n("Clips replaced: %1. <b>Clips not replaced: %2</b>", replaced, notReplacedIds.size()), MessageType::ErrorMessage);
        } else {
            pCore->displayMessage(i18np("One clip replaced", "%1 clips replaced", replaced), MessageType::InformationMessage);
        }
        return replaced > 0;
    };
    if (local_redo()) {
        Fun local_undo = [this, instances, originalId, replaceAudio, replaceVideo, notReplacedIds]() {
            int replaced = 0;
            for (auto &id : instances) {
                if (notReplacedIds.contains(id)) {
                    continue;
                }
                Q_ASSERT(m_allClips.count(id) > 0);
                if ((replaceAudio && m_allClips.at(id)->isAudioOnly()) || (replaceVideo && m_allClips.at(id)->clipState() == PlaylistState::VideoOnly)) {
                    // Match, replace
                    std::shared_ptr<ClipModel> clip = m_allClips.at(id);
                    clip->switchBinReference(originalId, m_uuid);
                    replaced++;
                    QModelIndex ix = makeClipIndexFromID(id);
                    clip->forceThumbReload = !clip->forceThumbReload;
                    Q_EMIT dataChanged(ix, ix,
                                       {BinIdRole, ClipThumbRole, AudioChannelsRole, AudioStreamRole, AudioMultiStreamRole, AudioStreamIndexRole, NameRole,
                                        ReloadAudioThumbRole});
                }
            }
            if (!notReplacedIds.isEmpty()) {
                pCore->displayMessage(i18n("Clips replaced: %1, Clips too long: %2", replaced, notReplacedIds.size()), MessageType::InformationMessage);
            } else {
                pCore->displayMessage(i18np("One clip replaced", "%1 clips replaced", replaced), MessageType::InformationMessage);
            }
            return replaced > 0;
        };
        pCore->pushUndo(local_undo, local_redo, replaceAudio ? (replaceVideo ? i18n("Replace clip") : i18n("Replace audio")) : i18n("Replace video"));
    }
}

int TimelineItemModel::clipAssetGroupInstances(int cid, const QString &assetId)
{
    if (!isClip(cid)) {
        return 0;
    }
    int gid = m_groups->getRootId(cid);
    int count = 0;
    if (gid > -1) {
        std::unordered_set<int> sub = m_groups->getLeaves(gid);
        for (auto &id : sub) {
            if (isClip(id) && clipAssetRow(id, assetId) > -1) {
                count++;
            }
        }
    }
    return count;
}

void TimelineItemModel::applyClipAssetGroupCommand(int cid, const QString &assetId, const QModelIndex &index, const QString &previousValue, QString value,
                                                   QUndoCommand *command)
{
    if (!isClip(cid)) {
        return;
    }
    int gid = m_groups->getRootId(cid);
    if (gid > -1) {
        std::unordered_set<int> sub;
        if (m_singleSelectionMode && m_currentSelection.count(cid)) {
            sub = m_currentSelection;
        } else {
            sub = m_groups->getLeaves(gid);
        }
        sub.erase(cid);
        for (auto &id : sub) {
            if (isClip(id)) {
                int assetRow = clipAssetRow(id, assetId);
                if (assetRow > -1) {
                    const auto clip = getClipPtr(id);
                    clip->m_effectStack->applyAssetCommand(assetRow, index, previousValue, value, command);
                }
            }
        }
    }
}

void TimelineItemModel::applyClipAssetGroupKeyframeCommand(int cid, const QString &assetId, const QModelIndex &index, GenTime pos,
                                                           const QVariant &previousValue, const QVariant &value, int ix, QUndoCommand *command)
{
    if (!isClip(cid)) {
        return;
    }
    int gid = m_groups->getRootId(cid);
    if (gid > -1) {
        std::unordered_set<int> sub;
        if (m_singleSelectionMode && m_currentSelection.count(cid)) {
            sub = m_currentSelection;
        } else {
            sub = m_groups->getLeaves(gid);
        }
        sub.erase(cid);
        double fps = pCore->getCurrentFps();
        for (auto &id : sub) {
            if (isClip(id)) {
                int assetRow = clipAssetRow(id, assetId);
                if (assetRow > -1) {
                    const auto clip = getClipPtr(id);
                    clip->m_effectStack->applyAssetKeyframeCommand(assetRow, index, pos + GenTime(clip->getIn(), fps), previousValue, value, ix, command);
                }
            }
        }
    }
}

void TimelineItemModel::applyClipAssetGroupMultiKeyframeCommand(int cid, const QString &assetId, const QList<QModelIndex> &indexes, GenTime pos,
                                                                const QStringList &sourceValues, const QStringList &values, QUndoCommand *command)
{
    int gid = m_groups->getRootId(cid);
    if (gid > -1) {
        std::unordered_set<int> sub;
        if (m_singleSelectionMode && m_currentSelection.count(cid)) {
            sub = m_currentSelection;
        } else {
            sub = m_groups->getLeaves(gid);
        }
        sub.erase(cid);
        double fps = pCore->getCurrentFps();
        for (auto &id : sub) {
            if (isClip(id)) {
                int assetRow = clipAssetRow(id, assetId);
                if (assetRow > -1) {
                    const auto clip = getClipPtr(id);
                    clip->m_effectStack->applyAssetMultiKeyframeCommand(assetRow, indexes, pos + GenTime(clip->getIn(), fps), sourceValues, values, command);
                }
            }
        }
    }
}

void TimelineItemModel::removeEffectFromGroup(int cid, const QString &assetId, int originalId)
{
    if (!isClip(cid)) {
        return;
    }
    int gid = m_groups->getRootId(cid);
    std::unordered_set<int> sub;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    if (gid > -1) {
        if (m_singleSelectionMode && m_currentSelection.count(cid)) {
            sub = m_currentSelection;
        } else {
            sub = m_groups->getLeaves(gid);
        }
    } else {
        sub = {cid};
    }
    QString effectName;
    int assetRow = -1;
    for (auto &id : sub) {
        if (isClip(id)) {
            if (id == cid && originalId > -1) {
                assetRow = clipAssetRow(id, assetId, originalId);
            } else {
                assetRow = clipAssetRow(id, assetId);
            }
            if (assetRow > -1) {
                const auto clip = getClipPtr(id);
                clip->m_effectStack->removeEffectWithUndo(assetId, effectName, assetRow, undo, redo);
            }
        }
    }
    pCore->pushUndo(undo, redo, i18n("Delete effect %1", effectName));
}

void TimelineItemModel::disableEffectFromGroup(int cid, const QString &assetId, bool disable, Fun &undo, Fun &redo)
{
    if (!isClip(cid)) {
        return;
    }
    int gid = m_groups->getRootId(cid);
    std::unordered_set<int> sub;
    if (gid > -1) {
        if (m_singleSelectionMode && m_currentSelection.count(cid)) {
            sub = m_currentSelection;
        } else {
            sub = m_groups->getLeaves(gid);
        }
    } else {
        return;
    }
    int assetRow = -1;
    for (auto &id : sub) {
        if (id == cid) {
            continue;
        }
        if (isClip(id)) {
            assetRow = clipAssetRow(id, assetId);
            if (assetRow > -1) {
                const auto clip = getClipPtr(id);
                auto effect = clip->m_effectStack->getEffectStackRow(assetRow);
                if (effect) {
                    effect->markEnabled(!disable, undo, redo);
                }
            }
        }
    }
}

QList<std::shared_ptr<KeyframeModelList>> TimelineItemModel::getGroupKeyframeModels(int cid, const QString &assetId)
{
    if (!isClip(cid) && !isComposition(cid)) {
        return {};
    }
    QList<std::shared_ptr<KeyframeModelList>> models;
    int gid = m_groups->getRootId(cid);
    if (gid > -1) {
        std::unordered_set<int> sub;
        if (m_singleSelectionMode && m_currentSelection.count(cid)) {
            sub = m_currentSelection;
        } else {
            sub = m_groups->getLeaves(gid);
        }
        sub.erase(cid);
        for (auto &id : sub) {
            if (isClip(id)) {
                int assetRow = clipAssetRow(id, assetId);
                if (assetRow > -1) {
                    const auto clip = getClipPtr(id);
                    auto mdl = clip->getKFModel(assetRow);
                    if (mdl) {
                        models << mdl;
                    }
                }
            }
        }
    }
    return models;
}
