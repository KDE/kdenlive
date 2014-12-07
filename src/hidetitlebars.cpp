/*
Copyright (C) 2012  Jean-Baptiste Mardelle <jb@kdenlive.org>
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "hidetitlebars.h"
#include "core.h"
#include "mainwindow.h"
#include "kdenlivesettings.h"


HideTitleBars::HideTitleBars(QObject* parent) :
    QObject(parent)
{
    QAction *showTitleBar = new QAction(i18n("Show Title Bars"), this);
    showTitleBar->setCheckable(true);
    showTitleBar->setChecked(KdenliveSettings::showtitlebars());
    pCore->window()->addAction("show_titlebars", showTitleBar);
    connect(showTitleBar, SIGNAL(triggered(bool)), SLOT(slotShowTitleBars(bool)));

    slotShowTitleBars(KdenliveSettings::showtitlebars());

    connect(pCore->window(), SIGNAL(GUISetupDone()), SLOT(slotInstallRightClick()));
}

void HideTitleBars::slotInstallRightClick()
{
    QList <QTabBar *> tabs = pCore->window()->findChildren<QTabBar *>();
    for (int i = 0; i < tabs.count(); ++i) {
        tabs.at(i)->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tabs.at(i), SIGNAL(customContextMenuRequested(QPoint)), SLOT(slotSwitchTitleBars()));
    }
}

void HideTitleBars::slotShowTitleBars(bool show)
{
    QList <QDockWidget *> docks = pCore->window()->findChildren<QDockWidget *>();
    for (int i = 0; i < docks.count(); ++i) {
        QDockWidget* dock = docks.at(i);
        if (show) {
            dock->setTitleBarWidget(0);
        } else {
            if (!dock->isFloating()) {
                dock->setTitleBarWidget(new QWidget);
            }
        }
    }
    KdenliveSettings::setShowtitlebars(show);
}

void HideTitleBars::slotSwitchTitleBars()
{
    slotShowTitleBars(!KdenliveSettings::showtitlebars());
}


