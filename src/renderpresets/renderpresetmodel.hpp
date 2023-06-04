/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KLocalizedString>
#include <QDomElement>
#include <QString>
#include <memory>

#include <mlt++/MltProfile.h>

class RenderPresetParams : public QMap<QString, QString>
{
public:
    // the number of the enum entries maps to the index of the combo boxes in the preset edit dialog
    enum RateControl { Unknown = 0, Average, Constant, Quality, Constrained };

    QString toString();
    void replacePlaceholder(const QString &placeholder, const QString &newValue);
    void refreshX265Params();
    RateControl videoRateControl() const;
    bool hasAlpha();
    bool isImageSequence();
};

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
    RenderPresetModel(const QString &name, const QString &groupName, const QString &params, const QString &extension, const QString &defaultVBitrate,
                      const QString &defaultVQuality, const QString &defaultABitrate, const QString &defaultAQuality, const QString &speedsString,
                      bool manualPreset);

    enum InstallType { BuildIn, Custom, Download };

    QDomElement toXml();

    QString name() const { return m_name; };
    QString note() const { return m_note; }
    QString standard() const { return m_standard; };
    RenderPresetParams params(QStringList removeParams = {}) const;
    QString extension() const;
    QString groupName() const { return m_groupName; };
    QString renderer() const { return m_renderer; };
    QString url() const;
    QStringList speeds() const;
    int defaultSpeedIndex() const { return m_defaultSpeedIndex; };
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
    bool isManual() const;

    QString getParam(const QString &name) const;
    bool hasParam(const QString &name) const;
    RenderPresetParams::RateControl audioRateControl() const;
    InstallType installType() const;
    bool hasFixedSize() const;
    QString error() const;
    QString warning() const;
    int estimateFileSize(int length);

private:
    void setParams(const QString &params);
    void checkPreset();

    QString m_presetFile;
    bool m_editable;
    QString m_name;
    QString m_note;
    QString m_standard;
    bool m_manual;
    RenderPresetParams m_params;
    QString m_extension;
    QString m_groupName;
    QString m_renderer;
    QString m_url;
    QString m_speeds;
    int m_defaultSpeedIndex;
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
