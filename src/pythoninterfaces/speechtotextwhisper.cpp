/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "speechtotextwhisper.h"
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
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

SpeechToTextWhisper::SpeechToTextWhisper(QObject *parent)
    : SpeechToText(SpeechToTextEngine::EngineWhisper, parent)
{
    buildWhisperDeps(KdenliveSettings::enableSeamless());
    addScript(QStringLiteral("whisper/whispertotext.py"));
    addScript(QStringLiteral("whisper/whispertosrt.py"));
    addScript(QStringLiteral("whisper/whisperquery.py"));
    addScript(QStringLiteral("whisper/seamlessquery.py"));
}

void SpeechToTextWhisper::buildWhisperDeps(bool enableSeamless)
{
    m_dependencies.clear();
    m_optionalDeps.clear();
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    QString scriptPath = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/whisper/requirements-whisper-windows.txt"));
#else
    QString scriptPath = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/whisper/requirements-whisper.txt"));
#endif
    if (!scriptPath.isEmpty()) {
        m_dependencies.insert(scriptPath, QString());
    }
    if (enableSeamless) {
        scriptPath = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/whisper/requirements-seamless.txt"));
        if (!scriptPath.isEmpty()) {
            m_dependencies.insert(scriptPath, QString());
            m_optionalDeps << QStringLiteral("srt_equalizer");
        }
    }
}

