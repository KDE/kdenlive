/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "colorparameter.h"
#include "core/effectsystem/abstractparameterlist.h"
#include "colorpropertiesview.h"
#include <QColor>
#include <QTextStream>

ColorParameter::ColorParameter(const ColorParameterDescription* parameterDescription, AbstractParameterList* parent, const QString &value) :
    AbstractParameter(parameterDescription, parent),
    m_description(parameterDescription)
{
    if (value.isEmpty()) set(m_description->defaultValue());
}

void ColorParameter::set(const char* data)
{
    QString string = QString(data).remove(m_description->prefix());
    QColor value = stringToColor(string);
    emit valueUpdated(value);
}

void ColorParameter::set(const QColor &value)
{
    emit valueUpdated(value);

    QString string = colorToString(value, m_description->supportsAlpha()).prepend(m_description->prefix());
    m_parent->setParameterValue(name(), string);
}

QColor ColorParameter::value() const
{
    return stringToColor(m_parent->parameterValue(name()).remove(m_description->prefix()));
}

QColor ColorParameter::stringToColor(QString string)
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

QString ColorParameter::colorToString(const QColor &color, bool hasAlpha)
{
    QString colorStr;
    QTextStream stream(&colorStr);
    stream << "0x";
    stream.setIntegerBase(16);
    stream.setFieldWidth(2);
    stream.setFieldAlignment(QTextStream::AlignRight);
    stream.setPadChar('0');
    stream <<  color.red() << color.green() << color.blue();
    //if(hasAlpha) {
        stream << color.alpha();
    //}
    return colorStr;
}

void ColorParameter::checkPropertiesViewState()
{
    bool exists = m_viewHandler->hasView(MultiViewHandler::propertiesView);
    // when the effect or parametergroup (=parent) has a widget than we want a parameter widget in all cases
    bool shouldExist = m_viewHandler->parentView(MultiViewHandler::propertiesView) ? true : false;
    if (shouldExist != exists) {
        if (shouldExist) {
            ColorPropertiesView *view = new ColorPropertiesView(m_description->displayName(),
                                                                value(),
                                                                m_description->supportsAlpha(),
                                                                static_cast<QWidget*>(m_viewHandler->parentView(MultiViewHandler::propertiesView))
                                                                );
            connect(this, SIGNAL(valueUpdated(const QColor&)), view, SLOT(setValue(const QColor &)));
            connect(view, SIGNAL(valueChanged(const QColor&)), this, SLOT(set(const QColor&)));
            m_viewHandler->setView(MultiViewHandler::propertiesView, view);
        } else {
            m_viewHandler->deleteView(MultiViewHandler::propertiesView);
        }
    }
}

void ColorParameter::resetValue()
{
    set(m_description->defaultValue());
}

#include "colorparameter.moc"
