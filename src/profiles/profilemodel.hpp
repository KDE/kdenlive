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

#ifndef PROFILEMODEL_H
#define PROFILEMODEL_H

#include "mlt++/MltProfile.h"
#include <QString>
#include <memory>


/* @brief This is a virtual class that represents any profile that we can get info from
 */
class ProfileInfo
{
public:
    ProfileInfo() = default;
    virtual ~ProfileInfo() = default;

    virtual bool is_valid() const = 0;
    virtual QString description() const = 0;
    virtual int frame_rate_num() const = 0;
    virtual int frame_rate_den() const = 0;
    virtual double fps() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual bool progressive() const = 0;
    virtual int sample_aspect_num() const = 0;
    virtual int sample_aspect_den() const = 0;
    virtual double sar() const = 0;
    virtual int display_aspect_num() const = 0;
    virtual int display_aspect_den() const = 0;
    virtual double dar() const = 0;
    virtual int is_explicit() const = 0;
    virtual void set_explicit(int b) = 0;
    virtual int colorspace() const = 0;
    QString colorspaceDescription() const;
    virtual QString path() const = 0;

    /* @brief overload of comparison operators */
    bool operator==(const ProfileInfo &other) const;
    bool operator!=(const ProfileInfo &other) const;

    /** @brief Returns true if both profiles have same fps, and can be mixed with the xml producer */
    bool isCompatible(std::unique_ptr<ProfileInfo> &other) const;
    bool isCompatible(Mlt::Profile *other) const;
};



/** @brief This is a wrapper around Mlt::Profile to be used by the rest of kdenlive.
 *  It has implicit conversion to Mlt::Profile so you can use it directly in calls to Mlt backend.
 *
 */

class ProfileModel : public ProfileInfo
{

public:
    ProfileModel() = delete;

    /* @brief Constructs a profile using the path to the profile description
     */
    ProfileModel(const QString &path);
    virtual ~ProfileModel() = default;

    bool is_valid() const override;
    QString description() const override;
    int frame_rate_num() const override;
    int frame_rate_den() const override;
    double fps() const override;
    int width() const override;
    int height() const override;
    bool progressive() const override;
    int sample_aspect_num() const override;
    int sample_aspect_den() const override;
    double sar() const override;
    int display_aspect_num() const override;
    int display_aspect_den() const override;
    double dar() const override;
    int is_explicit() const override;
    void set_explicit(int b) override;
    int colorspace() const override;
    QString path() const override;


    /* @brief get underlying profile. Use with caution*/
    Mlt::Profile &profile() { return *m_profile.get(); };

protected:
    QString m_path;
    bool m_invalid;
    QString m_description;

    std::unique_ptr<Mlt::Profile> m_profile;
};

#endif
