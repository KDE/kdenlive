/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "multiuihandler.h"


MultiUiHandler::MultiUiHandler(MultiUiHandler* parent) :
    QMap< int, QObject* >(),
    m_parent(parent)
{
    if (m_parent) {
        connect(m_parent, SIGNAL(createUi(EffectUiTypes, QObject*)), this, SIGNAL(createUi(EffectUiTypes, QObject*)));
    }
}

void MultiUiHandler::addUi(const EffectUiTypes type, QObject* ui)
{
    insert(static_cast<int>(type), ui);
}

