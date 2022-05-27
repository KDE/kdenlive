/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "speechtotext.h"
#include "core.h"
#include "kdenlivesettings.h"

#include <KLocalizedString>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

SpeechToText::SpeechToText()
    : AbstractPythonInterface()
{
    addDependency(QStringLiteral("vosk"), i18n("speech features"));
    addDependency(QStringLiteral("srt"), i18n("automated subtitling"));
    addScript(QStringLiteral("speech.py"));
    addScript(QStringLiteral("speechtotext.py"));
}

QString SpeechToText::featureName()
{
    return i18n("Speech to text");
}

QString SpeechToText::voskModelPath()
{
    QString modelDirectory = KdenliveSettings::vosk_folder_path();
    if (modelDirectory.isEmpty()) {
        modelDirectory = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("speechmodels"), QStandardPaths::LocateDirectory);
    }
    return modelDirectory;
}

QStringList SpeechToText::parseVoskDictionaries()
{
    QString modelDirectory = voskModelPath();
    if (modelDirectory.isEmpty()) {
        qDebug() << "=== /// CANNOT ACCESS SPEECH DICTIONARIES FOLDER";
        emit pCore->voskModelUpdate({});
        return {};
    }
    QDir dir = QDir(modelDirectory);
    QStringList dicts = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList final;
    for (auto &d : dicts) {
        QDir sub(dir.absoluteFilePath(d));
        if (sub.exists(QStringLiteral("mfcc.conf")) || (sub.exists(QStringLiteral("conf/mfcc.conf")))) {
            final << d;
        }
    }
    emit pCore->voskModelUpdate(final);
    return final;
}
