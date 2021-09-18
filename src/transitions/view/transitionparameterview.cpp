/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <KLocalizedContext>

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
    engine()->rootContext()->setContextObject(new KLocalizedContext(this));

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
