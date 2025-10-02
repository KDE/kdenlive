/*
SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "keyframemonitorhelper.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "monitor/monitor.h"

#include <core.h>
#include <utility>
KeyframeMonitorHelper::KeyframeMonitorHelper(Monitor *monitor, std::shared_ptr<AssetParameterModel> model, MonitorSceneType sceneType, QObject *parent)
    : QObject(parent)
    , m_monitor(monitor)
    , m_model(std::move(model))
    , m_active(false)
    , m_requestedSceneType(sceneType)
{
}

bool KeyframeMonitorHelper::connectMonitor(bool activate)
{
    if (activate == m_active) {
        return false;
    }
    m_active = activate;
    if (activate) {
        connect(m_monitor, &Monitor::effectPointsChanged, this, &KeyframeMonitorHelper::slotUpdateFromMonitorData, Qt::UniqueConnection);
    } else {
        m_monitor->setEffectKeyframe(false, true);
        disconnect(m_monitor, &Monitor::effectPointsChanged, this, &KeyframeMonitorHelper::slotUpdateFromMonitorData);
    }
    return m_active;
}

bool KeyframeMonitorHelper::isPlaying() const
{
    return m_monitor->isPlaying();
}

void KeyframeMonitorHelper::addIndex(const QPersistentModelIndex &index)
{
    QString name = m_model->data(index, AssetParameterModel::NameRole).toString();
    m_indexes << index;
}

QList<QPersistentModelIndex> KeyframeMonitorHelper::getIndexes()
{
    return m_indexes;
}

void KeyframeMonitorHelper::refreshParamsWhenReady(int pos)
{
    if (!m_monitor) {
        return;
    }
    if (m_monitor->effectSceneDisplayed(m_requestedSceneType)) {
        refreshParams(pos);
    } else {
        // Scene is not ready yet
        connect(m_monitor, &Monitor::sceneChanged, this, [this, pos](MonitorSceneType sceneType) {
            if (sceneType == m_requestedSceneType) {
                refreshParams(pos);
            }
        });
    }
}

void KeyframeMonitorHelper::refreshParams(int pos)
{
    QVariantList points;
    QVariantList types;
    QString rectAtPosData;
    std::shared_ptr<KeyframeModelList> keyframes = m_model->getKeyframeModel();
    for (const auto &ix : std::as_const(m_indexes)) {
        auto type = m_model->data(ix, AssetParameterModel::TypeRole).value<ParamType>();
        if (type != ParamType::AnimatedRect && type != ParamType::AnimatedFakeRect) {
            continue;
        }

        KeyframeModel *kfr = keyframes->getKeyModel(ix);
        bool ok;
        rectAtPosData = kfr->getInterpolatedValue(pos).toString();
        Keyframe kf = kfr->getNextKeyframe(GenTime(-1), &ok);
        while (ok) {
            if (kf.second == KeyframeType::Curve) {
                types << 1;
            } else {
                types << 0;
            }
            QString rectData = kfr->getInterpolatedValue(kf.first).toString();
            QStringList data = rectData.split(QLatin1Char(' '));
            if (data.size() > 3) {
                QRectF r(data.at(0).toInt(), data.at(1).toInt(), data.at(2).toInt(), data.at(3).toInt());
                points.append(QVariant(r.center()));
            }
            kf = kfr->getNextKeyframe(kf.first, &ok);
        }
    }
    if (m_monitor) {
        if (!rectAtPosData.isEmpty()) {
            QStringList data = rectAtPosData.split(QLatin1Char(' '));
            if (data.size() > 3) {
                QRect r(data.at(0).toInt(), data.at(1).toInt(), data.at(2).toInt(), data.at(3).toInt());
                m_monitor->setUpEffectGeometry(r, points, types);
                return;
            }
        }
        m_monitor->setUpEffectGeometry(points, types);
    }
}

void KeyframeMonitorHelper::slotUpdateFromMonitorData(const QVariantList &centers)
{
    std::shared_ptr<KeyframeModelList> keyframes = m_model->getKeyframeModel();
    if (centers.count() != keyframes->count()) {
        return;
    }
    for (const auto &ix : std::as_const(m_indexes)) {
        auto type = m_model->data(ix, AssetParameterModel::TypeRole).value<ParamType>();
        if (type != ParamType::AnimatedRect && type != ParamType::AnimatedFakeRect) {
            continue;
        }

        KeyframeModel *kfr = keyframes->getKeyModel(ix);
        bool ok;
        Keyframe kf = kfr->getNextKeyframe(GenTime(-1), &ok);
        int i = 0;
        while (ok) {
            QString rectData = kfr->getInterpolatedValue(kf.first).toString();
            QStringList data = rectData.split(QLatin1Char(' '));
            if (data.size() > 3) {
                QRectF r(data.at(0).toInt(), data.at(1).toInt(), data.at(2).toInt(), data.at(3).toInt());
                QPointF pt(r.center());
                QPointF expected = centers.at(i).toPointF();
                if (pt != expected) {
                    // Center rect to new pos
                    QPointF offset = expected - pt;
                    r.translate(offset);
                    QString res = QStringLiteral("%1 %2 %3 %4").arg(int(r.x())).arg(int(r.y())).arg(int(r.width())).arg(int(r.height()));
                    if (data.size() > 4) {
                        res.append(QStringLiteral(" %1").arg(data.at(4)));
                    }
                    kfr->updateKeyframe(kf.first, res);
                }
            }
            kf = kfr->getNextKeyframe(kf.first, &ok);
            i++;
        }
        break;
    }
}
