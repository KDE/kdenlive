/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb.kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ADDEFFECTCOMMAND_H
#define ADDEFFECTCOMMAND_H

#include <QUndoCommand>

class EffectDescription;
class EffectDevice;
class Effect;



/**
 * @class AddEffectCommand
 * @brief Handles creating and adding an effect to a clip.
 * 
 */

class AddEffectCommand : public QUndoCommand
{
public:
    /**
     * @brief Constructor.
     * @param description description of the effect
     * @param device device to create the effect
     * @param addEffect should we create or delete the clip on redo
     */
    explicit AddEffectCommand(EffectDescription *description, EffectDevice *device, bool addEffect, QUndoCommand* parent = 0);

    void undo();
    void redo();
    void addEffect();
    void deleteEffect();

private:
    EffectDescription *m_description;
    EffectDevice *m_device;
    Effect *m_effect;
    bool m_addEffect;
};

#endif
