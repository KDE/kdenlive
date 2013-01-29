/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TIMELINE_H
#define TIMELINE_H

#include <mlt/framework/mlt_producer.h>
#include <QObject>
#include <kdemacros.h>

class Project;
class ProducerWrapper;
class TimelineTrack;
class MonitorModel;
namespace Mlt
{
    class Tractor;
    class Profile;
    class Event;
}


/**
 * @class Timeline
 * @brief Entry point for anything timeline related. Manages a Mlt::Tractor.
 */


class KDE_EXPORT Timeline : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Creates a new timeline using the "xml-string" producer.
     * @param document mlt xml containing the tractor
     * @param parent project this timeline belongs to
     */
    Timeline(const QString &document, Project* parent);
    virtual ~Timeline();
    
    /** @brief Create empty xml doc. */
    QString getBlankDocument() const;

    QString toXml() const;

    /** @brief Returns the duration of the timeline. */
    int duration() const;

    /** @brief Returns a pointer to the project this timeline belongs to. */
    Project *project();
    /**
     * @brief Returns a list of pointers to the tracks this timeline consists of.
     *
     * Please note that the track order in Kdenlive is reversed.
     */
    QList<TimelineTrack *> tracks();
    /** @brief Returns a pointer to the profile used. */
    Mlt::Profile *profile();
    /** @brief Returns a pointer to the main producer used. */
    ProducerWrapper *producer();
    /** @brief Returns a pointer to the monitor model used for this timeline. */
    MonitorModel *monitor();

    /** @brief Returns a pointer to the track with the given index. */
    TimelineTrack *track(int index);

    /**
     * @brief Creates Track objects based on the MLT track producers.
     * 
     * This separate step (from the constructor) is necessary since the project clips need to be
     * loaded before their timeline instances however for loading the project clips we need to know
     * the project's profile which is determined when loading the tractor. Do not use this function
     * anywhere else!
     */
    void loadTracks();

    /** @brief Emits durationChanged. */
    void emitDurationChanged();

    /** @brief Callback for the "producer-changed" event of the main producer to find out about timeline duration changes. */
    static void producer_change(mlt_producer producer, Timeline *self);

signals:
    void durationChanged(int duration);

private:
    Project *m_parent;
    Mlt::Tractor *m_tractor;
    ProducerWrapper *m_producer;
    Mlt::Event *m_producerChangeEvent;
    Mlt::Profile *m_profile;
    QList <TimelineTrack *> m_tracks;
    MonitorModel *m_monitor;
    
};

#endif
