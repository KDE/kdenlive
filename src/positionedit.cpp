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
#include "kdenlivesettings.h"

#include <KDebug>

#include <QLabel>
#include <QSlider>

PositionEdit::PositionEdit(const QString name, int pos, int min, int max, const Timecode tc, QWidget* parent) :
        QWidget(parent)
{
    QVBoxLayout *l = new QVBoxLayout;
    QLabel *lab = new QLabel(name);
    l->addWidget(lab);

    QHBoxLayout *l2 = new QHBoxLayout;
    m_display = new TimecodeDisplay(tc);
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred));
    m_display->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred));
    l2->addWidget(m_display);
    l2->addWidget(m_slider);
    m_display->setRange(min, max);
    m_slider->setRange(min, max);
    connect(m_slider, SIGNAL(valueChanged(int)), m_display, SLOT(setValue(int)));
    connect(m_slider, SIGNAL(valueChanged(int)), this, SIGNAL(parameterChanged()));
    connect(m_display, SIGNAL(editingFinished()), this, SLOT(slotUpdatePosition()));
    m_slider->setValue(pos);
    l->addLayout(l2);
    setLayout(l);
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
    m_display->setValue(pos);
}


void PositionEdit::slotUpdatePosition()
{
    m_slider->blockSignals(true);
    m_slider->setValue(m_display->getValue());
    m_slider->blockSignals(false);
    emit parameterChanged();
}

