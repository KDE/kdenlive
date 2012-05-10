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
#include <kdemacros.h>

class AbstractParameterDescription;
class AbstractParameterList;
class MultiViewHandler;


class KDE_EXPORT AbstractParameter : public EffectSystemItem
{
    Q_OBJECT

public:
    AbstractParameter(const AbstractParameterDescription *parameterDescription, AbstractParameterList *parent);
    virtual ~AbstractParameter() {};

    virtual void set(const char *data) = 0;
    virtual const char *get() const;
    QString getName() const;
    const AbstractParameterDescription *getDescription() const;

    void checkPropertiesViewState() {};
    void checkTimelineViewState() {};
    void checkMonitorViewState() {};

    AbstractParameterList *m_parent;
    const AbstractParameterDescription *m_abstractDescription;
};

#endif
