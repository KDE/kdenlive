/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef PRODUCERREPOSITORY_H
#define PRODUCERREPOSITORY_H

#include <QMap>
#include <QHash>
#include <QAction>
#include "../monitor/mltcontroller.h"

class AbstractParameterDescription;
class QStringList;
class ProducerDescription;
class MltCore;

namespace Mlt {
    class Properties;
    class Repository;
}

enum ProducerTypes { AudioProducer, VideoProducer };


/**
 * @class EffectRepository
 * @brief Contains the descriptions of all available filters.
 * 
 * The filter list is filtered by a blacklist (data/blacklisted_effects.txt). To create the
 * descriptions the effect xml files are used and only when not available for a specific filter the
 * metadata provided by MLT is used.
 */

class KDENLIVECORE_EXPORT ProducerRepository
{
public:
    /**
     * @brief Constructs the repository.
     */
    ProducerRepository(MltCore *core);
    ~ProducerRepository();


    /**
     * @brief Returns a pointer to the requested producer description.
     * @param id service name of the producer whose description should be received
     */
    ProducerDescription* producerDescription(const QString &id);
    /**
     * @brief Returns a pointer to the main MLT repository
     */
    Mlt::Repository *repository();
    /**
     * @brief Returns a list of actions that can be used to trigger creation of a producer
     */
    QList <QAction *> producerActions(QWidget *parent) const;
    /**
     * @brief Returns the displayable name of a producer
     * @param id the producer's mlt service name
     */
    const QString getProducerDisplayName(const QString &id) const;
    /**
     * @brief Returns the names of this producer's properties.
     * @param id the producer's mlt service name
     */
    QStringList producerProperties(const QString &id);
    /**
     * @brief Returns an empty parameter description as received from the factory of its plugin.
     * @param type type of the parameter for which the description should be received
     */
    AbstractParameterDescription* newParameterDescription(const QString &type);

private:
    void initRepository();
    MltCore *m_core;
    QMap <QString, ProducerDescription*> m_producers;
};

#endif
