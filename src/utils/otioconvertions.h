/*
 * this file is part of Kdenlive, the Libre Video Editor by KDE
 * SPDX-FileCopyrightText: 2019 Vincent Pinon <vpinon@kde.org>
 * 
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef OTIOCONVERTIONS_H
#define OTIOCONVERTIONS_H

#include <QObject>
#include <QProcess>
#include <QTemporaryFile>

class OtioConvertions: public QObject
{
    Q_OBJECT
public:
    OtioConvertions();
    bool getOtioConverters();

private:
    QString m_adapters;

public slots:
    void slotExportProject();
    void slotImportProject();
};

#endif // OTIOCONVERTIONS_H
