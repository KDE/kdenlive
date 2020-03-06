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

#include "asseticonprovider.hpp"
#include "effects/effectsrepository.hpp"
#include "transitions/transitionsrepository.hpp"

#include <QDebug>
#include <QFont>
#include <QPainter>

AssetIconProvider::AssetIconProvider(bool effect)
    : QQuickImageProvider(QQmlImageProviderBase::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
{
    m_effect = effect;
}

QImage AssetIconProvider::requestImage(const QString &name, QSize *size, const QSize &requestedSize)
{
    QImage result;

    if (name.isEmpty()) {
        QPixmap pix(30, 30);
        return pix.toImage();
    }

    result = makeIcon(name, requestedSize);
    if (size) {
        *size = result.size();
    }
    return result;
}

QImage AssetIconProvider::makeIcon(const QString &effectName, const QSize &size)
{
    Q_UNUSED(size);
    QPixmap pix(30, 30);
    if (effectName.isEmpty()) {
        pix.fill(Qt::red);
        return pix.toImage();
    }
    QFont ft = QFont();
    ft.setBold(true);
    uint hex = qHash(effectName);
    QString t = QStringLiteral("#") + QString::number(hex, 16).toUpper().left(6);
    QColor col(t);
    bool isAudio = false;
    bool isCustom = false;
    bool isGroup = false;
    AssetListType::AssetType type = (AssetListType::AssetType)effectName.section(QLatin1Char('/'), -1).toInt();
    if (m_effect) {
        isAudio = type == AssetListType::AssetType::Audio || type == AssetListType::AssetType::CustomAudio;
        isCustom = type == AssetListType::AssetType::CustomAudio || type == AssetListType::AssetType::Custom;
        if (isCustom) {
            //isGroup = EffectsRepository::get()->isGroup(effectId);
        }
    } else {
        isAudio = (type == AssetListType::AssetType::AudioComposition) || (type == AssetListType::AssetType::AudioTransition);
    }
    QPainter p;
    if (isCustom) {
        pix.fill(Qt::transparent);
        p.begin(&pix);
        p.setPen(Qt::NoPen);
        p.setBrush(isGroup ? Qt::magenta : Qt::red);
        p.drawRoundedRect(pix.rect(), 4, 4);
        p.setPen(QPen());
    } else if (isAudio) {
        pix.fill(Qt::transparent);
        p.begin(&pix);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(pix.rect());
        p.setPen(QPen());
    } else {
        pix.fill(col);
        p.begin(&pix);
    }
    p.setFont(ft);
    p.drawText(pix.rect(), Qt::AlignCenter, effectName.at(0));
    p.end();
    return pix.toImage();
}
