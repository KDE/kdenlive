/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "ui_transcodeseekable_ui.h"

#include <QUrl>
#include <QProcess>

class ProjectClip;

class TranscodeSeek : public QDialog, public Ui::TranscodeSeekable_UI
{
    Q_OBJECT

public:
    TranscodeSeek(bool onUserRequest, bool forceReplace, QWidget *parent = nullptr);
    ~TranscodeSeek() override;

    enum HWENCODINGFMT { None, Nvidia, Vaapi };
    struct TranscodeInfo
    {
        QString url;
        ClipType::ProducerType type;
        std::pair<int, int> fps_info;
        QString vCodec;
    };
    void addUrl(const QString &id, TranscodeSeek::TranscodeInfo info, const QString &suffix, const QString &message);
    QMap<QString,QStringList>  ids() const;
    QString params(const QString &cid) const;
    QString params(int clipType, std::pair<int, int> fps_info) const;
    QString preParams() const;
    TranscodeSeek::TranscodeInfo info(const QString &id) const;

private:
    QMap<QString, QString> m_encodeParams;
    QMap<QString, TranscodeInfo> m_clipInfos;
    bool profileMatches(const QString &profileString, HWENCODINGFMT fmt);
    HWENCODINGFMT formatForProfile(const QString &profileString);
};
