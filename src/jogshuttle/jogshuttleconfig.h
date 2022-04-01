/*
    SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "jogaction.h"

class JogShuttleConfig : public QObject
{
    Q_OBJECT
public:
    static QStringList actionMap(const QString &actionMap);
    static QString actionMap(const QStringList &actionMap);
};
