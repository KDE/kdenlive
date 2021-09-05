/*
SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef RECTHELPER_H
#define RECTHELPER_H

#include "assets/keyframes/model/keyframemonitorhelper.hpp"

#include <QObject>
#include <QPersistentModelIndex>
#include <QVariant>

#include <memory>

class Monitor;
class AssetParameterModel;

class RectHelper : public KeyframeMonitorHelper
{
    Q_OBJECT

public:
    /** @brief Construct a keyframe list bound to the given effect
       @param init_value is the value taken by the param at time 0.
       @param model is the asset this parameter belong to
       @param index is the index of this parameter in its model
     */
    explicit RectHelper(Monitor *monitor, std::shared_ptr<AssetParameterModel> model, QPersistentModelIndex index, QObject *parent = nullptr);
    /** @brief Send data update to the monitor
     */
    /** @brief Send signals to the monitor to update the qml overlay.
       @param returns : true if the monitor's connection was changed to active.
    */
    bool connectMonitor(bool activate) override;
    /** @brief Send data update to the monitor
     */
    void refreshParams(int pos) override;

private slots:
    void slotUpdateFromMonitorRect(const QRect &rect);
};

#endif
