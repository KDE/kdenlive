/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "profileinfo.hpp"
#include <QDomElement>
#include <QString>
#include <memory>

#include <mlt++/MltProfile.h>

/** @class ProfileModel
    @brief This is a wrapper around Mlt::Profile to be used by the rest of kdenlive.
    It has implicit conversion to Mlt::Profile so you can use it directly in calls to Mlt backend.
  */
class ProfileModel : public ProfileInfo
{

public:
    ProfileModel() = delete;

    /** @brief Constructs a profile using the path to the profile description
     */
    ProfileModel(const QString &path);
    ~ProfileModel() override = default;

    bool is_valid() const override;
    QString description() const override;
    int frame_rate_num() const override;
    int frame_rate_den() const override;
    double fps() const override;
    int width() const override;
    int height() const override;
    bool progressive() const override;
    bool bottom_field_first() const override;
    int sample_aspect_num() const override;
    int sample_aspect_den() const override;
    double sar() const override;
    int display_aspect_num() const override;
    int display_aspect_den() const override;
    double dar() const override;
    int is_explicit() const;
    void set_explicit(int b);
    int colorspace() const override;
    mlt_profile get_profile() const;
    QString path() const override;

    void adjustDimensions() override{};

    /** @brief get underlying profile. Use with caution*/
    Mlt::Profile &profile() { return *m_profile.get(); };

protected:
    QString m_path;
    bool m_invalid;
    QString m_description;
    bool m_bottom_field_first;

    std::unique_ptr<Mlt::Profile> m_profile;
};

/** @class ProfileParam
    @brief This class serves to describe the parameters of a profile
 */
class ProfileParam : public ProfileInfo
{
public:
    ProfileParam() = delete;

    ProfileParam(QDomElement element);
    ProfileParam(ProfileInfo *p);
    ProfileParam(Mlt::Profile *p);
    ProfileParam(ProfileParam *p);

    QString path() const override;
    QString description() const override;
    int frame_rate_num() const override;
    int frame_rate_den() const override;
    int width() const override;
    int height() const override;
    bool progressive() const override;
    bool bottom_field_first() const override;
    int sample_aspect_num() const override;
    int sample_aspect_den() const override;
    int display_aspect_num() const override;
    int display_aspect_den() const override;
    int colorspace() const override;
    double fps() const override;
    double sar() const override;
    double dar() const override;

    // A profile's width should always be a multiple of 8
    void adjustDimensions() override;
    bool is_valid() const override;

    QString m_path;
    QString m_description;
    int m_frame_rate_num;
    int m_frame_rate_den;
    int m_width;
    int m_height;
    bool m_progressive;
    bool m_bottom_field_first;
    int m_sample_aspect_num;
    int m_sample_aspect_den;
    int m_display_aspect_num;
    int m_display_aspect_den;
    int m_colorspace;
    double m_fps;
    double m_sar;
    double m_dar;
};
