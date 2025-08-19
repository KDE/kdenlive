/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QLineF>
#include <QRectF>

namespace PainterUtils {

// Adjusts a QRectF for pen width so the border will be fully contained within the rect, not spilling over the edges.
inline QRectF adjustedForPen(const QRectF &rect, qreal penWidth)
{
    qreal delta = penWidth / 2.0;
    return rect.adjusted(delta, delta, -delta, -delta);
}

// Adjusts a horizontal line for pen width so the line start and end points are fully contained within the two points.
inline QLineF adjustedHorizontalLine(qreal y, qreal x1, qreal x2, qreal penWidth)
{
    qreal delta = penWidth / 2.0;
    return QLineF(x1 + delta, y, x2 - delta, y);
}

// Adjusts a vertical line for pen width so the line start and end points are fully contained within the two points.
inline QLineF adjustedVerticalLine(qreal x, qreal y1, qreal y2, qreal penWidth)
{
    qreal delta = penWidth / 2.0;
    return QLineF(x, y1 + delta, x, y2 - delta);
}

} // namespace PainterUtils