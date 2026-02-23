/*
    SPDX-FileCopyrightText: 2016 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "listparamwidget.h"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "mainwindow.h"

#include <QComboBox>
#include <QHBoxLayout>

ListParamWidget::ListParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    QHBoxLayout *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    m_list = new QComboBox(this);
    lay->addWidget(m_list);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_list->setIconSize(QSize(50, 30));
    setMinimumHeight(m_list->sizeHint().height());
    // setup the name
    slotRefresh();

    // Q_EMIT the signal of the base class when appropriate
    // The connection is ugly because the signal "currentIndexChanged" is overloaded in QComboBox
    connect(this->m_list, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [this](int) { Q_EMIT valueChanged(m_index, m_list->itemData(m_list->currentIndex()).toString(), true); });
}

void ListParamWidget::setCurrentIndex(int index)
{
    m_list->setCurrentIndex(index);
}

void ListParamWidget::setCurrentText(const QString &text)
{
    m_list->setCurrentText(text);
}

void ListParamWidget::addItem(const QString &text, const QVariant &value)
{
    m_list->addItem(text, value);
}

void ListParamWidget::setItemIcon(int index, const QIcon &icon)
{
    m_list->setItemIcon(index, icon);
}

void ListParamWidget::setIconSize(const QSize &size)
{
    m_list->setIconSize(size);
}

void ListParamWidget::slotShowComment(bool /*show*/)
{
    /*if (!m_labelComment->text().isEmpty()) {
        m_widgetComment->setVisible(show);
    }*/
}

QString ListParamWidget::getValue()
{
    return m_list->currentData().toString();
}

void ListParamWidget::slotRefresh()
{
    const QSignalBlocker bk(m_list);
    m_list->clear();
    QStringList names = m_model->data(m_index, AssetParameterModel::ListNamesRole).toStringList();
    QStringList values = m_model->data(m_index, AssetParameterModel::ListValuesRole).toStringList();
    QString value = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    if (values.first() == QLatin1String("%lumaPaths")) {
        // Special case: Luma files
        // Create thumbnails
        values = pCore->getLumasForProfile();
        m_list->addItem(i18n("None (Dissolve)"));
        for (int j = 0; j < values.count(); ++j) {
            const QString &entry = values.at(j);
            const QString name = values.at(j).section(QLatin1Char('/'), -1);
            m_list->addItem(pCore->nameForLumaFile(name), entry);
            if (!entry.isEmpty() && (entry.endsWith(QLatin1String(".png")) || entry.endsWith(QLatin1String(".pgm")))) {
                if (MainWindow::m_lumacache.contains(entry)) {
                    m_list->setItemIcon(j + 1, QPixmap::fromImage(MainWindow::m_lumacache.value(entry)));
                }
            }
        }
        if (!value.isEmpty() && values.contains(value)) {
            m_list->setCurrentIndex(values.indexOf(value) + 1);
        }
    } else {
        if (names.count() != values.count()) {
            names = values;
        }
        for (int i = 0; i < names.count(); i++) {
            m_list->addItem(names.at(i), values.at(i));
        }
        if (!value.isEmpty()) {
            int ix = m_list->findData(value);
            if (ix > -1) {
                m_list->setCurrentIndex(ix);
            }
        }
    }
}
