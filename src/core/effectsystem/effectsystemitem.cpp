 /*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#include "effectsystemitem.h"


EffectSystemItem::EffectSystemItem(EffectSystemItem* parent) :
    m_parent(parent)
{
    if (parent) {
        m_viewHandler = new MultiViewHandler(parent->getViewHandler());

        connect(parent, SIGNAL(propertiesViewUpdateRequired()), this, SLOT(checkPropertiesViewState()));
        connect(parent, SIGNAL(timelineViewUpdateRequired()), this, SLOT(checkTimelineViewState()));
        connect(parent, SIGNAL(monitorViewUpdateRequired()), this, SLOT(checkMonitorViewState()));
    } else {
        m_viewHandler = new MultiViewHandler();
    }
}

EffectSystemItem::~EffectSystemItem()
{
    delete m_viewHandler;
}

MultiViewHandler* EffectSystemItem::getViewHandler()
{
    return m_viewHandler;
}

void EffectSystemItem::checkViewsState()
{
    checkPropertiesViewState();
    checkTimelineViewState();
    checkMonitorViewState();
}

#include "effectsystemitem.moc"
