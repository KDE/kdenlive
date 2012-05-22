/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "listparameter.h"
#include "listpropertiesview.h"
#include "core/effectsystem/abstractparameterlist.h"
#include "core/effectsystem/multiviewhandler.h"
#include <QLocale>
#include <QWidget>


ListParameter::ListParameter(const ListParameterDescription *parameterDescription, AbstractParameterList* parent) :
    AbstractParameter(parameterDescription, parent),
    m_description(parameterDescription)
{
    setCurrentIndex(m_description->defaultIndex());
}

void ListParameter::set(const char* data)
{
    QLocale locale;
    QString value = QString(data);
    int index = m_description->items().indexOf(value);
    emit currentIndexUpdated(index);
}

void ListParameter::setCurrentIndex(int index)
{
    QLocale locale;

    emit currentIndexUpdated(index);

    QString value = m_description->items().at(index);
    m_parent->setParameterValue(name(), value);
}

int ListParameter::currentIndex() const
{
    return m_description->items().indexOf(m_parent->parameterValue(name()));
}

void ListParameter::checkPropertiesViewState()
{
    bool exists = m_viewHandler->hasView(MultiViewHandler::propertiesView);
    // when the effect or parametergroup (=parent) has a widget than we want a parameter widget in all cases
    bool shouldExist = m_viewHandler->parentView(MultiViewHandler::propertiesView) ? true : false;
    if (shouldExist != exists) {
        if (shouldExist) {
            ListPropertiesView *view = new ListPropertiesView(m_description->displayName(),
                                                              m_description->displayItems(),
                                                              currentIndex(),
                                                              m_description->comment(),
                                                              static_cast<QWidget*>(m_viewHandler->parentView(MultiViewHandler::propertiesView))
                                                             );
            connect(this, SIGNAL(currentIndexUpdated(int)), view, SLOT(setCurrentIndex(int)));
            connect(view, SIGNAL(currentIndexChanged(int)), this, SLOT(setCurrentIndex(int)));
            m_viewHandler->setView(MultiViewHandler::propertiesView, view);
        } else {
            m_viewHandler->deleteView(MultiViewHandler::propertiesView);
        }
    }
}

void ListParameter::resetValue()
{
    setCurrentIndex(m_description->defaultIndex());
}

#include "listparameter.moc"
