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

SpeechToText::SpeechToText(EngineType engineType, QObject *parent)
    : AbstractPythonInterface(parent)
    , m_engineType(engineType)
{
    if (engineType == EngineType::EngineVosk) {
        addDependency(QStringLiteral("vosk"), i18n("speech features"));
        addDependency(QStringLiteral("srt"), i18n("automated subtitling"));
        addScript(QStringLiteral("speech.py"));
        addScript(QStringLiteral("speechtotext.py"));
    } else if (engineType == EngineType::EngineWhisper) {
        buildWhisperDeps(KdenliveSettings::enableSeamless());
        addScript(QStringLiteral("whispertotext.py"));
        addScript(QStringLiteral("whispertosrt.py"));
    }
}

void SpeechToText::buildWhisperDeps(bool enableSeamless)
{
    m_dependencies.clear();
    m_optionalDeps.clear();
    addDependency(QStringLiteral("openai-whisper"), i18n("speech features"));
    addDependency(QStringLiteral("srt"), i18n("automated subtitling"));
    addDependency(QStringLiteral("srt_equalizer"), i18n("adjust subtitles length"), true);
    addDependency(QStringLiteral("torch"), i18n("machine learning framework"));
    if (enableSeamless) {
        addDependency(QStringLiteral("sentencepiece"), i18n("SeamlessM4T translation"), true);
        addDependency(QStringLiteral("protobuf"), i18n("SeamlessM4T translation"), true);
        addDependency(QStringLiteral("transformers"), i18n("SeamlessM4T translation"), true);
    }
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

QList<std::pair<QString, QString>> SpeechToText::whisperModels()
{
    QList<std::pair<QString, QString>> models;
    models.append({i18nc("Model file to download, smallest one", "Tiny (72MB)"), QStringLiteral("tiny")});
    models.append({i18nc("Model file to download, basic one", "Base (140MB)"), QStringLiteral("base")});
    models.append({i18nc("Model file to download, small one", "Small (460MB)"), QStringLiteral("small")});
    models.append({i18nc("Model file to download, medium one", "Medium (1.4GB)"), QStringLiteral("medium")});
    models.append({i18nc("Model file to download, large one", "Large (2.9GB)"), QStringLiteral("large")});
    models.append({i18nc("Model file to download, smallest one", "Tiny - English only (39MB)"), QStringLiteral("tiny.en")});
    models.append({i18nc("Model file to download, basic one", "Base - English only (74MB)"), QStringLiteral("base.en")});
    models.append({i18nc("Model file to download, small one", "Small - English only (244MB)"), QStringLiteral("small.en")});
    models.append({i18nc("Model file to download, medium one", "Medium - English only (1.5GB)"), QStringLiteral("medium.en")});
    return models;
}

QMap<QString, QString> SpeechToText::whisperLanguages()
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

QStringList SpeechToText::parseVoskDictionaries()
{
    QString modelDirectory = voskModelPath();
    if (modelDirectory.isEmpty()) {
        qDebug() << "=== /// CANNOT ACCESS SPEECH DICTIONARIES FOLDER";
        Q_EMIT pCore->voskModelUpdate({});
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
    Q_EMIT pCore->voskModelUpdate(final);
    return final;
}

QString SpeechToText::subtitleScript()
{
    if (m_engineType == EngineType::EngineWhisper) {
        return m_scripts->value(QStringLiteral("whispertosrt.py"));
    }
    return m_scripts->value(QStringLiteral("speech.py"));
}

QString SpeechToText::speechScript()
{
    if (m_engineType == EngineType::EngineWhisper) {
        return m_scripts->value(QStringLiteral("whispertotext.py"));
    }
    return m_scripts->value(QStringLiteral("speechtotext.py"));
}
