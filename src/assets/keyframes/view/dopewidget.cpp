/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "dopewidget.hpp"
#include "assets/keyframes/model/dopesheetmodel.hpp"
#include "core.h"
#include "dopefilter.hpp"
#include "effects/effectstack/model/effectstackmodel.hpp"
#include "monitor/monitor.h"
#include "monitor/monitorproxy.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QVariant>
#include <QtGlobal>

#include <KStandardAction>

DopeWidget::DopeWidget(QQmlEngine *engine, QWidget *parent)
    : QQuickWidget(engine, parent)
{
    setClearColor(palette().base().color());
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_proxyModel.reset(new DopeFilter(this));
    m_proxyModel->setSourceModel(pCore->dopeSheetModel().get());
    setInitialProperties({{"keyframeTypes", KeyframeModel::getKeyframeTypesVariant()},
                          {"dopesheetmodel", QVariant::fromValue(pCore->dopeSheetModel().get())},
                          {"dopesheetFilterModel", QVariant::fromValue(m_proxyModel.get())}});
    loadFromModule(QStringLiteral("org.kde.kdenlive"), QStringLiteral("DopeSheetView"));
    connect(pCore->dopeSheetModel().get(), &DopeSheetModel::activateEffect, this, &DopeWidget::activateEffect);
    connect(pCore->dopeSheetModel().get(), &DopeSheetModel::modelChanged, this, &DopeWidget::checkModelUpdate, Qt::QueuedConnection);
    connect(rootObject(), SIGNAL(filterDopeView(QVariant)), this, SLOT(slotUpdateFilter(QVariant)));
    connect(m_proxyModel.get(), &DopeFilter::expandAll, this, &DopeWidget::expandAll);
    connect(pCore->dopeSheetModel().get(), &DopeSheetModel::updateFiltering, m_proxyModel.get(), &DopeFilter::refreshFilter);
    connect(pCore->dopeSheetModel().get(), &DopeSheetModel::modelChanged, m_proxyModel.get(), &DopeFilter::refreshFilter);
}

void DopeWidget::slotUpdateFilter(QVariant text)
{
    m_proxyModel->setFilterName(text.toString());
}

void DopeWidget::deleteItem()
{
    if (rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "deleteSelection");
    }
}

