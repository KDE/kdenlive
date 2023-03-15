/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractpythoninterface.h"

#include <QObject>
#include <QProcess>

class SpeechToText: public AbstractPythonInterface
{
    Q_OBJECT
public:
    enum class EngineType { EngineVosk = 0, EngineWhisper = 1 };
    SpeechToText(EngineType engineType = EngineType::EngineVosk, QObject *parent = nullptr);
    QString runSubtitleScript(QString modelDirectory, QString language, QString audio, QString speech);
    QString subtitleScript();
    QString speechScript();
    QString voskModelPath();
    QStringList parseVoskDictionaries();
    static QList<std::pair<QString, QString>> whisperModels();
    static QMap<QString, QString> whisperLanguages();

protected:
    QString featureName() override;

private:
    EngineType m_engineType;

public Q_SLOTS:

Q_SIGNALS:
    void subtitleProgressUpdate(int);
    void subtitleFinished(int exitCode, QProcess::ExitStatus exitStatus);

};
