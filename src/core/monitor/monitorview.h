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
#include "kdenlivecore_export.h"
#include "monitormanager.h"
#include "mltcontroller.h"

class MonitorGraphicsScene;
class PositionBar;
class TimecodeWidget;
class AudioSignal;
class KDualAction;
class KAction;
class KSelectAction;
class KMessageWidget;
class QGraphicsView;
class QToolBar;
class QGridLayout;
class ProducerWrapper;


/**
 * @class MonitorView
 * @brief Widget contains the actual monitor and the related controls.
 */


class KDENLIVECORE_EXPORT MonitorView : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Sets up the GUI elements.
     */
    explicit MonitorView(DISPLAYMODE mode, Mlt::Profile *profile, MONITORID id, MONITORID role, QWidget* parent = 0);
    virtual ~MonitorView();
    /** @brief Returns the MLT controller of this view. */
    MltController *controller();
    /** @brief Returns the monitor ID of this view (auto, clip, timeline,...). */
    MONITORID id() const;
    bool isActive() const;
    void activate();
    void close();
    int open(ProducerWrapper*, MONITORID role = KeepMonitor, const QPoint &zone = QPoint(), bool isMulti = false);
    int position() const;
    void setProfile(Mlt::Profile *profile, bool reset = true);
    DISPLAYMODE displayType() const;
    static const QString nameForRole(MONITORID role);
    void relativeSeek(int pos, MONITORID role = KeepMonitor);

private slots:
    /** @brief Check playback state and update play icon status. */
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
    void toggleAudioMonitor(bool active);
    /** @brief Show / hide monitor zone. */
    void toggleZone(bool active);
    /** @brief Request a thumbnail for current active producer.
     *  @param pos the frame position for requested thumbnail
     */
    void slotGetThumb(int pos);
    /** @brief Change monitor's display mode (OpenGL, SDL,...) based on action's data
     *  @param a the action containing the requested DISPLAYMODE as data
     */
    void slotToggleMonitorMode(QAction *a);
    /** @brief Change monitor's role (Clip monitor, Project monitor,...) based on action's data
     *  @param a the action containing the requested MONITORID as data
     */
    void slotToggleMonitorRole(QAction *a);
    void slotZoneChanged(const QPoint &zone);
    
public slots:
    /** @brief Toggles between play and pause. */
    void togglePlaybackState();
    /** @brief Seek to wanted pos. */
    void seek(int pos, MONITORID role = KeepMonitor);
    /** @brief Activate clip monitor if it is contained in current roles. */
    void slotFocusClipMonitor();
    /** @brief Request a monitor refresh. */
    void refresh();

private:
    /** @brief Id of this monitor (Clip, Project, Auto,...). */
    MONITORID m_id;
    /** @brief Role of this monitor (Clip, Project, Auto,...). */
    MONITORID m_currentRole;
    /** @brief The active profile for this project. */
    Mlt::Profile *m_profile;
    /** @brief Monitor view's layout. */
    QGridLayout *m_layout;
    /** @brief The active controller for this view. */
    MltController *m_controller;
    /** @brief The scene for the QGraphicsView controller. */
    MonitorGraphicsScene *m_videoScene;
    /** @brief The position (timeline) bar. */
    PositionBar *m_positionBar;
    /** @brief The toolbar containing buttons. */
    QToolBar *m_toolbar;
    /** @brief The play / pause action. */
    KDualAction *m_playAction;
    /** @brief The action allowing to toggle between OpenGL and QGraphicsView controllers. */
    KSelectAction *m_monitorMode;
    /** @brief The action allowing to toggle between roles (Clip, Project, Record monitor, ... */
    KSelectAction *m_monitorRole;
    /** @brief The action allowing to enable / disable zone on this clip */
    KAction *m_zoneAction;
    KAction *m_sceneAction;
    /** @brief The editable timecode widget. */
    TimecodeWidget *m_timecodeWiget;
    /** @brief The audio meter widget. */
    AudioSignal *m_audioSignal;
    /** @brief The last requested seek position (or -1 if not seeking). */
    int m_seekPosition;
    ProducerWrapper *m_monitorProducer;
    /** @brief An optional info message for warnings. */
    KMessageWidget *m_infoMessage;
    /** @brief The list of producers for each roles of this monitor. */
    QHash<MONITORID, ProducerWrapper*> m_normalProducer;
    /** @brief The list of GPU accelerated producers for each roles of this monitor. */
    QHash<MONITORID, ProducerWrapper*> m_GPUProducer;
    /** @brief A basic default black color producer. */
    ProducerWrapper* m_blackProducer;
    /** @brief Connect the signals to current controller. */
    void connectController(bool doConnect = true);
    /** @brief Trigger display of audio volume. */
    void showAudio(Mlt::Frame* frame);
    /** @brief Create the controller using the requested mode. 
     *  @param mode THe requested DISPLAYMODE
     */
    MltController *buildController(DISPLAYMODE mode);
    /** @brief Change monitor's display mode (OpenGL, SDL,...) based on action's data
     *  @param mode the requested DISPLAYMODE
     *  @param checkAvailability Should we check if this DISPLAYMODE is available
     */
    void toggleMonitorMode(DISPLAYMODE mode, bool checkAvailability = true);
    /** @brief Change the active profile
     *  @param base the currently active profile
     *  @param newProfile the newly requested profile
     */
    bool requiresProfileUpdate(Mlt::Profile *base, Mlt::Profile *newProfile) const;
    /** @brief Add a role to current monitor
     * @param role The new role we want to add
     */
    void addMonitorRole(MONITORID role);
    /** @brief Change monitor's role (Clip monitor, Project monitor,...) based on action's data
     *  @param role the new MONITORID role
     *  @param setAction True if we want to update the combobox to this new role
     */
    void toggleMonitorRole(MONITORID role, bool setAction = true);
    
signals:
    /** @brief Controller for this view has changed, inform other widgets. */
    void controllerChanged(MONITORID, MltController *);
    void audioLevels(const QVector<double>&);
    void requestThumb(ProducerWrapper *, int);
    void positionChanged(int, bool);
    void zoneChanged(MONITORID,QPoint);
};

#endif
