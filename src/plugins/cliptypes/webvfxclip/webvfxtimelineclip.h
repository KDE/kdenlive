/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb.kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef WEBVFXTIMELINECLIP_H
#define WEBVFXTIMELINECLIP_H

#include "core/project/abstracttimelineclip.h"

class WebvfxProjectClip;


class WebvfxTimelineClip : public AbstractTimelineClip
{
    Q_OBJECT

public:
    WebvfxTimelineClip(WebvfxProjectClip* projectClip, TimelineTrack* parent, ProducerWrapper* producer = 0);
    ~WebvfxTimelineClip();
};

#endif
