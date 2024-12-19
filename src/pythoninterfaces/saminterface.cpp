/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "saminterface.h"
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

SamInterface::SamInterface(QObject *parent)
    : AbstractPythonInterface(parent)
{
    QString scriptPath = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/automask/requirements-sam.txt"));
    if (!scriptPath.isEmpty()) {
        m_dependencies.insert(scriptPath, QString());
    }
    addScript(QStringLiteral("automask/sam-objectmask.py"));
}

const QString SamInterface::modelFolder(bool)
{
    QString modelDirectory = KdenliveSettings::sam_folder_path();
    if (modelDirectory.isEmpty()) {
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
        const QString modelName = QStringLiteral("sammodels");
        if (!dir.exists(modelName)) {
            dir.mkpath(modelName);
        }
        modelDirectory = QStandardPaths::locate(QStandardPaths::AppDataLocation, modelName, QStandardPaths::LocateDirectory);
        KdenliveSettings::setSam_folder_path(modelDirectory);
    }
    return modelDirectory;
}

const QStringList SamInterface::getInstalledModels()
{
    QString modelDirectory = modelFolder();
    if (modelDirectory.isEmpty()) {
        qDebug() << "=== /// CANNOT ACCESS SPEECH DICTIONARIES FOLDER";
        return {};
    }
    QDir modelsFolder(modelDirectory);
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

bool SamInterface::installNewModel(const QString &modelName)
{
    /*WhisperDownload d(this, modelName, QApplication::activeWindow());
    d.exec();
    bool installedNew = d.newModelsInstalled();
    return installedNew;*/
    return false;
}

QString SamInterface::featureName()
{
    return i18n("Object Segmentation (SAM2)");
}

QString SamInterface::subtitleScript()
{
    return QString();
}

QString SamInterface::speechScript()
{
    return QString();
}

const QString SamInterface::getVenvPath()
{
    return QStringLiteral("venv-sam");
}
