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

#include "fontparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"

FontParamWidget::FontParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    setupUi(this);

    // setup the comment
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    setToolTip(comment);

    // setup the name
    labelName->setText(m_model->data(m_index, Qt::DisplayRole).toString());

    // set check state
    slotRefresh();
    setMinimumHeight(fontfamilywidget->sizeHint().height());
    // emit the signal of the base class when appropriate
    connect(this->fontfamilywidget, &QFontComboBox::currentFontChanged, this, [this](const QFont &font) { emit valueChanged(m_index, font.family(), true); });
}

void FontParamWidget::slotShowComment(bool show)
{
    Q_UNUSED(show);
}

void FontParamWidget::slotRefresh()
{
    QSignalBlocker bk(fontfamilywidget);
    const QString family = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    fontfamilywidget->setCurrentFont(QFont(family));
}

