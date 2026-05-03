/*
    SPDX-FileCopyrightText: 2025 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "uiutils.h"

#include <QIcon>
#include <QPixmap>

static QStringList forbiddenParams = {QStringLiteral("attach"), QStringLiteral("metadata"), QStringLiteral("null"),   QStringLiteral("dump"),
                                      QStringLiteral("concat"), QStringLiteral("safe"),     QStringLiteral("ladspa"), QStringLiteral("protocol_whitelist")};

const QStringList UiUtils::getProxyForbiddenParams()
{
    return forbiddenParams;
}

QIcon UiUtils::rotatedIcon(const QString &iconName, const QSize iconSize, qreal rotation)
{
    QIcon icon = QIcon::fromTheme(iconName);
    QPixmap pix = icon.pixmap(iconSize);
    QTransform trans;
    trans.rotate(rotation);
    pix = pix.transformed(trans);
    return QIcon(pix);
}
