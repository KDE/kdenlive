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


#include "listparamwidget.h"


ListParamWidget::ListParamWidget(const QString& name, const QString& comment, QWidget* parent)
    : AbstractParamWidget(parent)
{
    setupUi(this);

    //setup the comment
    setToolTip(comment);
    m_labelComment->setText(comment);
    m_widgetComment->setHidden(true);

    //setup the name
    m_labelName->setText(name);

    //emit the signal of the base class when appropriate
    //The connection is ugly because the signal "currentIndexChanged" is overloaded in QComboBox
    connect(this->m_list,
            static_cast<void(KComboBox::*)(int)>(&KComboBox::currentIndexChanged),
            [this](int){ emit qobject_cast<AbstractParamWidget*>(this)->valueChanged();});
}

void ListParamWidget::setCurrentIndex(int index)
{
    m_list->setCurrentIndex(index);
}

void ListParamWidget::setCurrentText(const QString& text)
{
    m_list->setCurrentText(text);
}

void ListParamWidget::addItem(const QString &text, const QVariant &value)
{
    m_list->addItem(text, value);
}

void ListParamWidget::setItemIcon(int index, const QIcon &icon)
{
    m_list->setItemIcon(index, icon);
}

void ListParamWidget::setIconSize(const QSize& size)
{
    m_list->setIconSize(size);
}

void ListParamWidget::slotShowComment(bool show)
{
    if(!m_labelComment->text().isEmpty()){
        m_widgetComment->setVisible(show);
    }
}

QString ListParamWidget::getValue()
{
    return m_list->itemData(m_list->currentIndex()).toString();
}