QMap<QString, QString> SpeechToTextWhisper::speechLanguages()
{
    QMap<QString, QString> models;
    models.insert(i18n("Autodetect"), QString());
    models.insert(i18n("Afrikaans"), QStringLiteral("Afrikaans"));
    models.insert(i18n("Albanian"), QStringLiteral("Albanian"));
    models.insert(i18n("Amharic"), QStringLiteral("Amharic"));
    models.insert(i18n("Arabic"), QStringLiteral("Arabic"));
    models.insert(i18n("Armenian"), QStringLiteral("Armenian"));
    models.insert(i18n("Assamese"), QStringLiteral("Assamese"));
    models.insert(i18n("Azerbaijani"), QStringLiteral("Azerbaijani"));
    models.insert(i18n("Bashkir"), QStringLiteral("Bashkir"));
    models.insert(i18n("Basque"), QStringLiteral("Basque"));
    models.insert(i18n("Belarusian"), QStringLiteral("Belarusian"));
    models.insert(i18n("Bengali"), QStringLiteral("Bengali"));
    models.insert(i18n("Bosnian"), QStringLiteral("Bosnian"));
    models.insert(i18n("Breton"), QStringLiteral("Breton"));
    models.insert(i18n("Bulgarian"), QStringLiteral("Bulgarian"));
    models.insert(i18n("Burmese"), QStringLiteral("Burmese"));
    models.insert(i18n("Castilian"), QStringLiteral("Castilian"));
    models.insert(i18n("Catalan"), QStringLiteral("Catalan"));
    models.insert(i18n("Chinese"), QStringLiteral("Chinese"));
    models.insert(i18n("Croatian"), QStringLiteral("Croatian"));
    models.insert(i18n("Czech"), QStringLiteral("Czech"));
    models.insert(i18n("Danish"), QStringLiteral("Danish"));
    models.insert(i18n("Dutch"), QStringLiteral("Dutch"));
    models.insert(i18n("English"), QStringLiteral("English"));
    models.insert(i18n("Estonian"), QStringLiteral("Estonian"));
    models.insert(i18n("Faroese"), QStringLiteral("Faroese"));
    models.insert(i18n("Finnish"), QStringLiteral("Finnish"));
    models.insert(i18n("Flemish"), QStringLiteral("Flemish"));
    models.insert(i18n("French"), QStringLiteral("French"));
    models.insert(i18n("Galician"), QStringLiteral("Galician"));
    models.insert(i18n("Georgian"), QStringLiteral("Georgian"));
    models.insert(i18n("German"), QStringLiteral("German"));
    models.insert(i18n("Greek"), QStringLiteral("Greek"));
    models.insert(i18n("Gujarati"), QStringLiteral("Gujarati"));
    models.insert(i18n("Haitian"), QStringLiteral("Haitian"));
    models.insert(i18n("Haitian Creole"), QStringLiteral("Haitian Creole"));
    models.insert(i18n("Hausa"), QStringLiteral("Hausa"));
    models.insert(i18n("Hawaiian"), QStringLiteral("Hawaiian"));
    models.insert(i18n("Hebrew"), QStringLiteral("Hebrew"));
    models.insert(i18n("Hindi"), QStringLiteral("Hindi"));
    models.insert(i18n("Hungarian"), QStringLiteral("Hungarian"));
    models.insert(i18n("Icelandic"), QStringLiteral("Icelandic"));
    models.insert(i18n("Indonesian"), QStringLiteral("Indonesian"));
    models.insert(i18n("Italian"), QStringLiteral("Italian"));
    models.insert(i18n("Japanese"), QStringLiteral("Japanese"));
    models.insert(i18n("Javanese"), QStringLiteral("Javanese"));
    models.insert(i18n("Kannada"), QStringLiteral("Kannada"));
    models.insert(i18n("Kazakh"), QStringLiteral("Kazakh"));
    models.insert(i18n("Khmer"), QStringLiteral("Khmer"));
    models.insert(i18n("Korean"), QStringLiteral("Korean"));
    models.insert(i18n("Lao"), QStringLiteral("Lao"));
    models.insert(i18n("Latin"), QStringLiteral("Latin"));
    models.insert(i18n("Latvian"), QStringLiteral("Latvian"));
    models.insert(i18n("Letzeburgesch"), QStringLiteral("Letzeburgesch"));
    models.insert(i18n("Lingala"), QStringLiteral("Lingala"));
    models.insert(i18n("Lithuanian"), QStringLiteral("Lithuanian"));
    models.insert(i18n("Luxembourgish"), QStringLiteral("Luxembourgish"));
    models.insert(i18n("Macedonian"), QStringLiteral("Macedonian"));
    models.insert(i18n("Malagasy"), QStringLiteral("Malagasy"));
    models.insert(i18n("Malay"), QStringLiteral("Malay"));
    models.insert(i18n("Malayalam"), QStringLiteral("Malayalam"));
    models.insert(i18n("Maltese"), QStringLiteral("Maltese"));
    models.insert(i18n("Maori"), QStringLiteral("Maori"));
    models.insert(i18n("Marathi"), QStringLiteral("Marathi"));
    models.insert(i18n("Moldavian"), QStringLiteral("Moldavian"));
    models.insert(i18n("Moldovan"), QStringLiteral("Moldovan"));
    models.insert(i18n("Mongolian"), QStringLiteral("Mongolian"));
    models.insert(i18n("Myanmar"), QStringLiteral("Myanmar"));
    models.insert(i18n("Nepali"), QStringLiteral("Nepali"));
    models.insert(i18n("Norwegian"), QStringLiteral("Norwegian"));
    models.insert(i18n("Nynorsk"), QStringLiteral("Nynorsk"));
    models.insert(i18n("Occitan"), QStringLiteral("Occitan"));
    models.insert(i18n("Panjabi"), QStringLiteral("Panjabi"));
    models.insert(i18n("Pashto"), QStringLiteral("Pashto"));
    models.insert(i18n("Persian"), QStringLiteral("Persian"));
    models.insert(i18n("Polish"), QStringLiteral("Polish"));
    models.insert(i18n("Portuguese"), QStringLiteral("Portuguese"));
    models.insert(i18n("Punjabi"), QStringLiteral("Punjabi"));
    models.insert(i18n("Pushto"), QStringLiteral("Pushto"));
    models.insert(i18n("Romanian"), QStringLiteral("Romanian"));
    models.insert(i18n("Russian"), QStringLiteral("Russian"));
    models.insert(i18n("Sanskrit"), QStringLiteral("Sanskrit"));
    models.insert(i18n("Serbian"), QStringLiteral("Serbian"));
    models.insert(i18n("Shona"), QStringLiteral("Shona"));
    models.insert(i18n("Sindhi"), QStringLiteral("Sindhi"));
    models.insert(i18n("Sinhala"), QStringLiteral("Sinhala"));
    models.insert(i18n("Sinhalese"), QStringLiteral("Sinhalese"));
    models.insert(i18n("Slovak"), QStringLiteral("Slovak"));
    models.insert(i18n("Slovenian"), QStringLiteral("Slovenian"));
    models.insert(i18n("Somali"), QStringLiteral("Somali"));
    models.insert(i18n("Spanish"), QStringLiteral("Spanish"));
    models.insert(i18n("Sundanese"), QStringLiteral("Sundanese"));
    models.insert(i18n("Swahili"), QStringLiteral("Swahili"));
    models.insert(i18n("Swedish"), QStringLiteral("Swedish"));
    models.insert(i18n("Tagalog"), QStringLiteral("Tagalog"));
    models.insert(i18n("Tajik"), QStringLiteral("Tajik"));
    models.insert(i18n("Tamil"), QStringLiteral("Tamil"));
    models.insert(i18n("Tatar"), QStringLiteral("Tatar"));
    models.insert(i18n("Telugu"), QStringLiteral("Telugu"));
    models.insert(i18n("Thai"), QStringLiteral("Thai"));
    models.insert(i18n("Tibetan"), QStringLiteral("Tibetan"));
    models.insert(i18n("Turkish"), QStringLiteral("Turkish"));
    models.insert(i18n("Turkmen"), QStringLiteral("Turkmen"));
    models.insert(i18n("Ukrainian"), QStringLiteral("Ukrainian"));
    models.insert(i18n("Urdu"), QStringLiteral("Urdu"));
    models.insert(i18n("Uzbek"), QStringLiteral("Uzbek"));
    models.insert(i18n("Valencian"), QStringLiteral("Valencian"));
    models.insert(i18n("Vietnamese"), QStringLiteral("Vietnamese"));
    models.insert(i18n("Welsh"), QStringLiteral("Welsh"));
    models.insert(i18n("Yiddish"), QStringLiteral("Yiddish"));
    models.insert(i18n("Yoruba"), QStringLiteral("Yoruba"));
    return models;
}

