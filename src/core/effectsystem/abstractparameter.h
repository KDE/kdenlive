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

#include "multiuihandler.h"
#include <QObject>
#include <kdemacros.h>

class AbstractParameterDescription;
class AbstractParameterList;
class MultiUiHandler;


class KDE_EXPORT AbstractParameter : public QObject
{
    Q_OBJECT

public:
    AbstractParameter(AbstractParameterDescription *parameterDescription, AbstractParameterList *parent);
    virtual ~AbstractParameter();

    virtual void set(const char *data) = 0;
    virtual const char *get() const = 0;
    QString getName() const;
    AbstractParameterDescription *getDescription();
    MultiUiHandler *getMultiUiHandler();

public slots:
    virtual void createUi(EffectUiTypes type, QObject *parent) = 0;

protected:
    AbstractParameterList *m_parent;
    AbstractParameterDescription *m_abstractDescription;

private:
    MultiUiHandler *m_uiHandler;
};

#endif
