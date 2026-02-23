/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "clipmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "clipsnapmodel.hpp"
#include "core.h"
#include "effects/effectstack/model/effectitemmodel.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#ifdef CRASH_AUTO_TEST
#include "logger.hpp"
#else
#define TRACE_CONSTR(...)
#endif
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
    , m_effectStack(EffectStackModel::construct(m_producer, ObjectId(KdenliveObjectType::TimelineClip, m_id, parent->uuid()), parent->m_undoStack))
    , m_clipMarkerModel(new ClipSnapModel())
    , m_binClipId(binClipId)
    , forceThumbReload(false)
    , m_currentState(state)
    , m_speed(speed)
    , m_positionOffset(0)
    , m_subPlaylistIndex(0)
    , m_mixDuration(0)
    , m_mixCutPos(0)
    , m_hasTimeRemap(hasTimeRemap())
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
    QObject::connect(m_effectStack.get(), &EffectStackModel::dataChanged, m_effectStack.get(),
                     [&](const QModelIndex &, const QModelIndex &, const QVector<int> &roles) {
                         if (m_currentTrackId != -1) {
                             if (auto ptr = m_parent.lock()) {
                                 QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                                 Q_EMIT ptr->dataChanged(ix, ix, roles);
                             }
                         }
                     });
    QObject::connect(m_effectStack.get(), &EffectStackModel::customDataChanged, m_effectStack.get(),
                     [&](const QModelIndex &, const QModelIndex &, const QVector<int> &roles) {
                         if (m_currentTrackId != -1) {
                             if (auto ptr = m_parent.lock()) {
                                 QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                                 Q_EMIT ptr->dataChanged(ix, ix, roles);
                             }
                         }
                     });
}

int ClipModel::construct(const std::shared_ptr<TimelineModel> &parent, const QString &binClipId, int id, PlaylistState::ClipState state, int audioStream,
                         double speed, bool warp_pitch)
{
    id = (id == -1 ? TimelineModel::getNextId() : id);
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(binClipId);

    // We refine the state according to what the clip can actually produce
    std::pair<bool, bool> videoAudio = stateToBool(state);
    videoAudio.first = videoAudio.first && binClip->hasVideo();
    videoAudio.second = videoAudio.second && binClip->hasAudio();
    state = stateFromBool(videoAudio);
    qDebug() << "// GET TIMELINE PROD FOR STREAM: " << audioStream;
    std::shared_ptr<Mlt::Producer> cutProducer = binClip->getTimelineProducer(-1, id, state, audioStream, speed);
    std::shared_ptr<ClipModel> clip(new ClipModel(parent, cutProducer, binClipId, id, state, speed));
    if (!qFuzzyCompare(speed, 1.)) {
        cutProducer->parent().set("warp_pitch", warp_pitch ? 1 : 0);
    }
    TRACE_CONSTR(clip.get(), parent, binClipId, id, state, speed);
    clip->setClipState_lambda(state)();
    parent->registerClip(clip);
    clip->m_clipMarkerModel->setReferenceModel(binClip->getMarkerModel(), speed);
    return id;
}

void ClipModel::allSnaps(std::vector<int> &snaps, int offset) const
{
    m_clipMarkerModel->allSnaps(snaps, offset);
}

int ClipModel::construct(const std::shared_ptr<TimelineModel> &parent, const QString &binClipId, const std::shared_ptr<Mlt::Producer> &producer,
                         PlaylistState::ClipState state, int tid, const QString &originalDecimalPoint, int playlist)
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
    if (producer->parent().property_exists("warp_speed")) {
        speed = producer->parent().get_double("warp_speed");
        warp_pitch = producer->parent().get_int("warp_pitch");
    }
    auto result = binClip->giveMasterAndGetTimelineProducer(id, producer, state, tid, playlist == 1);
    std::shared_ptr<ClipModel> clip(new ClipModel(parent, result.first, binClipId, id, state, speed));
    if (warp_pitch) {
        result.first->parent().set("warp_pitch", 1);
    }
    clip->setClipState_lambda(state)();
    clip->setSubPlaylistIndex(playlist, -1);
    parent->registerClip(clip);
    if (clip->m_endlessResize) {
        // Ensure parent is long enough
        if (producer->parent().get_out() < producer->get_length() - 1) {
            int out = producer->get_length();
            producer->parent().set("length", out + 1);
            producer->parent().set("out", out);
        }
    }
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
    binClip->registerService(m_parent, m_id, std::move(service), getClipInfo(), registerProducer);
}

ClipModel::TimelineClipInfo ClipModel::getClipInfo()
{
    TimelineClipInfo info;
    info.trackId = getCurrentTrackId();
    info.audioStream = audioStream();
    info.speed = m_speed;
    info.pitchShift = m_producer->parent().get_int("warp_pitch");
    return info;
}

void ClipModel::deregisterClipToBin(const QUuid &uuid)
{
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    binClip->deregisterTimelineClip(m_id, isAudioOnly(), getClipInfo(), uuid);
}

ClipModel::~ClipModel() = default;

