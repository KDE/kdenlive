/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "audioslider.hpp"
#include "iecscale.h"
#include "utils/painterutils.h"

#include <QDebug>
#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>
#include <cmath>

constexpr int kNarrowKnobHeight = 7;
constexpr int kTickLabelsGrooveMargin = 2;
constexpr int kTickWidth = 8;
constexpr int kTickHeight = 3;
constexpr int kKnobWidth = 14;
constexpr int kWideKnobHeight = 31;
constexpr int kDividerLineGap = 2;

AudioSlider::AudioSlider(Qt::Orientation orientation, QWidget *parent, bool narrowKnob, int neutralPosition)
    : QAbstractSlider(parent)
    , m_narrowKnob(narrowKnob)
    , m_neutralPosition(neutralPosition)
    , m_ticksVisible(true)
    , m_tickLabelsVisible(true)
    , m_maxLabelWidth(0)
    , m_knobHovered(false)
    , m_knobFocused(false)
    , m_labelFormatter(nullptr)
    , m_valueToSlider(nullptr)
{
    setOrientation(orientation);
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);

    // Enable hover tracking so we can highlight the knob when hovered
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);

    /*QFont ft = QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont);
    ft.setPointSizeF(ft.pointSize() * 0.6);

    setFont(ft);*/

    // Initialize label metrics
    m_labelHeight = fontMetrics().height();

    // Set minimum size based on orientation and knob type
    QSizeF knobSize = calculateKnobSize();
    setMinimumWidth(qRound(knobSize.width()));
    setMinimumHeight(qRound(knobSize.height()));
    if (orientation == Qt::Vertical) {
        setFixedWidth(qRound(knobSize.width()));
    } else {
        setFixedHeight(qRound(knobSize.height()));
    }

    // Set sensible defaults
    setRange(0, 10000);
    setValue(6000); // Default to 0 dB (in IEC scale)
    setPageStep(1000);
}

AudioSlider::~AudioSlider() = default;

void AudioSlider::setTickPositions(const QVector<double> &tickPositions)
{
    m_tickPositions = tickPositions;
    // Calculate the maximum label width based on actual tick values
    if (!tickPositions.isEmpty()) {
        m_maxLabelWidth = calculateMaxLabelWidth(tickPositions);
        // If labels are shown, update the widget size
        if (m_tickLabelsVisible && orientation() == Qt::Vertical) {
            int width = kKnobWidth;
            width += m_maxLabelWidth + kTickLabelsGrooveMargin;
            setMinimumWidth(width);
            setFixedWidth(width);
        }
    }
    updateBackgroundCache();
    update();
}

void AudioSlider::setLabelFormatter(std::function<QString(double)> formatter)
{
    m_labelFormatter = std::move(formatter);
    updateBackgroundCache();
    update();
}

void AudioSlider::setValueToSliderFunction(std::function<int(double)> func)
{
    m_valueToSlider = std::move(func);
    updateBackgroundCache();
    update();
}

int AudioSlider::calculateMaxLabelWidth(const QVector<double> &tickPositions) const
{
    if (tickPositions.isEmpty()) {
        return 0;
    }
    int maxWidth = 0;
    for (double val : tickPositions) {
        QString label = m_labelFormatter ? m_labelFormatter(val) : QString::number(val);
        maxWidth = qMax(maxWidth, fontMetrics().boundingRect(label).width());
    }
    return maxWidth;
}

void AudioSlider::setTicksVisible(bool visible)
{
    if (m_ticksVisible != visible) {
        m_ticksVisible = visible;
        updateBackgroundCache();
        update();
    }
}

bool AudioSlider::ticksVisible() const
{
    return m_ticksVisible;
}

void AudioSlider::setTickLabelsVisible(bool visible)
{
    if (m_tickLabelsVisible != visible) {
        m_tickLabelsVisible = visible;
        // Adjust size based on whether labels are shown
        if (orientation() == Qt::Vertical) {
            int width = kKnobWidth;
            if (visible) {
                width += m_maxLabelWidth + kTickLabelsGrooveMargin;
            }
            setMinimumWidth(width);
            setFixedWidth(width);
        }
        updateBackgroundCache();
        update();
    }
}

