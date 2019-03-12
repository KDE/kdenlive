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

#ifndef PROFILEINFO_H
#define PROFILEINFO_H

#include <QString>
#include <memory>

namespace Mlt {
class Profile;
}

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
    virtual int colorspace() const = 0;
    QString colorspaceDescription() const;
    virtual QString path() const = 0;

    /* @brief overload of comparison operators */
    bool operator==(const ProfileInfo &other) const;
    bool operator!=(const ProfileInfo &other) const;

    /** @brief Returns true if both profiles have same fps, and can be mixed with the xml producer */
    bool isCompatible(std::unique_ptr<ProfileInfo> &other) const;
    bool isCompatible(Mlt::Profile *other) const;

    virtual void adjustDimensions() = 0;

    const QString descriptiveString() const;
    const QString dialogDescriptiveString() const;
};

#endif
