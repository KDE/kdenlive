/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "projectmanagerview.h"
#include "core.h"
#include "projectmanager.h"
#include "mainwindow.h"

#include <KMessageBox>

ProjectManagerView::ProjectManagerView(MainWindow *parent) :
    QObject(parent),
    m_model(pCore->projectManager())
{
    
    KAction *fileRevert = KStandardAction::revert(this, SLOT(revertFile()), parent->actionCollection());
    fileRevert->setEnabled(false);
    connect(m_model, SIGNAL(revertPossible(bool)), fileRevert, SLOT(setEnabled(bool)));

    KStandardAction::open(m_model,                   SLOT(openFile()),               parent->actionCollection());
    KStandardAction::saveAs(m_model,                 SLOT(saveFileAs()),             parent->actionCollection());
    KStandardAction::openNew(m_model,                SLOT(newFile()),                parent->actionCollection());
}

ProjectManagerView::~ProjectManagerView()
{
}

void ProjectManagerView::revertFile()
{
    if (KMessageBox::warningContinueCancel(pCore->window(),
            i18n("This will delete all changes made since you last saved your project. Are you sure you want to continue?"),
            i18n("Revert to last saved version")) == KMessageBox::Cancel)
        return;

    m_model->revertFile();
}

#include "project/projectmanagerview.moc"
