/* This file is part of the KDE project
   Copyright (c) 2010 Jean-Baptiste Mardelle <jb@kdenlive.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#include "timecodedisplay.h"
#include "kdenlivesettings.h"

#include <QSize>
#include <QStyle>
#include <QStylePainter>
#include <QStyleOptionSlider>
#include <QLineEdit>
#include <QValidator>
#include <QHBoxLayout>
#include <QFrame>
#include <QMouseEvent>
#include <QToolButton>

#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>


TimecodeDisplay::TimecodeDisplay(Timecode t, QWidget *parent)
        : QWidget(parent),
        m_minimum(0),
        m_maximum(-1)
{
    setupUi(this);
    lineedit->setFont(KGlobalSettings::toolBarFont());
    QFontMetrics fm = lineedit->fontMetrics();
    lineedit->setMaximumWidth(fm.width("88:88:88:888"));
    slotPrepareTimeCodeFormat(t);
    connect(uparrow, SIGNAL(clicked()), this, SLOT(slotValueUp()));
    connect(downarrow, SIGNAL(clicked()), this, SLOT(slotValueDown()));
    connect(lineedit, SIGNAL(editingFinished()), this, SIGNAL(editingFinished()));
}

TimecodeDisplay::~TimecodeDisplay()
{
}

void TimecodeDisplay::slotValueUp()
{
    int val = value();
    val++;
    if (m_maximum > -1 && val > m_maximum) val = m_maximum;
    setValue(val);
    lineedit->clearFocus();
    emit editingFinished();
}

void TimecodeDisplay::slotValueDown()
{
    int val = value();
    val--;
    if (val < m_minimum) val = m_minimum;
    setValue(val);
    lineedit->clearFocus();
    emit editingFinished();
}

void TimecodeDisplay::slotPrepareTimeCodeFormat(Timecode t)
{
    m_timecode = t;
    m_frametimecode = KdenliveSettings::frametimecode();
    QString val = lineedit->text();
    lineedit->setInputMask("");
    if (m_frametimecode) {
        int frames = m_timecode.getFrameCount(lineedit->text());
        QIntValidator *valid = new QIntValidator(lineedit);
        valid->setBottom(0);
        lineedit->setValidator(valid);
        lineedit->setText(QString::number(frames));
    } else {
        int pos = lineedit->text().toInt();
        lineedit->setValidator(m_timecode.validator());
        lineedit->setText(m_timecode.getTimecodeFromFrames(pos));
    }
}

void TimecodeDisplay::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Up) slotValueUp();
    else if (e->key() == Qt::Key_Down) slotValueDown();
    else QWidget::keyPressEvent(e);
}

void TimecodeDisplay::wheelEvent(QWheelEvent *e)
{
    if (e->delta() > 0) slotValueUp();
    else slotValueDown();
}


int TimecodeDisplay::maximum() const
{
    return m_maximum;
}

int TimecodeDisplay::minimum() const
{
    return m_minimum;
}

int TimecodeDisplay::value() const
{
    int frames;
    if (m_frametimecode) frames = lineedit->text().toInt();
    else frames = m_timecode.getFrameCount(lineedit->text());
    return frames;
}

Timecode TimecodeDisplay::timecode() const
{
    return m_timecode;
}

void TimecodeDisplay::setRange(int min, int max)
{
    m_minimum = min;
    m_maximum = max;
}

void TimecodeDisplay::setValue(const QString &value)
{
    if (m_frametimecode) {
        lineedit->setText(QString::number(m_timecode.getFrameCount(value)));
    } else lineedit->setText(value);
}

void TimecodeDisplay::setValue(int value)
{
    /*    if (value < m_minimum)
            value = m_minimum;
        if (value > m_maximum)
            value = m_maximum;*/
    if (m_frametimecode) lineedit->setText(QString::number(value));
    else lineedit->setText(m_timecode.getTimecodeFromFrames(value));

    /*    setEditText(KGlobal::locale()->formatNumber(value, d->decimals));
        d->slider->blockSignals(true);
        d->slider->setValue(int((value - d->minimum) * 256 / (d->maximum - d->minimum) + 0.5));
        d->slider->blockSignals(false);*/
    //emit valueChanged(value, true);
}

#include <timecodedisplay.moc>
