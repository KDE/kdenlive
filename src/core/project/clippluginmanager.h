/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef CLIPPLUGINMANAGER_H
#define CLIPPLUGINMANAGER_H

#include <QObject>
#include <QHash>
#include <QStringList>
#include <kdemacros.h>

class AbstractClipPlugin;
class AbstractProjectClip;
class AbstractProjectItem;
class ProjectFolder;
class KUrl;
class QDomElement;
class QUndoCommand;


class KDE_EXPORT ClipPluginManager : public QObject
{
    Q_OBJECT

public:
    explicit ClipPluginManager(QObject* parent = 0);
    ~ClipPluginManager();

    void createClip(const KUrl &url, ProjectFolder *folder, QUndoCommand *parentCommand = 0) const;
    AbstractProjectClip *loadClip(const QDomElement &clipDescription, ProjectFolder *folder) const;

    void addSupportedMimetypes(const QStringList &mimetypes);

    AbstractClipPlugin *clipPlugin(const QString &producerType) const;

public slots:
    void execAddClipDialog(ProjectFolder *folder = 0) const;

private:
    QList<AbstractClipPlugin *> m_clipPlugins;
    QHash<QString, AbstractClipPlugin*> m_clipPluginsForProducers;
    QStringList m_supportedFileExtensions;
};

#endif
