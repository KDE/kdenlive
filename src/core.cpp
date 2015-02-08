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
#include "timeline/trackview.h"
#include "monitor/monitormanager.h"
#include "mltcontroller/bincontroller.h"
#include "bin/bin.h"
#include <QCoreApplication>
#include <locale>
#include <QDebug>


Core *Core::m_self = NULL;


Core::Core(MainWindow *mainWindow) :
    m_mainWindow(mainWindow)
{
    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(deleteLater()));
}

Core::~Core()
{
    delete m_projectManager;
    delete m_binWidget;
    delete m_binController;
    m_self = 0;
}

void Core::initialize(MainWindow* mainWindow)
{
    m_self = new Core(mainWindow);
    m_self->init();
}

void Core::init()
{
    initLocale();
    m_projectManager = new ProjectManager(this);
    m_binWidget = new Bin();
    m_binController = new BinController();
    connect(m_binWidget, SIGNAL(storeFolder(QString,QString,QString,QString)), m_binController, SLOT(slotStoreFolder(QString,QString,QString,QString)));
    connect(m_binController, SIGNAL(loadFolders(QMap<QString,QString>)), m_binWidget, SLOT(slotLoadFolders(QMap<QString,QString>)));
    m_monitorManager = new MonitorManager(this);
    emit coreIsReady();
}

Core* Core::self()
{
    return m_self;
}

MainWindow* Core::window()
{
    return m_mainWindow;
}

ProjectManager *Core::projectManager()
{
    return m_projectManager;
}

MonitorManager* Core::monitorManager()
{
    return m_monitorManager;
}

BinController *Core::binController()
{
    return m_binController;
}

Bin *Core::bin()
{
    return m_binWidget;
}

void Core::initLocale()
{
    QLocale systemLocale = QLocale();
    setlocale(LC_NUMERIC, NULL);
    char *separator = localeconv()->decimal_point;
    if (separator != systemLocale.decimalPoint()) {
        //qDebug()<<"------\n!!! system locale is not similar to Qt's locale... be prepared for bugs!!!\n------";
        // HACK: There is a locale conflict, so set locale to C
        // Make sure to override exported values or it won't work
        setenv("LANG", "C", 1);
        setlocale(LC_NUMERIC, "C");
        systemLocale = QLocale::c();
    }

    systemLocale.setNumberOptions(QLocale::OmitGroupSeparator);
    QLocale::setDefault(systemLocale);
}


