/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "abstracteffectstackitem.h"

#include <QWidget>

AbstractEffectStackItem::AbstractEffectStackItem(AbstractEffectStackItem* parent) :
    m_parent(parent),
    m_ui(NULL)
{
}

AbstractEffectStackItem::~AbstractEffectStackItem()
{
    delete m_ui;
}

QWidget* AbstractEffectStackItem::getWidget()
{
    return m_ui;
}

#include "abstracteffectstackitem.moc"
