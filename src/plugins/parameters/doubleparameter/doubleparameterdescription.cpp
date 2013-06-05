/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "doubleparameterdescription.h"
#include "doubleparameter.h"
#include <mlt++/Mlt.h>
#include <QString>
#include <QDomElement>
#include <QLocale>
#include <KDebug>
#include <KPluginFactory>


K_PLUGIN_FACTORY( DoubleParameterFactory, registerPlugin<DoubleParameterDescription>(); )
K_EXPORT_PLUGIN( DoubleParameterFactory( "kdenlivedoubleparameter" ) )


DoubleParameterDescription::DoubleParameterDescription(QObject *, const QVariantList&)
{
}


DoubleParameterDescription::~DoubleParameterDescription()
{
}

void DoubleParameterDescription::init(QDomElement parameter, QLocale locale)
{
    AbstractParameterDescription::init(parameter, locale);

    m_default = locale.toDouble(parameter.attribute("default"));
    m_factor = locale.toDouble(parameter.attribute("factor", QString("1")));
    m_offset = locale.toDouble(parameter.attribute("offset", QString("0")));
    m_min = locale.toDouble(parameter.attribute("min", QString("0")));
    m_max = locale.toDouble(parameter.attribute("max", QString("-1")));
    m_decimals = locale.toInt(parameter.attribute("decimals", QString("0")));
    m_suffix = parameter.attribute("suffix");
}

void DoubleParameterDescription::init(Mlt::Properties& properties, QLocale locale)
{
    AbstractParameterDescription::init(properties, locale);

    m_factor = 1;
    m_offset = 0;
    m_decimals = 0;
    m_suffix = properties.get("unit");
    // Should we use properties.get_double ??
    m_default = locale.toDouble(properties.get("default"));
    m_min = locale.toDouble(properties.get("minimum"));
    m_max = locale.toDouble(properties.get("maximum"));

    if(QString(properties.get("type")) == "float") {
        m_decimals = 3;
    }
}

AbstractParameter *DoubleParameterDescription::createParameter(AbstractParameterList* parent, const QString &value) const
{
    DoubleParameter *parameter = new DoubleParameter(this, parent, value);
    return static_cast<AbstractParameter*>(parameter);
}

double DoubleParameterDescription::defaultValue() const
{
    return m_default;
}

double DoubleParameterDescription::factor() const
{
    return m_factor;
}

double DoubleParameterDescription::offset() const
{
    return m_offset;
}

double DoubleParameterDescription::min() const
{
    return m_min;
}

double DoubleParameterDescription::max() const
{
    return m_max;
}

int DoubleParameterDescription::decimals() const
{
    return m_decimals;
}

QString DoubleParameterDescription::suffix() const
{
    return m_suffix;
}

#include "doubleparameterdescription.moc"
