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

#include "cornershelper.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "gentime.h"
#include "monitor/monitor.h"

#include <QSize>
#include <utility>
CornersHelper::CornersHelper(Monitor *monitor, std::shared_ptr<AssetParameterModel> model, QPersistentModelIndex index, QObject *parent)
    : KeyframeMonitorHelper(monitor, std::move(model), std::move(index), parent)
{
}

void CornersHelper::slotUpdateFromMonitorData(const QVariantList &v)
{
    const QVariantList points = QVariant(v).toList();
    QSize frameSize = pCore->getCurrentFrameSize();
    int ix = 0;
    for (const auto &point : points) {
        QPointF pt = point.toPointF();
        double x = (pt.x() / frameSize.width() + 1) / 3;
        double y = (pt.y() / frameSize.height() + 1) / 3;
        emit updateKeyframeData(m_indexes.at(ix), x);
        emit updateKeyframeData(m_indexes.at(ix + 1), y);
        ix += 2;
    }
}

void CornersHelper::refreshParams(int pos)
{
    QVariantList points{QPointF(), QPointF(), QPointF(), QPointF()};
    QSize frameSize = pCore->getCurrentFrameSize();
    for (const auto &ix : qAsConst(m_indexes)) {
        auto type = m_model->data(ix, AssetParameterModel::TypeRole).value<ParamType>();
        if (type != ParamType::KeyframeParam) {
            continue;
        }
        int paramName = m_model->data(ix, AssetParameterModel::NameRole).toInt();
        if (paramName > 7) {
            continue;
        }
        double value = m_model->getKeyframeModel()->getInterpolatedValue(pos, ix).toDouble();
        value = ((3 * value) - 1) * (paramName % 2 == 0 ? frameSize.width() : frameSize.height());
        switch (paramName) {
        case 0:
            points[0] = QPointF(value, points.at(0).toPointF().y());
            break;
        case 1:
            points[0] = QPointF(points.at(0).toPointF().x(), value);
            break;
        case 2:
            points[1] = QPointF(value, points.at(1).toPointF().y());
            break;
        case 3:
            points[1] = QPointF(points.at(1).toPointF().x(), value);
            break;
        case 4:
            points[2] = QPointF(value, points.at(2).toPointF().y());
            break;
        case 5:
            points[2] = QPointF(points.at(2).toPointF().x(), value);
            break;
        case 6:
            points[3] = QPointF(value, points.at(3).toPointF().y());
            break;
        case 7:
            points[3] = QPointF(points.at(3).toPointF().x(), value);
            break;
        default:
            break;
        }
    }
    if (m_monitor) {
        m_monitor->setUpEffectGeometry(QRect(), points);
    }
}
