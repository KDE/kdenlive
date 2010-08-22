/***************************************************************************
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

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>


DoubleParameterWidget::DoubleParameterWidget(const QString &name, int value, int min, int max, const QString suffix, QWidget *parent) :
        QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);

    m_name = new QLabel(name, this);
    layout->addWidget(m_name);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(min, max);
    //m_slider->setPageStep((max - min) / 10);
    layout->addWidget(m_slider);

    m_spinBox = new QSpinBox(this);
    m_spinBox->setRange(min, max);
    if (!suffix.isEmpty())
        m_spinBox->setSuffix(suffix);
    layout->addWidget(m_spinBox);

    connect(m_slider,  SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));
    connect(m_spinBox, SIGNAL(valueChanged(int)), this, SLOT(setValue(int)));

    m_spinBox->setValue(value);
}

void DoubleParameterWidget::setValue(int value)
{
    m_slider->blockSignals(true);
    m_spinBox->blockSignals(true);

    m_slider->setValue(value);
    m_spinBox->setValue(value);

    m_slider->blockSignals(false);
    m_spinBox->blockSignals(false);

    emit valueChanged(value);
}

int DoubleParameterWidget::getValue()
{
    return m_spinBox->value();
}


void DoubleParameterWidget::setName(const QString& name)
{
    m_name->setText(name);
}

#include "doubleparameterwidget.moc"