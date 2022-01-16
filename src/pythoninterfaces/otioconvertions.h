/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2019 Vincent Pinon <vpinon@kde.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef OTIOCONVERTIONS_H
#define OTIOCONVERTIONS_H

#include "pythoninterfaces/abstractpythoninterface.h"

#include <QDialog>
#include <QObject>
#include <QProcess>
#include <QTemporaryFile>

class OtioConvertions: public AbstractPythonInterface
{
    Q_OBJECT
public:
    OtioConvertions();
    bool getOtioConverters();
    bool configureSetup();
    bool wellConfigured();
    bool runOtioconvert(const QString &inputFile, const QString &outputFile);

private:
    QString m_importAdapters;
    QString m_exportAdapters;

public slots:
    void slotExportProject();
    void slotImportProject();
};

#endif // OTIOCONVERTIONS_H
