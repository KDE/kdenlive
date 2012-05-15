/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "booleanparameter.h"
#include "booleanpropertiesview.h"
#include "core/effectsystem/abstractparameterlist.h"
#include "core/effectsystem/multiviewhandler.h"
#include <QLocale>
#include <QWidget>


BooleanParameter::BooleanParameter(const BooleanParameterDescription *parameterDescription, AbstractParameterList* parent) :
    AbstractParameter(parameterDescription, parent),
    m_description(parameterDescription)
{
    set(m_description->defaultValue());
}

void BooleanParameter::set(const char* data)
{
    bool value = QString(data).toInt();
    emit valueUpdated(value);
}

void BooleanParameter::set(bool value)
{
    emit valueUpdated(value);
    m_parent->setParameterValue(name(), QString::number(value));
}

bool BooleanParameter::value() const
{
    return m_parent->parameterValue(name()).toInt();
}

void BooleanParameter::checkPropertiesViewState()
{
    bool exists = m_viewHandler->hasView(MultiViewHandler::propertiesView);
    // when the effect or parametergroup (=parent) has a widget than we want a parameter widget in all cases
    bool shouldExist = m_viewHandler->parentView(MultiViewHandler::propertiesView) ? true : false;
    if (shouldExist != exists) {
        if (shouldExist) {
            BooleanPropertiesView *view = new BooleanPropertiesView(m_description->displayName(),
                                                                    value(),
                                                                    m_description->comment(),
                                                                    static_cast<QWidget*>(m_viewHandler->parentView(MultiViewHandler::propertiesView)));
            connect(this, SIGNAL(valueUpdated(bool)), view, SLOT(setValue(bool)));
            connect(view, SIGNAL(valueChanged(bool)), this, SLOT(set(bool)));
            m_viewHandler->setView(MultiViewHandler::propertiesView, view);
        } else {
            m_viewHandler->deleteView(MultiViewHandler::propertiesView);
        }
    }
}

#include "booleanparameter.moc"
