/*
    SPDX-FileCopyrightText: 2013 Meltytech LLC
    SPDX-FileCopyrightText: 2013 Dan Dennedy <dan@dennedy.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

    Some ideas came from Qt-Plus: https://github.com/liuyanghejerry/Qt-Plus
    and Steinar Gunderson's Movit demo app.
*/

#include "colorwheel.h"

#include <KLocalizedString>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

#include <qmath.h>
#include <utility>

WheelContainer::WheelContainer(QString id, QString name, NegQColor color, int unitSize, QWidget *parent)
    : QWidget(parent)
    , m_id(std::move(id))
    , m_isMouseDown(false)
    , m_margin(2)
    , m_color(std::move(color))
    , m_defaultColor(m_color)
    , m_sourceColor(m_color)
    , m_unitSize(unitSize)
    , m_name(std::move(name))
    , m_wheelClick(false)
    , m_sliderClick(false)
    , m_sliderFocus(false)
{
    setMouseTracking(true);
    m_initialSize = QSize(m_unitSize * 11, m_unitSize * 11);
    m_sliderWidth = int(m_unitSize * 1.5);
    m_sliderBorder = m_unitSize * .2;
    resize(m_initialSize);
    setMinimumSize(m_initialSize * .4);
    setMaximumSize(m_initialSize * 1.5);
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

void WheelContainer::setColor(const QList<double> &values)
{
    const NegQColor color = NegQColor::fromRgbF(values.at(0) / m_sizeFactor, values.at(1) / m_sizeFactor, values.at(2) / m_sizeFactor);
    m_color = color;
    drawSlider();
    update();
}

void WheelContainer::setRedColor(double value)
{
    m_sourceColor = m_color;
    m_color.setRedF(value / m_sizeFactor);
    Q_EMIT colorChange(m_sourceColor, m_color, true);
    update();
}

void WheelContainer::setGreenColor(double value)
{
    m_sourceColor = m_color;
    m_color.setGreenF(value / m_sizeFactor);
    Q_EMIT colorChange(m_sourceColor, m_color, true);
    update();
}

void WheelContainer::setBlueColor(double value)
{
    m_sourceColor = m_color;
    m_color.setBlueF(value / m_sizeFactor);
    Q_EMIT colorChange(m_sourceColor, m_color, true);
    update();
}

int WheelContainer::wheelSize() const
{
    return qMin(width() - m_sliderWidth, height());
}

int WheelContainer::sliderHeight() const
{
    return wheelSize() - m_margin * 2 - m_sliderBorder * 2;
}

NegQColor WheelContainer::colorForPoint(const QPointF &point)
{
    if (m_wheelClick) {
        qreal w = wheelSize();
        qreal xf;
        qreal yf;
        if (!m_wheelRegion.contains(point.toPoint())) {
            // if (xf < 0. || xf > 1. || yf < 0. || yf > 1.) {
            //  Cursor is outside of circle, calculate position on the circumference
            qreal dx = point.x() - (w / 2);
            qreal dy = (w / 2) - point.y();
            if (qFuzzyIsNull(dy)) {
                yf = 0.5;
                if (dx > 0.) {
                    xf = 1.;
                } else {
                    xf = -1;
                }
            } else {
                // Calculate angle first
                qreal angle = (M_PI / 2) - qAtan(qAbs(dx / dy));
                // Calculate position on circle
                xf = 0.5 + ((dx < 0. ? -1. : 1.) * qCos(angle) * 0.5);
                yf = 0.5 + ((dy < 0. ? -1. : 1.) * qSin(angle) * 0.5);
            }
        } else {
            xf = qreal(point.x()) / w;
            yf = 1.0 - qreal(point.y()) / w;
        }
        qreal xp = 2.0 * xf - 1.0;
        qreal yp = 2.0 * yf - 1.0;
        qreal rad = qMin(hypot(xp, yp), 1.0);
        qreal theta = qAtan2(yp, xp);
        theta -= 105.0 / 360.0 * 2.0 * M_PI;
        if (theta < 0.0) {
            theta += 2.0 * M_PI;
        }
        qreal hue = (theta * 180.0 / M_PI) / 360.0;
        qreal value = m_color.valueF();
        if (m_zeroShift != 0. && value == 0 && hue != -1 && rad != 0) {
            // For lift, allow changing color if value slider is set to 0
            value = 0.00214;
        }
        return NegQColor::fromHsvF(hue, rad, value);
    }
    if (m_sliderClick) {
        qreal value = 1.0 - qreal(point.y() - m_margin) / (wheelSize() - m_margin * 2);
        value = qBound(0., value, 1.);
        if (qFuzzyIsNull(m_zeroShift)) {
            // Range is 0 to 1
            if (value < 0.001) {
                value = 0.001;
            }
        } else if (m_color.hueF() != -1 && m_color.saturationF() != 0 && qAbs(value) < 0.001) {
            // Range is -1 to 1
            value = value < 0. ? -0.001 : 0.001;
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
    if (m_sliderRegion.contains(event->position().toPoint())) {
        double y = m_color.valueF();
        if (event->modifiers() & Qt::ShiftModifier) {
            y += event->angleDelta().y() > 0 ? 0.002 : -0.002;
        } else {
            y += event->angleDelta().y() > 0 ? 0.01 : -0.01;
        }
        m_sliderClick = true;
        m_sliderFocus = true;
        qreal value = qBound(-m_zeroShift, y, 1. - m_zeroShift);
        if (qFuzzyIsNull(m_zeroShift)) {
            // Range is 0 to 1
            if (qAbs(value) < 0.001) {
                value = 0.001;
            }
        } else if (qAbs(value) < 0.001) {
            // Range is -1 to 1
            value = value < 0. ? -0.001 : 0.001;
        }
        m_sourceColor = m_color;
        m_color.setValueF(value);
        if (m_sourceColor != m_color) {
            changeColor(m_sourceColor, m_color, true);
        }
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void WheelContainer::mousePressEvent(QMouseEvent *event)
{
    if (event->modifiers() & Qt::ShiftModifier) {
        QPoint clicked = event->pos();
        if (m_wheelRegion.contains(clicked)) {
            m_wheelClick = true;
            m_sliderFocus = false;
            QPointF current = pointForColor();
            QPointF diff = clicked - current;
            double factor = fabs(diff.x()) > fabs(diff.y()) ? fabs(diff.x()) : fabs(diff.y());
            diff /= factor;
            m_lastPoint = current + diff;
        } else if (m_sliderRegion.contains(clicked)) {
            m_sliderClick = true;
            m_sliderFocus = true;
            double y = yForColor();
            int offset = clicked.y() > y ? 1 : -1;
            m_lastPoint = QPointF(clicked.x(), y + offset);
            update();
        } else {
            return;
        }
    } else {
        m_lastPoint = event->pos();
    }
    if (m_wheelRegion.contains(m_lastPoint.toPoint())) {
        m_wheelClick = true;
        m_sliderFocus = false;
        if (event->button() == Qt::LeftButton) {
            m_sourceColor = m_color;
            changeColor(m_sourceColor, colorForPoint(m_lastPoint), false);
        } else {
            // reset to default on middle/right button
            m_sourceColor = m_color;
            qreal r = m_color.redF();
            qreal b = m_color.blueF();
            qreal g = m_color.greenF();
            qreal max = qMax(r, b);
            max = qMax(max, g);
            m_color = NegQColor::fromRgbF(max, max, max);
        }
    } else if (m_sliderRegion.contains(m_lastPoint.toPoint())) {
        m_sliderClick = true;
        m_sliderFocus = true;
        if (event->button() == Qt::LeftButton) {
            m_sourceColor = m_color;
            NegQColor color = colorForPoint(m_lastPoint);
            qreal value = color.valueF() - m_zeroShift;
            if (qFuzzyIsNull(m_zeroShift)) {
                // Range is 0 to 1
                if (qAbs(value) < 0.001) {
                    value = 0.001;
                }
            } else if (qAbs(value) < 0.001) {
                // Range is -1 to 1
                value = value < 0. ? -0.001 : 0.001;
            }
            m_color.setValueF(value);
            changeColor(m_color, m_color, false);
        } else {
            m_sourceColor = m_color;
            m_color = NegQColor::fromHsvF(m_color.hueF(), m_color.saturationF(), m_defaultColor.valueF());
        }
        update();
    } else {
        if (m_sliderFocus) {
            m_sliderFocus = false;
            drawSlider();
            update();
        }
        clearFocus();
    }
    m_isMouseDown = true;
}

void WheelContainer::focusOutEvent(QFocusEvent *event)
{
    if (m_sliderFocus) {
        m_sliderFocus = false;
        drawSlider();
        update();
    }
    QWidget::focusOutEvent(event);
}

void WheelContainer::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_isMouseDown) {
        if (m_wheelRegion.contains(event->pos()) || m_sliderRegion.contains(event->pos())) {
            setCursor(Qt::CrossCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
        return;
    }
    setCursor(Qt::CrossCursor);
    if (event->modifiers() & Qt::ShiftModifier) {
        if (m_wheelClick) {
            QPointF diff = event->pos() - m_lastPoint;
            double factor = fabs(diff.x()) > fabs(diff.y()) ? fabs(diff.x()) : fabs(diff.y());
            diff /= factor;
            m_lastPoint += diff;
        } else if (m_sliderClick) {
            double y = yForColor();
            int offset = event->pos().y() > y ? 1 : -1;
            m_lastPoint = QPointF(event->pos().x(), y + offset);
        } else {
            return;
        }
    } else {
        m_lastPoint = event->pos();
    }
    if (m_wheelClick) {
        const NegQColor color = colorForPoint(m_lastPoint);
        changeColor(color, color, false);
    } else if (m_sliderClick) {
        const NegQColor color = colorForPoint(m_lastPoint);
        qreal value = color.valueF() - m_zeroShift;
        if (qFuzzyIsNull(m_zeroShift)) {
            // Range is 0 to 1
            if (qAbs(value) < 0.001) {
                value = 0.001;
            }
        } else if (qAbs(value) < 0.001) {
            // Range is -1 to 1
            value = value < 0. ? -0.001 : 0.001;
        }
        m_color.setValueF(value);
        changeColor(m_color, m_color, false);
    }
}

void WheelContainer::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    if ((m_sliderClick || m_wheelClick) && !(event->modifiers() & Qt::ShiftModifier)) {
        // Create an undo entry
        changeColor(m_sourceColor, m_color, true);
    }
    m_wheelClick = false;
    m_sliderClick = false;
    m_isMouseDown = false;
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
    return QString::number(m_color.redF() * m_sizeFactor, 'f', 3) + QLatin1Char(',') + QString::number(m_color.greenF() * m_sizeFactor, 'f', 3) +
           QLatin1Char(',') + QString::number(m_color.blueF() * m_sizeFactor, 'f', 3);
}

const QList<double> WheelContainer::getNiceParamValues() const
{
    return {m_color.redF() * m_sizeFactor, m_color.greenF() * m_sizeFactor, m_color.blueF() * m_sizeFactor};
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
    // painter.drawText(m_margin, wheelSize() + m_unitSize - m_margin, m_name + QLatin1Char(' ') + getParamValues());
    drawWheelDot(painter);
    drawSliderBar(painter);
    //    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
}

void WheelContainer::drawWheel()
{
    int r = wheelSize();
    QImage buffer = m_image;
    buffer.fill(0); // transparent
    QPainter painter(&buffer);
    painter.setRenderHint(QPainter::Antialiasing);
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
    painter.drawEllipse(QPointF(0, 0), r / 2 - m_unitSize * .6 - m_margin, r / 2 - m_unitSize * .6 - m_margin);

    m_wheelRegion = QRegion(r / 2, r / 2, r - 2 * m_margin, r - 2 * m_margin, QRegion::Ellipse);
    m_wheelRegion.translate(-(r - 2 * m_margin) / 2, -(r - 2 * m_margin) / 2);
    m_image = buffer;
}

void WheelContainer::drawSlider()
{
    QImage buffer = m_image;
    QPainter painter(&buffer);
    painter.setRenderHint(QPainter::Antialiasing);
    int pos = int(wheelSize() + m_unitSize * .2 + m_sliderBorder);
    qreal scale = qreal(pos + m_sliderWidth) / maximumWidth();
    int w = int(m_sliderWidth * scale - m_unitSize * .2);
    QLinearGradient gradient(0, 0, w, sliderHeight());
    NegQColor c = m_color;
    c.setValueF(1);
    gradient.setColorAt(0.0, c.qcolor);
    c.setValueF(-m_zeroShift);
    gradient.setColorAt(1.0, QColorUtils::complementary(c.qcolor));
    QBrush brush(gradient);
    QPen pen(m_sliderFocus ? QPalette().highlight().color() : Qt::NoPen);
    pen.setWidth(m_sliderBorder);
    painter.setPen(pen);
    painter.setBrush(brush);
    painter.translate(pos, m_margin + m_sliderBorder);
    painter.drawRoundedRect(QRect(0, 0, w, sliderHeight()), w / 3, w / 3);
    m_sliderRegion = QRegion(pos, m_margin + m_sliderBorder, w, sliderHeight() - m_margin);
    m_image = buffer;
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
    painter.drawEllipse(QPointF(m_color.saturationF() * (r - m_margin * 2), 0.0), 4, 4);
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
    int pos = wheelSize();
    qreal scale = qreal(pos + m_sliderWidth) / maximumWidth();
    int w = int(m_sliderWidth * scale + m_sliderBorder * 2);
    int h = m_sliderBorder * 2;
    QPen pen(Qt::white);
    pen.setWidth(m_sliderBorder);
    painter.setPen(pen);
    painter.setBrush(Qt::black);
    painter.translate(pos + m_sliderBorder, m_margin + value * sliderHeight() - (h / 2) + m_sliderBorder);
    painter.drawRect(0, 0, w, h);
    painter.resetTransform();
}

void WheelContainer::changeColor(const NegQColor &sourceColor, const NegQColor &color, bool createUndo)
{
    m_color = color;
    drawWheel();
    drawSlider();
    update();
    Q_EMIT colorChange(sourceColor, m_color, createUndo);
}

ColorWheel::ColorWheel(const QString &id, const QString &name, const NegQColor &color, QWidget *parent)
    : QWidget(parent)
{
    QFontInfo info(font());
    int unitSize = info.pixelSize();
    auto *lay = new QVBoxLayout(this);
    m_wheelName = new QLabel(name, this);
    m_wheelName->setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    lay->addWidget(m_wheelName);
    m_container = new WheelContainer(id, name, color, unitSize, this);
    auto *hb = new QHBoxLayout;
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
    connect(m_container, &WheelContainer::colorChange, this, [&](const NegQColor &sourceCol, const NegQColor &col, bool createUndo) {
        QList<double> vals = m_container->getNiceParamValues();
        m_redEdit->blockSignals(true);
        m_greenEdit->blockSignals(true);
        m_blueEdit->blockSignals(true);
        m_redEdit->setValue(vals.at(0));
        m_greenEdit->setValue(vals.at(1));
        m_blueEdit->setValue(vals.at(2));
        m_redEdit->blockSignals(false);
        m_greenEdit->blockSignals(false);
        m_blueEdit->blockSignals(false);
        Q_EMIT colorChange(sourceCol, col, createUndo);
    });
    connect(m_redEdit, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
            [&]() { m_container->setRedColor(m_redEdit->value()); });
    connect(m_greenEdit, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
            [&]() { m_container->setGreenColor(m_greenEdit->value()); });
    connect(m_blueEdit, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this,
            [&]() { m_container->setBlueColor(m_blueEdit->value()); });
    setMinimumHeight(m_wheelName->height() + m_container->minimumHeight() + m_redEdit->height());
    setMaximumWidth(m_container->maximumWidth());
    setMinimumWidth(3 * m_redEdit->sizeHint().width());
}

NegQColor ColorWheel::color() const
{
    return m_container->color();
}

void ColorWheel::setColor(const QList<double> &values)
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
