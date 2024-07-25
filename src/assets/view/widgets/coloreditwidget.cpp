/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "coloreditwidget.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "utils/qcolorutils.h"
#include "widgets/choosecolorwidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QTextStream>

ColorEditWidget::ColorEditWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    // setup the comment
    bool alphaEnabled = m_model->data(m_index, AssetParameterModel::AlphaRole).toBool();
    QString color = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();

    m_choosecolor = new ChooseColorWidget(this, QColorUtils::stringToColor(color), alphaEnabled);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_choosecolor, 1);

    // Q_EMIT the signal of the base class when appropriate
    connect(m_choosecolor, &ChooseColorWidget::modified, [this](const QColor &) { Q_EMIT valueChanged(m_index, getColor(), true); });
    connect(m_choosecolor, &ChooseColorWidget::disableCurrentFilter, this, &AbstractParamWidget::disableCurrentFilter);
    // setup comment
    setToolTip(comment);
}

void ColorEditWidget::slotShowComment(bool) {}

void ColorEditWidget::slotRefresh()
{
    QSignalBlocker bk(this);
    QString color = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    m_choosecolor->setColor(QColorUtils::stringToColor(color));
}

QString ColorEditWidget::getColor() const
{
    return QColorUtils::colorToString(m_choosecolor->color(), m_choosecolor->isAlphaChannelEnabled());
}

void ColorEditWidget::setColor(const QColor &color)
{
    m_choosecolor->setColor(color);
}
