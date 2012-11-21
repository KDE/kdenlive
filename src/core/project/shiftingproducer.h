/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef SHIFTINGPRODUCER_H
#define SHIFTINGPRODUCER_H


#include <QObject>
#include <QStringList>
#include <kdemacros.h>

class ProducerWrapper;


/**
 * @class ShiftingProducer
 * @brief Provides a steady connection to shifting producers.
 * 
 * Producers might shift for example when moving a clip across tracks, since some clip types
 * cannot use a single parent producer simultaneously (audio issues with avformat).
 * When a new producer is set properties added by Kdenlive (prefix "kdenlive") and registered ones
 * (@see registerProperty) are transfered to the new producer. Additionally producerShifted is emitted.
 */


class KDE_EXPORT ShiftingProducer : public QObject
{
    Q_OBJECT

public:
    /** @brief Constructor. */
    ShiftingProducer(ProducerWrapper *producer, QObject *parent = 0);
    /** @brief Deletes the current producer. */
    virtual ~ShiftingProducer();

    /**
     * @brief Returns the current producer.
     *
     * The returned producer is deleted after producerShifted is emitted or this gets deleted. 
     */
    ProducerWrapper *producer();
    /**
     * @brief Sets a new producer.
     * 
     * Emits producerShifted after the new producer is available, then transfers all Kdenlive specific
     * properties (prefix "kdenlive") and registered ones (@see registerProperty). The old producer
     * is deleted afterwards.
     */
    void setProducer(ProducerWrapper *newProducer);

    /**
     * @brief Registers a property for being transfered over to new producers.
     * @param name name of the property
     * @param value value the property should have
     * 
     * Kdenlive specific properties (prefix "kdenlive") do not need to be registered. 
     */
    void registerProperty(QString name, QString value = QString());

    /** @brief Returns a list of all registered properties. */
    QStringList registeredProperties() const;

signals:
    void producerShifted();

protected:
    ProducerWrapper *m_producer;

private:
    QStringList m_registeredProperties;
};

#endif
