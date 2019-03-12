/***************************************************************************
                          positionedit.cpp  -  description
                             -------------------
    begin                : 03 Aug 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "positionwidget.h"

#include "kdenlivesettings.h"
#include "timecodedisplay.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>

PositionWidget::PositionWidget(const QString &name, int pos, int min, int max, const Timecode &tc, const QString &comment, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    QLabel *label = new QLabel(name, this);
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred));
    m_slider->setRange(min, max);

    m_display = new TimecodeDisplay(tc, this);
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

void PositionWidget::updateTimecodeFormat()
{
    m_display->slotUpdateTimeCodeFormat();
}

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
}

bool PositionWidget::isValid() const
{
    return m_slider->minimum() != m_slider->maximum();
}

void PositionWidget::slotShowComment(bool show)
{
    Q_UNUSED(show);
}
