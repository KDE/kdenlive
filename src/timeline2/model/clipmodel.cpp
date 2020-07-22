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
#include "clipmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "clipsnapmodel.hpp"
#include "core.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "logger.hpp"
#include "macros.hpp"
#include "timelinemodel.hpp"
#include "trackmodel.hpp"
#include <QDebug>
#include <effects/effectsrepository.hpp>
#include <mlt++/MltProducer.h>
#include <utility>

ClipModel::ClipModel(const std::shared_ptr<TimelineModel> &parent, std::shared_ptr<Mlt::Producer> prod, const QString &binClipId, int id,
                     PlaylistState::ClipState state, double speed)
    : MoveableItem<Mlt::Producer>(parent, id)
    , m_producer(std::move(prod))
    , m_effectStack(EffectStackModel::construct(m_producer, {ObjectType::TimelineClip, m_id}, parent->m_undoStack))
    , m_clipMarkerModel(new ClipSnapModel())
    , m_binClipId(binClipId)
    , forceThumbReload(false)
    , m_currentState(state)
    , m_speed(speed)
    , m_fakeTrack(-1)
    , m_positionOffset(0)
{
    m_producer->set("kdenlive:id", binClipId.toUtf8().constData());
    m_producer->set("_kdenlive_cid", m_id);
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    m_canBeVideo = binClip->hasVideo();
    m_canBeAudio = binClip->hasAudio();
    m_clipType = binClip->clipType();
    if (binClip) {
        m_endlessResize = !binClip->hasLimitedDuration();
    } else {
        m_endlessResize = false;
    }
    QObject::connect(m_effectStack.get(), &EffectStackModel::dataChanged, [&](const QModelIndex &, const QModelIndex &, QVector<int> roles) {
        qDebug() << "// GOT CLIP STACK DATA CHANGE: " << roles;
        if (m_currentTrackId != -1) {
            if (auto ptr = m_parent.lock()) {
                QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                qDebug() << "// GOT CLIP STACK DATA CHANGE DONE: " << ix << " = " << roles;
                emit ptr->dataChanged(ix, ix, roles);
            }
        }
    });
}

int ClipModel::construct(const std::shared_ptr<TimelineModel> &parent, const QString &binClipId, int id, PlaylistState::ClipState state, int audioStream, double speed, bool warp_pitch)
{
    id = (id == -1 ? TimelineModel::getNextId() : id);
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(binClipId);

    // We refine the state according to what the clip can actually produce
    std::pair<bool, bool> videoAudio = stateToBool(state);
    videoAudio.first = videoAudio.first && binClip->hasVideo();
    videoAudio.second = videoAudio.second && binClip->hasAudio();
    state = stateFromBool(videoAudio);
    qDebug()<<"// GET TIMELINE PROD FOR STREAM: "<<audioStream;
    std::shared_ptr<Mlt::Producer> cutProducer = binClip->getTimelineProducer(-1, id, state, audioStream, speed);
    std::shared_ptr<ClipModel> clip(new ClipModel(parent, cutProducer, binClipId, id, state, speed));
    if (!qFuzzyCompare(speed, 1.)) {
        cutProducer->parent().set("warp_pitch", warp_pitch ? 1 : 0);
    }
    qDebug()<<"==== BUILT CLIP STREAM: "<<clip->audioStream();
    TRACE_CONSTR(clip.get(), parent, binClipId, id, state, speed);
    clip->setClipState_lambda(state)();
    parent->registerClip(clip);
    clip->m_clipMarkerModel->setReferenceModel(binClip->getMarkerModel(), speed);
    return id;
}

void ClipModel::allSnaps(std::vector<int> &snaps, int offset)
{
    m_clipMarkerModel->allSnaps(snaps, offset);
}

