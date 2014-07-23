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

#include <KLineEdit>
#include <QValidator>
#include <QMouseEvent>

#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>
#include <krestrictedline.h>
#include <KColorScheme>
#include <KRestrictedLine>

TimecodeDisplay::TimecodeDisplay(const Timecode& t, QWidget *parent)
        : QAbstractSpinBox(parent),
        m_timecode(t),
        m_frametimecode(false),
        m_minimum(0),
        m_maximum(-1),
        m_value(0)
{
    lineEdit()->setFont(KGlobalSettings::toolBarFont());
    lineEdit()->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFontMetrics fm = lineEdit()->fontMetrics();
#if QT_VERSION >= 0x040600
    setMinimumWidth(fm.width("88:88:88:88888888") + contentsMargins().right() + contentsMargins().right());
#else
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    setMinimumWidth(fm.width("88:88:88:88888888") + left + right);
#endif
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    setAccelerated(true);

    setValue(m_minimum);

    setTimeCodeFormat(KdenliveSettings::frametimecode(), true);

    connect(lineEdit(), SIGNAL(editingFinished()), this, SLOT(slotEditingFinished()));
}

void TimecodeDisplay::setTimeCodeFormat(bool frametimecode, bool init)
{
    if (!init && m_frametimecode == frametimecode) return;
    m_frametimecode = frametimecode;
    lineEdit()->clear();
    if (m_frametimecode) {
        QIntValidator *valid = new QIntValidator(lineEdit());
        valid->setBottom(0);
        lineEdit()->setValidator(valid);
        lineEdit()->setInputMask(QString());
    } else {
        lineEdit()->setValidator(0);
        lineEdit()->setInputMask(m_timecode.mask());
    }
    setValue(m_value);
}

void TimecodeDisplay::slotUpdateTimeCodeFormat()
{
    setTimeCodeFormat(KdenliveSettings::frametimecode());
}

void TimecodeDisplay::updateTimeCode(const Timecode &t)
{
    m_timecode = t;
    setTimeCodeFormat(KdenliveSettings::frametimecode());
}

void TimecodeDisplay::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        e->setAccepted(true);
        clearFocus();
    }
    else
        QAbstractSpinBox::keyPressEvent(e);
}

void TimecodeDisplay::mouseReleaseEvent(QMouseEvent *e)
{
    QAbstractSpinBox::mouseReleaseEvent(e);
    if (!lineEdit()->underMouse()) {
        clearFocus();
    }
}


void TimecodeDisplay::wheelEvent(QWheelEvent *e)
{
    QAbstractSpinBox::wheelEvent(e);
    clearFocus();
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
    return m_value;
}

GenTime TimecodeDisplay::gentime() const
{
    return GenTime(m_value, m_timecode.fps());
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

    if (m_frametimecode) {
	if (value == m_value && !lineEdit()->text().isEmpty()) return;
	m_value = value;
        lineEdit()->setText(QString::number(value));
    }
    else {
	if (value == m_value && lineEdit()->text() != ":::") return;
	m_value = value;
        QString v = m_timecode.getTimecodeFromFrames(value);
        lineEdit()->setText(v);
    }
}

void TimecodeDisplay::setValue(const GenTime &value)
{
    setValue((int) value.frames(m_timecode.fps()));
}


void TimecodeDisplay::slotEditingFinished()
{
    lineEdit()->deselect();
    if (m_frametimecode) setValue(lineEdit()->text().toInt());
    else setValue(lineEdit()->text());
    emit timeCodeEditingFinished();
}

#include <timecodedisplay.moc>
