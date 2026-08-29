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

void TimelineTriangle::buildQuadInPath(QPainterPath &path, double factor)
{
    if (m_endFade) {
        path.quadTo(width() * factor, height() * factor, 0, height());
    } else {
        path.quadTo(width() * (1. - factor), height() * (1. - factor), 0, height());
    }
}

void TimelineTriangle::buildQuadOutPath(QPainterPath &path, double factor)
{
    if (m_endFade) {
        path.quadTo(width() * (1. - factor), height() * (1. - factor), 0, height());
    } else {
        path.quadTo(width() * factor, height() * factor, 0, height());
    }
}

void TimelineTriangle::paint(QPainter *painter)
{
    QPainterPath path;
    path.moveTo(width(), 0);
    switch (m_curveType) {
    case int(mlt_keyframe_quadratic_in): {
        buildQuadInPath(path, 0.3);
        break;
    }
    case int(mlt_keyframe_cubic_in): {
        buildQuadInPath(path, 0.1);
        break;
    }
    case int(mlt_keyframe_quartic_in): {
        buildQuadInPath(path, 0.05);
        break;
    }
    case int(mlt_keyframe_quintic_in): {
        buildQuadInPath(path, 0.02);
        break;
    }
    case int(mlt_keyframe_exponential_in): {
        double offset = height() * 0.15;
        if (m_endFade) {
            path.cubicTo(width() - offset, 0, 0, offset, 0, height());
        } else {
            path.cubicTo(width(), height() - offset, width() - offset, height(), 0, height());
        }
        break;
    }
    case int(mlt_keyframe_quadratic_out): {
        buildQuadOutPath(path, 0.3);
        break;
    }
    case int(mlt_keyframe_cubic_out): {
        buildQuadOutPath(path, 0.1);
        break;
    }
    case int(mlt_keyframe_quartic_out): {
        buildQuadOutPath(path, 0.05);
        break;
    }
    case int(mlt_keyframe_quintic_out): {
        buildQuadOutPath(path, 0.02);
        break;
    }
    case int(mlt_keyframe_exponential_out): {
        double offset = height() * 0.15;
        if (m_endFade) {
            path.cubicTo(width(), height() - offset, width() - offset, height(), 0, height());
        } else {
            path.cubicTo(offset, 0, 0, offset, 0, height());
        }
        break;
    }
    default:
        path.lineTo(0, height());
        break;
    }
    QPen pen = painter->pen();
    QPainterPathStroker stroke;
    stroke.setWidth(2);
    auto outline = stroke.createStroke(path);
    path.lineTo(0, 0);
    painter->setOpacity(0.3);
    painter->fillPath(path, m_color);
    painter->setOpacity(1);
    painter->fillPath(outline, Qt::white);
}