int ClipModel::construct(const std::shared_ptr<TimelineModel> &parent, const QString &binClipId, const std::shared_ptr<Mlt::Producer> &producer,
                         PlaylistState::ClipState state, int tid, QString originalDecimalPoint)
{

    // we hand the producer to the bin clip, and in return we get a cut to a good master producer
    // We might not be able to use directly the producer that we receive as an argument, because it cannot share the same master producer with any other
    // clipModel (due to a mlt limitation, see ProjectClip doc)

    int id = TimelineModel::getNextId();
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(binClipId);

    // We refine the state according to what the clip can actually produce
    std::pair<bool, bool> videoAudio = stateToBool(state);
    videoAudio.first = videoAudio.first && binClip->hasVideo();
    videoAudio.second = videoAudio.second && binClip->hasAudio();
    state = stateFromBool(videoAudio);

    double speed = 1.0;
    bool warp_pitch = false;
    if (QString::fromUtf8(producer->parent().get("mlt_service")) == QLatin1String("timewarp")) {
        speed = producer->parent().get_double("warp_speed");
        warp_pitch = producer->parent().get_int("warp_pitch");
    }
    auto result = binClip->giveMasterAndGetTimelineProducer(id, producer, state, tid);
    std::shared_ptr<ClipModel> clip(new ClipModel(parent, result.first, binClipId, id, state, speed));
    if (warp_pitch) {
        result.first->parent().set("warp_pitch", 1);
    }
    clip->setClipState_lambda(state)();
    parent->registerClip(clip);
    clip->m_effectStack->importEffects(producer, state, result.second, originalDecimalPoint);
    clip->m_clipMarkerModel->setReferenceModel(binClip->getMarkerModel(), speed);
    return id;
}

void ClipModel::registerClipToBin(std::shared_ptr<Mlt::Producer> service, bool registerProducer)
{
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    if (!binClip) {
        qDebug() << "Error : Bin clip for id: " << m_binClipId << " NOT AVAILABLE!!!";
    }
    qDebug() << "REGISTRATION " << m_id << "ptr count" << m_parent.use_count();
    binClip->registerService(m_parent, m_id, std::move(service), registerProducer);
}

void ClipModel::deregisterClipToBin()
{
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    binClip->deregisterTimelineClip(m_id);
}

ClipModel::~ClipModel() = default;

