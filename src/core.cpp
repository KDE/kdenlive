/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "core.h"
#include "mainwindow.h"
#include "project/projectmanager.h"
#include "monitor/monitormanager.h"
#include <QCoreApplication>


Core *Core::m_self = NULL;


Core::Core(MainWindow *mainWindow) :
    m_mainWindow(mainWindow)
{
    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(deleteLater()));
}

Core::~Core()
{
    m_monitorManager->stopActiveMonitor();
    m_self = 0;
}

void Core::initialize(MainWindow* mainWindow)
{
    m_self = new Core(mainWindow);
    m_self->init();
}

void Core::init()
{
    m_projectManager = new ProjectManager(this);
    m_monitorManager = new MonitorManager(this);
//     m_pluginManager = new PluginManager(this);
}

void Core::loadPlugins()
{
//     m_pluginManager->load();
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

MonitorManager* Core::monitorManager()
{
    return m_monitorManager;
}

/*PluginManager* Core::pluginManager()
{
    return m_pluginManager;
}*/


#include "core.moc"
