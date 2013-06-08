/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#ifndef ABSTRACTPARAMETER_H
#define ABSTRACTPARAMETER_H

#include "effectsystemitem.h"
#include <QObject>
#include "kdenlivecore_export.h"

class AbstractParameterDescription;
class AbstractParameterList;


/**
 * @class AbstractParameter
 * @brief Abstract base class for parameters.
 * 
 * Parameters are constructed from their descriptions and also store a pointer to it. Therefore as
 * little information as possible should be copied from the description but instead the values from
 * there should be used directly.
 */

class KDENLIVECORE_EXPORT AbstractParameter : public EffectSystemItem
{
    Q_OBJECT

public:
    /**
     * @brief Stores the pointers to description and parent.
     */
    AbstractParameter(const AbstractParameterDescription *parameterDescription, AbstractParameterList *parent);
    virtual ~AbstractParameter();

    /**
     * @brief Should set the parameter's value.
     * @param data the new value.
     */
    virtual void set(const char *data) = 0;

    /**
     * @brief Returns the current raw value.
     * 
     * The value is received from the parent parameter list.
     */
    virtual const char *get() const;

    /**
     * @brief Returns the (internal) name.
     */
    QString name() const;

    /**
     * @brief Returns a pointer to the description.
     */
    const AbstractParameterDescription *description() const;

    /**
     * @brief Empty implementation. Needs to reimplemented if the parameter has a properties view.
     */
    void checkPropertiesViewState();

    /**
     * @brief Empty implementation. Needs to reimplemented if the parameter has a timeline view.
     */
    void checkTimelineViewState();

    /**
     * @brief Empty implementation. Needs to reimplemented if the parameter has a monitor view.
     */
    void checkMonitorViewState();

protected:
    /** pointer to parent parameter list */
    AbstractParameterList *m_parent;

protected slots:
    /**
     * @brief Empty implementation. Needs to reimplemented if there is something to reset.
     */
    virtual void resetValue();

private:
    const AbstractParameterDescription *m_description;
};

#endif
