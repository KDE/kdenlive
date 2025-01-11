/*
    SPDX-FileCopyrightText: 2015-2016 Meltytech LLC
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "timelinerecwaveform.h"
#include "capture/mediacapture.h"
#include "core.h"
#include <QPainter>
#include <QPainterPath>

TimelineRecWaveform::TimelineRecWaveform(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , m_repaint(false)
    , m_opaquePaint(false)
{
    setAntialiasing(false);
    setOpaquePainting(m_opaquePaint);
    setEnabled(false);
    m_precisionFactor = 1;
    // setRenderTarget(QQuickPaintedItem::FramebufferObject);
    // setMipmap(true);
    // setTextureSize(QSize(1, 1));
    connect(this, &TimelineRecWaveform::propertyChanged, this, static_cast<void (QQuickItem::*)()>(&QQuickItem::update));
}

void TimelineRecWaveform::paint(QPainter *painter)
{
    const QVector<double> &audioLevels = pCore->getAudioDevice()->recLevels();
    if (audioLevels.isEmpty()) {
        return;
    }

    if (m_outPoint == m_inPoint) {
        return;
    }
    QRectF bgRect(0, 0, width(), height());
    if (m_opaquePaint) {
        painter->fillRect(bgRect, m_bgColor);
    }
    QPen pen(painter->pen());
    int maxLength = audioLevels.length();
    double increment = 1 / m_scale;
    qreal indicesPrPixel = m_channels / m_scale; // qreal(m_outPoint - m_inPoint) / width() * m_precisionFactor;
    int h = int(height());
    double offset = 0;
    bool pathDraw = increment > 1.2;
    if (increment > 1. && !pathDraw) {
        pen.setWidth(int(ceil(increment)));
        offset = pen.width() / 2.;
        pen.setColor(m_color);
        pen.setCapStyle(Qt::FlatCap);
    } else if (pathDraw) {
        pen.setWidth(0);
        painter->setBrush(m_color);
        pen.setColor(m_bgColor.darker(200));
    } else {
        pen.setColor(m_color);
    }
    painter->setPen(pen);
    int startPos = int(m_inPoint / indicesPrPixel);
    // Draw merged channels
    double i = 0;
    int j = 0;
    int idx = 0;
    QPainterPath path;
    if (pathDraw) {
        path.moveTo(j - 1, height());
    }
    for (; i <= width(); j++) {
        double level;
        i = j * increment;
        idx = qCeil((startPos + i) * indicesPrPixel);
        idx += idx % m_channels;
        i -= offset;
        if (idx + m_channels >= maxLength || idx < 0) {
            break;
        }
        level = audioLevels.at(idx);
        if (pathDraw) {
            double val = height() - level * height();
            path.lineTo(i, val);
            path.lineTo((j + 1) * increment - offset, val);
        } else {
            painter->drawLine(int(i), h, int(i), int(h - (h * level)));
        }
    }
    if (pathDraw) {
        path.lineTo(i, height());
        painter->drawPath(path);
    }
}
