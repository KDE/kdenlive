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
class QDomDocument;


/**
 * @class BinModel
 * @brief Provides some functionality on top of the root folder for easy access from both the project and the item model.
 */


class KDE_EXPORT BinModel : public QObject
{
    Q_OBJECT

public:
    /** 
     * @brief Creates an empty root folder and a monitor model.
     * @param parent project this bin belongs to
     */
    BinModel(Project *parent = 0);
    /**
     * @brief Creates monitor model and root folder and passes the supplied information on to it.
     * @param element element describing the bin contents
     * @param parent project this bin belongs to
     * 
     */
    BinModel(const QDomElement &element, Project* parent = 0);

    /** @brief Returns a pointer to the project this bin belongs to. */
    Project *project();
    /** @brief Returns a pointer to the bin's monitor model. */
    MonitorModel *monitor();

    /** @brief Returns a pointer to the root folder. */
    ProjectFolder *rootFolder();
    /** 
     * @brief Returns a pointer to the clip or NULL if not found. 
     * @param id id of the clip which should be searched for
     */
    AbstractProjectClip *clip(int id);
    /** @brief Returns a pointer to the current item or NULL if there is no current item. */
    AbstractProjectItem *currentItem();
    /**
     * @brief Sets a new current item and tells the old one about it.
     * @param item new current item
     * 
     * emits currentItemChanged
     */
    void setCurrentItem(AbstractProjectItem *item);

    QDomElement toXml(QDomDocument &document) const;

public slots:
    /** @brief emits aboutToAddItem. */
    void emitAboutToAddItem(AbstractProjectItem *item);
    /** @brief emits itemAdded. */
    void emitItemAdded(AbstractProjectItem *item);
    /** @brief emits aboutToRemoveItem. */
    void emitAboutToRemoveItem(AbstractProjectItem *item);
    /** @brief emits itemRemoved. */
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
