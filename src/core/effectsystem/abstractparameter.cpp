/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "abstractparameter.h"
#include "abstractparameterdescription.h"
#include "abstractparameterlist.h"


AbstractParameter::AbstractParameter(const AbstractParameterDescription *parameterDescription, AbstractParameterList* parent) : 
    EffectSystemItem(parent),
    m_description(static_cast<const AbstractParameterDescription*>(parameterDescription)),
    m_parent(parent)
{
    connect(this, SIGNAL(reset()), this, SLOT(resetValue()));
}

AbstractParameter::~AbstractParameter()
{
}


const char* AbstractParameter::get() const
{
    return m_parent->parameterValue(name()).toUtf8().constData();
}

QString AbstractParameter::name() const
{
    return m_description->name();
}

const AbstractParameterDescription* AbstractParameter::description() const
{
    return m_description;
}

void AbstractParameter::checkPropertiesViewState()
{
}

void AbstractParameter::checkTimelineViewState()
{
}

void AbstractParameter::checkMonitorViewState()
{
}

void AbstractParameter::resetValue()
{
}

#include "abstractparameter.moc"
