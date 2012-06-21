/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QObject>
#include <kdemacros.h>

class Project;
class AbstractProjectPart;
class KAction;
class KUrl;


class KDE_EXPORT ProjectManager : public QObject
{
    Q_OBJECT

public:
    explicit ProjectManager(QObject* parent = 0);
    virtual ~ProjectManager();

    Project *current();

    void openProject(const KUrl &url);

    void registerPart(AbstractProjectPart *part);
    QList<AbstractProjectPart*> parts();

public slots:
    void execOpenFileDialog();
    void undoCommand();
    void redoCommand();

signals:
    void projectOpened(Project *project);

private:
    Project *m_project;
    KAction *m_undoAction;
    KAction *m_redoAction;
    QList<AbstractProjectPart*> m_projectParts;
};

#endif
