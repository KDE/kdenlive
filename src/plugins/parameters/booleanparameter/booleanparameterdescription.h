/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef BOOLEANPARAMETERDESCRIPTION_H
#define BOOLEANPARAMETERDESCRIPTION_H

#include "core/effectsystem/abstractparameterdescription.h"
#include <QVariantList>
#include <kdemacros.h>


class KDE_EXPORT BooleanParameterDescription : public AbstractParameterDescription
{
    Q_OBJECT

public:
    BooleanParameterDescription(QObject *, const QVariantList&);
    ~BooleanParameterDescription() {}

    void init(const QDomElement &parameter, const QLocale &locale);
    void init(Mlt::Properties &properties, const QLocale &locale);

    AbstractParameter *createParameter(AbstractParameterList *parent, const QString &value = QString()) const;

    bool defaultValue() const;

private:
    bool m_default;
};

#endif
