/*
Copyright (C) 2021  Julius Künzel <jk.kdedev@smartlab.uber.space>
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

#include "recthelper.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "gentime.h"
#include "monitor/monitor.h"

#include <QSize>
#include <utility>
RectHelper::RectHelper(Monitor *monitor, std::shared_ptr<AssetParameterModel> model, QPersistentModelIndex index, QObject *parent)
    : KeyframeMonitorHelper(monitor, std::move(model), std::move(index), parent)
{
}

bool RectHelper::connectMonitor(bool activate)
{
    if (activate == m_active) {
        return true;
    }
    m_active = activate;
    if (activate) {
        connect(m_monitor, &Monitor::effectChanged, this, &RectHelper::slotUpdateFromMonitorRect, Qt::UniqueConnection);
    } else {
        m_monitor->setEffectKeyframe(false);
        disconnect(m_monitor, &Monitor::effectChanged, this, &RectHelper::slotUpdateFromMonitorRect);
    }
    return m_active;
}

void RectHelper::slotUpdateFromMonitorRect(const QRect &rect) {
    QSize frameSize = pCore->getCurrentFrameSize();
    double x = double(rect.x() + rect.width() / 2) / frameSize.width();
    double y = double(rect.y() + rect.height() / 2) / frameSize.height();
    double w = double(rect.width()) / frameSize.width() * 0.5;
    double h = double(rect.height()) / frameSize.height() * 0.5;
    emit updateKeyframeData(m_indexes.at(0), x);
    emit updateKeyframeData(m_indexes.at(1), y);
    emit updateKeyframeData(m_indexes.at(2), w);
    emit updateKeyframeData(m_indexes.at(3), h);
}

void RectHelper::refreshParams(int pos)
{
    int x = 0, y = 0, w = 500, h = 500;
    QSize frameSize = pCore->getCurrentFrameSize();
    for (const auto &ix : qAsConst(m_indexes)) {
        auto type = m_model->data(ix, AssetParameterModel::TypeRole).value<ParamType>();
        if (type != ParamType::KeyframeParam) {
            continue;
        }
        QString paramName = m_model->data(ix, AssetParameterModel::NameRole).toString();
        double value = m_model->getKeyframeModel()->getInterpolatedValue(pos, ix).toDouble();
        if (paramName.contains(QLatin1String("Position X"))) {
            x = frameSize.width() * value;
        } else if (paramName.contains(QLatin1String("Position Y"))) {
            y = frameSize.height() * value;
        } else if (paramName.contains(QLatin1String("Size X"))) {
            w = frameSize.width() * value * 2;
        } else if (paramName.contains(QLatin1String("Size Y"))) {
            h = frameSize.height() * value * 2;
        }
    }
    if (m_monitor) {
        qDebug() << QRect(x, y, w, h);
        m_monitor->setUpEffectGeometry(QRect(x-w/2, y-h/2, w, h));
    }
}
