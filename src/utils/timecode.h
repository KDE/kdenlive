/*
   SPDX-FileCopyrightText: 2003 Jason Wood <jasonwood@blueyonder.co.uk>
   SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QString>

#include "gentime.h"

/**
 * Handles the conversion of a GenTime into a nicely formatted string, taking into account things such as drop frame if necessary.
 * Handles multiple formats, such as HH:MM:SS:FF, HH:MM:SS:F, All Frames, All Seconds, etc.
 *
 * For 29.97, 59.94, or 23.976 fps, the drop-frame timecode is used: For this frame rate, frames 0 and 1 are dropped every minute
 * but not every 10th to fix time shift; see https://en.wikipedia.org/wiki/SMPTE_timecode for details.
 *
 * To distinguish between normal and drop-frame timecode, they use a different format.
 *
 * HH:MM:SS:FF → Normal time code in hour/minute/second/frame format
 * HH:MM:SS,FF → Drop-frame time code
 *
 * @author Jason Wood
*/
class Timecode
{
public:
    enum Formats { HH_MM_SS_FF, HH_MM_SS_HH, Frames, Seconds };

    explicit Timecode(Formats format = HH_MM_SS_FF, double framesPerSecond = 25);

    /**
     * Set the current timecode format; this is the output format for this timecode.
     */
    void setFormat(double framesPerSecond, Formats format = HH_MM_SS_FF);

    Formats format() const;

    ~Timecode();

    /** Returns the timecode for a given time */
    QString getDisplayTimecode(const GenTime &time, bool frameDisplay) const;
    QString getTimecode(const GenTime &time) const;
    int getFrameCount(const QString &duration) const;
    const QString getDisplayTimecodeFromFrames(int frames, bool frameDisplay) const;
    const QString getTimecodeFromFrames(int frames) const;
    double fps() const;
    const QString mask(const GenTime &t = GenTime()) const;
    QString reformatSeparators(QString duration) const;

    /** @brief Returns the timecode in an hh:mm:ss or hh:mm:ss.fff format */
    static QString getStringTimecode(int frames, const double &fps, bool showFrames = false);
    /** @brief Returns duration in "MMm:SSs:FFf" or "SSs:FFf" format for markers/guides display */
    static QString formatMarkerDuration(int frames, double fps);
    // TODO: The scaleTimecode seems error prone, we should reconsider it and maybe add tests
    static QString scaleTimecode(QString timecode, double sourceFps, double targetFps);

private:
    Formats m_format;
    bool m_dropFrameTimecode;
    int m_displayedFramesPerSecond;
    double m_realFps;
    double m_dropFrames;
    int m_framesPer10Minutes;

    const QString getTimecodeHH_MM_SS_FF(const GenTime &time) const;
    const QString getTimecodeHH_MM_SS_FF(int frames) const;

    const QString getTimecodeHH_MM_SS_HH(const GenTime &time) const;
    const QString getTimecodeFrames(const GenTime &time) const;
    const QString getTimecodeSeconds(const GenTime &time) const;
    const QString getTimecodeDropFrame(const GenTime &time) const;
    const QString getTimecodeDropFrame(int framenumber) const;
};
