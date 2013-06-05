/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "shiftingproducer.h"
#include "producerwrapper.h"


ShiftingProducer::ShiftingProducer(ProducerWrapper* producer, QObject *parent) :
    QObject(parent),
    m_producer(producer)
{
}

ShiftingProducer::~ShiftingProducer()
{
    delete m_producer;
}

ProducerWrapper* ShiftingProducer::producer()
{
    return m_producer;
}

void ShiftingProducer::setProducer(ProducerWrapper* newProducer)
{
    ProducerWrapper *oldProducer = m_producer;
    m_producer = newProducer;

    if (oldProducer) {
        emit producerShifted();

        m_producer->pass_values(*oldProducer, "kdenlive");
        m_producer->pass_list(*oldProducer, m_registeredProperties.join(",").toUtf8().constData());

        delete oldProducer;
    }
}

void ShiftingProducer::registerProperty(QString name, QString value)
{
    m_registeredProperties.append(name);
    if (!value.isEmpty()) {
        m_producer->setProperty(name, value);
    }
}

QStringList ShiftingProducer::registeredProperties() const
{
    return m_registeredProperties;
}