bool ClipModel::requestResize(int size, bool right, Fun &undo, Fun &redo, bool logUndo, bool hasMix)
{
    QWriteLocker locker(&m_lock);
    int maxDuration = m_producer->get_length();
    if (!m_endlessResize && (size <= 0 || size > maxDuration) && !hasTimeRemap()) {
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
        if (right && (out - delta >= maxDuration) && !hasTimeRemap()) {
            return false;
        }
    }
    if (right) {
        out -= delta;
    } else {
        in += delta;
    }
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
    bool closing = false;
    // Ensure producer is long enough
    if (m_endlessResize && outPoint > maxDuration) {
        m_producer->parent().set("length", outPoint + 1);
        m_producer->parent().set("out", outPoint);
        m_producer->set("length", outPoint + 1);
    }
    if (m_currentTrackId != -1) {
        if (auto ptr = m_parent.lock()) {
            if (ptr->getTrackById(m_currentTrackId)->isLocked()) {
                return false;
            }
            closing = ptr->m_closing;
            if (right && ptr->getTrackById_const(m_currentTrackId)->isLastClip(getPosition())) {
                trackDuration = ptr->getTrackById_const(m_currentTrackId)->trackDuration();
            }
            track_operation = ptr->getTrackById(m_currentTrackId)->requestClipResize_lambda(m_id, inPoint, outPoint, right, hasMix, logUndo);
        } else {
            qDebug() << "Error : Moving clip failed because parent timeline is not available anymore";
            Q_ASSERT(false);
        }
    }
    QVector<int> roles{TimelineModel::DurationRole};
    if (!right) {
        roles.push_back(TimelineModel::StartRole);
        roles.push_back(TimelineModel::InPointRole);
    } else {
        roles.push_back(TimelineModel::OutPointRole);
    }
    Fun operation = [this, inPoint, outPoint, roles, logUndo, track_operation]() {
        if (track_operation()) {
            setInOut(inPoint, outPoint);
            if (logUndo && !m_endlessResize) {
                Q_EMIT pCore->clipInstanceResized(m_binClipId);
            }
            return true;
        }
        return false;
    };
    Fun postProcess = [this, roles, oldIn, oldOut, right, logUndo]() {
        if (m_currentTrackId > -1) {
            if (auto ptr = m_parent.lock()) {
                QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                ptr->notifyChange(ix, ix, roles);
                // invalidate timeline preview
                if (logUndo) {
                    bool isAudio = ptr->getTrackById_const(m_currentTrackId)->isAudioTrack();
                    if (right) {
                        int newOut = m_position + getOut() - getIn();
                        if (oldOut < newOut) {
                            isAudio ? Q_EMIT ptr->invalidateAudioZone(oldOut, newOut) : Q_EMIT ptr->invalidateZone(oldOut, newOut);
                        } else {
                            isAudio ? Q_EMIT ptr->invalidateAudioZone(newOut, oldOut) : Q_EMIT ptr->invalidateZone(newOut, oldOut);
                        }
                    } else {
                        if (oldIn < m_position) {
                            isAudio ? Q_EMIT ptr->invalidateAudioZone(oldIn, m_position) : Q_EMIT ptr->invalidateZone(oldIn, m_position);
                        } else {
                            isAudio ? Q_EMIT ptr->invalidateAudioZone(m_position, oldIn) : ptr->invalidateZone(m_position, oldIn);
                        }
                    }
                }
            }
        }
        return true;
    };
    if (operation()) {
        // Now, we are in the state in which the timeline should be when we try to revert current action. So we can build the reverse action from here
        if (m_currentTrackId != -1) {
            if (auto ptr = m_parent.lock()) {
                if (trackDuration > 0 && !closing) {
                    // Operation changed parent track duration, update effect stack
                    int newDuration = ptr->getTrackById_const(m_currentTrackId)->trackDuration();
                    if (logUndo || trackDuration != newDuration) {
                        // A clip move changed the track duration, update track effects
                        ptr->getTrackById(m_currentTrackId)->m_effectStack->adjustStackLength(true, 0, trackDuration, 0, newDuration, 0, undo, redo, logUndo);
                    }
                }
                track_reverse = ptr->getTrackById(m_currentTrackId)->requestClipResize_lambda(m_id, old_in, old_out, right, hasMix, logUndo);
            }
        }
        Fun reverse = [this, old_in, old_out, track_reverse, logUndo, roles]() {
            if (track_reverse()) {
                setInOut(old_in, old_out);
                if (logUndo && !m_endlessResize) {
                    Q_EMIT pCore->clipInstanceResized(m_binClipId);
                }
                return true;
            }
            qDebug() << "============\n+++++++++++++++++\nREVRSE TRACK OP FAILED FOR: " << m_id << "\n\n++++++++++++++++";
            return false;
        };
        Fun preProcess = [this, roles, oldIn, oldOut, newIn = m_position, newOut = m_position + getOut() - getIn(), right, logUndo]() {
            if (m_currentTrackId > -1) {
                if (auto ptr = m_parent.lock()) {
                    QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                    ptr->notifyChange(ix, ix, roles);
                    // invalidate timeline preview
                    if (logUndo) {
                        bool isAudio = ptr->getTrackById_const(m_currentTrackId)->isAudioTrack();
                        if (right) {
                            if (oldOut < newOut) {
                                isAudio ? Q_EMIT ptr->invalidateAudioZone(oldOut, newOut) : Q_EMIT ptr->invalidateZone(oldOut, newOut);
                            } else {
                                isAudio ? Q_EMIT ptr->invalidateAudioZone(newOut, oldOut) : Q_EMIT ptr->invalidateZone(newOut, oldOut);
                            }
                        } else {
                            if (oldIn < newIn) {
                                isAudio ? Q_EMIT ptr->invalidateAudioZone(oldIn, newIn) : Q_EMIT ptr->invalidateZone(oldIn, newIn);
                            } else {
                                isAudio ? Q_EMIT ptr->invalidateAudioZone(newIn, oldIn) : Q_EMIT ptr->invalidateZone(newIn, oldIn);
                            }
                        }
                    }
                }
            }
            return true;
        };
        if (logUndo) {
            qDebug() << "----------\n-----------\n// ADJUSTING EFFECT LENGTH, LOGUNDO " << logUndo << ", " << old_in << "/" << inPoint << "-" << outPoint
                     << ", " << m_producer->get_playtime();
        }

        if (!closing && logUndo) {
            if (hasTimeRemap()) {
                // Add undo /redo ops to resize keyframes
                requestRemapResize(in, out, old_in, old_out, reverse, operation);
            }
            adjustEffectLength(right, old_in, inPoint, old_out - old_in, m_producer->get_playtime(), offset, reverse, operation, logUndo);
        }
        postProcess();
        PUSH_LAMBDA(postProcess, operation);
        PUSH_LAMBDA(preProcess, reverse);
        UPDATE_UNDO_REDO(operation, reverse, undo, redo);
        return true;
    }
    return false;
}

