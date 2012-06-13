/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ADDCLIPCOMMAND_H
#define ADDCLIPCOMMAND_H

#include <QUndoCommand>
#include <KUrl>

class AbstractClipPlugin;
class ProjectFolder;
class AbstractProjectClip;

// WARNING: when the parentItem is recreated between redo and undo calls we will crash
// -> instead of a pointer store a "index"-Tree which allows us to get the current pointer to the parentItem
class AddClipCommand : public QUndoCommand
{
public:
    explicit AddClipCommand(const KUrl &url, AbstractClipPlugin *plugin, ProjectFolder *parentItem, QUndoCommand* parent = 0);

    void undo();
    void redo();

private:
    KUrl m_url;
    AbstractClipPlugin *m_plugin;
    AbstractProjectClip *m_clip;
    ProjectFolder *m_parentItem;
};

#endif
