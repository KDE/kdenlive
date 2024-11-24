/*
    this file is part of Kdenlive, the Libre Video Editor by KDE
    SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>

class OtioImport : public QObject
{
    Q_OBJECT

public:
    OtioImport(QObject *parent);

public Q_SLOTS:
    void slotImportProject();

private:
};
