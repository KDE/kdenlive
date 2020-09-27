/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
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

#include "timelineitemmodel.hpp"
#include "audiomixer/mixermanager.hpp"
#include "assets/keyframes/model/keyframemodel.hpp"
#include "bin/model/markerlistmodel.hpp"
#include "clipmodel.hpp"
#include "compositionmodel.hpp"
#include "core.h"
#include "doc/docundostack.hpp"
#include "groupsmodel.hpp"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "trackmodel.hpp"
#include "snapmodel.hpp"
#include "transitions/transitionsrepository.hpp"
#include <QDebug>
#include <QFileInfo>
#include <mlt++/MltField.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTractor.h>
#include <mlt++/MltTransition.h>

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
    registration::class_<TimelineItemModel>("TimelineItemModel");
}

TimelineItemModel::TimelineItemModel(Mlt::Profile *profile, std::weak_ptr<DocUndoStack> undo_stack)
    : TimelineModel(profile, std::move(undo_stack))
{
}

void TimelineItemModel::finishConstruct(const std::shared_ptr<TimelineItemModel> &ptr, const std::shared_ptr<MarkerListModel> &guideModel)
{
    ptr->weak_this_ = ptr;
    ptr->m_groups = std::make_unique<GroupsModel>(ptr);
    guideModel->registerSnapModel(std::static_pointer_cast<SnapInterface>(ptr->m_snaps));
}

