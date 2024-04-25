/*
    SPDX-FileCopyrightText: 2016 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

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
    m_doubleWidget = new DoubleWidget(name, value, min, max, factor, defaultValue, comment, -1, suffix, decimals,
                                      m_model->data(m_index, AssetParameterModel::OddRole).toBool(), this);
    m_lay->addWidget(m_doubleWidget);
    setMinimumHeight(m_doubleWidget->height());

    // Connect signal
    connect(m_doubleWidget, &DoubleWidget::valueChanged, this,
            [this](double val, bool createUndoEntry) { Q_EMIT valueChanged(m_index, QString::number(val, 'f'), createUndoEntry); });
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
