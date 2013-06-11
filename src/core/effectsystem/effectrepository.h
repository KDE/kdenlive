/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef EFFECTREPOSITORY_H
#define EFFECTREPOSITORY_H

#include <QMap>
#include <QHash>
#include <QAction>
#include "../monitor/mltcontroller.h"

class AbstractEffectRepositoryItem;
class AbstractParameterDescription;
class MltCore;

namespace Mlt {
    class Repository;
}

enum EffectTypes { AudioEffect, VideoEffect, CustomEffect };


/**
 * @class EffectRepository
 * @brief Contains the descriptions of all available filters.
 * 
 * The filter list is filtered by a blacklist (data/blacklisted_effects.txt). To create the
 * descriptions the effect xml files are used and only when not available for a specific filter the
 * metadata provided by MLT is used.
 */

class KDENLIVECORE_EXPORT EffectRepository
{
public:
    /**
     * @brief Constructs the repository.
     */
    EffectRepository(MltCore *core);
    ~EffectRepository();

    /**
     * @brief Returns a pointer to the requested effect description.
     * @param id name/kdenlive internal id of the effect whose description should be received
     */
    AbstractEffectRepositoryItem *effectDescription(const QString &id);
    /**
     * @brief Returns an empty parameter description as received from the factory of its plugin.
     * @param type type of the parameter for which the description should be received
     */
    AbstractParameterDescription* newParameterDescription(const QString &type);
    /**
     * @brief Returns a pointer to the main MLT repository
     */
    Mlt::Repository *repository();
    
    QMap<QString, QString> getEffectsList();

private:
    void initRepository();
    MltCore *m_core;
    /** key: id of the effect */
    QMap <QString, AbstractEffectRepositoryItem*> m_effects;
};

#endif
