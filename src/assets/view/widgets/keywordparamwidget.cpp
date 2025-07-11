/*
    SPDX-FileCopyrightText: 2016 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "keywordparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"

KeywordParamWidget::KeywordParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    setupUi(this);

    QStringList kwrdValues = m_model->data(m_index, AssetParameterModel::ListValuesRole).toStringList();
    QStringList kwrdNames = m_model->data(m_index, AssetParameterModel::ListNamesRole).toStringList();
    comboboxwidget->addItems(kwrdNames);
    int i = 0;
    for (const QString &keywordVal : std::as_const(kwrdValues)) {
        if (i >= comboboxwidget->count()) {
            break;
        }
        comboboxwidget->setItemData(i, keywordVal);
        i++;
    }
    comboboxwidget->insertItem(0, i18n("<Select a Keyword>"));
    comboboxwidget->setCurrentIndex(0);
    // set check state
    slotRefresh();
    setMinimumHeight(comboboxwidget->sizeHint().height());

    // Q_EMIT the signal of the base class when appropriate
    connect(lineeditwidget, &QLineEdit::editingFinished, this, [this]() { Q_EMIT valueChanged(m_index, lineeditwidget->text(), true); });
    connect(comboboxwidget, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int ix) {
        if (ix > 0) {
            QString comboval = comboboxwidget->itemData(ix).toString();
            this->lineeditwidget->insert(comboval);
            Q_EMIT valueChanged(m_index, lineeditwidget->text(), true);
            comboboxwidget->setCurrentIndex(0);
        }
    });
}

void KeywordParamWidget::slotShowComment(bool show)
{
    Q_UNUSED(show);
}

void KeywordParamWidget::slotRefresh()
{
    lineeditwidget->setText(m_model->data(m_index, AssetParameterModel::ValueRole).toString());
}
