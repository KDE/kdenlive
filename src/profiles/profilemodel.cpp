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
#include "core.h"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include <KLocalizedString>

#include <KMessageWidget>
#include <QDir>
#include <QFile>
#include <memory>
ProfileModel::ProfileModel(const QString &path)
    : m_path(path)
    , m_invalid(false)
{
    if (!QFile::exists(path) && path.contains(QLatin1Char('/'))) {
        qCWarning(KDENLIVE_LOG) << "WARNING, COULD NOT FIND PROFILE " << path << ". We will default to DV_PAL profile";
        m_invalid = true;
    }
    if (!path.contains(QLatin1Char('/'))) {
        QDir mltDir(KdenliveSettings::mltpath());
        if (!mltDir.exists(path)) {
            qCWarning(KDENLIVE_LOG) << "WARNING, COULD NOT FIND MLT PROFILE " << path << ". We will default to DV_PAL profile";
            m_invalid = true;
        }
    }
    m_profile = std::make_unique<Mlt::Profile>(path.toStdString().c_str());
    m_description = QString(m_profile->description());
}

bool ProfileModel::is_valid() const
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

QString ProfileModel::path() const
{
    return m_path;
}

mlt_profile ProfileModel::get_profile() const
{
    return m_profile->get_profile();
}

void ProfileModel::set_explicit(int b)
{
    m_profile->set_explicit(b);
}

ProfileParam::ProfileParam(QDomElement element)
    : m_description(element.attribute(QStringLiteral("description")))
    , m_frame_rate_num(element.attribute(QStringLiteral("frame_rate_num")).toInt())
    , m_frame_rate_den(element.attribute(QStringLiteral("frame_rate_den")).toInt())
    , m_progressive((element.attribute(QStringLiteral("progressive")).toInt() != 0))
    , m_sample_aspect_num(element.attribute(QStringLiteral("sample_aspect_num")).toInt())
    , m_sample_aspect_den(element.attribute(QStringLiteral("sample_aspect_den")).toInt())
    , m_display_aspect_num(element.attribute(QStringLiteral("display_aspect_num")).toInt())
    , m_display_aspect_den(element.attribute(QStringLiteral("display_aspect_den")).toInt())
    , m_colorspace(element.attribute(QStringLiteral("colorspace")).toInt())
{
    // Ensure profile has viable width / height
    int width = element.attribute(QStringLiteral("width")).toInt();
    int height = element.attribute(QStringLiteral("height")).toInt();
    if ((width % 8) + (height % 2) > 0) {
        pCore->displayBinMessage(
            i18n("The project profile is invalid (%1x%2), it was adjusted to %3x%4.", width, height, width + (width % 8), height + (height % 2)),
            KMessageWidget::Warning);
        if (width % 8 > 0) {
            width += 8 - width % 8;
        }
        height += height % 2;
        element.setAttribute(QStringLiteral("width"), width);
        element.setAttribute(QStringLiteral("height"), height);
    }
    m_width = width;
    m_height = height;
    m_fps = m_frame_rate_num / m_frame_rate_den;
    m_sar = m_sample_aspect_num / m_sample_aspect_den;
    m_dar = m_display_aspect_num / m_display_aspect_den;
}

ProfileParam::ProfileParam(ProfileInfo *p)
    : m_frame_rate_num(p->frame_rate_num())
    , m_frame_rate_den(p->frame_rate_den())
    , m_width(p->width())
    , m_height(p->height())
    , m_progressive(p->progressive())
    , m_sample_aspect_num(p->sample_aspect_num())
    , m_sample_aspect_den(p->sample_aspect_den())
    , m_display_aspect_num(p->display_aspect_num())
    , m_display_aspect_den(p->display_aspect_den())
    , m_colorspace(p->colorspace())
    , m_fps(p->fps())
    , m_sar(p->sar())
    , m_dar(p->dar())
{
}

ProfileParam::ProfileParam(Mlt::Profile *p)
    : m_frame_rate_num(p->frame_rate_num())
    , m_frame_rate_den(p->frame_rate_den())
    , m_width(p->width())
    , m_height(p->height())
    , m_progressive(p->progressive())
    , m_sample_aspect_num(p->sample_aspect_num())
    , m_sample_aspect_den(p->sample_aspect_den())
    , m_display_aspect_num(p->display_aspect_num())
    , m_display_aspect_den(p->display_aspect_den())
    , m_colorspace(p->colorspace())
    , m_fps(p->fps())
    , m_sar(p->sar())
    , m_dar(p->dar())
{
}
QString ProfileParam::path() const
{
    return m_path;
}
QString ProfileParam::description() const
{
    return m_description;
}
int ProfileParam::frame_rate_num() const
{
    return m_frame_rate_num;
}
int ProfileParam::frame_rate_den() const
{
    return m_frame_rate_den;
}
int ProfileParam::width() const
{
    return m_width;
}
int ProfileParam::height() const
{
    return m_height;
}
bool ProfileParam::progressive() const
{
    return m_progressive;
}
int ProfileParam::sample_aspect_num() const
{
    return m_sample_aspect_num;
}
int ProfileParam::sample_aspect_den() const
{
    return m_sample_aspect_den;
}
int ProfileParam::display_aspect_num() const
{
    return m_display_aspect_num;
}
int ProfileParam::display_aspect_den() const
{
    return m_display_aspect_den;
}
int ProfileParam::colorspace() const
{
    return m_colorspace;
}

double ProfileParam::fps() const
{
    return m_fps;
}
double ProfileParam::dar() const
{
    return m_dar;
}
double ProfileParam::sar() const
{
    return m_sar;
}
void ProfileParam::adjustDimensions()
{
    if (m_width % 8 > 0) {
        m_width += 8 - m_width % 8;
    }
    m_height += m_height % 2;
}

bool ProfileParam::is_valid() const
{
    return (m_frame_rate_den > 0 && m_sample_aspect_den > 0 && m_display_aspect_den > 0 && m_width > 0);
}
