/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "speechtotext.h"

#include <QObject>
#include <QProcess>

class SpeechToTextVosk : public SpeechToText
{
    Q_OBJECT
public:
    SpeechToTextVosk(QObject *parent = nullptr);
    QString runSubtitleScript(QString modelDirectory, QString language, QString audio, QString speech);
    QString subtitleScript() override;
    const QString modelFolder() override;
    QString speechScript() override;
    QString voskModelPath();
    const QStringList getInstalledModels() override;
    bool installNewModel(const QString &modelName = QString()) override;
    QMap<QString, QString> whisperLanguages();

public Q_SLOTS:

Q_SIGNALS:
    void subtitleProgressUpdate(int);
    void subtitleFinished(int exitCode, QProcess::ExitStatus exitStatus);
};
