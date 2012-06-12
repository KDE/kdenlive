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


class MonitorManager : public QObject
{
    Q_OBJECT

public:
    explicit MonitorManager(QObject* parent = 0);

    void registerModel(const QString &id, MonitorModel *model, bool needsOwnView = false);

public slots:
    void setProject(Project *project);

private slots:
    void onModelActivated(const QString &id);

private:
    QHash<QString, MonitorView*> m_views;
    QHash<QString, MonitorModel*> m_models;
    MonitorModel *m_active;
    QSignalMapper *m_modelSignalMapper;
};

#endif
