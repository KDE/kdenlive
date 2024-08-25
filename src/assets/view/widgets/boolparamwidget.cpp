/*
    SPDX-FileCopyrightText: 2016 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "boolparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QString>

BoolParamWidget::BoolParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    QHBoxLayout *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_checkBox = new QCheckBox(this);
    lay->addWidget(m_checkBox);
    // set check state
    slotRefresh();

    // Q_EMIT the signal of the base class when appropriate
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(this->m_checkBox, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
        // To represent 'checked' status, Qt uses number '2', but
        // the boolean parameters in MLT effects use number '1'
        int mltState = int(state);
        if (state == Qt::CheckState::Checked) {
            mltState = 1;
        }
        Q_EMIT valueChanged(m_index, QString::number(mltState), true);
    });
#else
    connect(this->m_checkBox, &QCheckBox::stateChanged, this, [this](int state) {
        // To represent 'checked' status, Qt uses number '2', but
        // the boolean parameters in MLT effects use number '1'
        if (state == 2) {
            state = 1;
        }
        Q_EMIT valueChanged(m_index, QString::number(state), true);
    });
#endif
}

void BoolParamWidget::slotShowComment(bool) {}

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
