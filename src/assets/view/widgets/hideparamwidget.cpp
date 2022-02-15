/*
    SPDX-FileCopyrightText: 2019 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "hideparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"

HideParamWidget::HideParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    setFixedSize(QSize(0, 0));
    setVisible(false);
}

void HideParamWidget::slotShowComment(bool show)
{
    Q_UNUSED(show);
}

void HideParamWidget::slotRefresh() {}
