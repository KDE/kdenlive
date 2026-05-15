
/*
SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "timelineitemmodel.hpp"

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <QDir>

class TimelineItemModel;

/** @class TimelineMixManager
    @brief This class handles mix related functions from a timelinemodel
 */
class TimelineMixManager : public QObject
{
    Q_OBJECT
public:
    TimelineMixManager(std::shared_ptr<TimelineItemModel> timeline, QObject *parent);
    bool plantMix(int tid, Mlt::Transition *t);
    bool resizeStartMix(int cid, int duration, bool singleResize);
    int getMixDuration(int cid);
    std::pair<int, int> getMixInOut(int cid);
    /** @brief Get Mix cut pos (the duration of the mix on the right clip) */
    int getMixCutPos(int cid);
    MixAlignment getMixAlign(int cid);
    void requestResizeMix(int cid, int duration, MixAlignment align, int leftFrames);

private:
    std::shared_ptr<TimelineItemModel> m_timeline;
    mutable QReadWriteLock m_lock; // This is a lock that ensures safety in case of concurrent access
};
