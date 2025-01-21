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
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    const QString modelName = QStringLiteral("sam2models");
    if (!dir.exists(modelName)) {
        dir.mkpath(modelName);
    }
    return QStandardPaths::locate(QStandardPaths::AppDataLocation, modelName, QStandardPaths::LocateDirectory);
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
    for (auto &f : files) {
        installedModels << modelsFolder.absoluteFilePath(f);
    }
    return installedModels;
}

bool SamInterface::installNewModel(const QString &)
{
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

const QString SamInterface::configForModel()
{
    KConfig conf(QStringLiteral("sammodelsinfo.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, QStringLiteral("models"));
    QMap<QString, QString> values = group.entryMap();
    QMapIterator<QString, QString> i(values);
    while (i.hasNext()) {
        i.next();
        if (QFileInfo(i.value()).completeBaseName() == QFileInfo(KdenliveSettings::samModelFile()).completeBaseName()) {
            return i.key();
        }
    }
    return QString();
}

AbstractPythonInterface::PythonExec SamInterface::venvPythonExecs(bool checkPip)
{
    if (KdenliveSettings::sam_system_python()) {
        // Use system python for SAM plugin
#ifdef Q_OS_WIN
        const QString pythonName = QStringLiteral("python");
        const QString pipName = QStringLiteral("pip");
#else
        const QString pythonName = QStringLiteral("python3");
        const QString pipName = QStringLiteral("pip3");
#endif
        const QStringList pythonPaths = {QFileInfo(KdenliveSettings::sam_system_python_path()).dir().absolutePath()};
        const QString pythonExe = QStandardPaths::findExecutable(pythonName, pythonPaths);
        QString pipExe;
        if (checkPip) {
            pipExe = QStandardPaths::findExecutable(pipName, pythonPaths);
        }
        return {pythonExe, pipExe};
    }
    return AbstractPythonInterface::venvPythonExecs(checkPip);
}
