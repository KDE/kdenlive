/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "fixedparameterdescription.h"
#include "fixedparameter.h"
#include <mlt++/Mlt.h>
#include <QString>
#include <QDomElement>
#include <QLocale>
#include <KPluginFactory>


K_PLUGIN_FACTORY( FixedParameterFactory, registerPlugin<FixedParameterDescription>(); )
K_EXPORT_PLUGIN( FixedParameterFactory( "kdenlivefixedparameter" ) )


FixedParameterDescription::FixedParameterDescription(QObject *, const QVariantList&)
{
}

FixedParameterDescription::~FixedParameterDescription()
{
}


void FixedParameterDescription::init(QDomElement parameter, QLocale locale)
{
    AbstractParameterDescription::init(parameter, locale);

    m_value = parameter.attribute("value");
}

void FixedParameterDescription::init(Mlt::Properties& properties, QLocale locale)
{
    AbstractParameterDescription::init(properties, locale);

    m_value = QString(properties.get("default"));
}

AbstractParameter *FixedParameterDescription::createParameter(AbstractParameterList* parent, const QString &value) const
{
    FixedParameter *parameter = new FixedParameter(this, parent, value);
    return static_cast<AbstractParameter*>(parameter);
}

QString FixedParameterDescription::value() const
{
    return m_value;
}

#include "fixedparameterdescription.moc"
