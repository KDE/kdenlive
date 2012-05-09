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
#include "abstractparameterdescription.h"
#include "multiuihandler.h"
#include <KDebug>


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
        AbstractParameter *parameter = parameterDescription->createParameter(this);
        if (parameter) {
            append(parameter);
        } else {
            kWarning() << "Parameter " << parameterDescription->getName() << " could not be created";
            delete parameter;
        }
    }
}

MultiUiHandler* AbstractParameterList::getUiHandler()
{
    return m_uiHandler;
}

#include "abstractparameterlist.moc"
