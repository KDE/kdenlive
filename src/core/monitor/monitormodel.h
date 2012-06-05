/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef MONITORMODEL_H
#define MONITORMODEL_H

#include <mlt/framework/mlt_consumer.h>
#include <QObject>
#include <kdemacros.h>

class ProducerWrapper;
class Project;
namespace Mlt
{
    class Consumer;
    class Frame;
    class Event;
}
class QImage;

typedef QAtomicPointer<Mlt::Frame> AtomicFramePointer;


class KDE_EXPORT MonitorModel : public QObject
{
    Q_OBJECT

public:
    explicit MonitorModel(Project* project);
    virtual ~MonitorModel();

    void setProducer(ProducerWrapper *producer);

    int position() const;
    double speed() const;
    int duration() const;
    AtomicFramePointer *framePointer();

    void updateFrame(mlt_frame frame);

    static void consumer_frame_show(mlt_consumer, MonitorModel *self, mlt_frame frame_ptr);

public slots:
    void play();
    void pause();
    void togglePlaybackState();
    void setPosition(int position);
    void setSpeed(double speed);

signals:
    void frameUpdated();
    void playbackStateChanged(bool plays);
    void positionChanged(int position);
    void speedChanged(double speed);
    void producerChanged();

private:
    inline void refreshConsumer();

    Project *m_project;
    Mlt::Consumer *m_consumer;
    Mlt::Event *m_frameShowEvent;
    ProducerWrapper *m_producer;
    AtomicFramePointer m_frame;
    int m_position;
};

#endif
