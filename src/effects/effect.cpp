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
#include <mlt++/Mlt.h>
#include <core/effectsystem/multiviewhandler.h>
#include <QWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <KDebug>


Effect::Effect(EffectDescription *effectDescription, AbstractEffectList* parent) :
    AbstractParameterList(parent),
    m_description(effectDescription)
{
    m_filter = new Mlt::Filter(*parent->service().profile(), effectDescription->tag().toUtf8().constData());

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
    bool exists = m_viewHandler->hasView(EffectPropertiesView);
    bool shouldExist = m_viewHandler->parentView(EffectPropertiesView) ? true : false;
    if (shouldExist != exists) {
        if (shouldExist) {
            // TODO : proper widget
            QWidget *p = static_cast<QWidget *>(m_viewHandler->parentView(EffectPropertiesView));
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

