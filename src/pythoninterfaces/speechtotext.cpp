/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "speechtotext.h"
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

SpeechToText::SpeechToText(SpeechToTextEngine::EngineType engineType, QObject *parent)
    : AbstractPythonInterface(parent)
    , m_engineType(engineType)
{
}

QString SpeechToText::featureName()
{
    return i18n("Speech to text");
}

QMap<QString, QString> SpeechToText::speechLanguages()
{
    return {};
}

QString SpeechToText::subtitleScript()
{
    return QString();
}

QString SpeechToText::speechScript()
{
    return QString();
}

SpeechToTextEngine::EngineType SpeechToText::engineType() const
{
    return m_engineType;
}
