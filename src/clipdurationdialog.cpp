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
#include "clipitem.h"
#include "kdenlivesettings.h"

#include <KDebug>
#include <KMessageBox>

#include <QWheelEvent>

ClipDurationDialog::ClipDurationDialog(AbstractClipItem *clip, Timecode tc, GenTime min, GenTime max, QWidget * parent):
        QDialog(parent),
        m_clip(clip),
        m_tc(tc),
        m_min(min),
        m_max(max),
        m_framesDisplay(KdenliveSettings::frametimecode())
{
    setFont(KGlobalSettings::toolBarFont());
    m_fps = m_tc.fps();
    m_view.setupUi(this);

    bool allowCrop = true;
    if (clip->type() == AVWIDGET) {
        ClipItem *item = static_cast <ClipItem *>(clip);
        int t = item->clipType();
        if (t == COLOR || t == IMAGE || t == TEXT) allowCrop = false;
    }

    if (!allowCrop || clip->type() == TRANSITIONWIDGET) {
        m_view.crop_up->hide();
        m_view.crop_down->hide();
        m_view.crop_position->hide();
        m_view.crop_label->hide();
        m_view.end_up->hide();
        m_view.end_down->hide();
        m_view.end_position->hide();
        m_view.end_label->hide();
    }

    m_crop = m_clip->cropStart().frames(m_fps);

    if (m_framesDisplay) {
        QValidator *valid = new QIntValidator();
        m_view.clip_position->setInputMask("");
        m_view.clip_position->setValidator(valid);
        m_view.crop_position->setInputMask("");
        m_view.clip_position->setValidator(valid);
        m_view.clip_duration->setInputMask("");
        m_view.clip_position->setValidator(valid);
        m_view.end_position->setInputMask("");
        m_view.clip_position->setValidator(valid);
    }
    m_view.clip_position->setText(tc.getDisplayTimecode(m_clip->startPos(), m_framesDisplay));
    m_view.crop_position->setText(tc.getDisplayTimecode(m_clip->cropStart(), m_framesDisplay));
    m_view.clip_duration->setText(tc.getDisplayTimecode(m_clip->cropDuration(), m_framesDisplay));
    m_view.end_position->setText(tc.getDisplayTimecode(m_clip->maxDuration() - m_clip->cropDuration() - m_clip->cropStart(), m_framesDisplay));

    connect(m_view.position_up, SIGNAL(clicked()), this, SLOT(slotPosUp()));
    connect(m_view.position_down, SIGNAL(clicked()), this, SLOT(slotPosDown()));
    connect(m_view.crop_up, SIGNAL(clicked()), this, SLOT(slotCropUp()));
    connect(m_view.crop_down, SIGNAL(clicked()), this, SLOT(slotCropDown()));
    connect(m_view.duration_up, SIGNAL(clicked()), this, SLOT(slotDurUp()));
    connect(m_view.duration_down, SIGNAL(clicked()), this, SLOT(slotDurDown()));
    connect(m_view.end_up, SIGNAL(clicked()), this, SLOT(slotEndUp()));
    connect(m_view.end_down, SIGNAL(clicked()), this, SLOT(slotEndDown()));
    connect(m_view.crop_position, SIGNAL(textChanged(const QString &)), this, SLOT(slotCheckCrop()));
    connect(m_view.clip_duration, SIGNAL(textChanged(const QString &)), this, SLOT(slotCheckDuration()));
    connect(m_view.clip_position, SIGNAL(textChanged(const QString &)), this, SLOT(slotCheckStart()));
    connect(m_view.end_position, SIGNAL(textChanged(const QString &)), this, SLOT(slotCheckEnd()));
    adjustSize();
}

ClipDurationDialog::~ClipDurationDialog()
{
}

void ClipDurationDialog::slotCheckStart()
{
    int pos = m_tc.getDisplayFrameCount(m_view.clip_position->text(), m_framesDisplay);
    int dur = m_tc.getDisplayFrameCount(m_view.clip_duration->text(), m_framesDisplay);

    GenTime start(pos, m_fps);
    GenTime duration(dur, m_fps);
    if (m_min != GenTime() && start < m_min) {
        m_view.clip_position->setText(m_tc.getDisplayTimecode(m_min, m_framesDisplay));
    } else if (m_max != GenTime() && start + duration > m_max) {
        m_view.clip_position->setText(m_tc.getDisplayTimecode(m_max - duration, m_framesDisplay));
    }
}

