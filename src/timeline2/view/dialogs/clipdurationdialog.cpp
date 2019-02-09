/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "clipdurationdialog.h"

#include <QFontDatabase>

#include <QWheelEvent>

ClipDurationDialog::ClipDurationDialog(int clipId, const Timecode &tc, int pos, int minpos, int in, int out, int length, int maxpos, QWidget *parent)
    : QDialog(parent)
    , m_clipId(clipId)
    , m_min(GenTime(minpos, tc.fps()))
    , m_max(GenTime(maxpos, tc.fps()))
    , m_length(GenTime(length, tc.fps()))
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);

    m_pos = new TimecodeDisplay(tc);
    m_cropStart = new TimecodeDisplay(tc);
    m_dur = new TimecodeDisplay(tc);
    m_cropEnd = new TimecodeDisplay(tc);

    clip_position_box->addWidget(m_pos);
    crop_start_box->addWidget(m_cropStart);
    clip_duration_box->addWidget(m_dur);
    crop_end_box->addWidget(m_cropEnd);

    bool allowCrop = length != -1;

    if (!allowCrop) {
        m_cropStart->setHidden(true);
        crop_label->hide();
        m_cropEnd->setHidden(true), end_label->hide();
    }

    m_crop = GenTime(in, tc.fps());

    m_pos->setValue(GenTime(pos, tc.fps()));
    m_dur->setValue(GenTime(out - in, tc.fps()));
    m_cropStart->setValue(GenTime(in, tc.fps()));
    m_cropEnd->setValue(GenTime(length - out, tc.fps()));

    connect(m_pos, &TimecodeDisplay::timeCodeEditingFinished, this, &ClipDurationDialog::slotCheckStart);
    connect(m_dur, &TimecodeDisplay::timeCodeEditingFinished, this, &ClipDurationDialog::slotCheckDuration);
    connect(m_cropStart, &TimecodeDisplay::timeCodeEditingFinished, this, &ClipDurationDialog::slotCheckCrop);
    connect(m_cropEnd, &TimecodeDisplay::timeCodeEditingFinished, this, &ClipDurationDialog::slotCheckEnd);

    adjustSize();
}

ClipDurationDialog::~ClipDurationDialog()
{
    delete m_pos;
    delete m_dur;
    delete m_cropStart;
    delete m_cropEnd;
}

void ClipDurationDialog::slotCheckStart()
{
    GenTime start = m_pos->gentime();
    GenTime duration = m_dur->gentime();
    if (m_min != GenTime() && start < m_min) {
        m_pos->setValue(m_min);
    } else if (m_max != GenTime() && start + duration > m_max) {
        m_pos->setValue(m_max - duration);
    }
}

void ClipDurationDialog::slotCheckDuration()
{
    GenTime start = m_pos->gentime();
    GenTime duration = m_dur->gentime();
    GenTime cropStart = m_cropStart->gentime();
    GenTime maxDuration;

    if (m_length <= GenTime()) {
        maxDuration = m_max;
    } else {
        maxDuration = m_max == GenTime() ? start + m_length - cropStart : qMin(m_max, start + m_length - cropStart);
    }

    if (maxDuration != GenTime() && start + duration > maxDuration) {
        m_dur->blockSignals(true);
        m_dur->setValue(maxDuration - start);
        m_dur->blockSignals(false);
    }

    m_cropEnd->blockSignals(true);
    m_cropEnd->setValue(m_length - m_dur->gentime() - cropStart);
    m_cropEnd->blockSignals(false);
}

void ClipDurationDialog::slotCheckCrop()
{
    GenTime duration = m_dur->gentime();
    GenTime cropStart = m_cropStart->gentime();

    GenTime diff = cropStart - m_crop;
    if ((diff > GenTime() && diff < duration) || diff < GenTime()) {
        duration -= diff;
    } else {
        m_cropStart->setValue(m_crop);
        return;
    }

    if (m_length > GenTime() && cropStart + duration > m_length) {
        m_cropStart->setValue(m_crop);
    } else {
        m_crop = cropStart;
        m_dur->blockSignals(true);
        m_dur->setValue(duration);
        m_dur->blockSignals(false);
    }
}

void ClipDurationDialog::slotCheckEnd()
{
    GenTime cropStart = m_cropStart->gentime();
    GenTime cropEnd = m_cropEnd->gentime();
    GenTime duration = m_length - cropEnd - cropStart;

    if (duration >= GenTime()) {
        m_dur->setValue(duration);
        slotCheckDuration();
    } else {
        m_cropEnd->blockSignals(true);
        m_cropEnd->setValue(m_length - m_dur->gentime() - cropStart);
        m_cropEnd->blockSignals(false);
    }
}

GenTime ClipDurationDialog::startPos() const
{
    return m_pos->gentime();
}

GenTime ClipDurationDialog::cropStart() const
{
    return m_cropStart->gentime();
}

GenTime ClipDurationDialog::duration() const
{
    return m_dur->gentime();
}
