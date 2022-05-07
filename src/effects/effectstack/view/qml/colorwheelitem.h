/*
    SPDX-FileCopyrightText: 2013-2014 Meltytech LLC
    SPDX-FileCopyrightText: 2013-2014 Dan Dennedy <dan@dennedy.org>
    SPDX-FileCopyrightText: 2013-2014 Brian Matherly <pez4brian@yahoo.com>
    SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org> small adaptations for Kdenlive

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "utils/qcolorutils.h"

#include <QImage>
#include <QQuickPaintedItem>

class ColorWheelItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color NOTIFY colorChanged)
    Q_PROPERTY(double red READ red)
    Q_PROPERTY(double green READ green)
    Q_PROPERTY(double blue READ blue)
public:
    explicit ColorWheelItem(QQuickItem *parent = 0);
    QColor color();
    void setColor(const NegQColor &color);
    Q_INVOKABLE void setColor(double r, double g, double b);
    double red();
    double green();
    double blue();
    Q_INVOKABLE void setFactorDefaultZero(qreal factor, qreal defvalue, qreal zero);

signals:
    void colorChanged();

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void hoverMoveEvent(QHoverEvent *event);
    void paint(QPainter *painter);

private:
    QImage m_image;
    QPoint m_lastPoint;
    QSize m_size;
    int m_margin;
    QRegion m_wheelRegion;
    QRegion m_sliderRegion;
    NegQColor m_color;
    bool m_isInWheel;
    bool m_isInSquare;
    qreal m_sizeFactor;
    qreal m_defaultValue;
    qreal m_zeroShift;

    int wheelSize() const;
    NegQColor colorForPoint(const QPoint &point);
    void drawWheel();
    void drawWheelDot(QPainter &painter);
    void drawSliderBar(QPainter &painter);
    void drawSlider();
    void updateCursor(const QPoint &pos);
};
