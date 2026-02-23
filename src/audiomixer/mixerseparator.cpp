/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "mixerseparator.h"
#include <QLinearGradient>
#include <QPainter>
#include <QPalette>

MixerSeparator::MixerSeparator(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setMinimumWidth(1);
}

QSize MixerSeparator::sizeHint() const
{
    return QSize(1, 10); // fixed width, height will expand
}

void MixerSeparator::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    QColor color = palette().color(QPalette::WindowText);
    color.setAlphaF(0.6);

    // Vertical gradient: visible in the center, fades at top and bottom. This way it doesn't look like a regular window/widget border.
    QLinearGradient grad(0, 0, 0, height());
    grad.setColorAt(0.0, Qt::transparent);
    grad.setColorAt(0.3, color);
    grad.setColorAt(0.5, color);
    grad.setColorAt(0.7, color);
    grad.setColorAt(1.0, Qt::transparent);

    painter.setPen(Qt::NoPen);
    painter.setBrush(grad);
    painter.drawRect(rect());
}