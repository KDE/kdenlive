/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TIMECODEFORMATTER_H
#define TIMECODEFORMATTER_H

#include "fraction.h"
#include <QObject>
#include <kdemacros.h>

class Timecode;


class KDE_EXPORT TimecodeFormatter : public QObject
{
    Q_OBJECT
    
public:
    enum Formats { HH_MM_SS_FF, HH_MM_SS_HH, Frames, Seconds, Milliseconds, Time, DefaultFormat };

    TimecodeFormatter(const Fraction &framerate = Fraction(), Formats defaultFormat = DefaultFormat, QObject *parent = 0);

    void setFramerate(const Fraction &framerate);
    Fraction framerate() const;

    void setDefaultFormat(Formats format);
    Formats defaultFormat() const;

    QString format(const Timecode &timecode, Formats format = DefaultFormat) const;
    QString mask(const Timecode &timecode) const;

    Timecode fromString(const QString &timecode, Formats format = DefaultFormat) const;

signals:
    void framerateChanged();
    void defaultFormatChanged();

private:
    QString formatHH_MM_SS_FF(const Timecode &timecode) const;
    QString formatHH_MM_SS_HH(const Timecode &timecode) const;
    QString formatFrames(const Timecode &timecode) const;
    QString formatSeconds(const Timecode &timecode) const;
    QString formatMilliseconds(const Timecode &timecode) const;
    QString formatTime(const Timecode &timecode) const;

    Timecode fromHH_MM_SS_FF(const QString &timecode) const;
    Timecode fromHH_MM_SS_HH(const QString &timecode) const;
    Timecode fromFrames(const QString &timecode) const;
    Timecode fromSeconds(const QString &timecode) const;
    Timecode fromMilliseconds(const QString &timecode) const;
    Timecode fromTime(const QString &timecode) const;

    void fillComponents(int factor, int &lastComponent, int &seconds, int &minutes, int &hours) const;
    QString fillMask(int hours, int minutes, int seconds, int lastComponent, bool isNegative = false) const;

    Formats m_defaultFormat;
    Fraction m_framerate;

    bool m_isDropFrame;
    int m_dropFrames;
    int m_framesPer10Minutes;
};

#endif
