/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "mainwindow.h"
#include "core.h"
#include "undoview.h"
#include "pluginmanager.h"
#include "kdenlivesettings.h"
#include "project/project.h"
#include "effectsystem/effectrepository.h"
#include "producersystem/producerrepository.h"
#include "project/clippluginmanager.h"
#include "project/projectmanager.h"
#include "bin/bin.h"
#include "monitor/monitorview.h"
#include "timelineview/timelinewidget.h"
#include <KLocale>
#include <QDockWidget>
#include <QGraphicsView>
#include <QLocale>
#include <QMenu>
#include <locale.h>
#include <KActionCollection>
#include <KTabWidget>
#include <KXMLGUIFactory>

#include <KDebug>


MainWindow::MainWindow(const QString &/*mltPath*/, const KUrl &url, const QString &/*clipsToLoad*/, QWidget* parent) :
    KXmlGuiWindow(parent)
{
    initLocale();

    Core::initialize(this);

    m_bin = new Bin(this);
    addDock(i18n("Bin"), "bin", m_bin);

    UndoView *undoView = new UndoView(this);
    addDock(i18n("Undo History"), "undo_history", undoView);
    m_container = new KTabWidget(this);
    m_container->setTabsClosable(true);
    m_timeline = new TimelineWidget(this);
    m_container->addTab(m_timeline, i18n("Timeline"));
    setCentralWidget(m_container);
    setDockNestingEnabled(true);

    KStandardAction::quit(this, SLOT(close()), actionCollection());

    pCore->pluginManager()->load();
    setupGUI();
    
    createActions();

    if (!url.isEmpty() && url.isValid()) {
        pCore->projectManager()->openProject(url);
    } else {
        // Start with new project
        pCore->projectManager()->openProject(KUrl());
    }
}

MainWindow::~MainWindow()
{
    delete m_bin;
}

TimelineWidget *MainWindow::addTimeline(const QString &/*id*/, const QString &title)
{
    TimelineWidget *timeline = new TimelineWidget(this);
    m_container->addTab(timeline, title);
    return timeline;
}

QDockWidget *MainWindow::addDock(const QString &title, const QString &objectName, QWidget* widget, Qt::DockWidgetArea area)
{
    QDockWidget *dockWidget = new QDockWidget(title, this);
    dockWidget->setObjectName(objectName);
    dockWidget->setWidget(widget);
    addDockWidget(area, dockWidget);
    return dockWidget;
}

void MainWindow::initLocale()
{
    QLocale systemLocale;
    setlocale(LC_NUMERIC, NULL);
    char *separator = localeconv()->decimal_point;
    if (separator != systemLocale.decimalPoint()) {
        kDebug()<<"------\n!!! system locale is not similar to Qt's locale... be prepared for bugs!!!\n------";
        // HACK: There is a locale conflict, so set locale to at least have correct decimal point
        if (strncmp(separator, ".", 1) == 0) {
            systemLocale = QLocale::c();
        } else if (strncmp(separator, ",", 1) == 0) {
            systemLocale = QLocale("fr_FR.UTF-8");
        }
    }

    systemLocale.setNumberOptions(QLocale::OmitGroupSeparator);
    QLocale::setDefault(systemLocale);
}

Bin* MainWindow::bin()
{
    return m_bin;
}

TimelineWidget* MainWindow::timelineWidget()
{
    return m_timeline;
}

void MainWindow::createActions()
{
    QMenu *addClipMenu = static_cast<QMenu*>(factory()->container("clip_producer_menu", this));
    QList <QAction *> producerActions = pCore->producerRepository()->producerActions(this);
    for (int i = 0; i < producerActions.count(); ++i) {
        addClipMenu->addAction(producerActions.at(i));
    }
    producerActions.clear();
    connect(addClipMenu, SIGNAL(triggered(QAction*)), pCore->clipPluginManager(), SLOT(execAddClipDialog(QAction*)));
    m_bin->setActionMenus(addClipMenu);
}


#include "mainwindow.moc"
