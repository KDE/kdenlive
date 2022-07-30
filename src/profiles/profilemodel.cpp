/*
   SPDX-FileCopyrightText: 2017 Nicolas Carion
   SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
   This file is part of Kdenlive. See www.kdenlive.org.
*/

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
    , m_bottom_field_first(false)
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
    QFile f(path);
    if (f.open(QFile::ReadOnly | QFile::Text)) {
        int lineCt = 0;
        QTextStream in(&f);
        while (!in.atEnd() && lineCt < 30) {
            QString line = in.readLine();
            if (line.contains(QStringLiteral("bottom_field_first"))) {
                m_bottom_field_first = line.split(QStringLiteral("=")).at(1).toInt() == 1;
                f.close();
            }
        }
        f.close();
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

bool ProfileModel::bottom_field_first() const
{
    return m_bottom_field_first;
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

void ProfileModel::setPath(const QString path)
{
    m_path = path;
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
    , m_progressive(element.attribute(QStringLiteral("progressive")).toInt() != 0)
    , m_bottom_field_first(element.attribute(QStringLiteral("bottom_field_first")).toInt() != 0)
    , m_sample_aspect_num(element.attribute(QStringLiteral("sample_aspect_num")).toInt())
    , m_sample_aspect_den(element.attribute(QStringLiteral("sample_aspect_den")).toInt())
    , m_display_aspect_num(element.attribute(QStringLiteral("display_aspect_num")).toInt())
    , m_display_aspect_den(element.attribute(QStringLiteral("display_aspect_den")).toInt())
    , m_colorspace(element.attribute(QStringLiteral("colorspace")).toInt())
{
    // Ensure profile has viable width / height
    int width = element.attribute(QStringLiteral("width")).toInt();
    int height = element.attribute(QStringLiteral("height")).toInt();
    if ((width % 2) + (height % 2) > 0) {
        pCore->displayBinMessage(
            i18n("The project profile is invalid (%1x%2), it was adjusted to %3x%4.", width, height, width + (width % 2), height + (height % 2)),
            KMessageWidget::Warning);
        width += width % 2;
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
    , m_bottom_field_first(p->bottom_field_first())
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

ProfileParam::ProfileParam(ProfileParam *p)
    : m_path(qstrdup(p->path().toUtf8().constData()))
    , m_description(qstrdup(p->description().toUtf8().constData()))
    , m_frame_rate_num(p->frame_rate_num())
    , m_frame_rate_den(p->frame_rate_den())
    , m_width(p->width())
    , m_height(p->height())
    , m_progressive(p->progressive())
    , m_bottom_field_first(p->bottom_field_first())
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
    , m_bottom_field_first(false)
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
bool ProfileParam::bottom_field_first() const
{
    return m_bottom_field_first;
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
    if (m_width % 2 > 0) {
        m_width += m_width % 2;
    }
    m_height += m_height % 2;
}

bool ProfileParam::is_valid() const
{
    return (m_frame_rate_den > 0 && m_sample_aspect_den > 0 && m_display_aspect_den > 0 && m_width > 0);
}
