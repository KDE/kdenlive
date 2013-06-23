/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb.kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "webvfxtimelineclip.h"
#include "webvfxprojectclip.h"
#include "core/project/producerwrapper.h"
#include "core/project/timelinetrack.h"

#include <KDebug>


WebvfxTimelineClip::WebvfxTimelineClip(WebvfxProjectClip* projectClip, TimelineTrack* parent, ProducerWrapper* producer) :
    AbstractTimelineClip(projectClip, parent, producer)
{
    kDebug() << "webvfx timelineclip created";
}

WebvfxTimelineClip::~WebvfxTimelineClip()
{

}

#include "webvfxtimelineclip.moc"
