/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef COLORPARAMETERDESCRIPTION_H
#define COLORPARAMETERDESCRIPTION_H

#include "core/effectsystem/abstractparameterdescription.h"
#include <QVariantList>
#include <QColor>
#include <kdemacros.h>


class KDE_EXPORT ColorParameterDescription : public AbstractParameterDescription
{
    Q_OBJECT

public:
    ColorParameterDescription(QObject *, const QVariantList&);
    ~ColorParameterDescription();

    void init(QDomElement parameter, QLocale locale);
    void init(Mlt::Properties &properties, QLocale locale);

    AbstractParameter *createParameter(AbstractParameterList *parent, const QString &value = QString()) const;

    QColor defaultValue() const;
    QString prefix() const;
    bool supportsAlpha() const;

protected:
    QColor m_default;
    QString m_prefix;
    bool m_supportsAlpha;
};

#endif
