/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef BINMODEL_H
#define BINMODEL_H

#include <QObject>
#include <kdemacros.h>

class Project;
class MonitorModel;
class ProjectFolder;
class AbstractProjectClip;
class AbstractProjectItem;
class QDomElement;


class KDE_EXPORT BinModel : public QObject
{
    Q_OBJECT

public:
    BinModel(Project *parent = 0);
    BinModel(const QDomElement &rootItem, Project* parent = 0);

    Project *project();
    MonitorModel *monitor();

    ProjectFolder *rootFolder();
    AbstractProjectClip *clip(int id);
    AbstractProjectItem *currentItem();
    void setCurrentItem(AbstractProjectItem *item);

public slots:
    void emitAboutToAddItem(AbstractProjectItem *item);
    void emitItemAdded(AbstractProjectItem *item);
    void emitAboutToRemoveItem(AbstractProjectItem *item);
    void emitItemRemoved(AbstractProjectItem *item);

signals:
    void aboutToAddItem(AbstractProjectItem *item);
    void itemAdded(AbstractProjectItem *item);
    void aboutToRemoveItem(AbstractProjectItem *item);
    void itemRemoved(AbstractProjectItem *item);
    void currentItemChanged(AbstractProjectItem *item);

private:
    Project *m_project;
    MonitorModel *m_monitor;
    ProjectFolder *m_rootFolder;
    AbstractProjectItem *m_currentItem;
};

#endif
