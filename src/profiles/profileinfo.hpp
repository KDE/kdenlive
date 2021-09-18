/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef PROFILEINFO_H
#define PROFILEINFO_H

#include <QString>
#include <memory>

namespace Mlt {
class Profile;
}

/** @brief This is a virtual class that represents any profile that we can get info from
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

    /** @brief overload of comparison operators */
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
