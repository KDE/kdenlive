/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QQuickImageProvider>
#include <memory>

/** @class QmlIconProvider
    @brief Provides icon pixmap from the icon name to Qml
 */
class QmlIconProvider : public QQuickImageProvider
{
public:
    explicit QmlIconProvider(QSize iconSize, QObject *parent);
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    QSize m_defaultSize;
};
