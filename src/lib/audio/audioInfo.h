/*
Copyright (C) 2012  Simon A. Eugster (Granjow)  <simon.eu@gmail.com>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef AUDIOINFO_H
#define AUDIOINFO_H

#include <QList>
#include <memory>
#include <mlt++/Mlt.h>

class AudioStreamInfo;
class AudioInfo
{
public:
    explicit AudioInfo(const std::shared_ptr<Mlt::Producer> &producer);
    ~AudioInfo();

    int size() const;
    AudioStreamInfo const *info(int pos) const;

    void dumpInfo() const;

private:
    QList<AudioStreamInfo *> m_list;
};

#endif // AUDIOINFO_H
