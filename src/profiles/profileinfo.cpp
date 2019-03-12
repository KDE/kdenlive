/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/

#include "profileinfo.hpp"
#include <klocalizedstring.h>
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
           other.progressive() == progressive() && other.sample_aspect_num() * 100 / other.sample_aspect_den() == sar &&
           other.display_aspect_num() * 100 / other.display_aspect_den() == dar && other.colorspace() == colorspace();
}

bool ProfileInfo::isCompatible(std::unique_ptr<ProfileInfo> &other) const
{
    return frame_rate_num() * 100 / frame_rate_den() == other->frame_rate_num() * 100 / other->frame_rate_den();
}

bool ProfileInfo::isCompatible(Mlt::Profile *other) const
{
    return frame_rate_num() * 100 / frame_rate_den() == other->frame_rate_num() * 100 / other->frame_rate_den();
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
        fps_str = QString::number((double)frame_rate_num() / frame_rate_den(), 'f', 2);
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
        text = QString::number((double)frame_rate_num() / frame_rate_den(), 'f', 2);
    }
    text.append(i18nc("frames per second", "fps"));
    if (!progressive()) {
        text.append(i18n(" interlaced"));
    }
    return text;
}
