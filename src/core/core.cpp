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
#include "monitor/monitorview.h"
#include "project/projectmanager.h"
#include <QCoreApplication>


Core *Core::m_self = NULL;


Core::Core(MainWindow *mainWindow) :
    m_mainWindow(mainWindow),
    m_currentProject(NULL)
{
    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(deleteLater()));
}

Core::~Core()
{
    delete m_projectManager;
    delete m_effectRepository;
    delete m_clipPluginManager;

    m_self = 0;
}

void Core::initialize(MainWindow* mainWindow, const KUrl &projectUrl, const QString &clipsToLoad)
{
    m_self = new Core(mainWindow);
    m_self->init(projectUrl, clipsToLoad);
}

void Core::init(const KUrl &projectUrl, const QString &clipsToLoad)
{
    m_effectRepository = new EffectRepository();
    m_clipPluginManager = new ClipPluginManager();
    m_projectManager = new ProjectManager(projectUrl, clipsToLoad);
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
