/***************************************************************************
 *   Copyright (C) 2021 by Jean-Baptiste Mardelle                          *
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

#include "multiswitchparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"

MultiSwitchParamWidget::MultiSwitchParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
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
        QString value;
        if (state == Qt::Checked) {
            value = m_model->data(m_index, AssetParameterModel::MaxRole).toString();
            if (value.contains(QLatin1String("0="))) {
                value.replace(QLatin1String("0="), QLatin1String("00:00:00.000="));
            }
            if (value.contains(QLatin1String("-1="))) {
                // Replace -1 with out position
                int out = m_model->data(m_index, AssetParameterModel::OutRole).toInt() - m_model->data(m_index, AssetParameterModel::InRole).toInt();
                value.replace(QLatin1String("-1="),  QString("%1=").arg(m_model->framesToTime(out)));
            }
        } else {
            value = m_model->data(m_index, AssetParameterModel::MinRole).toString();
            if (value.contains(QLatin1String("0="))) {
                value.replace(QLatin1String("0="), QLatin1String("00:00:00.000="));
            }
            if (value.contains(QLatin1String("-1="))) {
                // Replace -1 with out position
                int out = m_model->data(m_index, AssetParameterModel::OutRole).toInt() - m_model->data(m_index, AssetParameterModel::InRole).toInt();
                value.replace(QLatin1String("-1="),  QString("%1=").arg(m_model->framesToTime(out)));
            }
        }
        emit valueChanged(m_index, value, true);
    });
}

void MultiSwitchParamWidget::slotShowComment(bool show)
{
    if (!m_labelComment->text().isEmpty()) {
        m_widgetComment->setVisible(show);
    }
}

void MultiSwitchParamWidget::slotRefresh()
{
    const QSignalBlocker bk(m_checkBox);
    QString max = m_model->data(m_index, AssetParameterModel::MaxRole).toString();
    const QString value = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    bool convertToTime = false;
    if (value.contains(QLatin1Char(':'))) {
        convertToTime = true;
    }
    if (max.contains(QLatin1String("0=")) && convertToTime) {
        max.replace(QLatin1String("0="), QLatin1String("00:00:00.000="));
    }
    if (max.contains(QLatin1String("-1=")) && !value.contains(QLatin1String("-1="))) {
        // Replace -1 with out position
        int out = m_model->data(m_index, AssetParameterModel::OutRole).toInt() - m_model->data(m_index, AssetParameterModel::InRole).toInt();
        qDebug()<<"=== REPLACING WITH MAX OUT: "<<out;
        if (convertToTime) {
            max.replace(QLatin1String("-1="), QString("%1=").arg(m_model->framesToTime(out)));
        } else {
            max.replace(QLatin1String("-1="), QString("%1=").arg(out));
        }
    }
    qDebug()<<"=== GOT FILTER IN ROLE: "<<m_model->data(m_index, AssetParameterModel::InRole).toInt()<<" / OUT: "<<m_model->data(m_index, AssetParameterModel::OutRole).toInt();
    qDebug()<<"==== COMPARING MULTISWITCH: "<<value<<" = "<<max;
    m_checkBox->setChecked(value == max);
}
