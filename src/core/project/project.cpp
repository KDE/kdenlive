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
#include "abstractprojectclip.h"
#include "projectmanager.h"
#include "monitor/monitorview.h"
#include "kdenlivesettings.h"

#include <mlt++/Mlt.h>
#include <KIO/NetAccess>
#include <QFile>
#include <QDomImplementation>
#include <QUndoStack>
#include <KLocale>
#include <KFileDialog>
#include <KApplication>
#include <KMessageBox>

#include <KDebug>


Project::Project(const KUrl& url, QObject* parent) :
    QObject(parent)
    , m_url(url)
    , m_timecodeFormatter(0)
    , m_idCounter(0)
    , m_xmlConsumer(0)
{
    if (url.isEmpty()) {
        openNew();
    } else {
        openFile();
    }
    m_xmlConsumer = new Mlt::Consumer(*(profile()), "xml:kdenlive_clip");
    m_undoStack = new QUndoStack(this);
}

Project::Project(QObject* parent) :
    QObject(parent)
  , m_idCounter(0)
  , m_xmlConsumer(0)
{
    openNew();

    m_xmlConsumer = new Mlt::Consumer(*(profile()), "xml:kdenlive_clip");
    m_undoStack = new QUndoStack(this);
}

Project::~Project()
{
    delete  m_xmlConsumer;
}

KUrl Project::url() const
{
    return m_url;
}

double Project::displayRatio() const
{
    return profile()->dar();
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

Mlt::Profile* Project::profile() const
{
    return m_timeline->profile();
}

MonitorView* Project::binMonitor()
{
    return m_bin->monitor();
}

MonitorView* Project::timelineMonitor()
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
            QDomElement kdenliveDoc = document.documentElement().firstChildElement("kdenlivedoc");
            // Make some compatibility checks
            if (!upgradeDocument(kdenliveDoc)) {
                KMessageBox::sorry(kapp->activeWindow(), i18n("Failed to open project %1").arg(m_url.path()));
                openNew();
                return;
            }
            // Remove the kdenlivedoc info before passing playlist to MLT
            document.documentElement().removeChild(kdenliveDoc);
            loadTimeline(document.toString());
            if (kdenliveDoc.isNull())
                kdenliveDoc = convertMltPlaylist(document);
            updateClipCounter(kdenliveDoc.elementsByTagName("clip"));
            m_projectFolder = KUrl(kdenliveDoc.attribute("projectfolder"));
            m_bin = new BinModel(kdenliveDoc.firstChildElement("bin"), this);
            loadParts(kdenliveDoc);
            loadSettings(kdenliveDoc);
            m_timeline->loadTracks();
        } else {
            kWarning() << "unable to load document" << m_url.path() << errorMessage;
        }
    } else {
        kWarning() << "not able to access " << m_url.path();
    }
}

bool Project::upgradeDocument(QDomElement &kdenliveDoc)
{
    if (kdenliveDoc.isNull()) {
        // Probably an MLT playlist, allow direct opening
        return true;
    }
    double version = kdenliveDoc.attribute("version").toDouble();
    if (version < 0.90) return false;
    return true;
}


QDomElement Project::convertMltPlaylist(QDomDocument &document)
{
    // Project has no kdenlive info, might be an MLT playlist, create bin clips
    QDomElement kdenliveDoc = document.createElement("kdenlivedoc");
    document.documentElement().appendChild(kdenliveDoc);
    QDomNodeList producers = document.documentElement().elementsByTagName("producer");
    QDomElement binxml = document.createElement("bin");
    QDomElement folder = document.createElement("folder");
    kdenliveDoc.appendChild(binxml);
    binxml.appendChild(folder);
    folder.setAttribute("name", "loading");
    QString root = document.documentElement().attribute("root");
    root.append('/');
    for (int i = 0; i < producers.count(); ++i) {
        QDomElement clip = producers.at(i).cloneNode(true).toElement();
        clip.setTagName("clip");
        QString service = AbstractProjectClip::getXmlProperty(clip, "mlt_service");
        clip.setAttribute("producer_type", service);
        QString resource = AbstractProjectClip::getXmlProperty(clip, "resource");
        if (service != "color" && !resource.startsWith('/')) {
            // append root
            resource.prepend(root);
        }
        KUrl url(resource);
        clip.setAttribute("url", url.path());
        QString clipName = url.fileName();
        if (clipName == "<producer>")
            clipName = service;
        clip.setAttribute("name", clipName);
        folder.appendChild(clip);
    }
    return kdenliveDoc;
}


