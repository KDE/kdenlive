/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "undoview.h"
#include "core.h"
#include "project/project.h"
#include "project/projectmanager.h"
#include <KLocale>
#include <KIcon>


UndoView::UndoView(QWidget* parent) :
    QUndoView(parent)
{
    setCleanIcon(KIcon("edit-clear"));
    setEmptyLabel(i18n("Clean"));

    connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));
}

void UndoView::setProject(Project* project)
{
    setStack(project->undoStack());
}

#include "undoview.moc"
