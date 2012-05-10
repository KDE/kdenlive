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
#include <QObject>
#include <QDomNodeList>
#include <QList>
#include <kdemacros.h>

class MultiViewHandler;

class KDE_EXPORT AbstractParameterList : public EffectSystemItem, protected QList<AbstractParameter*>
{
    Q_OBJECT

public:
    AbstractParameterList(EffectSystemItem *parent = 0);
    virtual ~AbstractParameterList() {};

    void loadParameters(QList<AbstractParameterDescription *> parameters);

    virtual void setParameter(QString name, QString value) = 0;
    virtual QString getParameter(QString name) const = 0;
};

#endif
