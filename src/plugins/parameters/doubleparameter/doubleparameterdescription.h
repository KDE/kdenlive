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

#include "core/effectsystem/abstractparameterdescription.h"
#include <QVariantList>
#include <kdemacros.h>


class KDE_EXPORT DoubleParameterDescription : public AbstractParameterDescription
{
    Q_OBJECT

public:
    DoubleParameterDescription(QObject *, const QVariantList&);
    ~DoubleParameterDescription();

    void init(QDomElement parameter, QLocale locale);
    void init(Mlt::Properties &properties, QLocale locale);

    AbstractParameter *createParameter(AbstractParameterList *parent, const QString &value = QString()) const;

    double defaultValue() const;
    double factor() const;
    double offset() const;
    double min() const;
    double max() const;
    int decimals() const;
    QString suffix() const;

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