bool ClipModel::requestSlip(int offset, Fun &undo, Fun &redo, bool logUndo)
{
    QWriteLocker locker(&m_lock);
    if (offset == 0 || m_endlessResize) {
        return true;
    }
    int in = m_producer->get_in();
    int out = m_producer->get_out();
    int old_in = in, old_out = out;
    offset = qBound(out - m_producer->get_length() + 1, offset, in);
    int inPoint = in - offset;
    int outPoint = out - offset;

    Q_ASSERT(outPoint >= m_producer->get_playtime() - 1);
    Q_ASSERT(outPoint < m_producer->get_length());
    Q_ASSERT(inPoint >= 0);
    Q_ASSERT(inPoint <= m_producer->get_length() - m_producer->get_playtime());
    Q_ASSERT(inPoint <= outPoint);
    Q_ASSERT(out - in == outPoint - inPoint);

    if (m_currentTrackId != -1) {
        if (auto ptr = m_parent.lock()) {
            if (ptr->getTrackById(m_currentTrackId)->isLocked()) {
                return false;
            }
        } else {
            qDebug() << "Error : Slipping clip failed because parent timeline is not available anymore";
            Q_ASSERT(false);
        }
    }
    QVector<int> roles{TimelineModel::StartRole, TimelineModel::InPointRole, TimelineModel::OutPointRole};
    Fun operation = [this, inPoint, outPoint, roles, logUndo]() {
        setInOut(inPoint, outPoint);
        if (m_currentTrackId > -1) {
            if (auto ptr = m_parent.lock()) {
                QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                ptr->notifyChange(ix, ix, roles);
                pCore->refreshProjectMonitorOnce();
                // invalidate timeline preview
                if (logUndo) {
                    bool isAudio = ptr->getTrackById_const(m_currentTrackId)->isAudioTrack();
                    isAudio ? Q_EMIT ptr->invalidateAudioZone(m_position, m_position + getPlaytime())
                            : Q_EMIT ptr->invalidateZone(m_position, m_position + getPlaytime());
                }
            }
        }
        return true;
    };

    qDebug() << "=== SLIP CLIP"
             << "pos" << m_position << "offset" << offset << "old_in" << old_in << "old_out" << old_out << "inPoint" << inPoint << "outPoint" << outPoint
             << "endless" << m_endlessResize << "playtime" << getPlaytime() << "fulllength" << m_producer->get_length();
    ;

    if (operation()) {
        Fun reverse = []() { return true; };
        // Now, we are in the state in which the timeline should be when we try to revert current action. So we can build the reverse action from here
        reverse = [this, old_in, old_out, logUndo, roles]() {
            setInOut(old_in, old_out);
            if (m_currentTrackId > -1) {
                if (auto ptr = m_parent.lock()) {
                    QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                    ptr->notifyChange(ix, ix, roles);
                    pCore->refreshProjectMonitorOnce();
                    if (logUndo) {
                        bool isAudio = ptr->getTrackById_const(m_currentTrackId)->isAudioTrack();
                        isAudio ? Q_EMIT ptr->invalidateAudioZone(m_position, m_position + getPlaytime())
                                : Q_EMIT ptr->invalidateZone(m_position, m_position + getPlaytime());
                    }
                }
            }
            return true;
        };
        qDebug() << "----------\n-----------\n// ADJUSTING EFFECT LENGTH, LOGUNDO " << logUndo << ", " << old_in << "/" << inPoint << ", "
                 << m_producer->get_playtime();

        adjustEffectLength(true, old_in, inPoint, old_out - old_in, m_producer->get_playtime(), offset, reverse, operation, logUndo);
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
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    if (binClip) {
        return binClip->getFrameSize();
    }
    return QSize();
}

Mlt::Producer *ClipModel::service() const
{
    READ_LOCK();
    return m_producer.get();
}

bool ClipModel::isChain() const
{
    READ_LOCK();
    return m_producer->parent().type() == mlt_service_chain_type;
}

bool ClipModel::hasTimeRemap() const
{
    READ_LOCK();
    if (m_producer->parent().type() == mlt_service_chain_type) {
        Mlt::Chain fromChain(m_producer->parent());
        int count = fromChain.link_count();
        for (int i = 0; i < count; i++) {
            QScopedPointer<Mlt::Link> fromLink(fromChain.link(i));
            if (fromLink && fromLink->is_valid() && fromLink->property_exists("mlt_service")) {
                if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                    return true;
                }
            }
        }
    }
    return false;
}

