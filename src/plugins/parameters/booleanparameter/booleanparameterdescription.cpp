/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "booleanparameterdescription.h"
#include "booleanparameter.h"
#include <mlt++/Mlt.h>
#include <QString>
#include <QDomElement>
#include <QLocale>
#include <KDebug>
#include <KPluginFactory>


K_PLUGIN_FACTORY( BooleanParameterFactory, registerPlugin<BooleanParameterDescription>(); )
K_EXPORT_PLUGIN( BooleanParameterFactory( "kdenliveboolenparameter" ) )


BooleanParameterDescription::BooleanParameterDescription(QObject *, const QVariantList&)
{
}

void BooleanParameterDescription::init(const QDomElement &parameter, const QLocale &locale)
{
    AbstractParameterDescription::init(parameter, locale);

    m_default = parameter.attribute("default").toInt();
}

void BooleanParameterDescription::init(Mlt::Properties& properties, const QLocale &locale)
{
    AbstractParameterDescription::init(properties, locale);

    m_default = QString(properties.get("default")).toInt();
}

AbstractParameter *BooleanParameterDescription::createParameter(AbstractParameterList* parent, const QString &value) const
{
    BooleanParameter *parameter = new BooleanParameter(this, parent, value);
    return static_cast<AbstractParameter*>(parameter);
}

bool BooleanParameterDescription::defaultValue() const
{
    return m_default;
}

#include "booleanparameterdescription.moc"
