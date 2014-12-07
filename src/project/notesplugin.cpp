/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "notesplugin.h"
#include "dialogs/noteswidget.h"
#include "core.h"
#include "mainwindow.h"
#include "monitor/monitormanager.h"
#include "doc/kdenlivedoc.h"
#include "project/projectmanager.h"

#include "klocalizedstring.h"

NotesPlugin::NotesPlugin(ProjectManager *projectManager) :
    QObject(projectManager)
{
    m_widget = new NotesWidget();
    connect(m_widget, SIGNAL(insertNotesTimecode()), SLOT(slotInsertTimecode()));
    m_widget->setTabChangesFocus(true);
#if KDE_IS_VERSION(4,4,0)
    m_widget->setPlaceholderText(i18n("Enter your project notes here ..."));
#endif
    pCore->window()->addDock(i18n("Project Notes"), "notes_widget", m_widget);

    connect(projectManager, SIGNAL(docOpened(KdenliveDoc*)), SLOT(setProject(KdenliveDoc*)));
}

void NotesPlugin::setProject(KdenliveDoc* document)
{
    connect(m_widget, SIGNAL(seekProject(int)), pCore->monitorManager()->projectMonitor()->render, SLOT(seekToFrame(int)));
    connect(m_widget, SIGNAL(textChanged()), document, SLOT(setModified()));
}

void NotesPlugin::slotInsertTimecode()
{
    int frames = pCore->monitorManager()->projectMonitor()->render->seekPosition().frames(pCore->projectManager()->current()->fps());
    QString position = pCore->projectManager()->current()->timecode().getTimecodeFromFrames(frames);
    m_widget->insertHtml("<a href=\"" + QString::number(frames) + "\">" + position + "</a> ");
}

NotesWidget* NotesPlugin::widget()
{
    return m_widget;
}


