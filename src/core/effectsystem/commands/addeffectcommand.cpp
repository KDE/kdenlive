/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb.kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "addeffectcommand.h"
#include <effectsystem/effectdescription.h>
#include <effectsystem/effectdevice.h>
#include <effectsystem/effect.h>
#include "project/abstractprojectclip.h"
#include "project/projectfolder.h"
#include "project/abstractclipplugin.h"
#include <KLocalizedString>

#include <KDebug>

AddEffectCommand::AddEffectCommand(EffectDescription *description, EffectDevice *device, bool addEffect, QUndoCommand* parent) :
    QUndoCommand(parent)
    , m_description(description)
    , m_device(device)
    , m_effect(NULL)
    , m_addEffect(addEffect)
{
    setText(i18n("Add effect %1", m_description->displayName()));
}

void AddEffectCommand::undo()
{
    if (m_addEffect)
        deleteEffect();
    else
        addEffect();
}

void AddEffectCommand::redo()
{
    if (m_addEffect)
        addEffect();
    else
        deleteEffect();
}

void AddEffectCommand::addEffect()
{
    m_effect = m_description->createEffect(m_device);
    m_effect->checkPropertiesViewState();
    m_device->requestUpdateClip();
}

void AddEffectCommand::deleteEffect()
{
    if (m_effect) {
        delete m_effect;
        m_effect = NULL;
        m_device->requestUpdateClip();
    }
}

