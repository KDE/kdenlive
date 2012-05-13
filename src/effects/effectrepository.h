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

class AbstractEffectRepositoryItem;
class AbstractParameterDescription;
class QStringList;
class KPluginFactory;
namespace Mlt {
    class Properties;
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

class EffectRepository
{
public:
    /**
     * @brief Constructs the repository.
     */
    EffectRepository();
    ~EffectRepository();

    /**
     * @brief Returns an empty parameter description as received from the factory of its plugin.
     * @param type type of the parameter for which the description should be received
     */
    AbstractParameterDescription *newParameterDescription(const QString &type);

    /**
     * @brief Returns a pointer to the requested effect description.
     * @param id name/kdenlive internal id of the effect whose description should be received
     */
    AbstractEffectRepositoryItem *effectDescription(const QString &id);

private:
    void initRepository();
    void getNamesFromProperties(Mlt::Properties *properties, QStringList &names) const;
    void applyBlacklist(const QString &filename, QStringList &list) const;
    void loadParameterPlugins();

    QMap <QString, AbstractEffectRepositoryItem*> m_effects;
    QHash <QString, KPluginFactory*> m_parameterPlugins;
};

#endif
