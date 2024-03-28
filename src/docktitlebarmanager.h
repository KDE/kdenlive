/*
    SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>
class QAction;
class QDockWidget;

/**
 * @class DockTitleBarManager
 * @brief Helper for dock widget title bars eg. to change the visible state or add context menus.
 */
class DockTitleBarManager : public QObject
{
    Q_OBJECT

public:
    explicit DockTitleBarManager(QObject *parent = nullptr);

public Q_SLOTS:
    /** @brief Correctly hide/show dock widget title bars depending on position (floating, tabbed, docked) */
    void slotUpdateTitleBars(bool isTopLevel = true);
    /** @brief Connect/disconnect signals to update title bars on dock location changed */
    void connectDocks(bool doConnect);
    /** @brief Connect a dock that was created after app opening to signals updating title bars on dock location changed */
    void connectDockWidget(QDockWidget *dock);

private:
    QAction *m_switchAction;

private Q_SLOTS:
    void slotInstallRightClick();
    /** @brief Add/remove Dock tile bar depending on state (tabbed, floating, ...) */
    void slotUpdateDockLocation(Qt::DockWidgetArea dockLocationArea);
    /** @brief Set the global visible state of the DockWidget title bars and update them afterwards */
    void slotShowTitleBars(bool show);
    /** @brief Toggles the global visible state of the DockWidget title bars */
    void slotSwitchTitleBars();
};
