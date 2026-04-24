/*
    SPDX-FileCopyrightText: 2025 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QIcon>
#include <QString>

class UiUtils
{
public:
    /** @returns a rotated version of the icon associated with @param iconName.
     *  It will be of size @param iconSize and @param rotation.
     */
    static QIcon rotatedIcon(const QString &iconName, const QSize iconSize, qreal rotation = -90);
    /** @returns a user selected filename to save stuff. Ensures the requested file extension is always appended to the filename
     */
    static QString getSaveFileName(QWidget *parent = nullptr, const QString &caption = QString(), const QString &dir = QString(),
                                   const QString &filter = QString(), const QString &extension = QString());
    static const QStringList getProxyForbiddenParams();
    static QStringList checkUnknownProxyParams(QString proxyData);
    static void addSafeParameters(QStringList unknownParams);
};