bool AudioSlider::tickLabelsVisible() const
{
    return m_tickLabelsVisible;
}

void AudioSlider::setNeutralPosition(int value)
{
    m_neutralPosition = value;
    update();
}

int AudioSlider::neutralPosition() const
{
    return m_neutralPosition;
}

bool AudioSlider::isNarrowKnob() const
{
    return m_narrowKnob;
}

void AudioSlider::setNarrowKnob(bool narrow)
{
    if (m_narrowKnob != narrow) {
        m_narrowKnob = narrow;
        updateBackgroundCache();
        update();
    }
}

void AudioSlider::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    // Draw the cached background (groove and labels)
    if (m_backgroundCache.isNull() || m_backgroundCache.size() != size()) {
        updateBackgroundCache();
    }
    painter.drawPixmap(0, 0, m_backgroundCache);

    // Calculate knob position based on current value
    qreal knobPos = positionFromValue(value());
    QRectF knob = getKnobRect(knobPos);

    // Draw the knob
    drawKnob(painter, knob);
}

void AudioSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        event->accept();

        if (!isEnabled()) {
            return;
        }

        m_dragStartPosition = event->pos();
        qreal pos = (orientation() == Qt::Vertical) ? event->pos().y() : event->pos().x();
        setValue(valueFromPosition(pos));
        Q_EMIT sliderPressed();
    } else if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        event->accept();
        if (!isEnabled()) {
            return;
        }
        setValue(m_neutralPosition);
    } else {
        QAbstractSlider::mousePressEvent(event);
    }
}

void AudioSlider::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        event->accept();

        if (!isEnabled()) {
            return;
        }

        qreal pos = (orientation() == Qt::Vertical) ? event->pos().y() : event->pos().x();
        setValue(valueFromPosition(pos));
        Q_EMIT sliderMoved(value());
    } else {
        QAbstractSlider::mouseMoveEvent(event);
    }
}

void AudioSlider::focusInEvent(QFocusEvent *event)
{
    QAbstractSlider::focusInEvent(event);
    if (!m_knobFocused) {
        m_knobFocused = true;
        update();
    }
}

void AudioSlider::focusOutEvent(QFocusEvent *event)
{
    QAbstractSlider::focusOutEvent(event);
    if (m_knobFocused) {
        m_knobFocused = false;
        update();
    }
}

void AudioSlider::resizeEvent(QResizeEvent *event)
{
    QAbstractSlider::resizeEvent(event);
    updateBackgroundCache();
}

void AudioSlider::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange || event->type() == QEvent::PaletteChange) {
        updateBackgroundCache();
    }
    QAbstractSlider::changeEvent(event);
}

void AudioSlider::sliderChange(SliderChange change)
{
    if (change == QAbstractSlider::SliderValueChange) {
        // Get slider properties
        const int min = minimum();
        const int max = maximum();

        // Skip constraint calculation during initialization or when widget size is not valid
        if (!isVisible() || width() <= 0 || height() <= 0) {
            QAbstractSlider::sliderChange(change);
            return;
        }

        int newValue = qBound(min, value(), max);
        if (newValue != value()) {
            blockSignals(true);
            setValue(newValue);
            blockSignals(false);
        }
    }

    QAbstractSlider::sliderChange(change);
}

bool AudioSlider::event(QEvent *event)
{
    if (isEnabled() && (event->type() == QEvent::HoverMove || event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave)) {
        // Check if mouse is over the knob
        QPointF mousePos = mapFromGlobal(QCursor::pos());
        QRectF knobPos = getKnobRect(positionFromValue(value()));
        bool knobHovered = knobPos.contains(mousePos);
        if (knobHovered != m_knobHovered) {
            m_knobHovered = knobHovered;
            update();
        }
    }
    return QAbstractSlider::event(event);
}