void ClipModel::requestRemapResize(int inPoint, int outPoint, int oldIn, int oldOut, Fun &undo, Fun &redo)
{
    Mlt::Chain fromChain(m_producer->parent());
    int count = fromChain.link_count();
    for (int ix = 0; ix < count; ix++) {
        QScopedPointer<Mlt::Link> fromLink(fromChain.link(ix));
        if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
            if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                // Found a timeremap effect, read params
                std::shared_ptr<Mlt::Link> link = std::make_shared<Mlt::Link>(fromChain.link(ix)->get_link());
                if (!link->property_exists("time_map")) {
                    link->set("time_map", fromLink->get("map"));
                }
                (void)link->anim_get_rect("time_map", 0);
                Mlt::Animation anim = link->get_animation("time_map");
                QString oldKfrData = anim.serialize_cut(mlt_time_clock, 0, m_producer->get_length());
                QStringList str = oldKfrData.split(QLatin1Char(';'));
                QMap<int, int> keyframes;
                for (auto &s : str) {
                    int pos = m_producer->time_to_frames(s.section(QLatin1Char('='), 0, 0).toUtf8().constData());
                    int val = GenTime(s.section(QLatin1Char('='), 1).toDouble()).frames(pCore->getCurrentFps());
                    if (s == str.constLast()) {
                        // HACK: we always set last keyframe 1 frame after in MLT to ensure we have a correct last frame
                        pos--;
                    }
                    keyframes.insert(pos, val);
                }
                if (keyframes.contains(inPoint) && keyframes.lastKey() == outPoint) {
                    // Nothing to do, abort
                    return;
                }
                // Adjust start keyframes
                QList<int> toDelete;
                QMap<int, int> toAdd;
                if (inPoint != oldIn && !keyframes.contains(inPoint)) {
                    if (inPoint < oldIn) {
                        // Move oldIn keyframe to new in
                        if (keyframes.contains(oldIn)) {
                            int delta = oldIn - inPoint;
                            toAdd.insert(inPoint, qMax(0, keyframes.value(oldIn) - delta));
                            toDelete << oldIn;
                        } else {
                            // Move first keyframe available
                            bool found = false;
                            QMapIterator<int, int> i(keyframes);
                            while (i.hasNext()) {
                                i.next();
                                if (i.key() > oldIn) {
                                    int delta = i.key() - inPoint;
                                    toAdd.insert(inPoint, qMax(0, i.value() - delta));
                                    toDelete << i.key();
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                // Add standard keyframe
                                toAdd.insert(inPoint, inPoint);
                            }
                        }
                    } else if (outPoint != oldOut && !keyframes.contains(outPoint)) {
                        // inpoint moved forwards, delete previous
                        if (keyframes.contains(oldIn)) {
                            int delta = inPoint - oldIn;
                            toAdd.insert(inPoint, qMax(0, keyframes.value(oldIn) + delta));
                        } else {
                            toAdd.insert(inPoint, inPoint);
                        }
                        // Remove all keyframes before
                        QMapIterator<int, int> i(keyframes);
                        while (i.hasNext()) {
                            i.next();
                            if (i.key() == 0) {
                                // Don't remove 0 keyframe
                                continue;
                            }
                            if (i.key() < inPoint) {
                                toDelete << i.key();
                            } else {
                                break;
                            }
                        }
                    }
                }
                if (outPoint != oldOut) {
                    if (outPoint > oldOut) {
                        if (keyframes.contains(oldOut)) {
                            int delta = outPoint - oldOut;
                            toAdd.insert(outPoint, keyframes.value(oldOut) + delta);
                            toDelete << oldOut;
                        } else {
                            // Add default keyframe
                            toAdd.insert(outPoint, outPoint);
                        }
                    } else {
                        // Clip reduced
                        if (keyframes.contains(oldOut)) {
                            int delta = oldOut - outPoint;
                            toAdd.insert(outPoint, keyframes.value(oldOut) - delta);
                        } else {
                            // Add default keyframe
                            toAdd.insert(outPoint, outPoint);
                        }
                        // Delete all keyframes after outpoint
                        QMapIterator<int, int> i(keyframes);
                        while (i.hasNext()) {
                            i.next();
                            if (i.key() > outPoint) {
                                toDelete << i.key();
                            }
                        }
                    }
                }
                // Remove all requested keyframes
                for (int d : std::as_const(toDelete)) {
                    keyframes.remove(d);
                }
                // Add replacement keyframes
                QMapIterator<int, int> i(toAdd);
                while (i.hasNext()) {
                    i.next();
                    keyframes.insert(i.key(), i.value());
                }
                QStringList result;
                QMapIterator<int, int> j(keyframes);
                int offset = 0;
                while (j.hasNext()) {
                    j.next();
                    if (j.key() == keyframes.lastKey()) {
                        // HACK: we always set last keyframe 1 frame after in MLT to ensure we have a correct last frame
                        offset = 1;
                    }
                    result << QStringLiteral("%1=%2")
                                  .arg(m_producer->frames_to_time(j.key() + offset, mlt_time_clock))
                                  .arg(GenTime(j.value(), pCore->getCurrentFps()).seconds());
                }
                Fun operation = [this, kfrData = result.join(QLatin1Char(';'))]() {
                    setRemapValue("time_map", kfrData.toUtf8().constData());
                    if (auto ptr = m_parent.lock()) {
                        QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                        ptr->notifyChange(ix, ix, TimelineModel::FinalMoveRole);
                    }
                    return true;
                };
                Fun reverse = [this, oldKfrData]() {
                    setRemapValue("time_map", oldKfrData.toUtf8().constData());
                    if (auto ptr = m_parent.lock()) {
                        QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                        ptr->notifyChange(ix, ix, TimelineModel::FinalMoveRole);
                    }
                    return true;
                };
                operation();
                PUSH_LAMBDA(operation, redo);
                PUSH_FRONT_LAMBDA(reverse, undo);
            }
        }
    }
}

int ClipModel::getRemapInputDuration() const
{
    Mlt::Chain fromChain(m_producer->parent());
    int count = fromChain.link_count();
    for (int i = 0; i < count; i++) {
        QScopedPointer<Mlt::Link> fromLink(fromChain.link(i));
        if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
            if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                // Found a timeremap effect, read params
                std::shared_ptr<Mlt::Link> link = std::make_shared<Mlt::Link>(fromChain.link(i)->get_link());
                if (!link->property_exists("time_map")) {
                    link->set("time_map", fromLink->get("map"));
                }
                QString mapData = link->get("time_map");
                int min = GenTime(link->anim_get_double("time_map", getIn())).frames(pCore->getCurrentFps());
                QStringList str = mapData.split(QLatin1Char(';'));
                int max = -1;
                for (auto &s : str) {
                    int val = GenTime(s.section(QLatin1Char('='), 1).toDouble()).frames(pCore->getCurrentFps());
                    if (val > max) {
                        max = val;
                    }
                }
                return max - min;
            }
        }
    }
    return 0;
}

