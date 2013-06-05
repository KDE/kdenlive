/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#ifndef ABSTRACTEFFECTLIST_H
#define ABSTRACTEFFECTLIST_H

#include "effectsystemitem.h"
#include <mlt++/Mlt.h>
#include <QList>
#include <kdemacros.h>

class EffectRepository;
class Effect;
class EffectDescription;


/**
 * @class AbstractEffectList
 * @brief Abstract base class for effect containers.
 */

class KDE_EXPORT AbstractEffectList : public EffectSystemItem, protected QList<Effect *>
{
    Q_OBJECT
public:
    /**
     * @brief Constructs an empty effect list.
     * @param parent parent container
     */
    AbstractEffectList(EffectRepository *repository, AbstractEffectList *parent = 0);
    virtual ~AbstractEffectList();

    /**
     * @brief Should append a filter to the MLT DOM.
     * @param filter filter to append
     */
    virtual void appendFilter(Mlt::Filter *filter) = 0;

    /**
     * @brief Should return the service this object or its parent represent.
     */
    virtual Mlt::Service service() = 0;

public slots:
    /**
     * @brief Creates and appends an effect.
     * @param id name/kdenlive identifier of the effect
     */
    virtual void appendEffect(const QString &id);

    /**
     * @brief Creates and appends an effect.
     * @param description effect description for the effect to create
     */
    virtual void appendEffect(EffectDescription *description);

protected:
    /** change! */
    EffectRepository *m_repository;
};

#endif
