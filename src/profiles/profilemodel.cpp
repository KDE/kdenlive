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

#include "profilemodel.hpp"
#include <mlt++/MltProfile.h>
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include <KLocalizedString>

#include <QFile>
#include <QDir>

ProfileModel::ProfileModel(const QString& path):
    m_path(path)
    , m_invalid(false)
{
    if (!QFile::exists(path) && path.contains(QLatin1Char('/'))) {
        qCWarning(KDENLIVE_LOG) << "WARNING, COULD NOT FIND PROFILE " << path
                              << ". We will default to DV_PAL profile";
        m_invalid = true;
    }
    if (!path.contains(QLatin1Char('/'))) {
        QDir mltDir(KdenliveSettings::mltpath());
        if (!mltDir.exists(path)) {
            qCWarning(KDENLIVE_LOG) << "WARNING, COULD NOT FIND MLT PROFILE " << path
                                    << ". We will default to DV_PAL profile";
            m_invalid = true;
        }
    }
    m_profile = std::unique_ptr<Mlt::Profile>(new Mlt::Profile(path.toStdString().c_str()));
    m_description = QString(m_profile->description());
}

bool ProfileModel::is_valid( ) const
{
    return (!m_invalid) && m_profile->is_valid();
}

QString ProfileModel::description() const
{
    return m_profile->description();
}

int ProfileModel::frame_rate_num() const
{
    return m_profile->frame_rate_num();
}

int ProfileModel::frame_rate_den() const
{
    return m_profile->frame_rate_den();
}

double ProfileModel::fps() const
{
    return m_profile->fps();
}

int ProfileModel::width() const
{
    return m_profile->width();
}

int ProfileModel::height() const
{
    return m_profile->height();
}

bool ProfileModel::progressive() const
{
    return m_profile->progressive();
}

int ProfileModel::sample_aspect_num() const
{
    return m_profile->sample_aspect_num();
}

int ProfileModel::sample_aspect_den() const
{
    return m_profile->sample_aspect_den();
}

double ProfileModel::sar() const
{
    return m_profile->sar();
}

int ProfileModel::display_aspect_num() const
{
    return m_profile->display_aspect_num();
}

int ProfileModel::display_aspect_den() const
{
    return m_profile->display_aspect_den();
}

double ProfileModel::dar() const
{
    return m_profile->dar();
}

int ProfileModel::is_explicit() const
{
    return m_profile->is_explicit();
}

int ProfileModel::colorspace() const
{
    return m_profile->colorspace();
}


bool ProfileModel::operator==(const ProfileModel &other) const
{
    if (!description().isEmpty() && other.description()  == description()) {
        return true;
    }
    int fps = frame_rate_num() * 100 / frame_rate_den();
    int sar = sample_aspect_num() * 100 / sample_aspect_den();
    int dar = display_aspect_num() * 100 / display_aspect_den();
    return      other.frame_rate_num() * 100 / other.frame_rate_den() == fps &&
        other.width() == width() &&
        other.height() == height() &&
        other.progressive() == progressive() &&
        other.sample_aspect_num() * 100 / other.sample_aspect_den() == sar &&
        other.display_aspect_num() * 100 / other.display_aspect_den() == dar &&
        other.colorspace() == colorspace();
}

QString ProfileModel::path() const
{
    return m_path;
}

QString ProfileModel::colorspaceDescription() const
{
    //TODO: should the descriptions be translated?
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