void ClipModel::setRemapValue(const QString &name, const QString &value)
{
    if (m_producer->parent().type() != mlt_service_chain_type) {
        return;
    }
    Mlt::Chain fromChain(m_producer->parent());
    int count = fromChain.link_count();
    for (int i = 0; i < count; i++) {
        QScopedPointer<Mlt::Link> fromLink(fromChain.link(i));
        if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
            if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                // Found a timeremap effect, read params
                std::shared_ptr<Mlt::Link> link = std::make_shared<Mlt::Link>(fromChain.link(i)->get_link());
                link->set(name.toUtf8().constData(), value.toUtf8().constData());
                return;
            }
        }
    }
}

QMap<QString, QString> ClipModel::getRemapValues() const
{
    QMap<QString, QString> result;
    if (m_producer->parent().type() != mlt_service_chain_type) {
        return result;
    }
    Mlt::Chain fromChain(m_producer->parent());
    int count = fromChain.link_count();
    for (int i = 0; i < count; i++) {
        QScopedPointer<Mlt::Link> fromLink(fromChain.link(i));
        if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
            if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                // Found a timeremap effect, read params
                std::shared_ptr<Mlt::Link> link = std::make_shared<Mlt::Link>(fromChain.link(i)->get_link());
                // Ensure animation uses time not frames
                if (!link->property_exists("time_map")) {
                    link->set("time_map", link->get("map"));
                }
                (void)link->anim_get_double("time_map", 0);
                Mlt::Animation anim = link->get_animation("time_map");
                result.insert(QStringLiteral("time_map"), anim.serialize_cut(mlt_time_clock, 0, m_producer->get_length()));
                result.insert(QStringLiteral("pitch"), link->get("pitch"));
                result.insert(QStringLiteral("image_mode"), link->get("image_mode"));
                break;
            }
        }
    }
    return result;
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

int64_t ClipModel::getStartTimecodeOffset() const
{
    READ_LOCK();
    auto binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    int recTC = binClip->getStartTimecode();
    if (recTC >= 0) {
        return recTC + getIn();
    }
    return -1;
}

void ClipModel::setTimelineEffectsEnabled(bool enabled)
{
    QWriteLocker locker(&m_lock);
    m_effectStack->setEffectStackEnabled(enabled);
}

bool ClipModel::addEffect(const QString &effectId)
{
    QWriteLocker locker(&m_lock);
    if (EffectsRepository::get()->isAudioEffect(effectId)) {
        if (m_currentState == PlaylistState::VideoOnly) {
            return false;
        }
    } else if (m_currentState == PlaylistState::AudioOnly) {
        return false;
    }
    if (EffectsRepository::get()->isTextEffect(effectId) && m_clipType != ClipType::Text) {
        return false;
    }
    m_effectStack->appendEffect(effectId, true);
    return true;
}

std::pair<bool, bool> ClipModel::addEffectWithUndo(const QString &effectId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (EffectsRepository::get()->isAudioEffect(effectId)) {
        if (m_currentState == PlaylistState::VideoOnly) {
            return {false, false};
        }
    } else if (m_currentState == PlaylistState::AudioOnly) {
        return {false, false};
    }
    if (EffectsRepository::get()->isTextEffect(effectId) && m_clipType != ClipType::Text) {
        return {false, false};
    }
    return m_effectStack->appendEffectWithUndo(effectId, undo, redo);
}

bool ClipModel::copyEffect(const std::shared_ptr<EffectStackModel> &stackModel, int rowId)
{
    QWriteLocker locker(&m_lock);
    QDomDocument doc;
    m_effectStack->copyXmlEffect(stackModel->rowToXml(rowId, doc));
    return true;
}