void AudioSlider::drawKnob(QPainter &painter, const QRectF &rect)
{
    painter.save();
    bool highlight = (value() != m_neutralPosition) || m_knobHovered || m_knobFocused;
    if (orientation() == Qt::Horizontal) {
        // drawNarrowKnob and drawWideKnob expect vertical orientation, so in order to use them we need to rotate the painter and the rect
        QPointF center = rect.center();
        painter.translate(center);
        painter.rotate(90);
        painter.translate(-center);

        QSizeF rotatedSize(rect.height(), rect.width());
        QPointF rotatedTopLeft(center.x() - rotatedSize.width() / 2.0, center.y() - rotatedSize.height() / 2.0);
        QRectF rotatedRect(rotatedTopLeft, rotatedSize);

        if (m_narrowKnob) {
            drawNarrowKnob(painter, rotatedRect, highlight);
        } else {
            drawWideKnob(painter, rotatedRect, highlight);
        }
    } else {
        if (m_narrowKnob) {
            drawNarrowKnob(painter, rect, highlight);
        } else {
            drawWideKnob(painter, rect, highlight);
        }
    }
    painter.restore();
}

void AudioSlider::drawNarrowKnob(QPainter &painter, const QRectF &knobRect, bool highlight)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    bool isDarkTheme = palette().color(QPalette::Window).lightness() < palette().color(QPalette::WindowText).lightness();
    QColor mainColor = isEnabled() ? QColor(220, 220, 220) : (isDarkTheme ? QColor(130, 130, 130) : QColor(210, 210, 210));
    QColor outlineColor = isEnabled() ? QColor(120, 120, 120) : (isDarkTheme ? QColor(100, 100, 100) : QColor(180, 180, 180));
    QColor dividerColor = isEnabled() ? QColor(80, 80, 80) : QColor(80, 80, 80, 128);
    QColor highlightColor = palette().highlight().color();
    qreal penWidth = 1.0;
    QRectF drawingRect = PainterUtils::adjustedForPen(knobRect, penWidth);
    QPen borderPen(highlight ? highlightColor : outlineColor);
    borderPen.setWidthF(penWidth);
    painter.setPen(borderPen);
    painter.setBrush(mainColor);
    painter.drawRoundedRect(drawingRect, 1.0, 1.0);
    // Draw divider line centered
    qreal dividerY = knobRect.top() + kNarrowKnobHeight / 2.0;
    penWidth = 1.0;
    QPen dividerPen(highlight ? highlightColor : dividerColor);
    dividerPen.setWidthF(penWidth);
    painter.setPen(dividerPen);
    QLineF drawingLine = PainterUtils::adjustedHorizontalLine(dividerY, knobRect.left() + kDividerLineGap, knobRect.right() - kDividerLineGap, penWidth);
    painter.drawLine(drawingLine);
    painter.restore();
}

