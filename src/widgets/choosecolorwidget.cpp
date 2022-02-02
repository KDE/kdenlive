/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "choosecolorwidget.h"
#include "colorpickerwidget.h"
#include "utils/qcolorutils.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QTextStream>

#include <KColorButton>

ChooseColorWidget::ChooseColorWidget(const QString &text, const QString &color, const QString &comment, bool alphaEnabled, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel *label = new QLabel(text, this);

    QWidget *rightSide = new QWidget(this);
    auto *rightSideLayout = new QHBoxLayout(rightSide);
    rightSideLayout->setContentsMargins(0, 0, 0, 0);
    rightSideLayout->setSpacing(0);

    m_button = new KColorButton(QColorUtils::stringToColor(color), rightSide);
    if (alphaEnabled) {
        m_button->setAlphaChannelEnabled(alphaEnabled);
    }
    //     m_button->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
    auto *picker = new ColorPickerWidget(rightSide);

    layout->addWidget(label, 1);
    layout->addWidget(rightSide, 1);
    rightSideLayout->addStretch();
    rightSideLayout->addWidget(m_button, 2);
    rightSideLayout->addWidget(picker);

    connect(picker, &ColorPickerWidget::colorPicked, this, &ChooseColorWidget::setColor);
    connect(picker, &ColorPickerWidget::disableCurrentFilter, this, &ChooseColorWidget::disableCurrentFilter);
    connect(m_button, &KColorButton::changed, this, &ChooseColorWidget::modified);

    // connect the signal of the derived class to the signal of the base class
    connect(this, &ChooseColorWidget::modified, [this](const QColor &) { emit valueChanged(); });

    // setup comment
    setToolTip(comment);
}

QString ChooseColorWidget::getColor() const
{
    bool alphaChannel = m_button->isAlphaChannelEnabled();
    return QColorUtils::colorToString(m_button->color(), alphaChannel);
}

void ChooseColorWidget::setColor(const QColor &color)
{
    m_button->setColor(color);
}

void ChooseColorWidget::slotColorModified(const QColor &color)
{
    blockSignals(true);
    m_button->setColor(color);
    blockSignals(false);
}
