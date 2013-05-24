/***************************************************************************
 *   Copyright (C) 2010 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2012 by Till Theato (root@ttill.de)                     *
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

#include "timecodewidget.h"
#include "core.h"
#include "project/projectmanager.h"
#include "project/project.h"
#include <QLineEdit>
#include <QKeyEvent>
#include <KGlobalSettings>


TimecodeWidget::TimecodeWidget(QWidget* parent) :
    QAbstractSpinBox(parent),
    m_minimum(0),
    m_maximum(-1),
    m_value(0),
    m_timecodeFormatter(0)
{
    setFocusPolicy(Qt::ClickFocus);
    lineEdit()->setFont(KGlobalSettings::toolBarFont());
    lineEdit()->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFontMetrics fm = lineEdit()->fontMetrics();
    setMinimumWidth(fm.width("88:88:88:88888888") + contentsMargins().right() + contentsMargins().right());
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    setAccelerated(true);

    if (pCore->projectManager()->current()) {
        updateMask();
    }

    connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));

    connect(lineEdit(), SIGNAL(editingFinished()), this, SLOT(onEditingFinished()));
}

int TimecodeWidget::minimum() const
{
    return m_minimum;
}

void TimecodeWidget::setMinimum(int minimum)
{
    m_minimum = minimum;
}

int TimecodeWidget::maximum() const
{
    return m_maximum;
}

void TimecodeWidget::setMaximum(int maximum)
{
    m_maximum = maximum;
}

void TimecodeWidget::setRange(int minimum, int maximum)
{
    setMinimum(minimum);
    setMaximum(maximum);
}

Timecode TimecodeWidget::value() const
{
    return m_value;
}

void TimecodeWidget::stepBy(int steps)
{
    setValue(m_value + steps);
    emit editingFinished();
    emit valueChanged(m_value.frames());
}

void TimecodeWidget::setProject(Project* project)
{
    m_timecodeFormatter = project->timecodeFormatter();
    if (!m_value.formatter()) {
        m_value.setFormatter(m_timecodeFormatter);
    }
    connect(m_timecodeFormatter, SIGNAL(framerateChanged()), this, SLOT(updateMask()));
    connect(m_timecodeFormatter, SIGNAL(defaultFormatChanged()), this, SLOT(updateMask()));

    updateMask();
}

void TimecodeWidget::setValue(Timecode value)
{
    if (value < m_minimum) {
        value = m_minimum;
    }
    if (m_maximum > m_minimum && value > m_maximum) {
        value = m_maximum;
    }

    if (value != m_value || lineEdit()->text().isEmpty()) {
        m_value = value;
        lineEdit()->setText(value.formatted());
    }
}

void TimecodeWidget::onEditingFinished()
{
    if (!m_timecodeFormatter) return;
    lineEdit()->deselect();
    setValue(m_timecodeFormatter->fromString(lineEdit()->text()));
    emit editingFinished();
    emit valueChanged(m_value.frames());
}

void TimecodeWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Escape) {
        event->setAccepted(true);
        clearFocus();
    } else {
        QAbstractSpinBox::keyPressEvent(event);
    }
}

void TimecodeWidget::mouseReleaseEvent(QMouseEvent* event)
{
    QAbstractSpinBox::mouseReleaseEvent(event);
    if (!lineEdit()->underMouse()) {
        clearFocus();
    }
}

QAbstractSpinBox::StepEnabled TimecodeWidget::stepEnabled() const
{
    QAbstractSpinBox::StepEnabled result = QAbstractSpinBox::StepNone;
    if (m_value > m_minimum) {
        result |= QAbstractSpinBox::StepDownEnabled;
    }
    if (m_maximum == -1 || m_value < m_maximum) {
        result |= QAbstractSpinBox::StepUpEnabled;
    }
    return result;
}

void TimecodeWidget::updateMask()
{
    lineEdit()->clear();
    if (!m_timecodeFormatter) return;
    if (m_timecodeFormatter->defaultFormat() == TimecodeFormatter::Frames) {
        QIntValidator *validator = new QIntValidator(lineEdit());
        validator->setBottom(0);
        lineEdit()->setValidator(validator);
        lineEdit()->setInputMask(QString());
    } else {
        lineEdit()->setValidator(0);
        lineEdit()->setInputMask(m_value.mask());
    }
    setValue(m_value);
}

#include "timecodewidget.moc"