void AudioSlider::drawWideKnob(QPainter &painter, const QRectF &knobRect, bool highlight)
{
    constexpr qreal kTopPartHeight = 4.0;
    constexpr qreal kUpperPartHeight = 11.0;
    constexpr qreal kDividerHeight = 1.0;
    constexpr qreal kLowerPartHeight = 11.0;
    constexpr qreal kBottomPartHeight = 4.0;
    bool isDarkTheme = palette().color(QPalette::Window).lightness() < palette().color(QPalette::WindowText).lightness();
    const QColor topColor = isEnabled() ? QColor(230, 230, 230) : (isDarkTheme ? QColor(140, 140, 140) : QColor(220, 220, 220));
    const QColor upperColor = isEnabled() ? QColor(200, 200, 200) : (isDarkTheme ? QColor(120, 120, 120) : QColor(200, 200, 200));
    const QColor dividerColor = isEnabled() ? QColor(90, 90, 90) : QColor(90, 90, 90, 128);
    const QColor lowerColor = isEnabled() ? QColor(220, 220, 220) : (isDarkTheme ? QColor(130, 130, 130) : QColor(210, 210, 210));
    const QColor bottomColor = isEnabled() ? QColor(190, 190, 190) : (isDarkTheme ? QColor(110, 110, 110) : QColor(200, 200, 200));
    const QColor borderColor = isEnabled() ? QColor(120, 120, 120) : (isDarkTheme ? QColor(100, 100, 100) : QColor(180, 180, 180));
    const QColor highlightColor = palette().highlight().color();
    const qreal middlePartPos = kTopPartHeight;
    const qreal dividerPos = middlePartPos + kUpperPartHeight;
    const qreal lowerPartPos = dividerPos + kDividerHeight;
    const qreal bottomPartPos = lowerPartPos + kLowerPartHeight;
    qreal cornerRadius = 2.0;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(knobRect.topLeft());
    QPen borderPen(borderColor);
    if (highlight) {
        borderPen.setColor(highlightColor);
        borderPen.setWidthF(1.0);
    } else {
        borderPen.setColor(borderColor);
        borderPen.setWidthF(1.0);
    }
    QRectF borderRect(0, 0, kKnobWidth, kWideKnobHeight);
    // Be sure not to draw outside the border
    QPainterPath borderPath;
    borderPath.addRoundedRect(borderRect, cornerRadius, cornerRadius);
    painter.setClipPath(borderPath);
    painter.setPen(Qt::NoPen);

    // Top part (split into two rectangles, one rounded, one not)
    painter.setBrush(topColor);
    painter.setPen(borderPen);
    painter.drawRoundedRect(PainterUtils::adjustedForPen(QRectF(0, 0, kKnobWidth, kTopPartHeight), borderPen.widthF()), cornerRadius, cornerRadius);
    painter.setPen(Qt::NoPen);
    painter.drawRect(QRectF(0, kTopPartHeight - cornerRadius, kKnobWidth, cornerRadius));

    // Upper part
    painter.setBrush(upperColor);
    painter.drawRect(QRectF(0, middlePartPos, kKnobWidth, kUpperPartHeight));

    // Divider
    if (highlight) {
        painter.setBrush(highlightColor);
    } else {
        painter.setBrush(dividerColor);
    }
    painter.drawRect(QRectF(0 + kDividerLineGap, dividerPos, kKnobWidth - 2 * kDividerLineGap, kDividerHeight));

    // Lower Part
    painter.setBrush(lowerColor);
    painter.drawRect(QRectF(0, lowerPartPos, kKnobWidth, kLowerPartHeight));

    // Bottom part (split into two rectangles, one rounded, one not)
    painter.setBrush(bottomColor);
    painter.setPen(borderPen);
    painter.drawRoundedRect(PainterUtils::adjustedForPen(QRectF(0, bottomPartPos, kKnobWidth, kBottomPartHeight), borderPen.widthF()), cornerRadius,
                            cornerRadius);
    painter.setPen(Qt::NoPen);
    painter.drawRect(QRectF(0, bottomPartPos, kKnobWidth, cornerRadius));

    // Outer border
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(PainterUtils::adjustedForPen(borderRect, borderPen.widthF()), cornerRadius, cornerRadius);
    painter.restore();
}

void AudioSlider::drawGroove(QPainter &painter, const QRectF &grooveRect)
{
    painter.setRenderHint(QPainter::Antialiasing, true);
    QColor baseColor = palette().color(QPalette::WindowText);
    bool isDarkTheme = palette().color(QPalette::Window).lightness() < palette().color(QPalette::WindowText).lightness();
    baseColor.setAlphaF(isDarkTheme ? 0.2 : 0.1);
    QColor brushColor;
    if (isEnabled()) {
        brushColor = baseColor.darker(140);
    } else {
        brushColor = palette().color(QPalette::Disabled, QPalette::WindowText);
        brushColor.setAlphaF(0.2);
    }
    painter.setBrush(brushColor);
    QPen borderPen(baseColor);
    qreal penWidth = 1.0;
    borderPen.setWidthF(penWidth);
    painter.setPen(borderPen);
    painter.setOpacity(isEnabled() ? 1.0 : 0.5);
    painter.drawRoundedRect(PainterUtils::adjustedForPen(grooveRect, penWidth), 3.0, 3.0);
    painter.setOpacity(1.0);
}

