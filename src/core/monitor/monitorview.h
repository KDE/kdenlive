/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef MONITORVIEW_H
#define MONITORVIEW_H

#include <KIcon>
#include <QWidget>
#include <kdemacros.h>
#include "monitormanager.h"
#include "mltcontroller.h"

class MonitorGraphicsScene;
class PositionBar;
class TimecodeWidget;
class AudioSignal;
class KDualAction;
class KUrl;
class KAction;
class KSelectAction;
class KMessageWidget;
class QGraphicsView;
class QGridLayout;
class ProducerWrapper;


/**
 * @class MonitorView
 * @brief Widget contains the actual monitor and the related controls.
 */


class KDE_EXPORT MonitorView : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Sets up the GUI elements.
     */
    explicit MonitorView(Mlt::Profile *profile, MONITORID id, QWidget* parent = 0);
    virtual ~MonitorView();
    /** @brief Returns the MLT controller of this view. */
    MltController *controller();
    /** @brief Returns the monitor ID of this view (auto, clip, timeline,...). */
    MONITORID id() const;
    bool isActive() const;
    void activate();
    void close();
    int open(ProducerWrapper*, bool isMulti = false);
    void refresh();
    int position() const;
    void setProfile(Mlt::Profile *profile);
    DISPLAYMODE displayType() const;

private slots:
    /** @brief Toggles between play and pause. */
    void togglePlaybackState();
    void checkPlaybackState();
    /** @brief Toggles between direct QGLwidget rendering and embedded QGraphicsscene rendering. */
    //void toggleSceneState();
    /** @brief Updates the shown position of the controls (position bar, timecode widget). */
    void setPosition(int position);
    /** @brief Producer changed, update duration of position bar and timecode widget. */
    void onProducerChanged();
    /** @brief A frame was just displayed, adjust playhead in position bar and timecode widget. */
    void onShowFrame(Mlt::QFrame frame);
    /** @brief Play state might have changed, check that play icon reflects current state. */
    void slotCheckPlayState();
    /** @brief Show / hide audio monitor. */
    void toggleAudioMonitor();
    void toggleMonitorMode(int mode, bool checkAvailability = true);
    void slotGetThumb(int pos);
    
public slots:
    /** @brief Seek to wanted pos. */
    void seek(int pos);

private:
    /** @brief Id of this monitor (Clip, Project, Auto,...). */
    MONITORID m_id;
    Mlt::Profile *m_profile;
    QGridLayout *m_layout;
    /** @brief The active controller for this view. */
    MltController *m_controller;
    /** @brief The scene for the QGraphicsView controller. */
    MonitorGraphicsScene *m_videoScene;
    /** @brief The position (timeline) bar. */
    PositionBar *m_positionBar;
    /** @brief The play / pause action. */
    KDualAction *m_playAction;
    /** @brief The action allowing to toggle between OpenGL and QGraphicsView controllers. */
    KSelectAction *m_monitorMode;
    KAction *m_sceneAction;
    /** @brief The action allowing to show / hide audio monitor. */
    KAction *m_audioAction;
    /** @brief The editable timecode widget. */
    TimecodeWidget *m_timecodeWiget;
    /** @brief The audio meter widget. */
    AudioSignal *m_audioSignal;
    /** @brief The last requested seek position (or -1 if not seeking). */
    int m_seekPosition;
    ProducerWrapper* m_monitorProducer;
    ProducerWrapper* m_blackProducer;
    KMessageWidget *m_infoMessage;
    /** @brief Connect the signals to current controller. */
    void connectController(bool doConnect = true);
    /** @brief Trigger display of audio volume. */
    void showAudio(Mlt::Frame* frame);
    
signals:
    /** @brief Controller for this view has changed, inform other widgets. */
    void controllerChanged(MONITORID, MltController *);
    void audioLevels(QVector<double>);
    void requestThumb(ProducerWrapper *, int);
};

#endif
