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
#include "profilemodel.hpp"
#include "kdenlive_debug.h"
#include "kdenlivesettings.h"
#include <algorithm>
#include <QDir>
#include <QStandardPaths>
#include <mlt++/MltProfile.h>
#include <KLocalizedString>


std::unique_ptr<ProfileRepository> ProfileRepository::instance;
std::once_flag ProfileRepository::m_onceFlag;

ProfileRepository::ProfileRepository()
{
    refresh();
}

std::unique_ptr<ProfileRepository> & ProfileRepository::get()
{
    std::call_once(m_onceFlag, []{instance.reset(new ProfileRepository());});
    return instance;
}

void ProfileRepository::refresh()
{
    QWriteLocker locker(&m_mutex);

    //Helper function to check a profile and print debug info
    auto check_profile = [&](std::unique_ptr<ProfileModel>& profile, const QString& file) {
        if (m_profiles.count(file) > 0) {
            return false;
        }
        if (!profile->is_valid()) {
            qCWarning(KDENLIVE_LOG) << "//// WARNING: invalid profile found: "<<file<<". Ignoring.";
            return false;
        }
        return true;
    };

    //list MLT profiles.
    QDir mltDir(KdenliveSettings::mltpath());
    QStringList profilesFiles = mltDir.entryList(QDir::Files);

    //list Custom Profiles
    QStringList customProfilesDir = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("profiles/"), QStandardPaths::LocateDirectory);
    for (const auto& dir : customProfilesDir) {
        QStringList files = QDir(dir).entryList(QDir::Files);
        for (const auto& file : files) {
            profilesFiles << QDir(dir).absoluteFilePath(file);
        }
    }

    //Iterate through files
    for (const auto& file : profilesFiles) {
        std::unique_ptr<ProfileModel> profile(new ProfileModel(file));
        if (check_profile(profile, file)) {
            m_profiles.insert(std::make_pair(file, std::move(profile)));
        }
    }

}


QVector<QPair<QString, QString> > ProfileRepository::getAllProfiles()
{
    QReadLocker locker(&m_mutex);

    QVector<QPair<QString, QString> > list;
    for (const auto& profile : m_profiles) {
        list.push_back({profile.second->description(), profile.first});
    }
    std::sort(list.begin(), list.end());
    return list;
}

std::unique_ptr<ProfileModel> & ProfileRepository::getProfile(const QString& path)
{
    QReadLocker locker(&m_mutex);

    if (m_profiles.count(path) == 0) {
        qCWarning(KDENLIVE_LOG) << "//// WARNING: profile not found: "<<path<<". Returning default profile instead.";
        return getProfile(KdenliveSettings::default_profile());
    }

    return m_profiles.at(path);
}

bool ProfileRepository::profileExists(const QString& path)
{
    QReadLocker locker(&m_mutex);
    return m_profiles.count(path) != 0;
}

//static
QString ProfileRepository::getColorspaceDescription(int colorspace)
{
    //TODO: should the descriptions be translated?
    switch (colorspace) {
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

QVector<double> ProfileRepository::getAllFps()
{
    QVector<double> res;
    for (const auto& ptr : m_profiles) {
        res.push_back(ptr.second->fps());
    }
    std::sort(res.begin(), res.end());
    res.erase(std::unique(res.begin(), res.end()), res.end());
    return res;
}