bool ClipModel::requestResize(int size, bool right, Fun &undo, Fun &redo, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    // qDebug() << "RESIZE CLIP" << m_id << "target size=" << size << "right=" << right << "endless=" << m_endlessResize << "length" <<
    // m_producer->get_length();
    if (!m_endlessResize && (size <= 0 || size > m_producer->get_length())) {
        return false;
    }
    int delta = getPlaytime() - size;
    if (delta == 0) {
        return true;
    }
    int in = m_producer->get_in();
    int out = m_producer->get_out();
    int oldIn = m_position;
    int oldOut = m_position + out - in;
    int old_in = in, old_out = out;
    // check if there is enough space on the chosen side
    if (!m_endlessResize) {
        if (!right && in + delta < 0) {
            return false;
        }
        if (right && (out - delta >= m_producer->get_length())) {
            return false;
        }
    }
    if (right) {
        out -= delta;
    } else {
        in += delta;
    }
    // qDebug() << "Resize facts delta =" << delta << "old in" << old_in << "old_out" << old_out << "in" << in << "out" << out;
    std::function<bool(void)> track_operation = []() { return true; };
    std::function<bool(void)> track_reverse = []() { return true; };
    int outPoint = out;
    int inPoint = in;
    int offset = 0;
    int trackDuration = 0;
    if (m_endlessResize) {
        offset = inPoint;
        outPoint = out - in;
        inPoint = 0;
    }
    if (m_currentTrackId != -1) {
        if (auto ptr = m_parent.lock()) {
            if (ptr->getTrackById(m_currentTrackId)->isLocked()) {
                return false;
            }
            if (right && ptr->getTrackById_const(m_currentTrackId)->isLastClip(getPosition())) {
                trackDuration = ptr->getTrackById_const(m_currentTrackId)->trackDuration();
            }
            track_operation = ptr->getTrackById(m_currentTrackId)->requestClipResize_lambda(m_id, inPoint, outPoint, right);
        } else {
            qDebug() << "Error : Moving clip failed because parent timeline is not available anymore";
            Q_ASSERT(false);
        }
    } else {
        // Ensure producer is long enough
        if (m_endlessResize && outPoint > m_producer->parent().get_length()) {
            m_producer->set("length", outPoint + 1);
        }
    }
    QVector<int> roles{TimelineModel::DurationRole};
    if (!right) {
        roles.push_back(TimelineModel::StartRole);
        roles.push_back(TimelineModel::InPointRole);
    } else {
        roles.push_back(TimelineModel::OutPointRole);
    }

    Fun operation = [this, inPoint, outPoint, roles, oldIn, oldOut, right, logUndo, track_operation]() {
        if (track_operation()) {
            setInOut(inPoint, outPoint);
            if (m_currentTrackId > -1) {
                if (auto ptr = m_parent.lock()) {
                    QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                    ptr->notifyChange(ix, ix, roles);
                    // invalidate timeline preview
                    if (logUndo) {                        
                        if (right) {
                            int newOut = m_position + getOut() - getIn();
                            if (oldOut < newOut) {
                                emit ptr->invalidateZone(oldOut, newOut);
                            } else {
                                emit ptr->invalidateZone(newOut, oldOut);
                            }
                        } else {
                            if (oldIn < m_position) {
                                emit ptr->invalidateZone(oldIn, m_position);
                            } else {
                                emit ptr->invalidateZone(m_position, oldIn);
                            }
                        }
                    }
                }
            }
            return true;
        }
        return false;
    };
    if (operation()) {
        Fun reverse = []() { return true; };
        if (logUndo) {
            // Now, we are in the state in which the timeline should be when we try to revert current action. So we can build the reverse action from here
            if (m_currentTrackId != -1) {
                if (auto ptr = m_parent.lock()) {
                    if (trackDuration > 0) {
                        // Operation changed parent track duration, update effect stack
                        int newDuration = ptr->getTrackById_const(m_currentTrackId)->trackDuration();
                        if (logUndo || trackDuration != newDuration) {
                            // A clip move changed the track duration, update track effects
                            ptr->getTrackById(m_currentTrackId)->m_effectStack->adjustStackLength(true, 0, trackDuration, 0, newDuration, 0, undo, redo, logUndo);
                        }
                    }
                    track_reverse = ptr->getTrackById(m_currentTrackId)->requestClipResize_lambda(m_id, old_in, old_out, right);
                }
            }
            reverse = [this, old_in, old_out, track_reverse, logUndo, oldIn, oldOut, right, roles]() {
                if (track_reverse()) {
                    setInOut(old_in, old_out);
                    if (m_currentTrackId > -1) {
                        if (auto ptr = m_parent.lock()) {
                            QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                            ptr->notifyChange(ix, ix, roles);
                            if (logUndo) {                        
                                if (right) {
                                    int newOut = m_position + getOut() - getIn();
                                    if (oldOut < newOut) {
                                        emit ptr->invalidateZone(oldOut, newOut);
                                    } else {
                                        emit ptr->invalidateZone(newOut, oldOut);
                                    }
                                } else {
                                    if (oldIn < m_position) {
                                        emit ptr->invalidateZone(oldIn, m_position);
                                    } else {
                                        emit ptr->invalidateZone(m_position, oldIn);
                                    }
                                }
                            }
                        }
                    }
                    return true;
                }
                return false;
            };
            qDebug() << "----------\n-----------\n// ADJUSTING EFFECT LENGTH, LOGUNDO " << logUndo << ", " << old_in << "/" << inPoint << ", "
                 << m_producer->get_playtime();

            adjustEffectLength(right, old_in, inPoint, old_out - old_in, m_producer->get_playtime(), offset, reverse, operation, logUndo);
        }
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

const QString ClipModel::getProperty(const QString &name) const
{
    READ_LOCK();
    if (service()->parent().is_valid()) {
        return QString::fromUtf8(service()->parent().get(name.toUtf8().constData()));
    }
    return QString::fromUtf8(service()->get(name.toUtf8().constData()));
}

int ClipModel::getIntProperty(const QString &name) const
{
    READ_LOCK();
    if (service()->parent().is_valid()) {
        return service()->parent().get_int(name.toUtf8().constData());
    }
    return service()->get_int(name.toUtf8().constData());
}

QSize ClipModel::getFrameSize() const
{
    READ_LOCK();
    if (service()->parent().is_valid()) {
        return QSize(service()->parent().get_int("meta.media.width"), service()->parent().get_int("meta.media.height"));
    }
    return {service()->get_int("meta.media.width"), service()->get_int("meta.media.height")};
}

double ClipModel::getDoubleProperty(const QString &name) const
{
    READ_LOCK();
    if (service()->parent().is_valid()) {
        return service()->parent().get_double(name.toUtf8().constData());
    }
    return service()->get_double(name.toUtf8().constData());
}

Mlt::Producer *ClipModel::service() const
{
    READ_LOCK();
    return m_producer.get();
}

std::shared_ptr<Mlt::Producer> ClipModel::getProducer()
{
    READ_LOCK();
    return m_producer;
}

int ClipModel::getPlaytime() const
{
    READ_LOCK();
    return m_producer->get_playtime();
}

void ClipModel::setTimelineEffectsEnabled(bool enabled)
{
    QWriteLocker locker(&m_lock);
    m_effectStack->setEffectStackEnabled(enabled);
}

bool ClipModel::addEffect(const QString &effectId)
{
    QWriteLocker locker(&m_lock);
    AssetListType::AssetType type = EffectsRepository::get()->getType(effectId);
    if (type == AssetListType::AssetType::Audio || type == AssetListType::AssetType::CustomAudio) {
        if (m_currentState == PlaylistState::VideoOnly) {
            return false;
        }
    } else if (m_currentState == PlaylistState::AudioOnly) {
        return false;
    }
    m_effectStack->appendEffect(effectId, true);
    return true;
}

bool ClipModel::copyEffect(const std::shared_ptr<EffectStackModel> &stackModel, int rowId)
{
    QWriteLocker locker(&m_lock);
    QDomDocument doc;
    m_effectStack->copyXmlEffect(stackModel->rowToXml(rowId, doc));
    return true;
}

bool ClipModel::importEffects(std::shared_ptr<EffectStackModel> stackModel)
{
    QWriteLocker locker(&m_lock);
    m_effectStack->importEffects(std::move(stackModel), m_currentState);
    return true;
}

bool ClipModel::importEffects(std::weak_ptr<Mlt::Service> service)
{
    QWriteLocker locker(&m_lock);
    m_effectStack->importEffects(std::move(service), m_currentState);
    return true;
}

bool ClipModel::removeFade(bool fromStart)
{
    QWriteLocker locker(&m_lock);
    m_effectStack->removeFade(fromStart);
    return true;
}

bool ClipModel::adjustEffectLength(bool adjustFromEnd, int oldIn, int newIn, int oldDuration, int duration, int offset, Fun &undo, Fun &redo, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    return m_effectStack->adjustStackLength(adjustFromEnd, oldIn, oldDuration, newIn, duration, offset, undo, redo, logUndo);
}

bool ClipModel::adjustEffectLength(const QString &effectName, int duration, int originalDuration, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    qDebug() << ".... ADJUSTING FADE LENGTH: " << duration << " / " << effectName;
    Fun operation = [this, duration, effectName, originalDuration]() {
        return m_effectStack->adjustFadeLength(duration, effectName == QLatin1String("fadein") || effectName == QLatin1String("fade_to_black"), audioEnabled(),
                                               !isAudioOnly(), originalDuration > 0);
    };
    if (operation() && originalDuration > 0) {
        Fun reverse = [this, originalDuration, effectName]() {
            return m_effectStack->adjustFadeLength(originalDuration, effectName == QLatin1String("fadein") || effectName == QLatin1String("fade_to_black"),
                                                   audioEnabled(), !isAudioOnly(), true);
        };
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
    }
    return true;
}

bool ClipModel::audioEnabled() const
{
    READ_LOCK();
    return stateToBool(m_currentState).second;
}

bool ClipModel::isAudioOnly() const
{
    READ_LOCK();
    return m_currentState == PlaylistState::AudioOnly;
}

void ClipModel::refreshProducerFromBin(int trackId, PlaylistState::ClipState state, int stream, double speed, bool hasPitch)
{
    // We require that the producer is not in the track when we refresh the producer, because otherwise the modification will not be propagated. Remove the clip
    // first, refresh, and then replant.
    QWriteLocker locker(&m_lock);
    int in = getIn();
    int out = getOut();
    if (!qFuzzyCompare(speed, m_speed) && !qFuzzyIsNull(speed)) {
        in = in * std::abs(m_speed / speed);
        out = in + getPlaytime() - 1;
        // prevent going out of the clip's range
        out = std::min(out, int(double(m_producer->get_length()) * std::abs(m_speed / speed)) - 1);
        m_speed = speed;
        qDebug() << "changing speed" << in << out << m_speed;
    }
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    std::shared_ptr<Mlt::Producer> binProducer = binClip->getTimelineProducer(trackId, m_id, state, stream, m_speed);
    m_producer = std::move(binProducer);
    m_producer->set_in_and_out(in, out);
    if (hasPitch) {
        // Check if pitch shift is enabled
        m_producer->parent().set("warp_pitch", 1);
    } else if (!qFuzzyCompare(m_speed, 1.)) {
        m_producer->parent().set("warp_pitch", 0);
    }
    // replant effect stack in updated service
    m_effectStack->resetService(m_producer);
    m_producer->set("kdenlive:id", binClip->clipId().toUtf8().constData());
    m_producer->set("_kdenlive_cid", m_id);
    m_endlessResize = !binClip->hasLimitedDuration();
}

void ClipModel::refreshProducerFromBin(int trackId)
{
    if (trackId == -1) {
        trackId = m_currentTrackId;
    }
    bool hasPitch = false;
    if (!qFuzzyCompare(getSpeed(), 1.)) {
        hasPitch = m_producer->parent().get_int("warp_pitch") == 1;
    }
    int stream = m_producer->parent().get_int("audio_index");
    refreshProducerFromBin(trackId, m_currentState, stream, 0, hasPitch);
}

bool ClipModel::useTimewarpProducer(double speed, bool pitchCompensate, bool changeDuration, Fun &undo, Fun &redo)
{
    if (m_endlessResize) {
        // no timewarp for endless producers
        return false;
    }
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    double previousSpeed = getSpeed();
    int oldDuration = getPlaytime();
    int newDuration = qRound(oldDuration * std::fabs(m_speed / speed));
    int oldOut = getOut();
    int oldIn = getIn();
    bool revertSpeed = false;
    if (speed < 0) {
        if (previousSpeed > 0) {
            revertSpeed = true;
        }
    } else if (previousSpeed < 0) {
        revertSpeed = true;
    }
    bool hasPitch = getIntProperty(QStringLiteral("warp_pitch"));
    int audioStream = getIntProperty(QStringLiteral("audio_index"));
    auto operation = useTimewarpProducer_lambda(speed, audioStream, pitchCompensate);
    auto reverse = useTimewarpProducer_lambda(previousSpeed, audioStream, hasPitch);
    if (revertSpeed || (changeDuration && oldOut >= newDuration)) {
        // in that case, we are going to shrink the clip when changing the producer. We must undo that when reloading the old producer
        reverse = [reverse, oldIn, oldOut, this]() {
            bool res = reverse();
            if (res) {
                setInOut(oldIn, oldOut);
            }
            return res;
        };
    }
    if (revertSpeed) {
        int in = getIn();
        int out = getOut();
        in = qMax(0, qRound((m_producer->get_length() - 1 - out)* std::fabs(m_speed/speed) + 0.5));
        out = in + newDuration;
        operation = [operation, in, out, this]() {
            bool res = operation();
            if (res) {
                setInOut(in, out);
            } else {
            }
            return res;
        };

    }
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, local_undo, local_redo);
        // When calculating duration, result can be a few frames longer than possible duration so adjust
        if (changeDuration) {
            int requestedDuration = qMin(newDuration, getMaxDuration() - getIn());
            if (requestedDuration != getPlaytime()) {
                bool res = requestResize(requestedDuration, true, local_undo, local_redo, true);
                if (!res) {
                    qDebug()<<"==== CLIP WARP UPDATE DURATION FAILED!!!!";
                    local_undo();
                    return false;
                }
            }
        }
        adjustEffectLength(false, oldIn, getIn(), oldOut - oldIn, m_producer->get_playtime(), 0, local_undo, local_redo, true);
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
        return true;
    }
    qDebug() << "tw: operation fail";
    return false;
}

