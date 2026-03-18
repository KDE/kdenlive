/*
    SPDX-FileCopyrightText: 2025 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "uiutils.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QPixmap>

QIcon UiUtils::rotatedIcon(const QString &iconName, const QSize iconSize, qreal rotation)
{
    QIcon icon = QIcon::fromTheme(iconName);
    QPixmap pix = icon.pixmap(iconSize);
    QTransform trans;
    trans.rotate(rotation);
    pix = pix.transformed(trans);
    return QIcon(pix);
}

QString UiUtils::getSaveFileName(QWidget *parent, const QString &caption, const QString &dir, const QString &filter, const QString &extension)
{
    QString selectedFile = QFileDialog::getSaveFileName(parent, caption, dir, filter);
    if (!selectedFile.isEmpty() && QFileInfo(selectedFile).suffix().isEmpty()) {
        selectedFile.append(extension);
    }
    return selectedFile;
}
