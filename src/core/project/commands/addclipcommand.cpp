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

AddClipCommand::AddClipCommand(const KUrl &url, AbstractClipPlugin *plugin, ProjectFolder *parentItem, QUndoCommand* parent) :
    QUndoCommand(parent),
    m_url(url),
    m_plugin(plugin),
    m_parentItem(parentItem)
{
    setText(i18n("Add clip"));
}

void AddClipCommand::undo()
{
    delete m_clip;
}

void AddClipCommand::redo()
{
    m_clip = m_plugin->createClip(m_url, m_parentItem);
}
