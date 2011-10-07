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


#include "choosecolorwidget.h"
#include "colorpickerwidget.h"

#include <QLabel>
#include <QHBoxLayout>

#include <KColorButton>
#include <KLocalizedString>


ChooseColorWidget::ChooseColorWidget(QString text, QColor color, QWidget *parent) :
        QWidget(parent)
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QLabel *label = new QLabel(text, this);
    m_button = new KColorButton(color, this);
    m_button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    ColorPickerWidget *picker = new ColorPickerWidget(this);

    layout->addWidget(label);

    QHBoxLayout *subLayout = new QHBoxLayout(this);
    subLayout->setContentsMargins(0, 0, 0, 0);
    subLayout->setSpacing(0);
    subLayout->addWidget(m_button);//, 1, Qt::AlignRight);
    subLayout->addWidget(picker, 0, Qt::AlignRight);
    layout->addLayout(subLayout);

    connect(picker, SIGNAL(colorPicked(QColor)), this, SLOT(setColor(QColor)));
    connect(picker, SIGNAL(displayMessage(const QString&, int)), this, SIGNAL(displayMessage(const QString&, int)));
    connect(m_button, SIGNAL(changed(QColor)), this, SIGNAL(modified()));
}

QColor ChooseColorWidget::getColor()
{
    return m_button->color();
}

void ChooseColorWidget::setColor(QColor color)
{
    m_button->setColor(color);
}

#include "choosecolorwidget.moc"