Fun ClipModel::useTimewarpProducer_lambda(double speed, int stream, bool pitchCompensate)
{
    QWriteLocker locker(&m_lock);
    return [speed, stream, pitchCompensate, this]() {
        qDebug() << "timeWarp producer" << speed;
        refreshProducerFromBin(m_currentTrackId, m_currentState, stream, speed, pitchCompensate);
        return true;
    };
}

const QString &ClipModel::binId() const
{
    return m_binClipId;
}

std::shared_ptr<MarkerListModel> ClipModel::getMarkerModel() const
{
    READ_LOCK();
    return pCore->projectItemModel()->getClipByBinID(m_binClipId)->getMarkerModel();
}

int ClipModel::audioChannels() const
{
    READ_LOCK();
    return pCore->projectItemModel()->getClipByBinID(m_binClipId)->audioChannels();
}

bool ClipModel::audioMultiStream() const
{
    READ_LOCK();
    return pCore->projectItemModel()->getClipByBinID(m_binClipId)->audioStreamsCount() > 1;
}

int ClipModel::audioStream() const
{
    return m_producer->parent().get_int("audio_index");
}

int ClipModel::audioStreamIndex() const
{
    READ_LOCK();
    QList <int> streams = pCore->projectItemModel()->getClipByBinID(m_binClipId)->audioStreams().keys();
    return streams.indexOf(m_producer->parent().get_int("audio_index")) + 1;
}

