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
        m_max(max)
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

    if (KdenliveSettings::frametimecode()) {
        QValidator *valid = new QIntValidator();
        m_view.clip_position->setInputMask("");
        m_view.clip_position->setValidator(valid);
        m_view.clip_position->setText(QString::number(m_clip->startPos().frames(m_fps)));
        m_view.crop_position->setInputMask("");
        m_view.clip_position->setValidator(valid);
        m_view.crop_position->setText(QString::number(m_clip->cropStart().frames(m_fps)));
        m_view.clip_duration->setInputMask("");
        m_view.clip_position->setValidator(valid);
        m_view.clip_duration->setText(QString::number(m_clip->cropDuration().frames(m_fps)));
        m_view.end_position->setInputMask("");
        m_view.clip_position->setValidator(valid);
        m_view.end_position->setText(QString::number((m_clip->maxDuration() - m_clip->cropDuration() - m_clip->cropStart()).frames(m_fps)));
    } else {
        m_view.clip_position->setText(tc.getTimecode(m_clip->startPos()));
        m_view.crop_position->setText(tc.getTimecode(m_clip->cropStart()));
        m_view.clip_duration->setText(tc.getTimecode(m_clip->cropDuration()));
        m_view.end_position->setText(tc.getTimecode(m_clip->maxDuration() - m_clip->cropDuration() - m_clip->cropStart()));
    }
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
    int pos;
    int dur;
    if (KdenliveSettings::frametimecode()) {
        pos = m_view.clip_position->text().toInt();
        dur = m_view.clip_duration->text().toInt();
    } else {
        pos = m_tc.getFrameCount(m_view.clip_position->text());
        dur = m_tc.getFrameCount(m_view.clip_duration->text());
    }

    GenTime start(pos, m_fps);
    GenTime duration(dur, m_fps);
    if (m_min != GenTime() && start < m_min) {
        if (KdenliveSettings::frametimecode()) m_view.clip_position->setText(QString::number(m_min.frames(m_fps)));
        else m_view.clip_position->setText(m_tc.getTimecode(m_min));
    } else if (m_max != GenTime() && start + duration > m_max) {
        if (KdenliveSettings::frametimecode()) m_view.clip_position->setText(QString::number((m_max - duration).frames(m_fps)));
        else m_view.clip_position->setText(m_tc.getTimecode(m_max - duration));
    }
}

void ClipDurationDialog::slotCheckDuration()
{
    int pos;
    int dur;
    int crop;
    if (KdenliveSettings::frametimecode()) {
        pos = m_view.clip_position->text().toInt();
        dur = m_view.clip_duration->text().toInt();
        crop = m_view.crop_position->text().toInt();
    } else {
        pos = m_tc.getFrameCount(m_view.clip_position->text());
        dur = m_tc.getFrameCount(m_view.clip_duration->text());
        crop = m_tc.getFrameCount(m_view.crop_position->text());
    }

    GenTime start(pos, m_fps);
    GenTime duration(dur, m_fps);
    GenTime cropStart(crop, m_fps);
    GenTime maxDuration;
    if (m_clip->maxDuration() == GenTime()) maxDuration = m_max;
    else maxDuration = m_max == GenTime() ? start + m_clip->maxDuration() - cropStart : qMin(m_max, start + m_clip->maxDuration() - cropStart);
    if (maxDuration != GenTime() && start + duration > maxDuration) {
        m_view.clip_duration->blockSignals(true);
        if (KdenliveSettings::frametimecode()) m_view.clip_duration->setText(QString::number((maxDuration - start).frames(m_fps)));
        else m_view.clip_duration->setText(m_tc.getTimecode(maxDuration - start));
        m_view.clip_duration->blockSignals(false);
    }

    if (KdenliveSettings::frametimecode()) dur = m_view.clip_duration->text().toInt();
    else dur = m_tc.getFrameCount(m_view.clip_duration->text());
    GenTime durationUp(dur, m_fps);
    m_view.end_position->blockSignals(true);
    if (KdenliveSettings::frametimecode()) m_view.end_position->setText(QString::number((m_clip->maxDuration() - durationUp - cropStart).frames(m_fps)));
    else m_view.end_position->setText(m_tc.getTimecode(m_clip->maxDuration() - durationUp - cropStart));
    m_view.end_position->blockSignals(false);
}

