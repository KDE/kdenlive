/***************************************************************************
 *   Copyright (C) 2016 by Nicolas Carion                                  *
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


#include "boolparamwidget.h"


BoolParamWidget::BoolParamWidget(const QString& name, const QString& comment, bool checked, QWidget* parent)
    : AbstractParamWidget(parent)
{
    setupUi(this);

    //setup the comment
    setToolTip(comment);
    m_labelComment->setText(comment);
    m_widgetComment->setHidden(true);

    //setup the name
    m_labelName->setText(name);

    //set check state
    m_checkBox->setChecked(checked);

    //emit the signal of the base class when appropriate
    connect(this->m_checkBox, &QCheckBox::stateChanged,
            [this](int){ emit qobject_cast<AbstractParamWidget*>(this)->valueChanged();});
}

void BoolParamWidget::slotShowComment(bool show)
{
    if(!m_labelComment->text().isEmpty()){
        m_widgetComment->setVisible(show);
    }
}

bool BoolParamWidget::getValue()
{
    return m_checkBox->isChecked();
}
