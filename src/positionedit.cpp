/***************************************************************************
                          geomeytrval.cpp  -  description
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

PositionEdit::PositionEdit(const QString name, int pos, int min, int max, const Timecode tc, QWidget* parent) :
        QWidget(parent),
        m_tc(tc),
        m_frameDisplay(KdenliveSettings::frametimecode())
{
    m_ui.setupUi(this);
    m_ui.label->setText(name);
    m_ui.horizontalSlider->setRange(min, max);
    connect(m_ui.horizontalSlider, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateTimecode()));
    connect(m_ui.krestrictedline, SIGNAL(editingFinished()), this, SLOT(slotUpdatePosition()));
    m_ui.horizontalSlider->setValue(pos);
    if (m_frameDisplay) {
        QValidator *valid = new QIntValidator();
        m_ui.krestrictedline->setInputMask("");
        m_ui.krestrictedline->setValidator(valid);
    }
    m_ui.krestrictedline->setText(m_tc.getDisplayTimecodeFromFrames(pos, m_frameDisplay));
}

int PositionEdit::getPosition() const
{
    return m_ui.horizontalSlider->value();
}

void PositionEdit::setPosition(int pos)
{
    m_ui.horizontalSlider->setValue(pos);
    m_ui.krestrictedline->setText(m_tc.getDisplayTimecodeFromFrames(pos, m_frameDisplay));
}

void PositionEdit::slotUpdateTimecode()
{
    m_ui.krestrictedline->setText(m_tc.getDisplayTimecodeFromFrames(m_ui.horizontalSlider->value(), m_frameDisplay));
    emit parameterChanged();
}

void PositionEdit::slotUpdatePosition()
{
    m_ui.horizontalSlider->blockSignals(true);
    int pos = m_tc.getDisplayFrameCount(m_ui.krestrictedline->text(), m_frameDisplay);
    m_ui.horizontalSlider->setValue(pos);
    if (pos != m_ui.horizontalSlider->value()) {
        // Value out of range
        m_ui.krestrictedline->setText(m_tc.getDisplayTimecodeFromFrames(m_ui.horizontalSlider->value(), m_frameDisplay));
    }
    m_ui.horizontalSlider->blockSignals(false);
    emit parameterChanged();
}

