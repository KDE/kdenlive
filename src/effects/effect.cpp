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
#include <core/effectsystem/multiviewhandler.h>
#include <QWidget>
#include <QVBoxLayout>
#include <KDebug>

#include <QLabel>


Effect::Effect(EffectDescription *effectDescription, AbstractEffectList* parent) :
    AbstractParameterList(parent),
    m_description(effectDescription)
{
    m_filter = new Mlt::Filter(*parent->getService().profile(), effectDescription->getTag().toUtf8().constData());
    parent->appendFilter(m_filter);
    loadParameters(effectDescription->getParameters());
}

Effect::~Effect()
{
    // ?
    delete m_filter;
}

void Effect::setParameter(QString name, QString value)
{
    setProperty(name, value);
}

QString Effect::getParameter(QString name) const
{
    return getProperty(name);
}

void Effect::setProperty(QString name, QString value)
{
    m_filter->set(name.toUtf8().constData(), value.toUtf8().constData());
}

QString Effect::getProperty(QString name) const
{
    return QString(m_filter->get(name.toUtf8().constData()));
}

void Effect::checkPropertiesViewState()
{
    bool exists = m_viewHandler->hasView(EffectPropertiesView);
    bool shouldExist = m_viewHandler->getParentView(EffectPropertiesView) ? true : false;
    if (shouldExist != exists) {
        if (shouldExist) {
            // TODO : proper widget
            QWidget *p = static_cast<QWidget *>(m_viewHandler->getParentView(EffectPropertiesView));
            QFrame *w = new QFrame(p);
            QVBoxLayout *l = new QVBoxLayout(w);
            p->layout()->addWidget(w);
            m_viewHandler->setView(EffectPropertiesView, w);
            orderedChildViewUpdate(EffectPropertiesView, begin(), end());
        } else {
            QObject *view = m_viewHandler->popView(EffectPropertiesView);
            orderedChildViewUpdate(EffectPropertiesView, begin(), end());
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

#include "effect.moc"

