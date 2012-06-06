/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "project.h"
#include "core.h"
#include "timeline.h"
#include "projectfolder.h"
#include "abstractprojectclip.h"
#include "mainwindow.h"
#include "timecodeformatter.h"
#include "bin/bin.h"
#include "monitor/monitormodel.h"
#include <mlt++/Mlt.h>
#include <KIO/NetAccess>
#include <QFile>
#include <QDomImplementation>

#include <KDebug>


Project::Project(const KUrl& url, QObject* parent) :
    QObject(parent),
    m_url(url),
    m_timeline(0)
{
    if (!url.isEmpty()) {
        QString temporaryFileName;
        if (KIO::NetAccess::download(url.path(), temporaryFileName, NULL)) {
            QFile file(temporaryFileName);
            QString errorMessage;
            QDomImplementation::setInvalidDataPolicy(QDomImplementation::DropInvalidChars);
            QDomDocument document;
            bool success = document.setContent(&file, false, &errorMessage);
            file.close();
            KIO::NetAccess::removeTempFile(temporaryFileName);

            if (success) {
                loadTimeline(document.toString());
                loadClips(document.documentElement().firstChildElement("kdenlivedoc").firstChildElement("project_items"));
                m_timeline->loadTracks();
            } else {
                kWarning() << "unable to load document" << url.path() << errorMessage;
            }

        } else {
            kWarning() << "not able to access " << url.path();
        }
    } else {
        kWarning() << "empty URL";
    }

    m_monitor = new MonitorModel(this);
}

Project::Project(QObject* parent) :
    QObject(parent)
{
}

Project::~Project()
{
    if (m_timeline) {
        delete m_timeline;
    }
    if (m_items) {
        delete m_items;
    }
}

KUrl Project::url() const
{
    return m_url;
}

Timeline* Project::timeline()
{
    return m_timeline;
}

AbstractProjectClip* Project::clip(int id)
{
    return m_items->clip(id);
}

ProjectFolder* Project::items()
{
    return m_items;
}

Mlt::Profile* Project::profile()
{
    return m_timeline->profile();
}

void Project::addItem(AbstractProjectItem* item)
{
    // item is added to its parent by the clipPluginManager; no further action required here
    emit itemAdded(item);
}

MonitorModel* Project::monitor()
{
    return m_monitor;
}

TimecodeFormatter* Project::timecodeFormatter()
{
    return m_timecodeFormatter;
}

void Project::loadClips(const QDomElement& description)
{
    m_items = new ProjectFolder(description, this);
}

void Project::loadTimeline(const QString& content)
{
    m_timeline = new Timeline(content, this);
    m_timecodeFormatter = new TimecodeFormatter(Fraction(profile()->frame_rate_num(), profile()->frame_rate_den()));
}

#include "project.moc"
