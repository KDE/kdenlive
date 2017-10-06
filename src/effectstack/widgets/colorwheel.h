/*
 * Copyright (c) 2013 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef COLORWHEEL_H
#define COLORWHEEL_H

#include <QWidget>
#include <QPainter>
#include <QResizeEvent>

class NegQColor
{
public:
    int8_t sign_r=1;
    int8_t sign_g=1;
    int8_t sign_b=1;
    QColor qcolor;
    static NegQColor fromHsvF(qreal h, qreal s, qreal l, qreal a = 1.0);
    static NegQColor fromRgbF(qreal r, qreal g, qreal b, qreal a = 1.0);
    qreal redF();
    qreal greenF();
    qreal blueF();
    qreal valueF();
    int hue();
    qreal hueF();
    qreal saturationF();
};

class ColorWheel : public QWidget
{
    Q_OBJECT
public:
    explicit ColorWheel(const QString &id, const QString &name, const NegQColor &color, QWidget *parent = nullptr);

    QSize sizeHint() const Q_DECL_OVERRIDE;
    QSize minimumSizeHint() const Q_DECL_OVERRIDE;
    NegQColor color();
    void setColor(const NegQColor &color);

    void setFactorDefaultZero(qreal factor, qreal defvalue, qreal zero);

signals:
    void colorChange(const NegQColor &color);

public slots:
    void changeColor(const NegQColor &color);

protected:
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent *event) Q_DECL_OVERRIDE;

private:
    QString m_id;
    QSize m_initialSize;
    QImage m_image;
    bool m_isMouseDown;
    QPoint m_lastPoint;
    int m_margin;
    int m_sliderWidth;
    QRegion m_wheelRegion;
    QRegion m_sliderRegion;
    NegQColor m_color;
    bool m_isInWheel;
    bool m_isInSquare;
    int m_unitSize;
    QString m_name;

    qreal m_sizeFactor = 1;
    qreal m_defaultValue = 1;
    qreal m_zeroShift = 0;

    int wheelSize() const;
    NegQColor colorForPoint(const QPoint &point);
    void drawWheel();
    void drawWheelDot(QPainter &painter);
    void drawSliderBar(QPainter &painter);
    void drawSlider();
    QString getParamValues();
};

#endif // COLORWHEEL_H
