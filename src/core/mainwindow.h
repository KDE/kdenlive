/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <KXmlGuiWindow>
#include <KUrl>
#include "kdenlivecore_export.h"

class TimelineWidget;
class Bin;
class KTabWidget;


/**
 * @class MainWindow
 * @brief Kdenlive's main window.
 */

class KDENLIVECORE_EXPORT MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

public:
    /**
     * @brief Initializes Core and creates some core widgets.
     * @param mltPath tbd
     * @param url url of project to load
     * @param clipsToLoad list of clips to add to project
     */
    explicit MainWindow(const QString &mltPath = QString(),
                        const KUrl &url = KUrl(), const QString & clipsToLoad = QString(), QWidget* parent = 0);
    virtual ~MainWindow();

    /** @brief Returns a pointer to the bin widget. */
    Bin *bin();

    /** @brief Returns a pointer to the timeline widget. */
    TimelineWidget *timelineWidget();

    /**
     * @brief Adds a new dock widget to this window.
     * @param title title of the dock widget
     * @param objectName objectName of the dock widget (required for storing layouts)
     * @param widget widget to use in the dock
     * @param area area to which the dock should be added to
     * @returns the created dock widget
     */
    QDockWidget *addDock(const QString &title, const QString &objectName, QWidget *widget, Qt::DockWidgetArea area = Qt::TopDockWidgetArea);
    TimelineWidget *addTimeline(const QString &id, const QString &title);
    TimelineWidget *getTimeline(const QString &id);

private slots:
    void slotCloseTimeline(int index);

private:
    void initLocale();
    void createClipMenu();

    Bin *m_bin;
    TimelineWidget *m_timeline;
    KTabWidget *m_container;
    
    /** @brief Creates the GUI actions, that needs to be done after calling setupGUI(). */
    void createActions();
    
};

#endif
