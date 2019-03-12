/*
 * Copyright (c) 2013-2016 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
 * Author: Brian Matherly <pez4brian@yahoo.com>
 * Author: Jean-Baptiste Mardelle <jb@kdenlive.org> small adaptations for Kdenlive
 * Some ideas came from Qt-Plus: https://github.com/liuyanghejerry/Qt-Plus
 * and Steinar Gunderson's Movit demo app.
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

#include "colorwheelitem.h"
#include <QApplication>
#include <QCursor>
#include <QPainter>
#include <cstdio>
#include <qmath.h>

static const qreal WHEEL_SLIDER_RATIO = 10.0;

NegQColor NegQColor::fromHsvF(qreal h, qreal s, qreal l, qreal a)
{
    NegQColor color;
    color.qcolor = QColor::fromHsvF(h, s, l < 0 ? -l : l, a);
    color.sign_r = l < 0 ? -1 : 1;
    color.sign_g = l < 0 ? -1 : 1;
    color.sign_b = l < 0 ? -1 : 1;
    return color;
}

NegQColor NegQColor::fromRgbF(qreal r, qreal g, qreal b, qreal a)
{
    NegQColor color;
    color.qcolor = QColor::fromRgbF(r < 0 ? -r : r, g < 0 ? -g : g, b < 0 ? -b : b, a);
    color.sign_r = r < 0 ? -1 : 1;
    color.sign_g = g < 0 ? -1 : 1;
    color.sign_b = b < 0 ? -1 : 1;
    return color;
}
qreal NegQColor::redF()
{
    return qcolor.redF() * sign_r;
}
qreal NegQColor::greenF()
{
    return qcolor.greenF() * sign_g;
}
qreal NegQColor::blueF()
{
    return qcolor.blueF() * sign_b;
}
qreal NegQColor::valueF()
{
    return qcolor.valueF() * sign_g;
}
int NegQColor::hue()
{
    return qcolor.hue();
}
qreal NegQColor::hueF()
{
    return qcolor.hueF();
}
qreal NegQColor::saturationF()
{
    return qcolor.saturationF();
}

ColorWheelItem::ColorWheelItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , m_image()
    , m_lastPoint(0, 0)
    , m_size(0, 0)
    , m_margin(5)
    , m_color(NegQColor())
    , m_isInWheel(false)
    , m_isInSquare(false)
    , m_sizeFactor(1)
    , m_defaultValue(1)
    , m_zeroShift(0)
{
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MidButton);
    setAcceptHoverEvents(true);
}

void ColorWheelItem::setFactorDefaultZero(qreal factor, qreal defvalue, qreal zero)
{
    m_sizeFactor = factor;
    m_defaultValue = defvalue;
    m_zeroShift = zero;
}

QColor ColorWheelItem::color()
{
    return QColor(m_color.redF() * m_sizeFactor, m_color.greenF() * m_sizeFactor, m_color.blueF() * m_sizeFactor);
}

void ColorWheelItem::setColor(double r, double g, double b)
{
    m_color = NegQColor::fromRgbF(r, g, b);
    update();
}

void ColorWheelItem::setColor(const NegQColor &color)
{
    m_color = color;
    update();
    emit colorChanged();
}

double ColorWheelItem::red()
{
    return m_color.redF() * m_sizeFactor;
}

double ColorWheelItem::green()
{
    return m_color.greenF() * m_sizeFactor;
}

double ColorWheelItem::blue()
{
    return m_color.blueF() * m_sizeFactor;
}

int ColorWheelItem::wheelSize() const
{
    qreal ws = (qreal)width() / (1.0 + 1.0 / WHEEL_SLIDER_RATIO);
    return qMin(ws, height());
}

NegQColor ColorWheelItem::colorForPoint(const QPoint &point)
{
    if (!m_image.valid(point)) return NegQColor();
    if (m_isInWheel) {
        qreal w = wheelSize();
        qreal xf = qreal(point.x()) / w;
        qreal yf = 1.0 - qreal(point.y()) / w;
        qreal xp = 2.0 * xf - 1.0;
        qreal yp = 2.0 * yf - 1.0;
        qreal rad = qMin(hypot(xp, yp), 1.0);
        qreal theta = qAtan2(yp, xp);
        theta -= 105.0 / 360.0 * 2.0 * M_PI;
        if (theta < 0.0) theta += 2.0 * M_PI;
        qreal hue = (theta * 180.0 / M_PI) / 360.0;
        return NegQColor::fromHsvF(hue, rad, m_color.valueF());
    }
    if (m_isInSquare) {
        qreal value = 1.0 - qreal(point.y() - m_margin) / (wheelSize() - m_margin * 2);
        if (qAbs(m_zeroShift) > 0) {
            value = value - m_zeroShift;
        }
        return NegQColor::fromHsvF(m_color.hueF(), m_color.saturationF(), value);
    }
    return NegQColor();
}

void ColorWheelItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_lastPoint = event->pos();
        event->accept();
        if (m_wheelRegion.contains(m_lastPoint)) {
            m_isInWheel = true;
            m_isInSquare = false;
            NegQColor color = colorForPoint(m_lastPoint);
            setColor(color);
        } else if (m_sliderRegion.contains(m_lastPoint)) {
            m_isInWheel = false;
            m_isInSquare = true;
            NegQColor color = colorForPoint(m_lastPoint);
            setColor(color);
        }
    } else if (event->button() == Qt::RightButton) {
        NegQColor color = NegQColor::fromRgbF(m_defaultValue / m_sizeFactor, m_defaultValue / m_sizeFactor, m_defaultValue / m_sizeFactor);
        setColor(color);
        event->accept();
    } else {
        event->ignore();
    }
}

void ColorWheelItem::mouseMoveEvent(QMouseEvent *event)
{
    updateCursor(event->pos());
    if (event->buttons() == Qt::NoButton) return;
    m_lastPoint = event->pos();
    if (m_isInWheel) {
        NegQColor color = colorForPoint(m_lastPoint);
        setColor(color);
    } else if (m_isInSquare) {
        NegQColor color = colorForPoint(m_lastPoint);
        setColor(color);
    }
    event->accept();
}

void ColorWheelItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isInWheel = false;
        m_isInSquare = false;
    }
    event->accept();
}

void ColorWheelItem::hoverMoveEvent(QHoverEvent *event)
{
    updateCursor(event->pos());
}

void ColorWheelItem::paint(QPainter *painter)
{
    QSize size(width(), height());

    if (m_size != size) {
        m_image = QImage(QSize(width(), height()), QImage::Format_ARGB32_Premultiplied);
        m_image.fill(qRgba(0, 0, 0, 0));
        drawWheel();
        drawSlider();
        m_size = size;
    }

    painter->setRenderHint(QPainter::Antialiasing);
    painter->drawImage(0, 0, m_image);
    drawWheelDot(*painter);
    drawSliderBar(*painter);
}

void ColorWheelItem::drawWheel()
{
    int r = wheelSize();
    QPainter painter(&m_image);
    painter.setRenderHint(QPainter::Antialiasing);
    m_image.fill(0); // transparent

    QConicalGradient conicalGradient;
    conicalGradient.setColorAt(0.0, Qt::red);
    conicalGradient.setColorAt(60.0 / 360.0, Qt::yellow);
    conicalGradient.setColorAt(135.0 / 360.0, Qt::green);
    conicalGradient.setColorAt(180.0 / 360.0, Qt::cyan);
    conicalGradient.setColorAt(240.0 / 360.0, Qt::blue);
    conicalGradient.setColorAt(315.0 / 360.0, Qt::magenta);
    conicalGradient.setColorAt(1.0, Qt::red);

    QRadialGradient radialGradient(0.0, 0.0, r / 2);
    radialGradient.setColorAt(0.0, Qt::white);
    radialGradient.setColorAt(1.0, Qt::transparent);

    painter.translate(r / 2, r / 2);
    painter.rotate(-105);

    QBrush hueBrush(conicalGradient);
    painter.setPen(Qt::NoPen);
    painter.setBrush(hueBrush);
    painter.drawEllipse(QPoint(0, 0), r / 2 - m_margin, r / 2 - m_margin);

    QBrush saturationBrush(radialGradient);
    painter.setBrush(saturationBrush);
    painter.drawEllipse(QPoint(0, 0), r / 2 - m_margin, r / 2 - m_margin);

    m_wheelRegion = QRegion(r / 2, r / 2, r - 2 * m_margin, r - 2 * m_margin, QRegion::Ellipse);
    m_wheelRegion.translate(-(r - 2 * m_margin) / 2, -(r - 2 * m_margin) / 2);
}

void ColorWheelItem::drawWheelDot(QPainter &painter)
{
    int r = wheelSize() / 2;
    QPen pen(Qt::white);
    pen.setWidth(2);
    painter.setPen(pen);
    painter.setBrush(Qt::black);
    painter.translate(r, r);
    painter.rotate(360.0 - m_color.hue());
    painter.rotate(-105);
    painter.drawEllipse(QPointF(m_color.saturationF() * r, 0.0), 4, 4);
    painter.resetTransform();
}

void ColorWheelItem::drawSliderBar(QPainter &painter)
{
    qreal value = 1.0 - m_color.valueF();
    if (qAbs(m_zeroShift) > 0) {
        value -= m_zeroShift;
    }
    int ws = wheelSize() * qApp->devicePixelRatio();
    int w = (qreal)ws / WHEEL_SLIDER_RATIO;
    int h = ws - m_margin * 2;
    QPen pen(Qt::white);
    pen.setWidth(2);
    painter.setPen(pen);
    painter.setBrush(Qt::black);
    painter.translate(ws, m_margin + value * h);
    painter.drawRect(0, 0, w, 4);
    painter.resetTransform();
}

void ColorWheelItem::drawSlider()
{
    QPainter painter(&m_image);
    painter.setRenderHint(QPainter::Antialiasing);
    int ws = wheelSize();
    int w = (qreal)ws / WHEEL_SLIDER_RATIO;
    int h = ws - m_margin * 2;
    QLinearGradient gradient(0, 0, w, h);
    gradient.setColorAt(0.0, Qt::white);
    gradient.setColorAt(1.0, Qt::black);
    QBrush brush(gradient);
    painter.setPen(Qt::NoPen);
    painter.setBrush(brush);
    painter.translate(ws, m_margin);
    painter.drawRect(0, 0, w, h);
    m_sliderRegion = QRegion(ws, m_margin, w, h);
}

void ColorWheelItem::updateCursor(const QPoint &pos)
{
    if (m_wheelRegion.contains(pos) || m_sliderRegion.contains(pos)) {
        setCursor(QCursor(Qt::CrossCursor));
    } else {
        unsetCursor();
    }
}
