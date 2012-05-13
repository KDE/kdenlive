/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "fixedparameter.h"
#include "core/effectsystem/abstractparameterlist.h"
#include <QLocale>
#include <QWidget>


FixedParameter::FixedParameter(const FixedParameterDescription *parameterDescription, AbstractParameterList* parent) :
    AbstractParameter(parameterDescription, parent),
    m_description(parameterDescription)
{
    m_parent->setParameterValue(name(), m_description->value());
}

void FixedParameter::set(const char* data)
{
    Q_UNUSED(data)
}

QString FixedParameter::getValue() const
{
    return m_parent->parameterValue(name());
}

#include "fixedparameter.moc"
