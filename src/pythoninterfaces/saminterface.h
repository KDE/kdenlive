/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractpythoninterface.h"
#include "definitions.h"

#include <QObject>
#include <QProcess>

class SamInterface : public AbstractPythonInterface
{
    Q_OBJECT
public:
    SamInterface(QObject *parent = nullptr);
    QString runSubtitleScript(QString modelDirectory, QString language, QString audio, QString speech);
    virtual QString subtitleScript();
    virtual QString speechScript();
    const QString modelFolder(bool mainFolder = true);
    virtual const QStringList getInstalledModels();
    virtual bool installNewModel(const QString &modelName = QString());
    const QString getVenvPath() override;
    static const QString configForModel();
    AbstractPythonInterface::PythonExec venvPythonExecs(bool checkPip = false) override;
    bool useSystemPython() override;
    bool installRequirements(QString reqFile) override;
    QString featureName() override;

public Q_SLOTS:

Q_SIGNALS:
    void subtitleProgressUpdate(int);
    void subtitleFinished(int exitCode, QProcess::ExitStatus exitStatus);
};
