 /*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#include "abstractparameterlist.h"
#include "abstractparameter.h"
#include "multiuihandler.h"


AbstractParameterList::AbstractParameterList(QObject *parent) :
    QList<AbstractParameter* >()
{
    m_uiHandler = new MultiUiHandler();
}

AbstractParameterList::~AbstractParameterList()
{
    delete m_uiHandler;
}

void AbstractParameterList::loadParameters(QList<AbstractParameterDescription *> parameters)
{
    foreach(AbstractParameterDescription *parameterDescription, parameters) {
        /*ParameterType type = parameterDescription->getType();
        AbstractParameter *parameter;
        if (type == DoubleParameterType) {
            parameter = new DoubleParameter(static_cast<DoubleParameterDescription*>(parameterDescription), this);
        }
        if (parameter) {
            append(parameter);
        }*/
    }
}

MultiUiHandler* AbstractParameterList::getUiHandler()
{
    return m_uiHandler;
}

#include "abstractparameterlist.moc"
