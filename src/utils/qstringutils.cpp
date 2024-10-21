/*
    SPDX-FileCopyrightText: 2023 Julius Künzel <jk.kdedev@smartlab.uber.space>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "qstringutils.h"

#include <QFileInfo>
#include <QString>
#include <QStringList>

QString QStringUtils::getUniqueName(const QStringList &names, const QString &name)
{
    int i = 0;
    QString newName = name;
    while (names.contains(newName)) {
        // name is not unique, add a suffix
        newName = name + QStringLiteral("-%1").arg(i);
        i++;
    }
    return newName;
}

QString QStringUtils::getUniqueFileName(const QStringList &names, const QString &name)
{
    int i = 0;
    const QString baseName = QFileInfo(name).completeBaseName();
    const QString extension = QFileInfo(name).suffix();
    QString newName = name;
    while (names.contains(newName)) {
        // name is not unique, add a suffix
        newName = baseName + QString::asprintf("-%04d.", ++i) + extension;
    }
    return newName;
}

QString QStringUtils::appendToFilename(const QString &filename, const QString &appendix)
{
    QString name = filename.section(QLatin1Char('.'), 0, -2);
    QString extension = filename.section(QLatin1Char('.'), -1);
    return name + appendix + QLatin1Char('.') + extension;
}
