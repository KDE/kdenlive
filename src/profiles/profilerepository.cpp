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
 ***************************************************************************/

#include "profilerepository.hpp"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include "profilemodel.hpp"
#include <KLocalizedString>
#include <KMessageBox>
#include <QDir>
#include <QStandardPaths>
#include <algorithm>
#include <mlt++/MltProfile.h>

std::unique_ptr<ProfileRepository> ProfileRepository::instance;
std::once_flag ProfileRepository::m_onceFlag;
std::vector<std::pair<int, QString>> ProfileRepository::colorProfiles{
    {601, QStringLiteral("ITU-R 601")}, {709, QStringLiteral("ITU-R 709")}, {240, QStringLiteral("SMPTE240M")}};

ProfileRepository::ProfileRepository()
{
    refresh();
}

std::unique_ptr<ProfileRepository> &ProfileRepository::get()
{
    std::call_once(m_onceFlag, [] { instance.reset(new ProfileRepository()); });
    return instance;
}

void ProfileRepository::refresh(bool fullRefresh)
{
    QWriteLocker locker(&m_mutex);

    if (fullRefresh) {
        // Reset all profiles
        m_profiles.clear();
    }

    // Helper function to check a profile and print debug info
    auto check_profile = [&](std::unique_ptr<ProfileModel> &profile, const QString &file) {
        if (m_profiles.count(file) > 0) {
            return false;
        }
        if (!profile->is_valid()) {
            qCWarning(KDENLIVE_LOG) << "//// WARNING: invalid profile found: " << file << ". Ignoring.";
            return false;
        }
        return true;
    };

    // list MLT profiles.
    QDir mltDir(KdenliveSettings::mltpath());
    QStringList profilesFiles = mltDir.entryList(QDir::Files);

    // list Custom Profiles
    QStringList customProfilesDir = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("profiles/"), QStandardPaths::LocateDirectory);
    for (const auto &dir : qAsConst(customProfilesDir)) {
        QStringList files = QDir(dir).entryList(QDir::Files);
        for (const auto &file : qAsConst(files)) {
            profilesFiles << QDir(dir).absoluteFilePath(file);
        }
    }

    // Iterate through files
    for (const auto &file : qAsConst(profilesFiles)) {
        std::unique_ptr<ProfileModel> profile(new ProfileModel(file));
        if (check_profile(profile, file)) {
            m_profiles.insert(std::make_pair(file, std::move(profile)));
        }
    }
}

QVector<QPair<QString, QString>> ProfileRepository::getAllProfiles() const
{
    QReadLocker locker(&m_mutex);

    QVector<QPair<QString, QString>> list;
    for (const auto &profile : m_profiles) {
        list.push_back({profile.second->description(), profile.first});
    }
    std::sort(list.begin(), list.end());
    return list;
}

std::unique_ptr<ProfileModel> &ProfileRepository::getProfile(const QString &path)
{
    QReadLocker locker(&m_mutex);
    if (m_profiles.count(path) == 0) {
        // qCWarning(KDENLIVE_LOG) << "//// WARNING: profile not found: " << path << ". Returning default profile instead.";
        QString default_profile = KdenliveSettings::default_profile();
        if (m_profiles.count(default_profile) == 0) {
            qCWarning(KDENLIVE_LOG) << "//// WARNING: default profile not found: " << default_profile << ". Returning random profile instead.";
            return (*(m_profiles.begin())).second;
        }
        return m_profiles.at(default_profile);
    }
    return m_profiles.at(path);
}

bool ProfileRepository::profileExists(const QString &path) const
{
    QReadLocker locker(&m_mutex);
    return m_profiles.count(path) != 0;
}

// static
QString ProfileRepository::getColorspaceDescription(int colorspace)
{
    // TODO: should the descriptions be translated?
    for (const auto &cs : colorProfiles) {
        if (cs.first == colorspace) return cs.second;
    }
    return i18n("Unknown");
}

// static
int ProfileRepository::getColorspaceFromDescription(const QString &description)
{
    for (const auto &cs : colorProfiles) {
        if (cs.second == description) return cs.first;
    }
    return 0;
}

QVector<double> ProfileRepository::getAllFps() const
{
    QReadLocker locker(&m_mutex);
    QVector<double> res;
    for (const auto &ptr : m_profiles) {
        res.push_back(ptr.second->fps());
    }
    std::sort(res.begin(), res.end());
    res.erase(std::unique(res.begin(), res.end()), res.end());
    return res;
}

QString ProfileRepository::findMatchingProfile(ProfileInfo *profile) const
{
    QReadLocker locker(&m_mutex);
    for (const auto &ptr : m_profiles) {
        if (*ptr.second.get() == *profile) {
            return ptr.first;
        }
    }
    return QString();
}

const QString ProfileRepository::saveProfile(ProfileInfo *profile, QString profilePath)
{
    if (profilePath.isEmpty()) {
        int i = 0;
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/profiles/"));
        if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
        }
        QString customName = QStringLiteral("customprofile");
        profilePath = dir.absoluteFilePath(customName + QString::number(i));
        while (QFile::exists(profilePath)) {
            ++i;
            profilePath = dir.absoluteFilePath(customName + QString::number(i));
        }
    }
    QFile file(profilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        KMessageBox::sorry(nullptr, i18n("Cannot open file %1", profilePath));
        return QString();
    }
    QTextStream out(&file);
    out << "description=" << profile->description() << '\n'
        << "frame_rate_num=" << profile->frame_rate_num() << '\n'
        << "frame_rate_den=" << profile->frame_rate_den() << '\n'
        << "width=" << profile->width() << '\n'
        << "height=" << profile->height() << '\n'
        << "progressive=" << static_cast<int>(profile->progressive()) << '\n'
        << "sample_aspect_num=" << profile->sample_aspect_num() << '\n'
        << "sample_aspect_den=" << profile->sample_aspect_den() << '\n'
        << "display_aspect_num=" << profile->display_aspect_num() << '\n'
        << "display_aspect_den=" << profile->display_aspect_den() << '\n'
        << "colorspace=" << profile->colorspace() << '\n';
    if (file.error() != QFile::NoError) {
        KMessageBox::error(nullptr, i18n("Cannot write to file %1", profilePath));
        profilePath.clear();
    }
    file.close();
    refresh(false);
    return profilePath;
}

bool ProfileRepository::deleteProfile(const QString &path)
{
    bool success = false;
    if (path.contains(QLatin1Char('/'))) {
        success = QFile::remove(path);
    }
    if (!success) {
        qCDebug(KDENLIVE_LOG) << "//// Cannot delete profile " << path << ", does not seem to be custom one";
    }
    refresh(true);
    return success;
}
