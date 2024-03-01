/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

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
    TimelineWidget(const QUuid uuid, QWidget *parent = Q_NULLPTR);
    ~TimelineWidget() override;
    /** @brief Sets the model shown by this widget */
    void setModel(const std::shared_ptr<TimelineItemModel> &model, MonitorProxy *proxy);
    /** @brief Load the marker model (created after model ins instanciated) */
    void loadMarkerModel();

    /** @brief Return the project's tractor
     */
    Mlt::Tractor *tractor();
    TimelineController *controller();
    std::shared_ptr<TimelineItemModel> model();
    void setTool(ToolType::ProjectTool tool);
    ToolType::ProjectTool activeTool();
    QPair<int, int> getAvTracksCount() const;
    /** @brief calculate zoom level for a scale */
    int zoomForScale(double value) const;
    /** @brief Give keyboard focus to timeline qml */
    void focusTimeline();
    /** @brief Initiate timeline clip context menu */
    void setTimelineMenu(QMenu *clipMenu, QMenu *compositionMenu, QMenu *timelineMenu, QMenu *guideMenu, QMenu *timelineRulerMenu, QAction *editGuideAction, QMenu *headerMenu, QMenu *thumbsMenu, QMenu *subtitleClipMenu);
    bool loading;
    void connectSubtitleModel(bool firstConnect);
    void unsetModel();
    const QUuid &getUuid() const;
    bool hasSubtitles() const;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;

public Q_SLOTS:
    void slotChangeZoom(int value, bool zoomOnMouse);
    void slotFitZoom();
    /** @brief Center timeline view on current timeline cursor position */
    void slotCenterView();
    void zoneUpdated(const QPoint &zone);
    void zoneUpdatedWithUndo(const QPoint &oldZone, const QPoint &newZone);
    /** @brief Favorite effects have changed, reload model for context menu */
    void updateEffectFavorites();
    /** @brief Favorite transitions have changed, reload model for context menu */
    void updateTransitionFavorites();
    /** @brief Bin clip drag ended, make sure we correctly processed the drop */
    void endDrag();
    /** @brief Start an audio recording on track tid*/
    void startAudioRecord(int tid);
    void stopAudioRecord();
    /** @brief Show menu to switch track target audio stream */
    void showTargetMenu(int tid = -1);
    /** @brief Focus qml item under mouse in timeline, for example after app looses focus or a menu showed up*/
    void regainFocus();

private Q_SLOTS:
    void slotUngrabHack();
    void slotResetContextPos(QAction *);
    void showClipMenu(int cid);
    void showMixMenu(int cid);
    void showCompositionMenu();
    void showTimelineMenu();
    void showRulerMenu();
    void showHeaderMenu();
    void showSubtitleClipMenu();
    void emitMousePos(int offset);

private:
    TimelineController *m_proxy;
    QMenu *m_timelineClipMenu;
    QMenu *m_timelineMixMenu;
    QMenu *m_timelineCompositionMenu;
    QMenu *m_timelineMenu;
    QMenu *m_timelineRulerMenu;
    QMenu *m_guideMenu;
    QMenu *m_headerMenu;
    QMenu *m_targetsMenu;
    QActionGroup *m_targetsGroup{nullptr};
    QMenu *m_thumbsMenu;
    QMenu *m_favEffects;
    QMenu *m_favCompositions;
    QAction *m_editGuideAcion;
    QMenu *m_timelineSubtitleClipMenu;
    static const int comboScale[];
    std::unique_ptr<QSortFilterProxyModel> m_sortModel;
    /** @brief Keep last scale before fit to restore it on second click */
    double m_prevScale;
    /** @brief Keep last scroll position before fit to restore it on second click */
    int m_scrollPos;
    /** @brief Returns an alphabetically sorted list of favorite effects or transitions */
    const QMap<QString, QString> sortedItems(const QStringList &items, bool isTransition);
    QPoint m_clickPos;
    QUuid m_uuid;

Q_SIGNALS:
    void focusProjectMonitor();
    void zoneMoved(const QPoint &zone);
};
