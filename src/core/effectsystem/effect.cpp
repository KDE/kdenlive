/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/


#include "effect.h"
#include "abstracteffectlist.h"
#include "effectdescription.h"
#include "effectpropertiesview.h"
#include "multiviewhandler.h"
#include <mlt++/Mlt.h>
#include <QWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <KDebug>


Effect::Effect(EffectDescription *effectDescription, AbstractEffectList* parent) :
    AbstractParameterList(parent),
    m_description(effectDescription)
{
    m_filter = new Mlt::Filter(*parent->service().profile(), effectDescription->tag().toUtf8().constData());
    setProperty("kdenlive_id", m_description->getId());

    // TODO: do this properly
    parent->appendFilter(m_filter);

    createParameters(effectDescription->parameters());
}

Effect::~Effect()
{
    // ?
    delete m_filter;
}

void Effect::setParameterValue(const QString &name, const QString &value)
{
    setProperty(name, value);
}

QString Effect::parameterValue(const QString &name) const
{
    return property(name);
}

void Effect::setProperty(const QString &name, const QString &value)
{
    m_filter->set(name.toUtf8().constData(), value.toUtf8().constData());
}

QString Effect::property(const QString &name) const
{
    return QString(m_filter->get(name.toUtf8().constData()));
}

void Effect::checkPropertiesViewState()
{
    bool exists = m_viewHandler->hasView(MultiViewHandler::propertiesView);
    bool shouldExist = m_viewHandler->parentView(MultiViewHandler::propertiesView) ? true : false;
    if (shouldExist != exists) {
        if (shouldExist) {
            EffectPropertiesView *view = new EffectPropertiesView(m_description->displayName(),
                                                                  m_description->description(),
                                                                  static_cast<QWidget *>(m_viewHandler->parentView(MultiViewHandler::propertiesView))
                                                                 );
            connect(view, SIGNAL(reset()), this, SIGNAL(reset()));
            connect(view, SIGNAL(disabled(bool)), this, SLOT(setDisabled(bool)));
            connect(view, SIGNAL(collapsed(bool)), this, SLOT(setPropertiesViewCollapsed(bool)));
            m_viewHandler->setView(MultiViewHandler::propertiesView, view);
            orderedChildViewUpdate(MultiViewHandler::propertiesView, begin(), end());
        } else {
            QObject *view = m_viewHandler->popView(MultiViewHandler::propertiesView);
            orderedChildViewUpdate(MultiViewHandler::propertiesView, begin(), end());
            delete view;
        }
    }
}

void Effect::checkTimelineViewState()
{
}

void Effect::checkMonitorViewState()
{
}

void Effect::setDisabled(bool disabled)
{
    setProperty("disable", QString::number(disabled));
}

void Effect::setPropertiesViewCollapsed(bool collapsed)
{
    // ...
}


#include "effect.moc"

