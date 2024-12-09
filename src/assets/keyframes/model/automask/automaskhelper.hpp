/*
SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "assets/keyframes/model/keyframemonitorhelper.hpp"

#include <QObject>
#include <QPersistentModelIndex>
#include <QVariant>

#include <memory>

class Monitor;
class AssetParameterModel;

class AutomaskHelper : public KeyframeMonitorHelper
{
    Q_OBJECT

public:
    /** @brief Construct a keyframe list bound to the given effect
       @param init_value is the value taken by the param at time 0.
       @param model is the asset this parameter belong to
       @param index is the index of this parameter in its model
     */
    explicit AutomaskHelper(Monitor *monitor, std::shared_ptr<AssetParameterModel> model, QPersistentModelIndex index, QObject *parent = nullptr);
    /** @brief Send data update to the monitor
     */
    void refreshParams(int pos) override;

private:
    QList<QPoint> m_includePoints;
    QList<QPoint> m_excludePoints;

private Q_SLOTS:
    void slotUpdateFromMonitorData(const QVariantList &v) override;
    void addMonitorControlPoint(int xPos, int yPos, bool exclude);
    void generateImage(const QList<QPoint> includes, const QList<QPoint> excludes);

Q_SIGNALS:
    void previewImageReady();
};
