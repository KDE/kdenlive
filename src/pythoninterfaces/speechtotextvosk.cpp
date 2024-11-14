/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "speechtotextvosk.h"
#include "core.h"
#include "dialogs/whisperdownload.h"
#include "kdenlivesettings.h"

#include <KIO/Global>
#include <KLocalizedString>

#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

SpeechToTextVosk::SpeechToTextVosk(QObject *parent)
    : SpeechToText(SpeechToTextEngine::EngineVosk, parent)
{
    addDependency(QStringLiteral("vosk"), i18n("speech features"));
    addDependency(QStringLiteral("srt"), i18n("automated subtitling"));
    addScript(QStringLiteral("vosk/speech.py"));
    addScript(QStringLiteral("vosk/speechtotext.py"));
}

const QString SpeechToTextVosk::modelFolder(bool)
{
    QString modelDirectory = KdenliveSettings::vosk_folder_path();
    if (modelDirectory.isEmpty()) {
        modelDirectory = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("speechmodels"), QStandardPaths::LocateDirectory);
    }
    return modelDirectory;
}

const QStringList SpeechToTextVosk::getInstalledModels()
{
    QString modelDirectory = modelFolder();
    if (modelDirectory.isEmpty()) {
        qDebug() << "=== /// CANNOT ACCESS SPEECH DICTIONARIES FOLDER";
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
    return final;
}

bool SpeechToTextVosk::installNewModel(const QString &)
{
    return true;
}

QString SpeechToTextVosk::subtitleScript()
{
    return m_scripts.value(QStringLiteral("vosk/speech.py"));
}

QString SpeechToTextVosk::speechScript()
{
    return m_scripts.value(QStringLiteral("vosk/speechtotext.py"));
}
