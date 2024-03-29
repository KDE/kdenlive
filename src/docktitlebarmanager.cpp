/*
    SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
QDockWidget
    This file is part of Kdenlive. See www.kdenlive.org.
*/

#include "docktitlebarmanager.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include <KLocalizedString>
#include <QDockWidget>

DockTitleBarManager::DockTitleBarManager(QObject *parent)
    : QObject(parent)
{
    m_switchAction = new QAction(i18n("Show Title Bars"), this);
    m_switchAction->setCheckable(true);
    m_switchAction->setChecked(KdenliveSettings::showtitlebars());
    pCore->window()->addAction(QStringLiteral("show_titlebars"), m_switchAction);
    connect(m_switchAction, &QAction::triggered, this, &DockTitleBarManager::slotShowTitleBars);
    connect(pCore->window(), &MainWindow::GUISetupDone, this, &DockTitleBarManager::slotInstallRightClick, Qt::DirectConnection);
}

void DockTitleBarManager::slotInstallRightClick()
{
    // install right click
    QList<QTabBar *> tabs = pCore->window()->findChildren<QTabBar *>();
    for (QTabBar *tab : qAsConst(tabs)) {
        tab->setContextMenuPolicy(Qt::CustomContextMenu);
        tab->setAcceptDrops(true);
        tab->setChangeCurrentOnDrag(true);
        connect(tab, &QWidget::customContextMenuRequested, this, &DockTitleBarManager::slotSwitchTitleBars);
    }

    connectDocks(true);
    slotUpdateTitleBars();
}

void DockTitleBarManager::connectDocks(bool doConnect)
{
    // connect
    QList<QDockWidget *> docks = pCore->window()->findChildren<QDockWidget *>();
    for (QDockWidget *dock : qAsConst(docks)) {
        if (doConnect) {
            connect(dock, &QDockWidget::dockLocationChanged, this, &DockTitleBarManager::slotUpdateDockLocation);
            connect(dock, &QDockWidget::topLevelChanged, this, &DockTitleBarManager::slotUpdateTitleBars);
        } else {
            disconnect(dock, &QDockWidget::dockLocationChanged, this, &DockTitleBarManager::slotUpdateDockLocation);
            disconnect(dock, &QDockWidget::topLevelChanged, this, &DockTitleBarManager::slotUpdateTitleBars);
        }
    }
}

void DockTitleBarManager::connectDockWidget(QDockWidget *dock)
{
    connect(dock, &QDockWidget::dockLocationChanged, this, &DockTitleBarManager::slotUpdateDockLocation);
    connect(dock, &QDockWidget::topLevelChanged, this, &DockTitleBarManager::slotUpdateTitleBars);
    slotUpdateTitleBars();
}

void DockTitleBarManager::slotUpdateDockLocation(Qt::DockWidgetArea dockLocationArea)
{
    slotUpdateTitleBars(dockLocationArea != Qt::NoDockWidgetArea);
}

void DockTitleBarManager::slotShowTitleBars(bool show)
{
    KdenliveSettings::setShowtitlebars(show);
    slotUpdateTitleBars();
}

void DockTitleBarManager::slotUpdateTitleBars(bool isTopLevel)
{
    QList<QTabBar *> tabbars = pCore->window()->findChildren<QTabBar *>();
    for (QTabBar *tab : qAsConst(tabbars)) {
        tab->setAcceptDrops(true);
        tab->setChangeCurrentOnDrag(true);
        // Fix tabbar tooltip containing ampersand
        for (int i = 0; i < tab->count(); i++) {
            tab->setTabToolTip(i, KLocalizedString::removeAcceleratorMarker(tab->tabText(i)));
        }
    }

    if (!KdenliveSettings::showtitlebars() && !isTopLevel) {
        return;
    }

    QList<QDockWidget *> docks = pCore->window()->findChildren<QDockWidget *>();
    for (QDockWidget *dock : qAsConst(docks)) {
        QWidget *bar = dock->titleBarWidget();
        auto handleRemoveBar = [&dock, &bar]() -> void {
            if (bar) {
                dock->setTitleBarWidget(nullptr);
                delete bar;
            }
        };

        if (!KdenliveSettings::showtitlebars()) {
            if (dock->isFloating()) {
                handleRemoveBar();
            } else if (bar == nullptr) {
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

        const bool hasVisibleDockSibling = std::find_if(std::begin(docked), std::end(docked), [](QDockWidget *sub) {
                                               return sub->toggleViewAction()->isChecked() && !sub->isTopLevel();
                                           }) != std::end(docked);

        if (!hasVisibleDockSibling) {
            handleRemoveBar();
            continue;
        }

        if (!bar) {
            dock->setTitleBarWidget(new QWidget());
        }
    }
}

void DockTitleBarManager::slotSwitchTitleBars()
{
    m_switchAction->trigger();
}
