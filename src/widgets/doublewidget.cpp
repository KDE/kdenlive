/**************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
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

#include "doublewidget.h"
#include "dragvalue.h"

#include <QVBoxLayout>

DoubleWidget::DoubleWidget(const QString &name, double value, double min, double max, double factor, double defaultValue, const QString &comment, int id,
                           const QString &suffix, int decimals, bool oddOnly, QWidget *parent)
    : QWidget(parent)
    , m_factor(factor)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_dragVal = new DragValue(name, defaultValue * m_factor, decimals, min, max, id, suffix, true, oddOnly, this);
    layout->addWidget(m_dragVal);
    setMinimumHeight(m_dragVal->height());

    if (!comment.isEmpty()) {
        setToolTip(comment);
    }
    m_dragVal->setValue(value * factor, false);
    connect(m_dragVal, &DragValue::valueChanged, this, &DoubleWidget::slotSetValue);
}

bool DoubleWidget::hasEditFocus() const
{
    return m_dragVal->hasEditFocus();
}

DoubleWidget::~DoubleWidget()
{
    delete m_dragVal;
}

int DoubleWidget::spinSize()
{
    return m_dragVal->spinSize();
}

void DoubleWidget::setSpinSize(int width)
{
    m_dragVal->setSpinSize(width);
}

void DoubleWidget::setValue(double value)
{
    m_dragVal->blockSignals(true);
    m_dragVal->setValue(value * m_factor);
    m_dragVal->blockSignals(false);
}

void DoubleWidget::enableEdit(bool enable)
{
    m_dragVal->setEnabled(enable);
}

void DoubleWidget::slotSetValue(double value, bool final)
{
    if (final) {
        emit valueChanged(value / m_factor);
    }
}

double DoubleWidget::getValue()
{
    return m_dragVal->value();
}

void DoubleWidget::slotReset()
{
    m_dragVal->slotReset();
}

void DoubleWidget::slotShowComment(bool show)
{
    Q_UNUSED(show)
}
