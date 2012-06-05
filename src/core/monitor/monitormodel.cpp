/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "monitormodel.h"
#include "project/project.h"
#include "project/producerwrapper.h"
#include "kdenlivesettings.h"
#include <mlt++/Mlt.h>
#include <QImage>

MonitorModel::MonitorModel(Project* project) :
    QObject(project),
    m_project(project),
    m_producer(NULL),
    m_frame(NULL)
{
    // do this in Project
//     setenv("MLT_PROFILE", KdenliveSettings::current_profile().toUtf8().constData(), 1);

    // use Mlt::FilteredConsumer for splitview?
    m_consumer = new Mlt::Consumer(*m_project->profile(), "sdl_audio");

    Q_ASSERT(m_consumer);

    m_frameShowEvent = m_consumer->listen("consumer-frame-show", this, (mlt_listener)consumer_frame_show);

    QString audioDevice = KdenliveSettings::audiodevicename();
    if (!audioDevice.isEmpty())
        m_consumer->set("audio_device", audioDevice.toUtf8().constData());

    QString audioDriver = KdenliveSettings::audiodrivername();
    if (!audioDriver.isEmpty())
        m_consumer->set("audio_driver", audioDriver.toUtf8().constData());

    m_consumer->set("progressive", 1);
    m_consumer->set("real_time", KdenliveSettings::mltthreads());
}

MonitorModel::~MonitorModel()
{
    delete m_frameShowEvent;
    delete m_consumer;
    Mlt::Frame *frame = m_frame.fetchAndStoreAcquire(0);
    if (frame) {
        delete frame;
    }
}

void MonitorModel::setProducer(ProducerWrapper* producer)
{
    pause();
    m_consumer->stop();

    m_producer = producer;
    m_producer->set_speed(0.);

    // should we mlt_service_disconnect ?
    m_consumer->connect(*static_cast<Mlt::Service *>(producer));
    m_consumer->start();
    refreshConsumer();

    emit producerChanged();
}

void MonitorModel::play()
{
    if (m_producer) {
        setSpeed(1);
        refreshConsumer();
        kDebug() << "play";
        emit playbackStateChanged(true);
    }
}

void MonitorModel::pause()
{
    if (m_producer) {
        m_producer->pause();
//         m_consumer->stop();
        kDebug() << "pause";
        emit playbackStateChanged(false);
    }
}

void MonitorModel::togglePlaybackState()
{
    if (speed() == 0) {
        play();
    } else {
        pause();
    }
}

void MonitorModel::setPosition(int position)
{
    if (m_producer) {
        m_producer->seek(position);
        if (speed() == 0) {
            refreshConsumer();
        }
        emit positionChanged(position);
    }
}

int MonitorModel::position() const
{
    return m_consumer->position();
}

void MonitorModel::setSpeed(double speed)
{
    if (m_producer) {
        m_producer->set_speed(speed);
        emit speedChanged(speed);
    }
}

double MonitorModel::speed() const
{
    if (m_producer) {
        return m_producer->get_speed();
    } else {
        return 0;
    }
}

int MonitorModel::duration() const
{
    if (m_producer) {
        return m_producer->get_playtime();
    } else {
        return 0;
    }
}

AtomicFramePointer* MonitorModel::framePointer()
{
    return &m_frame;
}

void MonitorModel::updateFrame(mlt_frame frame_ptr)
{
    Mlt::Frame *frame = m_frame.fetchAndStoreAcquire(new Mlt::Frame(frame_ptr));
    if (frame) {
        delete frame;
    }
    emit frameUpdated();
    if (m_position != m_consumer->position()) {
        m_position = m_consumer->position();
        emit positionChanged(m_position);
    }
}

void MonitorModel::consumer_frame_show(mlt_consumer , MonitorModel* self, mlt_frame frame_ptr)
{
    self->updateFrame(frame_ptr);
}

void MonitorModel::refreshConsumer()
{
    m_consumer->set("refresh", 1);
}


#include "monitormodel.moc"

