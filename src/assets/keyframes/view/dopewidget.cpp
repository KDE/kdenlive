/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "dopewidget.hpp"

#include <QQmlContext>
#include <QQmlEngine>
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

    setInitialProperties({{"keyframeTypes", KeyframeModel::getKeyframeTypesVariant()},
                          {"keyframeTypeNames", KeyframeModel::getKeyframeTypesVariant().keys()},
                          {"dopesheetmodel", QVariant::fromValue(pCore->dopeSheetModel().get())}});
    loadFromModule(QStringLiteral("org.kde.kdenlive"), QStringLiteral("DopeSheetView"));
}

void DopeWidget::deleteItem()
{
    if (rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "deleteSelection");
    }
}

void DopeWidget::grabKeyframes()
{
    if (rootObject()) {
        pCore->dopeSheetModel()->setGrabbed(true);
        QMetaObject::invokeMethod(rootObject(), "updateGrabbedKeyframesFromModel");
    }
}

void DopeWidget::clearSelection()
{
    if (rootObject()) {
        pCore->dopeSheetModel()->setGrabbed(false);
        QMetaObject::invokeMethod(rootObject(), "clearGrabAndSelection");
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
    QMetaObject::invokeMethod(rootObject(), "updateOwner");
}

void DopeWidget::clear()
{
    pCore->dopeSheetModel()->clearModel();
}

void DopeWidget::setViewProperties(QVariantMap properties)
{
    for (QVariantMap::const_iterator iter = properties.begin(); iter != properties.end(); ++iter) {
        rootObject()->setProperty(iter.key().toLatin1().constData(), iter.value());
        QQmlEngine::setObjectOwnership(qvariant_cast<QObject *>(iter.value()), QQmlEngine::CppOwnership);
    }
}

void DopeWidget::moveGrab(bool left)
{
    if (!rootObject()) {
        return;
    }
    QMetaObject::invokeMethod(rootObject(), "moveGrab", Q_ARG(QVariant, left));
}

void DopeWidget::gotoPreviousSnap()
{
    if (!rootObject()) {
        return;
    }
    // Find active model
    QVariant returnedValue;
    QMetaObject::invokeMethod(rootObject(), "getActiveIndex", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    const QModelIndex activeIndex = returnedValue.toModelIndex();
    int pos = pCore->getMonitorPosition();
    pos = pCore->dopeSheetModel()->getPreviousSnap(activeIndex, pos);
    pCore->seekMonitor(Kdenlive::ProjectMonitor, pos);
}

void DopeWidget::gotoNextSnap()
{
    if (!rootObject()) {
        return;
    }
    // Find active model
    QVariant returnedValue;
    QMetaObject::invokeMethod(rootObject(), "getActiveIndex", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    const QModelIndex activeIndex = returnedValue.toModelIndex();
    int pos = pCore->getMonitorPosition();
    pos = pCore->dopeSheetModel()->getNextSnap(activeIndex, pos);
    pCore->seekMonitor(Kdenlive::ProjectMonitor, pos);
}
