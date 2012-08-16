/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "monitormanager.h"
#include "monitormodel.h"
#include "monitorview.h"
#include "core.h"
#include "mainwindow.h"
#include "project/project.h"
#include "project/projectmanager.h"
#include <KLocale>
#include <QSignalMapper>


MonitorManager::MonitorManager(QObject* parent) :
    QObject(parent),
    m_active(0)
{
    m_modelSignalMapper = new QSignalMapper(this);

    MonitorView *autoView = new MonitorView(pCore->window());
    pCore->window()->addDock(i18n("Monitor"), "auto_monitor", autoView);
    m_views.insert("auto", autoView);

    /*MonitorView *projectView = new MonitorView(1, pCore->window());
    pCore->window()->addDock(i18n("Monitor"), "project_monitor", projectView);
    m_views.insert("project", projectView);*/

    connect(m_modelSignalMapper, SIGNAL(mapped(const QString &)), this, SLOT(onModelActivated(const QString &)));
    connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));
}

void MonitorManager::registerModel(const QString &id, MonitorModel* model, bool needsOwnView)
{
    if (m_models.contains(id) || model == NULL) return;
    m_models.insert(id, model);
    if (m_views.contains(id)) {
        m_views.value(id)->setModel(model);
    } else {
        if (needsOwnView) {
            MonitorView *view = new MonitorView(pCore->window());
            view->setModel(model);
            m_views.insert(id, view);
            pCore->window()->addDock(model->name(), id + "_monitor", view);
        }
    }
    connect(model, SIGNAL(activated()), m_modelSignalMapper, SLOT(map()));
    m_modelSignalMapper->setMapping(model, id);
}

void MonitorManager::setProject(Project* project)
{
    registerModel("bin", project->binMonitor());
    registerModel("timeline", project->timelineMonitor());
}

void MonitorManager::onModelActivated(const QString &id)
{
    MonitorModel *model = m_models.value(id);

    if (model == m_active) {
        return;
    }

    m_active = model;

    MonitorView *view;
    if (m_views.contains(id)) {
        view = m_views.value(id);
    } else {
        view = m_views.value("auto");
        view->setModel(model);
    }

    view->parentWidget()->raise();
}

#include "monitormanager.moc"
