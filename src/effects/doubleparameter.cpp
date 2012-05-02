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
#include <QDomElement>


// WARNING: locale!
DoubleParameter::DoubleParameter(QDomElement parameterDescription, AbstractParameterList* parent) :
    AbstractParameter(parameterDescription, parent)
{
    m_default  = parameterDescription.attribute("default").toDouble();
    m_min = parameterDescription.attribute("min").toInt();
    m_max = parameterDescription.attribute("max").toInt();
    m_factor = parameterDescription.attribute("factor").toInt();
    m_offset = parameterDescription.attribute("offset").toInt();

    double value;
    if (parameterDescription.hasAttribute("value"))
        value = parameterDescription.attribute("value").toDouble();
    else
        value = m_default;
    set(value);
}

void DoubleParameter::set(const char* data)
{
    double value = QString::fromUtf8(data).toDouble();
    value = m_offset + value * m_factor;
    emit valueUpdated(value);
}

void DoubleParameter::set(double value, bool update)
{
    if (update)
        emit valueUpdated(value);
    value = (value + m_min - m_offset) / m_factor;
    m_parent->setParameter(m_name, QString::number(value));
}

const char* DoubleParameter::get() const
{
    return m_parent->getParameter(m_name).toUtf8().constData();
}

double DoubleParameter::getValue() const
{
    return m_parent->getParameter(m_name).toDouble();
}

void DoubleParameter::createUi(EffectUiTypes type, QObject* parent)
{
    DoubleParameterEffectStackItem *effectStackItem;
    switch (type)
    {
        case EffectStackEffectUi:
            effectStackItem = new DoubleParameterEffectStackItem(m_name, getValue(), m_min, m_max, QString(), -1, QString(), 0, static_cast<AbstractEffectStackItem*>(parent));
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

