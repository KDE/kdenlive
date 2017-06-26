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
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "core.h"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "macros.hpp"
#include "timelinemodel.hpp"
#include "trackmodel.hpp"
#include <QDebug>
#include <mlt++/MltProducer.h>
#include <utility>

// this can be deleted
#include "bin/model/markerlistmodel.hpp"
#include "gentime.h"
#include <effects/effectsrepository.hpp>

ClipModel::ClipModel(std::weak_ptr<TimelineModel> parent, std::shared_ptr<Mlt::Producer> prod, const QString &binClipId, int id)
    : MoveableItem<Mlt::Producer>(std::move(parent), id)
    , m_producer(std::move(prod))
    , m_effectStack(EffectStackModel::construct(m_producer))
    , m_binClipId(binClipId)
{
    m_producer->set("kdenlive:id", binClipId.toUtf8().constData());
    m_producer->set("_kdenlive_cid", id);
    std::shared_ptr<ProjectClip> binClip = pCore->bin()->getBinClip(m_binClipId);
    if (binClip) {
        m_endlessResize = !binClip->hasLimitedDuration();
    } else {
        m_endlessResize = false;
    }
}

int ClipModel::construct(const std::weak_ptr<TimelineModel> &parent, const QString &binClipId, int id)
{
    std::shared_ptr<ProjectClip> binClip = pCore->bin()->getBinClip(binClipId);
    std::shared_ptr<Mlt::Producer> originalProducer = binClip->originalProducer();
    std::shared_ptr<Mlt::Producer> cutProducer(originalProducer->cut());
    return construct(parent, binClipId, cutProducer, id);
}

int ClipModel::construct(const std::weak_ptr<TimelineModel> &parent, const QString &binClipId, std::shared_ptr<Mlt::Producer> producer, int id)
{
    std::shared_ptr<ClipModel> clip(new ClipModel(parent, producer, binClipId, id));
    id = clip->m_id;
    if (auto ptr = parent.lock()) {
        ptr->registerClip(clip);
    } else {
        qDebug() << "Error : construction of clip failed because parent timeline is not available anymore";
        Q_ASSERT(false);
    }

    return id;
}

void ClipModel::registerClipToBin()
{
    auto binClip = pCore->bin()->getBinClip(m_binClipId);
    if (!binClip) {
        qDebug() << "Error : Bin clip for id: " << m_binClipId << " NOT AVAILABLE!!!";
    }
    binClip->registerTimelineClip(m_parent, m_id);
}

void ClipModel::deregisterClipToBin()
{
    std::shared_ptr<ProjectClip> binClip = pCore->bin()->getBinClip(m_binClipId);
    binClip->deregisterTimelineClip(m_id);
}

ClipModel::~ClipModel()
{
}

bool ClipModel::requestResize(int size, bool right, Fun &undo, Fun &redo)
{
    QWriteLocker locker(&m_lock);
    if (!m_endlessResize && (size <= 0 || size > m_producer->get_length())) {
        return false;
    }
    int delta = getPlaytime() - size;
    int in = m_producer->get_in();
    int out = m_producer->get_out();
    int old_in = in, old_out = out;
    // check if there is enough space on the chosen side
    if (!right && in + delta < 0 && !m_endlessResize) {
        return false;
    }
    if (!m_endlessResize && right && out - delta >= m_producer->get_length()) {
        return false;
    }
    if (right) {
        out -= delta;
    } else {
        in += delta;
    }
    std::function<bool(void)> track_operation = []() { return true; };
    std::function<bool(void)> track_reverse = []() { return true; };
    if (m_currentTrackId != -1) {
        if (auto ptr = m_parent.lock()) {
            track_operation = ptr->getTrackById(m_currentTrackId)->requestClipResize_lambda(m_id, in, out, right);
        } else {
            qDebug() << "Error : Moving clip failed because parent timeline is not available anymore";
            Q_ASSERT(false);
        }
    }
    Fun operation = [this, in, out, track_operation]() {
        if (track_operation()) {
            int outPoint = in < 0 ? out - in : out;
            if (outPoint >= m_producer->get_length()) {
                m_producer->parent().set("length", outPoint + 1);
                m_producer->set("length", outPoint + 1);
            }
            m_producer->set_in_and_out(in < 0 ? 0 : in, outPoint);
            return true;
        }
        return false;
    };
    if (operation()) {
        // Now, we are in the state in which the timeline should be when we try to revert current action. So we can build the reverse action from here
        auto ptr = m_parent.lock();
        if (m_currentTrackId != -1 && ptr) {
            track_reverse = ptr->getTrackById(m_currentTrackId)->requestClipResize_lambda(m_id, old_in, old_out, right);
        }
        Fun reverse = [this, old_in, old_out, track_reverse]() {
            if (track_reverse()) {
                m_producer->set_in_and_out(old_in, old_out);
                return true;
            }
            return false;
        };
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

Mlt::Producer *ClipModel::service() const
{
    READ_LOCK();
    return m_producer.get();
}

int ClipModel::getPlaytime() const
{
    READ_LOCK();
    return m_producer->get_playtime();
}

void ClipModel::setTimelineEffectsEnabled(bool enabled)
{
    READ_LOCK();
    m_effectStack->setEffectStackEnabled(enabled);
}

bool ClipModel::addEffect(const QString &effectId)
{
    READ_LOCK();
    m_effectStack->appendEffect(effectId, m_id);
    return true;
}

bool ClipModel::copyEffect(std::shared_ptr<EffectStackModel> stackModel, int rowId)
{
    READ_LOCK();
    m_effectStack->copyEffect(stackModel->getEffectStackRow(rowId), m_id);
    return true;
}

bool ClipModel::hasAudio() const
{
    READ_LOCK();
    QString service = getProperty("mlt_service");
    return service.contains(QStringLiteral("avformat"));
}

bool ClipModel::isAudioOnly() const
{
    READ_LOCK();
    QString service = getProperty("mlt_service");
    return service.contains(QStringLiteral("avformat")) && (getIntProperty(QStringLiteral("video_index")) == -1);
}

void ClipModel::refreshProducerFromBin()
{
    QWriteLocker locker(&m_lock);
    int in = getIn();
    int out = getOut();
    std::shared_ptr<ProjectClip> binClip = pCore->bin()->getBinClip(m_binClipId);
    std::shared_ptr<Mlt::Producer> originalProducer = binClip->originalProducer();
    m_producer.reset(originalProducer->cut(in, out));
    // replant effect stack in updated service
    m_effectStack->resetService(m_producer);
    m_producer->set("kdenlive:id", binClip->AbstractProjectItem::clipId().toUtf8().constData());
    m_producer->set("_kdenlive_cid", m_id);
    m_endlessResize = !binClip->hasLimitedDuration();
}

QVariant ClipModel::getAudioWaveform()
{
    READ_LOCK();
    std::shared_ptr<ProjectClip> binClip = pCore->bin()->getBinClip(m_binClipId);
    if (binClip) {
        return QVariant::fromValue(binClip->audioFrameCache);
    }
    return QVariant();
}

const QString &ClipModel::binId() const
{
    return m_binClipId;
}

std::shared_ptr<MarkerListModel> ClipModel::getMarkerModel() const
{
    READ_LOCK();
    return pCore->bin()->getBinClip(m_binClipId)->getMarkerModel();
}
