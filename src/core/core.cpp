/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "core.h"
#include "mainwindow.h"
#include "project/clippluginmanager.h"
#include "effectsystem/effectrepository.h"
#include "bin/bin.h"
#include "timelineview/timelinewidget.h"
#include "monitor/monitormanager.h"
#include "project/projectmanager.h"
#include <QCoreApplication>


Core *Core::m_self = NULL;


Core::Core(MainWindow *mainWindow) :
    m_mainWindow(mainWindow)
{
    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(deleteLater()));
}

Core::~Core()
{
    delete m_effectRepository;

    m_self = 0;
}

void Core::initialize(MainWindow* mainWindow)
{
    m_self = new Core(mainWindow);
    m_self->init();
}

void Core::init()
{
    m_effectRepository = new EffectRepository();
    m_clipPluginManager = new ClipPluginManager(this);
    m_projectManager = new ProjectManager(this);
    m_monitorManager = new MonitorManager(this);
}

Core* Core::self()
{
    return m_self;
}

MainWindow* Core::window()
{
    return m_mainWindow;
}

ProjectManager* Core::projectManager()
{
    return m_projectManager;
}

EffectRepository* Core::effectRepository()
{
    return m_effectRepository;
}

ClipPluginManager* Core::clipPluginManager()
{
    return m_clipPluginManager;
}

MonitorManager* Core::monitorManager()
{
    return m_monitorManager;
}
