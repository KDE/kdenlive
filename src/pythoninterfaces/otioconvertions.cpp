/*
    SPDX-FileCopyrightText: 2019 Vincent Pinon <vpinon@kde.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "otioconvertions.h"
#include <KLocalizedString>

#include "core.h"
#include "doc/kdenlivedoc.h"
#include "mainwindow.h"
#include "project/projectmanager.h"

#include <KMessageBox>
#include <QStandardPaths>

OtioConvertions::OtioConvertions()
    : AbstractPythonInterface()
{
    addDependency(QStringLiteral("opentimelineio"), i18n("OpenTimelineIO convertion"));
    addScript(QStringLiteral("otiointerface.py"));
    connect(this, &OtioConvertions::dependenciesAvailable, this, [&](){
        if (QStandardPaths::findExecutable(QStringLiteral("otioconvert")).isEmpty()) {
            emit setupError(i18n("Could not find \"otioconvert\" script although it is installed through pip3.\n"
                                       "Please check the otio scripts are installed in a directory "
                                       "listed in PATH environment variable"));
            return;
        }
        m_adapters = runScript(QStringLiteral("otiointerface.py"), {});
        qInfo() << "OTIO adapters:" << m_adapters;
        if (!m_adapters.contains("kdenlive")) {
            emit setupError(i18n("Your OpenTimelineIO module does not include Kdenlive adapter.\n"
                                       "Please install version >= 0.12\n"));
        }
    });
}

bool OtioConvertions::wellConfigured()
{
    checkDependencies();
    return checkSetup() && missingDependencies().isEmpty() && !m_adapters.isEmpty() && m_adapters.contains("kdenlive");
}

bool OtioConvertions::configureSetup()
{
    QDialog *d = new QDialog(pCore->window());
    QVBoxLayout *l = new QVBoxLayout();
    QLabel *label = new QLabel(i18n("Configure your OpenTimelineIO setup"));
    QHBoxLayout *h = new QHBoxLayout();
    PythonDependencyMessage *msg = new PythonDependencyMessage(d, this);
    msg->setCloseButtonVisible(false);
    QToolButton *refresh = new QToolButton(d);
    refresh->setText(i18n("Check again"));
    refresh->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    connect(refresh, &QToolButton::clicked, this, [&](){
        if (wellConfigured()) {
            checkVersions();
        }
    });
    h->addWidget(msg);
    h->addWidget(refresh);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, d, &QDialog::reject);
    l->addWidget(label);
    l->addLayout(h);
    l->addWidget(buttonBox);
    d->setLayout(l);
    if (!wellConfigured()) {
        d->show();
        return true;
    }
    d->close();
    return false;
}

bool OtioConvertions::runOtioconvert(const QString &inputFile, const QString &outputFile) {
    QProcess convert;
    convert.start(QStringLiteral("otioconvert"), {"-i", inputFile, "-o", outputFile});
    convert.waitForFinished();
    if(convert.exitStatus() != QProcess::NormalExit || convert.exitCode() != 0) {
        KMessageBox::detailedError(pCore->window(), i18n("OpenTimelineIO Project conversion failed"),
                           QString(convert.readAllStandardError()));
        return false;
    }
    pCore->displayMessage(i18n("Project conversion complete"), InformationMessage);
    return true;
}

void OtioConvertions::slotExportProject()
{
    if (configureSetup()) {
        return;
    }
    QString exportFile = QFileDialog::getSaveFileName(
                pCore->window(), i18n("Export Project"),
                pCore->currentDoc()->projectDataFolder(),
                i18n("OpenTimelineIO adapters (%1)(%1)", m_adapters));
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
    } else {
        tmp.close();
    }
    runOtioconvert(tmp.fileName(), exportFile);
    tmp.remove();
}

void OtioConvertions::slotImportProject()
{
    if (configureSetup()) {
        return;
    }
    // Select foreign project to import
    QString importFile = QFileDialog::getOpenFileName(
                pCore->window(), i18n("Project to import"),
                pCore->currentDoc()->projectDataFolder(),
                i18n("OpenTimelineIO adapters (%1)(%1)", m_adapters));
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
    if (!runOtioconvert(importFile, importedFile)) {
        return;
    }
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
