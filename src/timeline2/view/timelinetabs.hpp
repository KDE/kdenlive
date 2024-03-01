/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QMutex>
#include <QTabWidget>
#include <memory>

class TimelineWidget;
class TimelineItemModel;
class AssetParameterModel;
class EffectStackModel;
class MonitorProxy;
class QMenu;

/** @class TimelineContainer
    @brief This is a class that extends QTabWidget to provide additional functionality related to timeline tabs
 */
class TimelineContainer : public QWidget
{

public:
    TimelineContainer(QWidget *parent);

protected:
    QSize sizeHint() const override;
};

class TimelineTabs : public QTabWidget
{
    Q_OBJECT

public:
    /** @brief Construct the tabs as well as the widget for the main timeline */
    TimelineTabs(QWidget *parent);
    ~TimelineTabs() override;

    /** @brief Returns a pointer to the current timeline */
    TimelineWidget *getCurrentTimeline() const;
    /** @brief Activate a timeline tab by uuid */
    bool raiseTimeline(const QUuid &uuid);
    void disconnectTimeline(TimelineWidget *timeline);
    /** @brief Do some closing stuff on timelinewidgets */
    void closeTimelines();
    /** @brief Store timeline menus */
    void setTimelineMenu(QMenu *compositionMenu, QMenu *timelineMenu, QMenu *guideMenu, QMenu *timelineRulerMenu, QAction *editGuideAction, QMenu *headerMenu,
                         QMenu *thumbsMenu, QMenu *subtitleClipMenu);
    /** @brief Mark a tab as modified */
    void setModified(const QUuid &uuid, bool modified);
    /** @brief Returns the uuid list for opened timeline tabs. */
    const QStringList openedSequences();
    /** @brief Get a timeline tab by uuid. */
    TimelineWidget *getTimeline(const QUuid uuid) const;
    /** @brief We display the current tab's name in window title if the tab bar is hidden
     */
    void updateWindowTitle();
    /** @brief Build the timeline clip menu with dynamic actions. */
    void buildClipMenu();

protected:
    /** @brief Helper function to connect a timeline's signals/slots*/
    void connectTimeline(TimelineWidget *timeline);

Q_SIGNALS:
    /** @brief Request repaint of audio thumbs
        This is an input signal, forwarded to the timelines
     */
    void audioThumbFormatChanged();

    /** @brief The parameter controlling whether we show video thumbnails has changed
        This is an input signal, forwarded to the timelines
    */
    void showThumbnailsChanged();

    /** @brief The parameter controlling whether we show audio thumbnails has changed
        This is an input signal, forwarded to the timelines
    */
    void showAudioThumbnailsChanged();

    /** @brief Change the level of zoom
        This is an input signal, forwarded to the timelines
     */
    void changeZoom(int value, bool zoomOnMouse);
    void fitZoom();
    /** @brief Requests that a given parameter model is displayed in the asset panel */
    void showTransitionModel(int tid, std::shared_ptr<AssetParameterModel>);
    /** @brief Requests that a given mix is displayed in the asset panel */
    void showMixModel(int cid, std::shared_ptr<AssetParameterModel>, bool refreshOnly);
    /** @brief Requests that a given effectstack model is displayed in the asset panel */
    void showItemEffectStack(const QString &clipName, std::shared_ptr<EffectStackModel>, QSize size, bool showKeyframes);
    void showSubtitle(int itemId);
    void updateAssetPosition(int itemId, const QUuid uuid);
    /** @brief Zoom level changed in timeline, update slider
     */
    void updateZoom(int);

public Q_SLOTS:
    TimelineWidget *addTimeline(const QUuid uuid, const QString &tabName, std::shared_ptr<TimelineItemModel> timelineModel, MonitorProxy *proxy);
    void connectCurrent(int ix);
    void closeTimelineByIndex(int ix);
    void closeTimelineTab(const QUuid uuid);
    void renameTab(const QUuid &uuid, const QString &name);
    void slotNextSequence();
    void slotPreviousSequence();

private Q_SLOTS:
    void onTabBarDoubleClicked(int index);

private:
    TimelineWidget *m_activeTimeline;
    QMenu *m_timelineClipMenu{nullptr};
    QMenu *m_timelineCompositionMenu;
    QMenu *m_timelineMenu;
    QMenu *m_timelineRulerMenu;
    QMenu *m_guideMenu;
    QMenu *m_headerMenu;
    QMenu *m_thumbsMenu;
    QAction *m_editGuideAction;
    QMenu *m_timelineSubtitleClipMenu;
    QMutex m_lock;
};
