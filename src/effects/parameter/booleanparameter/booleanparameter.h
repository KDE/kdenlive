/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#ifndef BOOLEANPARAMETER_H
#define BOOLEANPARAMETER_H

#include "core/effectsystem/abstractparameter.h"
#include "booleanparameterdescription.h"



class BooleanParameter : public AbstractParameter
{
    Q_OBJECT

public:
    BooleanParameter(const BooleanParameterDescription *parameterDescription, AbstractParameterList *parent);
    ~BooleanParameter() {};

    void set(const char*data);
    bool value() const;

public slots:
    void set(bool value);

    void checkPropertiesViewState();

private:
    const BooleanParameterDescription *m_description;

signals:
    void valueUpdated(bool value);
};

#endif
