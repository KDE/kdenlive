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

#include "doubleparameterwidget.h"
#include "effectstack/dragvalue.h"

#include <QGridLayout>
#include <QRadioButton>

DoubleParameterWidget::DoubleParameterWidget(const QString &name, double value, double min, double max, double defaultValue, const QString &comment, int id, const QString &suffix, int decimals, bool showRadiobutton, QWidget *parent)
    : AbstractParamWidget(parent)
    , factor(1)
    , m_radio(nullptr)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    QGridLayout *layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    if (showRadiobutton) {
        m_radio = new QRadioButton(this);
        layout->addWidget(m_radio, 0, 0);
        connect(m_radio, &QRadioButton::toggled, this, &DoubleParameterWidget::displayInTimeline);
    }

    m_dragVal = new DragValue(name, defaultValue, decimals, min, max, id, suffix, true, this);
    layout->addWidget(m_dragVal, 0, 1);

    if (!comment.isEmpty()) {
        setToolTip(comment);
    }
    m_dragVal->setValue(value, false);
    connect(m_dragVal, &DragValue::valueChanged, this, &DoubleParameterWidget::slotSetValue);
    connect(m_dragVal, &DragValue::inTimeline, this, &DoubleParameterWidget::setInTimeline);

    //connect the signal of the derived class to the signal of the base class
    connect(this, &DoubleParameterWidget::valueChanged,
            [this](double){ emit qobject_cast<AbstractParamWidget*>(this)->valueChanged();});

}

bool DoubleParameterWidget::hasEditFocus() const
{
    return m_dragVal->hasEditFocus();
}

DoubleParameterWidget::~DoubleParameterWidget()
{
    delete m_dragVal;
}

int DoubleParameterWidget::spinSize()
{
    return m_dragVal->spinSize();
}

void DoubleParameterWidget::setSpinSize(int width)
{
    m_dragVal->setSpinSize(width);
}

void DoubleParameterWidget::setValue(double value)
{
    m_dragVal->blockSignals(true);
    m_dragVal->setValue(value);
    m_dragVal->blockSignals(false);
}

void DoubleParameterWidget::enableEdit(bool enable)
{
    m_dragVal->setEnabled(enable);
}

void DoubleParameterWidget::slotSetValue(double value, bool final)
{
    if (m_radio && !m_radio->isChecked()) {
        m_radio->setChecked(true);
    }
    if (final) {
        emit valueChanged(value);
    }
}

void DoubleParameterWidget::setChecked(bool check)
{
    if (m_radio) {
        m_radio->setChecked(check);
    }
}

void DoubleParameterWidget::hideRadioButton()
{
    if (m_radio) {
        m_radio->setVisible(false);
    }
}

double DoubleParameterWidget::getValue()
{
    return m_dragVal->value();
}

void DoubleParameterWidget::slotReset()
{
    m_dragVal->slotReset();
}

void DoubleParameterWidget::setInTimelineProperty(bool intimeline)
{
    m_dragVal->setInTimelineProperty(intimeline);
}

void DoubleParameterWidget::slotShowComment(bool show)
{
    Q_UNUSED(show)
}

