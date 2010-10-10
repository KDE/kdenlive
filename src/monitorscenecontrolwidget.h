/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#ifndef MONITORSCENECONTROLWIDGET_H
#define MONITORSCENECONTROLWIDGET_H

#include "ui_monitorscenecontrolwidget_ui.h"

#include <QWidget>
#include <QToolButton>

class MonitorScene;


class MonitorSceneControlWidget : public QWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the UI and connects it. */
    MonitorSceneControlWidget(MonitorScene *scene, QWidget *parent = 0);
    virtual ~MonitorSceneControlWidget();

    /** @brief Returns a button for showing and hiding the monitor scene controls (this widget). */
    QToolButton *getShowHideButton();

private:
    Ui::MonitorSceneControlWidget_UI m_ui;
    MonitorScene *m_scene;
    QToolButton *m_buttonConfig;

signals:
    void showScene(bool);
};

#endif
