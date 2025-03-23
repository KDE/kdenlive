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
};
