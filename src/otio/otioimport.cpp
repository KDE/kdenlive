/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "otioimport.h"

#include "core.h"
#include "doc/kdenlivedoc.h"
#include "mainwindow.h"

#include <KLocalizedString>

#include <QFileDialog>

#include <opentimelineio/timeline.h>

OtioImport::OtioImport(QObject *parent)
    : QObject(parent)
{
}

void OtioImport::slotImportProject()
{
    const QString importFile =
        QFileDialog::getOpenFileName(pCore->window(), i18n("OpenTimelineIO Import"), pCore->currentDoc()->projectDataFolder(), i18n("*.otio"));
    if (importFile.isNull() || !QFile::exists(importFile)) {
        return;
    }
}