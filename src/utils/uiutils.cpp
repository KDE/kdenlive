/*
    SPDX-FileCopyrightText: 2025 Julius Künzel <julius.kuenzel@kde.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "uiutils.h"
#include "kdenlivesettings.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QPixmap>

static QStringList forbiddenParams = {QStringLiteral("attach"), QStringLiteral("metadata"), QStringLiteral("null"),   QStringLiteral("dump"),
                                      QStringLiteral("concat"), QStringLiteral("safe"),     QStringLiteral("ladspa"), QStringLiteral("protocol_whitelist")};

const QStringList UiUtils::getProxyForbiddenParams()
{
    return forbiddenParams;
}

QStringList UiUtils::checkUnknownProxyParams(QString proxyData)
{
    // Next check if all our params are in the safe list
    proxyData.prepend(QLatin1Char(' '));
    const QStringList params = proxyData.simplified().split(QLatin1String(" -"), Qt::SkipEmptyParts);
    QStringList detectedParams;
    for (auto &p : params) {
        QString paramName = p.section(QLatin1Char(' '), 0, 0);
        if (paramName.contains(QLatin1Char('='))) {
            paramName = p.section(QLatin1Char('='), 0, 0);
        }
        detectedParams << paramName;
    }
    detectedParams.removeDuplicates();
    QStringList unknownParams;
    for (auto &d : detectedParams) {
        if (!KdenliveSettings::safeFFmpegParams().contains(d)) {
            unknownParams << d;
        }
    }
    return unknownParams;
}

void UiUtils::addSafeParameters(QStringList unknownParams)
{
    unknownParams << KdenliveSettings::safeFFmpegParams();
    unknownParams.removeDuplicates();
    KdenliveSettings::setSafeFFmpegParams(unknownParams);
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

QString UiUtils::getSaveFileName(QWidget *parent, const QString &caption, const QString &dir, const QString &filter, const QString &extension)
{
    QString selectedFile = QFileDialog::getSaveFileName(parent, caption, dir, filter);
    if (!selectedFile.isEmpty() && QFileInfo(selectedFile).suffix().isEmpty()) {
        selectedFile.append(extension);
    }
    return selectedFile;
}
