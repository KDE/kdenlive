/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "abstractprojectpart.h"
#include "core.h"
#include "projectmanager.h"


AbstractProjectPart::AbstractProjectPart(const QString& name, QObject* parent) :
    QObject(parent),
    m_name(name)
{
    pCore->projectManager()->registerPart(this);
}

QString AbstractProjectPart::name() const
{
    return m_name;
}

void AbstractProjectPart::setModified()
{
    emit modified();
}

#include "abstractprojectpart.moc"
