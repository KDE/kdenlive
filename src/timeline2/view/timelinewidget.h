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

#include "timeline2/model/timelineitemmodel.hpp"
#include <QQuickWidget>

class ThumbnailProvider;
class TimelineController;
class QSortFilterProxyModel;
class MonitorProxy;
class QMenu;
class QActionGroup;

class TimelineWidget : public QQuickWidget
{
    Q_OBJECT

public:
    TimelineWidget(QWidget *parent = Q_NULLPTR);
    ~TimelineWidget() override;
    /* @brief Sets the model shown by this widget */
    void setModel(const std::shared_ptr<TimelineItemModel> &model, MonitorProxy *proxy);

    /* @brief Return the project's tractor
     */
    Mlt::Tractor *tractor();
    TimelineController *controller();
    std::shared_ptr<TimelineItemModel> model();
    void setTool(ProjectTool tool);
    QPair<int, int>getTracksCount() const;
    /* @brief calculate zoom level for a scale */
    int zoomForScale(double value) const;
    /* @brief Give keyboard focus to timeline qml */
    void focusTimeline();
    /** @brief Initiate timeline clip context menu */
    void setTimelineMenu(QMenu *clipMenu, QMenu *compositionMenu, QMenu *timelineMenu, QMenu *timelineRulerMenu, QMenu *guideMenu, QAction *editGuideAction, QMenu *headerMenu, QMenu *thumbsMenu);
    bool loading;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

public slots:
    void slotChangeZoom(int value, bool zoomOnMouse);
    void slotFitZoom();
    void zoneUpdated(const QPoint &zone);
    void zoneUpdatedWithUndo(const QPoint &oldZone, const QPoint &newZone);
    /* @brief Favorite effects have changed, reload model for context menu */
    void updateEffectFavorites();
    /* @brief Favorite transitions have changed, reload model for context menu */
    void updateTransitionFavorites();
    /* @brief Bin clip drag ended, make sure we correctly processed the drop */
    void endDrag();
    /* @brief Show menu to switch track target audio stream */
    void showTargetMenu(int tid = -1);

private slots:
    void slotUngrabHack();
    void showClipMenu(int cid);
    void showCompositionMenu();
    void showTimelineMenu();
    void showRulerMenu();
    void showHeaderMenu();

private:
    TimelineController *m_proxy;
    QMenu *m_timelineClipMenu;
    QMenu *m_timelineCompositionMenu;
    QMenu *m_timelineMenu;
    QMenu *m_timelineRulerMenu;
    QMenu *m_guideMenu;
    QMenu *m_headerMenu;
    QMenu *m_targetsMenu;
    QActionGroup *m_targetsGroup;
    QMenu *m_thumbsMenu;
    QMenu *m_favEffects;
    QMenu *m_favCompositions;
    QAction *m_editGuideAcion;
    static const int comboScale[];
    std::unique_ptr<QSortFilterProxyModel> m_sortModel;
    /* @brief Keep last scale before fit to restore it on second click */
    double m_prevScale;
    /* @brief Keep last scroll position before fit to restore it on second click */
    int m_scrollPos;
    /* @brief Returns an alphabetically sorted list of favorite effects or transitions */
    const QMap<QString, QString> sortedItems(const QStringList &items, bool isTransition);
    QPoint m_clickPos;

signals:
    void focusProjectMonitor();
    void zoneMoved(const QPoint &zone);
};

#endif
