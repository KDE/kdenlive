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
#include "transitions/transitionsrepository.hpp"
#include <QDebug>
#include <QFileInfo>
#include <mlt++/MltField.h>
#include <mlt++/MltProfile.h>
#include <mlt++/MltTractor.h>
#include <mlt++/MltTransition.h>

#include <utility>

TimelineItemModel::TimelineItemModel(Mlt::Profile *profile, std::weak_ptr<DocUndoStack> undo_stack)
    : TimelineModel(profile, undo_stack)
{
}

void TimelineItemModel::finishConstruct(std::shared_ptr<TimelineItemModel> ptr, std::shared_ptr<MarkerListModel> guideModel)
{
    ptr->weak_this_ = ptr;
    ptr->m_groups = std::unique_ptr<GroupsModel>(new GroupsModel(ptr));
    guideModel->registerSnapModel(ptr->m_snaps);
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
    } else if (row < getTracksCount() && row >= 0) {
        // Get sort order
        //row = getTracksCount() - 1 - row;
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
        qDebug()<<"/// WARNING; INVALID CLIP INDEX REQUESTED\n________________";
        return QModelIndex();
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
    //ind = getTracksCount() - 1 - ind;
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
    return QModelIndex();
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
    return getTracksCount();
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
    roles[FakeTrackIdRole] = "fakeTrackId";
    roles[FakePositionRole] = "fakePosition";
    roles[StartRole] = "start";
    roles[DurationRole] = "duration";
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
    roles[AudioLevelsRole] = "audioLevels";
    roles[AudioChannelsRole] = "audioChannels";
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
    roles[ThumbsFormatRole] = "thumbsFormat";
    roles[EffectNamesRole] = "effectNames";
    roles[EffectsEnabledRole] = "isStackEnabled";
    roles[GrabbedRole] = "isGrabbed";
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
            QString result = clip->getProperty("kdenlive:clipname");
            if (result.isEmpty()) {
                result = clip->getProperty("resource");
                if (!result.isEmpty()) {
                    result = QFileInfo(result).fileName();
                } else {
                    result = clip->getProperty("mlt_service");
                }
            }
            return result;
        }
        case ResourceRole: {
            QString result = clip->getProperty("resource");
            if (result == QLatin1String("<producer>")) {
                result = clip->getProperty("mlt_service");
            }
            return result;
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
        case AudioLevelsRole:
            return clip->getAudioWaveform();
        case AudioChannelsRole:
            return clip->audioChannels();
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
        case GroupedRole: {
            int parentId = m_groups->getDirectAncestor(id);
            return parentId != -1 && parentId != m_temporarySelectionGroup;
        }
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
        case SpeedRole:
            return clip->getSpeed();
        case GrabbedRole:
            return clip->isGrabbed();
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
            return getTrackById_const(id)->getProperty("kdenlive:locked_track").toInt() == 1;
        case HeightRole: {
            int collapsed = getTrackById_const(id)->getProperty("kdenlive:collapsed").toInt();
            if ( collapsed > 0) {
                return collapsed;
            }
            int height = getTrackById_const(id)->getProperty("kdenlive:trackheight").toInt();
            // qDebug() << "DATA yielding height" << height;
            return (height > 0 ? height : 60);
        }
        case ThumbsFormatRole:
            return getTrackById_const(id)->getProperty("kdenlive:thumbs_format").toInt();
        case IsCompositeRole: {
            return Qt::Unchecked;
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
        default:
            break;
        }
    } else {
        qDebug() << "UNKNOWN DATA requested " << index << roleNames()[role];
    }
    return QVariant();
}

void TimelineItemModel::setTrackProperty(int trackId, const QString &name, const QString &value)
{
    std::shared_ptr<TrackModel> track = getTrackById(trackId);
    track->setProperty(name, value);
    QVector<int> roles;
    if (name == QLatin1String("kdenlive:track_name")) {
        roles.push_back(NameRole);
    } else if (name == QLatin1String("kdenlive:locked_track")) {
        roles.push_back(IsLockedRole);
    } else if (name == QLatin1String("hide")) {
        roles.push_back(IsDisabledRole);
        if(!track->isAudioTrack()) {
            pCore->requestMonitorRefresh();
        }
    } else if (name == QLatin1String("kdenlive:thumbs_format")) {
        roles.push_back(ThumbsFormatRole);
    }
    if (!roles.isEmpty()) {
        QModelIndex ix = makeTrackIndexFromID(trackId);
        emit dataChanged(ix, ix, roles);
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
    track->importEffects(service);
}

QVariant TimelineItemModel::getTrackProperty(int tid, const QString &name) const
{
    return getTrackById_const(tid)->getProperty(name);
}

int TimelineItemModel::getFirstVideoTrackIndex() const
{
    int trackId;
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

bool TimelineItemModel::isInMultiSelection(int cid) const
{
    if (m_temporarySelectionGroup == -1) {
        return false;
    }
    bool res = (m_groups->getRootId(cid) == m_temporarySelectionGroup) && (m_groups->getDirectChildren(m_temporarySelectionGroup).size() != 1);
    return res;
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
    auto it = m_allTracks.cbegin();
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();
    // Make sure all previous track compositing is removed
    if (rebuild) {
        QScopedPointer<Mlt::Service> service(new Mlt::Service(field->get_service()));
        while ((service != nullptr) && service->is_valid()) {
            if (service->type() == transition_type) {
                Mlt::Transition t((mlt_transition)service->get_service());
                QString serviceName = t.get("mlt_service");
                if (t.get_int("internal_added") == 237) {
                    // remove all compositing transitions
                    field->disconnect_service(t);
                }
            }
            service.reset(service->producer());
        }
    }
    QString composite = TransitionsRepository::get()->getCompositingTransition();
    while (it != m_allTracks.cend()) {
        int trackId = getTrackMltIndex((*it)->getId());
        if (!composite.isEmpty() && !(*it)->isAudioTrack()) {
            // video track, add composition
            Mlt::Transition *transition = TransitionsRepository::get()->getTransition(composite);
            transition->set("internal_added", 237);
            transition->set("always_active", 1);
            field->plant_transition(*transition, 0, trackId);
            transition->set_tracks(0, trackId);
        } else if ((*it)->isAudioTrack()) {
            // audio mix
            Mlt::Transition *transition = TransitionsRepository::get()->getTransition(QStringLiteral("mix"));
            transition->set("internal_added", 237);
            transition->set("always_active", 1);
            transition->set("sum", 1);
            field->plant_transition(*transition, 0, trackId);
            transition->set_tracks(0, trackId);
        }
        ++it;
    }
    field->unlock();
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