void ClipDurationDialog::slotCheckCrop()
{
    int dur;
    int crop;
    if (KdenliveSettings::frametimecode()) {
        dur = m_view.clip_duration->text().toInt();
        crop = m_view.crop_position->text().toInt();
    } else {
        dur = m_tc.getFrameCount(m_view.clip_duration->text());
        crop = m_tc.getFrameCount(m_view.crop_position->text());
    }
    int diff = crop - m_crop;
    if ((diff > 0 && diff < dur) || diff < 0) {
        dur -= diff;
    } else {
        if (KdenliveSettings::frametimecode()) m_view.crop_position->setText(QString::number(m_crop));
        else m_view.crop_position->setText(m_tc.getTimecode(GenTime(m_crop, m_fps)));
        return;
    }
    GenTime duration(dur, m_fps);
    GenTime cropStart(crop, m_fps);
    GenTime maxDuration = m_clip->maxDuration();
    if (maxDuration != GenTime() && cropStart + duration > maxDuration) {
        if (KdenliveSettings::frametimecode()) m_view.crop_position->setText(QString::number(m_crop));
        else m_view.crop_position->setText(m_tc.getTimecode(GenTime(m_crop, m_fps)));
    } else {
        m_crop = crop;
        m_view.clip_duration->blockSignals(true);
        if (KdenliveSettings::frametimecode()) m_view.clip_duration->setText(QString::number(duration.frames(m_fps)));
        else m_view.clip_duration->setText(m_tc.getTimecode(duration));
        m_view.clip_duration->blockSignals(false);
    }
}

void ClipDurationDialog::slotCheckEnd()
{
    int crop;
    int end;
    int dur;
    if (KdenliveSettings::frametimecode()) {
        crop = m_view.crop_position->text().toInt();
        end = m_view.end_position->text().toInt();
        dur = m_clip->maxDuration().frames(m_fps) - crop - end;
    } else {
        crop = m_tc.getFrameCount(m_view.crop_position->text());
        end = m_tc.getFrameCount(m_view.end_position->text());
        dur = m_tc.getFrameCount(m_tc.getTimecode(m_clip->maxDuration())) - crop - end;
    }
    if (dur >= 0) {
        if (KdenliveSettings::frametimecode()) m_view.clip_duration->setText(QString::number(dur));
        else m_view.clip_duration->setText(m_tc.getTimecode(GenTime(dur, m_fps)));
    } else {
        dur = m_tc.getFrameCount(m_view.clip_duration->text());
        m_view.end_position->blockSignals(true);
        if (KdenliveSettings::frametimecode()) m_view.end_position->setText(QString::number(m_clip->maxDuration().frames(m_fps) - (crop + dur)));
        else m_view.end_position->setText(m_tc.getTimecode(m_clip->maxDuration() - GenTime(crop + dur, m_fps)));
        m_view.end_position->blockSignals(false);
    }
}

void ClipDurationDialog::slotPosUp()
{
    int position;
    if (KdenliveSettings::frametimecode()) position = m_view.clip_position->text().toInt();
    else position = m_tc.getFrameCount(m_view.clip_position->text());
    //if (duration >= m_clip->duration().frames(m_fps)) return;
    position ++;
    if (KdenliveSettings::frametimecode()) m_view.clip_position->setText(QString::number(position));
    else m_view.clip_position->setText(m_tc.getTimecode(GenTime(position, m_fps)));
}

void ClipDurationDialog::slotPosDown()
{
    int position;
    if (KdenliveSettings::frametimecode()) position = m_view.clip_position->text().toInt();
    else position = m_tc.getFrameCount(m_view.clip_position->text());
    //if (duration >= m_clip->duration().frames(m_fps)) return;
    position --;
    if (KdenliveSettings::frametimecode()) m_view.clip_position->setText(QString::number(position));
    else m_view.clip_position->setText(m_tc.getTimecode(GenTime(position, m_fps)));
}

