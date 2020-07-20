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
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include <klocalizedstring.h>

HideTitleBars::HideTitleBars(QObject *parent)
    : QObject(parent)
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
    // install right click
    QList<QTabBar *> tabs = pCore->window()->findChildren<QTabBar *>();
    for (QTabBar *tab : qAsConst(tabs)) {
        tab->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(tab, &QWidget::customContextMenuRequested, this, &HideTitleBars::slotSwitchTitleBars);
    }
    // connect
    QList<QDockWidget *> docks = pCore->window()->findChildren<QDockWidget *>();
    for (QDockWidget *dock : qAsConst(docks)) {
        connect(dock, &QDockWidget::dockLocationChanged, pCore->window(), &MainWindow::slotUpdateDockLocation);
        connect(dock, &QDockWidget::topLevelChanged, pCore->window(), &MainWindow::updateDockTitleBars);
    }
    slotShowTitleBars(KdenliveSettings::showtitlebars());
}

void HideTitleBars::slotShowTitleBars(bool show)
{
    QList<QDockWidget *> docks = pCore->window()->findChildren<QDockWidget *>();
    for (QDockWidget *dock : qAsConst(docks)) {
        QWidget *bar = dock->titleBarWidget();
        auto handleRemoveBar = [&dock, &bar]() -> void {
            if (bar) {
                dock->setTitleBarWidget(nullptr);
                delete bar;
            }
        };

        if (!show) {
            if (!dock->isFloating() && bar == nullptr) {
                dock->setTitleBarWidget(new QWidget());
            }
            continue;
        }

        if (dock->isFloating()) {
            handleRemoveBar();
            continue;
        }
        // Since Qt 5.6 we only display title bar in non tabbed dockwidgets
        QList<QDockWidget *> docked = pCore->window()->tabifiedDockWidgets(dock);
        if (docked.isEmpty()) {
            handleRemoveBar();
            continue;
        }

        const bool hasVisibleDockSibling =
            std::find_if(std::begin(docked), std::end(docked), [](QDockWidget *sub) { return sub->toggleViewAction()->isChecked() && !sub->isTopLevel(); }) != std::end(docked);

        if (!hasVisibleDockSibling) {
            handleRemoveBar();
            continue;
        }

        if (!bar) {
            dock->setTitleBarWidget(new QWidget());
        }
    }
    KdenliveSettings::setShowtitlebars(show);
}

void HideTitleBars::slotSwitchTitleBars()
{
    m_switchAction->trigger();
}
