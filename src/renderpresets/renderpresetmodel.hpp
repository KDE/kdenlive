/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef RENDERPRESETMODEL_H
#define RENDERPRESETMODEL_H

#include <QDomElement>
#include <QString>
#include <memory>
#include <KLocalizedString>

#include <mlt++/MltProfile.h>

/** @class RenderPresetModel
    @brief This class serves to describe the parameters of a render preset
 */
class RenderPresetModel
{
public:
    RenderPresetModel() = delete;

    RenderPresetModel(QDomElement preset, const QString &presetFile, bool editable, const QString &groupName = QString(),
                       const QString &renderer = QStringLiteral("avformat"));
    RenderPresetModel(const QString &groupName, const QString &path, QString presetName, const QString &params, bool codecInName);
    RenderPresetModel(const QString &name, const QString &groupName, const QString &params, const QString &defaultVBitrate,
                       const QString &defaultVQuality, const QString &defaultABitrate, const QString &defaultAQuality, const QString &speeds);

    enum InstallType {
        BuildIn,
        Custom,
        Download
    };

    enum RateControl {
        Unknown = -1,
        Average = 0,
        Constant,
        Quality,
        Constrained
    };

    QDomElement toXml();

    QString name() const { return m_name; };
    QString note() const { return m_note; }
    QString standard() const { return m_standard; };
    QString params(QStringList removeParams = {}) const;
    QString extension() const;
    QString groupName() const { return m_groupName; };
    QString renderer() const { return m_renderer; };
    QString url() const;
    QStringList speeds() const;
    QString topFieldFirst() const { return m_topFieldFirst; };
    QString presetFile() const { return m_presetFile; };
    QStringList audioBitrates() const;
    QString defaultABitrate() const;
    QStringList audioQualities() const;
    QString defaultAQuality() const;
    QStringList videoBitrates() const;
    QString defaultVBitrate() const;
    QStringList videoQualities() const;
    QString defaultVQuality() const;
    bool editable() const;

    QString getParam(const QString &name) const;
    bool hasParam(const QString &name) const;
    RenderPresetModel::RateControl videoRateControl() const;
    RenderPresetModel::RateControl audioRateControl() const;
    QString x265Params() const;
    InstallType installType() const;
    bool hasFixedSize() const;
    QString error() const;
    QString warning() const;
    int estimateFileSize(int length);

private:

    void checkPreset();

    QString m_presetFile;
    bool m_editable;
    QString m_name;
    QString m_note;
    QString m_standard;
    QString m_params;
    QString m_extension;
    QString m_groupName;
    QString m_renderer;
    QString m_url;
    QString m_speeds;
    QString m_topFieldFirst;
    QString m_vBitrates;
    QString m_defaultVBitrate;
    QString m_vQualities;
    QString m_defaultVQuality;
    QString m_aBitrates;
    QString m_defaultABitrate;
    QString m_aQualities;
    QString m_defaultAQuality;

    QString m_errors;
    QString m_warnings;
};

#endif
