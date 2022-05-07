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
    SpeechToText();
    QString runSubtitleScript(QString modelDirectory, QString language, QString audio, QString speech);
    QString subtitleScript() { return m_scripts->value(QStringLiteral("speech.py")); };
    QString speechScript() { return m_scripts->value(QStringLiteral("speechtotext.py")); };
    QString voskModelPath();
    QStringList parseVoskDictionaries();

protected:
    QString featureName() override;

public slots:

signals:
    void subtitleProgressUpdate(int);
    void subtitleFinished(int exitCode, QProcess::ExitStatus exitStatus);

};
