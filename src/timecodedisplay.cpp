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
#include <krestrictedline.h>
#include <KColorScheme>
#include <KRestrictedLine>

TimecodeDisplay::TimecodeDisplay(Timecode t, QWidget *parent)
        : QAbstractSpinBox(parent),
        m_timecode(t),
        m_frametimecode(false),
        m_minimum(0),
        m_maximum(-1)
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

    setTimeCodeFormat(KdenliveSettings::frametimecode(), true);

    connect(lineEdit(), SIGNAL(editingFinished()), this, SLOT(slotEditingFinished()));
    connect(lineEdit(), SIGNAL(cursorPositionChanged(int, int)), this, SLOT(slotCursorPositionChanged(int, int)));
}

// virtual protected
QAbstractSpinBox::StepEnabled TimecodeDisplay::stepEnabled () const
{
    QAbstractSpinBox::StepEnabled result = QAbstractSpinBox::StepNone;
    if (getValue() > m_minimum) result |= QAbstractSpinBox::StepDownEnabled;
    if (m_maximum == -1 || getValue() < m_maximum) result |= QAbstractSpinBox::StepUpEnabled;
    return result;
}

// virtual
void TimecodeDisplay::stepBy(int steps)
{
    int val = getValue();
    val += steps;
    setValue(val);
    emit editingFinished();
}

void TimecodeDisplay::setTimeCodeFormat(bool frametimecode, bool init)
{
    if (!init && m_frametimecode == frametimecode) return;
    int val = getValue();
    m_frametimecode = frametimecode;
    if (m_frametimecode) {
        QIntValidator *valid = new QIntValidator(lineEdit());
        valid->setBottom(0);
        lineEdit()->setValidator(valid);
    } else {
        lineEdit()->setValidator(m_timecode.validator());
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
    if (e->key() == Qt::Key_Return)
        slotEditingFinished();
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

/*
void TimecodeDisplay::wheelEvent(QWheelEvent *e)
{
    if (e->delta() > 0)
        slotValueUp();
    else
        slotValueDown();
}*/


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
    if (m_frametimecode) return lineEdit()->text().toInt();
    else return m_timecode.getFrameCount(lineEdit()->text());
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

    if (value == getValue() && !lineEdit()->text().isEmpty()) return;
    //downarrow->setEnabled(value > m_minimum);
    //uparrow->setEnabled(m_maximum < m_minimum || value < m_maximum);

    if (m_frametimecode)
        lineEdit()->setText(QString::number(value));
    else {
        QString v = m_timecode.getTimecodeFromFrames(value);
        lineEdit()->setText(v);
    }
}

void TimecodeDisplay::setValue(GenTime value)
{
    setValue(m_timecode.getTimecode(value));
}

void TimecodeDisplay::slotCursorPositionChanged(int oldPos, int newPos)
{
    if (!lineEdit()->hasFocus()) return;
    lineEdit()->blockSignals(true);
    QString text = lineEdit()->text();

    if (newPos < text.size() && !text.at(newPos).isDigit()) {
        // char at newPos is a separator (':' or ';')

        // make it possible move the cursor backwards at separators
        if (newPos == oldPos - 1)
            lineEdit()->setSelection(newPos, -1);
        else
            lineEdit()->setSelection(newPos + 2, -1);
    } else if (newPos < text.size()) {
        lineEdit()->setSelection(newPos + 1, -1);
    } else {
        lineEdit()->setSelection(newPos, -1);
    }

    lineEdit()->blockSignals(false);
}

void TimecodeDisplay::slotEditingFinished()
{
    clearFocus();
    lineEdit()->deselect();
    emit editingFinished();
}

#include <timecodedisplay.moc>