int ClipModel::fadeIn() const
{
    return m_effectStack->getFadePosition(true);
}

int ClipModel::fadeOut() const
{
    return m_effectStack->getFadePosition(false);
}

double ClipModel::getSpeed() const
{
    return m_speed;
}

KeyframeModel *ClipModel::getKeyframeModel()
{
    return m_effectStack->getEffectKeyframeModel();
}

bool ClipModel::showKeyframes() const
{
    READ_LOCK();
    return !service()->get_int("kdenlive:hide_keyframes");
}

void ClipModel::setShowKeyframes(bool show)
{
    QWriteLocker locker(&m_lock);
    service()->set("kdenlive:hide_keyframes", (int)!show);
}

void ClipModel::setPosition(int pos)
{
    MoveableItem::setPosition(pos);
    m_clipMarkerModel->updateSnapModelPos(pos);
}

void ClipModel::setInOut(int in, int out)
{
    MoveableItem::setInOut(in, out);
    m_clipMarkerModel->updateSnapModelInOut(std::pair<int, int>(in, out));
}

void ClipModel::setCurrentTrackId(int tid, bool finalMove)
{
    if (tid == m_currentTrackId) {
        return;
    }
    bool registerSnap = m_currentTrackId == -1 && tid > -1;

    if (m_currentTrackId > -1 && tid == -1) {
        // Removing clip
        m_clipMarkerModel->deregisterSnapModel();
    }
    MoveableItem::setCurrentTrackId(tid, finalMove);
    if (registerSnap) {
        if (auto ptr = m_parent.lock()) {
            m_clipMarkerModel->registerSnapModel(ptr->m_snaps, getPosition(), getIn(), getOut(), m_speed);
        }
    }

    if (finalMove && tid != -1 && m_lastTrackId != m_currentTrackId) {
        refreshProducerFromBin(m_currentTrackId);
        m_lastTrackId = m_currentTrackId;
    }
}

