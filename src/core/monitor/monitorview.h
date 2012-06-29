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

class MonitorGraphicsScene;
class MonitorModel;
class PositionBar;
class TimecodeWidget;
class KDualAction;
class QGraphicsView;


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
    explicit MonitorView(QWidget* parent = 0);
    virtual ~MonitorView();

    /**
     * @brief Assigns a new model to the view and connects the controls to it.
     * @param model new monitor model
     */
    void setModel(MonitorModel *model);
    /** @brief Returns the current model */
    MonitorModel *model();

private slots:
    /** @brief Toggles between play and pause. */
    void togglePlaybackState();
    /** @brief Updates the shown position of the controls (position bar, timecode widget). */
    void setPosition(int position);

    void onPlaybackStateChange(bool plays);
    void onProducerChanged();

private:
    QGraphicsView *m_videoView;
    MonitorGraphicsScene *m_videoScene;
    MonitorModel *m_model;
    PositionBar *m_positionBar;
    KDualAction *m_playAction;
    TimecodeWidget *m_timecodeWiget;
};

#endif
