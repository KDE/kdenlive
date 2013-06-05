/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "imagetimelineclip.h"
#include "imageprojectclip.h"
#include "core/project/producerwrapper.h"
#include "core/project/timelinetrack.h"

#include <KDebug>


ImageTimelineClip::ImageTimelineClip(ImageProjectClip* projectClip, TimelineTrack* parent, ProducerWrapper* producer) :
    AbstractTimelineClip(projectClip, parent, producer)
{
    kDebug() << "image timelineclip created";
}

ImageTimelineClip::~ImageTimelineClip()
{

}

#include "imagetimelineclip.moc"
