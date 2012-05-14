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

ColorParameter::ColorParameter(const ColorParameterDescription* parameterDescription, AbstractParameterList* parent) :
    AbstractParameter(parameterDescription, parent),
    m_description(parameterDescription)
{
    set(m_description->defaultValue());
}

void ColorParameter::set(const char* data)
{
    QString string = QString(data).remove(m_description->prefix());
    QColor value = m_description->stringToColor(data);
    emit valueUpdated(value);
}

void ColorParameter::set(const QColor &value)
{
    emit valueUpdated(value);

    QString string = m_description->colorToString(value, m_description->supportsAlpha()).prepend(m_description->prefix());
    m_parent->setParameterValue(name(), string);
}

QColor ColorParameter::value() const
{
    return m_description->stringToColor(m_parent->parameterValue(name()).remove(m_description->prefix()));
}

void ColorParameter::checkPropertiesViewState()
{
    bool exists = m_viewHandler->hasView(EffectPropertiesView);
    // when the effect or parametergroup (=parent) has a widget than we want a parameter widget in all cases
    bool shouldExist = m_viewHandler->parentView(EffectPropertiesView) ? true : false;
    if (shouldExist != exists) {
        if (shouldExist) {
            ColorPropertiesView *view = new ColorPropertiesView(m_description->displayName(),
                                                                value(),
                                                                m_description->supportsAlpha(),
                                                                static_cast<QWidget*>(m_viewHandler->parentView(EffectPropertiesView))
                                                                );
            connect(this, SIGNAL(valueUpdated(const QColor&)), view, SLOT(setValue(const QColor &)));
            connect(view, SIGNAL(valueChanged(const QColor&)), this, SLOT(set(const QColor&)));
            m_viewHandler->setView(EffectPropertiesView, view);
        } else {
            m_viewHandler->deleteView(EffectPropertiesView);
        }
    }
}

#include "colorparameter.moc"
