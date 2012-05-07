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

#include "multiuihandler.h"
#include <mlt++/Mlt.h>
#include <QList>

class Effect;
class EffectDescription;

class AbstractEffectList : public QObject, protected QList<Effect *>
{
    Q_OBJECT
public:
    AbstractEffectList();
    ~AbstractEffectList();

    virtual void appendFilter(Mlt::Filter *filter) = 0;
    virtual void appendEffect(QString id) = 0;
    virtual void appendEffect(EffectDescription *description) = 0;

    MultiUiHandler *getUiHandler();
    virtual Mlt::Service *getService() = 0;

public slots:
    virtual void appendEffect(QString id) = 0;

protected:
    MultiUiHandler *m_uiHandler;
};

#endif
