/*
Copyright (C) 2012  Jean-Baptiste Mardelle <jb@kdenlive.org>
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
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

private:
    QAction *m_switchAction;

private slots:
    void slotInstallRightClick();
    void slotShowTitleBars(bool show);
    void slotSwitchTitleBars();
};

#endif
