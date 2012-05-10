/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "multiviewhandler.h"


MultiViewHandler::MultiViewHandler(MultiViewHandler* parent) :
    m_parent(parent)
{
}

void MultiViewHandler::setView(const EffectViewTypes type, QObject* ui)
{
    // delete previous UI first?
    insert(static_cast<int>(type), ui);
}

QObject* MultiViewHandler::getView(EffectViewTypes type)
{
    if (contains(type)) {
        return value(type);
    }
    return NULL;
}

bool MultiViewHandler::hasView(EffectViewTypes type) const
{
    return contains(type);
}

void MultiViewHandler::deleteView(EffectViewTypes type)
{
    if (contains(type)) {
        QObject *view = value(type);
        if (view) {
            delete view;
        }
        remove(type);
    }
}

QObject* MultiViewHandler::popView(const EffectViewTypes type)
{
    if (contains(type)) {
        return take(type);
    }
    return NULL;
}

QObject* MultiViewHandler::getParentView(EffectViewTypes type)
{
    if (m_parent) {
        return m_parent->getView(type);
    }
    return NULL;
}


#include "multiviewhandler.moc"
