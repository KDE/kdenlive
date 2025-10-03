/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
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

/** @class RotatedRectHelper
    @brief Helper class for managing rotated rectangular effects that receive data from the monitor's qml overlay.

    This class extends KeyframeMonitorHelper to handle both AnimatedRect geometry (x, y, width, height)
    and rotation parameters for effects that support rotated rectangles. It translates monitor overlay
    interactions into keyframe data updates for both geometry and rotation components.

    In practice, this is used currently only for qtblend aka Transform effect.
    */
class RotatedRectHelper : public KeyframeMonitorHelper
{
    Q_OBJECT

public:
    /** @brief Construct a helper for managing rotated rectangle effects with monitor interaction.
       @param monitor The Monitor instance this helper will interact with for overlay updates
       @param model The asset parameter model this helper will manage keyframes for
       @param parent Optional QObject parent
     */
    explicit RotatedRectHelper(Monitor *monitor, std::shared_ptr<AssetParameterModel> model, QObject *parent = nullptr);

    bool connectMonitor(bool activate) override;

    /** @brief Send data update to the monitor for both geometry and rotation parameters.
     */
    void refreshParams(int pos) override;

private Q_SLOTS:
    /** @brief Handle rectangle geometry updates from the monitor overlay.
       @param rect The new rectangle geometry from the monitor overlay
     */
    void slotUpdateFromMonitorRect(const QRectF &rect);

    /** @brief Handle rotation updates from the monitor overlay.
       @param rotation The new rotation value in degrees from the monitor overlay
     */
    void slotUpdateRotationFromMonitorData(double rotation);

private:
    /** @brief Find the rotation parameter index in the managed parameters.
       @return QPersistentModelIndex of the rotation parameter, or invalid index if not found
     */
    QPersistentModelIndex findRotationParameter() const;

    /** @brief Find the AnimatedRect parameter index in the managed parameters.
       @return QPersistentModelIndex of the AnimatedRect parameter, or invalid index if not found
     */
    QPersistentModelIndex findAnimatedRectParameter() const;
};
