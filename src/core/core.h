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

class MainWindow;
class ProjectManager;
class ClipPluginManager;
class EffectRepository;
class Project;
class PluginManager;
class MonitorManager;
class KUrl;


#define pCore Core::self()


class Core : public QObject
{
    Q_OBJECT

public:
    virtual ~Core();

    static void initialize(MainWindow *mainWindow);

    static Core *self();

    MainWindow *window();
    ProjectManager *projectManager();
    EffectRepository *effectRepository();
    ClipPluginManager *clipPluginManager();
    MonitorManager *monitorManager();
//     PluginManager *pluginManager();

private:
    Core(MainWindow *mainWindow);
    static Core *m_self;
    void init();

    MainWindow *m_mainWindow;
    ProjectManager *m_projectManager;
    EffectRepository *m_effectRepository;
    ClipPluginManager *m_clipPluginManager;
    MonitorManager *m_monitorManager;
//     PluginManager *m_pluginManager;
};

#endif
