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

class ColorWheel : public QWidget
{
    Q_OBJECT
public:
    explicit ColorWheel(const QString &id, const QString &name, const QColor &color, QWidget *parent = nullptr);

    QSize sizeHint() const Q_DECL_OVERRIDE;
    QSize minimumSizeHint() const Q_DECL_OVERRIDE;
    QColor color();
    void setColor(const QColor &color);

signals:
    void colorChange(const QColor &color);

public slots:
    void changeColor(const QColor &color);

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
    QColor m_color;
    bool m_isInWheel;
    bool m_isInSquare;
    int m_unitSize;
    QString m_name;

    int wheelSize() const;
    QColor colorForPoint(const QPoint &point);
    void drawWheel();
    void drawWheelDot(QPainter &painter);
    void drawSliderBar(QPainter &painter);
    void drawSlider();
    QString getParamValues();
};

#endif // COLORWHEEL_H
