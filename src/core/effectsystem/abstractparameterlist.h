/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#ifndef ABSTRACTPARAMETERLIST_H
#define ABSTRACTPARAMETERLIST_H

#include "effectsystemitem.h"
#include "abstractparameter.h"
#include "abstractparameterdescription.h"
#include <QObject>
#include <QList>
#include "kdenlivecore_export.h"


namespace Mlt
{
    class Properties;
}

/**
 * @class AbstractParameterList
 * @brief Abstract base class for parameter containers.
 */

class KDENLIVECORE_EXPORT AbstractParameterList : public EffectSystemItem, protected QList<AbstractParameter*>
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an empty parameter list.
     */
    AbstractParameterList(EffectSystemItem *parent = 0);
    virtual ~AbstractParameterList() {}

    /**
     * @brief Creates and stores parameters from a list of descriptions.
     * @param parameters list of parameters to load
     */
    void createParameters(const QList<AbstractParameterDescription *> &parameters, Mlt::Properties currentProperties);

    /**
     * @brief Should set the value of a parameter to its MLT property.
     * @param name name/identifier of the parameter
     * @param value value to set
     * 
     * This function should only be used to pass the value from a parameter object to its MLT
     * filter property.
     */
    virtual void setParameterValue(const QString &name, const QString &value) = 0;

    /**
     * @brief Should receive the parameter value from MLT.
     * @param name name/id of the parameter
     */
    virtual QString parameterValue(const QString &name) const = 0;

};

#endif
