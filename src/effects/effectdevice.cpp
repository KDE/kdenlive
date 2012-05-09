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

EffectDevice::EffectDevice(Mlt::Service* service) : 
    AbstractEffectList(),
    m_service(service)
{
    m_uiHandler = new MultiUiHandler();
}

EffectDevice::~EffectDevice()
{

}

void EffectDevice::appendFilter(Mlt::Filter* filter)
{
    m_service->attach(*filter);
}

Mlt::Service* EffectDevice::getService()
{
    return m_service;
}

void EffectDevice::appendEffect(QString id)
{
    
}

void EffectDevice::appendEffect(EffectDescription* description)
{

}
