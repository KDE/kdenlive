/*
Copyright (C) 2018  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "keyframemonitorhelper.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "monitor/monitor.h"

#include <utility>
#include <core.h>
KeyframeMonitorHelper::KeyframeMonitorHelper(Monitor *monitor, std::shared_ptr<AssetParameterModel> model, const QPersistentModelIndex &index, QObject *parent)
    : QObject(parent)
    , m_monitor(monitor)
    , m_model(std::move(model))
    , m_active(false)
{
    m_indexes << index;
}

bool KeyframeMonitorHelper::connectMonitor(bool activate)
{
    if (activate == m_active) {
        return true;
    }
    m_active = activate;
    if (activate) {
        connect(m_monitor, &Monitor::effectPointsChanged, this, &KeyframeMonitorHelper::slotUpdateFromMonitorData, Qt::UniqueConnection);
    } else {
        m_monitor->setEffectKeyframe(false);
        disconnect(m_monitor, &Monitor::effectPointsChanged, this, &KeyframeMonitorHelper::slotUpdateFromMonitorData);
    }
    return m_active;
}

void KeyframeMonitorHelper::addIndex(const QPersistentModelIndex &index)
{
    m_indexes << index;
}

void KeyframeMonitorHelper::refreshParams(int /* pos */ )
{
    QVariantList points;
    QVariantList types;
    std::shared_ptr<KeyframeModelList> keyframes = m_model->getKeyframeModel();
    for (const auto &ix : qAsConst(m_indexes)) {
        auto type = m_model->data(ix, AssetParameterModel::TypeRole).value<ParamType>();
        if (type != ParamType::AnimatedRect) {
            continue;
        }
        KeyframeModel *kfr = keyframes->getKeyModel(ix);
        bool ok;
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
        break;
    }
    if (m_monitor) {
        m_monitor->setUpEffectGeometry(QRect(), points, types);
    }
}

void KeyframeMonitorHelper::slotUpdateFromMonitorData(const QVariantList &centers)
{
    std::shared_ptr<KeyframeModelList> keyframes = m_model->getKeyframeModel();
    if (centers.count() != keyframes->count()) {
        qDebug() << "* * * *CENTER POINTS MISMATCH, aborting edit";
        return;
    }
    for (const auto &ix : qAsConst(m_indexes)) {
        auto type = m_model->data(ix, AssetParameterModel::TypeRole).value<ParamType>();
        if (type != ParamType::AnimatedRect) {
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
                    QString res = QString("%1 %2 %3 %4").arg((int)r.x()).arg((int)r.y()).arg((int)r.width()).arg((int)r.height());
                    if (data.size() > 4) {
                        res.append(QString(" %1").arg(data.at(4)));
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

