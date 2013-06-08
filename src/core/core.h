/*
Copyright (C) 2012  Till Theato <root@ttill.de>
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
class ClipPluginManager;
class EffectRepository;
class ProducerRepository;
class Project;
class PluginManager;
class MonitorManager;
class MltCore;
class KUrl;

#define pCore Core::self()


/**
 * @class Core
 * @brief Singleton that provides access to the different parts of Kdenlive
 * 
 * Needs to be initialize before any widgets are created in MainWindow.
 * Plugins should be loaded after the widget setup.
 */

class KDENLIVECORE_EXPORT Core : public QObject
{
    Q_OBJECT

public:
    virtual ~Core();

    /**
     * @brief Constructs the singleton object and the managers.
     * @param mainWindow pointer to MainWindow
     */
    static void initialize(MainWindow *mainWindow);

    /** @brief Returns a pointer to the singleton object. */
    static Core *self();

    /** @brief Makes the plugin manager load all auto load plugins. */
    void loadPlugins();

    /** @brief Returns a pointer to the main window. */
    MainWindow *window();
    /** @brief Returns a pointer to the project manager. */
    ProjectManager *projectManager();
    /** @brief Returns a pointer to the mlt core. */
    MltCore* mltCore();
    /** @brief Returns a pointer to the effect repository. */
    EffectRepository *effectRepository();
    /** @brief Returns a pointer to the producer repository. */
    ProducerRepository* producerRepository();
    /** @brief Returns a pointer to the clip plugin manager. */
    ClipPluginManager *clipPluginManager();
    /** @brief Returns a pointer to the monitor manager. */
    MonitorManager *monitorManager();
    /** @brief Returns a pointer to the plugin manager. */
    PluginManager *pluginManager();

private:
    Core(MainWindow *mainWindow);
    static Core *m_self;
    void init();

    MainWindow *m_mainWindow;
    ProjectManager *m_projectManager;
    MltCore *m_mltCore;
    EffectRepository *m_effectRepository;
    ProducerRepository *m_producerRepository;
    ClipPluginManager *m_clipPluginManager;
    MonitorManager *m_monitorManager;
    PluginManager *m_pluginManager;
};

#endif
