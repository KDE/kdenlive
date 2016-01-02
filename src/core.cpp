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
#include "mltcontroller/bincontroller.h"
#include "bin/bin.h"
#include "library/librarywidget.h"
#include <QCoreApplication>
#include <QDebug>

#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

Core *Core::m_self = NULL;


Core::Core(MainWindow *mainWindow) :
    m_mainWindow(mainWindow)
    , m_projectManager(NULL)
    , m_monitorManager(NULL)
    , m_binController(NULL)
    , m_binWidget(NULL)
    , m_library(NULL)
{
    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(deleteLater()));
}

Core::~Core()
{
    m_monitorManager->stopActiveMonitor();
    delete m_projectManager;
    delete m_binWidget;
    delete m_binController;
    delete m_monitorManager;
    m_self = 0;
}

void Core::build(MainWindow* mainWindow)
{
    m_self = new Core(mainWindow);
    m_self->initLocale();
}

void Core::initialize()
{
    m_projectManager = new ProjectManager(this);
    m_binWidget = new Bin();
    m_binController = new BinController();
    m_library = new LibraryWidget(m_projectManager);
    connect(m_library, &LibraryWidget::addProjectClips, m_binWidget, &Bin::droppedUrls);
    connect(m_binWidget, SIGNAL(storeFolder(QString,QString,QString,QString)), m_binController, SLOT(slotStoreFolder(QString,QString,QString,QString)));
    connect(m_binController, SIGNAL(loadFolders(QMap<QString,QString>)), m_binWidget, SLOT(slotLoadFolders(QMap<QString,QString>)));
    connect(m_binController, SIGNAL(requestAudioThumb(QString)), m_binWidget, SLOT(slotCreateAudioThumb(QString)));
    connect(m_binController, SIGNAL(abortAudioThumbs()), m_binWidget, SLOT(abortAudioThumbs()));
    connect(m_binController, SIGNAL(loadThumb(QString,QImage,bool)), m_binWidget, SLOT(slotThumbnailReady(QString,QImage,bool)));
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

LibraryWidget *Core::library()
{
    return m_library;
}

void Core::initLocale()
{
    QLocale systemLocale = QLocale();
#ifndef Q_OS_MAC
    setlocale(LC_NUMERIC, NULL);
#else
    setlocale(LC_NUMERIC_MASK, NULL);
#endif
    char *separator = localeconv()->decimal_point;
    if (separator != systemLocale.decimalPoint()) {
        //qDebug()<<"------\n!!! system locale is not similar to Qt's locale... be prepared for bugs!!!\n------";
        // HACK: There is a locale conflict, so set locale to C
        // Make sure to override exported values or it won't work
        qputenv("LANG", "C");
#ifndef Q_OS_MAC
        setlocale(LC_NUMERIC, "C");
#else
        setlocale(LC_NUMERIC_MASK, "C");
#endif
        systemLocale = QLocale::c();
    }

    systemLocale.setNumberOptions(QLocale::OmitGroupSeparator);
    QLocale::setDefault(systemLocale);
}


