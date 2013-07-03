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
#include "multiviewhandler.h"
#include <QWidget>
#include <KDebug>


EffectDevice::EffectDevice(Mlt::Service service, EffectRepository *repository, QWidget *propertiesViewMainWidget) : 
    AbstractEffectList(repository),
    m_service(service)
{
    m_viewHandler->setView(MultiViewHandler::propertiesView, propertiesViewMainWidget);
}

EffectDevice::~EffectDevice()
{

}

void EffectDevice::appendFilter(Mlt::Filter* filter)
{
    m_service.attach(*filter);
}

void EffectDevice::removeFilter(Mlt::Filter* filter)
{
    m_service.detach(*filter);
}

Mlt::Service EffectDevice::service()
{
    return m_service;
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
