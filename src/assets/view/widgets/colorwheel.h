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

#ifndef COLORWHEELPARAM_H
#define COLORWHEELPARAM_H

#include <QPainter>
#include <QResizeEvent>
#include <QWidget>

class QDoubleSpinBox;
class QLabel;

class NegQColor
{
public:
    int8_t sign_r = 1;
    int8_t sign_g = 1;
    int8_t sign_b = 1;
    QColor qcolor;
    static NegQColor fromHsvF(qreal h, qreal s, qreal l, qreal a = 1.0);
    static NegQColor fromRgbF(qreal r, qreal g, qreal b, qreal a = 1.0);
    qreal redF() const;
    qreal greenF() const;
    qreal blueF() const;
    qreal valueF() const;
    int hue() const;
    qreal hueF() const;
    qreal saturationF() const;
    void setRedF(qreal val);
    void setGreenF(qreal val);
    void setBlueF(qreal val);
    void setValueF(qreal val);
};

class WheelContainer : public QWidget
{
    Q_OBJECT
public:
    explicit WheelContainer(QString id, QString name, NegQColor color, int unitSize, QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    NegQColor color() const;
    void setColor(QList <double> values);
    void setFactorDefaultZero(qreal factor, qreal defvalue, qreal zero);
    const QList <double> getNiceParamValues() const;
    void setRedColor(double value);
    void setGreenColor(double value);
    void setBlueColor(double value);

public slots:
    void changeColor(const NegQColor &color);

signals:
    void colorChange(const NegQColor &color);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_id;
    QSize m_initialSize;
    QImage m_image;
    bool m_isMouseDown;
    QPointF m_lastPoint;
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
    NegQColor colorForPoint(const QPointF &point);
    QPointF pointForColor();
    double yForColor();
    void drawWheel();
    void drawWheelDot(QPainter &painter);
    void drawSliderBar(QPainter &painter);
    void drawSlider();
    const QString getParamValues() const;
};

class ColorWheel : public QWidget
{
    Q_OBJECT
public:
    explicit ColorWheel(QString id, QString name, NegQColor color, QWidget *parent = nullptr);
    NegQColor color() const;
    void setColor(QList <double> values);
    void setFactorDefaultZero(qreal factor, qreal defvalue, qreal zero);

private:
    WheelContainer *m_container;
    QLabel *m_wheelName;
    QDoubleSpinBox *m_redEdit;
    QDoubleSpinBox *m_greenEdit;
    QDoubleSpinBox *m_blueEdit;

signals:
    void colorChange(const NegQColor &color);
};

#endif // COLORWHEEL_H
