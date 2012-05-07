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

#include "abstractparameter.h"
#include "doubleparameterdescription.h"
#include "parametertypes.h"



class DoubleParameter : public AbstractParameter
{
    Q_OBJECT

public:
    DoubleParameter(DoubleParameterDescription *parameterDescription, AbstractParameterList *parent);
//     ~DoubleParameter();

    void set(const char*data);
    const char *get() const;
    double getValue() const;

    void createUi(EffectUiTypes type, QObject *parent);

public slots:
    void set(double value, bool update = false);

private:
    DoubleParameterDescription *m_description;

signals:
    void valueUpdated(double value);
};

#endif
