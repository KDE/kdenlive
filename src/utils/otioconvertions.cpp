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
#include <KLocalizedString>
#include "otioconvertions.h"

#include "mainwindow.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "project/projectmanager.h"

#include <QStandardPaths>
#include <KMessageBox>

OtioConvertions::OtioConvertions()
= default;

bool OtioConvertions::getOtioConverters()
{
    if(QStandardPaths::findExecutable(QStringLiteral("otioconvert")).isEmpty()) {
        KMessageBox::error(pCore->window(), i18n("Could not find \"otioconvert\" script.\n"
                                   "You need to install OpenTimelineIO,\n"
                                   "through your package manager if available,\n"
                                   "or by \"pip3 install opentimelineio\",\n"
                                   "and check the scripts are installed in a directory "
                                   "listed in PATH environment variable"));
        return false;
    }
#ifdef Q_OS_WIN
    QString pyExec = QStandardPaths::findExecutable(QStringLiteral("python"));
#else
    QString pyExec = QStandardPaths::findExecutable(QStringLiteral("python3"));
#endif
    if(pyExec.isEmpty()) {
        KMessageBox::error(pCore->window(), ("Could not find \"python3\" executable, needed for OpenTimelineIO adapters.\n"
                                   "If already installed, check it is installed in a directory"
                                   "listed in PATH environment variable"));
        return false;
    }
    QProcess listAdapters;
    listAdapters.start(pyExec,QStringList());
    listAdapters.write(QStringLiteral(
                           "from opentimelineio import *\n"
                           "for a in plugins.ActiveManifest().adapters:\n"
                           "    if a.has_feature('read') and a.has_feature('write'):\n"
                           "        print('*.'+' *.'.join(a.suffixes), end=' ')"
                           ).toUtf8());
    listAdapters.closeWriteChannel();
    listAdapters.waitForFinished();
    if(listAdapters.exitStatus() != QProcess::NormalExit || listAdapters.exitCode() != 0) {
        KMessageBox::error(pCore->window(), i18n("python3 could not load opentimelineio module"));
        return false;
    }
    m_adapters = listAdapters.readAll();
    qInfo() << "OTIO adapters:" << m_adapters;
    if (!m_adapters.contains("kdenlive")) {
        KMessageBox::error(pCore->window(), i18n("Your OpenTimelineIO module does not include Kdenlive adapter.\n"
                                   "Please install version >= 0.12\n"));
        return false;
    }
    return true;
}

void OtioConvertions::slotExportProject()
{
    if(m_adapters.isEmpty() && !getOtioConverters()) {
        return;
    }
    QString exportFile = QFileDialog::getSaveFileName(
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
    QTemporaryFile tmp;
    tmp.setFileTemplate(QStringLiteral("XXXXXX.kdenlive"));
    if (!tmp.open() || !(tmp.write(xml) > 0)) {
        KMessageBox::error(pCore->window(), i18n("Unable to write to temporary kdenlive file for export: %1",
                                   tmp.fileName()));
        return;
    }
    QProcess convert;
    convert.start(QStringLiteral("otioconvert"), {"-i", tmp.fileName(), "-o", exportFile});
    convert.waitForFinished();
    tmp.remove();
    if(convert.exitStatus() != QProcess::NormalExit || convert.exitCode() != 0) {
        KMessageBox::error(pCore->window(), i18n("Project conversion failed:\n%1",
                           QString(convert.readAllStandardError())));
        return;
    }
    pCore->displayMessage(i18n("Project conversion complete"), InformationMessage);
}

void OtioConvertions::slotImportProject()
{
    if(m_adapters.isEmpty() && !getOtioConverters()) {
        return;
    }
    // Select foreign project to import
    QString importFile = QFileDialog::getOpenFileName(
                pCore->window(), i18n("Project to import"),
                pCore->currentDoc()->projectDataFolder(),
                i18n("OpenTimelineIO adapters (%1)", m_adapters));
    if (importFile.isNull() || !QFile::exists(importFile)) {
        return;
    }
    // Select converted project file
    QString importedFile = QFileDialog::getSaveFileName(
                pCore->window(), i18n("Imported Project"),
                pCore->currentDoc()->projectDataFolder(),
                i18n("Kdenlive project (*.kdenlive)"));
    if (importedFile.isNull()) {
        return;
    }
    // Start conversion process
    QProcess convert;
    convert.start(QStringLiteral("otioconvert"), {"-i", importFile, "-o", importedFile});
    convert.waitForFinished();
    if(convert.exitStatus() != QProcess::NormalExit || convert.exitCode() != 0 || !QFile::exists(importedFile)) {
        KMessageBox::error(pCore->window(), i18n("Project conversion failed:\n%1",
                           QString(convert.readAllStandardError())));
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
    pCore->projectManager()->openFile(QUrl::fromLocalFile(importedFile));
}
