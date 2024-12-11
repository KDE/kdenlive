/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "ui_maskmanage_ui.h"

#include <QMutex>
#include <QReadWriteLock>
#include <QTimer>
#include <QWidget>
#include <memory>

class QVBoxLayout;
class EffectStackModel;
class EffectItemModel;
class AssetIconProvider;
class BuiltStack;
class EffectStackFilter;
class AssetPanel;
class QPushButton;
class AutomaskHelper;

class MaskManager : public QWidget, public Ui::MaskManage_UI
{
    Q_OBJECT

public:
    MaskManager(QWidget *parent);
    virtual ~MaskManager() override;
    void setOwner(ObjectId owner);

private Q_SLOTS:
    void initMaskMode();

private:
    ObjectId m_owner{KdenliveObjectType::NoItem, {}};
    AutomaskHelper *m_maskHelper;
    bool m_connected{false};
};
