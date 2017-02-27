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

#include <memory>
#include <QString>


/** @brief This is a wrapper around Mlt::Profile to be used by the rest of kdenlive.
 *  It has implicit conversion to Mlt::Profile so you can use it directly in calls to Mlt backend.
 *
 */

namespace Mlt{
    class Profile;
}

class ProfileModel
{

public:
    ProfileModel() = delete;

    /* @brief Constructs a profile using the path to the profile description
     */
    ProfileModel(const QString& path);

    bool is_valid( ) const;
    QString description() const;
    int frame_rate_num() const;
    int frame_rate_den() const;
    double fps() const;
    int width() const;
    int height() const;
    bool progressive() const;
    int sample_aspect_num() const;
    int sample_aspect_den() const;
    double sar() const;
    int display_aspect_num() const;
    int display_aspect_den() const;
    double dar() const;
    int is_explicit() const;
    int colorspace() const;

protected:
    QString m_path;
    bool m_invalid;
    QString m_description;

    std::unique_ptr<Mlt::Profile> m_profile;

};


#endif
