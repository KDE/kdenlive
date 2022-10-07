/*
    SPDX-FileCopyrightText: 2008 Marco Gittler <g.marco@freenet.de>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "positionwidget.h"

#include "kdenlivesettings.h"
#include "timecodedisplay.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

PositionWidget::PositionWidget(const QString &name, int pos, int min, int max, const QString &comment, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    QLabel *label = new QLabel(name, this);
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred));
    m_slider->setRange(min, max);

    m_display = new TimecodeDisplay(this);
    m_display->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred));
    m_display->setRange(min, max);

    layout->addWidget(label);
    layout->addWidget(m_slider);
    layout->addWidget(m_display);

    m_slider->setValue(pos);
    m_display->setValue(pos);
    connect(m_slider, SIGNAL(valueChanged(int)), m_display, SLOT(setValue(int)));
    connect(m_slider, &QAbstractSlider::valueChanged, this, &PositionWidget::valueChanged);
    connect(m_display, &TimecodeDisplay::timeCodeEditingFinished, this, &PositionWidget::slotUpdatePosition);

    setToolTip(comment);
}

PositionWidget::~PositionWidget() = default;

int PositionWidget::getPosition() const
{
    return m_slider->value();
}

void PositionWidget::setPosition(int pos)
{
    m_slider->setValue(pos);
}

void PositionWidget::slotUpdatePosition()
{
    m_slider->blockSignals(true);
    m_slider->setValue(m_display->getValue());
    m_slider->blockSignals(false);
    emit valueChanged();
}

void PositionWidget::setRange(int min, int max, bool absolute)
{
    if (absolute) {
        m_slider->setRange(min, max);
        m_display->setRange(min, max);
    } else {
        m_slider->setRange(0, max - min);
        m_display->setRange(0, max - min);
    }
    m_slider->setSingleStep(std::ceil((max - min) / 10.));
}

bool PositionWidget::isValid() const
{
    return m_slider->minimum() != m_slider->maximum();
}

void PositionWidget::slotShowComment(bool show)
{
    Q_UNUSED(show);
}
