/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TIMELINEWIDGET_H
#define TIMELINEWIDGET_H

#include <QWidget>
#include "kdenlivecore_export.h"

class Project;
class ToolManager;
class TimelineView;
class Timeline;
class TimelineScene;
class TimelinePositionBar;
class TrackHeaderContainer;
class KToolBar;
class QStackedWidget;
class MarkersWidget;
class KComboBox;
class QFrame;

/**
 * @class TimelineWidget
 * @brief The timeline widgets contains the sub widgets (track headers, position bar, actual timeline view) and establishes the connections between them.
 */


class KDENLIVECORE_EXPORT TimelineWidget : public QWidget
{
    Q_OBJECT

public:
    /** @brief Creates the sub widgets and a tool manager. */
    explicit TimelineWidget(QWidget* parent = 0);
    virtual ~TimelineWidget();

    /** @brief Returns a pointer to the timeline view. */
    TimelineView *view();
    /** @brief Returns a pointer to the currently used timeline scene. */
    TimelineScene *scene();
    /** @brief Returns a pointer to the tool manager. */
    ToolManager *toolManager();
    void setClipTimeline(Timeline *timeline);
    void updateMarkers(const QList <int> markers);

private slots:
    /** @brief Creates a new timeline scene and triggers the creation of new track headers. */
    void setProject(Project *project);
    void slotAddMarker();
    void slotRemoveMarker(int pos);

private:
    TimelineScene *m_scene;
    TimelineView *m_view;
    TimelinePositionBar *m_positionBar;
    TrackHeaderContainer *m_headerContainer;
    ToolManager *m_toolManager;
    KToolBar *m_toolbar;
    QFrame *m_toolContainer;
    QStackedWidget *m_toolPanel;
    MarkersWidget *m_markersWidget;
    Timeline *m_clipTimeline;
    KComboBox *m_toolPanelSelector;
};

#endif
