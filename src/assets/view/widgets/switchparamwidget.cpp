/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "switchparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"

SwitchParamWidget::SwitchParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    setupUi(this);

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
        emit valueChanged(m_index,
                          (state == Qt::Checked ? m_model->data(m_index, AssetParameterModel::MaxRole).toString()
                                                   : m_model->data(m_index, AssetParameterModel::MinRole).toString()),
                          true);
    });
}

void SwitchParamWidget::slotShowComment(bool show)
{
    if (!m_labelComment->text().isEmpty()) {
        m_widgetComment->setVisible(show);
    }
}

void SwitchParamWidget::slotRefresh()
{
    const QSignalBlocker bk(m_checkBox);
    QString max = m_model->data(m_index, AssetParameterModel::MaxRole).toString();
    QString val = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    if (val == max) {
        m_checkBox->setChecked(true);
    } else {
        QString min = m_model->data(m_index, AssetParameterModel::MinRole).toString();
        if (val == min) {
            m_checkBox->setChecked(false);
            return;
        }
        if (val.contains(QLatin1Char(';'))) {
            // Value is not equal to min/max, might be caused by a frame > timecode replacement
            max = max.section(QLatin1Char(';'), 0, 0);
            val = val.section(QLatin1Char(';'), 0, 0);
            if (val.endsWith(max.section(QLatin1Char(' '), -1))) {
                m_checkBox->setChecked(true);
            } else {
                m_checkBox->setChecked(false);
            }
        }
    }
}
