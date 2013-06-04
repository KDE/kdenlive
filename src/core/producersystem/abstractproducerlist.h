/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#ifndef ABSTRACTPRODUCERLIST_H
#define ABSTRACTPRODUCERLIST_H

#include <mlt++/Mlt.h>
#include <QList>
#include <QObject>
#include <kdemacros.h>

class ProducerRepository;
class MultiViewHandler;
class Producer;
class ProducerDescription;


/**
 * @class AbstractProducerList
 * @brief Abstract base class for producer containers.
 */

class KDE_EXPORT AbstractProducerList : public QObject, protected QList<Producer *>
{
    Q_OBJECT
public:
    /**
     * @brief Constructs an empty effect list.
     * @param parent parent container
     */
    AbstractProducerList(ProducerRepository *repository, AbstractProducerList *parent = 0);
    virtual ~AbstractProducerList();

    /**
     * @brief Should append a filter to the MLT DOM.
     * @param filter filter to append
     */
    virtual void appendProducer(Mlt::Producer *producer) = 0;

    /**
     * @brief Should return the service this object or its parent represent.
     */
    virtual Mlt::Service service() = 0;

public slots:
    /**
     * @brief Creates and appends an effect.
     * @param id name/kdenlive identifier of the effect
     */
    virtual void appendProducer(const QString &id);

    /**
     * @brief Creates and appends an effect.
     * @param description effect description for the effect to create
     */
//    virtual void appendProducer(ProducerDescription *description);

protected:
    MultiViewHandler *m_viewHandler;
    /** change! */
    ProducerRepository *m_repository;
};

#endif
