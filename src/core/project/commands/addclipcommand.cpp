/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "addclipcommand.h"
#include "project/abstractprojectclip.h"
#include "project/projectfolder.h"
#include "project/abstractclipplugin.h"
#include <KLocalizedString>

#include <KDebug>

AddClipCommand::AddClipCommand(const KUrl &url, const QString &id, const AbstractClipPlugin *plugin, ProjectFolder *parentItem, bool addClip, QUndoCommand* parent) :
    QUndoCommand(parent)
    , m_url(url)
    , m_id(id)
    , m_plugin(plugin)
    , m_clip(NULL)
    , m_parentItem(parentItem)
    , m_addClip(addClip)
{
    setText(i18n("Add clip"));
}

AddClipCommand::AddClipCommand(const QString &displayName, const QString &service, Mlt::Properties &properties, const QString &id, const AbstractClipPlugin *plugin, ProjectFolder *parentItem, bool addClip, QUndoCommand* parent) :
    QUndoCommand(parent)
    , m_displayName(displayName)
    , m_service(service)
    , m_properties(properties)
    , m_id(id)
    , m_plugin(plugin)
    , m_clip(NULL)
    , m_parentItem(parentItem)
    , m_addClip(addClip)
{
    setText(i18n("Add clip"));
}

void AddClipCommand::undo()
{
    if (m_addClip)
        deleteClip();
    else
        addClip();
}

void AddClipCommand::redo()
{
    if (m_addClip)
        addClip();
    else
        deleteClip();
}

void AddClipCommand::addClip()
{
    if (!m_service.isEmpty()) {
        m_clip = m_plugin->createClip(m_service, m_properties, m_id, m_parentItem);
        m_clip->setName(m_displayName);
    }
    else 
        m_clip = m_plugin->createClip(m_url, m_id, m_parentItem);
    m_clip->finishInsert(m_parentItem);
}

void AddClipCommand::deleteClip()
{
    if (!m_clip) {
        m_clip = m_parentItem->clip(m_id);
    }
    m_clip->setParent(NULL);
    delete m_clip;
    m_clip = NULL;
}

