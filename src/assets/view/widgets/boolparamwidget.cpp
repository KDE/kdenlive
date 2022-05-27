/*
    SPDX-FileCopyrightText: 2016 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "boolparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"

BoolParamWidget::BoolParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    setupUi(this);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    // setup the comment
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    setToolTip(comment);
    m_labelComment->setText(comment);
    m_widgetComment->setHidden(true);

    // setup the name
    m_labelName->setText(m_model->data(m_index, Qt::DisplayRole).toString());
    setMinimumHeight(m_labelName->sizeHint().height());

    // set check state
    slotRefresh();

    // emit the signal of the base class when appropriate
    connect(this->m_checkBox, &QCheckBox::stateChanged, this, [this](int state) {
        // To represent 'checked' status, Qt uses number '2', but
        // the boolean parameters in MLT effects use number '1'
        if (state == 2) {
            state = 1;
        }
        emit valueChanged(m_index, QString::number(state), true);
    });
}

void BoolParamWidget::slotShowComment(bool show)
{
    if (!m_labelComment->text().isEmpty()) {
        m_widgetComment->setVisible(show);
    }
}

void BoolParamWidget::slotRefresh()
{
    QSignalBlocker bk(m_checkBox);
    bool checked = m_model->data(m_index, AssetParameterModel::ValueRole).toInt();
    m_checkBox->setChecked(checked);
}

bool BoolParamWidget::getValue()
{
    return m_checkBox->isChecked();
}
