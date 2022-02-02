/*
    SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef DOCKTITLEBARMANAGER_H
#define DOCKTITLEBARMANAGER_H

#include <QObject>
class QAction;

/**
 * @class DockTitleBarManager
 * @brief Helper for dock widget title bars eg. to change the visible state or add context menus.
 */
class DockTitleBarManager : public QObject
{
    Q_OBJECT

public:
    explicit DockTitleBarManager(QObject *parent = nullptr);

public slots:
    /** @brief Correctly hide/show dock widget title bars depending on position (floating, tabbed, docked) */
    void slotUpdateTitleBars(bool isTopLevel = true);

private:
    QAction *m_switchAction;

private slots:
    void slotInstallRightClick();
    /** @brief Add/remove Dock tile bar depending on state (tabbed, floating, ...) */
    void slotUpdateDockLocation(Qt::DockWidgetArea dockLocationArea);
    /** @brief Set the global visible state of the DockWidget title bars and update them afterwards */
    void slotShowTitleBars(bool show);
    /** @brief Toggles the the global visible state of the DockWidget title bars */
    void slotSwitchTitleBars();
};

#endif
