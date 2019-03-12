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

#ifndef PROFILEREPOSITORY_H
#define PROFILEREPOSITORY_H

#include "definitions.h" //for QString hash function
#include "profileinfo.hpp"
#include <QReadWriteLock>
#include <QString>
#include <memory>
#include <mutex>
#include <unordered_map>

/** @brief This class is used to read all the profiles available to the user (MLT defaults one and Custom ones).
 * You can then query profiles based on their paths
 * Note that this class is a Singleton, with Mutex protections to allow concurrent access.
 */

class ProfileModel;

class ProfileRepository
{

public:
    // Returns the instance of the Singleton
    static std::unique_ptr<ProfileRepository> &get();

    /* @brief Reloads all the profiles from the disk */
    void refresh(bool fullRefresh = false);

    /* @brief Returns a list of all the pairs (description, path) of all the profiles loaded */
    QVector<QPair<QString, QString>> getAllProfiles() const;

    /* @brief Returns a profile model given the profile's @param path
     */
    std::unique_ptr<ProfileModel> &getProfile(const QString &path);

    /* @brief Returns true if the given profile exists in repository
     */
    bool profileExists(const QString &path) const;

    /* @brief Find a profile that match the parameters of the given one and return its path. If not found, returns empty string */
    QString findMatchingProfile(ProfileInfo *profile) const;

    /** @brief Get the descriptive text for given colorspace code (defined by MLT)
     *  @param colorspace An int as defined in mlt_profile.h
     *  @return The string description */
    static QString getColorspaceDescription(int colorspace);
    /* @brief This is the opposite function */
    static int getColorspaceFromDescription(const QString &description);

    /** @brief Returns all the possible fps of the profiles in the repository*/
    QVector<double> getAllFps() const;

    /** @brief Saves given profile as custom one. If the path is left empty, it will be set to the standard custom_profile directory
     *  @returns The path of the saved folder
     */
    const QString saveProfile(ProfileInfo *profile, QString profilePath = QString());

    /** @brief Delete a (custom) profile*/
    bool deleteProfile(const QString &path);

protected:
    // Constructor is protected because class is a Singleton
    ProfileRepository();

    static std::unique_ptr<ProfileRepository> instance;
    static std::once_flag m_onceFlag; // flag to create the repository only once;

    static std::vector<std::pair<int, QString>> colorProfiles;

    mutable QReadWriteLock m_mutex;

    std::unordered_map<QString, std::unique_ptr<ProfileModel>> m_profiles; // map from the profile path to the instance of the profile. We use unordered_map
                                                                           // because QMap and QHash currently don't support move insertion, hence inserting
                                                                           // unique_ptr is impossible.
};

#endif