bool ClipModel::copyEffectWithUndo(const std::shared_ptr<EffectStackModel> &stackModel, int rowId, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    QDomDocument doc;
    m_effectStack->copyXmlEffectWithUndo(stackModel->rowToXml(rowId, doc), undo, redo);
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
    Fun operation = [this, duration, effectName, originalDuration]() {
        return m_effectStack->adjustFadeLength(duration, effectName.startsWith(QLatin1String("fadein")) || effectName.startsWith(QLatin1String("fade_to_")),
                                               audioEnabled(), !isAudioOnly(), originalDuration > 0);
    };
    if (operation() && originalDuration > 0) {
        Fun reverse = [this, originalDuration, effectName]() {
            return m_effectStack->adjustFadeLength(originalDuration,
                                                   effectName.startsWith(QLatin1String("fadein")) || effectName.startsWith(QLatin1String("fade_to_")),
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

void ClipModel::refreshProducerFromBin(int trackId, PlaylistState::ClipState state, int stream, double speed, bool hasPitch, bool secondPlaylist,
                                       bool timeremap)
{
    // We require that the producer is not in the track when we refresh the producer, because otherwise the modification will not be propagated. Remove the clip
    // first, refresh, and then replant.
    QWriteLocker locker(&m_lock);
    int in = getIn();
    int out = getOut();
    if (!qFuzzyCompare(speed, m_speed) && !qFuzzyIsNull(speed)) {
        in = int(in * std::abs(m_speed / speed));
        out = in + getPlaytime() - 1;
        // prevent going out of the clip's range
        out = std::min(out, int(double(m_producer->get_length()) * std::abs(m_speed / speed)) - 1);
        m_speed = speed;
        qDebug() << "changing speed" << in << out << m_speed;
    }
    QString remapMap;
    int remapPitch = 0;
    QString remapBlend;
    if (m_hasTimeRemap) {
        if (m_producer->parent().type() == mlt_service_chain_type) {
            Mlt::Chain fromChain(m_producer->parent());
            int count = fromChain.link_count();
            for (int i = 0; i < count; i++) {
                QScopedPointer<Mlt::Link> fromLink(fromChain.link(i));
                if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
                    if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                        // Found a timeremap effect, read params
                        if (!fromLink->property_exists("time_map")) {
                            fromLink->set("time_map", fromLink->get("map"));
                        }
                        remapMap = fromLink->get("time_map");
                        remapPitch = fromLink->get_int("pitch");
                        remapBlend = fromLink->get("image_mode");
                        break;
                    }
                }
            }
        } else {
            qDebug() << "=== NON CHAIN ON REFRESH!!!";
        }
    }
    ProjectClip::TimeWarpInfo remapInfo;
    remapInfo.enableRemap = timeremap;
    if (timeremap) {
        remapInfo.timeMapData = remapMap;
        remapInfo.pitchShift = remapPitch;
        remapInfo.imageMode = remapBlend;
    }

    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    // Q_ASSERT(binClip->statusReady());
    std::shared_ptr<Mlt::Producer> binProducer = binClip->getTimelineProducer(trackId, m_id, state, stream, m_speed, secondPlaylist, remapInfo);
    m_producer = std::move(binProducer);
    m_producer->set_in_and_out(in, out);
    if (m_hasTimeRemap != hasTimeRemap()) {
        m_hasTimeRemap = !m_hasTimeRemap;
        // producer is not on a track, no data refresh needed
    }
    if (m_hasTimeRemap) {
        // Restore timeremap parameters
        if (m_producer->parent().type() == mlt_service_chain_type) {
            Mlt::Chain fromChain(m_producer->parent());
            int count = fromChain.link_count();
            for (int i = 0; i < count; i++) {
                QScopedPointer<Mlt::Link> fromLink(fromChain.link(i));
                if (fromLink && fromLink->is_valid() && fromLink->property_exists("mlt_service")) {
                    if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                        // Found a timeremap effect, read params
                        fromLink->set("time_map", remapMap.toUtf8().constData());
                        fromLink->set("pitch", remapPitch);
                        fromLink->set("image_mode", remapBlend.toUtf8().constData());
                        break;
                    }
                }
            }
        }
    }
    if (hasPitch) {
        // Check if pitch shift is enabled
        m_producer->parent().set("warp_pitch", 1);
    } else if (!qFuzzyCompare(m_speed, 1.)) {
        m_producer->parent().set("warp_pitch", 0);
    }
    // replant effect stack in updated service
    int activeEffect = m_effectStack->getActiveEffect();
    m_effectStack->resetService(m_producer);
    m_producer->set("kdenlive:id", binClip->clipId().toUtf8().constData());
    m_producer->set("_kdenlive_cid", m_id);
    if (activeEffect > 0) {
        m_producer->set("kdenlive:activeeffect", activeEffect);
    }
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
    if (m_producer->property_exists("kdenlive:audio_index")) {
        // The clip was disabled, restore original audio stream
        stream = m_producer->get_int("kdenlive:audio_index");
    }
    refreshProducerFromBin(trackId, m_currentState, stream, 0, hasPitch, m_subPlaylistIndex == 1, hasTimeRemap());
}

bool ClipModel::useTimeRemapProducer(bool enable, Fun &undo, Fun &redo)
{
    if (m_endlessResize) {
        // no timewarp for endless producers
        return false;
    }
    std::function<bool(void)> local_undo = []() { return true; };
    std::function<bool(void)> local_redo = []() { return true; };
    int audioStream = getIntProperty(QStringLiteral("audio_index"));
    QMap<QString, QString> remapProperties;
    remapProperties.insert(QStringLiteral("image_mode"), QStringLiteral("nearest"));
    if (!enable) {
        // Store the remap properties
        if (m_producer->parent().type() == mlt_service_chain_type) {
            Mlt::Chain fromChain(m_producer->parent());
            int count = fromChain.link_count();
            for (int i = 0; i < count; i++) {
                QScopedPointer<Mlt::Link> fromLink(fromChain.link(i));
                if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
                    if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                        // Found a timeremap effect, read params
                        remapProperties.insert(QStringLiteral("time_map"), fromLink->get("time_map"));
                        remapProperties.insert(QStringLiteral("pitch"), fromLink->get("pitch"));
                        remapProperties.insert(QStringLiteral("image_mode"), fromLink->get("image_mode"));
                        break;
                    }
                }
            }
        } else {
            qDebug() << "=== NON CHAIN ON REFRESH!!!";
        }
    }
    auto operation = useTimeRemapProducer_lambda(enable, audioStream, remapProperties);
    auto reverse = useTimeRemapProducer_lambda(!enable, audioStream, remapProperties);
    if (operation()) {
        UPDATE_UNDO_REDO(operation, reverse, local_undo, local_redo);
        UPDATE_UNDO_REDO(local_redo, local_undo, undo, redo);
        return true;
    }
    return false;
}

Fun ClipModel::useTimeRemapProducer_lambda(bool enable, int audioStream, const QMap<QString, QString> &remapProperties)
{
    QWriteLocker locker(&m_lock);
    return [enable, audioStream, remapProperties, this]() {
        refreshProducerFromBin(m_currentTrackId, m_currentState, audioStream, 0, false, false, enable);
        if (enable) {
            QMapIterator<QString, QString> j(remapProperties);
            if (m_producer->parent().type() == mlt_service_chain_type) {
                Mlt::Chain fromChain(m_producer->parent());
                int count = fromChain.link_count();
                for (int i = 0; i < count; i++) {
                    QScopedPointer<Mlt::Link> fromLink(fromChain.link(i));
                    if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
                        if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                            while (j.hasNext()) {
                                j.next();
                                fromLink->set(j.key().toUtf8().constData(), j.value().toUtf8().constData());
                            }
                            break;
                        }
                    }
                }
            }
        }
        return true;
    };
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
        int out = getOut();
        int in = qMax(0, qRound((m_producer->get_length() - 1 - out) * std::fabs(m_speed / speed)));
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
                    qDebug() << "==== CLIP WARP UPDATE DURATION FAILED!!!!";
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
    return pCore->projectItemModel()->getClipByBinID(m_binClipId)->audioChannels(audioStream());
}

