/***************************************************************************
 *   Copyright (C) 2018 by Jean-Baptiste Mardelle                          *
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

#include "urlparamwidget.hpp"
#include "assets/model/assetparametermodel.hpp"

#include <KUrlRequester>

UrlParamWidget::UrlParamWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    setupUi(this);

    // setup the comment
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();
    labelComment->setText(comment);
    setToolTip(comment);
    labelComment->setHidden(true);
    QString filter = m_model->data(m_index, AssetParameterModel::FilterRole).toString();
    if (!filter.isEmpty()) {
        urlwidget->setFilter(filter);
    }
    slotRefresh();

    // setup the name
    label->setText(m_model->data(m_index, Qt::DisplayRole).toString());
    setMinimumHeight(urlwidget->sizeHint().height());

    // set check state
    slotRefresh();

    // emit the signal of the base class when appropriate
    connect(this->urlwidget, &KUrlRequester::textChanged, this, [this]() {
        QFileInfo info(urlwidget->url().toLocalFile());
        if (info.exists() && info.isFile()) {
            emit valueChanged(m_index, this->urlwidget->url().toLocalFile(), true);
        }
    });
}

void UrlParamWidget::slotShowComment(bool show)
{
    if (!labelComment->text().isEmpty()) {
        labelComment->setVisible(show);
    }
}

void UrlParamWidget::slotRefresh()
{
    const QSignalBlocker bk(urlwidget);
    urlwidget->setUrl(QUrl::fromLocalFile(m_model->data(m_index, AssetParameterModel::ValueRole).toString()));
}
