/*
    SPDX-FileCopyrightText: 2025 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "rotatedrecthelper.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "monitor/monitor.h"

#include <utility>

RotatedRectHelper::RotatedRectHelper(Monitor *monitor, std::shared_ptr<AssetParameterModel> model, QObject *parent)
    : KeyframeMonitorHelper(monitor, std::move(model), MonitorSceneRotatedGeometry, parent)
{
}

bool RotatedRectHelper::connectMonitor(bool activate)
{

    if (activate == m_active) {
        return false;
    }
    m_active = activate;

    if (activate) {
        connect(m_monitor, &Monitor::effectChanged, this, &RotatedRectHelper::slotUpdateFromMonitorRect, Qt::UniqueConnection);
        connect(m_monitor, &Monitor::effectRotationChanged, this, &RotatedRectHelper::slotUpdateRotationFromMonitorData, Qt::UniqueConnection);
    } else {
        m_monitor->setEffectKeyframe(false, true);
        // Note: We intentionally keep both geometry and rotation signals connected for continuous updates even when not active (clip not below playhead)
    }
    return m_active;
}

void RotatedRectHelper::refreshParams(int pos)
{
    // First call parent implementation to handle AnimatedRect updates (x, y, width, height)
    KeyframeMonitorHelper::refreshParams(pos);

    // Now handle rotation-specific parts
    double rotation = 0;
    std::shared_ptr<KeyframeModelList> keyframes = m_model->getKeyframeModel();

    // Find rotation parameter and get its interpolated value
    for (const auto &ix : std::as_const(m_indexes)) {
        QString name = m_model->data(ix, AssetParameterModel::NameRole).toString();
        if (name == QLatin1String("rotation")) {
            rotation = keyframes->getInterpolatedValue(pos, ix).toDouble();
            break;
        }
    }

    if (m_monitor) {
        m_monitor->setEffectSceneProperty(QStringLiteral("rect_rotation"), rotation);
    }
}

void RotatedRectHelper::slotUpdateFromMonitorRect(const QRectF &rect)
{
    QPersistentModelIndex rectIndex = findAnimatedRectParameter();
    if (!rectIndex.isValid()) {
        return;
    }

    // Create new rect value with updated coordinates
    // Format of AnimatedRect string: "x y width height opacity" (opacity is optional)
    QString newValue = QStringLiteral("%1 %2 %3 %4").arg(int(rect.x())).arg(int(rect.y())).arg(int(rect.width())).arg(int(rect.height()));

    // Update the model
    Q_EMIT updateKeyframeData(rectIndex, QVariant(newValue));
}

void RotatedRectHelper::slotUpdateRotationFromMonitorData(double rotation)
{
    QPersistentModelIndex rotationIndex = findRotationParameter();
    if (!rotationIndex.isValid()) {
        return;
    }
    m_model->setParameter(QLatin1String("rotation"), QString::number(rotation), true, rotationIndex);
    Q_EMIT updateKeyframeData(rotationIndex, QVariant(rotation));
}

QPersistentModelIndex RotatedRectHelper::findRotationParameter() const
{
    for (const auto &ix : std::as_const(m_indexes)) {
        QString paramName = m_model->data(ix, AssetParameterModel::NameRole).toString();
        if (paramName == QLatin1String("rotation")) {
            return ix;
        }
    }
    return QPersistentModelIndex();
}

QPersistentModelIndex RotatedRectHelper::findAnimatedRectParameter() const
{
    for (const auto &ix : std::as_const(m_indexes)) {
        auto type = m_model->data(ix, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::AnimatedRect || type == ParamType::AnimatedFakeRect) {
            return ix;
        }
    }
    return QPersistentModelIndex();
}
