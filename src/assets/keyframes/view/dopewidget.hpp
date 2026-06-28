/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QModelIndex>
#include <QQuickWidget>
#include <QWidget>

class AssetParameterModel;
class EffectStackModel;
class DopeFilter;

class DopeWidget : public QQuickWidget
{
    Q_OBJECT
public:
    DopeWidget(QQmlEngine *engine, QWidget *parent = nullptr);
    void setViewProperties(QVariantMap properties);
    void deleteItem();
    void doKeyPressEvent(QKeyEvent *ev);
    void clear();
    void grabKeyframes();
    void clearSelection();
    void moveGrab(bool left);
    void sendStandardCommand(int command);

protected:
    virtual void focusInEvent(QFocusEvent *event) override;
    virtual void focusOutEvent(QFocusEvent *event) override;

public Q_SLOTS:
    void registerDopeStack(std::shared_ptr<EffectStackModel> model);
    void registerDopeAsset(std::shared_ptr<AssetParameterModel> model, const QString assetName);
    void slotAddRemoveKeyframe();
    void activateEffect(QPersistentModelIndex ix);
    void gotoPreviousSnap();
    void gotoNextSnap();

private Q_SLOTS:
    void updateActiveEffect(QPersistentModelIndex ix, bool active);
    /** @brief The model was updated, ensure we enable/disable parameters depending on the keyframes positions */
    void checkModelUpdate();
    void slotUpdateFilter(QVariant text);
    void expandAll();

private:
    QModelIndex m_activeIndex;
    QMetaObject::Connection m_activeEffectConnection;
    std::unique_ptr<DopeFilter> m_proxyModel;
};
