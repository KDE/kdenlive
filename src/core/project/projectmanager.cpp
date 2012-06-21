/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "projectmanager.h"
#include "project.h"
#include "abstractprojectpart.h"
#include "core.h"
#include "mainwindow.h"
#include "bin/bin.h"
#include "timelineview/timelinewidget.h"
#include "monitor/monitorview.h"
#include <KFileDialog>
#include <KActionCollection>
#include <KAction>
#include <QUndoStack>


ProjectManager::ProjectManager(QObject* parent) :
    QObject(parent),
    m_project(0)
{
    KStandardAction::open(this, SLOT(execOpenFileDialog()), pCore->window()->actionCollection());

    m_undoAction = KStandardAction::undo(this, SLOT(undoCommand()), pCore->window()->actionCollection());
    m_redoAction = KStandardAction::redo(this, SLOT(redoCommand()), pCore->window()->actionCollection());
    m_undoAction->setPriority(QAction::LowPriority);
    m_redoAction->setPriority(QAction::LowPriority);
    m_undoAction->setEnabled(false);
    m_redoAction->setEnabled(false);

    openProject(KUrl());
}

ProjectManager::~ProjectManager()
{
}

Project* ProjectManager::current()
{
    return m_project;
}

void ProjectManager::execOpenFileDialog()
{
    KUrl url = KFileDialog::getOpenUrl(KUrl("kfiledialog:///projectfolder"), "application/x-kdenlive");
    if (!url.isEmpty()) {
        openProject(url);
    }
}

void ProjectManager::openProject(const KUrl& url)
{
    if (m_project) {
        delete m_project;
    }
    m_project = new Project(url, this);

    pCore->window()->setCaption(m_project->description());

    emit projectOpened(m_project);

    connect(m_project->undoStack(), SIGNAL(canUndoChanged(bool)), m_undoAction, SLOT(setEnabled(bool)));
    connect(m_project->undoStack(), SIGNAL(canRedoChanged(bool)), m_redoAction, SLOT(setEnabled(bool)));
    m_undoAction->setEnabled(false);
    m_redoAction->setEnabled(false);
}

void ProjectManager::registerPart(AbstractProjectPart* part)
{
    m_projectParts.append(part);
}

QList<AbstractProjectPart*> ProjectManager::parts()
{
    return m_projectParts;
}

void ProjectManager::undoCommand()
{
    if (m_project) {
        m_project->undoStack()->undo();
    }
}

void ProjectManager::redoCommand()
{
    if (m_project) {
        m_project->undoStack()->redo();
    }
}

#include "projectmanager.moc"
