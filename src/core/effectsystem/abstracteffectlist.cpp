 /*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#include "abstracteffectlist.h"
#include "abstracteffectrepositoryitem.h"
#include "effectdescription.h"
#include "effect.h"


AbstractEffectList::AbstractEffectList(EffectRepository *repository, AbstractEffectList *parent) :
    EffectSystemItem(parent),
    m_repository(repository)
{
}

AbstractEffectList::~AbstractEffectList()
{
}

void AbstractEffectList::requestUpdateClip()
{
    emit updateClip();
}

void AbstractEffectList::appendEffect(const QString& id)
{
    AbstractEffectRepositoryItem *item = m_repository->effectDescription(id);
    EffectDescription *effect = static_cast<EffectDescription*>(item);
    appendEffect(effect);
}

void AbstractEffectList::appendEffect(EffectDescription* description)
{
    Effect *effect = description->createEffect(this);
    append(effect);
    orderedChildViewUpdate(MultiViewHandler::propertiesView, begin(), end());
}

#include "abstracteffectlist.moc"
