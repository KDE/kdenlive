/*
 * Copyright (c) 2013 Meltytech, LLC
 * Author: Dan Dennedy <dan@dennedy.org>
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

#include "colorwheel.h"

#include <KLocalizedString>
#include <QFrame>
#include <QVBoxLayout>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QFontDatabase>

#include <qmath.h>
#include <utility>

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

qreal NegQColor::redF() const
{
    return qcolor.redF() * sign_r;
}

void NegQColor::setRedF(qreal val)
{
    sign_r = val < 0 ? -1 : 1;
    qcolor.setRedF(val * sign_r);
}

qreal NegQColor::greenF() const
{
    return qcolor.greenF() * sign_g;
}

void NegQColor::setGreenF(qreal val)
{
    sign_g = val < 0 ? -1 : 1;
    qcolor.setGreenF(val * sign_g);
}

qreal NegQColor::blueF() const
{
    return qcolor.blueF() * sign_b;
}

void NegQColor::setBlueF(qreal val)
{
    sign_b = val < 0 ? -1 : 1;
    qcolor.setBlueF(val * sign_b);
}

qreal NegQColor::valueF() const
{
    return qcolor.valueF() * sign_g;
}

void NegQColor::setValueF(qreal val)
{
    qcolor = QColor::fromHsvF(hueF(), saturationF(), val < 0 ? -val : val, 1.);
    sign_r = val < 0 ? -1 : 1;
    sign_g = val < 0 ? -1 : 1;
    sign_b = val < 0 ? -1 : 1;
}

int NegQColor::hue() const
{
    return qcolor.hue();
}

qreal NegQColor::hueF() const
{
    return qcolor.hueF();
}

qreal NegQColor::saturationF() const
{
    return qcolor.saturationF();
}

WheelContainer::WheelContainer(QString id, QString name, NegQColor color, int unitSize, QWidget *parent)
    : QWidget(parent)
    , m_id(std::move(id))
    , m_isMouseDown(false)
    , m_margin(0)
    , m_color(std::move(color))
    , m_isInWheel(false)
    , m_isInSquare(false)
    , m_unitSize(unitSize)
    , m_name(std::move(name))
{
    m_initialSize = QSize(m_unitSize * 11, m_unitSize * 11);
    m_sliderWidth = m_unitSize * 1.5;
    resize(m_initialSize);
    setMinimumSize(m_initialSize * .4);
    setMaximumSize(m_initialSize * 1.5);
    setCursor(Qt::CrossCursor);
}


void WheelContainer::setFactorDefaultZero(qreal factor, qreal defvalue, qreal zero)
{
    m_sizeFactor = factor;
    m_defaultValue = defvalue;
    m_zeroShift = zero;
}

NegQColor WheelContainer::color() const
{
    return m_color;
}

void WheelContainer::setColor(QList <double> values)
{
    const NegQColor color = NegQColor::fromRgbF(values.at(0) / m_sizeFactor, values.at(1) / m_sizeFactor, values.at(2) / m_sizeFactor);
    m_color = color;
    update();
}

void WheelContainer::setRedColor(double value)
{
    m_color.setRedF(value / m_sizeFactor);
    emit colorChange(m_color);
    update();
}

void WheelContainer::setGreenColor(double value)
{
    m_color.setGreenF(value / m_sizeFactor);
    emit colorChange(m_color);
    update();
}

void WheelContainer::setBlueColor(double value)
{
    m_color.setBlueF(value / m_sizeFactor);
    emit colorChange(m_color);
    update();
}

int WheelContainer::wheelSize() const
{
    return qMin(width() - m_sliderWidth, height());
}

NegQColor WheelContainer::colorForPoint(const QPointF &point)
{
    if (!m_image.valid(point.toPoint())) {
        return NegQColor();
    }
    if (m_isInWheel) {
        qreal w = wheelSize();
        qreal xf = qreal(point.x()) / w;
        qreal yf = 1.0 - qreal(point.y()) / w;
        qreal xp = 2.0 * xf - 1.0;
        qreal yp = 2.0 * yf - 1.0;
        qreal rad = qMin(hypot(xp, yp), 1.0);
        qreal theta = qAtan2(yp, xp);
        theta -= 105.0 / 360.0 * 2.0 * M_PI;
        if (theta < 0.0) {
            theta += 2.0 * M_PI;
        }
        qreal hue = (theta * 180.0 / M_PI) / 360.0;
        return NegQColor::fromHsvF(hue, rad, m_color.valueF());
    }
    if (m_isInSquare) {
        qreal value = 1.0 - qreal(point.y() - m_margin) / (wheelSize() - m_margin * 2);
        if (!qFuzzyCompare(m_zeroShift, 0.)) {
            value = value - m_zeroShift;
        }
        return NegQColor::fromHsvF(m_color.hueF(), m_color.saturationF(), value);
    }
    return {};
}

QSize WheelContainer::sizeHint() const
{
    return m_initialSize * .8;
}

QSize WheelContainer::minimumSizeHint() const
{
    return m_initialSize * .4;
}

void WheelContainer::wheelEvent(QWheelEvent *event)
{
    if (m_sliderRegion.contains(event->pos())) {
        double y = m_color.valueF();
        if (event->modifiers() & Qt::ShiftModifier) {
            y += event->angleDelta().y() > 0 ? 0.002 : -0.002;
        } else {
            y += event->angleDelta().y() > 0 ? 0.04 : -0.04;
        }
        m_color.setValueF(qBound(-m_zeroShift, y, 1. - m_zeroShift)); 
        changeColor(m_color);
    } else {
        QWidget::wheelEvent(event);
    }
}

void WheelContainer::mousePressEvent(QMouseEvent *event)
{
    if (event->modifiers() & Qt::ShiftModifier) {
        QPoint clicked = event->pos();
        if (m_wheelRegion.contains(clicked)) {
            QPointF current = pointForColor();
            QPointF diff = clicked - current;
            double factor = fabs(diff.x()) > fabs(diff.y()) ? fabs(diff.x()) : fabs(diff.y());
            diff /= factor;
            m_lastPoint = current + diff;
        } else if (m_sliderRegion.contains(clicked)) {
            double y = yForColor();
            int offset = clicked.y() > y ? 1 : -1;
            m_lastPoint = QPointF(clicked.x(), y + offset);
        } else {
            return;
        }
    } else {
        m_lastPoint = event->pos();
    }
    if (m_wheelRegion.contains(m_lastPoint.toPoint())) {
        m_isInWheel = true;
        m_isInSquare = false;
        if (event->button() == Qt::LeftButton) {
            changeColor(colorForPoint(m_lastPoint));
        } else {
            // reset to default on middle/right button
            qreal r = m_color.redF();
            qreal b = m_color.blueF();
            qreal g = m_color.greenF();
            qreal max = qMax(r, b);
            max = qMax(max, g);
            changeColor(NegQColor::fromRgbF(max, max, max));
        }
    } else if (m_sliderRegion.contains(m_lastPoint.toPoint())) {
        m_isInWheel = false;
        m_isInSquare = true;
        if (event->button() == Qt::LeftButton) {
            changeColor(colorForPoint(m_lastPoint));
        } else {
            NegQColor c;
            c = NegQColor::fromHsvF(m_color.hueF(), m_color.saturationF(), m_defaultValue / m_sizeFactor);
            changeColor(c);
        }
    }
    m_isMouseDown = true;
}

void WheelContainer::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_isMouseDown) {
        return;
    }
    if (event->modifiers() & Qt::ShiftModifier) {
        if (m_isInWheel) {
            QPointF diff = event->pos() - m_lastPoint;
            double factor = fabs(diff.x()) > fabs(diff.y()) ? fabs(diff.x()) : fabs(diff.y());
            diff /= factor;
            m_lastPoint += diff;
        } else if (m_isInSquare) {
            double y = yForColor();
            int offset = event->pos().y() > y ? 1 : -1;
            m_lastPoint = QPointF(event->pos().x(), y + offset);
        } else {
            return;
        }
    } else {
        m_lastPoint = event->pos();
    }
    if (m_wheelRegion.contains(m_lastPoint.toPoint()) && m_isInWheel) {
        const NegQColor color = colorForPoint(m_lastPoint);
        changeColor(color);
    } else if (m_sliderRegion.contains(m_lastPoint.toPoint()) && m_isInSquare) {
        const NegQColor color = colorForPoint(m_lastPoint);
        changeColor(color);
    }
}

void WheelContainer::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    m_isMouseDown = false;
    m_isInWheel = false;
    m_isInSquare = false;
}

void WheelContainer::resizeEvent(QResizeEvent *event)
{
    m_image = QImage(event->size(), QImage::Format_ARGB32_Premultiplied);
    m_image.fill(palette().window().color().rgb());

    drawWheel();
    drawSlider();
    update();
}

const QString WheelContainer::getParamValues() const
{
    return QString::number(m_color.redF() * m_sizeFactor, 'g', 3) + QLatin1Char(',') + QString::number(m_color.greenF() * m_sizeFactor, 'g', 3) +
           QLatin1Char(',') + QString::number(m_color.blueF() * m_sizeFactor, 'g', 3);
}

const QList <double> WheelContainer::getNiceParamValues() const
{
    return {m_color.redF()* m_sizeFactor, m_color.greenF()* m_sizeFactor , m_color.blueF()* m_sizeFactor};
}

void WheelContainer::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    //    QStyleOption opt;
    //    opt.init(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawImage(0, 0, m_image);
    // painter.drawRect(0, 0, width(), height());
    //painter.drawText(m_margin, wheelSize() + m_unitSize - m_margin, m_name + QLatin1Char(' ') + getParamValues());
    drawWheelDot(painter);
    drawSliderBar(painter);
    //    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}

void WheelContainer::drawWheel()
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
    painter.drawEllipse(QPointF(0, 0), r / 2 - m_margin, r / 2 - m_margin);

    QBrush saturationBrush(radialGradient);
    painter.setBrush(saturationBrush);
    painter.drawEllipse(QPointF(0, 0), r / 2 - m_margin, r / 2 - m_margin);
    
    painter.setBrush(Qt::gray);
    painter.setOpacity(0.4);
    painter.drawEllipse(QPointF(0, 0), r / 2 - m_unitSize * .6, r / 2 - m_unitSize * .6);

    m_wheelRegion = QRegion(r / 2, r / 2, r - 2 * m_margin, r - 2 * m_margin, QRegion::Ellipse);
    m_wheelRegion.translate(-(r - 2 * m_margin) / 2, -(r - 2 * m_margin) / 2);
}

void WheelContainer::drawSlider()
{
    QPainter painter(&m_image);
    painter.setRenderHint(QPainter::Antialiasing);
    int ws = wheelSize() + m_unitSize * .2;
    qreal scale = qreal(ws + m_sliderWidth) / maximumWidth();
    int w = m_sliderWidth * scale - m_unitSize * .4;
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

void WheelContainer::drawWheelDot(QPainter &painter)
{
    int r = wheelSize() / 2;
    QPen pen(Qt::white);
    pen.setWidth(2);
    painter.setPen(pen);
    painter.setBrush(Qt::black);
    painter.translate(r, r);
    painter.rotate(360.0 - m_color.hue());
    painter.rotate(-105);
    //    r -= margin;
    painter.drawEllipse(QPointF(m_color.saturationF() * r, 0.0), 4, 4);
    painter.resetTransform();
}

QPointF WheelContainer::pointForColor()
{
    int r = wheelSize() / 2;
    QTransform transform;
    transform.translate(r, r);
    transform.rotate(255 - m_color.hue());
    transform.translate(m_color.saturationF() * r, 0);
    return transform.map(QPointF(0, 0));
}

double WheelContainer::yForColor()
{
    qreal value = 1.0 - m_color.valueF();
    if (m_id == QLatin1String("lift")) {
        value -= m_zeroShift;
    }
    int ws = wheelSize();
    int h = ws - m_margin * 2;
    return m_margin + value * h;
}


void WheelContainer::drawSliderBar(QPainter &painter)
{
    qreal value = 1.0 - m_color.valueF();
    if (m_id == QLatin1String("lift")) {
        value -= m_zeroShift;
    }
    int ws = wheelSize();
    qreal scale = qreal(ws + m_sliderWidth) / maximumWidth();
    int w = m_sliderWidth * scale;
    int h = ws - m_margin * 2;
    QPen pen(Qt::white);
    pen.setWidth(2);
    painter.setPen(pen);
    painter.setBrush(Qt::black);
    painter.translate(ws, m_margin + value * h);
    painter.drawRect(0, 0, w, 4);
    painter.resetTransform();
}

void WheelContainer::changeColor(const NegQColor &color)
{
    m_color = color;
    drawWheel();
    drawSlider();
    update();
    emit colorChange(m_color);
}


ColorWheel::ColorWheel(QString id, QString name, NegQColor color, QWidget *parent)
    : QWidget(parent)
{
    QFontInfo info(font());
    int unitSize = info.pixelSize();
    QVBoxLayout *lay = new QVBoxLayout(this);
    m_wheelName = new QLabel(name, this);
    m_wheelName->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    lay->addWidget(m_wheelName);
    m_container = new WheelContainer(id, name, color, unitSize, this);
    QHBoxLayout *hb = new QHBoxLayout;
    m_redEdit = new QDoubleSpinBox(this);
    m_redEdit->setPrefix(i18n("R: "));
    m_redEdit->setFrame(QFrame::NoFrame);
    m_redEdit->setDecimals(3);
    m_redEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_redEdit->setFont(m_wheelName->font());
    m_redEdit->setObjectName(QStringLiteral("dragMinimal"));
    m_greenEdit = new QDoubleSpinBox(this);
    m_greenEdit->setPrefix(i18n("G: "));
    m_greenEdit->setObjectName(QStringLiteral("dragMinimal"));
    m_greenEdit->setFont(m_wheelName->font());
    m_greenEdit->setDecimals(3);
    m_greenEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_blueEdit = new QDoubleSpinBox(this);
    m_blueEdit->setPrefix(i18n("B: "));
    m_blueEdit->setObjectName(QStringLiteral("dragMinimal"));
    m_blueEdit->setFont(m_wheelName->font());
    m_blueEdit->setDecimals(3);
    m_blueEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    lay->setContentsMargins(0, 0, 2, 0);
    lay->setSpacing(0);
    lay->addWidget(m_container);
    hb->addWidget(m_redEdit);
    hb->addWidget(m_greenEdit);
    hb->addWidget(m_blueEdit);
    hb->setSpacing(0);
    hb->setContentsMargins(0, 0, 0, 0);
    lay->addLayout(hb);
    m_container->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    connect(m_container, &WheelContainer::colorChange, this, [&] (const NegQColor &col) {
        QList <double> vals = m_container->getNiceParamValues();
        m_redEdit->blockSignals(true);
        m_greenEdit->blockSignals(true);
        m_blueEdit->blockSignals(true);
        m_redEdit->setValue(vals.at(0));
        m_greenEdit->setValue(vals.at(1));
        m_blueEdit->setValue(vals.at(2));
        m_redEdit->blockSignals(false);
        m_greenEdit->blockSignals(false);
        m_blueEdit->blockSignals(false);
        emit colorChange(col);
    });
    connect(m_redEdit, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [&]() {
        m_container->setRedColor(m_redEdit->value());
    });
    connect(m_greenEdit, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [&]() {
        m_container->setGreenColor(m_greenEdit->value());
    });
    connect(m_blueEdit, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [&]() {
        m_container->setBlueColor(m_blueEdit->value());
    });
    setMinimumHeight(m_wheelName->height() + m_container->minimumHeight() + m_redEdit->height());
    setMaximumWidth(m_container->maximumWidth());
    setMinimumWidth(3 * m_redEdit->sizeHint().width());
}


NegQColor ColorWheel::color() const
{
    return m_container->color();
}

void ColorWheel::setColor(QList<double> values)
{
    m_container->setColor(values);
    m_redEdit->blockSignals(true);
    m_greenEdit->blockSignals(true);
    m_blueEdit->blockSignals(true);
    m_redEdit->setValue(values.at(0));
    m_greenEdit->setValue(values.at(1));
    m_blueEdit->setValue(values.at(2));
    m_redEdit->blockSignals(false);
    m_greenEdit->blockSignals(false);
    m_blueEdit->blockSignals(false);
}

void ColorWheel::setFactorDefaultZero(qreal factor, qreal defvalue, qreal zero)
{
    m_container->setFactorDefaultZero(factor, defvalue, zero);
    if (zero > 0) {
        // Lift has a special range
        m_redEdit->setRange(-1, 1);
        m_greenEdit->setRange(-1, 1);
        m_blueEdit->setRange(-1, 1);
    } else {
        m_redEdit->setRange(0, factor);
        m_greenEdit->setRange(0, factor);
        m_blueEdit->setRange(0, factor);
    }
    m_redEdit->setSingleStep(.01);
    m_greenEdit->setSingleStep(.01);
    m_blueEdit->setSingleStep(.01);
}