void ClipDurationDialog::slotDurUp()
{
    int duration;
    int crop;
    if (KdenliveSettings::frametimecode()) {
        duration = m_view.clip_duration->text().toInt();
        crop = m_view.crop_position->text().toInt();
    } else {
        duration = m_tc.getFrameCount(m_view.clip_duration->text());
        crop = m_tc.getFrameCount(m_view.crop_position->text());
    }
    if (m_clip->maxDuration() != GenTime() && duration + crop > m_clip->maxDuration().frames(m_fps)) return;
    duration ++;
    if (KdenliveSettings::frametimecode()) m_view.clip_duration->setText(QString::number(duration));
    else m_view.clip_duration->setText(m_tc.getTimecode(GenTime(duration, m_fps)));
}

void ClipDurationDialog::slotDurDown()
{
    int duration;
    if (KdenliveSettings::frametimecode()) {
        duration = m_view.clip_duration->text().toInt();
    } else {
        duration = m_tc.getFrameCount(m_view.clip_duration->text());
    }
    if (duration <= 0) return;
    duration --;
    if (KdenliveSettings::frametimecode()) m_view.clip_duration->setText(QString::number(duration));
    else m_view.clip_duration->setText(m_tc.getTimecode(GenTime(duration, m_fps)));
}

void ClipDurationDialog::slotCropUp()
{
    int duration;
    int crop;
    if (KdenliveSettings::frametimecode()) {
        duration = m_view.clip_duration->text().toInt();
        crop = m_view.crop_position->text().toInt();
    } else {
        duration = m_tc.getFrameCount(m_view.clip_duration->text());
        crop = m_tc.getFrameCount(m_view.crop_position->text());
    }

    if (m_clip->maxDuration() != GenTime() && duration + crop > m_clip->maxDuration().frames(m_fps)) return;
    crop ++;
    if (KdenliveSettings::frametimecode()) m_view.crop_position->setText(QString::number(crop));
    else m_view.crop_position->setText(m_tc.getTimecode(GenTime(crop, m_fps)));
}

void ClipDurationDialog::slotCropDown()
{
    int crop;
    if (KdenliveSettings::frametimecode()) {
        crop = m_view.crop_position->text().toInt();
    } else {
        crop = m_tc.getFrameCount(m_view.crop_position->text());
    }

    if (crop <= 0) return;
    crop --;
    if (KdenliveSettings::frametimecode()) m_view.crop_position->setText(QString::number(crop));
    else m_view.crop_position->setText(m_tc.getTimecode(GenTime(crop, m_fps)));
}

void ClipDurationDialog::slotEndUp()
{
    int end;
    if (KdenliveSettings::frametimecode()) end = m_view.end_position->text().toInt();
    else end = m_tc.getFrameCount(m_view.end_position->text());
    kDebug() << "/ / / / FIRST END: " << end;
    end ++;
    if (KdenliveSettings::frametimecode()) m_view.end_position->setText(QString::number(end));
    else m_view.end_position->setText(m_tc.getTimecode(GenTime(end, m_fps)));
    kDebug() << "/ / / / SEC END: " << end;
}

void ClipDurationDialog::slotEndDown()
{
    int end;
    if (KdenliveSettings::frametimecode()) end = m_view.end_position->text().toInt();
    else end = m_tc.getFrameCount(m_view.end_position->text());
    if (end <= 0) return;
    end --;
    if (KdenliveSettings::frametimecode()) m_view.end_position->setText(QString::number(end));
    else m_view.end_position->setText(m_tc.getTimecode(GenTime(end, m_fps)));
}

GenTime ClipDurationDialog::startPos() const
{
    int pos;
    if (KdenliveSettings::frametimecode()) pos = m_view.clip_position->text().toInt();
    else pos = m_tc.getFrameCount(m_view.clip_position->text());
    return GenTime(pos, m_fps);
}

GenTime ClipDurationDialog::cropStart() const
{
    int pos;
    if (KdenliveSettings::frametimecode()) pos = m_view.crop_position->text().toInt();
    else pos = m_tc.getFrameCount(m_view.crop_position->text());
    return GenTime(pos, m_fps);
}

GenTime ClipDurationDialog::duration() const
{
    int pos;
    if (KdenliveSettings::frametimecode()) pos = m_view.clip_duration->text().toInt();
    else pos = m_tc.getFrameCount(m_view.clip_duration->text());
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


