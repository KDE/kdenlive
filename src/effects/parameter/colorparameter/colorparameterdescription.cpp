/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/


#include "colorparameterdescription.h"
#include "colorparameter.h"
#include <QDomElement>
#include <QTextStream>
#include <KPluginFactory>

K_PLUGIN_FACTORY( ColorParameterFactory, registerPlugin<ColorParameterDescription>(); )
K_EXPORT_PLUGIN( ColorParameterFactory( "kdenlivecolorparameter" ) )


ColorParameterDescription::ColorParameterDescription(QObject* , const QVariantList& )
{
}

ColorParameterDescription::~ColorParameterDescription()
{
}

void ColorParameterDescription::init(QDomElement parameter, QLocale locale)
{
    AbstractParameterDescription::init(parameter, locale);

    m_prefix = parameter.attribute("paramprefix");
    m_default = stringToColor(parameter.attribute("default").remove(m_prefix));
    m_supportsAlpha = parameter.attribute("alpha", "0").toInt();
}

void ColorParameterDescription::init(Mlt::Properties& properties, QLocale locale)
{
    AbstractParameterDescription::init(properties, locale);

    /* tbd */
    m_valid = false;
}

AbstractParameter *ColorParameterDescription::createParameter(AbstractParameterList* parent) const
{
    ColorParameter *parameter = new ColorParameter(this, parent);
    return static_cast<AbstractParameter*>(parameter);
}

QColor ColorParameterDescription::defaultValue() const
{
    return m_default;
}

QString ColorParameterDescription::prefix() const
{
    return m_prefix;
}

bool ColorParameterDescription::supportsAlpha() const
{
    return m_supportsAlpha;
}

QColor ColorParameterDescription::stringToColor(QString string) const
{
    QColor color("black");
    if (string.startsWith("0x")) {
        if (string.length() == 10) {
            // 0xRRGGBBAA
            uint intval = string.toUInt(0, 16);
            color.setRgb( ( intval >> 24 ) & 0xff,   // r
                          ( intval >> 16 ) & 0xff,   // g
                          ( intval >>  8 ) & 0xff,   // b
                          ( intval       ) & 0xff ); // a
        } else {
            // 0xRRGGBB, 0xRGB
            color.setNamedColor(string.replace(0, 2, "#"));
        }
    } else {
        if (string.length() == 9) {
            // #AARRGGBB
            uint intval = string.replace('#', "0x").toUInt(0, 16);
            color.setRgb( ( intval >> 16 ) & 0xff,   // r
                          ( intval >>  8 ) & 0xff,   // g
                          ( intval       ) & 0xff,   // b
                          ( intval >> 24 ) & 0xff ); // a
        } else {
            // #RRGGBB, #RGB
            color.setNamedColor(string);
        }
    }

    return color;
}

QString ColorParameterDescription::colorToString(const QColor &color, bool hasAlpha) const
{
    QString colorStr;
    QTextStream stream(&colorStr);
    stream << "0x";
    stream.setIntegerBase(16);
    stream.setFieldWidth(2);
    stream.setFieldAlignment(QTextStream::AlignRight);
    stream.setPadChar('0');
    stream <<  color.red() << color.green() << color.blue();
    if(hasAlpha) {
        stream << color.alpha();
    }
    return colorStr;
}

#include "colorparameterdescription.moc"
