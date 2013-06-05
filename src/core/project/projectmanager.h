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


/**
 * @class ProjectManager
 * @brief Takes care of interaction with projects.
 */


class KDE_EXPORT ProjectManager : public QObject
{
    Q_OBJECT

public:
    /** @brief Sets up actions to interact for project interaction (undo, redo, open, save, ...) and creates an empty project. */
    explicit ProjectManager(QObject* parent = 0);
    virtual ~ProjectManager();

    /** @brief Returns a pointer to the currently opened project. At any time a project should be open. */
    Project *current();

    /**
     * @brief Opens a project file.
     * @param url url of the project file
     * 
     * emits projectOpened
     */
    void openProject(const KUrl &url);

    /** @brief Adds a project part to the parts list which is used by projects. */
    void registerPart(AbstractProjectPart *part);
    /** @brief Returns the registered project parts. */
    QList<AbstractProjectPart*> parts();

public slots:
    /** @brief Allows to open a project through the open file dialog. */
    void execOpenFileDialog();
    /** @brief Saves the current project to disk. */
    void saveProject();
    /** @brief Saves the current project to disk allowing the user to choose a new filename. */
    void saveProjectAs();
    /** @brief Calls undo on the command stack of the current project. */
    void undoCommand();
    /** @brief Calls redo on the command stack of the current project. */
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
