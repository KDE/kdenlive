/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "dopewidget.hpp"

#include <QQmlContext>
#include <QQuickItem>
#include <QVariant>
#include <QtGlobal>

#include <ki18n_version.h>

#if KI18N_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <KLocalizedQmlContext>
#else
#include <KLocalizedContext>
#endif

#include "assets/keyframes/model/dopesheetmodel.hpp"
#include "core.h"
#include "effects/effectstack/model/effectstackmodel.hpp"

DopeWidget::DopeWidget(QWidget *parent)
    : QQuickWidget(parent)
{
#if KI18N_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    KLocalization::setupLocalizedContext(engine());
#else
    engine()->rootContext()->setContextObject(new KLocalizedContext(this));
#endif
    setClearColor(palette().base().color());
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    QList<QQmlContext::PropertyPair> propertyList = {{"miniFontSize", QVariant::fromValue(QFontInfo(font()).pixelSize())},
                                                     {"keyframeTypes", KeyframeModel::getKeyframeTypesVariant()},
                                                     {"keyframeTypeNames", KeyframeModel::getKeyframeTypesVariant().keys()}};
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
