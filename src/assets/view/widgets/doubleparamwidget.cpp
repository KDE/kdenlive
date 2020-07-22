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

#include "doubleparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "widgets/doublewidget.h"
#include <QVBoxLayout>
#include <utility>

DoubleParamWidget::DoubleParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_doubleWidget(nullptr)
{
    m_lay = new QVBoxLayout(this);
    m_lay->setContentsMargins(0, 0, 0, 0);
    m_lay->setSpacing(0);

    // Retrieve parameters from the model
    QString name = m_model->data(m_index, Qt::DisplayRole).toString();
    double value = m_model->data(m_index, AssetParameterModel::ValueRole).toDouble();
    double min = m_model->data(m_index, AssetParameterModel::MinRole).toDouble();
    double max = m_model->data(m_index, AssetParameterModel::MaxRole).toDouble();
    double defaultValue = m_model->data(m_index, AssetParameterModel::DefaultRole).toDouble();
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    QString suffix = m_model->data(m_index, AssetParameterModel::SuffixRole).toString();
    int decimals = m_model->data(m_index, AssetParameterModel::DecimalsRole).toInt();
    double factor = m_model->data(m_index, AssetParameterModel::FactorRole).toDouble();
    // Construct object
    m_doubleWidget = new DoubleWidget(name, value, min, max, factor, defaultValue, comment, -1, suffix, decimals, m_model->data(m_index, AssetParameterModel::OddRole).toBool(), this);
    m_lay->addWidget(m_doubleWidget);
    setMinimumHeight(m_doubleWidget->height());

    // Connect signal
    connect(m_doubleWidget, &DoubleWidget::valueChanged, this, [this](double val) { emit valueChanged(m_index, QString::number(val, 'f'), true); });
    slotRefresh();
}

void DoubleParamWidget::slotRefresh()
{
    QSignalBlocker bk(m_doubleWidget);
    double value = m_model->data(m_index, AssetParameterModel::ValueRole).toDouble();
    m_doubleWidget->setValue(value);
}

void DoubleParamWidget::slotShowComment(bool show)
{
    m_doubleWidget->slotShowComment(show);
}
