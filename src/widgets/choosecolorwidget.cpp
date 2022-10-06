/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "choosecolorwidget.h"
#include "colorpickerwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QTextStream>

#include <KColorButton>

ChooseColorWidget::ChooseColorWidget(QWidget *parent, const QColor &color, bool alphaEnabled)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_button = new KColorButton(color, this);
    if (alphaEnabled) {
        m_button->setAlphaChannelEnabled(alphaEnabled);
    }
    auto *picker = new ColorPickerWidget(this);

    layout->addWidget(m_button, 2);
    layout->addWidget(picker);

    connect(picker, &ColorPickerWidget::colorPicked, this, &ChooseColorWidget::setColor);
    connect(picker, &ColorPickerWidget::disableCurrentFilter, this, &ChooseColorWidget::disableCurrentFilter);
    connect(m_button, &KColorButton::changed, this, &ChooseColorWidget::modified);
}

QColor ChooseColorWidget::color() const
{
    return m_button->color();
}

void ChooseColorWidget::setColor(const QColor &color)
{
    m_button->setColor(color);
}

bool ChooseColorWidget::isAlphaChannelEnabled() const
{
    return m_button->isAlphaChannelEnabled();
}

void ChooseColorWidget::setAlphaChannelEnabled(bool alpha)
{
    m_button->setAlphaChannelEnabled(alpha);
}

void ChooseColorWidget::slotColorModified(const QColor &color)
{
    blockSignals(true);
    m_button->setColor(color);
    blockSignals(false);
}
