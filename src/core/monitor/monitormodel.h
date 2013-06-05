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
namespace Mlt
{
    class Consumer;
    class Frame;
    class Event;
    class Profile;
}
class QImage;

typedef QAtomicPointer<Mlt::Frame> AtomicFramePointer;


/**
 * @class MonitorModel
 * @brief Wrapper around Mlt::Consumer
 */

class KDE_EXPORT MonitorModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates a new consumer according to the parameters and some global settings.
     * @param profile profile to use
     * @param name Displayable (translated) name used as dock widget title by MonitorManager
     */
    explicit MonitorModel(Mlt::Profile *profile, const QString &name, QObject *parent = 0);
    virtual ~MonitorModel();

    /** @brief Returns the displayable translated name. */
    QString name() const;

    /**
     * @brief Connects the provided producer to the handled consumer.
     * @param producer new producer to use
     * 
     * emits activated
     * emits producerChanged
     */
    void setProducer(ProducerWrapper *producer);

    /** @brief Returns the current position. */
    int position() const;
    /** @brief Returns the current playback speed. */
    double speed() const;
    /** @brief Returns the producer's duration (playtime). */
    int duration() const;

    /**
     * @brief Returns the frame pointer.
     * 
     * The frame pointer either points to the current frame or is 0 if it was already used.
     */
    AtomicFramePointer *framePointer();

    /**
     * @brief Updates the frame pointer.
     * 
     * emits frameUpdated
     * emits positionChanged if necessary
     * 
     * Should only be used internally by consumer_frame_show!
     */
    void updateFrame(mlt_frame frame);

    /** @brief Emits durationChanged and makes sure the current position is still valid. */
    void emitDurationChanged();

    /** @brief Callback for the consumer-frame-show consumer event. Calls updateFrame. */
    static void consumer_frame_show(mlt_consumer, MonitorModel *self, mlt_frame frame_ptr);

    /** @brief Callback for the producer-changed producer event. Calls emitDurationChanged. */
    static void producer_change(mlt_producer producer, MonitorModel *self);

public slots:
    /** @brief Starts playback. */
    void play();
    /** @brief Pauses playback. */
    void pause();
    /** @brief Toggles between play and pause. */
    void togglePlaybackState();
    /** 
     * @brief Sets the position.
     * @param position new position
     * 
     * A range change for position is performed.
     * 
     * emits positionChanged and triggers a frame update if necessary
     */
    void setPosition(int position);
    /** 
     * @brief Sets the speed.
     * @param speed new speed
     * 
     * Does not trigger a monitor refresh!
     * emits speedChanged
     */
    void setSpeed(double speed);
    /**
     * @brief Emits activated.
     */
    void activate();
    /**
     * @brief Tells the consumer to refresh/get a new frame.
     */
    void refresh();

signals:
    void frameUpdated();
    void playbackStateChanged(bool plays);
    void positionChanged(int position);
    void speedChanged(double speed);
    void producerChanged();
    void durationChanged(int duration);
    void activated();

private:
    QString m_name;
    Mlt::Consumer *m_consumer;
    Mlt::Event *m_frameShowEvent;
    Mlt::Event *m_producerChangeEvent;
    ProducerWrapper *m_producer;
    AtomicFramePointer m_frame;
    int m_position;
};

#endif
