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

#include <QLineEdit>
#include <QValidator>
#include <QMouseEvent>

#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>


TimecodeDisplay::TimecodeDisplay(Timecode t, QWidget *parent)
        : QWidget(parent),
        m_timecode(t),
        m_minimum(0),
        m_maximum(-1)
{
    setupUi(this);
    lineedit->setFont(KGlobalSettings::toolBarFont());
    QFontMetrics fm = lineedit->fontMetrics();
    lineedit->setMaximumWidth(fm.width("88:88:88:888"));
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);

    setTimeCodeFormat(KdenliveSettings::frametimecode(), true);

    connect(uparrow, SIGNAL(clicked()), this, SLOT(slotValueUp()));
    connect(downarrow, SIGNAL(clicked()), this, SLOT(slotValueDown()));
    connect(lineedit, SIGNAL(editingFinished()), this, SIGNAL(editingFinished()));
}

void TimecodeDisplay::slotValueUp()
{
    int val = getValue();
    val++;
    setValue(val);
    lineedit->clearFocus();
    emit editingFinished();
}

void TimecodeDisplay::slotValueDown()
{
    int val = getValue();
    val--;
    setValue(val);
    lineedit->clearFocus();
    emit editingFinished();
}

void TimecodeDisplay::setTimeCodeFormat(bool frametimecode, bool init)
{
    if (!init && m_frametimecode == frametimecode) return;
    int val = getValue();
    m_frametimecode = frametimecode;
    if (m_frametimecode) {
        QIntValidator *valid = new QIntValidator(lineedit);
        valid->setBottom(0);
        lineedit->setValidator(valid);
    } else {
        lineedit->setValidator(m_timecode.validator());
    }
    setValue(val);
}

void TimecodeDisplay::slotUpdateTimeCodeFormat()
{
    setTimeCodeFormat(KdenliveSettings::frametimecode());
}

void TimecodeDisplay::updateTimeCode(Timecode t)
{
    m_timecode = t;
    setTimeCodeFormat(KdenliveSettings::frametimecode());
}

void TimecodeDisplay::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Up)
        slotValueUp();
    else if (e->key() == Qt::Key_Down)
        slotValueDown();
    else
        QWidget::keyPressEvent(e);
}

void TimecodeDisplay::wheelEvent(QWheelEvent *e)
{
    if (e->delta() > 0)
        slotValueUp();
    else
        slotValueDown();
}


int TimecodeDisplay::maximum() const
{
    return m_maximum;
}

int TimecodeDisplay::minimum() const
{
    return m_minimum;
}

int TimecodeDisplay::getValue() const
{
    if (m_frametimecode) return lineedit->text().toInt();
    else return m_timecode.getFrameCount(lineedit->text());
}

GenTime TimecodeDisplay::gentime() const
{
    return GenTime(getValue(), m_timecode.fps());
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
    setValue(m_timecode.getFrameCount(value));
}

void TimecodeDisplay::setValue(int value)
{
    if (value < m_minimum)
        value = m_minimum;
    if (m_maximum > m_minimum && value > m_maximum)
        value = m_maximum;

    if (value == getValue()) return;
    downarrow->setEnabled(value > m_minimum);
    uparrow->setEnabled(m_maximum < m_minimum || value < m_maximum);

    if (m_frametimecode)
        lineedit->setText(QString::number(value));
    else {
        QString v = m_timecode.getTimecodeFromFrames(value);
        kDebug() << "// SETTING TO: " << value << " = " << v << "( " << m_timecode.fps();
        lineedit->setText(v);
    }
}

void TimecodeDisplay::setValue(GenTime value)
{
    setValue(m_timecode.getTimecode(value));
}

#include <timecodedisplay.moc>
