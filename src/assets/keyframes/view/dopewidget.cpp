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

#include "assets/keyframes/model/dopesheetmodel.hpp"
#include "core.h"
#include "effects/effectstack/model/effectstackmodel.hpp"

DopeWidget::DopeWidget(QQmlEngine *engine, QWidget *parent)
    : QQuickWidget(engine, parent)
{
    setClearColor(palette().base().color());
    setResizeMode(QQuickWidget::SizeRootObjectToView);

    setInitialProperties({{"keyframeTypes", KeyframeModel::getKeyframeTypesVariant()},
                          {"keyframeTypeNames", KeyframeModel::getKeyframeTypesVariant().keys()},
                          {"dopesheetmodel", QVariant::fromValue(pCore->dopeSheetModel().get())}});
    loadFromModule(QStringLiteral("org.kde.kdenlive"), QStringLiteral("DopeSheetView"));
    connect(pCore->dopeSheetModel().get(), &DopeSheetModel::activateEffect, this, &DopeWidget::activateEffect);
    connect(pCore->dopeSheetModel().get(), &DopeSheetModel::modelChanged, this, &DopeWidget::checkModelUpdate, Qt::QueuedConnection);
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

void DopeWidget::focusInEvent(QFocusEvent *event)
{
    QMetaObject::invokeMethod(rootObject(), "switchFocus", Q_ARG(QVariant, true));
    QQuickWidget::focusInEvent(event);
}

void DopeWidget::focusOutEvent(QFocusEvent *event)
{
    QMetaObject::invokeMethod(rootObject(), "switchFocus", Q_ARG(QVariant, false));
    QQuickWidget::focusOutEvent(event);
}

void DopeWidget::registerDopeStack(std::shared_ptr<EffectStackModel> model)
{
    if (!pCore->dopeSheetModel()->registerStack(model)) {
        // model is already active
        return;
    }
    if (!model || !rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "updateOwner", Qt::DirectConnection, Q_ARG(QVariant, -1), Q_ARG(QVariant, -1));
        return;
    }
    connect(model.get(), &EffectStackModel::currentChanged, this, &DopeWidget::updateActiveEffect, Qt::QueuedConnection);
    // Check if we are on a keyframe
    int pos = pCore->getMonitorPosition() - pCore->getItemPosition(model->getOwnerId());
    QVariant returnedValue;
    QMetaObject::invokeMethod(rootObject(), "getActiveIndex", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    const QPersistentModelIndex activeIndex = returnedValue.toModelIndex();
    pCore->dopeSheetModel()->isOnKeyframe(pos, true, activeIndex);
    QMetaObject::invokeMethod(rootObject(), "updateOwner", Qt::DirectConnection, Q_ARG(QVariant, int(model->getOwnerId().type)),
                              Q_ARG(QVariant, model->getOwnerId().itemId));
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
    const QPersistentModelIndex activeIndex = returnedValue.toModelIndex();
    int pos = pCore->getMonitorPosition();
    pos = pCore->dopeSheetModel()->getNextSnap(activeIndex, pos);
    pCore->seekMonitor(Kdenlive::ProjectMonitor, pos);
}

void DopeWidget::slotAddRemoveKeyframe()
{
    if (!rootObject()) {
        return;
    }
    // Find active model
    QVariant returnedValue;
    QMetaObject::invokeMethod(rootObject(), "getActiveIndex", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    const QModelIndex activeIndex = returnedValue.toModelIndex();
    if (!activeIndex.isValid()) {
        pCore->displayMessage(i18n("Select a parameter to add a keyframe"), ErrorMessage);
        return;
    }
    int pos = pCore->getMonitorPosition() - pCore->dopeSheetModel()->dopePosition();
    pCore->dopeSheetModel()->addRemoveKeyframe(activeIndex, pos);
}

void DopeWidget::activateEffect(QPersistentModelIndex ix)
{
    QMetaObject::invokeMethod(rootObject(), "setActiveIndexFromModel", Qt::QueuedConnection, Q_ARG(QVariant, QVariant(ix)));
}

void DopeWidget::updateActiveEffect(QPersistentModelIndex ix, bool active)
{
    if (active) {
        QMetaObject::invokeMethod(rootObject(), "setActiveIndexFromModel", Qt::QueuedConnection, Q_ARG(QVariant, QVariant(ix)));
    }
}

void DopeWidget::checkModelUpdate()
{
    // Check if we are on a keyframe
    int pos = pCore->getMonitorPosition() - pCore->dopeSheetModel()->dopePosition();
    QVariant returnedValue;
    QMetaObject::invokeMethod(rootObject(), "getActiveIndex", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    const QPersistentModelIndex activeIndex = returnedValue.toModelIndex();
    bool onKeyframe = pCore->dopeSheetModel()->isOnKeyframe(pos, false, activeIndex);
    QMetaObject::invokeMethod(rootObject(), "updateOverKeyframeFromModel", Qt::QueuedConnection, Q_ARG(QVariant, QVariant(onKeyframe)));
}
