/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "videotimelineclip.h"
#include "videoprojectclip.h"
#include "core/project/producerwrapper.h"
#include "core/project/timelinetrack.h"

#include <KDebug>


VideoTimelineClip::VideoTimelineClip(VideoProjectClip* projectClip, TimelineTrack* parent, ProducerWrapper* producer) :
    AbstractTimelineClip(projectClip, parent, producer)
{
    kDebug() << "video timelineclip created";
}

VideoTimelineClip::~VideoTimelineClip()
{

}

#include "videotimelineclip.moc"
