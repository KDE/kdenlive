/*
   SPDX-FileCopyrightText: 2017 Nicolas Carion
   SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
   This file is part of Kdenlive. See www.kdenlive.org.
*/

#include "profileinfo.hpp"
#include <KLocalizedString>

#include <cmath>
#include <mlt++/MltProfile.h>

bool ProfileInfo::operator==(const ProfileInfo &other) const
{
    if (!description().isEmpty() && other.description() == description()) {
        return true;
    }
    int fps = frame_rate_num() * 100 / frame_rate_den();
    int sar = sample_aspect_num() * 100 / sample_aspect_den();
    int dar = display_aspect_num() * 100 / display_aspect_den();
    return other.frame_rate_num() * 100 / other.frame_rate_den() == fps && other.width() == width() && other.height() == height() &&
           other.progressive() == progressive() && (progressive() ? true : other.bottom_field_first() == bottom_field_first()) &&
           other.sample_aspect_num() * 100 / other.sample_aspect_den() == sar && other.display_aspect_num() * 100 / other.display_aspect_den() == dar &&
           other.colorspace() == colorspace();
}

bool ProfileInfo::isCompatible(std::unique_ptr<ProfileInfo> &other) const
{
    return frame_rate_num() * 100 / frame_rate_den() == other->frame_rate_num() * 100 / other->frame_rate_den();
}

bool ProfileInfo::isCompatible(Mlt::Profile *other) const
{
    return frame_rate_num() * 100 / frame_rate_den() == other->frame_rate_num() * 100 / other->frame_rate_den();
}

bool ProfileInfo::hasValidFps() const
{
    double fps = double(frame_rate_num()) / frame_rate_den();
    double fps_int;
    double fps_frac = std::modf(fps, &fps_int);
    if (fps_frac > 0.) {
        // Check for 23.98, 29.97, 59.94
        bool validFps = false;
        if (qFuzzyCompare(fps_int, 23.0)) {
            if (qFuzzyCompare(fps, 23.98)) {
                validFps = true;
            }
        } else if (qFuzzyCompare(fps_int, 29.0)) {
            if (qFuzzyCompare(fps, 29.97)) {
                validFps = true;
            }
        } else if (qFuzzyCompare(fps_int, 59.0)) {
            if (qFuzzyCompare(fps, 59.94)) {
                validFps = true;
            }
        }
        return validFps;
    }
    // Integer fps
    return true;
}

QString ProfileInfo::colorspaceDescription() const
{
    // TODO: should the descriptions be translated?
    switch (colorspace()) {
    case 601:
        return QStringLiteral("ITU-R 601");
    case 709:
        return QStringLiteral("ITU-R 709");
    case 240:
        return QStringLiteral("SMPTE240M");
    default:
        return i18n("Unknown");
    }
}

const QString ProfileInfo::descriptiveString() const
{
    QString data = description();
    if (!data.isEmpty()) {
        data.append(QLatin1Char(' '));
    }
    QString fps_str;
    if (frame_rate_num() % frame_rate_den() == 0) {
        fps_str = QString::number(frame_rate_num() / frame_rate_den());
    } else {
        fps_str = QString::number(double(frame_rate_num()) / frame_rate_den(), 'f', 2);
    }
    data.append(QStringLiteral("(%1x%2, %3fps)").arg(width()).arg(height()).arg(fps_str));
    return data;
}

const QString ProfileInfo::dialogDescriptiveString() const
{
    QString text;
    if (frame_rate_num() % frame_rate_den() == 0) {
        text = QString::number(frame_rate_num() / frame_rate_den());
    } else {
        text = QString::number(frame_rate_num() / frame_rate_den(), 'f', 2);
    }
    text.append(i18nc("frames per second", "fps"));
    if (!progressive()) {
        text.append(i18n(" interlaced"));
    }
    return text;
}