QSizeF AudioSlider::calculateTickSize() const
{
    if (orientation() == Qt::Vertical) {
        return QSizeF(kTickWidth, kTickHeight);
    } else {
        return QSizeF(kTickHeight, kTickWidth);
    }
}

void AudioSlider::drawTicks(QPainter &painter, const QRectF &tickRect)
{
    if (!m_ticksVisible || m_tickPositions.isEmpty()) {
        return;
    }
    QColor textColor = palette().color(QPalette::Text);
    const int min = minimum();
    const int max = maximum();
    const int range = max - min;
    auto valueToSlider = m_valueToSlider ? m_valueToSlider : [](double v) { return static_cast<int>(v); };
    QSizeF tickSize = calculateTickSize();
    if (orientation() == Qt::Vertical) {
        const qreal tickStartX = tickRect.left() + (tickRect.width() - tickSize.width()) / 2.0;
        for (double tickValue : m_tickPositions) {
            int sliderValue = valueToSlider(tickValue);
            qreal ratio = static_cast<qreal>(sliderValue - min) / range;
            qreal y = tickRect.bottom() - (tickRect.height() - 1.0) * ratio;
            QLinearGradient tickGradient(tickStartX, y, tickStartX + tickSize.width(), y);
            tickGradient.setColorAt(0.0, QColor(textColor.red(), textColor.green(), textColor.blue(), 160));
            tickGradient.setColorAt(1.0, QColor(textColor.red(), textColor.green(), textColor.blue(), 100));
            painter.save();
            painter.setPen(Qt::NoPen);
            painter.setBrush(tickGradient);
            painter.drawRect(QRectF(tickStartX, y - 1.0, tickSize.width(), tickSize.height()));
            painter.restore();
        }
    } else {
        const qreal tickStartY = tickRect.top() + (tickRect.height() - tickSize.height()) / 2.0;
        for (double tickValue : m_tickPositions) {
            int sliderValue = valueToSlider(tickValue);
            qreal ratio = static_cast<qreal>(sliderValue - min) / range;
            qreal x = tickRect.left() + (tickRect.width() - 1.0) * ratio;
            QLinearGradient tickGradient(x, tickStartY, x, tickStartY + tickSize.height());
            tickGradient.setColorAt(0.0, QColor(textColor.red(), textColor.green(), textColor.blue(), 160));
            tickGradient.setColorAt(1.0, QColor(textColor.red(), textColor.green(), textColor.blue(), 100));
            painter.save();
            painter.setPen(Qt::NoPen);
            painter.setBrush(tickGradient);
            painter.drawRect(QRectF(x - 1.0, tickStartY, tickSize.width(), tickSize.height()));
            painter.restore();
        }
    }
}

