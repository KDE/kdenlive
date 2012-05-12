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
#include <KDebug>


AbstractParameterList::AbstractParameterList(EffectSystemItem *parent) :
    EffectSystemItem(parent),
    QList<AbstractParameter* >()
{
}

void AbstractParameterList::createParameters(const QList<AbstractParameterDescription *> &parameters)
{
    foreach(AbstractParameterDescription *parameterDescription, parameters) {
        AbstractParameter *parameter = parameterDescription->createParameter(this);
        if (parameter) {
            append(parameter);
        } else {
            kWarning() << "Parameter " << parameterDescription->name() << " could not be created";
            delete parameter;
        }
    }
}

#include "abstractparameterlist.moc"
