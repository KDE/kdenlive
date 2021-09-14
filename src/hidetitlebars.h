/*
    SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>

    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef HIDETITLEBARS_H
#define HIDETITLEBARS_H

#include <QObject>
class QAction;

/**
 * @class HideTitleBars
 * @brief Handles functionality to switch the title bars of the dock widgets on/off.
 */
class HideTitleBars : public QObject
{
    Q_OBJECT

public:
    explicit HideTitleBars(QObject *parent = nullptr);

public slots:
    /** @brief Correctly hide/show dock widget title bars depending on position (floating, tabbed, docked) */
    void updateTitleBars();

private:
    QAction *m_switchAction;

private slots:
    void slotInstallRightClick();
    void slotShowTitleBars(bool show);
    void slotSwitchTitleBars();
};

#endif