std::shared_ptr<TimelineItemModel> TimelineItemModel::construct(Mlt::Profile *profile, std::shared_ptr<MarkerListModel> guideModel,
                                                                std::weak_ptr<DocUndoStack> undo_stack)
{
    std::shared_ptr<TimelineItemModel> ptr(new TimelineItemModel(profile, std::move(undo_stack)));
    finishConstruct(ptr, std::move(guideModel));
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
    } else if (row < (int)m_allTracks.size() && row >= 0) {
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

QModelIndex TimelineItemModel::makeClipIndexFromID(int clipId) const
{
    Q_ASSERT(m_allClips.count(clipId) > 0);
    int trackId = m_allClips.at(clipId)->getCurrentTrackId();
    if (trackId == -1) {
        // Clip is not inserted in a track
        qDebug() << "/// WARNING; INVALID CLIP INDEX REQUESTED: "<<clipId<<"\n________________";
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

QModelIndex TimelineItemModel::makeTrackIndexFromID(int trackId) const
{
    // we retrieve iterator
    Q_ASSERT(m_iteratorTable.count(trackId) > 0);
    auto it = m_iteratorTable.at(trackId);
    int ind = (int)std::distance<decltype(m_allTracks.cbegin())>(m_allTracks.begin(), it);
    // Get sort order
    // ind = getTracksCount() - 1 - ind;
    return index(ind);
}

QModelIndex TimelineItemModel::parent(const QModelIndex &index) const
{
    READ_LOCK();
    // qDebug() << "TimelineItemModel::parent"<< index;
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
        const int id = (int)parent.internalId();
        if (!isTrack(id)) {
            // clips don't have children
            // if it is not a track, it is something invalid
            return 0;
        }
        return getTrackClipsCount(id) + getTrackCompositionsCount(id);
    }
    return (int)m_allTracks.size();
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
    roles[IsProxyRole] = "isProxy";
    roles[ServiceRole] = "mlt_service";
    roles[BinIdRole] = "binId";
    roles[TrackIdRole] = "trackId";
    roles[FakeTrackIdRole] = "fakeTrackId";
    roles[FakePositionRole] = "fakePosition";
    roles[StartRole] = "start";
    roles[DurationRole] = "duration";
    roles[MaxDurationRole] = "maxDuration";
    roles[MarkersRole] = "markers";
    roles[KeyframesRole] = "keyframeModel";
    roles[ShowKeyframesRole] = "showKeyframes";
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
    roles[FileHashRole] = "hash";
    roles[SpeedRole] = "speed";
    roles[HeightRole] = "trackHeight";
    roles[TrackTagRole] = "trackTag";
    roles[ItemIdRole] = "item";
    roles[ItemATrack] = "a_track";
    roles[HasAudio] = "hasAudio";
    roles[CanBeAudioRole] = "canBeAudio";
    roles[CanBeVideoRole] = "canBeVideo";
    roles[ReloadThumbRole] = "reloadThumb";
    roles[PositionOffsetRole] = "positionOffset";
    roles[ThumbsFormatRole] = "thumbsFormat";
    roles[AudioRecordRole] = "audioRecord";
    roles[TrackActiveRole] = "trackActive";
    roles[EffectNamesRole] = "effectNames";
    roles[EffectsEnabledRole] = "isStackEnabled";
    roles[GrabbedRole] = "isGrabbed";
    roles[SelectedRole] = "selected";
    return roles;
}

QVariant TimelineItemModel::data(const QModelIndex &index, int role) const
{
    READ_LOCK();
    if (!m_tractor || !index.isValid()) {
        // qDebug() << "DATA abort. Index validity="<<index.isValid();
        return QVariant();
    }
    const int id = (int)index.internalId();
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
        // qDebug() << "REQUESTING DATA "<<roleNames()[role]<<index;
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
        case IsProxyRole: {
            return clip->isProxied();
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
        case StatusRole:
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
        case ReloadThumbRole:
            return clip->forceThumbReload;
        case PositionOffsetRole:
            return clip->getOffset();
        case SpeedRole:
            return clip->getSpeed();
        case GrabbedRole:
            return clip->isGrabbed();
        case SelectedRole:
            return clip->selected;
        default:
            break;
        }
    } else if (isTrack(id)) {
        // qDebug() << "DATA REQUESTED FOR TRACK "<< id;
        switch (role) {
        case NameRole:
        case Qt::DisplayRole: {
            return getTrackById_const(id)->getProperty("kdenlive:track_name").toString();
        }
        case TypeRole:
            return QVariant::fromValue(ClipType::ProducerType::Track);
        case DurationRole:
            // qDebug() << "DATA yielding duration" << m_tractor->get_playtime();
            return getTrackById_const(id)->trackDuration();
        case IsDisabledRole:
            // qDebug() << "DATA yielding mute" << 0;
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
            // qDebug() << "DATA yielding height" << height;
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
        qDebug() << "UNKNOWN DATA requested " << index << roleNames()[role];
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

void TimelineItemModel::hideTrack(int trackId, const QString state)
{
    QWriteLocker locker(&m_lock);
    QString previousState = getTrackProperty(trackId, QStringLiteral("hide")).toString();
    Fun undo_lambda = [this, trackId, previousState]() {
        setTrackProperty(trackId, QStringLiteral("hide"), previousState);
        return true;
    };
    Fun redo_lambda = [this, trackId, state]() {
        setTrackProperty(trackId, QStringLiteral("hide"), state);
        return true;
    };
    redo_lambda();
    PUSH_UNDO(undo_lambda, redo_lambda, state == QLatin1String("3") ? i18n("Hide Track") : i18n("Enable Track"));
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
        if (!track->isAudioTrack()) {
            pCore->invalidateItem(ObjectId(ObjectType::TimelineTrack, trackId));
            pCore->requestMonitorRefresh();
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
        emit dataChanged(ix, ix, roles);
        if (updateMultiTrack) {
            emit trackVisibilityChanged();
        }
    }
}

void TimelineItemModel::setTrackStackEnabled(int tid, bool enable)
{
    std::shared_ptr<TrackModel> track = getTrackById(tid);
    track->setEffectStackEnabled(enable);
    QModelIndex ix = makeTrackIndexFromID(tid);
    emit dataChanged(ix, ix, {TimelineModel::EffectsEnabledRole});
}

void TimelineItemModel::importTrackEffects(int tid, std::weak_ptr<Mlt::Service> service)
{
    std::shared_ptr<TrackModel> track = getTrackById(tid);
    std::shared_ptr<Mlt::Tractor> destination = track->getTrackService();
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

int TimelineItemModel::getFirstVideoTrackIndex() const
{
    int trackId = -1;
    auto it = m_allTracks.cbegin();
    while (it != m_allTracks.cend()) {
        trackId = (*it)->getId();
        if (!(*it)->isAudioTrack()) {
            break;
        }
        ++it;
    }
    return trackId;
}

int TimelineItemModel::getFirstAudioTrackIndex() const
{
    int trackId = -1;
    auto it = m_allTracks.cbegin();
    while (it != m_allTracks.cend()) {
        if ((*it)->isAudioTrack()) {
            trackId = (*it)->getId();
        }
        ++it;
    }
    return trackId;
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
    emit dataChanged(topleft, bottomright, roles);
}

void TimelineItemModel::notifyChange(const QModelIndex &topleft, const QModelIndex &bottomright, const QVector<int> &roles)
{
    emit dataChanged(topleft, bottomright, roles);
}

void TimelineItemModel::buildTrackCompositing(bool rebuild)
{
    bool isMultiTrack = pCore->enableMultiTrack(false);
    auto it = m_allTracks.cbegin();
    QScopedPointer<Mlt::Service> service(m_tractor->field());
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();
    // Make sure all previous track compositing is removed
    if (rebuild) {
        while (service != nullptr && service->is_valid()) {
            if (service->type() == transition_type) {
                Mlt::Transition t((mlt_transition)service->get_service());
                service.reset(service->producer());
                if (t.get_int("internal_added") == 237) {
                    // remove all compositing transitions
                    field->disconnect_service(t);
                    t.disconnect_all_producers();
                }
            } else {
                service.reset(service->producer());
            }
        }
    }
    QString composite = TransitionsRepository::get()->getCompositingTransition();
    bool hasMixer = pCore->mixer() != nullptr;
    if (hasMixer) {
        pCore->mixer()->cleanup();
    }
    while (it != m_allTracks.cend()) {
        int trackPos = getTrackMltIndex((*it)->getId());
        if (!composite.isEmpty() && !(*it)->isAudioTrack()) {
            // video track, add composition
            std::unique_ptr<Mlt::Transition> transition = TransitionsRepository::get()->getTransition(composite);
            transition->set("internal_added", 237);
            transition->set("always_active", 1);
            transition->set_tracks(0, trackPos);
            field->plant_transition(*transition.get(), 0, trackPos);
        } else if ((*it)->isAudioTrack()) {
            // audio mix
            std::unique_ptr<Mlt::Transition> transition = TransitionsRepository::get()->getTransition(QStringLiteral("mix"));
            transition->set("internal_added", 237);
            transition->set("always_active", 1);
            transition->set("sum", 1);
            transition->set_tracks(0, trackPos);
            field->plant_transition(*transition.get(), 0, trackPos);
            if (hasMixer) {
                pCore->mixer()->registerTrack((*it)->getId(), (*it)->getTrackService(), getTrackTagById((*it)->getId()));
                connect(pCore->mixer(), &MixerManager::showEffectStack, this, &TimelineItemModel::showTrackEffectStack);
            }
        }
        ++it;
    }
    field->unlock();
    if (isMultiTrack) {
        pCore->enableMultiTrack(true);
    }
    if (composite.isEmpty()) {
        pCore->displayMessage(i18n("Could not setup track compositing, check your install"), MessageType::ErrorMessage);
    }
}

void TimelineItemModel::notifyChange(const QModelIndex &topleft, const QModelIndex &bottomright, int role)
{
    emit dataChanged(topleft, bottomright, {role});
}

void TimelineItemModel::_beginRemoveRows(const QModelIndex &i, int j, int k)
{
    // qDebug()<<"FORWARDING beginRemoveRows"<<i<<j<<k;
    beginRemoveRows(i, j, k);
}
void TimelineItemModel::_beginInsertRows(const QModelIndex &i, int j, int k)
{
    // qDebug()<<"FORWARDING beginInsertRows"<<i<<j<<k;
    beginInsertRows(i, j, k);
}
void TimelineItemModel::_endRemoveRows()
{
    // qDebug()<<"FORWARDING endRemoveRows";
    endRemoveRows();
}
void TimelineItemModel::_endInsertRows()
{
    // qDebug()<<"FORWARDING endinsertRows";
    endInsertRows();
}

void TimelineItemModel::_resetView()
{
    beginResetModel();
    endResetModel();
}
