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
    comboboxwidget->insertItem(0, i18n("Insert a Keyword…"));
    comboboxwidget->setCurrentIndex(0);

    label->setText(m_model->data(m_index, Qt::DisplayRole).toString());
    // set check state
    slotRefresh();

    // Q_EMIT the signal of the base class when appropriate
    connect(lineeditwidget, &QPlainTextEdit::textChanged, this, [this]() { Q_EMIT valueChanged(m_index, lineeditwidget->toPlainText(), true); });
    connect(comboboxwidget, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int ix) {
        if (ix > 0) {
            QString comboval = comboboxwidget->itemData(ix).toString();
            this->lineeditwidget->insertPlainText(comboval);
            Q_EMIT valueChanged(m_index, lineeditwidget->toPlainText(), true);
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
    lineeditwidget->setPlainText(m_model->data(m_index, AssetParameterModel::ValueRole).toString());
}
