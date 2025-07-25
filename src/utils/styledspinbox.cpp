/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "styledspinbox.hpp"
#include <QPalette>

StyledDoubleSpinBox::StyledDoubleSpinBox(double neutral, QWidget *parent)
    : QDoubleSpinBox(parent)
    , m_neutral(neutral)
{
    connect(this, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StyledDoubleSpinBox::updateStyle);
}

void StyledDoubleSpinBox::setNeutralPosition(double neutral)
{
    m_neutral = neutral;
    updateStyle(value());
}

void StyledDoubleSpinBox::updateStyle(double value)
{
    bool isNeutral = qFuzzyCompare(value, m_neutral);
    QColor textColor = isNeutral ? parentWidget()->palette().color(QPalette::Text) : palette().highlight().color();
    QPalette pal = this->palette();
    if (pal.color(QPalette::Text) != textColor) {
        pal.setColor(QPalette::Text, textColor);
        setPalette(pal);
    }
}

bool StyledDoubleSpinBox::event(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange || event->type() == QEvent::PaletteChange) {
        if (!isEnabled()) {
            setPalette(parentWidget()->palette()); // reset to default for disabled
        } else {
            updateStyle(value());
        }
    }
    return QDoubleSpinBox::event(event);
}

StyledSpinBox::StyledSpinBox(int neutral, QWidget *parent)
    : QSpinBox(parent)
    , m_neutral(neutral)
{
    connect(this, QOverload<int>::of(&QSpinBox::valueChanged), this, &StyledSpinBox::updateStyle);
}

void StyledSpinBox::setNeutralPosition(int neutral)
{
    m_neutral = neutral;
    updateStyle(value());
}

void StyledSpinBox::updateStyle(int value)
{
    bool isNeutral = value == m_neutral;
    QColor textColor = isNeutral ? parentWidget()->palette().color(QPalette::Text) : palette().highlight().color();
    QPalette pal = this->palette();
    if (pal.color(QPalette::Text) != textColor) {
        pal.setColor(QPalette::Text, textColor);
        setPalette(pal);
    }
}

bool StyledSpinBox::event(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange || event->type() == QEvent::PaletteChange) {
        if (!isEnabled()) {
            setPalette(parentWidget()->palette()); // reset to default for disabled
        } else {
            updateStyle(value());
        }
    }
    return QSpinBox::event(event);
}