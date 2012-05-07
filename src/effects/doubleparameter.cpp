/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "doubleparameter.h"
#include "abstractparameterlist.h"
#include "doubleparametereffectstackitem.h"
#include <QLocale>


DoubleParameter::DoubleParameter(DoubleParameterDescription *parameterDescription, AbstractParameterList* parent) :
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

void DoubleParameter::set(double value, bool update)
{
    QLocale locale;
    if (update)
        emit valueUpdated(value);
    value = (value + m_description->getMin() - m_description->getOffset()) / m_description->getFactor();
    m_parent->setParameter(getName(), locale.toString(value));
}

const char* DoubleParameter::get() const
{
    return m_parent->getParameter(getName()).toUtf8().constData();
}

double DoubleParameter::getValue() const
{
    return m_parent->getParameter(getName()).toDouble();
}

void DoubleParameter::createUi(EffectUiTypes type, QObject* parent)
{
    DoubleParameterEffectStackItem *effectStackItem;
    switch (type)
    {
        case EffectStackEffectUi:
            effectStackItem = new DoubleParameterEffectStackItem(m_description->getDisplayName(), getValue(), m_description->getMin(), m_description->getMax(), m_description->getComment(), -1, m_description->getSuffix(), m_description->getDecimals(), static_cast<AbstractEffectStackItem*>(parent));
            connect(this, SIGNAL(valueUpdated(double)), effectStackItem, SLOT(setValue(double)));
            connect(effectStackItem, SIGNAL(valueChanged(double)), this, SLOT(set(double)));
            getMultiUiHandler()->addUi(type, static_cast<QObject *>(effectStackItem));
            break;
        case TimelineEffectUi:
            break;
        case MonitorEffectUi:
            break;
    }
}

