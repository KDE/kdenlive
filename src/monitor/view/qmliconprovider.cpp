/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "qmliconprovider.hpp"

#include <QDebug>
#include <QIcon>
#include <QStyle>

QmlIconProvider::QmlIconProvider(QSize iconSize, QObject *parent)
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
    , m_defaultSize(iconSize)
{
}

QPixmap QmlIconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    if (size) {
        *size = m_defaultSize;
    }
    QIcon icon = QIcon::fromTheme(id);
    return icon.pixmap(requestedSize.width() > 0 ? requestedSize.width() : m_defaultSize.width(),
                       requestedSize.height() > 0 ? requestedSize.height() : m_defaultSize.height());
}
