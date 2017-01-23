/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef CORE_H
#define CORE_H

#include <QObject>
#include "kdenlivecore_export.h"

class MainWindow;
class ProjectManager;
class MonitorManager;
class BinController;
class Bin;
class LibraryWidget;
class ProducerQueue;
class TimelineWidget;

#define pCore Core::self()

/**
 * @class Core
 * @brief Singleton that provides access to the different parts of Kdenlive
 *
 * Needs to be initialize before any widgets are created in MainWindow.
 * Plugins should be loaded after the widget setup.
 */

class /*KDENLIVECORE_EXPORT*/ Core : public QObject
{
    Q_OBJECT

public:
    virtual ~Core();

    /**
     * @brief Constructs the singleton object and the managers.
     * @param mainWindow pointer to MainWindow
     */
    static void build(MainWindow *mainWindow);

    /** @brief Returns a pointer to the singleton object. */
    static Core *self();

    /** @brief Builds all necessary parts. */
    void initialize();

    /** @brief Returns a pointer to the main window. */
    MainWindow *window();

    /** @brief Returns a pointer to the project manager. */
    ProjectManager *projectManager();
    /** @brief Returns a pointer to the monitor manager. */
    MonitorManager *monitorManager();
    /** @brief Returns a pointer to the project bin controller. */
    BinController *binController();
    /** @brief Returns a pointer to the project bin. */
    Bin *bin();
    /** @brief Returns a pointer to the producer queue. */
    ProducerQueue *producerQueue();
    /** @brief Returns a pointer to the library. */
    LibraryWidget *library();
    /** @brief Returns a pointer to the timeline. */
    TimelineWidget *timeline();

private:
    explicit Core(MainWindow *mainWindow);
    static Core *m_self;

    /** @brief Makes sure Qt's locale and system locale settings match. */
    void initLocale();

    MainWindow *m_mainWindow;
    ProjectManager *m_projectManager;
    MonitorManager *m_monitorManager;
    BinController *m_binController;
    ProducerQueue *m_producerQueue;
    Bin *m_binWidget;
    LibraryWidget *m_library;
    TimelineWidget *m_timelineWidget;

signals:
    void coreIsReady();
    void updateLibraryPath();
};

#endif
