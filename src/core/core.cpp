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
#include "project/project.h"
#include "project/clippluginmanager.h"
#include "effectsystem/effectrepository.h"
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
    if (m_currentProject) {
        delete m_currentProject;
    }

    delete m_effectRepository;
    delete m_clipPluginManager;

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
    m_clipPluginManager = new ClipPluginManager();
}

Core* Core::self()
{
    return m_self;
}

MainWindow* Core::window()
{
    return m_mainWindow;
}

Project* Core::currentProject()
{
    return m_currentProject;
}

void Core::setCurrentProject(Project* project)
{
    if (m_currentProject) {
        delete m_currentProject;
    }

    m_currentProject = project;
}

EffectRepository* Core::effectRepository()
{
    return m_effectRepository;
}

ClipPluginManager* Core::clipPluginManager()
{
    return m_clipPluginManager;
}
