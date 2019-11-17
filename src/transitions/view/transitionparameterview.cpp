/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
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

#include "transitionparameterview.hpp"
#include "transitions/transitionsrepository.hpp"

#include <KDeclarative/KDeclarative>
#include <QQmlContext>
#include <kdeclarative_version.h>

#include <QStringListModel>

TransitionParameterView::TransitionParameterView(QWidget *parent)
    : QQuickWidget(parent)
{
    KDeclarative::KDeclarative kdeclarative;
    kdeclarative.setDeclarativeEngine(engine());
    kdeclarative.setupEngine(engine());
    kdeclarative.setupContext();

    // Set void model for the moment
    auto *model = new QStringListModel();
    QStringList list;
    list << "a"
         << "b"
         << "c"
         << "s"
         << "w";
    model->setStringList(list);
    rootContext()->setContextProperty("paramModel", model);
    rootContext()->setContextProperty("transitionName", "");

    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setSource(QUrl(QStringLiteral("qrc:/qml/transitionView.qml")));
    setFocusPolicy(Qt::StrongFocus);
}

void TransitionParameterView::setModel(const std::shared_ptr<AssetParameterModel> &model)
{
    m_model = model;
    rootContext()->setContextProperty("paramModel", model.get());
    QString transitionId = model->getAssetId();
    QString name = TransitionsRepository::get()->getName(transitionId);
    rootContext()->setContextProperty("transitionName", name);
}
