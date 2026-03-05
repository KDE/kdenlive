/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "dopewidget.hpp"
#include "assets/keyframes/model/dopesheetmodel.hpp"
#include "core.h"
#include "effects/effectstack/model/effectstackmodel.hpp"

#include <QQmlContext>
#include <QQuickItem>

DopeWidget::DopeWidget(QWidget *parent)
    : QQuickWidget(parent)
{
    setClearColor(palette().base().color());
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    QList<QQmlContext::PropertyPair> propertyList = {{"miniFontSize", QVariant::fromValue(QFontInfo(font()).pixelSize())}};
    rootContext()->setContextProperties(propertyList);
    setSource(QUrl(QStringLiteral("qrc:/qt/qml/org/kde/kdenlive/DopeSheetView.qml")));
}

void DopeWidget::deleteItem()
{
    if (rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "deleteSelection");
    }
}

void DopeWidget::doKeyPressEvent(QKeyEvent *ev)
{
    keyPressEvent(ev);
}

void DopeWidget::registerDopeStack(std::shared_ptr<EffectStackModel> model)
{
    pCore->dopeSheetModel()->registerStack(model);
    if (!model || !rootObject()) {
        return;
    }
    QMetaObject::invokeMethod(rootObject(), "updateOwner", Qt::QueuedConnection, Q_ARG(QVariant, pCore->getItemDuration(model->getOwnerId())),
                              Q_ARG(QVariant, pCore->getItemPosition(model->getOwnerId())));
    rootObject()->setProperty("frameDuration", pCore->getItemDuration(model->getOwnerId()));
    rootObject()->setProperty("offset", pCore->getItemPosition(model->getOwnerId()));
}
