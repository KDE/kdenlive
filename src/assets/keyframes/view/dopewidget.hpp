/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QModelIndex>
#include <QQuickWidget>
#include <QWidget>

class EffectStackModel;

class DopeWidget : public QQuickWidget
{
    Q_OBJECT
public:
    DopeWidget(QWidget *parent = nullptr);
    void setViewProperties(QVariantMap properties);
    void deleteItem();
    void doKeyPressEvent(QKeyEvent *ev);
    void clear();
    void grabKeyframes();
    void clearSelection();
    void moveGrab(bool left);
    void gotoPreviousSnap();
    void gotoNextSnap();

protected:
    virtual void focusInEvent(QFocusEvent *event) override;
    virtual void focusOutEvent(QFocusEvent *event) override;

public Q_SLOTS:
    void registerDopeStack(std::shared_ptr<EffectStackModel> model);
    void slotAddRemoveKeyframe();
    void activateEffect(QPersistentModelIndex ix);

private Q_SLOTS:
    void updateActiveEffect(QPersistentModelIndex ix, bool active);
    /** @brief The model was updated, ensure we enable/disable parameters depending on the keyframes positions */
    void checkModelUpdate();

private:
    QModelIndex m_activeIndex;
};
