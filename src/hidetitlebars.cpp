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
#include <klocalizedstring.h>

HideTitleBars::HideTitleBars(QObject *parent) :
    QObject(parent)
{
    m_switchAction = new QAction(i18n("Show Title Bars"), this);
    m_switchAction->setCheckable(true);
    m_switchAction->setChecked(KdenliveSettings::showtitlebars());
    pCore->window()->addAction(QStringLiteral("show_titlebars"), m_switchAction);
    connect(m_switchAction, &QAction::triggered, this, &HideTitleBars::slotShowTitleBars);
    connect(pCore->window(), &MainWindow::GUISetupDone, this, &HideTitleBars::slotInstallRightClick);
}

void HideTitleBars::slotInstallRightClick()
{
    QList<QTabBar *> tabs = pCore->window()->findChildren<QTabBar *>();
    for (int i = 0; i < tabs.count(); ++i) {
        tabs.at(i)->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tabs.at(i), &QWidget::customContextMenuRequested, this, &HideTitleBars::slotSwitchTitleBars);
    }
    slotShowTitleBars(KdenliveSettings::showtitlebars());
}

void HideTitleBars::slotShowTitleBars(bool show)
{
    QList<QDockWidget *> docks = pCore->window()->findChildren<QDockWidget *>();
    for (int i = 0; i < docks.count(); ++i) {
        QDockWidget *dock = docks.at(i);
        QWidget *bar = dock->titleBarWidget();
        if (show) {
            if (dock->isFloating()) {
                if (bar) {
                    dock->setTitleBarWidget(nullptr);
                    delete bar;
                }
                continue;
            }
            // Since Qt 5.6 we only display title bar in non tabbed dockwidgets
            QList<QDockWidget *> docked = pCore->window()->tabifiedDockWidgets(dock);
            if (docked.isEmpty()) {
                if (bar) {
                    dock->setTitleBarWidget(nullptr);
                    delete bar;
                }
                continue;
            } else {
                bool hasVisibleDockSibling = false;
                foreach (QDockWidget *sub, docked) {
                    if (sub->toggleViewAction()->isChecked()) {
                        // we have another docked widget, so tabs are visible and can be used instead of title bars
                        hasVisibleDockSibling = true;
                        break;
                    }
                }
                if (!hasVisibleDockSibling) {
                    if (bar) {
                        dock->setTitleBarWidget(nullptr);
                        delete bar;
                    }
                    continue;
                }
            }
            if (!bar) {
                dock->setTitleBarWidget(new QWidget);
            }
        } else {
            if (!dock->isFloating() && !bar) {
                dock->setTitleBarWidget(new QWidget);
            }
        }
    }
    KdenliveSettings::setShowtitlebars(show);
}

void HideTitleBars::slotSwitchTitleBars()
{
    m_switchAction->trigger();
}