void AudioSlider::drawLabels(QPainter &painter, const QRectF &labelRect, const QRectF &tickRect)
{
    if (!m_tickLabelsVisible || m_tickPositions.isEmpty()) {
        return;
    }
    painter.setFont(font());
    QColor textColor = palette().color(QPalette::Text);
    painter.setOpacity(0.8);
    painter.setPen(isEnabled() ? textColor : palette().color(QPalette::Disabled, QPalette::Text));
    const int min = minimum();
    const int max = maximum();
    const int range = max - min;
    auto valueToSlider = m_valueToSlider ? m_valueToSlider : [](double v) { return static_cast<int>(v); };
    auto labelFormatter = m_labelFormatter ? m_labelFormatter : [](double v) { return QString::number(v); };
    qreal labelMargin = 2.0;
    if (orientation() == Qt::Vertical) {
        qreal prevY = -1.0;
        for (double tickValue : m_tickPositions) {
            int sliderValue = valueToSlider(tickValue);
            qreal ratio = static_cast<qreal>(sliderValue - min) / range;
            qreal y = tickRect.bottom() - (tickRect.height() - 1.0) * ratio;
            y -= m_labelHeight / 2.0; // Center label on tick
            // Clamp to widget bounds
            if (y + m_labelHeight > height()) {
                y = height() - m_labelHeight;
            } else if (y < 0) {
                y = 0;
            }
            // Prevent overlap with previous label
            if (prevY < 0 || (prevY - m_labelHeight) - y > labelMargin) {
                const QString label = labelFormatter(tickValue);
                QRectF thisLabelRect(labelRect.left(), y, m_maxLabelWidth, m_labelHeight);
                painter.drawText(thisLabelRect, Qt::AlignRight | Qt::AlignVCenter, label);
                prevY = y;
            }
        }
    } else {
        qreal prevX = -1.0;
        for (double tickValue : m_tickPositions) {
            int sliderValue = valueToSlider(tickValue);
            qreal ratio = static_cast<qreal>(sliderValue - min) / range;
            qreal x = tickRect.left() + (tickRect.width() - 1.0) * ratio;
            x -= m_maxLabelWidth / 2.0; // Center label on tick
            // Clamp to widget bounds
            if (x + m_maxLabelWidth > width()) {
                x = width() - m_maxLabelWidth;
            } else if (x < 0) {
                x = 0;
            }
            // Prevent overlap with previous label
            if (prevX < 0 || prevX - (x + m_maxLabelWidth) > labelMargin) {
                const QString label = labelFormatter(tickValue);
                QRectF thisLabelRect(x, labelRect.top(), m_maxLabelWidth, m_labelHeight);
                painter.drawText(thisLabelRect, Qt::AlignHCenter | Qt::AlignTop, label);
                prevX = x;
            }
        }
    }
}

QSizeF AudioSlider::calculateKnobSize() const
{
    if (orientation() == Qt::Vertical) {
        return QSizeF(kKnobWidth, m_narrowKnob ? kNarrowKnobHeight : kWideKnobHeight);
    } else {
        return QSizeF(m_narrowKnob ? kNarrowKnobHeight : kWideKnobHeight, kKnobWidth);
    }
}

QRectF AudioSlider::getKnobRect(qreal pos) const
{
    QSizeF knobSize = calculateKnobSize();
    QRectF groove = getGrooveRect(QRectF(rect()));
    if (orientation() == Qt::Vertical) {
        qreal x = groove.left() + (groove.width() - knobSize.width()) / 2.0;
        qreal y = pos - knobSize.height() / 2.0;
        return QRectF(x, y, knobSize.width(), knobSize.height());
    } else {
        qreal y = groove.top() + (groove.height() - knobSize.height()) / 2.0;
        qreal x = pos - knobSize.width() / 2.0;
        return QRectF(x, y, knobSize.width(), knobSize.height());
    }
}

QRectF AudioSlider::getGrooveRect(const QRectF &widgetRect) const
{
    qreal grooveWidth = 6.0;
    if (orientation() == Qt::Vertical) {
        qreal rightMargin = (qMax(calculateKnobSize().width(), m_ticksVisible ? kTickWidth : 0.0) - grooveWidth) / 2.0;
        qreal leftMargin = widgetRect.right() - rightMargin - grooveWidth;
        return QRectF(leftMargin, 0, grooveWidth, widgetRect.height());
    } else {
        qreal topMargin = (qMax(calculateKnobSize().height(), m_ticksVisible ? kTickWidth : 0.0) - grooveWidth) / 2.0;
        return QRectF(0, topMargin, widgetRect.width(), grooveWidth);
    }
}

QRectF AudioSlider::getTickRect(const QRectF &grooveRect) const
{
    QSizeF knobSize = calculateKnobSize();
    if (orientation() == Qt::Vertical) {
        qreal tickWidth = kTickWidth;
        qreal knobHeight = knobSize.height();
        qreal x = grooveRect.left() + (grooveRect.width() - tickWidth) / 2.0;
        qreal y = grooveRect.top() + knobHeight / 2.0;
        qreal height = grooveRect.height() - knobHeight;
        return QRectF(x, y, tickWidth, height);
    } else {
        qreal knobWidth = knobSize.width();
        qreal tickHeight = kTickWidth;
        qreal y = grooveRect.top() + (grooveRect.height() - tickHeight) / 2.0;
        qreal x = grooveRect.left() + knobWidth / 2.0;
        qreal width = grooveRect.width() - knobWidth;
        return QRectF(x, y, width, tickHeight);
    }
}

