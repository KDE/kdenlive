/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "asseticonprovider.hpp"
#include "effects/effectsrepository.hpp"
#include "transitions/transitionsrepository.hpp"

#include <QDebug>
#include <QFont>
#include <QPainter>

AssetIconProvider::AssetIconProvider(bool effect)
    : QQuickImageProvider(QQmlImageProviderBase::Image, QQmlImageProviderBase::ForceAsynchronousImageLoading)
    , m_effect(effect)
{
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
    AssetListType::AssetType type = AssetListType::AssetType(effectName.section(QLatin1Char('/'), -1).toInt());
    if (m_effect) {
        isAudio = type == AssetListType::AssetType::Audio || type == AssetListType::AssetType::CustomAudio || type == AssetListType::AssetType::TemplateAudio;
        isCustom = type == AssetListType::AssetType::CustomAudio || type == AssetListType::AssetType::Custom || type == AssetListType::AssetType::Template || type == AssetListType::AssetType::TemplateAudio;
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
