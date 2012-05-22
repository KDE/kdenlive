/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef IMAGETIMELINECLIP_H
#define IMAGETIMELINECLIP_H

#include "core/project/abstracttimelineclip.h"

class ImageProjectClip;


class ImageTimelineClip : public AbstractTimelineClip
{
    Q_OBJECT

public:
    ImageTimelineClip(ProducerWrapper* producer, ImageProjectClip* projectClip, TimelineTrack* parent);
    ~ImageTimelineClip();
};

#endif
