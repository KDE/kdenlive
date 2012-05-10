/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "booleanparameter.h"
#include "booleanparametereffectstackitem.h"
#include "core/effectsystem/abstractparameterlist.h"
#include "core/effectsystem/multiviewhandler.h"
#include <QLocale>
#include <QWidget>


BooleanParameter::BooleanParameter(const BooleanParameterDescription *parameterDescription, AbstractParameterList* parent) :
    AbstractParameter(parameterDescription, parent),
    m_description(parameterDescription)
{
    set(m_description->getDefault());
}

void BooleanParameter::set(const char* data)
{
    bool value = QString(data).toInt();
    emit valueUpdated(value);
}

void BooleanParameter::set(bool value)
{
    emit valueUpdated(value);
    m_parent->setParameter(getName(), QString::number(value));
}

bool BooleanParameter::getValue() const
{
    return m_parent->getParameter(getName()).toInt();
}

void BooleanParameter::checkPropertiesViewState()
{
    bool exists = m_viewHandler->hasView(EffectPropertiesView);
    // when the effect or parametergroup (=parent) has a widget than we want a parameter widget in all cases
    bool shouldExist = m_viewHandler->getParentView(EffectPropertiesView) ? true : false;
    if (shouldExist != exists) {
        if (shouldExist) {
            BooleanParameterEffectStackItem *effectStackItem = new BooleanParameterEffectStackItem(m_description->getDisplayName(),
                                                                                                   getValue(),
                                                                                                   m_description->getComment(),
                                                                                                   static_cast<QWidget*>(m_viewHandler->getParentView(EffectPropertiesView)));
            connect(this, SIGNAL(valueUpdated(bool)), effectStackItem, SLOT(setValue(bool)));
            connect(effectStackItem, SIGNAL(valueChanged(bool)), this, SLOT(set(bool)));
            m_viewHandler->setView(EffectPropertiesView, effectStackItem);
        } else {
            m_viewHandler->deleteView(EffectPropertiesView);
        }
    }
}

#include "booleanparameter.moc"
