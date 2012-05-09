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
    m_abstractDescription(static_cast<const AbstractParameterDescription*>(parameterDescription)),
    m_parent(parent)
{
    m_uiHandler = new MultiUiHandler(parent->getUiHandler());
    connect(m_uiHandler, SIGNAL(createUi(EffectUiTypes, QObject*)), this, SLOT(createUi(EffectUiTypes, QObject*)));
}

AbstractParameter::~AbstractParameter()
{
    delete m_uiHandler;
}

MultiUiHandler *AbstractParameter::getMultiUiHandler()
{
    return m_uiHandler;
}

QString AbstractParameter::getName() const
{
    return m_abstractDescription->getName();
}

const AbstractParameterDescription* AbstractParameter::getDescription() const
{
    return m_abstractDescription;
}

#include "abstractparameter.moc"