void Project::openNew()
{
    m_timeline = new Timeline(QString(), this);
    m_timecodeFormatter = new TimecodeFormatter(Fraction(profile()->frame_rate_num(), profile()->frame_rate_den()), TimecodeFormatter::DefaultFormat, this);
    m_bin = new BinModel(this);
    m_projectFolder = KdenliveSettings::defaultprojectfolder();
    loadParts();
}

void Project::loadTimeline(const QString& content)
{
    m_timeline = new Timeline(content, this);
    m_timecodeFormatter = new TimecodeFormatter(Fraction(profile()->frame_rate_num(), profile()->frame_rate_den()), TimecodeFormatter::DefaultFormat, this);
}

void Project::loadParts(const QDomElement& element)
{
    QList<AbstractProjectPart*> parts = pCore->projectManager()->parts();
    foreach (AbstractProjectPart* part, parts) {
        part->init();
        if (!element.isNull()) {
            part->load(element.firstChildElement(part->name()));
        }
    }
}

MonitorManager *Project::monitorManager()
{
    return pCore->monitorManager();
}

QList<QDomElement> Project::saveParts(QDomDocument& document) const
{
    QList<QDomElement> partElements;
    QList<AbstractProjectPart*> parts = pCore->projectManager()->parts();
    for (int i = 0; i < parts.count(); ++i) {
        QDomElement partElement = document.createElement(parts.at(i)->name());
        parts.at(i)->save(document, partElement);
        partElements.append(partElement);
    }

    return partElements;
}

void Project::loadSettings(const QDomElement& kdenliveDoc)
{
    QDomNamedNodeMap attributes = kdenliveDoc.firstChildElement("settings").attributes();
    for (uint i = 0; i < attributes.length(); ++i) {
        QDomNode attribute = attributes.item(i);
        m_settings.insert(attribute.nodeName(), attribute.nodeValue());
    }
}

QDomElement Project::saveSettings(QDomDocument& document) const
{
    QDomElement settings = document.createElement("settings");

    QHash<QString, QString>::const_iterator i = m_settings.constBegin();
    while (i != m_settings.constEnd()) {
        settings.setAttribute(i.key(), i.value());
        ++i;
    }
    
    return settings;
}

QDomDocument Project::toXml() const
{
    QDomDocument document;

    document.setContent(m_timeline->toXml(), false);

    QDomElement kdenliveDoc = document.createElement("kdenlivedoc");
    kdenliveDoc.setAttribute("version", "0.90");
    kdenliveDoc.appendChild(m_bin->toXml(document));
    kdenliveDoc.appendChild(saveSettings(document));

    QList<QDomElement> parts = saveParts(document);
    foreach (QDomElement part, parts) {
        kdenliveDoc.appendChild(part);
    }

    document.documentElement().appendChild(kdenliveDoc);

    return document;
}

void Project::save()
{
    if (m_url.isEmpty()) {
        saveAs();
    } else {
        QFile file(m_url.path());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            // TODO: warning
            return /*false*/;
        }
        file.write(toXml().toString().toUtf8());
        file.close();
    }
}

void Project::saveAs()
{
    QString outputFile = KFileDialog::getSaveFileName(/*m_activeDocument->projectFolder(), getMimeType(false)*/);

    if (!outputFile.isEmpty()) {
        if (QFile::exists(outputFile) &&
                KMessageBox::questionYesNo(pCore->window(), i18n("File %1 already exists.\nDo you want to overwrite it?", outputFile)) == KMessageBox::No) {
            // Show the file dialog again if the user does not want to overwrite the file
            saveAs();
        } else {
            m_url = KUrl(outputFile);
            pCore->window()->setCaption(caption());
            save();
        }
    }
}

const KUrl &Project::projectFolder() const
{
    return m_projectFolder;
}

QString Project::getFreeId()
{
    return QString::number(m_idCounter++);
}

void Project::updateClipCounter(const QDomNodeList clips)
{
    for (int i = 0; i < clips.count(); ++i) {
        bool ok;
        int id = clips.at(i).toElement().attribute("id").toInt(&ok);
        if (ok && id >= m_idCounter)
            m_idCounter = id + 1;
    }
}

Mlt::Consumer *Project::xmlConsumer()
{
    return m_xmlConsumer;
}
    
    
#include "project.moc"
