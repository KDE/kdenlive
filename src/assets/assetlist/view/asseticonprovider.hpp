/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KImageCache>
#include <QCache>
#include <QQuickImageProvider>
#include <memory>

/** @class AssetIconProvider
    @brief \@todo Describe class AssetIconProvider
    @todo Describe class AssetIconProvider
 */
class AssetIconProvider : public QObject
{
public:
    explicit AssetIconProvider(bool effect, QObject *parent);
    const QPixmap makePixmap(const QString &effectName);

private:
    QImage makeIcon(const QString &effectName);
    bool m_effect;
};
