/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "abstractviewcontainer.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <KIcon>
#include <KLocale>


AbstractViewContainer::AbstractViewContainer(QWidget* parent) :
    QWidget(parent)
{
}

AbstractViewContainer::~AbstractViewContainer()
{
}

#include "abstractviewcontainer.moc"
