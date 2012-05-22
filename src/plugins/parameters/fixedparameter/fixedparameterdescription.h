/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef FIXEDPARAMETERDESCRIPTION_H
#define FIXEDPARAMETERDESCRIPTION_H

#include "core/effectsystem/abstractparameterdescription.h"
#include <QVariantList>
#include <kdemacros.h>


class KDE_EXPORT FixedParameterDescription : public AbstractParameterDescription
{
    Q_OBJECT

public:
    FixedParameterDescription(QObject *, const QVariantList&);
    ~FixedParameterDescription();

    void init(QDomElement parameter, QLocale locale);
    void init(Mlt::Properties &properties, QLocale locale);

    AbstractParameter *createParameter(AbstractParameterList *parent) const;

    QString value() const;

private:
    QString m_value;
};

#endif
