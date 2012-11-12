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
#include "mainwindow.h"
#include "timecodeformatter.h"
#include "binmodel.h"
#include "abstractprojectpart.h"
#include "projectmanager.h"
#include "monitor/monitormodel.h"
#include <mlt++/Mlt.h>
#include <KIO/NetAccess>
#include <QFile>
#include <QDomImplementation>
#include <QUndoStack>
#include <KLocale>

#include <KDebug>


Project::Project(const KUrl& url, QObject* parent) :
    QObject(parent),
    m_url(url)
{
    if (url.isEmpty()) {
        openNew();
    } else {
        openFile();
    }

    m_undoStack = new QUndoStack(this);
}

Project::Project(QObject* parent) :
    QObject(parent)
{
    openNew();

    m_undoStack = new QUndoStack(this);
}

Project::~Project()
{
}

KUrl Project::url() const
{
    return m_url;
}

QString Project::caption()
{
    if (m_url.isEmpty())
        return i18n("Untitled") + " / " + profile()->description();
    else
        return m_url.fileName() + " / " + profile()->description();
}

Timeline* Project::timeline()
{
    return m_timeline;
}

BinModel* Project::bin()
{
    return m_bin;
}

Mlt::Profile* Project::profile()
{
    return m_timeline->profile();
}

MonitorModel* Project::binMonitor()
{
    return m_bin->monitor();
}

MonitorModel* Project::timelineMonitor()
{
    return m_timeline->monitor();
}

TimecodeFormatter* Project::timecodeFormatter()
{
    return m_timecodeFormatter;
}

QUndoStack* Project::undoStack()
{
    return m_undoStack;
}

QString Project::setting(const QString& name) const
{
    return m_settings.value(name);
}

void Project::setSetting(const QString& name, const QString& value)
{
    m_settings.insert(name, value);
}

void Project::openFile()
{
    QString temporaryFileName;
    if (KIO::NetAccess::download(m_url.path(), temporaryFileName, NULL)) {
        QFile file(temporaryFileName);
        QString errorMessage;
        QDomImplementation::setInvalidDataPolicy(QDomImplementation::DropInvalidChars);
        QDomDocument document;
        bool success = document.setContent(&file, false, &errorMessage);
        file.close();
        KIO::NetAccess::removeTempFile(temporaryFileName);

        if (success) {
            loadTimeline(document.toString());
            QDomElement kdenliveDoc = document.documentElement().firstChildElement("kdenlivedoc");
            m_bin = new BinModel(kdenliveDoc.firstChildElement("project_items"), this);
            loadParts(kdenliveDoc);
            loadSettings(kdenliveDoc.firstChildElement("settings"));
            m_timeline->loadTracks();
        } else {
            kWarning() << "unable to load document" << m_url.path() << errorMessage;
        }
    } else {
        kWarning() << "not able to access " << m_url.path();
    }
}

void Project::openNew()
{
    m_timeline = new Timeline(this);
    m_timecodeFormatter = new TimecodeFormatter(Fraction(profile()->frame_rate_num(), profile()->frame_rate_den()));
    m_bin = new BinModel(this);
}

void Project::loadTimeline(const QString& content)
{
    m_timeline = new Timeline(content, this);
    m_timecodeFormatter = new TimecodeFormatter(Fraction(profile()->frame_rate_num(), profile()->frame_rate_den()));
}

void Project::loadParts(const QDomElement& element)
{
    QList<AbstractProjectPart*> parts = pCore->projectManager()->parts();
    for (int i = 0; i < parts.count(); ++i) {
        parts.at(i)->load(element.firstChildElement(parts.at(i)->name()));
    }
}

void Project::loadSettings(const QDomElement& settings)
{
    QDomNamedNodeMap attributes = settings.attributes();
    for (int i = 0; i < attributes.length(); ++i) {
        QDomNode attribute = attributes.item(i);
        m_settings.insert(attribute.nodeName(), attribute.nodeValue());
    }
}

void Project::save()
{
    if (m_url.isEmpty()) {
        
    }

    QFile file(m_url.path());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        // TODO: warning
        return /*false*/;
    }
    
    QDomDocument document;
    document.setContent(m_timeline->toXML(), false);

    file.write(document.toString().toUtf8());
    file.close();
}

#include "project.moc"
