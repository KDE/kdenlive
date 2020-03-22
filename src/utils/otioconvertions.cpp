/*
 * this file is part of Kdenlive, the Libre Video Editor by KDE
 * Copyright 2019  Vincent Pinon <vpinon@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "otioconvertions.h"

#include "mainwindow.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "project/projectmanager.h"

#include <QStandardPaths>
#include <KMessageBox>

OtioConvertions::OtioConvertions()
{
}

void OtioConvertions::getOtioConverters()
{
    if(QStandardPaths::findExecutable(QStringLiteral("otioconvert")).isEmpty() ||
       QStandardPaths::findExecutable(QStringLiteral("python3")).isEmpty()) {
        qInfo("otioconvert or python3 not available, project import/export not enabled");
        return;
    }
    connect(&m_otioProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &OtioConvertions::slotGotOtioConverters);
    m_otioProcess.start(QStringLiteral("python3"));
    m_otioProcess.write(QStringLiteral(
"from opentimelineio import *\n"
"for a in plugins.ActiveManifest().adapters:\n"
"    if a.has_feature('read') and a.has_feature('write'):\n"
"        print('*.'+' *.'.join(a.suffixes), end=' ')"
                            ).toUtf8());
    m_otioProcess.closeWriteChannel();
}

void OtioConvertions::slotGotOtioConverters(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_otioProcess.disconnect();
    if(exitStatus != QProcess::NormalExit || exitCode != 0) {
        qInfo("OpenTimelineIO python module is not available");
        return;
    }
    m_adapters = m_otioProcess.readAll();
    if (!m_adapters.contains("kdenlive")) {
        qInfo() << "Kdenlive not supported by OTIO: " << m_adapters;
        return;
    }
    QAction *exportAction = pCore->window()->actionCollection()->action("export_project");
    QAction *importAction = pCore->window()->actionCollection()->action("import_project");
    exportAction->setEnabled(true);
    importAction->setEnabled(true);
}

void OtioConvertions::slotExportProject()
{
    auto exportFile = QFileDialog::getSaveFileName(
                pCore->window(), i18n("Export Project"),
                pCore->currentDoc()->projectDataFolder(),
                i18n("OpenTimelineIO adapters (%1)", m_adapters));
    if (exportFile.isNull()) {
        return;
    }
    QByteArray xml = pCore->projectManager()->projectSceneList("").toUtf8();
    if (xml.isNull()) {
        KMessageBox::error(pCore->window(), i18n("Project file could not be saved for export."));
        return;
    }
    m_otioTempFile.setFileTemplate(QStringLiteral("XXXXXX.kdenlive"));
    if (!m_otioTempFile.open() || !(m_otioTempFile.write(xml) > 0)) {
        pCore->displayMessage(i18n("Unable to write to temporary kdenlive file for export: %1",
                                   m_otioTempFile.fileName()), ErrorMessage);
        return;
    }
    connect(&m_otioProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &OtioConvertions::slotProjectExported);
    m_otioProcess.start(QStringLiteral("otioconvert"), {"-i", m_otioTempFile.fileName(), "-o", exportFile});
}

void OtioConvertions::slotImportProject()
{
    // Select foreign project to import
    auto importFile = QFileDialog::getOpenFileName(
                pCore->window(), i18n("Project to import"),
                pCore->currentDoc()->projectDataFolder(),
                i18n("OpenTimelineIO adapters (%1)", m_adapters));
    if (importFile.isNull() || !QFile::exists(importFile)) {
        return;
    }
    // Select converted project file
    m_importedFile = QFileDialog::getSaveFileName(
                pCore->window(), i18n("Imported Project"),
                pCore->currentDoc()->projectDataFolder(),
                i18n("Kdenlive project (*.kdenlive)"));
    if (m_importedFile.isNull()) {
        return;
    }
    // Start convertion process
    connect(&m_otioProcess, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &OtioConvertions::slotProjectImported);
    m_otioProcess.start(QStringLiteral("otioconvert"), {"-i", importFile, "-o", m_importedFile});
}

void OtioConvertions::slotProjectExported(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_otioProcess.disconnect();
    m_otioTempFile.remove();
    if(exitStatus != QProcess::NormalExit || exitCode != 0) {
        pCore->displayMessage(i18n("Project conversion failed"), ErrorMessage);
        qWarning() << exitCode << exitStatus << QString(m_otioProcess.readAllStandardError());
        return;
    }
    pCore->displayMessage(i18n("Project conversion complete"), InformationMessage);
}

void OtioConvertions::slotProjectImported(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_otioProcess.disconnect();
    if(exitStatus != QProcess::NormalExit || exitCode != 0 || !QFile::exists(m_importedFile)) {
        pCore->displayMessage(i18n("Project conversion failed"), ErrorMessage);
        qWarning() << exitCode << exitStatus << QString(m_otioProcess.readAllStandardError());
        return;
    }
    pCore->displayMessage(i18n("Project conversion complete"), InformationMessage);
    // Verify current project can be closed
    if (pCore->currentDoc()->isModified() &&
        KMessageBox::warningContinueCancel(pCore->window(), i18n(
            "The current project has not been saved\n"
            "Do you want to load imported project abandoning latest changes?")
        ) != KMessageBox::Continue) {
        return;
    }
    pCore->projectManager()->openFile(QUrl::fromLocalFile(m_importedFile));
}
