 /*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#include "effectdevice.h"
#include "effectdescription.h"
#include "effect.h"
#include "core/effectsystem/multiviewhandler.h"
#include <QWidget>
#include <KDebug>


EffectDevice::EffectDevice(Mlt::Service service, EffectRepository *repository, QWidget *parameterViewParent) : 
    AbstractEffectList(),
    m_service(service),
    m_repository(repository)
{
    m_viewHandler->setView(EffectPropertiesView, parameterViewParent);
}

EffectDevice::~EffectDevice()
{

}

void EffectDevice::appendFilter(Mlt::Filter* filter)
{
    m_service.attach(*filter);
}

Mlt::Service EffectDevice::getService()
{
    return m_service;
}

void EffectDevice::appendEffect(QString id)
{
    AbstractEffectRepositoryItem *item = m_repository->getEffectDescription(id);
    EffectDescription *effect = static_cast<EffectDescription*>(item);
    appendEffect(effect);
}

void EffectDevice::appendEffect(EffectDescription* description)
{
    Effect *effect = description->createEffect(this);
    append(effect);
    orderedChildViewUpdate(EffectPropertiesView, begin(), end());
}

void EffectDevice::checkPropertiesViewState()
{
}

void EffectDevice::checkTimelineViewState()
{
}

void EffectDevice::checkMonitorViewState()
{
}

#include "effectdevice.moc"
