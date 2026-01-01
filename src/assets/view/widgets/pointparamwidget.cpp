/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "pointparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "widgets/pointwidget.h"

#include <QHBoxLayout>
#include <QSpinBox>

PointParamWidget::PointParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    QHBoxLayout *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    // Retrieve parameters from the model. Points are stored in MLT as rectangle, using only the first 2 coordinates. So for example a point (50, 100) might be
    // represented as this string: "50 100 0 0".
    QPointF value;
    QPointF min;
    QPointF max;
    QPointF defaultValue;
    QPointF factor;
    auto paramType = m_model->data(m_index, AssetParameterModel::TypeRole).value<ParamType>();
    if (paramType == ParamType::FakePoint || paramType == ParamType::AnimatedFakePoint) {
        AssetPointInfo paramInfo = m_model->data(m_index, AssetParameterModel::FakePointRole).value<AssetPointInfo>();
        // TODO
    } else {
        value = dataToPoint(m_model->data(m_index, AssetParameterModel::ValueRole));
        min = dataToPoint(m_model->data(m_index, AssetParameterModel::MinRole));
        max = dataToPoint(m_model->data(m_index, AssetParameterModel::MaxRole));
        defaultValue = dataToPoint(m_model->data(m_index, AssetParameterModel::DefaultRole));
        factor = dataToPoint(m_model->data(m_index, AssetParameterModel::FactorRole), QPointF(1, 1));
    }
    int decimals = m_model->data(m_index, AssetParameterModel::DecimalsRole).toInt();
    const QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    m_pointWidget =
        new PointWidget(i18nc("Position as the x,y coordinate for a point", "Position:"), value, min, max, factor, defaultValue, decimals, comment, -1, this);
    lay->addWidget(m_pointWidget);

    // Q_EMIT the signal of the base class when appropriate
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(m_pointWidget, &PointWidget::valueChanged, this, [this](QString point, bool createUndo) {
#else
    connect(this->m_checkBox, &QCheckBox::stateChanged, this, [this](QString point, bool createUndo) {
#endif
        Q_EMIT valueChanged(m_index, point, createUndo);
    });
}

QPointF PointParamWidget::dataToPoint(QVariant data, QPointF defaultVal)
{
    const QStringList params = data.toString().split(QLatin1Char(' '));
    if (params.size() > 1) {
        return QPointF(params.at(0).toDouble(), params.at(1).toDouble());
    }
    return defaultVal;
}

void PointParamWidget::slotRefresh()
{
    const QSignalBlocker bk(m_pointWidget);
    QPointF value = dataToPoint(m_model->data(m_index, AssetParameterModel::ValueRole));
    m_pointWidget->setValue(value);
}

void PointParamWidget::setValue(QPointF val)
{
    m_pointWidget->setValue(val);
}
