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
#include "kdenlivecore_export.h"

class EffectRepository;


/**
 * @class EffectDevice
 * @brief Top level effect system item to handle connection between effects and item they are attached to.
 * 
 * Expand when timeline is refactored?
 */

class KDENLIVECORE_EXPORT EffectDevice : public AbstractEffectList
{
    Q_OBJECT

public:
    /**
     * @brief Creates an empty effect device.
     * @param service MLT service the effects should be attached to
     * @param repository remove!!
     * @param propertiesViewMainWidget top widget for the properties view 
     */
    EffectDevice(Mlt::Service service, EffectRepository *repository, QWidget *propertiesViewMainWidget);
    ~EffectDevice();

    /**
     * @brief Appends a filter to the service.
     * @param filter pointer to filter to attach
     */
    void appendFilter(Mlt::Filter *filter);
    
    void removeFilter(Mlt::Filter* filter);

    /**
     * @brief Returns the MLT service of which the device manages the filters.
     */
    Mlt::Service service();

    /**
     * tbd
     */
    void checkPropertiesViewState();

    /**
     * tbd
     */
    void checkTimelineViewState();

    /**
     * tbd
     */
    void checkMonitorViewState();

private:
    Mlt::Service m_service;
};

#endif
