/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "notesplugin.h"
#include "noteswidget.h"
#include "core/core.h"
#include "core/mainwindow.h"
#include "core/project/projectmanager.h"
#include "core/project/project.h"
#include "core/monitor/monitormodel.h"
#include "core/timecode.h"
#include <KLocalizedString>
#include <KPluginFactory>


K_PLUGIN_FACTORY( NotesPluginFactory, registerPlugin<NotesPlugin>(); )
K_EXPORT_PLUGIN( NotesPluginFactory( "kdenlivenotes" ) )

NotesPlugin::NotesPlugin(QObject* parent, const QVariantList& args) :
    AbstractProjectPart("notes", parent)
{
    m_widget = new NotesWidget(pCore->window());
    connect(m_widget, SIGNAL(insertTimecode()), this, SLOT(insertTimecode()));
    connect(m_widget, SIGNAL(seekProject(int)), this, SLOT(seekProject(int)));
    connect(m_widget, SIGNAL(textChanged()), this, SIGNAL(modified()));

    pCore->window()->addDock(i18n("Project Notes"), "notes_widget", m_widget);
}

void NotesPlugin::load(const QDomElement& element)
{
    m_widget->clear();
    if (!element.isNull()) {
        m_widget->setText(element.firstChild().nodeValue());
    }
}

void NotesPlugin::save(QDomDocument &document, QDomElement &element) const
{
    QDomText value = document.createTextNode(m_widget->toHtml());
    element.appendChild(value);
}

void NotesPlugin::insertTimecode()
{
    Timecode position = Timecode(pCore->projectManager()->current()->timelineMonitor()->position());
    m_widget->insertHtml("<a href=\"" + position.formatted(TimecodeFormatter::Frames) + "\">" + position.formatted(TimecodeFormatter::HH_MM_SS_FF) + "</a> ");
}

void NotesPlugin::seekProject(int position)
{
    pCore->projectManager()->current()->timelineMonitor()->setPosition(position);
}
