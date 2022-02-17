/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef TIMELINETABS_H
#define TIMELINETABS_H

#include <QTabWidget>
#include <memory>

class TimelineWidget;
class AssetParameterModel;
class EffectStackModel;
class TimelineItemModel;
class MonitorProxy;
class QAction;
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
    void raiseTimeline(const QUuid &uuid);
    void disconnectTimeline(TimelineWidget *timeline);
    /** @brief Do some closing stuff on timelinewidgets */
    void closeTimelines();
    /** @brief Store timeline menus */
    void setTimelineMenu(QMenu *clipMenu, QMenu *compositionMenu, QMenu *timelineMenu, QMenu *guideMenu, QMenu *timelineRulerMenu, QAction *editGuideAction, QMenu *headerMenu, QMenu *thumbsMenu, QMenu *subtitleClipMenu);
    /** @brief Mark a tab as modified */
    void setModified(const QUuid &uuid, bool modified);

protected:
    /** @brief Helper function to connect a timeline's signals/slots*/
    void connectTimeline(TimelineWidget *timeline);

signals:
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
    void showItemEffectStack(const QString &clipName, std::shared_ptr<EffectStackModel>, QSize, bool);
    void showSubtitle(int itemId);
    /** @brief Zoom level changed in timeline, update slider
     */
    void updateZoom(int);

public slots:
    TimelineWidget *addTimeline(const QUuid &uuid, const QString &tabName, std::shared_ptr<TimelineItemModel> timelineModel, MonitorProxy *proxy);
    void connectCurrent(int ix);
    void closeTimelineByIndex(int ix);
    void closeTimeline(const QUuid &uuid);
    void renameTab(const QUuid &uuid, const QString &name);

private:
    TimelineWidget *m_activeTimeline;
    QMenu *m_timelineClipMenu;
    QMenu *m_timelineCompositionMenu;
    QMenu *m_timelineMenu;
    QMenu *m_timelineRulerMenu;
    QMenu *m_guideMenu;
    QMenu *m_headerMenu;
    QMenu *m_thumbsMenu;
    QAction *m_editGuideAction;
    QMenu *m_timelineSubtitleClipMenu;
};

#endif
