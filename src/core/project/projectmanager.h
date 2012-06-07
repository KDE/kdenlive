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

class Project;
class KUrl;


class ProjectManager : public QObject
{
    Q_OBJECT

public:
    explicit ProjectManager(QObject* parent = 0);
    virtual ~ProjectManager();

    Project *current();

    void openProject(const KUrl &url);

public slots:
    void execOpenFileDialog();

signals:
    void projectOpened(Project *project);

private:
    Project *m_project;
};

#endif
