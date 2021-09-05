/*
 *   SPDX-FileCopyrightText: 2017 Nicolas Carion *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#ifndef ASSETICONPROVIDER_H
#define ASSETICONPROVIDER_H

#include <KImageCache>
#include <QCache>
#include <QQuickImageProvider>
#include <memory>

/** @class AssetIconProvider
    @brief \@todo Describe class AssetIconProvider
    @todo Describe class AssetIconProvider
 */
class AssetIconProvider : public QQuickImageProvider
{
public:
    explicit AssetIconProvider(bool effect);
    QImage requestImage(const QString &name, QSize *size, const QSize &requestedSize) override;

private:
    QImage makeIcon(const QString &effectName, const QSize &size);
    std::unique_ptr<KImageCache> m_cache;

    bool m_effect;
};

#endif
