/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#ifndef FIXEDPARAMETER_H
#define FIXEDPARAMETER_H

#include "core/effectsystem/abstractparameter.h"
#include "fixedparameterdescription.h"



class FixedParameter : public AbstractParameter
{
    Q_OBJECT

public:
    FixedParameter(const FixedParameterDescription *parameterDescription, AbstractParameterList *parent);
    ~FixedParameter() {};

    void set(const char *data);
    QString value() const;

private:
    const FixedParameterDescription *m_description;
};

#endif
