/*
    SPDX-FileCopyrightText: 2012 Jean-Baptiste Mardelle (jb@kdenlive.org)

    SPDX-License-Identifier: GPL-2.0-or-later

*/

#ifndef JOGSHUTTLECONFIG_H
#define JOGSHUTTLECONFIG_H

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

#endif // JOGSHUTTLECONFIG_H
