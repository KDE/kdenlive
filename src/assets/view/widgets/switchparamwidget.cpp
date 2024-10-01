/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "switchparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"

#include <QCheckBox>
#include <QHBoxLayout>

SwitchParamWidget::SwitchParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    // setup the comment
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    setToolTip(comment);
    QHBoxLayout *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_checkBox = new QCheckBox(this);
    lay->addWidget(m_checkBox);

    // set check state
    slotRefresh();

    // Q_EMIT the signal of the base class when appropriate
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(this->m_checkBox, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
#else
    connect(this->m_checkBox, &QCheckBox::stateChanged, this, [this](int state) {
#endif
        Q_EMIT valueChanged(m_index,
                            (state == Qt::Checked ? m_model->data(m_index, AssetParameterModel::MaxRole).toString()
                                                  : m_model->data(m_index, AssetParameterModel::MinRole).toString()),
                            true);
    });
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
