/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#ifndef DOUBLEPARAMETER_H
#define DOUBLEPARAMETER_H

#include "core/effectsystem/abstractparameter.h"
#include "doubleparameterdescription.h"



class DoubleParameter : public AbstractParameter
{
    Q_OBJECT

public:
    DoubleParameter(const DoubleParameterDescription *parameterDescription, AbstractParameterList *parent);
    ~DoubleParameter() {};

    void set(const char*data);
    double value() const;

public slots:
    void set(double value);

    void checkPropertiesViewState();

protected:
    void resetValue();

private:
    const DoubleParameterDescription *m_description;

signals:
    void valueUpdated(double value);
};

#endif
