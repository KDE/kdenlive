 /*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#ifndef EFFECTDEVICE_H
#define EFFECTDEVICE_H

#include "abstracteffectlist.h"
#include <mlt++/Mlt.h>


class EffectDevice : public AbstractEffectList
{
    Q_OBJECT

public:
    EffectDevice(Mlt::Service *service);
    ~EffectDevice();
    
    void appendFilter(Mlt::Filter *filter);
    Mlt::Service *getService();

    void appendEffect(QString id);

private:
    Mlt::Service *m_service;
};

#endif
