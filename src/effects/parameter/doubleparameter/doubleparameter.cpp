/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "doubleparameter.h"
#include "doubleparametereffectstackitem.h"
#include "core/effectsystem/abstractparameterlist.h"
#include "core/effectsystem/multiviewhandler.h"
#include <QLocale>
#include <QWidget>


DoubleParameter::DoubleParameter(const DoubleParameterDescription *parameterDescription, AbstractParameterList* parent) :
    AbstractParameter(parameterDescription, parent),
    m_description(parameterDescription)
{
    set(m_description->getDefault());
}

void DoubleParameter::set(const char* data)
{
    QLocale locale;
    double value = locale.toDouble(QString(data));
    value = m_description->getOffset() + value * m_description->getFactor();
    emit valueUpdated(value);
}

void DoubleParameter::set(double value)
{
    QLocale locale;

    emit valueUpdated(value);

    value = (value + m_description->getMin() - m_description->getOffset()) / m_description->getFactor();
    m_parent->setParameterValue(name(), locale.toString(value));
}

double DoubleParameter::getValue() const
{
    QLocale locale;
    return locale.toDouble(m_parent->parameterValue(name()));
}

void DoubleParameter::checkPropertiesViewState()
{
    bool exists = m_viewHandler->hasView(EffectPropertiesView);
    // when the effect or parametergroup (=parent) has a widget than we want a parameter widget in all cases
    bool shouldExist = m_viewHandler->parentView(EffectPropertiesView) ? true : false;
    if (shouldExist != exists) {
        if (shouldExist) {
            DoubleParameterEffectStackItem *effectStackItem = new DoubleParameterEffectStackItem(m_description->displayName(),
                                                                                                 getValue(),
                                                                                                 m_description->getMin(),
                                                                                                 m_description->getMax(),
                                                                                                 m_description->comment(),
                                                                                                 -1,
                                                                                                 m_description->getSuffix(),
                                                                                                 m_description->getDecimals(),
                                                                                                 static_cast<QWidget*>(m_viewHandler->parentView(EffectPropertiesView))
                                                                                                );
            connect(this, SIGNAL(valueUpdated(double)), effectStackItem, SLOT(setValue(double)));
            connect(effectStackItem, SIGNAL(valueChanged(double)), this, SLOT(set(double)));
            m_viewHandler->setView(EffectPropertiesView, effectStackItem);
        } else {
            m_viewHandler->deleteView(EffectPropertiesView);
        }
    }
}

#include "doubleparameter.moc"
