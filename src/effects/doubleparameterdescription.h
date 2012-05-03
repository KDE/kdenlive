/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef DOUBLEPARAMETERDESCRIPTION_H
#define DOUBLEPARAMETERDESCRIPTION_H

#include "abstractparameterdescription.h"


class DoubleParameterDescription : public AbstractParameterDescription
{
public:
    DoubleParameterDescription(QDomElement parameter, QLocale locale);
    DoubleParameterDescription(Mlt::Properties &properties, QLocale locale);
    ~DoubleParameterDescription();

    double getDefault() const;
    double getFactor() const;
    double getOffset() const;
    double getMin() const;
    double getMax() const;
    int getDecimals() const;
    QString getSuffix() const;

protected:
    double m_default;
    double m_factor;
    double m_offset;
    double m_min;
    double m_max;
    int m_decimals;
    QString m_suffix;
};

#endif
