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

class EffectRepository;


class EffectDevice : public AbstractEffectList
{
    Q_OBJECT

public:
    EffectDevice(Mlt::Service service, EffectRepository *repository, QWidget *parameterViewParent);
    ~EffectDevice();

    void appendFilter(Mlt::Filter *filter);
    void appendEffect(QString id);
    void appendEffect(EffectDescription *description);
    Mlt::Service getService();

    void checkPropertiesViewState();
    void checkTimelineViewState();
    void checkMonitorViewState();

private:
    Mlt::Service m_service;
    EffectRepository *m_repository;
};

#endif