bool ClipModel::audioMultiStream() const
{
    READ_LOCK();
    return pCore->projectItemModel()->getClipByBinID(m_binClipId)->audioStreamsCount() > 1;
}

int ClipModel::audioStream() const
{
    if (m_currentState == PlaylistState::Disabled) {
        return m_producer->get_int("kdenlive:audio_index");
    }
    return m_producer->parent().get_int("audio_index");
}

int ClipModel::audioStreamIndex() const
{
    READ_LOCK();
    return pCore->projectItemModel()->getClipByBinID(m_binClipId)->audioStreamIndex(audioStream()) + 1;
}

int ClipModel::fadeIn() const
{
    return m_effectStack->getFadePosition(true);
}

int ClipModel::fadeOut() const
{
    return m_effectStack->getFadePosition(false);
}

int ClipModel::fadeMethod(bool fadeIn) const
{
    return m_effectStack->getFadeMethod(fadeIn);
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
    service()->set("kdenlive:hide_keyframes", !show);
}

void ClipModel::setPosition(int pos)
{
    MoveableItem::setPosition(pos);
    m_clipMarkerModel->updateSnapModelPos(pos);
}

void ClipModel::setMixDuration(int mix, int cutOffset)
{
    if (mix == 0) {
        // Deleting a mix
        m_mixCutPos = 0;
    } else {
        // Creating a new mix
        m_mixCutPos = cutOffset;
    }
    m_mixDuration = mix;
    if (m_mixCutPos > 0) {
        m_clipMarkerModel->updateSnapMixPosition(m_mixDuration - m_mixCutPos);
    }
}

void ClipModel::setMixDuration(int mix)
{
    m_mixDuration = mix;
    if (m_mixDuration == 0) {
        m_mixCutPos = 0;
    }
    m_clipMarkerModel->updateSnapMixPosition(m_mixDuration - m_mixCutPos);
}

int ClipModel::getMixDuration() const
{
    return m_mixDuration;
}

int ClipModel::getMixCutPosition() const
{
    return m_mixCutPos;
}

void ClipModel::setInOut(int in, int out)
{
    MoveableItem::setInOut(in, out);
    m_clipMarkerModel->updateSnapModelInOut({in, out, qMax(0, m_mixDuration - m_mixCutPos)});
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

    if (finalMove && m_lastTrackId != m_currentTrackId) {
        if (tid != -1) {
            refreshProducerFromBin(m_currentTrackId);
        }
        m_lastTrackId = m_currentTrackId;
    }
}

