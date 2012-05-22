/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "doubleparameter.h"
#include "doublepropertiesview.h"
#include "core/effectsystem/abstractparameterlist.h"
#include "core/effectsystem/multiviewhandler.h"
#include <QLocale>
#include <QWidget>


DoubleParameter::DoubleParameter(const DoubleParameterDescription *parameterDescription, AbstractParameterList* parent) :
    AbstractParameter(parameterDescription, parent),
    m_description(parameterDescription)
{
    set(m_description->defaultValue());
}

void DoubleParameter::set(const char* data)
{
    QLocale locale;
    double value = locale.toDouble(QString(data));
    value = m_description->offset() + value * m_description->factor();
    emit valueUpdated(value);
}

void DoubleParameter::set(double value)
{
    QLocale locale;

    emit valueUpdated(value);

    value = (value + m_description->min() - m_description->offset()) / m_description->factor();
    m_parent->setParameterValue(name(), locale.toString(value));
}

double DoubleParameter::value() const
{
    QLocale locale;
    return m_description->offset() + locale.toDouble(m_parent->parameterValue(name())) * m_description->factor();
}

void DoubleParameter::checkPropertiesViewState()
{
    bool exists = m_viewHandler->hasView(MultiViewHandler::propertiesView);
    // when the effect or parametergroup (=parent) has a widget than we want a parameter widget in all cases
    bool shouldExist = m_viewHandler->parentView(MultiViewHandler::propertiesView) ? true : false;
    if (shouldExist != exists) {
        if (shouldExist) {
            DoublePropertiesView *view = new DoublePropertiesView(m_description->displayName(),
                                                                  value(),
                                                                  m_description->min(),
                                                                  m_description->max(),
                                                                  m_description->comment(),
                                                                  -1,
                                                                  m_description->suffix(),
                                                                  m_description->decimals(),
                                                                  static_cast<QWidget*>(m_viewHandler->parentView(MultiViewHandler::propertiesView))
                                                                  );
            connect(this, SIGNAL(valueUpdated(double)), view, SLOT(setValue(double)));
            connect(view, SIGNAL(valueChanged(double)), this, SLOT(set(double)));
            m_viewHandler->setView(MultiViewHandler::propertiesView, view);
        } else {
            m_viewHandler->deleteView(MultiViewHandler::propertiesView);
        }
    }
}

void DoubleParameter::resetValue()
{
    set(m_description->defaultValue());
}

#include "doubleparameter.moc"