void DopeWidget::expandAll()
{
    if (rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "expandAll");
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

void DopeWidget::registerDopeAsset(std::shared_ptr<AssetParameterModel> model, const QString assetName)
{
    if (!pCore->dopeSheetModel()->registerComposition(model, assetName)) {
        // model is already active
        return;
    }
    QObject::disconnect(m_activeEffectConnection);
    if (!model || !rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "updateOwner", Qt::DirectConnection, Q_ARG(QVariant, -1), Q_ARG(QVariant, -1));
        return;
    }
    QVariant monitorProxy = QVariant::fromValue(pCore->getMonitor(Kdenlive::ProjectMonitor)->getControllerProxy());
    rootObject()->setProperty("proxy", monitorProxy);
    QQmlEngine::setObjectOwnership(qvariant_cast<QObject *>(monitorProxy), QQmlEngine::CppOwnership);
    // Check if we are on a keyframe
    int pos = pCore->getMonitorPosition(pCore->dopeSheetModel()->getMonitorId()) - pCore->getItemPosition(model->getOwnerId());
    QVariant returnedValue;
    QMetaObject::invokeMethod(rootObject(), "getActiveCppParamIndex", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    const QPersistentModelIndex activeIndex = returnedValue.toModelIndex();
    if (activeIndex.isValid()) {
        pCore->dopeSheetModel()->isOnKeyframe(pos, true, activeIndex);
    }
    QMetaObject::invokeMethod(rootObject(), "updateOwner", Qt::DirectConnection, Q_ARG(QVariant, int(model->getOwnerId().type)),
                              Q_ARG(QVariant, model->getOwnerId().itemId));
}

void DopeWidget::registerDopeStack(std::shared_ptr<EffectStackModel> model)
{
    if (!pCore->dopeSheetModel()->registerStack(model)) {
        // model is already active
        return;
    }
    QObject::disconnect(m_activeEffectConnection);
    if (!model || !rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "updateOwner", Qt::DirectConnection, Q_ARG(QVariant, -1), Q_ARG(QVariant, -1));
        return;
    }
    QVariant monitorProxy = QVariant::fromValue(
        pCore->getMonitor(model->getOwnerId().type == KdenliveObjectType::BinClip ? Kdenlive::ClipMonitor : Kdenlive::ProjectMonitor)->getControllerProxy());
    rootObject()->setProperty("proxy", monitorProxy);
    QQmlEngine::setObjectOwnership(qvariant_cast<QObject *>(monitorProxy), QQmlEngine::CppOwnership);
    m_activeEffectConnection = connect(model.get(), &EffectStackModel::currentChanged, this, &DopeWidget::updateActiveEffect, Qt::QueuedConnection);
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
    QMetaObject::invokeMethod(rootObject(), "getActiveCppParamIndex", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    const QModelIndex activeIndex = returnedValue.toModelIndex();
    if (!activeIndex.isValid()) {
        return;
    }
    int pos = pCore->getMonitorPosition(pCore->dopeSheetModel()->getMonitorId());
    pos = pCore->dopeSheetModel()->getPreviousSnap(activeIndex, pos);
    pCore->seekMonitor(pCore->dopeSheetModel()->getMonitorId(), pos);
}

void DopeWidget::gotoNextSnap()
{
    if (!rootObject()) {
        return;
    }
    // Find active model
    QVariant returnedValue;
    QMetaObject::invokeMethod(rootObject(), "getActiveCppParamIndex", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    const QPersistentModelIndex activeIndex = returnedValue.toModelIndex();
    if (!activeIndex.isValid()) {
        return;
    }
    int pos = pCore->getMonitorPosition(pCore->dopeSheetModel()->getMonitorId());
    pos = pCore->dopeSheetModel()->getNextSnap(activeIndex, pos);
    pCore->seekMonitor(pCore->dopeSheetModel()->getMonitorId(), pos);
}

void DopeWidget::slotAddRemoveKeyframe()
{
    if (!rootObject()) {
        return;
    }
    // Find active model
    QVariant returnedValue;
    QMetaObject::invokeMethod(rootObject(), "getActiveCppParamIndex", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    const QModelIndex activeIndex = returnedValue.toModelIndex();
    if (!activeIndex.isValid()) {
        pCore->displayMessage(i18n("Select a parameter to add a keyframe"), ErrorMessage);
        return;
    }
    int pos = pCore->getMonitorPosition(pCore->dopeSheetModel()->getMonitorId()) - pCore->dopeSheetModel()->dopePosition();
    pCore->dopeSheetModel()->addRemoveKeyframe(activeIndex, pos);
}

void DopeWidget::activateEffect(QPersistentModelIndex ix)
{
    const QModelIndex dopeRow = pCore->dopeSheetModel()->getRowFromEffectIndex(ix);
    if (dopeRow.isValid()) {
        const QModelIndex mapped = m_proxyModel->mapFromSource(dopeRow);
        if (mapped.row() >= 0) {
            QMetaObject::invokeMethod(rootObject(), "setActiveIndexFromModel", Qt::QueuedConnection, Q_ARG(QVariant, QVariant(mapped.row())));
        } else {
            qDebug() << "==== ACTIVATING INVALID EFFECT ROW: " << mapped.row() << " / FROM: " << ix;
        }
    }
}

void DopeWidget::updateActiveEffect(QPersistentModelIndex ix, bool active)
{
    if (active) {
        activateEffect(ix);
    }
}

void DopeWidget::checkModelUpdate()
{
    // Check if we are on a keyframe
    int pos = pCore->getMonitorPosition(pCore->dopeSheetModel()->getMonitorId()) - pCore->dopeSheetModel()->dopePosition();
    QVariant returnedValue;
    QMetaObject::invokeMethod(rootObject(), "getActiveCppParamIndex", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    const QPersistentModelIndex activeIndex = returnedValue.toModelIndex();
    if (!activeIndex.isValid()) {
        return;
    }
    bool onKeyframe = pCore->dopeSheetModel()->isOnKeyframe(pos, false, activeIndex);
    QMetaObject::invokeMethod(rootObject(), "updateOverKeyframeFromModel", Qt::QueuedConnection, Q_ARG(QVariant, QVariant(onKeyframe)));
}

void DopeWidget::sendStandardCommand(int command)
{
    switch (command) {
    case KStandardAction::Copy:
        QMetaObject::invokeMethod(rootObject(), "copyKeyframes", Qt::QueuedConnection);
        break;
    case KStandardAction::Paste:
        QMetaObject::invokeMethod(rootObject(), "pasteKeyframes", Qt::QueuedConnection);
        break;
    default:
        qDebug() << ":::: UNKNOWN COMMAND: " << command;
        break;
    }
}
