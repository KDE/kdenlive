/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
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

#include "assetparameterview.hpp"

#include "assets/model/assetparametermodel.hpp"
#include "assets/view/widgets/abstractparamwidget.hpp"

#include <QDebug>
#include <QLabel>
#include <QVBoxLayout>

#include <utility>

AssetParameterView::AssetParameterView(QWidget *parent)
    : QWidget(parent)
{
    m_lay = new QVBoxLayout(this);
    m_lay->setContentsMargins(4, 0, 4, 0);
    m_lay->setSpacing(2);
}

void AssetParameterView::setModel(std::shared_ptr<AssetParameterModel> model)
{
    unsetModel();
    m_model = model;
    connect(m_model.get(), &AssetParameterModel::dataChanged, this, &AssetParameterView::refresh);


    for (int i = 0; i < model->rowCount(); ++i) {
        QModelIndex index = model->index(i, 0);
        auto w = AbstractParamWidget::construct(model, index, this);
        connect(w, &AbstractParamWidget::valueChanged, model.get(), &AssetParameterModel::commitChanges);
        m_lay->addWidget(w);
        m_widgets.push_back(w);
    }
    m_lay->addStretch();
}

void AssetParameterView::unsetModel()
{
    if (m_model) {
        // if a model is already there, we have to disconnect signals first
        disconnect(m_model.get(), &AssetParameterModel::dataChanged, this, &AssetParameterView::refresh);
    }

    // clear layout
    m_widgets.clear();
    QLayoutItem *child;
    while ((child = m_lay->takeAt(0)) != nullptr) {
        delete child;
    }

    // Release ownership of smart pointer
    m_model.reset();
}

void AssetParameterView::refresh(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    Q_UNUSED(roles);
    // We are expecting indexes that are children of the root index, which is "invalid"
    Q_ASSERT(!topLeft.parent().isValid());
    // We make sure the range is valid
    Q_ASSERT(bottomRight.row() < (int)m_widgets.size());

    for (size_t i = (size_t)topLeft.row(); i <= (size_t)bottomRight.row(); ++i) {
        m_widgets[i]->slotRefresh();
    }
}
