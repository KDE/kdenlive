/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractpythoninterface.h"
#include "definitions.h"

#include <QObject>
#include <QProcess>

class SpeechToText: public AbstractPythonInterface
{
    Q_OBJECT
public:
    SpeechToText(SpeechToTextEngine::EngineType engineType = SpeechToTextEngine::EngineNone, QObject *parent = nullptr);
    QString runSubtitleScript(QString modelDirectory, QString language, QString audio, QString speech);
    SpeechToTextEngine::EngineType engineType() const;
    virtual QString subtitleScript();
    virtual QString speechScript();
    virtual QMap<QString, QString> speechLanguages();
    virtual const QString modelFolder(bool mainFolder = true, bool create = false) = 0;
    virtual const QStringList getInstalledModels() = 0;
    virtual bool installNewModel(const QString &modelName = QString()) = 0;

protected:
    QString featureName() override;
    SpeechToTextEngine::EngineType m_engineType;

public Q_SLOTS:

Q_SIGNALS:
    void subtitleProgressUpdate(int);
    void subtitleFinished(int exitCode, QProcess::ExitStatus exitStatus);

};
