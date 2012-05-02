 /*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#include "abstractparameterlist.h"
#include "parametertypes.h"
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

void AbstractParameterList::loadParameters(QDomNodeList parameters)
{
    for (int i = 0; i < parameters.count(); ++i) {
        QDomElement parameterDescription = parameters.at(i).toElement();
        QString type = parameterDescription.attribute("type");
        AbstractParameter *parameter;
        if (type == "double" || type == "constant") {
            parameter = new DoubleParameter(parameterDescription, this);
        }
        append(parameter);
    }
}

MultiUiHandler* AbstractParameterList::getUiHandler()
{
    return m_uiHandler;
}

