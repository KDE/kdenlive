/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef ASSETICONPROVIDER_H
#define ASSETICONPROVIDER_H

#include <KImageCache>
#include <QCache>
#include <QQuickImageProvider>
#include <memory>

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