QRectF AudioSlider::getTickLabelRect(const QRectF &tickRect, const QRectF &knobRect) const
{
    if (!m_tickLabelsVisible) return QRectF();
    if (orientation() == Qt::Vertical) {
        qreal leftOfTicks = tickRect.left();
        qreal leftOfKnob = knobRect.left();
        qreal left = std::min(leftOfTicks, leftOfKnob) - kTickLabelsGrooveMargin - m_maxLabelWidth;
        return QRectF(left, tickRect.top(), m_maxLabelWidth, tickRect.height());
    } else {
        qreal belowTicks = tickRect.bottom();
        qreal belowKnob = knobRect.bottom();
        qreal top = std::max(belowTicks, belowKnob) + kTickLabelsGrooveMargin;
        return QRectF(tickRect.left(), top, tickRect.width(), m_labelHeight);
    }
}

void AudioSlider::updateBackgroundCache()
{
    QSize newSize = size();
    if (!newSize.isValid() || newSize.width() <= 0 || newSize.height() <= 0) {
        return;
    }

    qreal scalingFactor = devicePixelRatioF();
    m_backgroundCache = QPixmap(newSize * scalingFactor);
    m_backgroundCache.setDevicePixelRatio(scalingFactor);
    m_backgroundCache.fill(Qt::transparent);

    QPainter painter(&m_backgroundCache);

    QRectF groove = getGrooveRect(QRectF(rect()));
    QRectF ticks = getTickRect(groove);
    qreal knobPos = positionFromValue(value());
    QRectF knob = getKnobRect(knobPos);
    QRectF labels = getTickLabelRect(ticks, knob);

    drawGroove(painter, groove);
    if (m_ticksVisible) drawTicks(painter, ticks);
    if (m_tickLabelsVisible) drawLabels(painter, labels, ticks);
}

int AudioSlider::valueFromPosition(qreal pos) const
{
    const int min = minimum();
    const int max = maximum();
    const int range = max - min;
    if (range <= 0) return min; // Prevent division by zero in case of identical min/max

    QRectF groove = getGrooveRect(QRectF(rect()));
    QRectF ticks = getTickRect(groove);
    if (orientation() == Qt::Vertical) {
        const qreal sliderLength = ticks.height() - 1.0;
        if (sliderLength <= 0) return value();
        const qreal sliderMin = ticks.y();
        const qreal sliderMax = ticks.y() + sliderLength;
        qreal posY = qBound(sliderMin, pos, sliderMax);
        qreal ratio = 1.0 - (posY - sliderMin) / sliderLength;
        int res = min + qRound(ratio * range);
        return res;
    } else {
        const qreal sliderLength = ticks.width() - 1.0;
        if (sliderLength <= 0) return value();
        const qreal sliderMin = ticks.x();
        const qreal sliderMax = ticks.x() + sliderLength;
        qreal posX = qBound(sliderMin, pos, sliderMax);
        qreal ratio = (posX - sliderMin) / sliderLength;
        int res = min + qRound(ratio * range);
        return res;
    }
}

qreal AudioSlider::positionFromValue(int val) const
{
    QRectF groove = getGrooveRect(QRectF(rect()));
    QRectF ticks = getTickRect(groove);
    const int min = minimum();
    const int max = maximum();
    const int range = max - min;
    if (range <= 0) {
        // Prevent division by zero in case of identical min/max
        return orientation() == Qt::Vertical ? ticks.bottom() : ticks.left();
    }

    val = qBound(min, val, max);
    if (orientation() == Qt::Vertical) {
        qreal ratio = static_cast<qreal>(val - min) / range;
        qreal res = ticks.bottom() - ratio * (ticks.height() - 1.0);
        return res;
    } else {
        qreal ratio = static_cast<qreal>(val - min) / range;
        qreal res = ticks.left() + ratio * (ticks.width() - 1.0);
        return res;
    }
}
