/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef PROJECTFOLDER_H
#define PROJECTFOLDER_H

#include "abstractprojectitem.h"

class BinModel;


/**
 * @class ProjectFolder
 * @brief A folder in the bin.
 */


class ProjectFolder : public AbstractProjectItem
{
    Q_OBJECT

public:
    /**
     * @brief Creates the supplied folder and loads its children.
     * @param description element describing the folder and its children
     * @param parent parent folder
     */
    ProjectFolder(const QDomElement &description, ProjectFolder* parent);
    /**
     * @brief Creates the supplied folder and loads its children. This constructor is used for the root folder.
     * @param description element describing the folder and its children
     * @param bin bin model
     */
    ProjectFolder(const QDomElement &description, BinModel *bin);
    /** @brief Creates an empty folder. */
    ProjectFolder(ProjectFolder *parent);
    /** @brief Creates an empty root folder. */
    ProjectFolder(BinModel *bin);
    ~ProjectFolder();

    /** 
     * @brief Returns the clip if it is a child (also indirect).
     * @param id id of the child which should be returned
     */
    AbstractProjectClip *clip(const QString &id);
    AbstractProjectClip* clipAt(int index);
    /** @brief Returns a pointer to the bin model this folder belongs to. */
    BinModel *bin();

    QDomElement toXml(QDomDocument &document) const;

private:
    void loadChildren(const QDomElement &description);

    BinModel *m_bin;
};

#endif