void ClipDurationDialog::slotCheckDuration()
{
    int pos = m_tc.getDisplayFrameCount(m_view.clip_position->text(), m_framesDisplay);
    int dur = m_tc.getDisplayFrameCount(m_view.clip_duration->text(), m_framesDisplay);
    int crop = m_tc.getDisplayFrameCount(m_view.crop_position->text(), m_framesDisplay);

    GenTime start(pos, m_fps);
    GenTime duration(dur, m_fps);
    GenTime cropStart(crop, m_fps);
    GenTime maxDuration;
    if (m_clip->maxDuration() == GenTime()) maxDuration = m_max;
    else maxDuration = m_max == GenTime() ? start + m_clip->maxDuration() - cropStart : qMin(m_max, start + m_clip->maxDuration() - cropStart);
    if (maxDuration != GenTime() && start + duration > maxDuration) {
        m_view.clip_duration->blockSignals(true);
        m_view.clip_duration->setText(m_tc.getDisplayTimecode(maxDuration - start, m_framesDisplay));
        m_view.clip_duration->blockSignals(false);
    }

    dur = m_tc.getDisplayFrameCount(m_view.clip_duration->text(), m_framesDisplay);
    GenTime durationUp(dur, m_fps);
    m_view.end_position->blockSignals(true);
    m_view.end_position->setText(m_tc.getDisplayTimecode(m_clip->maxDuration() - durationUp - cropStart, m_framesDisplay));
    m_view.end_position->blockSignals(false);
}

void ClipDurationDialog::slotCheckCrop()
{
    int dur = m_tc.getDisplayFrameCount(m_view.clip_duration->text(), m_framesDisplay);
    int crop = m_tc.getDisplayFrameCount(m_view.crop_position->text(), m_framesDisplay);

    int diff = crop - m_crop;
    if ((diff > 0 && diff < dur) || diff < 0) {
        dur -= diff;
    } else {
        m_view.crop_position->setText(m_tc.getDisplayTimecode(GenTime(m_crop, m_fps), m_framesDisplay));
        return;
    }
    GenTime duration(dur, m_fps);
    GenTime cropStart(crop, m_fps);
    GenTime maxDuration = m_clip->maxDuration();
    if (maxDuration != GenTime() && cropStart + duration > maxDuration) {
        m_view.crop_position->setText(m_tc.getDisplayTimecode(GenTime(m_crop, m_fps), m_framesDisplay));
    } else {
        m_crop = crop;
        m_view.clip_duration->blockSignals(true);
        m_view.clip_duration->setText(m_tc.getDisplayTimecode(duration, m_framesDisplay));
        m_view.clip_duration->blockSignals(false);
    }
}

void ClipDurationDialog::slotCheckEnd()
{
    int crop = m_tc.getDisplayFrameCount(m_view.crop_position->text(), m_framesDisplay);
    int end = m_tc.getDisplayFrameCount(m_view.end_position->text(), m_framesDisplay);
    int dur = m_clip->maxDuration().frames(m_fps) - crop - end;

    if (dur >= 0) {
        m_view.clip_duration->setText(m_tc.getDisplayTimecode(GenTime(dur, m_fps), m_framesDisplay));
    } else {
        dur = m_tc.getFrameCount(m_view.clip_duration->text());
        m_view.end_position->blockSignals(true);
        m_view.end_position->setText(m_tc.getDisplayTimecode(m_clip->maxDuration() - GenTime(crop + dur, m_fps), m_framesDisplay));
        m_view.end_position->blockSignals(false);
    }
}

void ClipDurationDialog::slotPosUp()
{
    int position = m_tc.getDisplayFrameCount(m_view.clip_position->text(), m_framesDisplay);
    //if (duration >= m_clip->duration().frames(m_fps)) return;
    position ++;
    m_view.clip_position->setText(m_tc.getDisplayTimecode(GenTime(position, m_fps), m_framesDisplay));
}