QString SpeechToTextWhisper::subtitleScript()
{
    return m_scripts.value(QStringLiteral("whisper/whispertosrt.py"));
}

QString SpeechToTextWhisper::speechScript()
{
    return m_scripts.value(QStringLiteral("whisper/whispertotext.py"));
}

bool SpeechToTextWhisper::installNewModel(const QString &modelName)
{
    WhisperDownload d(this, modelName, QApplication::activeWindow());
    d.exec();
    bool installedNew = d.newModelsInstalled();
    return installedNew;
}

const QString SpeechToTextWhisper::modelFolder(bool mainFolder, bool create)
{
    QString folder;
    if (mainFolder) {
        folder = KdenliveSettings::whisperModelFolder();
        if (folder.isEmpty() || !QFileInfo::exists(folder)) {
            if (create) {
                QDir dir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
                dir.mkpath(".cache/whisper");
            }
            folder = QStandardPaths::locate(QStandardPaths::HomeLocation, QStringLiteral("/.cache/whisper"), QStandardPaths::LocateDirectory);
        }
    } else {
        folder = QStandardPaths::locate(QStandardPaths::HomeLocation, QStringLiteral("/.cache/huggingface/hub/models--facebook--seamless-m4t-v2-large"),
                                        QStandardPaths::LocateDirectory);
    }
    if (folder.isEmpty()) {
        // No folder found for whisper models
        return {};
    }
    QDir modelsFolder(folder);
    if (!modelsFolder.exists()) {
        return QString();
    }
    return modelsFolder.absolutePath();
}

const QStringList SpeechToTextWhisper::getInstalledModels()
{
    const QString path = modelFolder();
    QDir modelsFolder(path);
    if (path.isEmpty() || !modelsFolder.exists()) {
        return {};
    }
    QStringList installedModels;
    QStringList files = modelsFolder.entryList({QStringLiteral("*.pt")}, QDir::Files);
    QMap<QString, QString> modelsMap;
    for (auto &m : KdenliveSettings::whisperAvailableModels()) {
        modelsMap.insert(m.section(QLatin1Char('='), 1), m.section(QLatin1Char('='), 0, 0));
    }
    for (int i = 0; i < files.count(); i++) {
        if (modelsMap.contains(files.at(i))) {
            installedModels << modelsMap.value(files.at(i));
        }
    }
    return installedModels;
}

const QString SpeechToTextWhisper::installMessage() const
{
    return i18n("Install Whisper");
}
