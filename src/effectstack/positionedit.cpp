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

#include "positionedit.h"

#include "timecodedisplay.h"
#include "kdenlivesettings.h"

#include <QDebug>

#include <QLabel>
#include <QSlider>
#include <QHBoxLayout>

PositionEdit::PositionEdit(const QString &name, int pos, int min, int max, const Timecode&tc, QWidget* parent) :
    QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    QLabel *label = new QLabel(name, this);
    m_slider = new QSlider(Qt::Horizontal);
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
    connect(m_slider, &QAbstractSlider::valueChanged, this, &PositionEdit::parameterChanged);
    connect(m_display, &TimecodeDisplay::timeCodeEditingFinished, this, &PositionEdit::slotUpdatePosition);
}

PositionEdit::~PositionEdit()
{
    m_display->blockSignals(true);
    m_slider->blockSignals(true);
    delete m_slider;
    delete m_display;
}

void PositionEdit::updateTimecodeFormat()
{
    m_display->slotUpdateTimeCodeFormat();
}

int PositionEdit::getPosition() const
{
    return m_slider->value();
}

void PositionEdit::setPosition(int pos)
{
    m_slider->setValue(pos);
}

void PositionEdit::slotUpdatePosition()
{
    m_slider->blockSignals(true);
    m_slider->setValue(m_display->getValue());
    m_slider->blockSignals(false);
    emit parameterChanged(m_display->getValue());
}

void PositionEdit::setRange(int min, int max, bool absolute)
{
    if (absolute) {
        m_slider->setRange(min, max);
        m_display->setRange(min, max);
    } else {
        m_slider->setRange(0, max - min);
        m_display->setRange(0, max - min);
    }
}

bool PositionEdit::isValid() const
{
    return m_slider->minimum() != m_slider->maximum();
}


