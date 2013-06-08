/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ABSTRACTVIEWCONTAINER_H
#define ABSTRACTVIEWCONTAINER_H

#include <QWidget>
#include "kdenlivecore_export.h"

class QToolButton;
class QFrame;


class KDENLIVECORE_EXPORT AbstractViewContainer : public QWidget
{
    Q_OBJECT

public:
    explicit AbstractViewContainer(QWidget* parent = 0);
    virtual ~AbstractViewContainer();
    virtual void addChild(QWidget *child) = 0;

};

#endif
