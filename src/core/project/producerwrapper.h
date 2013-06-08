/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef PRODUCERWRAPPER_H
#define PRODUCERWRAPPER_H

#include <mlt++/Mlt.h>
#include <QString>
#include "kdenlivecore_export.h"

class QPixmap;


/**
 * @class ProducerWrapper
 * @brief Provides a thin convenience wrapper around Mlt::Producer
 */


class KDENLIVECORE_EXPORT ProducerWrapper : public Mlt::Producer
{
public:
    /**
     * @brief Constructor.
     * @param producer producer to create reference to
     */
    ProducerWrapper(Mlt::Producer *producer);
    /**
     * @brief Constructs a new producer.
     * @param profile profile to use
     * @param input construction argument for the producer (for example the resource)
     * @param service type of producer to create
     * @see mlt_factory_producer
     */
    ProducerWrapper(Mlt::Profile &profile, const QString &input, const QString &service = QString());
    virtual ~ProducerWrapper();

    /**
     * @brief Sets a property.
     * @param name name of the property
     * @param value the new value
     */
    void setProperty(const QString &name, const QString &value);
    /**
     * @brief Returns the value of a property.
     * @param name name o the property
     */
    QString property(const QString &name);

    /**
     * @brief Returns a pixmap created from a frame of the producer.
     * @param position frame position
     * @param width width of the pixmap (only a guidance)
     * @param height height of the pixmap (only a guidance)
     */
    QPixmap pixmap(int position = 0, int width = 0, int height = 0);
    
    QString resourceName() const;
    QString serviceName() const;
    
private:
    QString m_resource;
    QString m_service;
};

#endif
