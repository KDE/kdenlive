/*
    SPDX-FileCopyrightText: 2016 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "fontparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"

FontParamWidget::FontParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    setupUi(this);

    // setup the comment
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    setToolTip(comment);

    // set check state
    slotRefresh();
    setMinimumHeight(fontfamilywidget->sizeHint().height());
    // Q_EMIT the signal of the base class when appropriate
    connect(this->fontfamilywidget, &QFontComboBox::currentFontChanged, this, [this](const QFont &font) { Q_EMIT valueChanged(m_index, font.family(), true); });
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
