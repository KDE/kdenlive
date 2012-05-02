 /*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#ifndef PARAMETERTYPES_H
#define PARAMETERTYPES_H

#include "abstractparameter.h"

#include "doubleparameter.h"

#include <QtCore>
#include <QDomElement>
/*
typedef QHash<QString, AbstractParameter *(*)(QDomElement, AbstractParameterList*)> parameterConstructionHash;
parameterConstructionHash parameterTypes;

parameterConstructionHash *getParameterTypes()
{
    if (parameterTypes.isEmpty()) {
//         parameterTypes["double"] = init_DoubleParameter;
//         parameterTypes["constant"] = init_DoubleParameter;
    }
    return &parameterTypes;
}*/


#endif
