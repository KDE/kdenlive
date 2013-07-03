/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef EFFECTSWIDGET_H
#define EFFECTSWIDGET_H

#include <QWidget>

#include <mlt++/Mlt.h>

class KAction;
class QListWidget;
class QFrame;
class KToolBar;
class Timeline;
class EffectRepository;
class QToolButton;
class EffectDevice;

class ToolPanel : public QWidget
{
    Q_OBJECT
    
public:
    explicit ToolPanel(QWidget *parent = 0);
    virtual void fillToolBar() = 0;
    
};

/**
 * @class EffectsWidget
 * @brief The timeline widgets contains the sub widgets (track headers, position bar, actual timeline view) and establishes the connections between them.
 */


class EffectsWidget : public ToolPanel
{
    Q_OBJECT

public:
    /** @brief Creates the sub widgets and a tool manager. */
    explicit EffectsWidget(KToolBar *toolbar, EffectRepository *repo, QWidget* parent = 0);
    virtual ~EffectsWidget();
    void setTimeline(Timeline *time);
    void fillToolBar();

private:
    QToolButton* m_toolButton;
    KToolBar *m_toolbar;
    EffectRepository *m_repository;
    QFrame *m_frame;
    Mlt::Service m_service;
    EffectDevice *m_device;
    QAction *m_effectAction;
    
private slots:
    void slotAddEffect(QAction *a);
    
signals:
    void refreshMonitor();

};

#endif
