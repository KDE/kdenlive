/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "effectbuttonsparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"

#include <QHBoxLayout>
#include <QPushButton>

EffectButtonsParamWidget::EffectButtonsParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    QHBoxLayout *lay = new QHBoxLayout(this);
    QString buttonNames = m_model->data(m_index, AssetParameterModel::NameRole).toString();
    qDebug() << "====== \nFOUND PARAME NAMES: " << buttonNames;
    const QStringList comments = m_model->data(m_index, AssetParameterModel::CommentRole).toString().split(QLatin1Char(';'));
    const QStringList icons = m_model->data(m_index, AssetParameterModel::ListValuesRole).toStringList();
    // setToolTip(comment);
    QStringList bNames = buttonNames.split(QLatin1Char(';'));
    int ix = 0;
    for (auto &b : bNames) {
        QPushButton *flip = new QPushButton(this);
        if (comments.size() > ix) {
            flip->setToolTip(i18n(comments.at(ix).toUtf8().constData()));
        }
        if (icons.size() > ix) {
            flip->setIcon(QIcon::fromTheme(icons.at(ix)));
        }
        flip->setCheckable(true);
        flip->setFlat(true);
        flip->setProperty("filter", b);
        connect(flip, &QPushButton::clicked, this, &EffectButtonsParamWidget::buttonClicked);
        lay->addWidget(flip);
        ix++;
    }

    // set check state
    slotRefresh();
}

void EffectButtonsParamWidget::buttonClicked(bool checked)
{
    auto *button = qobject_cast<QPushButton *>(QObject::sender());
    if (button) {
        const QString filterName = button->property("filter").toString();
        if (checked) {
        }
    }
}

void EffectButtonsParamWidget::paramChanged(int) {}

void EffectButtonsParamWidget::slotShowComment(bool /*show*/) {}

void EffectButtonsParamWidget::slotRefresh() {}