Fun ClipModel::setClipState_lambda(PlaylistState::ClipState state)
{
    QWriteLocker locker(&m_lock);
    return [this, state]() {
        if (auto ptr = m_parent.lock()) {
            m_currentState = state;
            // Enforce producer reload
            m_lastTrackId = -1;
            if (m_currentTrackId != -1 && ptr->isClip(m_id)) { // if this is false, the clip is being created. Don't update model in that case
                refreshProducerFromBin(m_currentTrackId);
                QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                emit ptr->dataChanged(ix, ix, {TimelineModel::StatusRole});
            }
            return true;
        }
        return false;
    };
}

bool ClipModel::setClipState(PlaylistState::ClipState state, Fun &undo, Fun &redo)
{
    if (state == PlaylistState::VideoOnly && !canBeVideo()) {
        return false;
    }
    if (state == PlaylistState::AudioOnly && !canBeAudio()) {
        return false;
    }
    if (state == m_currentState) {
        return true;
    }
    auto old_state = m_currentState;
    auto operation = setClipState_lambda(state);
    if (operation()) {
        auto reverse = setClipState_lambda(old_state);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

PlaylistState::ClipState ClipModel::clipState() const
{
    READ_LOCK();
    return m_currentState;
}

ClipType::ProducerType ClipModel::clipType() const
{
    READ_LOCK();
    return m_clipType;
}

void ClipModel::passTimelineProperties(const std::shared_ptr<ClipModel> &other)
{
    READ_LOCK();
    Mlt::Properties source(m_producer->get_properties());
    Mlt::Properties dest(other->service()->get_properties());
    dest.pass_list(source, "kdenlive:hide_keyframes,kdenlive:activeeffect");
}

bool ClipModel::canBeVideo() const
{
    return m_canBeVideo;
}

bool ClipModel::canBeAudio() const
{
    return m_canBeAudio;
}

const QString ClipModel::effectNames() const
{
    READ_LOCK();
    return m_effectStack->effectNames();
}

int ClipModel::getFakeTrackId() const
{
    return m_fakeTrack;
}

void ClipModel::setFakeTrackId(int fid)
{
    m_fakeTrack = fid;
}

int ClipModel::getFakePosition() const
{
    return m_fakePosition;
}

void ClipModel::setFakePosition(int fid)
{
    m_fakePosition = fid;
}

QDomElement ClipModel::toXml(QDomDocument &document)
{
    QDomElement container = document.createElement(QStringLiteral("clip"));
    container.setAttribute(QStringLiteral("binid"), m_binClipId);
    container.setAttribute(QStringLiteral("id"), m_id);
    container.setAttribute(QStringLiteral("in"), getIn());
    container.setAttribute(QStringLiteral("out"), getOut());
    container.setAttribute(QStringLiteral("position"), getPosition());
    container.setAttribute(QStringLiteral("state"), (int)m_currentState);
    if (auto ptr = m_parent.lock()) {
        int trackId = ptr->getTrackPosition(m_currentTrackId);
        container.setAttribute(QStringLiteral("track"), trackId);
        if (ptr->isAudioTrack(getCurrentTrackId())) {
            container.setAttribute(QStringLiteral("audioTrack"), 1);
            int partner = ptr->getClipSplitPartner(m_id);
            if (partner != -1) {
                int mirrorId = ptr->getMirrorVideoTrackId(m_currentTrackId);
                if (mirrorId > -1) {
                    mirrorId = ptr->getTrackPosition(mirrorId);
                }
                container.setAttribute(QStringLiteral("mirrorTrack"), mirrorId);
            } else {
                container.setAttribute(QStringLiteral("mirrorTrack"), QStringLiteral("-1"));
            }
        }
    }
    container.setAttribute(QStringLiteral("speed"), QString::number(m_speed, 'f'));
    container.setAttribute(QStringLiteral("audioStream"), getIntProperty(QStringLiteral("audio_index")));
    if (!qFuzzyCompare(m_speed, 1.)) {
        container.setAttribute(QStringLiteral("warp_pitch"), getIntProperty(QStringLiteral("warp_pitch")));
    }
    container.appendChild(m_effectStack->toXml(document));
    return container;
}

bool ClipModel::checkConsistency()
{
    if (!m_effectStack->checkConsistency()) {
        qDebug() << "Consistency check failed for effecstack";
        return false;
    }
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    auto instances = binClip->timelineInstances();
    bool found = false;
    for (const auto &i : qAsConst(instances)) {
        if (i == m_id) {
            found = true;
            break;
        }
    }
    if (!found) {
        qDebug() << "ERROR: binClip doesn't acknowledge timeline clip existence";
        return false;
    }

    if (m_currentState == PlaylistState::VideoOnly && !m_canBeVideo) {
        qDebug() << "ERROR: clip is in video state but doesn't have video";
        return false;
    }
    if (m_currentState == PlaylistState::AudioOnly && !m_canBeAudio) {
        qDebug() << "ERROR: clip is in video state but doesn't have video";
        return false;
    }
    // TODO: check speed

    return true;
}

int ClipModel::getSubPlaylistIndex() const
{
    return m_subPlaylistIndex;
}

void ClipModel::setSubPlaylistIndex(int index)
{
    m_subPlaylistIndex = index;
}

void ClipModel::setOffset(int offset)
{
    m_positionOffset = offset;
    if (auto ptr = m_parent.lock()) {
        QModelIndex ix = ptr->makeClipIndexFromID(m_id);
        emit ptr->dataChanged(ix, ix, {TimelineModel::PositionOffsetRole});
    }
}

void ClipModel::setGrab(bool grab)
{
    QWriteLocker locker(&m_lock);
    if (grab == m_grabbed) {
        return;
    }
    m_grabbed = grab;
    if (auto ptr = m_parent.lock()) {
        QModelIndex ix = ptr->makeClipIndexFromID(m_id);
        emit ptr->dataChanged(ix, ix, {TimelineModel::GrabbedRole});
    }
}

void ClipModel::setSelected(bool sel)
{
    QWriteLocker locker(&m_lock);
    if (sel == selected) {
        return;
    }
    selected = sel;
    if (auto ptr = m_parent.lock()) {
        if (m_currentTrackId != -1) {
            QModelIndex ix = ptr->makeClipIndexFromID(m_id);
            emit ptr->dataChanged(ix, ix, {TimelineModel::SelectedRole});
        }
    }
}

void ClipModel::clearOffset()
{
    if (m_positionOffset != 0) {
        setOffset(0);
    }
}

int ClipModel::getOffset() const
{
    return m_positionOffset;
}

int ClipModel::getMaxDuration() const
{
    READ_LOCK();
    if (m_endlessResize) {
        return -1;
    }
    return m_producer->get_length();
}

const QString ClipModel::clipName()
{
    return pCore->projectItemModel()->getClipByBinID(m_binClipId)->clipName();
}

bool ClipModel::isProxied() const
{
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    return binClip->hasProxy();
}
