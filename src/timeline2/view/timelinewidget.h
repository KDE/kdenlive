/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef TIMELINEWIDGET_H
#define TIMELINEWIDGET_H

#include "assets/assetlist/model/assetfilter.hpp"
#include "assets/assetlist/model/assettreemodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include <QQuickWidget>

class ThumbnailProvider;
class KActionCollection;
class AssetParameterModel;
class TimelineController;

class TimelineWidget : public QQuickWidget
{
    Q_OBJECT

public:
    TimelineWidget(KActionCollection *actionCollection, QWidget *parent = Q_NULLPTR);
    ~TimelineWidget();
    /* @brief Sets the model shown by this widget */
    void setModel(std::shared_ptr<TimelineItemModel> model);

    /* @brief Return the project's tractor
     */
    Mlt::Tractor *tractor();
    TimelineController *controller();
    void setTool(ProjectTool tool);
    QPoint getTracksCount() const;
    bool loading;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

public slots:
    void slotChangeZoom(int value, bool zoomOnMouse);
    void zoneUpdated(const QPoint &zone);

private:
    ThumbnailProvider *m_thumbnailer;
    TimelineController *m_proxy;
    static const int comboScale[];
    std::shared_ptr<AssetTreeModel> m_transitionModel;
    std::unique_ptr<AssetFilter> m_transitionProxyModel;

signals:
    void focusProjectMonitor();
    /* @brief Requests a zoom in for timeline
       @param zoomOnMouse is set to true if we want to center zoom on mouse, otherwise we center on timeline cursor position
     */
    void zoomIn(bool zoomOnMouse);
    /* @brief Requests a zoom out for timeline
       @param zoomOnMouse is set to true if we want to center zoom on mouse, otherwise we center on timeline cursor position
     */
    void zoomOut(bool zoomOnMouse);
    void zoneMoved(const QPoint &zone);
};

#endif