void ClipDurationDialog::slotPosDown()
{
    int position = m_tc.getDisplayFrameCount(m_view.clip_position->text(), m_framesDisplay);
    //if (duration >= m_clip->duration().frames(m_fps)) return;
    position --;
    m_view.clip_position->setText(m_tc.getDisplayTimecode(GenTime(position, m_fps), m_framesDisplay));
}

void ClipDurationDialog::slotDurUp()
{
    int duration = m_tc.getDisplayFrameCount(m_view.clip_duration->text(), m_framesDisplay);
    int crop = m_tc.getDisplayFrameCount(m_view.crop_position->text(), m_framesDisplay);

    if (m_clip->maxDuration() != GenTime() && duration + crop > m_clip->maxDuration().frames(m_fps)) return;
    duration ++;
    m_view.clip_duration->setText(m_tc.getDisplayTimecode(GenTime(duration, m_fps), m_framesDisplay));
}

void ClipDurationDialog::slotDurDown()
{
    int duration = m_tc.getDisplayFrameCount(m_view.clip_duration->text(), m_framesDisplay);
    if (duration <= 0) return;
    duration --;
    m_view.clip_duration->setText(m_tc.getDisplayTimecode(GenTime(duration, m_fps), m_framesDisplay));
}

void ClipDurationDialog::slotCropUp()
{
    int duration = m_tc.getDisplayFrameCount(m_view.clip_duration->text(), m_framesDisplay);
    int crop = m_tc.getDisplayFrameCount(m_view.crop_position->text(), m_framesDisplay);

    if (m_clip->maxDuration() != GenTime() && duration + crop > m_clip->maxDuration().frames(m_fps)) return;
    crop ++;
    m_view.crop_position->setText(m_tc.getDisplayTimecode(GenTime(crop, m_fps), m_framesDisplay));
}

void ClipDurationDialog::slotCropDown()
{
    int crop = m_tc.getDisplayFrameCount(m_view.crop_position->text(), m_framesDisplay);

    if (crop <= 0) return;
    crop --;
    m_view.crop_position->setText(m_tc.getDisplayTimecode(GenTime(crop, m_fps), m_framesDisplay));
}

void ClipDurationDialog::slotEndUp()
{
    int end = m_tc.getDisplayFrameCount(m_view.end_position->text(), m_framesDisplay);
    end ++;
    m_view.end_position->setText(m_tc.getDisplayTimecode(GenTime(end, m_fps), m_framesDisplay));
}

void ClipDurationDialog::slotEndDown()
{
    int end = m_tc.getDisplayFrameCount(m_view.end_position->text(), m_framesDisplay);
    if (end <= 0) return;
    end --;
    m_view.end_position->setText(m_tc.getDisplayTimecode(GenTime(end, m_fps), m_framesDisplay));
}

GenTime ClipDurationDialog::startPos() const
{
    int pos = m_tc.getDisplayFrameCount(m_view.clip_position->text(), m_framesDisplay);
    return GenTime(pos, m_fps);
}

GenTime ClipDurationDialog::cropStart() const
{
    int pos = m_tc.getDisplayFrameCount(m_view.crop_position->text(), m_framesDisplay);
    return GenTime(pos, m_fps);
}

GenTime ClipDurationDialog::duration() const
{
    int pos = m_tc.getDisplayFrameCount(m_view.clip_duration->text(), m_framesDisplay);
    return GenTime(pos, m_fps);
}

void ClipDurationDialog::wheelEvent(QWheelEvent * event)
{
    if (m_view.clip_position->underMouse()) {
        if (event->delta() > 0)
            slotPosUp();
        else
            slotPosDown();
    } else if (m_view.clip_duration->underMouse()) {
        if (event->delta() > 0)
            slotDurUp();
        else
            slotDurDown();
    } else if (m_view.crop_position->underMouse()) {
        if (event->delta() > 0)
            slotCropUp();
        else
            slotCropDown();
    } else if (m_view.end_position->underMouse()) {
        if (event->delta() > 0)
            slotEndUp();
        else
            slotEndDown();
    }
}

#include "clipdurationdialog.moc"


