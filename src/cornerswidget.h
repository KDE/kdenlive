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


#ifndef CORNERSWIDGET_H
#define CORNERSWIDGET_H

#include "ui_cornerswidget_ui.h"

#include <QWidget>

class QDomElement;
class Monitor;
class MonitorScene;
class OnMonitorCornersItem;
class MonitorSceneControlWidget;


class CornersWidget : public QWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the UI and connects it.
     * @param monitor Project monitor
     * @param clipPos Position of the clip in timeline
     * @param isEffect true if used in an effect, false if used in a transition
     * @param factor Factor by which the parameters differ from the range 0-1
     * @param parent (optional) Parent widget */
    CornersWidget(Monitor *monitor, int clipPos, bool isEffect, int factor, QWidget* parent = 0);
    virtual ~CornersWidget();

    /** @brief Returns a polygon representing the corners in the range 0 - factor. */
    QPolygon getValue();

    /** @brief Takes a polygon @param points in the range 0 - factor and converts it into range (- frame width|height) - (2*frame width|height). */
    void setValue(const QPolygon &points);

    /** @brief Takes in and outpoint of the clip to know when to show the on-monitor scene.
     * @param minframe In point of the clip
     * @param maxframe Out point of the clip */
    void setRange(int minframe, int maxframe);

private:
    Ui::CornersWidget_UI m_ui;
    Monitor *m_monitor;
    /** Position of the clip in timeline. */
    int m_clipPos;
    /** In point of the clip (crop from start). */
    int m_inPoint;
    /** Out point of the clip (crop from end). */
    int m_outPoint;
    bool m_isEffect;
    MonitorScene *m_scene;
    OnMonitorCornersItem *m_item;
    bool m_showScene;
    MonitorSceneControlWidget *m_config;
    int m_factor;

private slots:
    /** @brief Makes sure the monitor effect scene is only visible if the clip this geometry belongs to is visible.
    * @param renderPos Postion of the Monitor / Timeline cursor */
    void slotCheckMonitorPosition(int renderPos);

    /** @brief Switches from normal monitor to monitor scene according to @param show. */
    void slotShowScene(bool show = true);

    /** @brief Updates the on-monitor item according to the spinboxes. */
    void slotUpdateItem();
    /** @brief Updates the spinboxes according to the on-monitor item.
     * @param changed (default = true) Whether to emit parameterChanged */
    void slotUpdateProperties(bool changed = true);

signals:
    void parameterChanged();
    void checkMonitorPosition(int);
};

#endif
