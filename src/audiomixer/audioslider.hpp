/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QAbstractSlider>
#include <QFont>
#include <QPainterPath>
#include <QPixmap>
#include <QVector>
#include <functional>

/**
 * @class AudioSlider
 * @brief A custom audio slider widget with integrated gain labels
 */
class AudioSlider : public QAbstractSlider
{
    Q_OBJECT

public:
    explicit AudioSlider(Qt::Orientation orientation, QWidget *parent = nullptr, bool narrowKnob = false, int neutralPosition = 0);
    ~AudioSlider() override;

    /**
     * @brief Set the tick positions that correspond to the gain labels
     * @param tickPositions The gain values (in dB) for which to draw ticks and labels
     */
    void setTickPositions(const QVector<double> &tickPositions);

    /**
     * @brief Set whether to show tick marks at tick positions
     */
    void setTicksVisible(bool visible);

    /**
     * @brief Get whether tick marks are shown
     */
    bool ticksVisible() const;

    /**
     * @brief Set the neutral position value
     */
    void setNeutralPosition(int value);

    /**
     * @brief Get the neutral position value
     */
    int neutralPosition() const;

    /**
     * @brief Check if narrow knob style is used
     */
    bool isNarrowKnob() const;

    /**
     * @brief Set whether to use narrow or wide knob style
     */
    void setNarrowKnob(bool narrow);

    void setLabelFormatter(std::function<QString(double)> formatter);
    void setValueToSliderFunction(std::function<int(double)> func);

    /**
     * @brief Set whether to show labels at tick positions
     */
    void setTickLabelsVisible(bool visible);

    /**
     * @brief Get whether labels are shown
     */
    bool tickLabelsVisible() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    void sliderChange(SliderChange change) override;
    bool event(QEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    bool m_narrowKnob;
    int m_neutralPosition;
    QPointF m_dragStartPosition;
    QVector<double> m_tickPositions;
    bool m_ticksVisible = true;
    bool m_tickLabelsVisible = true;
    QPixmap m_backgroundCache;
    int m_maxLabelWidth; // Store the maximum width needed for labels
    bool m_knobHovered = false;
    bool m_knobFocused = false;

    int m_labelHeight = 0;
    QFont m_labelFont;

    std::function<QString(double)> m_labelFormatter;
    std::function<int(double)> m_valueToSlider;

    void drawKnob(QPainter &painter, const QRectF &rect);
    void drawWideKnob(QPainter &painter, const QRectF &rect, bool highlight);
    void drawNarrowKnob(QPainter &painter, const QRectF &rect, bool highlight);
    void drawGroove(QPainter &painter, const QRectF &rect);
    void drawTicks(QPainter &painter, const QRectF &tickRect);
    void drawLabels(QPainter &painter, const QRectF &labelRect, const QRectF &tickRect);
    int valueFromPosition(qreal pos) const;
    qreal positionFromValue(int value) const;
    void updateBackgroundCache();
    int calculateMaxLabelWidth(const QVector<double> &tickPositions) const;
    QSizeF calculateKnobSize() const;
    QSizeF calculateTickSize() const;
    QRectF getKnobRect(qreal pos) const;
    QRectF getGrooveRect(const QRectF &widgetRect) const;
    QRectF getTickRect(const QRectF &grooveRect) const;
    QRectF getTickLabelRect(const QRectF &tickRect, const QRectF &knobRect) const;
};