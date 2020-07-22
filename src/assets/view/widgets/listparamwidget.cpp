/***************************************************************************
 *   Copyright (C) 2016 by Nicolas Carion                                  *
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

#include "listparamwidget.h"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "mainwindow.h"

ListParamWidget::ListParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    setupUi(this);

    // Get data from model
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();

    // setup the comment
    setToolTip(comment);
    m_labelComment->setText(comment);
    m_widgetComment->setHidden(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_list->setIconSize(QSize(50, 30));
    setMinimumHeight(m_list->sizeHint().height());
    // setup the name
    m_labelName->setText(m_model->data(m_index, Qt::DisplayRole).toString());
    slotRefresh();

    // emit the signal of the base class when appropriate
    // The connection is ugly because the signal "currentIndexChanged" is overloaded in QComboBox
    connect(this->m_list, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, [this](int) {
                emit valueChanged(m_index, m_list->itemData(m_list->currentIndex()).toString(), true);
    });
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

void ListParamWidget::slotShowComment(bool show)
{
    if (!m_labelComment->text().isEmpty()) {
        m_widgetComment->setVisible(show);
    }
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
        if (pCore->getCurrentFrameSize().width() > 1000) {
            // HD project
            values = MainWindow::m_lumaFiles.value(QStringLiteral("16_9"));
        } else if (pCore->getCurrentFrameSize().height() > 1000) {
            values = MainWindow::m_lumaFiles.value(QStringLiteral("9_16"));
        } else if (pCore->getCurrentFrameSize().height() == pCore->getCurrentFrameSize().width()) {
            values = MainWindow::m_lumaFiles.value(QStringLiteral("square"));
        } else if (pCore->getCurrentFrameSize().height() == 480) {
            values = MainWindow::m_lumaFiles.value(QStringLiteral("NTSC"));
        } else {
            values = MainWindow::m_lumaFiles.value(QStringLiteral("PAL"));
        }
        m_list->addItem(i18n("None (Dissolve)"));
        for (int j = 0; j < values.count(); ++j) {
            const QString &entry = values.at(j);
            m_list->addItem(values.at(j).section(QLatin1Char('/'), -1), entry);
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
            if (ix > -1)  {
                m_list->setCurrentIndex(ix);
            }
        }
    }
}
