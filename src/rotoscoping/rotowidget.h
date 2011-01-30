/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#ifndef ROTOWIDGET_H
#define ROTOWIDGET_H

#include "bpoint.h"

#include <QWidget>

class Monitor;
class MonitorScene;
class SplineItem;

class RotoWidget : public QWidget
{
    Q_OBJECT

public:
    RotoWidget(QString data, Monitor *monitor, int in, int out, QWidget* parent = 0);
    virtual ~RotoWidget();

    QString getSpline();

public slots:
    /** @brief Switches from normal monitor to monitor scene according to @param show. */
    void slotShowScene(bool show = true);
    /** @brief Updates the on-monitor item.  */
    void slotSyncPosition(int relTimelinePos);

signals:
    void valueChanged();
    void checkMonitorPosition(int);


private:
    Monitor *m_monitor;
    MonitorScene *m_scene;
    bool m_showScene;
    QVariant m_data;
    SplineItem *m_item;
    int m_in;
    int m_out;
    int m_pos;

private slots:
    /** @brief Makes sure the monitor effect scene is only visible if the clip this geometry belongs to is visible.
    * @param renderPos Postion of the Monitor / Timeline cursor */
    void slotCheckMonitorPosition(int renderPos);

    void slotUpdateData();
};

#endif