Fun ClipModel::setClipState_lambda(PlaylistState::ClipState state)
{
    QWriteLocker locker(&m_lock);
    return [this, state]() {
        if (state == m_currentState) {
            return true;
        }
        if (auto ptr = m_parent.lock()) {
            m_currentState = state;
            // Enforce producer reload
            m_lastTrackId = -1;
            if (m_currentTrackId != -1 && ptr->isClip(m_id)) { // if this is false, the clip is being created. Don't update model in that case
                refreshProducerFromBin(m_currentTrackId);
                QModelIndex ix = ptr->makeClipIndexFromID(m_id);
                Q_EMIT ptr->dataChanged(ix, ix, {TimelineModel::PlaylistStateRole, TimelineModel::AudioStreamRole});
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

bool ClipModel::stackEnabled() const
{
    READ_LOCK();
    return m_effectStack->isStackEnabled();
}

const QStringList ClipModel::externalFiles() const
{
    READ_LOCK();
    return m_effectStack->externalFiles();
}

QDomElement ClipModel::toXml(QDomDocument &document)
{
    QDomElement container = document.createElement(QStringLiteral("clip"));
    container.setAttribute(QStringLiteral("binid"), m_binClipId);
    container.setAttribute(QStringLiteral("id"), m_id);
    container.setAttribute(QStringLiteral("in"), getIn());
    container.setAttribute(QStringLiteral("out"), getOut());
    container.setAttribute(QStringLiteral("position"), getPosition());
    container.setAttribute(QStringLiteral("state"), m_currentState);
    container.setAttribute(QStringLiteral("playlist"), m_subPlaylistIndex);
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
    if (m_hasTimeRemap) {
        if (m_producer->parent().type() == mlt_service_chain_type) {
            Mlt::Chain fromChain(m_producer->parent());
            int count = fromChain.link_count();
            for (int i = 0; i < count; i++) {
                QScopedPointer<Mlt::Link> fromLink(fromChain.link(i));
                if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
                    if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                        // Found a timeremap effect, read params
                        container.setAttribute(QStringLiteral("timemap"), fromLink->get("time_map"));
                        container.setAttribute(QStringLiteral("timepitch"), fromLink->get_int("pitch"));
                        container.setAttribute(QStringLiteral("timeblend"), fromLink->get("image_mode"));
                        break;
                    }
                }
            }
        } else {
            qDebug() << "=== NON CHAIN ON REFRESH!!!";
        }
    }
    container.appendChild(m_effectStack->toXml(document));
    return container;
}

bool ClipModel::checkConsistency()
{
    if (!m_effectStack->checkConsistency()) {
        qDebug() << "Consistency check failed for effectstack";
        return false;
    }
    if (m_currentTrackId == -1) {
        qDebug() << ":::: CLIP IS NOT INSERTED IN A TRACK";
        return true;
    }
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    const QUuid timelineUuid = getUuid();
    auto instances = binClip->timelineInstances(timelineUuid);
    bool found = instances.contains(m_id);
    if (!found) {
        qDebug() << "ERROR: binClip doesn't acknowledge timeline clip existence: " << m_id << ", CURRENT TRACK: " << m_currentTrackId;
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

void ClipModel::setSubPlaylistIndex(int index, int trackId)
{
    if (m_subPlaylistIndex == index) {
        return;
    }
    m_subPlaylistIndex = index;
    if (trackId > -1) {
        refreshProducerFromBin(trackId);
    }
}

void ClipModel::setOffset(int offset)
{
    m_positionOffset = offset;
    if (auto ptr = m_parent.lock()) {
        QModelIndex ix = ptr->makeClipIndexFromID(m_id);
        Q_EMIT ptr->dataChanged(ix, ix, {TimelineModel::PositionOffsetRole});
    }
}

ItemInfo ClipModel::getItemInfo() const
{
    READ_LOCK();
    ItemInfo info;
    info.itemId = m_id;
    info.position = m_position;
    info.trackId = m_currentTrackId;
    info.cropStart = m_producer->get_in();
    info.playTime = m_producer->get_playtime();
    info.maxDuration = m_endlessResize ? -1 : m_producer->get_length();
    return info;
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
        Q_EMIT ptr->dataChanged(ix, ix, {TimelineModel::GrabbedRole});
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
            Q_EMIT ptr->dataChanged(ix, ix, {TimelineModel::SelectedRole});
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
    if (m_clipType == ClipType::Timeline) {
        // Special case, timeline clips can in some cases be longer than max duration
        int maxDuration = m_producer->parent().get_int("kdenlive:maxduration");
        if (maxDuration > 0) {
            return maxDuration;
        }
    }
    return m_producer->get_length();
}

const QString ClipModel::clipName() const
{
    return pCore->projectItemModel()->getClipByBinID(m_binClipId)->clipName();
}

const QString ClipModel::clipTag() const
{
    if (KdenliveSettings::tagsintimeline()) {
        return pCore->projectItemModel()->getClipByBinID(m_binClipId)->tags();
    }
    return QString();
}

FileStatus::ClipStatus ClipModel::clipStatus() const
{
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    return binClip->clipStatus();
}

QString ClipModel::clipHash() const
{
    QDomDocument document;
    QDomElement container = document.createElement(QStringLiteral("clip"));
    container.setAttribute(QStringLiteral("service"), m_producer->parent().get("mlt_service"));
    container.setAttribute(QStringLiteral("in"), getIn());
    container.setAttribute(QStringLiteral("out"), getOut());
    container.setAttribute(QStringLiteral("position"), getPosition());
    container.setAttribute(QStringLiteral("state"), m_currentState);
    container.setAttribute(QStringLiteral("playlist"), m_subPlaylistIndex);
    container.setAttribute(QStringLiteral("speed"), QString::number(m_speed, 'f'));
    container.setAttribute(QStringLiteral("audioStream"), getIntProperty(QStringLiteral("audio_index")));
    std::vector<int> snaps;
    allSnaps(snaps);
    QString snapData;
    for (auto &s : snaps) {
        snapData.append(QString::number(s));
    }
    container.setAttribute(QStringLiteral("markers"), snapData);
    document.appendChild(container);
    container.appendChild(m_effectStack->toXml(document));
    return document.toString();
}

const QString ClipModel::clipThumbPath()
{
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    if (binClip) {
        return binClip->baseThumbPath();
    }
    return QString();
}

void ClipModel::switchBinReference(const QString newId, const QUuid &uuid)
{
    deregisterClipToBin(uuid);
    m_binClipId = newId;
    refreshProducerFromBin(-1);
    registerClipToBin(getProducer(), false);
    if (auto ptr = m_parent.lock()) {
        ptr->replugClip(m_id);
        QVector<int> roles{TimelineModel::ClipThumbRole};
        QModelIndex ix = ptr->makeClipIndexFromID(m_id);
        ptr->notifyChange(ix, ix, roles);
        // invalidate timeline preview
        if (!ptr->getTrackById_const(m_currentTrackId)->isAudioTrack()) {
            Q_EMIT ptr->invalidateZone(m_position, m_position + getPlaytime());
        } else {
            Q_EMIT ptr->invalidateAudioZone(m_position, m_position + getPlaytime());
        }
    }
}

int ClipModel::assetRow(const QString &assetId, int eid, bool enabledOnly) const
{
    return m_effectStack->effectRow(assetId, eid, enabledOnly);
}

std::shared_ptr<KeyframeModelList> ClipModel::getKFModel(int row)
{
    auto item = m_effectStack->getEffectStackRow(row);
    if (!item || item->childCount() > 0) {
        // group, error
        return nullptr;
    }
    std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
    return eff->getKeyframeModel();
}

const QString ClipModel::activeEffectId() const
{
    int activeEffect = m_effectStack->getActiveEffect();
    if (activeEffect > -1) {
        std::shared_ptr<AbstractEffectItem> item = m_effectStack->getEffectStackRow(activeEffect);
        if (item) {
            std::shared_ptr<EffectItemModel> eff = std::static_pointer_cast<EffectItemModel>(item);
            return eff->getAssetId();
        }
    }
    return QString();
}

void ClipModel::setActiveEffect(int row)
{
    m_effectStack->setActiveEffect(row);
}

bool ClipModel::hasDisabledBuiltInTransform()
{
    return m_effectStack->hasDisabledBuiltInTransform();
}

void ClipModel::refreshAVInfo()
{
    std::shared_ptr<ProjectClip> binClip = pCore->projectItemModel()->getClipByBinID(m_binClipId);
    if (binClip) {
        m_canBeVideo = binClip->hasVideo();
        m_canBeAudio = binClip->hasAudio();
        refreshProducerFromBin(m_currentTrackId);
    }
}
