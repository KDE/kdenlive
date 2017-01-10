/***************************************************************************
 *   Copyright (C) 2016 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
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
