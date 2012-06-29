/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef MONITORMANAGER_H
#define MONITORMANAGER_H

#include <QObject>
#include <QHash>

class MonitorView;
class MonitorModel;
class Project;
class QSignalMapper;


/**
 * @class MonitorManager
 * @brief Takes care of assigning the monitor models to views.
 */


class MonitorManager : public QObject
{
    Q_OBJECT

public:
    /** @brief Creates a "auto" monitor view. */
    explicit MonitorManager(QObject* parent = 0);

    /**
     * @brief Registers a model and creates a view for it is necessary.
     * @param id indentifier for model and view
     * @param model pointer to model
     * @param needsOwnView @c false if a view with id @param id exists it is used, otherwise on activation
     *                              the model is assigned to the auto view
     *                     @c true if no view with id @param id exists one is created.
     * 
     * The two default project models (bin, timeline) do not need to be added manually, they are registerd
     * internally once a project is openend.
     */
    void registerModel(const QString &id, MonitorModel *model, bool needsOwnView = false);

private slots:
    void setProject(Project *project);
    void onModelActivated(const QString &id);

private:
    QHash<QString, MonitorView*> m_views;
    QHash<QString, MonitorModel*> m_models;
    MonitorModel *m_active;
    QSignalMapper *m_modelSignalMapper;
};

#endif
