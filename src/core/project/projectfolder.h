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

class ClipPluginManager;


class ProjectFolder : public AbstractProjectItem
{
    Q_OBJECT

public:
    ProjectFolder(const QDomElement &description, ClipPluginManager *clipPluginManager, AbstractProjectItem* parent = 0);
    ~ProjectFolder();

    AbstractProjectClip *clip(int id);
};

#endif
