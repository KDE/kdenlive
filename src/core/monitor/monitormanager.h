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
#include <QMap>
#include <kdemacros.h>
#include "mltcontroller.h"

class MonitorView;
class MonitorModel;
class Project;
class QSignalMapper;
class MltController;


enum MONITORID {AutoMonitor, ClipMonitor, ProjectMonitor};

/**
 * @class MonitorManager
 * @brief Takes care of assigning the monitor models to views.
 */


class KDE_EXPORT MonitorManager : public QObject
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
    //void registerModel(MONITORID id, MonitorModel*model, bool needsOwnView = false);
    void requestThumbnails(const QString &id, QList <int> positions);
    void requestActivation(MONITORID id);
    bool isSupported(DISPLAYMODE mode);
    bool isAvailable(DISPLAYMODE mode);
    
    static bool requiresUniqueDisplay(DISPLAYMODE mode);
    
private slots:
    void setProject(Project *project);
    void onModelActivated(MONITORID id);
    void updateController(MONITORID id, MltController *controller);

private:
    QHash<MONITORID, MltController*> m_controllers;
    QHash<MONITORID, MonitorView*> m_monitors;
    MonitorModel *m_active;
    QSignalMapper *m_modelSignalMapper;
    QMap <QString, QList<int> > m_thumbRequests;
};

#endif
