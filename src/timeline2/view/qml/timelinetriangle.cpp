/*
    SPDX-FileCopyrightText: 2015-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "timelinetriangle.h"
#include "assets/keyframes/model/keyframemodel.hpp"
#include <QPainter>
#include <QPainterPath>

TimelineTriangle::TimelineTriangle(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &TimelineTriangle::curveChanged, this, [&]() { update(); });
}
void TimelineTriangle::paint(QPainter *painter)
{
    QPainterPath path;
    path.moveTo(0, height());
    path.lineTo(0, 0);
    path.lineTo(width(), 0);
    switch (m_curveType) {
    case int(KeyframeType::CubicIn): {
        if (m_endFade) {
            double factor = width() / height();
            path.quadTo(width() * 0.1, height() * 0.1 * factor, 0, height());
        } else {
            double factor = width() / height();
            path.quadTo(width() - width() * 0.1, height() - height() * 0.1 * factor, 0, height());
        }
        break;
    }
    case int(KeyframeType::ExponentialIn): {
        if (m_endFade) {
            double factor = width() / height();
            path.cubicTo(width() * 0.15 * factor, 0, 0, height() * 0.15, 0, height());
        } else {
            double factor = width() / height();
            path.cubicTo(width(), height() - height() * 0.15, width() - width() * 0.15 * factor, height(), 0, height());
        }
        break;
    }
    case int(KeyframeType::CubicOut): {
        if (m_endFade) {
            double factor = width() / height();
            path.quadTo(width() - width() * 0.1, height() - height() * 0.1 * factor, 0, height());
        } else {
            double factor = width() / height();
            path.quadTo(width() * 0.1, height() * 0.1 * factor, 0, height());
        }
        break;
    }
    case int(KeyframeType::ExponentialOut): {
        if (m_endFade) {
            double factor = width() / height();
            path.cubicTo(width(), height() - height() * 0.15, width() - width() * 0.15 * factor, height(), 0, height());
        } else {
            double factor = width() / height();
            path.cubicTo(width() * 0.15 * factor, 0, 0, height() * 0.15, 0, height());
        }
        break;
    }
    default:
        break;
    }
    painter->fillPath(path, m_color);
    painter->setPen(Qt::white);
    // painter->drawLine(int(width()), 0, 0, int(height()));
}
