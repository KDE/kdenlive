/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "otioexport.h"

#include "core.h"
#include "doc/kdenlivedoc.h"
#include "mainwindow.h"

#include <KLocalizedString>

#include <QFileDialog>

#include <opentimelineio/timeline.h>

OtioExport::OtioExport(QObject *parent)
    : QObject(parent)
{
}

void OtioExport::slotExportProject()
{
    const QString exportFile =
        QFileDialog::getSaveFileName(pCore->window(), i18n("OpenTimelineIO Export"), pCore->currentDoc()->projectDataFolder(), i18n("*.otio"));
    if (exportFile.isNull()) {
        return;
    }
}
