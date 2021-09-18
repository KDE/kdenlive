/*
SPDX-FileCopyrightText: 2012 Simon A. Eugster (Granjow)  <simon.eu@gmail.com>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
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
